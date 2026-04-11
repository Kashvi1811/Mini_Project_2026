param(
    [ValidateSet("menu", "shell", "app", "ui", "viewer", "process-visualizer", "help")]
    [string]$Mode = "menu",

    [ValidateSet("auto", "vm_cli", "vm_cli_2")]
    [string]$CliTarget = "auto",

    [switch]$Rebuild,

    [switch]$NewWindow
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

Set-Location $PSScriptRoot

$sourceCandidates = switch ($CliTarget) {
    "vm_cli" {
        @((Join-Path $PSScriptRoot "vm_cli.cpp"))
    }
    "vm_cli_2" {
        @((Join-Path $PSScriptRoot "vm_cli_2.cpp"))
    }
    default {
        @(
            (Join-Path $PSScriptRoot "vm_cli_2.cpp"),
            (Join-Path $PSScriptRoot "vm_cli.cpp")
        )
    }
}

$sourcePath = $sourceCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $sourcePath) {
    throw "Could not find matching source for CliTarget '$CliTarget' in custom_vm_project."
}

$sourceBase = [System.IO.Path]::GetFileNameWithoutExtension($sourcePath)
$exeName = "$sourceBase.exe"
$exePath = Join-Path $PSScriptRoot $exeName
$shouldBuild = $Rebuild -or -not (Test-Path $exePath) -or ((Get-Item $sourcePath).LastWriteTime -gt (Get-Item $exePath).LastWriteTime)

if ($shouldBuild) {
    $compiler = Get-Command g++ -ErrorAction SilentlyContinue
    if (-not $compiler) {
        if (Test-Path $exePath) {
            Write-Warning "g++ not found. Using existing $exeName."
        } else {
            throw "g++ not found in PATH. Install MinGW/LLVM g++ or run from a machine with g++."
        }
    } else {
        Write-Host "Building $exeName from: $sourcePath" -ForegroundColor Cyan
        & g++ $sourcePath -std=gnu++17 -O2 -o $exePath
        if ($LASTEXITCODE -ne 0) {
            throw "Build failed (exit code: $LASTEXITCODE)."
        }
    }
}

if (-not (Test-Path $exePath)) {
    throw "$exeName was not found and could not be built."
}

if ($NewWindow) {
    $vmArgs = switch ($Mode) {
        "menu" { "menu" }
        "shell" { "shell" }
        "app" { "kali-app --open" }
        "ui" { "kali-ui --open" }
        "viewer" { "viewer --open" }
        "process-visualizer" { "process-visualizer --open" }
        "help" { "help" }
    }

    $exeQuoted = '"' + $exePath + '"'
    $cmdLine = 'title Custom VM Terminal && cd /d "' + $PSScriptRoot + '" && ' + $exeQuoted + ' ' + $vmArgs
    Start-Process -FilePath "cmd.exe" -WorkingDirectory $PSScriptRoot -ArgumentList "/k", $cmdLine | Out-Null
    exit 0
}

switch ($Mode) {
    "menu" {
        & $exePath menu
    }
    "shell" {
        & $exePath shell
    }
    "app" {
        & $exePath kali-app --open
    }
    "ui" {
        & $exePath kali-ui --open
    }
    "viewer" {
        & $exePath viewer --open
    }
    "process-visualizer" {
        & $exePath process-visualizer --open
    }
    "help" {
        & $exePath help
    }
}

if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}
