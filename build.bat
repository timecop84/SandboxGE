@echo off
setlocal EnableExtensions EnableDelayedExpansion

rem Build script for SandboxGE
rem Actions: clean | build (default) | rebuild | run | all

set "ACTION=%~1"
if "%ACTION%"=="" set "ACTION=build"

set "BUILD_DIR=build"
set "CONFIG=Release"
set "DEMO_FLAG=ON"

set "EXTRA_INC="
if exist "..\cloth_solver\modules\math\include" (
    set "EXTRA_INC=-DSANDBOX_GE_EXTRA_INCLUDE_DIRS=..\cloth_solver\modules\math\include"
)

set "TARGETS=SandboxGE UnifiedDemo"

if /I "!ACTION!"=="clean" (
    if exist "!BUILD_DIR!" (
        echo [CLEAN] Removing build directory...
        rmdir /S /Q "!BUILD_DIR!"
        echo [OK] Build directory removed
    ) else (
        echo [INFO] Nothing to clean
    )
    goto :eof
)

if /I "!ACTION!"=="rebuild" (
    if exist "!BUILD_DIR!" (
        echo [CLEAN] Removing build directory...
        rmdir /S /Q "!BUILD_DIR!"
    )
    set "ACTION=build"
)

if /I "!ACTION!"=="all" (
    if exist "!BUILD_DIR!" (
        echo [CLEAN] Removing build directory...
        rmdir /S /Q "!BUILD_DIR!"
    )
    set "ACTION=run"
)

rem Kill any running instances
taskkill /F /IM "UnifiedDemo.exe" 2>nul >nul

echo [BUILD] Configuring CMake...
cmake -S . -B "!BUILD_DIR!" -DSANDBOX_GE_BUILD_DEMO=!DEMO_FLAG! !EXTRA_INC!
if errorlevel 1 (
    echo [ERROR] CMake configuration failed
    exit /b 1
)

echo.
echo [BUILD] Building targets...
cmake --build "!BUILD_DIR!" --config "!CONFIG!" --target !TARGETS!
if errorlevel 1 (
    echo [ERROR] Build failed
    exit /b 1
)

echo.
echo [OK] Build completed successfully

rem Copy shaders
set "SHADER_SRC=%~dp0shaders"
if exist "!SHADER_SRC!" (
    cmake -E copy_directory "!SHADER_SRC!" "!BUILD_DIR!\shaders" >NUL
    cmake -E copy_directory "!SHADER_SRC!" "!BUILD_DIR!\!CONFIG!\shaders" >NUL
)

if /I "!ACTION!"=="run" (
    set "DEMO_EXE=!BUILD_DIR!\!CONFIG!\UnifiedDemo.exe"
    if not exist "!DEMO_EXE!" (
        echo [ERROR] Executable not found: !DEMO_EXE!
        exit /b 1
    )
    echo.
    echo [RUN] Launching UnifiedDemo...
    timeout /t 1 /nobreak >nul
    start "" "!DEMO_EXE!"
    echo [OK] UnifiedDemo launched
)

goto :eof