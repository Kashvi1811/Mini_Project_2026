Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Invoke-Checked {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Label,

        [Parameter(Mandatory = $true)]
        [scriptblock]$Action
    )

    Write-Host "`n==> $Label" -ForegroundColor Cyan
    & $Action
    if ($LASTEXITCODE -ne 0) {
        throw "Step failed: $Label (exit code: $LASTEXITCODE)"
    }
}

Set-Location $PSScriptRoot

$compiler = Get-Command g++ -ErrorAction SilentlyContinue
if (-not $compiler) {
    throw "g++ not found in PATH. Install MinGW/LLVM g++ and retry."
}

$sourceCandidates = @(
    (Join-Path $PSScriptRoot "kali_desktop_vm\vm_cli.cpp"),
    (Join-Path $PSScriptRoot "vm_cli.cpp")
)

$availableSources = $sourceCandidates | Where-Object { Test-Path $_ }
if (-not $availableSources -or $availableSources.Count -eq 0) {
    throw "Could not find vm_cli.cpp source (checked kali_desktop_vm\\vm_cli.cpp and vm_cli.cpp)."
}

$exePath = Join-Path $PSScriptRoot "vm_engine.exe"
$sourcePath = $null

foreach ($candidate in $availableSources) {
    Write-Host "`n==> Build VM engine from $candidate" -ForegroundColor Cyan
    & g++ $candidate -std=c++17 -O2 -o $exePath
    if ($LASTEXITCODE -eq 0) {
        $sourcePath = $candidate
        break
    }
}

if (-not $sourcePath) {
    throw "Build failed for all vm_cli.cpp candidates."
}

Write-Host "Using source: $sourcePath" -ForegroundColor DarkCyan

$programs = @(
    @{ Name = "factorial"; Asm = "sample_factorial.asm"; Bin = "sample_factorial.bin"; Trace = "trace_factorial_bin.jsonl" },
    @{ Name = "fibonacci"; Asm = "sample_fibonacci.asm"; Bin = "sample_fibonacci.bin"; Trace = "trace_fibonacci_bin.jsonl" },
    @{ Name = "addition"; Asm = "sample_addition.asm"; Bin = "sample_addition.bin"; Trace = "trace_addition_bin.jsonl" }
)

foreach ($program in $programs) {
    if (-not (Test-Path (Join-Path $PSScriptRoot $program.Asm))) {
        throw "Missing sample program: $($program.Asm)"
    }

    Invoke-Checked "Assemble $($program.Name)" {
        & $exePath asm-build $program.Asm $program.Bin
    }

    Invoke-Checked "Run binary $($program.Bin)" {
        & $exePath run-bin $program.Bin --trace $program.Trace
    }

    Invoke-Checked "Trace summary $($program.Trace)" {
        & $exePath trace-summary $program.Trace
    }
}

Write-Host "`nDemo completed successfully." -ForegroundColor Green
Write-Host "Engine: $exePath"
Write-Host "Generated binaries and traces are in: $PSScriptRoot"
