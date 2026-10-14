#pragma once
#include "sleekconfig.h"
#include <Windows.h>
#include <utility>

namespace sleekconfig {
inline const ScApi* FindApi() noexcept {
    HMODULE module = GetModuleHandleA("SleekConfig.asi");
    if (!module) return nullptr;
    auto get = reinterpret_cast<ScGetApiFn>(GetProcAddress(module, "SleekConfig_GetApi"));
    return get ? get(SC_API_VERSION, sizeof(ScApi)) : nullptr;
}
inline ScRegisterBoundSettingFn FindBoundSettingApi() noexcept {
    HMODULE module = GetModuleHandleA("SleekConfig.asi");
    return module ? reinterpret_cast<ScRegisterBoundSettingFn>(
        GetProcAddress(module, "SleekConfig_RegisterBoundSetting")) : nullptr;
}
class Registration {
public:
    Registration() = default;
    ~Registration() { Close(); }
    Registration(const Registration&) = delete;
    Registration& operator=(const Registration&) = delete;
    Registration(Registration&& other) noexcept { *this = std::move(other); }
    Registration& operator=(Registration&& other) noexcept {
        if (this != &other) { Close(); api_=other.api_; mod_=other.mod_;
            other.api_=nullptr; other.mod_=0; } return *this;
    }
    ScResult Open(const ScModDesc& desc) noexcept {
        Close(); api_=FindApi();
        if (!api_) return SC_NOT_FOUND;
        const auto result=api_->register_mod(&desc,&mod_);
        if (result!=SC_OK) { api_=nullptr; mod_=0; }
        return result;
    }
    void Close() noexcept { if (api_ && mod_) api_->unregister_mod(mod_);
        api_=nullptr; mod_=0; }
    explicit operator bool() const noexcept { return api_ && mod_; }
    ScResult Add(const ScSettingDesc& desc) const noexcept {
        return *this ? api_->register_setting(mod_,&desc) : SC_NOT_FOUND;
    }
    ScResult Add(const ScSettingDesc& desc, const ScBindingDesc& binding) const noexcept {
        if (!*this) return SC_NOT_FOUND;
        if (!(api_->capabilities & SC_CAP_VARIABLE_BINDING)) return SC_UNSUPPORTED;
        const auto add=FindBoundSettingApi();
        return add ? add(mod_,&desc,&binding) : SC_UNSUPPORTED;
    }
    ScResult Add(const ScSettingDesc& desc, bool* variable) const noexcept {
        static_assert(sizeof(bool)==1, "BOOL8 requires one-byte bool storage");
        return Add(desc, ScBindingDesc{sizeof(ScBindingDesc),SC_STORAGE_BOOL8,
            variable,sizeof(bool)});
    }
    ScResult Add(const ScSettingDesc& desc, int32_t* variable) const noexcept {
        return Add(desc, ScBindingDesc{sizeof(ScBindingDesc),SC_STORAGE_INT32,
            variable,sizeof(int32_t)});
    }
    ScResult Add(const ScSettingDesc& desc, float* variable) const noexcept {
        return Add(desc, ScBindingDesc{sizeof(ScBindingDesc),SC_STORAGE_FLOAT,
            variable,sizeof(float)});
    }
    template<size_t N>
    ScResult Add(const ScSettingDesc& desc, char (&variable)[N]) const noexcept {
        static_assert(N>=SC_TEXT_CAP, "Text storage needs at least SC_TEXT_CAP bytes");
        return Add(desc, ScBindingDesc{sizeof(ScBindingDesc),SC_STORAGE_TEXT,
            variable,static_cast<uint32_t>(N)});
    }
    ScResult Get(const char* id, ScValue& value) const noexcept {
        return *this ? api_->get_value(mod_,id,&value) : SC_NOT_FOUND;
    }
    ScResult Set(const char* id, const ScValue& value) const noexcept {
        return *this ? api_->set_value(mod_,id,&value) : SC_NOT_FOUND;
    }
    ScMod Handle() const noexcept { return mod_; }
    const ScApi* Api() const noexcept { return api_; }
private:
    const ScApi* api_{};
    ScMod mod_{};
};
}
