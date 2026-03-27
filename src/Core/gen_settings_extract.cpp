/// Preprocessor-based settings extractor for the typed-array Data layout generator.
/// This file is preprocessed (not compiled) to extract TYPE/NAME/DEFAULT from all settings.
///
/// Usage: clang -E -P -I<src> -I<src/Core> -I<base> gen_settings_extract.cpp | grep '__SETTING__'
///
/// The output is parsed by gen_settings_data.sh to produce SettingsData.generated.h.

/// Stub out everything except the settings list macros
#define DECLARE_SETTINGS_TRAITS_ALLOW_CUSTOM_SETTINGS(...)
#define IMPLEMENT_SETTINGS_TRAITS(...)
#define DECLARE_SETTING_ENUM(...)
#define DECLARE_SETTING_MULTI_ENUM(...)
#define IMPLEMENT_SETTING_ENUM(...)
#define IMPLEMENT_SETTING_MULTI_ENUM(...)
#define IMPLEMENT_SETTING_AUTO_ENUM(...)

/// Provide stubs for C++ constructs used in Settings.cpp that the preprocessor would choke on
/// We only need the preprocessor to expand the settings list macros, nothing else.

/// Extraction macros — emit one parseable line per setting
#define EXTRACT_DECLARE(TYPE, NAME, DEFAULT, DESCRIPTION, FLAGS, ...) \
    __SETTING__ TYPE __SEP__ NAME __SEP__ DEFAULT __END__

#define EXTRACT_DECLARE_ALIAS(TYPE, NAME, DEFAULT, DESCRIPTION, FLAGS, ALIAS, ...) \
    __SETTING__ TYPE __SEP__ NAME __SEP__ DEFAULT __END__

/// Pull in settings definitions
#include <Core/SettingsObsoleteMacros.h>
#include <Core/FormatFactorySettings.h>

/// Include the settings list macros from Settings.cpp.
/// We use a separate .inc file to avoid pulling in the entire Settings.cpp.
#include <Core/SettingsList.inc>

/// Compose the full list (same as LIST_OF_SETTINGS in Settings.cpp)
#define LIST_OF_SETTINGS(M, ALIAS) \
    COMMON_SETTINGS(M, ALIAS) \
    OBSOLETE_SETTINGS(M, ALIAS) \
    FORMAT_FACTORY_SETTINGS(M, ALIAS) \
    OBSOLETE_FORMAT_SETTINGS(M, ALIAS)

/// Expand
LIST_OF_SETTINGS(EXTRACT_DECLARE, EXTRACT_DECLARE_ALIAS)
