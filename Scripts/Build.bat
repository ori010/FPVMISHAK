@echo off
rem Compiles the C++ code (editor build). Close the Unreal Editor first.
call "%~dp0_Config.bat" || goto :fail

echo === Building FPVGameEditor (Win64 Development) ===
call "%UE_BUILD%" FPVGameEditor Win64 Development -Project="%UPROJECT%" -WaitMutex -NoHotReloadFromIDE || goto :fail

echo.
echo === Build succeeded ===
exit /b 0

:fail
echo.
echo === Build FAILED (see messages above) ===
exit /b 1
