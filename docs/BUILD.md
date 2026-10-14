# Build on Windows

You need **Visual Studio 2022**, Desktop development with C++, the **v143**
toolset and a Windows 10/11 SDK. Build **Win32**, not x64.

## 1. Get plugin-sdk

Clone [plugin-sdk](https://github.com/DK22Pac/plugin-sdk). In PowerShell, set its path:
This release was built against commit `5d7c561152ca2b1dfa0321ca9d35399c2dc99f82`.

```powershell
$env:PLUGIN_SDK_DIR = 'C:\dev\plugin-sdk'
```

Build `plugin_sa/Plugin_SA.vcxproj` in **Release | Win32** using Visual Studio.
It produces `output/lib/Plugin.lib`. For a Debug SleekConfig build, also build
the SDK's **zDebug | Win32** configuration to produce `Plugin_d.lib`.

## 2. Build SleekConfig

From this repository:

```powershell
./build.ps1
```

Output: `bin/Release/SleekConfig.asi`.
Dear ImGui is already included; injector and stb headers come from plugin-sdk.

For Debug, run `./build.ps1 -Configuration Debug`.

## 3. Run the tests

```powershell
./test.ps1
```

Tests cover registry, saved data, bound variables, menu controls, idle
allocations and the beginner example. They do not require GTA to be running.
The CLEO sample is source-only; it is not compiled by this command.
