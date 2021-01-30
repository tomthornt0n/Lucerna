@echo off

REM for 4coder - 4coder must be started from a VS developer command prompt, so cl is in the path

call .\windows_build.bat

cd bin
.\platform_windows.exe
cd ..