#pragma once
#include "sleekconfig.hpp"

// Keep this object at a stable address until Detach(). Main thread only.
// Existing callback integrations can continue using AsiSettingsExample.hpp.
class PointerSettingsExample {
public:
    PointerSettingsExample()=default;
    PointerSettingsExample(const PointerSettingsExample&)=delete;
    PointerSettingsExample& operator=(const PointerSettingsExample&)=delete;
    PointerSettingsExample(PointerSettingsExample&&)=delete;
    PointerSettingsExample& operator=(PointerSettingsExample&&)=delete;
    struct Config {
        bool enabled=true;
        float strength=1.0f;
        int32_t tint=0x3399FF;
    };
    const Config& GetConfig() const noexcept { return config_; }
    bool IsAttached() const noexcept { return static_cast<bool>(menu_); }
    bool Attach() {
        if (menu_) return true;
        ScModDesc mod{};
        mod.size=sizeof(mod);mod.required_api=SC_API_VERSION;
        mod.id="example.asi.pointers";mod.name="Pointer Settings Example";
        mod.category="Visuals";mod.persistence=SC_EXTERNAL;
        if (menu_.Open(mod)!=SC_OK) return false;
        if (!(menu_.Api()->capabilities & SC_CAP_VARIABLE_BINDING)) return Fail();

        ScSettingDesc s{};
        s.size=sizeof(s);s.id="enabled";s.label="Enable effect";
        s.type=SC_BOOL;s.default_value.type=SC_BOOL;s.default_value.integer=1;
        s.minimum=0;s.maximum=1;s.step=1;
        if (menu_.Add(s,&config_.enabled)!=SC_OK) return Fail();

        s={};s.size=sizeof(s);s.id="strength";s.label="Effect strength";
        s.type=SC_FLOAT;s.default_value.type=SC_FLOAT;s.default_value.real=1.0f;
        s.minimum=0;s.maximum=3;s.step=0.05;
        if (menu_.Add(s,&config_.strength)!=SC_OK) return Fail();

        s={};s.size=sizeof(s);s.id="tint";s.label="Effect color";
        s.type=SC_COLOR;s.default_value.type=SC_COLOR;s.default_value.integer=0x3399FF;
        if (menu_.Add(s,&config_.tint)!=SC_OK) return Fail();
        return true;
    }
    void Detach() noexcept { menu_.Close(); }
private:
    bool Fail() noexcept { menu_.Close();return false; }
    Config config_;
    sleekconfig::Registration menu_;
};
