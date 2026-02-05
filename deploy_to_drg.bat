@echo off
chcp 65001 >nul
setlocal

xmake

:: 编译产物路径（Shipping Win64）
set "ROOT=D:\Project\UE4SS-Lite"
set "BIN=%ROOT%\Binaries\Game__Shipping__Win64"
set "JS_DLL=%BIN%\UE4SSL.JavaScript\UE4SSL.JavaScript.dll"
set "MAIN_DLL=%BIN%\UE4SSL\UE4SSL.dll"
set "MAIN_JS=%ROOT%\Script\JavaScript\examples\main.js"

:: 目标：深岩银河游戏目录下的 ue4ss
set "DEST=D:\SteamLibrary\steamapps\common\Deep Rock Galactic\FSD\Binaries\Win64\ue4ss"
set "JS_MOD_DIR=%DEST%\mods\mymod\js"

echo 正在部署到 DRG 测试目录...
echo.

if not exist "%DEST%" (
    echo 创建目标目录: %DEST%
    mkdir "%DEST%"
)
if not exist "%JS_MOD_DIR%" (
    echo 创建 JS 模组目录: %JS_MOD_DIR%
    mkdir "%DEST%\mods" 2>nul
    mkdir "%DEST%\mods\mymod" 2>nul
    mkdir "%JS_MOD_DIR%"
)

set "ERR=0"

if not exist "%JS_DLL%" (
    echo [错误] 未找到: %JS_DLL%
    set "ERR=1"
) else (
    copy /Y "%JS_DLL%" "%DEST%\"
    if errorlevel 1 (set "ERR=1") else (echo [OK] UE4SSL.JavaScript.dll)
)

if not exist "%MAIN_DLL%" (
    echo [错误] 未找到: %MAIN_DLL%
    set "ERR=1"
) else (
    copy /Y "%MAIN_DLL%" "%DEST%\"
    if errorlevel 1 (set "ERR=1") else (echo [OK] UE4SSL.dll)
)

if not exist "%MAIN_JS%" (
    echo [错误] 未找到: %MAIN_JS%
    set "ERR=1"
) else (
    copy /Y "%MAIN_JS%" "%JS_MOD_DIR%\main.js"
    if errorlevel 1 (set "ERR=1") else (echo [OK] main.js -^> mods\mymod\js\main.js)
)

echo.
if %ERR%==0 (
    echo 部署完成: %DEST%
) else (
    echo 部署未完全成功，请先编译项目或检查路径。
)
pause
