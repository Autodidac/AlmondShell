@echo off
setlocal

REM Resolve script directory safely
set "SCRIPT_DIR=%~dp0"

REM Rebuild the argument list safely, preserving quotes
set "PSARGS="
:argloop
if "%~1"=="" goto afterargs
    set "PSARGS=%PSARGS% "%~1""
    shift
    goto argloop
:afterargs

REM Call PowerShell with safe argument forwarding
powershell -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT_DIR%almondshell_install_build_tools.ps1" %PSARGS%

endlocal
