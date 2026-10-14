param(
    [ValidateSet('Release','Debug')] [string]$Configuration = 'Release',
    [string]$Project = 'SleekConfig.vcxproj'
)
$ErrorActionPreference = 'Stop'
if (-not $env:PLUGIN_SDK_DIR -or -not (Test-Path -LiteralPath $env:PLUGIN_SDK_DIR)) {
    throw 'Set PLUGIN_SDK_DIR to your plugin-sdk folder. See docs/BUILD.md.'
}
$taskVswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$taskVs = & $taskVswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $taskVs) { throw 'Install Visual Studio 2022 C++ tools.' }
$taskMsbuild = Join-Path $taskVs 'MSBuild\Current\Bin\MSBuild.exe'
$taskStart = [Diagnostics.ProcessStartInfo]::new()
$taskStart.FileName = $taskMsbuild
$taskStart.UseShellExecute = $false
$taskStart.EnvironmentVariables.Clear()
$taskEnvironment = [Environment]::GetEnvironmentVariables('Process')
foreach ($taskKey in $taskEnvironment.Keys) {
    $taskStart.EnvironmentVariables[$taskKey.ToUpperInvariant()] = $taskEnvironment[$taskKey]
}
$taskArguments = @((Join-Path $PSScriptRoot $Project),
    "/p:Configuration=$Configuration", '/p:Platform=Win32', '/m:2', '/v:minimal', '/nologo', '/nodeReuse:false')
$taskStart.Arguments = ($taskArguments | ForEach-Object { '"' + $_ + '"' }) -join ' '
$taskProcess = [Diagnostics.Process]::Start($taskStart)
$taskProcess.WaitForExit()
if ($taskProcess.ExitCode -ne 0) { throw 'Build failed.' }
Write-Output ("Built $Project ($Configuration/Win32).")
