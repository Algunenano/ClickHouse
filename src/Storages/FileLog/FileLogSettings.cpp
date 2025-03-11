#include <Core/BaseSettings.h>
#include <Core/BaseSettingsFwdMacrosImpl.h>
#include <Core/FormatFactorySettings.h>
#include <Parsers/ASTCreateQuery.h>
#include <Parsers/ASTFunction.h>
#include <Parsers/ASTSetQuery.h>
#include <Storages/FileLog/FileLogSettings.h>
#include <Common/Exception.h>

namespace DB
{

namespace ErrorCodes
{
    extern const int UNKNOWN_SETTING;
    extern const int INVALID_SETTING_VALUE;
}

#define FILELOG_RELATED_SETTINGS(DECLARE, ALIAS) \
    /* default is stream_poll_timeout_ms */ \
    DECLARE(Milliseconds, poll_timeout_ms, 0, "Timeout for single poll from StorageFileLog.", 0) \
    DECLARE(UInt64, poll_max_batch_size, 0, "Maximum amount of messages to be polled in a single StorageFileLog poll.", 0) \

#define LIST_OF_FILELOG_SETTINGS(M, ALIAS) \
    FILELOG_RELATED_SETTINGS(M, ALIAS) \
    // LIST_OF_ALL_FORMAT_SETTINGS(M, ALIAS)

DECLARE_SETTINGS_TRAITS(FileLogSettingsTraits, LIST_OF_FILELOG_SETTINGS, FILELOG_SETTINGS_SUPPORTED_TYPES)
IMPLEMENT_SETTINGS_TRAITS(FileLogSettingsTraits, LIST_OF_FILELOG_SETTINGS)

struct FileLogSettingsImpl : public BaseSettings<FileLogSettingsTraits>
{
};

FILELOG_SETTINGS_SUPPORTED_TYPES(FileLogSettings, IMPLEMENT_SETTING_SUBSCRIPT_OPERATOR)

#define INITIALIZE_SETTING_EXTERN(TYPE, NAME, DEFAULT, DESCRIPTION, FLAGS) \
    FileLogSettings##TYPE NAME = { &FileLogSettingsImpl ::data_##TYPE , &FileLogSettingsImpl :: position_##NAME };

namespace FileLogSetting
{
LIST_OF_FILELOG_SETTINGS(INITIALIZE_SETTING_EXTERN, SKIP_ALIAS)
}

#undef INITIALIZE_SETTING_EXTERN

FileLogSettings::FileLogSettings() : impl(std::make_unique<FileLogSettingsImpl>())
{
}

FileLogSettings::FileLogSettings(const FileLogSettings & settings) : impl(std::make_unique<FileLogSettingsImpl>(*settings.impl))
{
}

FileLogSettings::FileLogSettings(FileLogSettings && settings) noexcept
    : impl(std::make_unique<FileLogSettingsImpl>(std::move(*settings.impl)))
{
}

FileLogSettings::~FileLogSettings() = default;

void FileLogSettings::loadFromQuery(ASTStorage & storage_def)
{
    if (storage_def.settings)
    {
        try
        {
            impl->applyChanges(storage_def.settings->changes);
        }
        catch (Exception & e)
        {
            if (e.code() == ErrorCodes::UNKNOWN_SETTING)
                e.addMessage("for storage " + storage_def.engine->name);
            throw;
        }
    }
    else
    {
        auto settings_ast = std::make_shared<ASTSetQuery>();
        settings_ast->is_standalone = false;
        storage_def.set(storage_def.settings, settings_ast);
    }

    /// Check that batch size is not too high (the same as we check setting max_block_size).
    constexpr UInt64 max_sane_block_rows_size = 4294967296; // 2^32
    if ((*this)[FileLogSetting::poll_max_batch_size] > max_sane_block_rows_size)
        throw Exception(
            ErrorCodes::INVALID_SETTING_VALUE, "Sanity check: 'poll_max_batch_size' value is too high ({})", (*this)[FileLogSetting::poll_max_batch_size].value);
}

bool FileLogSettings::hasBuiltin(std::string_view name)
{
    return FileLogSettingsImpl::hasBuiltin(name);
}
}
