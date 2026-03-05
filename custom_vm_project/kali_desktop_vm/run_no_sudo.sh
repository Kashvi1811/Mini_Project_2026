#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
DEPLOY_DIR="$HOME/custom_vm_app"

mkdir -p "$DEPLOY_DIR"

copy_if_changed() {
  local src="$1"
  local dst="$2"

  if [[ ! -f "$src" ]]; then
    return
  fi

  if [[ ! -f "$dst" ]] || ! cmp -s "$src" "$dst"; then
    cp "$src" "$dst"
  fi
}

copy_if_changed "$ROOT_DIR/vm_cli.cpp" "$DEPLOY_DIR/vm_cli.cpp"
copy_if_changed "$ROOT_DIR/viewer.html" "$DEPLOY_DIR/viewer.html"
copy_if_changed "$ROOT_DIR/kali_ui.html" "$DEPLOY_DIR/kali_ui.html"
copy_if_changed "$ROOT_DIR/sample_factorial.asm" "$DEPLOY_DIR/sample_factorial.asm"
copy_if_changed "$ROOT_DIR/sample_fibonacci.asm" "$DEPLOY_DIR/sample_fibonacci.asm"
copy_if_changed "$ROOT_DIR/sample_addition.asm" "$DEPLOY_DIR/sample_addition.asm"
copy_if_changed "$SCRIPT_DIR/run_custom_vm.sh" "$DEPLOY_DIR/run_custom_vm.sh"
copy_if_changed "$SCRIPT_DIR/custom_vm_bashrc" "$DEPLOY_DIR/custom_vm_bashrc"

sed -i 's/\r$//' "$DEPLOY_DIR/run_custom_vm.sh" 2>/dev/null || true
sed -i 's/\r$//' "$DEPLOY_DIR/custom_vm_bashrc" 2>/dev/null || true
chmod +x "$DEPLOY_DIR/run_custom_vm.sh"

if [[ ! -x "$DEPLOY_DIR/vm_cli" || "$DEPLOY_DIR/vm_cli.cpp" -nt "$DEPLOY_DIR/vm_cli" ]]; then
  if ! command -v g++ >/dev/null 2>&1; then
    echo "g++ not found. Run once: bash $SCRIPT_DIR/install_in_kali.sh"
    exit 1
  fi
  g++ "$DEPLOY_DIR/vm_cli.cpp" -std=gnu++17 -O2 -o "$DEPLOY_DIR/vm_cli"
fi

exec "$DEPLOY_DIR/run_custom_vm.sh" "$@"
