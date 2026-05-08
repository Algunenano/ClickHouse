#pragma once

#include <Common/VariableContext.h>
#include <Common/Stopwatch.h>
#include <Interpreters/Context_fwd.h>
#include <base/types.h>
#include <base/strong_typedef.h>
#include <Poco/Message.h>
#include <atomic>
#include <memory>
#include <cstddef>


/** Implements global counters for various events happening in the application
  *  - for high level profiling.
  * See .cpp for list of events.
  */

namespace ProfileEvents
{
    /// Event identifier (index in array).
    using Event = StrongTypedef<size_t, struct EventTag>;
    using Count = size_t;
    using Increment = Int64;

    /// Bit 63 of each counter is the "send to system.trace_log on increment" flag.
    /// Counters never realistically reach 2^63, so we steal the top bit and keep the
    /// counter packed in 8 bytes (4 per cache line) instead of paying 16 bytes for
    /// 8 bytes of value + 1 byte of bool + 7 bytes of padding.
    static constexpr Count COUNTER_TRACE_BIT = Count(1) << 63;
    static constexpr Count COUNTER_VALUE_MASK = ~COUNTER_TRACE_BIT;

    struct Counter : public std::atomic<Count>
    {
        using std::atomic<Count>::atomic;
    };
    class Counters;

    /// Counters - how many times each event happened
    extern Counters global_counters;

    class Timer
    {
    public:
        enum class Resolution : UInt32
        {
            Nanoseconds = 1,
            Microseconds = 1000,
            Milliseconds = 1000000,
        };
        Timer(Counters & counters_, Event timer_event_, Resolution resolution_);
        Timer(Counters & counters_, Event timer_event_, Event counter_event, Resolution resolution_);
        Timer(Timer && other) noexcept
            : counters(other.counters), timer_event(std::move(other.timer_event)), watch(std::move(other.watch)), resolution(std::move(other.resolution))
            {}
        ~Timer() { end(); }
        void cancel() { watch.reset(); }
        void restart() { watch.restart(); }
        void end();
        UInt64 get();

    private:
        Counters & counters;
        Event timer_event;
        Stopwatch watch;
        Resolution resolution;
    };

    class Counters
    {
    private:
        Counter * counters = nullptr;
        std::unique_ptr<Counter[]> counters_holder;
        /// Used to propagate increments
        std::atomic<Counters *> parent = {};
        /// Fast-path gate for `increment`: true iff any tracing flag (per-event or "all")
        /// has been set on this `Counters` or any ancestor. When false (the common case),
        /// `increment` skips the per-event trace-bit check loop entirely.
        std::atomic_bool any_trace_in_chain = false;
        Counter prev_cpu_wait_microseconds = 0;
        Counter prev_cpu_virtual_time_microseconds = 0;

    public:

        VariableContext level = VariableContext::Thread;

        /// By default, any instance have to increment global counters
        explicit Counters(VariableContext level_ = VariableContext::Thread, Counters * parent_ = &global_counters);

        /// Global level static initializer (constexpr to enable constant initialization
        /// before any dynamic initializer can allocate memory and call ProfileEvents::increment)
        constexpr explicit Counters(Counter * allocated_counters) noexcept
            : counters(allocated_counters), parent(nullptr), level(VariableContext::Global) {}

        Counters(Counters && src) noexcept;

        Counter & operator[] (Event event)
        {
            return counters[event];
        }

        const Counter & operator[] (Event event) const
        {
            return counters[event];
        }

        double getCPUOverload(Int64 os_cpu_busy_time_threshold, bool reset = false);

        void increment(Event event, Count amount = 1);
        void incrementNoTrace(Event event, Count amount = 1);

        struct Snapshot
        {
            Snapshot();
            Snapshot(Snapshot &&) = default;

            Count operator[] (Event event) const noexcept
            {
                return counters_holder[event];
            }

            Snapshot & operator=(Snapshot &&) = default;
        private:
            std::unique_ptr<Count[]> counters_holder;

            friend class Counters;
            friend struct CountersIncrement;
        };

        /// Every single value is fetched atomically, but not all values as a whole.
        Snapshot getPartiallyAtomicSnapshot() const;

        /// Reset all counters to zero and reset parent.
        void reset();

        /// Get parent (thread unsafe)
        Counters * getParent()
        {
            return parent.load(std::memory_order_relaxed);
        }

        /// Set parent (thread unsafe)
        void setUserCounters(Counters * user)
        {
            auto * current_val = this;
            auto * parent_val = this->parent.load(std::memory_order_relaxed);

            while (parent_val != nullptr && parent_val->level != VariableContext::Global && parent_val->level != VariableContext::User)
            {
                current_val = parent_val;
                parent_val = current_val->parent.load(std::memory_order_relaxed);
            }

            current_val->parent.store(user, std::memory_order_relaxed);
            current_val->inheritTracingFromParent(user);
        }

        /// Set parent (thread unsafe)
        void setParent(Counters * parent_)
        {
            parent.store(parent_, std::memory_order_relaxed);
            inheritTracingFromParent(parent_);
        }

        /// Trace every event by setting bit 63 on every counter, and mark the chain.
        void setTraceAllProfileEvents()
        {
            for (Event i = Event(0); i < num_counters; ++i)
                counters[i].fetch_or(COUNTER_TRACE_BIT, std::memory_order_relaxed);
            markChainTracing();
        }

        void setTraceProfileEvent(ProfileEvents::Event event)
        {
            counters[event].fetch_or(COUNTER_TRACE_BIT, std::memory_order_relaxed);
            markChainTracing();
        }

        bool anyTraceInChain() const noexcept
        {
            return any_trace_in_chain.load(std::memory_order_relaxed);
        }

    private:
        /// Walk up the parent chain marking `any_trace_in_chain` so descendants attaching
        /// to any of these ancestors will inherit the flag at attach time.
        void markChainTracing()
        {
            Counters * c = this;
            while (c != nullptr)
            {
                c->any_trace_in_chain.store(true, std::memory_order_relaxed);
                c = c->parent.load(std::memory_order_relaxed);
            }
        }

        void inheritTracingFromParent(Counters * p)
        {
            if (p && p->any_trace_in_chain.load(std::memory_order_relaxed))
                any_trace_in_chain.store(true, std::memory_order_relaxed);
        }

    public:

        void setTraceProfileEvents(const String & events_list);

        /// Set all counters to zero
        void resetCounters();

        /// Add elapsed time to `timer_event` when returned object goes out of scope.
        /// Use the template parameter to control timer resolution, the default
        /// is `Timer::Resolution::Microseconds`.
        template <Timer::Resolution resolution = Timer::Resolution::Microseconds>
        Timer timer(Event timer_event)
        {
            return Timer(*this, timer_event, resolution);
        }

        /// Increment `counter_event` and add elapsed time to `timer_event` when returned object goes out of scope.
        /// Use the template parameter to control timer resolution, the default
        /// is `Timer::Resolution::Microseconds`.
        template <Timer::Resolution resolution = Timer::Resolution::Microseconds>
        Timer timer(Event timer_event, Event counter_event)
        {
            return Timer(*this, timer_event, counter_event, resolution);
        }

        static const Event num_counters;
    };

    enum class ValueType : uint8_t
    {
        Number,
        Bytes,
        Milliseconds,
        Microseconds,
        Nanoseconds,
    };

    /// Increment a counter for event. Thread-safe.
    void increment(Event event, Count amount = 1);

    /// The same as above but ignores value of setting 'trace_profile_events'
    /// and never sends profile event to trace log.
    void incrementNoTrace(Event event, Count amount = 1);

    /// Increment a counter for log messages.
    void incrementForLogMessage(Poco::Message::Priority priority);

    /// Increment time consumed by logging.
    void incrementLoggerElapsedNanoseconds(UInt64 ns);

    /// Get name of event by identifier. Returns statically allocated string.
    const std::string_view & getName(Event event);

    /// Get description of event by identifier. Returns statically allocated string.
    const std::string_view & getDocumentation(Event event);

    /// Get ProfileEvent by its name
    Event getByName(std::string_view name);

    /// Get value type of event by identifier. Returns enum value.
    ValueType getValueType(Event event);

    /// Get index just after last event identifier.
    Event end();

    /// Check CPU overload. If should_throw parameter is set, the method will throw when the server is overloaded.
    /// Otherwise, this method will return true if the server is overloaded.
    bool checkCPUOverload(Int64 os_cpu_busy_time_threshold, double min_ratio, double max_ratio, bool should_throw);

    struct CountersIncrement
    {
        CountersIncrement() noexcept = default;
        explicit CountersIncrement(Counters::Snapshot const & snapshot);
        CountersIncrement(Counters::Snapshot const & after, Counters::Snapshot const & before);

        CountersIncrement(CountersIncrement &&) = default;
        CountersIncrement & operator=(CountersIncrement &&) = default;

        Increment operator[](Event event) const noexcept
        {
            return increment_holder[event];
        }
    private:
        void init();

        static_assert(sizeof(Count) == sizeof(Increment), "Sizes of counter and increment differ");

        std::unique_ptr<Increment[]> increment_holder;
    };
}
