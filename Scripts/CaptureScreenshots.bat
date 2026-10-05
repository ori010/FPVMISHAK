@echo off
rem Visual check: plays a scripted flight with real rendering and saves screenshots of the key moments
rem (FPV view, explosion, MISSION COMPLETE, restart) to Saved\Screenshots\FPVVisual.
rem An editor window opens for about a minute (longer the first time, while shaders compile).
call "%~dp0_Config.bat" || goto :fail

set "LOG=%PROJECT_DIR%\Saved\Logs\VisualCheck.log"
"%UE_EDITOR_CMD%" "%UPROJECT%" -ExecCmds="Automation RunTests FPVGame.Visual;Quit" -testexit="Automation Test Queue Empty" ^
    -unattended -nopause -nosplash -NoSound -log -abslog="%LOG%" -ResX=1600 -ResY=900 ^
    "-ini:EditorPerProjectUserSettings:[/Script/UnrealEd.EditorPerformanceSettings]:bThrottleCPUWhenNotForeground=False" >nul
if errorlevel 1 goto :fail

echo Screenshots saved to %PROJECT_DIR%\Saved\Screenshots\FPVVisual
start "" "%PROJECT_DIR%\Saved\Screenshots\FPVVisual"
exit /b 0

:fail
echo === Visual check FAILED - see %LOG% ===
exit /b 1
