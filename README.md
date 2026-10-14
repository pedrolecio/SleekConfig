# SleekConfig 1.0

A shared in-game settings menu for **GTA San Andreas 1.0 US (32-bit PC)**.
Mods register their settings; SleekConfig provides the controls and saves values.

## Install

1. Install an ASI loader and Mod Loader.
2. Download the ZIP from [Releases](https://github.com/pedrolecio/SleekConfig/releases).
3. Copy its `modloader` folder beside `gta_sa.exe`, or install the ZIP through MO2.
4. Launch the game and press **F10**.

Install compatible mods separately. Keep only one active `SleekConfig.asi`.

## Adapt your mod

Start with **[the simple C++ tutorial](docs/INTEGRATION.md)**.
It connects a checkbox and slider directly to your mod's variables.

- [Working example](examples/BasicSettings.hpp)
- [CLEO integration](docs/CLEO.md)
- [Settings reference](docs/SETTINGS.md)
- [Build SleekConfig](docs/BUILD.md)

## Features

- Categories, page tabs, search and mod icons.
- Toggles, sliders, dropdowns, colors, text and key bindings.
- Save Mod, Save All, Restore Saved and Defaults.
- Custom hotkey, accent color and background brightness.
- Native C/C++ API and CLEO bridge.

Settings are saved in `SleekConfig/<mod-id>.dat` beside the loaded ASI.
Saved values take priority over imported INI values and registered defaults.

## Credits

- **pedrowav** — SleekConfig.
- **Omar Cornut and contributors** — Dear ImGui.
- **plugin-sdk contributors** — GTA integration.
- **stb contributors** — PNG loading through plugin-sdk.

SleekConfig uses the [MIT license](LICENSE).
Dear ImGui's license is in [vendor/imgui/LICENSE.txt](vendor/imgui/LICENSE.txt).
plugin-sdk and its bundled dependencies retain their own licenses.
