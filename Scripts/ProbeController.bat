@echo off
rem Controller check: opens the game for 75 seconds and records what the controller sends.
rem While the window is open: click into it, move BOTH sticks fully in all directions,
rem press R2 fully, and press Triangle, Cross and Options. The summary is printed at the end.
call "%~dp0_Config.bat" || exit /b 1

set "LOG=%PROJECT_DIR%\Saved\Logs\ControllerProbe.log"
"%UE_EDITOR_CMD%" "%UPROJECT%" -ExecCmds="Automation RunTests FPVManual.ControllerProbe;Quit" -testexit="Automation Test Queue Empty" ^
    -unattended -nopause -nosplash -NoSound -log -abslog="%LOG%" -ResX=1280 -ResY=720 ^
    "-ini:EditorPerProjectUserSettings:[/Script/UnrealEd.EditorPerformanceSettings]:bThrottleCPUWhenNotForeground=False" >nul

echo.
findstr /C:"PlayStation controller" /C:"range:" /C:"Buttons seen" "%LOG%"
