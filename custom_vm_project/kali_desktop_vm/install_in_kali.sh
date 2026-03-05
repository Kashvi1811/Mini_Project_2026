#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
DEPLOY_DIR="$HOME/custom_vm_app"
PACKAGES=(build-essential xfce4-terminal xdg-utils dbus-x11)

echo "[1/6] Installing base tools"
MISSING_PACKAGES=()
for pkg in "${PACKAGES[@]}"; do
	if ! dpkg -s "$pkg" >/dev/null 2>&1; then
		MISSING_PACKAGES+=("$pkg")
	fi
done

if [[ ${#MISSING_PACKAGES[@]} -gt 0 ]]; then
	sudo apt update
	sudo apt install -y "${MISSING_PACKAGES[@]}"
else
	echo "All required packages already installed; skipping sudo install."
fi

echo "[2/6] Preparing deployment folder: $DEPLOY_DIR"
mkdir -p "$DEPLOY_DIR"

# copy runtime assets
cp "$ROOT_DIR/vm_cli.cpp" "$DEPLOY_DIR/"
cp "$ROOT_DIR/viewer.html" "$DEPLOY_DIR/"
cp "$ROOT_DIR/kali_ui.html" "$DEPLOY_DIR/"
cp "$ROOT_DIR/sample_factorial.asm" "$DEPLOY_DIR/"
cp "$ROOT_DIR/sample_fibonacci.asm" "$DEPLOY_DIR/"
cp "$ROOT_DIR/sample_addition.asm" "$DEPLOY_DIR/"
cp "$(cd "$(dirname "$0")" && pwd)/run_custom_vm.sh" "$DEPLOY_DIR/"
cp "$(cd "$(dirname "$0")" && pwd)/run_no_sudo.sh" "$DEPLOY_DIR/"
cp "$(cd "$(dirname "$0")" && pwd)/custom_vm_bashrc" "$DEPLOY_DIR/"
chmod +x "$DEPLOY_DIR/run_custom_vm.sh"
chmod +x "$DEPLOY_DIR/run_no_sudo.sh"
chmod +x "$(cd "$(dirname "$0")" && pwd)/run_no_sudo.sh"

# Ensure Linux line endings for scripts/config copied from Windows workspace
sed -i 's/\r$//' "$DEPLOY_DIR/run_custom_vm.sh"
sed -i 's/\r$//' "$DEPLOY_DIR/run_no_sudo.sh"
sed -i 's/\r$//' "$DEPLOY_DIR/custom_vm_bashrc"

echo "[3/6] Building vm_cli"
g++ "$DEPLOY_DIR/vm_cli.cpp" -std=gnu++17 -O2 -o "$DEPLOY_DIR/vm_cli"

echo "[4/6] Installing desktop launcher"
install -Dm644 "$(cd "$(dirname "$0")" && pwd)/custom-vm.desktop" "$HOME/.local/share/applications/custom-vm.desktop"

# patch launcher path
sed -i "s|__APP_DIR__|$DEPLOY_DIR|g" "$HOME/.local/share/applications/custom-vm.desktop"

echo "[5/6] Installing autostart entry"
mkdir -p "$HOME/.config/autostart"
cp "$(cd "$(dirname "$0")" && pwd)/custom-vm-autostart.desktop" "$HOME/.config/autostart/custom-vm-autostart.desktop"
sed -i "s|__APP_DIR__|$DEPLOY_DIR|g" "$HOME/.config/autostart/custom-vm-autostart.desktop"

echo "[6/6] Done"
echo "Launch from menu: Custom VM Interface"
echo "Or run: $DEPLOY_DIR/run_no_sudo.sh"

BASHRC="$HOME/.bashrc"
sed -i "/^alias custrun='/d" "$BASHRC" 2>/dev/null || true
if ! grep -q "alias custrun='~/custom_vm_app/run_no_sudo.sh --window'" "$BASHRC" 2>/dev/null; then
	echo "alias custrun='~/custom_vm_app/run_no_sudo.sh --window'" >> "$BASHRC"
fi
sed -i "/^alias custui='/d" "$BASHRC" 2>/dev/null || true
if ! grep -q "alias custui='~/custom_vm_app/run_no_sudo.sh --ui --window'" "$BASHRC" 2>/dev/null; then
	echo "alias custui='~/custom_vm_app/run_no_sudo.sh --ui --window'" >> "$BASHRC"
fi
echo "Aliases added/verified: custrun, custui (open a new terminal or run: source ~/.bashrc)"
