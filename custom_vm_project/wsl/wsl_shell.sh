#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")"

g++ ../vm_cli.cpp -std=gnu++17 -O2 -o ../vm_cli
../vm_cli shell
