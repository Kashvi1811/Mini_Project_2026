#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")"

target="${1:-kali}"

if [[ "$target" == "viewer" ]]; then
  page="../viewer.html"
else
  page="../kali_ui.html"
fi

open_with_default() {
  if command -v wslview >/dev/null 2>&1; then
    wslview "$page"
  elif command -v xdg-open >/dev/null 2>&1; then
    xdg-open "$page"
  else
    echo "No opener found. Install wslu (wslview) or xdg-utils." >&2
    exit 1
  fi
}

open_with_default
echo "Opened $page"
