# Add your ASI mod to SleekConfig

You need your mod's C++ source. This adds settings to the menu; your gameplay
code must read those settings for them to do anything.

## 1. Copy three files

Copy these into your mod's source folder:

- [`sleekconfig.h`](../include/sleekconfig.h)
- [`sleekconfig.hpp`](../include/sleekconfig.hpp)
- [`BasicSettings.hpp`](../examples/BasicSettings.hpp)

Build your mod as **Win32/x86**. You do not need a SleekConfig import library.

## 2. Give the mod your name

Open `BasicSettings.hpp` and change:

```cpp
mod.id = "yourname.yourmod";  // Unique ID. Keep it stable across updates.
mod.name = "Your Mod";       // Name in the menu.
mod.author = "Your name";
```

IDs use lowercase letters, numbers, `.`, `_` or `-`.
Setting IDs are unique inside your mod; mod IDs are unique across all mods.

## 3. Keep one settings object alive

Add this to your existing mod:

```cpp
#include "BasicSettings.hpp"

static BasicSettings settings;
```

The example contains `enabled` and `strength`. SleekConfig writes changes
directly into those variables. No callback is needed for these two controls.

## 4. Attach, then use the values

Call these lines from your existing **game-main-thread** update:

```cpp
settings.Attach(); // Does nothing after successful registration.
const auto& cfg = settings.GetConfig();
```

Use `cfg.enabled` to decide whether your effect runs, and `cfg.strength` where
your effect currently uses its strength value. For example:

```cpp
if (cfg.enabled) {
    // Replace this line with your own effect function.
    UpdateYourEffect(cfg.strength);
}
```

With plugin-sdk, use your existing `Events::gameProcessEvent` handler.
Do not call `Attach()` from a worker thread or render callback.
If SleekConfig is absent or incompatible, the example keeps its defaults.

## 5. Try it

1. Install your ASI and SleekConfig.
2. Start GTA and press **F10**.
3. Select your mod and change **Enabled** or **Strength**.
4. Click **Save mod**, restart GTA, and check that the values return.

The data file is `SleekConfig/<mod-id>.dat` beside `SleekConfig.asi`.
SleekConfig creates it when the player saves.

## Add more controls

Copy one of the registration blocks in `BasicSettings.hpp`. Change its ID,
label, type, default and range. See the [short settings reference](SETTINGS.md).

Three rules:

- Keep the settings object and bound variables alive at a stable address.
- Call `settings.Detach()` before unloading your ASI.
- Registration writes the declared default immediately. If your mod already
  loads an INI, [publish its values after registration](SETTINGS.md#keep-your-existing-ini).

Use a [change callback](AsiSettingsExample.hpp) when an edit must also rebuild
an effect or can be rejected. For restart-only settings, bind pending values
and mark the control `SC_RESTART_GAME`; the flag alone does not delay writes.
