@echo off
setlocal enabledelayedexpansion

:: UE4SSL.JavaScript Build Script
:: ================================

cd /d "%~dp0..\.."
echo [Build] Working directory: %cd%

:: Default configuration
set MODE=Game__Shipping__Win64
set REBUILD=0
set VERBOSE=0

:: Parse arguments
:parse_args
if "%~1"=="" goto :start_build
if /i "%~1"=="--debug" (
    set MODE=Game__Debug__Win64
    shift
    goto :parse_args
)
if /i "%~1"=="--release" (
    set MODE=Game__Shipping__Win64
    shift
    goto :parse_args
)
if /i "%~1"=="--rebuild" (
    set REBUILD=1
    shift
    goto :parse_args
)
if /i "%~1"=="--verbose" (
    set VERBOSE=1
    shift
    goto :parse_args
)
if /i "%~1"=="--help" (
    goto :show_help
)
shift
goto :parse_args

:show_help
echo.
echo Usage: build.bat [options]
echo.
echo Options:
echo   --debug      Build in debug mode (Game__Debug__Win64)
echo   --release    Build in release mode (Game__Shipping__Win64) [default]
echo   --rebuild    Clean and rebuild
echo   --verbose    Show verbose output
echo   --help       Show this help message
echo.
exit /b 0

:start_build
echo.
echo [Build] Configuration: %MODE%
echo.

:: Check if xmake is available
where xmake >nul 2>nul
if errorlevel 1 (
    echo [Error] xmake not found in PATH!
    echo Please install xmake from: https://xmake.io/
    exit /b 1
)

:: Configure xmake
echo [Build] Configuring xmake...
xmake f -m %MODE% -y
if errorlevel 1 (
    echo [Error] xmake configure failed!
    exit /b 1
)

:: Clean if rebuild requested
if %REBUILD%==1 (
    echo [Build] Cleaning previous build...
    xmake c -y
)

:: Build UE4SSL.JavaScript target
echo [Build] Building UE4SSL.JavaScript...
if %VERBOSE%==1 (
    xmake build -v UE4SSL.JavaScript
) else (
    xmake build UE4SSL.JavaScript
)

if errorlevel 1 (
    echo.
    echo [Error] Build failed!
    exit /b 1
)

echo.
echo [Build] Build completed successfully!
echo [Build] Output: Binaries\%MODE%\UE4SSL.JavaScript.dll
echo.

exit /b 0
