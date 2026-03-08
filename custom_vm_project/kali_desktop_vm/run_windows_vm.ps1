param(
    [ValidateSet("menu", "shell", "app", "ui", "viewer", "help")]
    [string]$Mode = "menu",

    [switch]$Rebuild,
    [switch]$NewWindow
)

$parentScript = Join-Path $PSScriptRoot "..\run_windows_vm.ps1"
if (-not (Test-Path $parentScript)) {
    throw "Parent launcher not found: $parentScript"
}

$invokeArgs = @("-NoProfile", "-ExecutionPolicy", "Bypass", "-File", $parentScript, "-Mode", $Mode)
if ($Rebuild) { $invokeArgs += "-Rebuild" }
if ($NewWindow) { $invokeArgs += "-NewWindow" }

& powershell @invokeArgs
exit $LASTEXITCODE
