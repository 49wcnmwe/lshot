param([string]$Baseline = '00a4291b38ac3d0115a2139c6f71e5c22f975951')
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$repoRoot = Split-Path $projectRoot -Parent
$files = @('cpp/src/ui/selection_info/panel.h', 'cpp/src/ui/selection_info/panel.cpp',
           'cpp/src/ui/selection_info/controller.h', 'cpp/src/ui/selection_info/controller.cpp')
$current = @{}
$utf8 = New-Object System.Text.UTF8Encoding($false)
try {
    foreach ($file in $files) {
        $path = Join-Path $repoRoot $file
        $current[$file] = [System.IO.File]::ReadAllBytes($path)
        $original = & git -C $repoRoot show "${Baseline}:$file"
        if ($LASTEXITCODE -ne 0) { throw "Cannot read regression baseline: $file" }
        [System.IO.File]::WriteAllText($path, ($original -join "`n") + "`n", $utf8)
    }
    & "$PSScriptRoot/build.ps1" -Configuration Release -SkipTests -NoUpdates -Targets jietuba_cpp,jietuba_mint_tests
    if ($LASTEXITCODE -ne 0) { throw 'Regression baseline did not compile' }
    $env:QT_QPA_PLATFORM = 'offscreen'
    # Windows GUI executables do not send QtTest text to this console; always request a report.
    $reportPath = Join-Path $projectRoot 'build/msvc/test_reports/capture-ratio-baseline.txt'
    & "$projectRoot/build/msvc/Release/jietuba_mint_tests.exe" capture_ratio_starts_square_and_applies_explicitly capture_ratio_free_requires_apply_and_survives_restart capture_ratio_remembers_applied_values_only capture_ratio_constrains_initial_drag -o "${reportPath},txt"
    $testExit = $LASTEXITCODE
    $report = Get-Content -LiteralPath $reportPath -Raw
    $report | Write-Output
    if ($testExit -eq 0 -or $report -notmatch '4 failed') { throw "The four ratio regressions must fail on the prior implementation (exit $testExit)" }
} finally {
    foreach ($file in $current.Keys) { [System.IO.File]::WriteAllBytes((Join-Path $repoRoot $file), $current[$file]) }
    Remove-Item Env:QT_QPA_PLATFORM -ErrorAction SilentlyContinue
    Remove-Item Env:JIETUBA_TEST_SANDBOX -ErrorAction SilentlyContinue
}
exit 0
