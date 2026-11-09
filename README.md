# SleekConfig 1.1

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

- Categories, page tabs, sections, search and mod icons.
- Toggles, sliders, dropdowns, colors, text and key bindings.
- Presets for every mod: save, replace and load them from the mod's page.
- Fine sliders: Shift-drag for 10x finer control; arrow keys and the mouse
  wheel step a clicked slider.
- Save Mod, Save All, Restore Saved and Defaults.
- Custom hotkey, slider speed, accent color, background and layout.
- Native C/C++ API and CLEO bridge.

Settings are saved in `SleekConfig/<mod-id>.dat` beside the loaded ASI.
Saved values take priority over imported INI values and registered defaults.

Presets are saved in `presets/<mod-id>/<name>.dat` beside the ASI. The menu
lists presets found in any folder under `modloader` whose header names the open
mod, so mods can [ship their own presets](docs/SETTINGS.md#presets).

## What's new in 1.1

- **Presets** per mod, picked from a dropdown at the top of each mod page.
- **Finer sliders:** relative dragging, Shift for fine control, arrow-key and
  wheel steps, and step size and decimals that follow each option's range.
- **New layout:** one row per option with aligned controls, section headers,
  a fixed mod header, compact sidebar and one-line footer.
- Saved DAT files, INIs and the v1 API are unchanged; existing mods need no update.

## Credits

- **pedrowav** — SleekConfig.
- **Omar Cornut and contributors** — Dear ImGui.
- **plugin-sdk contributors** — GTA integration.
- **stb contributors** — PNG loading through plugin-sdk.

SleekConfig uses the [MIT license](LICENSE).
Dear ImGui's license is in [vendor/imgui/LICENSE.txt](vendor/imgui/LICENSE.txt).
plugin-sdk and its bundled dependencies retain their own licenses.
