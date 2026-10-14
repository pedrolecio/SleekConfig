#pragma once
#include "sleekconfig.hpp"

// Keep one instance alive, at a stable address, on the game main thread.
class BasicSettings {
public:
    BasicSettings() = default;
    BasicSettings(const BasicSettings&) = delete;
    BasicSettings& operator=(const BasicSettings&) = delete;
    struct Config {
        bool enabled = true;
        float strength = 1.0f;
    };
    const Config& GetConfig() const { return config_; }

    bool Attach() {
        if (menu_) return true;
        ScModDesc mod{};
        mod.size = sizeof(mod);
        mod.required_api = SC_API_VERSION;
        mod.id = "yourname.yourmod";
        mod.name = "Your Mod";
        mod.author = "Your name";
        mod.version = "1.0";
        mod.category = "Gameplay";
        mod.persistence = SC_EXTERNAL;
        if (menu_.Open(mod) != SC_OK) return false;

        ScSettingDesc setting{};
        setting.size = sizeof(setting);
        setting.id = "enabled";
        setting.label = "Enabled";
        setting.page = "General";
        setting.type = SC_BOOL;
        setting.default_value.type = SC_BOOL;
        setting.default_value.integer = 1;
        setting.minimum = 0;
        setting.maximum = 1;
        setting.step = 1;
        if (menu_.Add(setting, &config_.enabled) != SC_OK) return Fail();

        setting = {};
        setting.size = sizeof(setting);
        setting.id = "strength";
        setting.label = "Strength";
        setting.page = "General";
        setting.type = SC_FLOAT;
        setting.default_value.type = SC_FLOAT;
        setting.default_value.real = 1.0f;
        setting.minimum = 0.0;
        setting.maximum = 3.0;
        setting.step = 0.05;
        if (menu_.Add(setting, &config_.strength) != SC_OK) return Fail();
        return true;
    }
    void Detach() { menu_.Close(); }

private:
    bool Fail() { menu_.Close(); return false; }
    Config config_;
    sleekconfig::Registration menu_;
};
