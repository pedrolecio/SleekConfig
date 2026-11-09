# Settings: quick reference

Start from a registration block in [BasicSettings.hpp](../examples/BasicSettings.hpp).
Always set `size`, a unique `id`, a `label`, `type` and `default_value.type`.

| Control | Type | Bound variable | Default field |
|---|---|---|---|
| Checkbox | `SC_BOOL` | `bool` | `integer`: 0 or 1 |
| Integer slider | `SC_INT` | `int32_t` | `integer` |
| Decimal slider | `SC_FLOAT` | `float` | `real` |
| Dropdown | `SC_ENUM` | `int32_t` | `integer`: selected index |
| Color | `SC_COLOR` | `int32_t` | `integer`: `0xRRGGBB` |
| Key | `SC_KEY` | `int32_t` | `integer`: Windows virtual-key code |
| Text | `SC_STRING` | `char[256]` | `text` |

Set `minimum`, `maximum` and `step` for numeric controls. Use `step = 0`
for unrestricted decimals. Dropdown options use `"Low|Medium|High"`, with
range `0..2` and a matching integer default.

`page` creates a tab. `group` creates a section; `"Effects/Advanced"` nests it.
`description` adds help text. `tooltip` adds hover help.

Flags:

- `SC_READ_ONLY`: disable edits.
- `SC_RELOAD_MOD`: tell the player a mod reload is needed.
- `SC_RESTART_GAME`: tell the player a game restart is needed.

Reload/restart flags are labels. Your code must control when edits take effect.
Use `SC_BUTTON` with a change callback for actions; do not bind it to a variable.

## Keep your existing INI

Load your INI as usual, then register settings and publish the loaded values:

```cpp
ScValue value{};
value.type = SC_FLOAT;
value.real = loadedStrength; // A value already read from your INI.
menu.Api()->publish_value(menu.Handle(), "strength", &value);
```

Keep the INI value in a separate snapshot: binding registration writes defaults
immediately. Publish on the game main thread, after adding the settings and
before SleekConfig's first registry tick. Saved DAT values are then applied last.
Check the return code; the value must satisfy the registered type/range/step.

Both persistence modes save settings to DAT files. `SC_EXTERNAL` can also call
your `on_save`/`on_load` handlers. `SC_MANAGED_INI` imports an existing relative
`ini_path`; it does not write menu edits back into that INI.

## Read or change a value

Read the bound variable directly during gameplay. To queue a change yourself,
use `menu.Set(id, value)`; it applies on the next game tick.
If your mod changes a variable directly, publish it to update the menu too.

## Presets

Players can save and load presets for your mod with no extra code. To ship
presets with your mod, put `.dat` files anywhere in your mod's `modloader`
folder, for example `modloader/YourMod/presets/Cinematic.dat`:

```text
SLEEKCONFIG-PRESET	1
mod	yourname.yourmod
name	Cinematic
set	strength	3	0.75
set	enabled	1	1
```

Fields are separated by tabs. `set` rows use the setting ID, the `ScType`
number and the value as written in `SleekConfig/<mod-id>.dat` (text values
are hex-encoded UTF-8). The easiest way to make one is **Save preset** in the
menu, then copy the file from `presets/<mod-id>/`. Rows for unknown IDs, wrong
types or out-of-range values are skipped when loading.

## Icons

Place a PNG named after your mod ID in `icons` beside `SleekConfig.asi`:
`icons/yourname.yourmod.png`. Restart the game to load it.

## Return codes

`SC_OK` is success. Common failures are `SC_NOT_FOUND` (framework or item
missing), `SC_DUPLICATE` (ID reused), `SC_INVALID` (bad descriptor/value) and
`SC_UNSUPPORTED` (missing capability). Use `menu.Api()->last_error(menu.Handle())`
for mod-specific error details.

The complete C API is in [sleekconfig.h](../include/sleekconfig.h).
