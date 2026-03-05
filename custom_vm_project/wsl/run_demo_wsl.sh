#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")"

echo "[1/6] Building vm_cli"
g++ ../vm_cli.cpp -std=gnu++17 -O2 -o ../vm_cli

echo "[2/6] Showing CLI help"
../vm_cli --help

echo "[3/6] Assembling factorial sample"
../vm_cli asm-build ../sample_factorial.asm ../sample_factorial.bin

echo "[4/6] Running factorial binary with trace"
../vm_cli run-bin ../sample_factorial.bin --trace ../trace_factorial_bin.jsonl

echo "[5/6] Dumping factorial binary"
../vm_cli dump-bin ../sample_factorial.bin

echo "[6/6] Running fibonacci sample"
../vm_cli asm-build ../sample_fibonacci.asm ../sample_fibonacci.bin
../vm_cli run-bin ../sample_fibonacci.bin --no-trace

echo "Done. WSL demo completed successfully."
