#!/usr/bin/env bash
set -euo pipefail

APP_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$APP_DIR"

PARENT_DIR="$(cd "$APP_DIR/.." && pwd)"
OPEN_UI=0
USE_VM_SHELL=0
FORCE_WINDOW=0

for arg in "$@"; do
  if [[ "$arg" == "--ui" ]]; then
    OPEN_UI=1
  elif [[ "$arg" == "--vm-shell" ]]; then
    USE_VM_SHELL=1
  elif [[ "$arg" == "--window" ]]; then
    FORCE_WINDOW=1
  fi
done

# Allow running directly from kali_desktop_vm source folder
if [[ ! -f "./vm_cli.cpp" && -f "$PARENT_DIR/vm_cli.cpp" ]]; then
  cp "$PARENT_DIR/vm_cli.cpp" ./vm_cli.cpp
fi
if [[ ! -f "./kali_ui.html" && -f "$PARENT_DIR/kali_ui.html" ]]; then
  cp "$PARENT_DIR/kali_ui.html" ./kali_ui.html
fi
if [[ ! -f "./viewer.html" && -f "$PARENT_DIR/viewer.html" ]]; then
  cp "$PARENT_DIR/viewer.html" ./viewer.html
fi
if [[ ! -f "./custom_vm_bashrc" && -f "$PARENT_DIR/kali_desktop_vm/custom_vm_bashrc" ]]; then
  cp "$PARENT_DIR/kali_desktop_vm/custom_vm_bashrc" ./custom_vm_bashrc
fi

if [[ -f "./custom_vm_bashrc" ]]; then
  sed -i 's/\r$//' ./custom_vm_bashrc
fi

if [[ ! -x "./vm_cli" ]]; then
  if [[ ! -f "./vm_cli.cpp" ]]; then
    echo "vm_cli.cpp not found. Run ./install_in_kali.sh first, or run this script from ~/custom_vm_app."
    exit 1
  fi
  g++ vm_cli.cpp -std=gnu++17 -O2 -o vm_cli
fi

setup_runtime_dir() {
  local current_runtime="${XDG_RUNTIME_DIR:-}"
  if [[ -z "$current_runtime" || ! -d "$current_runtime" ]]; then
    local fallback_runtime="/tmp/custom_vm_runtime_${UID}"
    mkdir -p "$fallback_runtime"
    chmod 700 "$fallback_runtime"
    export XDG_RUNTIME_DIR="$fallback_runtime"
    return
  fi

  local runtime_mode
  runtime_mode="$(stat -c %a "$current_runtime" 2>/dev/null || echo "")"
  if [[ "$runtime_mode" == "777" || "$runtime_mode" == "0777" ]]; then
    local secure_runtime="/tmp/custom_vm_runtime_${UID}"
    mkdir -p "$secure_runtime"
    chmod 700 "$secure_runtime"
    export XDG_RUNTIME_DIR="$secure_runtime"
  fi
}

setup_runtime_dir
export APP_DIR
export XCURSOR_THEME="${XCURSOR_THEME:-Adwaita}"
export XCURSOR_SIZE="${XCURSOR_SIZE:-24}"

TERMINAL_STYLE=(
  "--title=Custom VM Terminal"
  "--font=Monospace 12"
)

# Optional UI launch (disabled by default)
if [[ "$OPEN_UI" -eq 1 ]]; then
  if ! ./vm_cli kali-ui --open; then
    echo "Kali UI auto-launch failed. Trying xdg-open fallback..."
    xdg-open "file://$APP_DIR/kali_ui.html" >/dev/null 2>&1 || true
  fi
fi

launch_current_terminal() {
  if [[ "$USE_VM_SHELL" -eq 1 ]]; then
    bash -lc 'tput cnorm 2>/dev/null || true; stty sane 2>/dev/null || true; cd "$APP_DIR"; ./vm_cli shell'
  else
    bash -lc 'tput cnorm 2>/dev/null || true; stty sane 2>/dev/null || true; cd "$APP_DIR"; export CUSTOM_VM_HOME="$APP_DIR"; if [[ -f "$APP_DIR/custom_vm_bashrc" ]]; then exec bash --rcfile "$APP_DIR/custom_vm_bashrc" -i; else exec bash -i; fi'
  fi
}

launch_separate_window() {
  local cmd_vm_shell="bash -lc 'tput cnorm 2>/dev/null || true; stty sane 2>/dev/null || true; cd \"$APP_DIR\"; ./vm_cli shell'"
  local cmd_bash_profile="bash -lc 'tput cnorm 2>/dev/null || true; stty sane 2>/dev/null || true; cd \"$APP_DIR\"; export CUSTOM_VM_HOME=\"$APP_DIR\"; if [[ -f \"$APP_DIR/custom_vm_bashrc\" ]]; then exec bash --rcfile \"$APP_DIR/custom_vm_bashrc\" -i; else exec bash -i; fi'"
  local launch_cmd="$cmd_bash_profile"
  if [[ "$USE_VM_SHELL" -eq 1 ]]; then
    launch_cmd="$cmd_vm_shell"
  fi

  if command -v xfce4-terminal >/dev/null 2>&1; then
    xfce4-terminal "${TERMINAL_STYLE[@]}" --hold -e "$launch_cmd" >/dev/null 2>&1 &
    disown || true
    sleep 0.2
    return 0
  fi

  if command -v gnome-terminal >/dev/null 2>&1; then
    gnome-terminal -- bash -lc "${launch_cmd#bash -lc }" >/dev/null 2>&1 &
    disown || true
    sleep 0.2
    return 0
  fi

  if command -v xterm >/dev/null 2>&1; then
    xterm -fa Monospace -fs 12 -e "$launch_cmd" >/dev/null 2>&1 &
    disown || true
    sleep 0.2
    return 0
  fi

  return 1
}

if [[ "$FORCE_WINDOW" -eq 0 && ( "${TERM_PROGRAM:-}" == "vscode" || -z "${SESSION_MANAGER:-}" ) ]]; then
  launch_current_terminal
  exit 0
fi

# Keep terminal for manual commands too
if ! launch_separate_window; then
  echo "Unable to open separate terminal window. Running in current terminal..."
  launch_current_terminal
fi
