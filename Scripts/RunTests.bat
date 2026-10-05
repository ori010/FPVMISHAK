@echo off
rem Runs all FPVGame automation tests headless (no window) and prints the result.
rem The full log is written to Saved\Logs\AutomationRun.log.
call "%~dp0_Config.bat" || goto :fail

set "LOG=%PROJECT_DIR%\Saved\Logs\AutomationRun.log"
echo === Running automation tests (this takes about 1-2 minutes) ===
"%UE_EDITOR_CMD%" "%UPROJECT%" -ExecCmds="Automation RunTests FPVGame;Quit" -testexit="Automation Test Queue Empty" ^
    -unattended -nopause -nosplash -NullRHI -NoSound -log -abslog="%LOG%" ^
    -ReportExportPath="%PROJECT_DIR%\Saved\Automation\Report" ^
    "-ini:EditorPerProjectUserSettings:[/Script/UnrealEd.EditorPerformanceSettings]:bThrottleCPUWhenNotForeground=False" >nul
set "RESULT=%ERRORLEVEL%"

echo.
findstr /C:"Test Completed." "%LOG%"
echo.
if not "%RESULT%"=="0" goto :fail
echo === All tests passed ===
exit /b 0

:fail
echo === Tests FAILED - search "Error" in %LOG% ===
exit /b 1
