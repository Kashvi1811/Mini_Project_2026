      modified:   trace.jsonl
        modified:   vm_cli.cpp
        deleted:    wsl/Makefile
        deleted:    wsl/README_WSL.md
        deleted:    wsl/open_ui_wsl.sh
        deleted:    wsl/run_demo_wsl.sh
        deleted:    wsl/wsl_shell.sh

Untracked files:
  (use "git add <file>..." to include in what will be committed)
        run_windows_vm.bat
        run_windows_vm.ps1

no changes added to commit (use "git add" and/or "git commit -a")
@echo off
setlocal
set "SCRIPT_DIR=%~dp0"
if /I "%~1"=="--here" (
	shift
	powershell -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT_DIR%run_windows_vm.ps1" %*
) else (
	powershell -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT_DIR%run_windows_vm.ps1" -NewWindow %*
)
set "RC=%ERRORLEVEL%"
endlocal & exit /b %RC%
