@echo off
cd /d %~dp0
echo Running UnifiedDemo with console output...
echo.
build\Release\UnifiedDemo.exe
pause
