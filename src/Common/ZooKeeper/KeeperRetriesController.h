#pragma once
#include <Interpreters/ProcessList.h>
#include <base/sleep.h>
#include <Common/Exception.h>
#include <Common/ZooKeeper/KeeperException.h>
#include <Common/logger_useful.h>

#include <memory>

namespace DB
{

namespace ErrorCodes
{
    extern const int OK;
}

struct KeeperRetriesInfo
{
    KeeperRetriesInfo() = delete;
    KeeperRetriesInfo(UInt64 max_retries_, UInt64 initial_backoff_ms_, UInt64 max_backoff_ms_)
        : max_retries(max_retries_), initial_backoff_ms(std::min(initial_backoff_ms_, max_backoff_ms_)), max_backoff_ms(max_backoff_ms_)
    {
    }

    static KeeperRetriesInfo fromBackupRestoreSettings(const Settings & settings)
    {
        return KeeperRetriesInfo{
            settings.backup_restore_keeper_max_retries,
            settings.backup_restore_keeper_retry_initial_backoff_ms,
            settings.backup_restore_keeper_retry_max_backoff_ms};
    }

    static KeeperRetriesInfo fromInsertSettings(const Settings & settings)
    {
        return KeeperRetriesInfo{
            settings.insert_keeper_max_retries,
            settings.insert_keeper_retry_initial_backoff_ms,
            settings.insert_keeper_retry_max_backoff_ms};
    }

    const UInt64 max_retries;
    const UInt64 initial_backoff_ms;
    const UInt64 max_backoff_ms;
};

class KeeperRetriesControl
{
public:
    KeeperRetriesControl(std::string name_, Poco::Logger * logger_, KeeperRetriesInfo retries_info_, QueryStatusPtr elem)
        : name(std::move(name_)), logger(logger_), retries_info(std::move(retries_info_)), process_list_element(elem)
    {
    }

    void retryLoop(auto && f)
    {
        retryLoop(f, []() {});
    }

    /// retryLoop() executes f() until it succeeds/max_retries is reached/non-retrialable error is encountered
    ///
    /// the callable f() can provide feedback in terms of errors in two ways:
    /// 1. throw KeeperException exception:
    ///     in such case, retries are done only on hardware keeper errors
    ///     because non-hardware error codes are semantically not really errors, just a response
    /// 2. set an error code in the KeeperRetriesControl object (setUserError/setKeeperError)
    ///     The idea is that if the caller has some semantics on top of non-hardware keeper errors,
    ///     then it can provide feedback to retries controller via user errors
    ///
    /// It is possible to use it multiple times (it will share nº of errors over the total amount of calls)
    void retryLoop(auto && f, auto && iteration_cleanup)
    {
        current_iteration = 0;
        current_backoff_ms = retries_info.initial_backoff_ms;

        while (current_iteration == 0 || canTry())
        {
            try
            {
                f();
                iteration_cleanup();
            }
            catch (const zkutil::KeeperException & e)
            {
                iteration_cleanup();

                if (!Coordination::isHardwareError(e.code))
                    throw;

                setKeeperError(std::current_exception(), e.code, e.message());
            }
            catch (...)
            {
                iteration_cleanup();
                throw;
            }
            current_iteration++;
        }
    }

    bool callAndCatchAll(auto && f)
    {
        try
        {
            f();
            return true;
        }
        catch (const zkutil::KeeperException & e)
        {
            setKeeperError(std::current_exception(), e.code, e.message());
        }
        catch (const Exception & e)
        {
            setUserError(std::current_exception(), e.code(), e.what());
        }
        return false;
    }

    void setUserError(std::exception_ptr exception, int code, std::string message)
    {
        if (logger)
            LOG_TRACE(logger, "KeeperRetriesControl: {}: setUserError: error={} message={}", name, code, message);

        /// if current iteration is already failed, keep initial error
        if (!iteration_succeeded)
            return;

        iteration_succeeded = false;
        user_error.code = code;
        user_error.message = std::move(message);
        user_error.exception = exception;
        keeper_error = KeeperError{};
        total_failures++;
    }

    template <typename... Args>
    void setUserError(std::exception_ptr exception, int code, fmt::format_string<Args...> fmt, Args &&... args)
    {
        setUserError(exception, code, fmt::format(fmt, std::forward<Args>(args)...));
    }

    void setUserError(int code, std::string message)
    {
        setUserError(std::make_exception_ptr(Exception::createDeprecated(message, code)), code, message);
    }

    template <typename... Args>
    void setUserError(int code, fmt::format_string<Args...> fmt, Args &&... args)
    {
        setUserError(code, fmt::format(fmt, std::forward<Args>(args)...));
    }

    void setKeeperError(std::exception_ptr exception, Coordination::Error code, std::string message)
    {
        if (logger)
            LOG_TRACE(logger, "KeeperRetriesControl: {}: setKeeperError: error={} message={}", name, code, message);

        /// if current iteration is already failed, keep initial error
        if (!iteration_succeeded)
            return;

        iteration_succeeded = false;
        keeper_error.code = code;
        keeper_error.message = std::move(message);
        keeper_error.exception = exception;
        user_error = UserError{};
        total_failures++;
    }

    template <typename... Args>
    void setKeeperError(std::exception_ptr exception, Coordination::Error code, fmt::format_string<Args...> fmt, Args &&... args)
    {
        setKeeperError(exception, code, fmt::format(fmt, std::forward<Args>(args)...));
    }

    void setKeeperError(Coordination::Error code, std::string message)
    {
        setKeeperError(std::make_exception_ptr(zkutil::KeeperException::createDeprecated(message, code)), code, message);
    }

    template <typename... Args>
    void setKeeperError(Coordination::Error code, fmt::format_string<Args...> fmt, Args &&... args)
    {
        setKeeperError(code, fmt::format(fmt, std::forward<Args>(args)...));
    }

    void stopRetries() { stop_retries = true; }

    void requestUnconditionalRetry() { unconditional_retry = true; }

    bool isLastRetry() const { return total_failures >= retries_info.max_retries; }

    bool isRetry() const { return current_iteration > 1; }

    Coordination::Error getLastKeeperErrorCode() const { return keeper_error.code; }

    /// action will be called only once and only after latest failed retry
    void actionAfterLastFailedRetry(std::function<void()> f) { action_after_last_failed_retry = std::move(f); }

private:
    struct KeeperError
    {
        using Code = Coordination::Error;
        Code code = Code::ZOK;
        std::string message;
        std::exception_ptr exception;
    };

    struct UserError
    {
        int code = ErrorCodes::OK;
        std::string message;
        std::exception_ptr exception;
    };

    bool canTry()
    {
        if (unconditional_retry)
        {
            unconditional_retry = false;
            return true;
        }

        if (iteration_succeeded)
        {
            if (logger && total_failures > 0)
                LOG_DEBUG(
                    logger,
                    "KeeperRetriesControl: {}: succeeded after: Iterations={} Total keeper failures={}/{}",
                    name,
                    current_iteration,
                    total_failures,
                    retries_info.max_retries);
            return false;
        }

        if (stop_retries)
        {
            logLastError("stop retries on request");
            action_after_last_failed_retry();
            throwIfError();
            return false;
        }

        if (total_failures >= retries_info.max_retries)
        {
            logLastError("retry limit is reached");
            action_after_last_failed_retry();
            throwIfError();
            return false;
        }

        if (process_list_element && !process_list_element->checkTimeLimitSoft())
            return false;

        /// retries
        logLastError("will retry due to error");
        sleepForMilliseconds(current_backoff_ms);
        current_backoff_ms = std::min(current_backoff_ms * 2, retries_info.max_backoff_ms);

        /// reset the flag, it will be set to false in case of error
        iteration_succeeded = true;

        return true;
    }

    void throwIfError() const
    {
        if (user_error.exception)
            std::rethrow_exception(user_error.exception);

        if (keeper_error.exception)
            std::rethrow_exception(keeper_error.exception);
    }

    void logLastError(std::string_view header)
    {
        if (!logger)
            return;
        if (user_error.code != ErrorCodes::OK)
        {
            LOG_DEBUG(
                logger,
                "KeeperRetriesControl: {}: {}: retry_count={}/{} timeout={}ms error={} message={}",
                name,
                header,
                current_iteration,
                retries_info.max_retries,
                current_backoff_ms,
                user_error.code,
                user_error.message);
        }
    }


    std::string name;
    Poco::Logger * logger = nullptr;
    KeeperRetriesInfo retries_info;
    UInt64 total_failures = 0;
    UserError user_error;
    KeeperError keeper_error;
    std::function<void()> action_after_last_failed_retry = []() {};
    bool unconditional_retry = false;
    bool iteration_succeeded = true;
    bool stop_retries = false;
    QueryStatusPtr process_list_element;

    UInt64 current_iteration = 0;
    UInt64 current_backoff_ms = 0;
};

using KeeperRetriesControlPtr = std::shared_ptr<KeeperRetriesControl>;

}
