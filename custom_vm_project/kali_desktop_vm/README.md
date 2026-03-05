# Kali Desktop VM Bundle (Separate Folder)

This folder is for building a **full Kali-like VM environment** around your project (OS-level experience), separate from your app code.

## Goal
Create a Kali Linux VM (VirtualBox/VMware), install your app inside it, auto-launch on login, and export as OVA.

## Folder Contents
- `install_in_kali.sh` → installs dependencies and deploys app in Kali VM
- `run_custom_vm.sh` → launcher script used by menu/autostart
- `custom-vm.desktop` → app menu entry
- `custom-vm-autostart.desktop` → autostart entry for login
- `EXPORT_CHECKLIST.md` → final OVA/export checklist

## How to Use (inside Kali VM)
1. Copy your project folder into Kali VM.
2. In terminal:
   - `cd custom_vm_project/kali_desktop_vm`
   - `chmod +x install_in_kali.sh run_custom_vm.sh`
   - `./install_in_kali.sh`
3. Re-login or reboot Kali VM.
4. Your app appears in app menu as **Custom VM Interface** and can auto-launch.

## Daily Run (no password)
- One-time setup: `bash install_in_kali.sh`
- Daily run: `bash run_no_sudo.sh`
- Daily run with UI: `bash run_no_sudo.sh --ui`
- Force separate terminal window: add `--window`
- Menu/autostart also use no-password path via `run_no_sudo.sh`.
- After installer, aliases are available: `custrun` and `custui` (run `source ~/.bashrc` once or open new terminal).

### Launcher behavior
- `./run_custom_vm.sh` → opens only normal terminal (best cursor + arrow-key behavior).
- `./run_custom_vm.sh --vm-shell` → opens custom `vm_cli shell` mode.
- `./run_custom_vm.sh --ui` → opens Kali UI + terminal.
- Default terminal uses a dedicated profile (`custom_vm_bashrc`) with visible cursor, clean prompt, and useful aliases (`vmm`, `vms`, `vmv`, `vmh`).
- XFCE launch now applies branded terminal style (title, dark background, green text, readable cursor).
- Prompt now uses a cleaner Kali-like two-line layout with startup quick commands.

## Notes
- This setup gives a true desktop VM environment feel.
- Your core engine remains in `../vm_cli.cpp`, `../viewer.html`, `../kali_ui.html`.
- If you see `Failed to initialize Xfconf` / `dbus-launch (No such file or directory)`, install missing package: `sudo apt install -y dbus-x11`.
- If browser launch fails, run manually: `xdg-open ~/custom_vm_app/kali_ui.html`.
- If cursor ever disappears, run: `tput cnorm; stty sane; reset`.
- Re-run `install_in_kali.sh` after updates so `~/custom_vm_app` gets the latest launcher/profile.
