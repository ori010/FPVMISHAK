@echo off
rem Opens the project in the Unreal Editor (builds the C++ code first if needed).
call "%~dp0_Config.bat" || exit /b 1
if not exist "%PROJECT_DIR%\Binaries\Win64\UnrealEditor-FPVGame.dll" call "%~dp0Build.bat" || exit /b 1
start "" "%UE_EDITOR%" "%UPROJECT%"
