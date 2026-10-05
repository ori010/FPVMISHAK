@echo off
rem (Re)generates the editor content: input assets, materials, Blueprints and the L_Prototype map.
rem Only MISSING assets are created, so your edits are kept.
rem   SetupContent.bat -RebuildMap   regenerates only the map
rem   SetupContent.bat -Force        resets ALL generated assets to their defaults (overwrites your tuning!)
call "%~dp0_Config.bat" || goto :fail

echo === Generating content ===
"%UE_EDITOR_CMD%" "%UPROJECT%" -run=FPVSetupContent %* -unattended -nopause -nosplash -NullRHI || goto :fail

echo.
echo === Content setup succeeded ===
exit /b 0

:fail
echo.
echo === Content setup FAILED (see messages above) ===
exit /b 1
