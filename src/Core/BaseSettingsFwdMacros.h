#pragma once

#define DECLARE_SETTING_TRAIT(CLASS_NAME, TYPE) \
    struct CLASS_NAME##TYPE##Impl; \
    using CLASS_NAME##TYPE = const CLASS_NAME##TYPE##Impl &;

#define DECLARE_SETTING_SUBSCRIPT_OPERATOR(CLASS_NAME, TYPE) \
    const SettingField##TYPE & operator[](CLASS_NAME##TYPE t) const; \
    SettingField##TYPE & operator[](CLASS_NAME##TYPE t);
