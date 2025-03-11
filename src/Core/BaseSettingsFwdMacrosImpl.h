#pragma once

#define IMPLEMENT_SETTING_SUBSCRIPT_OPERATOR(CLASS_NAME, TYPE) \
    struct CLASS_NAME##TYPE##Impl \
    { \
        std::vector<SettingField##TYPE> CLASS_NAME##Impl::* data; \
        UInt16* position; \
    }; \
    const SettingField##TYPE & CLASS_NAME::operator[](CLASS_NAME##TYPE & t) const \
    { \
        return (impl.get()->*t.data)[*t.position]; \
    } \
    SettingField##TYPE & CLASS_NAME::operator[](CLASS_NAME##TYPE & t) \
    { \
        return (impl.get()->*t.data)[*t.position]; \
    }
