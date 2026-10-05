@echo off
rem Launches the prototype directly as a game window (no editor UI). Build first (Build.bat).
call "%~dp0_Config.bat" || exit /b 1
start "" "%UE_EDITOR%" "%UPROJECT%" /Game/Maps/L_Prototype -game -windowed -ResX=1600 -ResY=900 -log
