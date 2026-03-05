# WSL Support Pack for Custom VM

This folder contains WSL/Linux-specific scripts to run the same VM features from your project.

## What works in WSL
- Build and run CLI VM engine (`vm_cli.cpp`)
- Preset programs (`fact`, `fib`)
- ASM pipeline (`asm`, `asm-build`, `run-bin`, `dump-bin`)
- Trace generation and summary
- Browser-based UI files (`viewer.html`, `kali_ui.html`) via `xdg-open`/`wslview`

## Windows-only features
- `kali-app` command (`kali_ui.hta` + PowerShell host) is Windows-specific.
- In WSL, use UI open script from this folder.

## Quick setup in WSL
From project root (`custom_vm_project`) in WSL:

```bash
cd wsl
chmod +x run_demo_wsl.sh open_ui_wsl.sh wsl_shell.sh
make build
```

## Common commands (WSL)

```bash
# Help
../vm_cli --help

# Presets
../vm_cli fact 5
../vm_cli fib 8

# ASM to binary workflow
../vm_cli asm-build ../sample_factorial.asm ../sample_factorial.bin
../vm_cli run-bin ../sample_factorial.bin --trace ../trace_factorial_bin.jsonl
../vm_cli dump-bin ../sample_factorial.bin
../vm_cli trace-summary ../trace_factorial_bin.jsonl
```

## One-command demo

```bash
./run_demo_wsl.sh
```

## Open UI in WSL

```bash
./open_ui_wsl.sh kali
./open_ui_wsl.sh viewer
```

## Interactive shell (themed)

```bash
./wsl_shell.sh
```
