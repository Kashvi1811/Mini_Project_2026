@echo off
setlocal
set "SCRIPT_DIR=%~dp0"
set "PARENT_BAT=%SCRIPT_DIR%..\run_windows_vm.bat"

if not exist "%PARENT_BAT%" (
  echo Parent launcher not found: "%PARENT_BAT%"
  exit /b 1
)

call "%PARENT_BAT%" %*
set "RC=%ERRORLEVEL%"
endlocal & exit /b %RC%
