#pragma once
#include "sleekconfig.hpp"
#include <cstring>

// Add SleekConfig/include to your project's include directories.
// Call Attach and Detach from your mod's game-main-thread lifecycle.
class AsiSettingsExample {
public:
    struct Config {
        bool enabled = true;
        float strength = 1.0f;
        int tint = 0x3399FF;
    };

    const Config& GetConfig() const noexcept { return config_; }
    bool IsAttached() const noexcept { return static_cast<bool>(menu_); }

    bool Attach() {
        if (menu_) return true;
        ScModDesc mod{};
        mod.size = sizeof(mod);
        mod.required_api = SC_API_VERSION;
        mod.id = "example.asi.effect";
        mod.name = "My ASI Effect";
        mod.author = "Your name";
        mod.description = "An example effect with live settings.";
        mod.version = "1.0";
        mod.category = "Visuals";
        mod.persistence = SC_EXTERNAL;
        mod.on_change = &Changed;
        mod.user = this;
        if (menu_.Open(mod) != SC_OK) return false;
        if (!(menu_.Api()->capabilities & SC_CAP_COLOR)) return Fail();

        ScSettingDesc s{};
        s.size = sizeof(s);
        s.id = "enabled";
        s.label = "Enable effect";
        s.page = "General";
        s.group = "Effect";
        s.type = SC_BOOL;
        s.default_value.type = SC_BOOL;
        s.default_value.integer = 1;
        s.minimum = 0;
        s.maximum = 1;
        s.step = 1;
        if (menu_.Add(s) != SC_OK) return Fail();

        s = {};
        s.size = sizeof(s);
        s.id = "strength";
        s.label = "Effect strength";
        s.page = "General";
        s.group = "Effect";
        s.description = "Applied during gameplay.";
        s.tooltip = "1.0 is the normal strength.";
        s.type = SC_FLOAT;
        s.default_value.type = SC_FLOAT;
        s.default_value.real = 1.0f;
        s.minimum = 0.0;
        s.maximum = 3.0;
        s.step = 0.05;
        if (menu_.Add(s) != SC_OK) return Fail();

        s = {};
        s.size = sizeof(s);
        s.id = "tint";
        s.label = "Effect color";
        s.page = "Appearance";
        s.group = "Colors";
        s.type = SC_COLOR;
        s.default_value.type = SC_COLOR;
        s.default_value.integer = 0x3399FF;
        if (menu_.Add(s) != SC_OK) return Fail();
        return true;
    }

    void Detach() noexcept { menu_.Close(); }

private:
    bool Fail() noexcept { menu_.Close(); return false; }

    static int SC_CALL Changed(void* user, const char* id,
                               const ScValue* proposed) noexcept {
        if (!user || !id || !proposed) return 0;
        auto& cfg = static_cast<AsiSettingsExample*>(user)->config_;
        if (std::strcmp(id, "enabled") == 0 && proposed->type == SC_BOOL)
            cfg.enabled = proposed->integer != 0;
        else if (std::strcmp(id, "strength") == 0 && proposed->type == SC_FLOAT)
            cfg.strength = proposed->real;
        else if (std::strcmp(id, "tint") == 0 && proposed->type == SC_COLOR)
            cfg.tint = proposed->integer;
        else
            return 0;
        return 1;
    }

    Config config_;
    sleekconfig::Registration menu_;
};
