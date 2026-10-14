# Add a CLEO script

Use [CleoSettingsExample.sc](../examples/CleoSettingsExample.sc) as a starting
point. It uses GTA3script syntax and standard CLEO dynamic-library commands.

1. Install CLEO 4 or 5 and SleekConfig.
2. Change the example's mod ID, display name and author.
3. Keep its register/add-settings section.
4. Put your effect in `main_loop`. Use `enabled`, `strength` and `tint` there.
5. Compile the script with a compiler supporting this syntax and place the
   resulting `.cs` in GTA's `CLEO` folder.

The sample loads `SleekConfig.asi` by name. For that sample, put the ASI beside
`gta_sa.exe`, or change its load path to `modloader/SleekConfig/SleekConfig.asi`.
Keep only one ASI copy. Press **F10** to edit settings; **F8** stops this demo.

How it works:

- `SC_Cleo_Register`: create the mod entry.
- `SC_Cleo_AddBoolPage` / `AddScaledFloatPage` / `AddColorPage`: add controls.
- `SC_Cleo_Revision`: detect changes.
- `SC_Cleo_GetInt` / `GetScaledFloat`: read the new values into your script.
- `SC_Cleo_Unregister`: remove the entry before the script stops.

CLEO float controls use scaled integers: `150 / 100 = 1.5`.
The sample converts the integer back to a float before using it.
If SleekConfig cannot load, it keeps its defaults.

Do not reverse-engineer the ASI. The script exports are listed in
[sleekconfig_cleo.h](../include/sleekconfig_cleo.h).
