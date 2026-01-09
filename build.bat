@echo off
setlocal EnableExtensions EnableDelayedExpansion

rem Simple helper to configure/build/run SandboxGE (with the demo).
rem Actions: clean | build (default) | run | dry-run (echo only)

set "ACTION=%~1"
if "%ACTION%"=="" set "ACTION=build"

set "DRY=0"
if /I "%ACTION%"=="dry-run" (
    set "DRY=1"
    set "ACTION=run"
)

set "BUILD_DIR=%CMAKE_BUILD_DIR%"
if "!BUILD_DIR!"=="" set "BUILD_DIR=build"

set "CONFIG=%CONFIG%"
if "!CONFIG!"=="" set "CONFIG=Release"

set "DEMO_FLAG=%SANDBOX_GE_BUILD_DEMO%"
if "!DEMO_FLAG!"=="" set "DEMO_FLAG=ON"

set "EXTRA_INC="
if not "%SANDBOX_GE_EXTRA_INCLUDE_DIRS%"=="" set "EXTRA_INC=-DSANDBOX_GE_EXTRA_INCLUDE_DIRS=%SANDBOX_GE_EXTRA_INCLUDE_DIRS%"

set "EXTRA_CMAKE_ARGS=%SANDBOX_GE_CMAKE_ARGS%"

set "TARGETS=SandboxGE"
if /I "!DEMO_FLAG!"=="ON" set "TARGETS=SandboxGE SandboxGE_Demo"

if /I "!ACTION!"=="clean" (
    if "!DRY!"=="1" (
        echo [dry-run] rmdir /S /Q "!BUILD_DIR!"
    ) else (
        if exist "!BUILD_DIR!" (
            echo Cleaning "!BUILD_DIR!" ...
            rmdir /S /Q "!BUILD_DIR!"
        ) else (
            echo Nothing to clean in "!BUILD_DIR!".
        )
    )
    goto :eof
)

if /I "!ACTION!"=="run" if /I "!DEMO_FLAG!" NEQ "ON" (
    echo Forcing SANDBOX_GE_BUILD_DEMO=ON to enable the demo executable.
    set "DEMO_FLAG=ON"
    set "TARGETS=SandboxGE SandboxGE_Demo"
)

set "CONFIGURE_CMD=cmake -S . -B "!BUILD_DIR!" -DSANDBOX_GE_BUILD_DEMO=!DEMO_FLAG! !EXTRA_INC! !EXTRA_CMAKE_ARGS!"
set "BUILD_CMD=cmake --build "!BUILD_DIR!" --config "!CONFIG!" --target !TARGETS!"

call :run_cmd "!CONFIGURE_CMD!"
call :run_cmd "!BUILD_CMD!"
call :copy_shaders

if /I "!ACTION!"=="run" (
    call :find_demo_exe
    if "!DEMO_EXE!"=="" (
        echo Could not find SandboxGE_Demo.exe after build. Ensure SANDBOX_GE_BUILD_DEMO=ON and rebuild.
        goto :eof
    )
    call :run_cmd ""!DEMO_EXE!""
)

goto :eof

:run_cmd
set "CMD=%~1"
if "!DRY!"=="1" (
    echo [dry-run] !CMD!
) else (
    echo !CMD!
    call !CMD!
    if errorlevel 1 (
        echo Command failed with exit code !errorlevel!.
        exit /b !errorlevel!
    )
)
exit /b 0

:find_demo_exe
set "DEMO_EXE="
if exist "!BUILD_DIR!\SandboxGE_Demo.exe" set "DEMO_EXE=!BUILD_DIR!\SandboxGE_Demo.exe"
if exist "!BUILD_DIR!\!CONFIG!\SandboxGE_Demo.exe" set "DEMO_EXE=!BUILD_DIR!\!CONFIG!\SandboxGE_Demo.exe"
exit /b 0

:copy_shaders
set "SHADER_SRC=%~dp0shaders"
if not exist "!SHADER_SRC!" (
    echo Shader source folder not found: "!SHADER_SRC!"
    exit /b 0
)
set "DEST1=!BUILD_DIR!\shaders"
set "DEST2=!BUILD_DIR!\!CONFIG!\shaders"
if "!DRY!"=="1" (
    echo [dry-run] cmake -E copy_directory "!SHADER_SRC!" "!DEST1!"
    echo [dry-run] cmake -E copy_directory "!SHADER_SRC!" "!DEST2!"
) else (
    cmake -E copy_directory "!SHADER_SRC!" "!DEST1!" >NUL
    cmake -E copy_directory "!SHADER_SRC!" "!DEST2!" >NUL
)
exit /b 0
