@echo off
rem Shared settings for the helper scripts. Change UE_ROOT if Unreal Engine 5.8 is installed somewhere else.
if not defined UE_ROOT set "UE_ROOT=D:\Epic Games\UE_5.8"
set "PROJECT_DIR=%~dp0.."
for %%I in ("%PROJECT_DIR%") do set "PROJECT_DIR=%%~fI"
set "UPROJECT=%PROJECT_DIR%\FPVGame.uproject"
set "UE_BUILD=%UE_ROOT%\Engine\Build\BatchFiles\Build.bat"
set "UE_EDITOR=%UE_ROOT%\Engine\Binaries\Win64\UnrealEditor.exe"
set "UE_EDITOR_CMD=%UE_ROOT%\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"

if not exist "%UE_EDITOR%" (
    echo [ERROR] Unreal Engine not found at "%UE_ROOT%".
    echo         Edit Scripts\_Config.bat and set UE_ROOT to your UE 5.8 install folder.
    exit /b 1
)
exit /b 0
