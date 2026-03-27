#include <Columns/ColumnArray.h>
#include <Columns/ColumnLowCardinality.h>
#include <Columns/ColumnMap.h>
#include <Columns/ColumnTuple.h>
#include <Core/BaseSettings.h>
#include <Core/BaseSettingsFwdMacrosImpl.h>
#include <Core/BaseSettingsProgramOptions.h>
#include <Core/DistributedCacheDefines.h>
#include <Core/FormatFactorySettings.h>
#include <Core/Settings.h>
#include <Core/SettingsChangesHistory.h>
#include <Core/SettingsEnums.h>
#include <Core/SettingsFields.h>
#include <Core/SettingsObsoleteMacros.h>
#include <Core/SettingsTierType.h>
#include <IO/ReadBufferFromString.h>
#include <IO/S3Defines.h>
#include <Storages/System/MutableColumnsAndConstraints.h>
#include <base/types.h>
#include <Common/NamePrompter.h>
#include <Common/typeid_cast.h>

#include <boost/program_options.hpp>
#include <Poco/Util/AbstractConfiguration.h>
#include <Poco/Util/Application.h>

#include <cstring>

namespace
{
#if !CLICKHOUSE_CLOUD
constexpr UInt64 default_max_size_to_drop = 50000000000lu;
constexpr UInt64 default_distributed_cache_connect_max_tries = 5lu;
constexpr UInt64 default_distributed_cache_read_request_max_tries = 10lu;
constexpr UInt64 default_distributed_cache_credentials_refresh_period_seconds = 5;
constexpr UInt64 default_distributed_cache_connect_backoff_min_ms = 0;
constexpr UInt64 default_distributed_cache_connect_backoff_max_ms = 50;
constexpr UInt64 default_distributed_cache_connect_timeout_ms = 50;
constexpr UInt64 default_distributed_cache_send_timeout_ms = 3000;
constexpr UInt64 default_distributed_cache_receive_timeout_ms = 3000;
constexpr UInt64 default_distributed_cache_tcp_keep_alive_timeout_ms = 2900;
constexpr UInt64 default_distributed_cache_use_clients_cache_for_read = true;
constexpr UInt64 default_distributed_cache_use_clients_cache_for_write = false;
#else
constexpr UInt64 default_max_size_to_drop = 0lu;
constexpr UInt64 default_distributed_cache_connect_max_tries = DistributedCache::DEFAULT_CONNECT_MAX_TRIES;
constexpr UInt64 default_distributed_cache_read_request_max_tries = DistributedCache::DEFAULT_READ_REQUEST_MAX_TRIES;
constexpr UInt64 default_distributed_cache_credentials_refresh_period_seconds = DistributedCache::DEFAULT_CREDENTIALS_REFRESH_PERIOD_SECONDS;
constexpr UInt64 default_distributed_cache_connect_backoff_min_ms = DistributedCache::DEFAULT_CONNECT_BACKOFF_MIN_MS;
constexpr UInt64 default_distributed_cache_connect_backoff_max_ms = DistributedCache::DEFAULT_CONNECT_BACKOFF_MAX_MS;
constexpr UInt64 default_distributed_cache_connect_timeout_ms = DistributedCache::DEFAULT_CONNECT_TIMEOUTS_MS;
constexpr UInt64 default_distributed_cache_send_timeout_ms = DistributedCache::DEFAULT_SEND_TIMEOUT_MS;
constexpr UInt64 default_distributed_cache_receive_timeout_ms = DistributedCache::DEFAULT_RECEIVE_TIMEOUT_MS;
constexpr UInt64 default_distributed_cache_tcp_keep_alive_timeout_ms = DistributedCache::DEFAULT_TCP_KEEP_ALIVE_TIMEOUT_MS;
constexpr UInt64 default_distributed_cache_use_clients_cache_for_read = DistributedCache::DEFAULT_USE_CLIENTS_CACHE_FOR_READ;
constexpr UInt64 default_distributed_cache_use_clients_cache_for_write = DistributedCache::DEFAULT_USE_CLIENTS_CACHE_FOR_WRITE;
#endif
}

namespace DB
{

namespace ErrorCodes
{
    extern const int THERE_IS_NO_PROFILE;
    extern const int NO_ELEMENTS_IN_CONFIG;
    extern const int UNKNOWN_ELEMENT_IN_CONFIG;
    extern const int BAD_ARGUMENTS;
}

/** List of settings: type, name, default value, description, flags
  *
  * This looks rather inconvenient. It is done that way to avoid repeating settings in different places.
  * Note: as an alternative, we could implement settings to be completely dynamic in the form of the map: String -> Field,
  *  but we are not going to do it, because settings are used everywhere as static struct fields.
  *
  * `flags` can include a Tier (BETA | EXPERIMENTAL) and an optional bitwise AND with IMPORTANT.
  * The default (0) means a PRODUCTION ready setting
  *
  * A setting is "IMPORTANT" if it affects the results of queries and can't be ignored by older versions.
  * Tiers:
  * EXPERIMENTAL: The feature is in active development stage. Mostly for developers or for ClickHouse enthusiasts.
  * BETA: There are no known bugs problems in the functionality, but the outcome of using it together with other
  * features/components is unknown and correctness is not guaranteed.
  * PRODUCTION (Default): The feature is safe to use along with other features from the PRODUCTION tier.
  *
  * When adding new or changing existing settings add them to the settings changes history in SettingsChangesHistory.cpp
  * for tracking settings changes in different versions and for special `compatibility` settings to work correctly.
  *
  * The settings in this list are used to autogenerate the markdown documentation. You can find the script which
  * generates the markdown from source here: https://github.com/ClickHouse/clickhouse-docs/blob/main/scripts/settings/autogenerate-settings.sh
  *
  * If a setting has an effect only in ClickHouse Cloud, then please include in the description: "Only has an effect in ClickHouse Cloud."
  */

// clang-format off
#if defined(__CLION_IDE__)
/// CLion freezes for a minute every time it processes this
#define COMMON_SETTINGS(DECLARE, DECLARE_WITH_ALIAS)
#define OBSOLETE_SETTINGS(DECLARE, DECLARE_WITH_ALIAS)
#else
/// Settings definitions are in separate .inc files so the code generator
/// (gen_settings_data.sh) can also include them via the preprocessor.
#include <Core/SettingsList.inc>
#endif /// __CLION_IDE__


#define LIST_OF_SETTINGS(M, ALIAS)     \
    COMMON_SETTINGS(M, ALIAS)          \
    OBSOLETE_SETTINGS(M, ALIAS)        \
    FORMAT_FACTORY_SETTINGS(M, ALIAS)  \
    OBSOLETE_FORMAT_SETTINGS(M, ALIAS) \

// clang-format on

/// Include the generated Data struct with typed arrays and offset constants.
/// This replaces the macro-generated Data with a layout-optimized version
/// where settings of the same type are packed into contiguous arrays.
#include <Core/SettingsData.generated.h>

DECLARE_SETTINGS_TRAITS_GENERATED(SettingsTraits, LIST_OF_SETTINGS, DB::SettingsData, 1)

/// Override the IMPLEMENT macros to use generated offsets (SettingOffset::NAME)
/// instead of offsetof(Data, NAME), since Data now has typed arrays, not named members.
#undef IMPLEMENT_SETTINGS_TRAITS_
#define IMPLEMENT_SETTINGS_TRAITS_(TYPE, NAME, DEFAULT, DESCRIPTION, FLAGS, ...) \
    res.field_infos.emplace_back( \
        FieldInfo\
        { \
            #NAME, \
            "", \
            #TYPE, \
            DESCRIPTION, \
            static_cast<UInt64>(FLAGS), \
            &settingFieldOps<SettingField##TYPE>(), \
            SettingOffset::NAME.offset, \
            []() -> std::pair<Field, String> { SettingField##TYPE d(DEFAULT); return {static_cast<Field>(d), d.toString()}; }, \
        });

IMPLEMENT_SETTINGS_TRAITS(SettingsTraits, LIST_OF_SETTINGS)

/** Settings of query execution.
  * These settings go to users.xml.
  */
struct SettingsImpl : public BaseSettings<SettingsTraits>, public IHints<2>
{
    SettingsImpl() = default;

    /** Set multiple settings from "profile" (in server configuration file (users.xml), profiles contain groups of multiple settings).
        * The profile can also be set using the `set` functions, like the profile setting.
        */
    void setProfile(const String & profile_name, const Poco::Util::AbstractConfiguration & config);

    /// Load settings from configuration file, at "path" prefix in configuration.
    void loadSettingsFromConfig(const String & path, const Poco::Util::AbstractConfiguration & config);

    /// Dumps profile events to column of type Map(String, String)
    void dumpToMapColumn(IColumn * column, bool changed_only = true);

    /// Check that there is no user-level settings at the top level in config.
    /// This is a common source of mistake (user don't know where to write user-level setting).
    static void checkNoSettingNamesAtTopLevel(const Poco::Util::AbstractConfiguration & config, const String & config_path);

    std::vector<String> getAllRegisteredNames() const override;

    void set(std::string_view name, const Field & value) override;

private:
    void applyCompatibilitySetting(const String & compatibility);

    std::unordered_set<std::string_view> settings_changed_by_compatibility_setting;
};

/** Set the settings from the profile (in the server configuration, many settings can be listed in one profile).
    * The profile can also be set using the `set` functions, like the `profile` setting.
    */
void SettingsImpl::setProfile(const String & profile_name, const Poco::Util::AbstractConfiguration & config)
{
    String elem = "profiles." + profile_name;

    if (!config.has(elem))
        throw Exception(ErrorCodes::THERE_IS_NO_PROFILE, "There is no profile '{}' in configuration file.", profile_name);

    Poco::Util::AbstractConfiguration::Keys config_keys;
    config.keys(elem, config_keys);

    for (const std::string & key : config_keys)
    {
        if (key == "constraints")
            continue;
        if (key == "profile" || key.starts_with("profile["))   /// Inheritance of profiles from the current one.
            setProfile(config.getString(elem + "." + key), config);
        else
            set(key, config.getString(elem + "." + key));
    }
}

void SettingsImpl::loadSettingsFromConfig(const String & path, const Poco::Util::AbstractConfiguration & config)
{
    if (!config.has(path))
        throw Exception(ErrorCodes::NO_ELEMENTS_IN_CONFIG, "There is no path '{}' in configuration file.", path);

    Poco::Util::AbstractConfiguration::Keys config_keys;
    config.keys(path, config_keys);

    for (const std::string & key : config_keys)
    {
        set(key, config.getString(path + "." + key));
    }
}

void SettingsImpl::dumpToMapColumn(IColumn * column, bool changed_only)
{
    if (!column)
        return;

    auto & column_map = typeid_cast<ColumnMap &>(*column);
    auto & offsets = column_map.getNestedColumn().getOffsets();

    auto & tuple_column = column_map.getNestedData();
    auto & key_column = typeid_cast<ColumnLowCardinality &>(tuple_column.getColumn(0));
    auto & value_column = typeid_cast<ColumnLowCardinality &>(tuple_column.getColumn(1));

    size_t size = 0;

    /// Iterate over standard settings
    const auto & accessor = Traits::Accessor::instance();
    for (size_t i = 0; i < accessor.size(); i++)
    {
        if (changed_only && !accessor.isValueChanged(*this, i))
            continue;

        const auto & name = accessor.getName(i);
        auto value = accessor.getValueString(*this, i);
        key_column.insertData(name.data(), name.size());
        value_column.insertData(value.data(), value.size());
        ++size;
    }

    /// Iterate over the custom settings
    for (const auto & custom : custom_settings_map)
    {
        const auto & setting_field = custom.second;
        if (changed_only && !setting_field.changed)
            continue;

        const auto & name = custom.first;
        auto value = setting_field.toString();
        key_column.insertData(name.data(), name.size());
        value_column.insertData(value.data(), value.size());
        ++size;
    }

    offsets.push_back(offsets.back() + size);
}

void SettingsImpl::checkNoSettingNamesAtTopLevel(const Poco::Util::AbstractConfiguration & config, const String & config_path)
{
    if (config.getBool("skip_check_for_incorrect_settings", false))
        return;

    SettingsImpl settings;
    for (const auto & setting : settings.all())
    {
        const auto & name = setting.getName();
        bool should_skip_check = name == "max_table_size_to_drop" || name == "max_partition_size_to_drop";
        if (config.has(name) && (setting.getTier() != SettingsTierType::OBSOLETE) && !should_skip_check)
        {
            throw Exception(ErrorCodes::UNKNOWN_ELEMENT_IN_CONFIG, "A setting '{}' appeared at top level in config {}."
                " But it is user-level setting that should be located in users.xml inside <profiles> section for specific profile."
                " You can add it to <profiles><default> if you want to change default value of this setting."
                " You can also disable the check - specify <skip_check_for_incorrect_settings>1</skip_check_for_incorrect_settings>"
                " in the main configuration file.",
                name, config_path);
        }
    }
}

std::vector<String> SettingsImpl::getAllRegisteredNames() const
{
    std::vector<String> all_settings;
    for (const auto & setting_field : all())
        all_settings.push_back(setting_field.getName());
    return all_settings;
}

void SettingsImpl::set(std::string_view name, const Field & value)
{
    if (name == "compatibility")
    {
        if (value.getType() != Field::Types::Which::String)
            throw Exception(ErrorCodes::BAD_ARGUMENTS, "Unexpected type of value for setting 'compatibility'. Expected String, got {}", value.getTypeName());
        applyCompatibilitySetting(value.safeGet<String>());
    }
    /// If we change setting that was changed by compatibility setting before
    /// we should remove it from settings_changed_by_compatibility_setting,
    /// otherwise the next time we will change compatibility setting
    /// this setting will be changed too (and we don't want it).
    else if (settings_changed_by_compatibility_setting.contains(name))
        settings_changed_by_compatibility_setting.erase(name);

    BaseSettings::set(name, value);
}

void SettingsImpl::applyCompatibilitySetting(const String & compatibility_value)
{
    /// First, revert all changes applied by previous compatibility setting
    for (const auto & setting_name : settings_changed_by_compatibility_setting)
        resetToDefault(setting_name);

    settings_changed_by_compatibility_setting.clear();
    /// If setting value is empty, we don't need to change settings
    if (compatibility_value.empty())
        return;

    ClickHouseVersion version(compatibility_value);
    const auto & settings_changes_history = getSettingsChangesHistory();
    /// Iterate through ClickHouse version in descending order and apply reversed
    /// changes for each version that is higher that version from compatibility setting
    for (auto it = settings_changes_history.rbegin(); it != settings_changes_history.rend(); ++it)
    {
        if (version >= it->first)
            break;

        /// Apply reversed changes from this version.
        for (const auto & change : it->second)
        {
            /// In case the alias is being used (e.g. use enable_analyzer) we must change the original setting
            auto final_name = SettingsTraits::resolveName(change.name);

            /// If this setting was changed manually, we don't change it
            if (isChanged(final_name) && !settings_changed_by_compatibility_setting.contains(final_name))
                continue;

            /// Don't mark as changed if the value isn't really changed
            if (get(final_name) == change.previous_value)
                continue;

            BaseSettings::set(final_name, change.previous_value);
            settings_changed_by_compatibility_setting.insert(final_name);
        }
    }
}

/// Offset of the Data base class within SettingsImpl.
/// This accounts for the vtable pointer and any other base class overhead,
/// so that SettingIndex offsets can be applied directly to SettingsImpl*.
static const size_t SETTINGS_DATA_BASE_OFFSET = [] {
    const SettingsImpl * fake = reinterpret_cast<const SettingsImpl *>(alignof(SettingsImpl));
    const auto * data = static_cast<const SettingsTraits::Data *>(fake);
    return reinterpret_cast<const char *>(data) - reinterpret_cast<const char *>(fake);
}();

#define INITIALIZE_SETTING_EXTERN(TYPE, NAME, DEFAULT, DESCRIPTION, FLAGS, ...) \
    Settings ## TYPE NAME{SettingOffset::NAME.offset + SETTINGS_DATA_BASE_OFFSET};

namespace Setting
{
    LIST_OF_SETTINGS(INITIALIZE_SETTING_EXTERN, INITIALIZE_SETTING_EXTERN)  /// NOLINT (misc-use-internal-linkage)
}

#undef INITIALIZE_SETTING_EXTERN

Settings::Settings()
    : impl(std::make_unique<SettingsImpl>())
{}

Settings::Settings(const Settings & settings)
    : impl(std::make_unique<SettingsImpl>(*settings.impl))
{}

Settings::Settings(Settings && settings) noexcept
    : impl(std::make_unique<SettingsImpl>(std::move(*settings.impl)))
{}

Settings::~Settings() = default;

Settings & Settings::operator=(const Settings & other)
{
    if (&other == this)
        return *this;
    *impl = *other.impl;
    return *this;
}

bool Settings::operator==(const Settings & other) const
{
    return *impl == *other.impl;
}

/// Override operator[] implementation for Settings — uses SettingIndex offset instead of pointer-to-member.
/// The offset already includes the Data base class adjustment (SETTINGS_DATA_BASE_OFFSET),
/// so it can be applied directly to the SettingsImpl pointer.
#define IMPLEMENT_SETTING_SUBSCRIPT_OPERATOR_WITH_OFFSET(CLASS_NAME, TYPE) \
    const SettingField##TYPE & CLASS_NAME::operator[](CLASS_NAME##TYPE t) const \
    { \
        return *reinterpret_cast<const SettingField##TYPE *>( \
            reinterpret_cast<const char *>(impl.get()) + t.offset); \
    } \
    SettingField##TYPE & CLASS_NAME::operator[](CLASS_NAME##TYPE t) \
    { \
        return *reinterpret_cast<SettingField##TYPE *>( \
            reinterpret_cast<char *>(impl.get()) + t.offset); \
    }

COMMON_SETTINGS_SUPPORTED_TYPES(Settings, IMPLEMENT_SETTING_SUBSCRIPT_OPERATOR_WITH_OFFSET)
#undef IMPLEMENT_SETTING_SUBSCRIPT_OPERATOR_WITH_OFFSET

bool Settings::has(std::string_view name) const
{
    return impl->has(name);
}

bool Settings::isChanged(std::string_view name) const
{
    return impl->isChanged(name);
}

SettingsTierType Settings::getTier(std::string_view name) const
{
    return impl->getTier(name);
}

bool Settings::tryGet(std::string_view name, Field & value) const
{
    return impl->tryGet(name, value);
}

Field Settings::get(std::string_view name) const
{
    return impl->get(name);
}

void Settings::set(std::string_view name, const Field & value)
{
    impl->set(name, value);
}

void Settings::setDefaultValue(std::string_view name)
{
    impl->resetToDefault(name);
}

std::vector<String> Settings::getHints(const String & name) const
{
    return impl->getHints(name);
}

String Settings::toString() const
{
    return impl->toString();
}

SettingsChanges Settings::changes() const
{
    return impl->changes();
}

void Settings::applyChanges(const SettingsChanges & changes)
{
    impl->applyChanges(changes);
}

std::vector<std::string_view> Settings::getAllRegisteredNames() const
{
    std::vector<std::string_view> setting_names;
    for (const auto & setting : impl->all())
    {
        setting_names.emplace_back(setting.getName());
    }
    return setting_names;
}

std::vector<std::string_view> Settings::getAllAliasNames() const
{
    std::vector<std::string_view> alias_names;
    const auto & settings_to_aliases = SettingsImpl::Traits::settingsToAliases();
    for (const auto & [_, aliases] : settings_to_aliases)
    {
        alias_names.insert(alias_names.end(), aliases.begin(), aliases.end());
    }
    return alias_names;
}

std::vector<std::string_view> Settings::getChangedAndObsoleteNames() const
{
    std::vector<std::string_view> setting_names;
    for (const auto & setting : impl->allChanged())
    {
        if (setting.getTier() == SettingsTierType::OBSOLETE)
            setting_names.emplace_back(setting.getName());
    }
    return setting_names;
}

std::vector<std::string_view> Settings::getUnchangedNames() const
{
    std::vector<std::string_view> setting_names;
    for (const auto & setting : impl->allUnchanged())
    {
        setting_names.emplace_back(setting.getName());
    }
    return setting_names;
}

void Settings::dumpToSystemSettingsColumns(MutableColumnsAndConstraints & params) const
{
    MutableColumns & res_columns = params.res_columns;

    const auto fill_data_for_setting = [&](std::string_view setting_name, const auto & setting)
    {
        res_columns[1]->insert(setting.getValueString());
        res_columns[2]->insert(setting.isValueChanged());

        /// Trim starting/ending newline.
        std::string_view doc = setting.getDescription();
        if (!doc.empty() && doc[0] == '\n')
            doc = doc.substr(1);
        if (!doc.empty() && doc[doc.length() - 1] == '\n')
            doc = doc.substr(0, doc.length() - 1);

        res_columns[3]->insert(doc);

        Field min;
        Field max;
        std::vector<Field> disallowed_values;
        SettingConstraintWritability writability = SettingConstraintWritability::WRITABLE;
        params.constraints.get(*this, setting_name, min, max, disallowed_values, writability);

        /// These two columns can accept strings only.
        if (!min.isNull())
            min = Settings::valueToStringUtil(setting_name, min);
        if (!max.isNull())
            max = Settings::valueToStringUtil(setting_name, max);

        Array disallowed_array;
        for (const auto & value : disallowed_values)
            disallowed_array.emplace_back(Settings::valueToStringUtil(setting_name, value));

        res_columns[4]->insert(min);
        res_columns[5]->insert(max);
        res_columns[6]->insert(disallowed_array);
        res_columns[7]->insert(writability == SettingConstraintWritability::CONST);
        res_columns[8]->insert(setting.getTypeName());
        res_columns[9]->insert(setting.getDefaultValueString());
        res_columns[11]->insert(setting.getTier() == SettingsTierType::OBSOLETE);
        res_columns[12]->insert(setting.getTier());
    };

    const auto & settings_to_aliases = SettingsImpl::Traits::settingsToAliases();
    for (const auto & setting : impl->all())
    {
        const auto & setting_name = setting.getName();
        res_columns[0]->insert(setting_name);

        fill_data_for_setting(setting_name, setting);
        res_columns[10]->insert("");

        if (auto it = settings_to_aliases.find(setting_name); it != settings_to_aliases.end())
        {
            for (const auto alias : it->second)
            {
                res_columns[0]->insert(alias);
                fill_data_for_setting(alias, setting);
                res_columns[10]->insert(setting_name);
            }
        }
    }
}

void Settings::dumpToMapColumn(IColumn * column, bool changed_only) const
{
    impl->dumpToMapColumn(column, changed_only);
}

NameToNameMap Settings::toNameToNameMap() const
{
    NameToNameMap query_parameters;
    for (const auto & param : *impl)
    {
        std::string value;
        ReadBufferFromOwnString buf(param.getValueString());
        readQuoted(value, buf);
        query_parameters.emplace(param.getName(), value);
    }
    return query_parameters;
}

void Settings::write(WriteBuffer & out, SettingsWriteFormat format) const
{
    impl->write(out, format);
}

void Settings::read(ReadBuffer & in, SettingsWriteFormat format)
{
    impl->read(in, format);
}

void Settings::writeEmpty(WriteBuffer & out)
{
    BaseSettingsHelpers::writeString("", out);
}

void Settings::addToProgramOptions(boost::program_options::options_description & options)
{
    addProgramOptions(*impl, options);
}

void Settings::addToProgramOptions(std::string_view setting_name, boost::program_options::options_description & options)
{
    const auto & accessor = SettingsImpl::Traits::Accessor::instance();
    size_t index = accessor.find(setting_name);
    chassert(index != static_cast<size_t>(-1));
    auto on_program_option = boost::function1<void, const std::string &>(
            [this, setting_name](const std::string & value)
            {
                this->set(setting_name, value);
            });
    options.add(boost::shared_ptr<boost::program_options::option_description>(new boost::program_options::option_description(
            setting_name.data(), boost::program_options::value<std::string>()->composing()->notifier(on_program_option), accessor.getDescription(index).data()))); // NOLINT
}

void Settings::addToProgramOptionsAsMultitokens(boost::program_options::options_description & options) const
{
    addProgramOptionsAsMultitokens(*impl, options);
}

void Settings::addToClientOptions(Poco::Util::LayeredConfiguration &config, const boost::program_options::variables_map &options, bool repeated_settings) const
{
    for (const auto & setting : impl->all())
    {
        const auto & name = setting.getName();
        if (options.contains(name))
        {
            if (repeated_settings)
                config.setString(name, options[name].as<Strings>().back());
            else
                config.setString(name, options[name].as<String>());
        }
    }
}

Field Settings::castValueUtil(std::string_view name, const Field & value)
{
    return SettingsImpl::castValueUtil(name, value);
}

String Settings::valueToStringUtil(std::string_view name, const Field & value)
{
    return SettingsImpl::valueToStringUtil(name, value);
}

Field Settings::stringToValueUtil(std::string_view name, const String & str)
{
    return SettingsImpl::stringToValueUtil(name, str);
}

bool Settings::hasBuiltin(std::string_view name)
{
    return SettingsImpl::hasBuiltin(name);
}

std::string_view Settings::resolveName(std::string_view name)
{
    return SettingsImpl::Traits::resolveName(name);
}

void Settings::checkNoSettingNamesAtTopLevel(const Poco::Util::AbstractConfiguration & config, const String & config_path)
{
    SettingsImpl::checkNoSettingNamesAtTopLevel(config, config_path);
}

}
