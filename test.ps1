$ErrorActionPreference = 'Stop'
foreach ($taskName in @('registry_tests','menu_tests','idle_tick_performance','basic_example_tests')) {
    & (Join-Path $PSScriptRoot 'build.ps1') -Project "tests\$taskName.vcxproj"
    $taskRun = Join-Path $PSScriptRoot "tests\run-$taskName"
    New-Item -ItemType Directory -Force -Path $taskRun | Out-Null
    Push-Location -LiteralPath $taskRun
    try {
        & (Join-Path $PSScriptRoot "tests\bin\$taskName.exe")
        if ($LASTEXITCODE -ne 0) { throw "$taskName failed." }
    } finally { Pop-Location }
    Write-Output ($taskName + ': PASS')
}
