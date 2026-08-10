@echo off
setlocal EnableDelayedExpansion

rem ============================================================
rem  deeptrace_cli 打包脚本
rem
rem  流程:
rem    1. 构建 deeptrace 静态库 Release(CLI 的链接依赖)
rem    2. 构建 deeptrace_cli Release
rem    3. 收集 deeptrace_cli.exe 并打成 zip 归档
rem
rem  用法:
rem    package.bat [版本]          默认版本 v1.3
rem
rem  输出:
rem    cli/out/dist/deeptrace_cli-<版本>-win64.zip
rem
rem  注意:.bat 必须保持 CRLF 行尾(.gitattributes 已强制);
rem  若在 WSL/Linux 上编辑后出现命令解析错乱,先转回 CRLF。
rem ============================================================

set "VERSION=%~1"
if "%VERSION%"=="" set "VERSION=v1.3"

rem 校验版本号:仅允许字母/数字/._- ,防止路径或命令注入
echo(!VERSION!| findstr /r /c:"^[A-Za-z0-9._-]*$" >nul
if errorlevel 1 (
    echo ERROR: invalid version "%VERSION%" - allowed chars: A-Z a-z 0-9 . _ -
    exit /b 1
)

rem ---- 1. deeptrace 静态库 Release(CLI find_library 的链接依赖)----
call "%~dp0..\..\deeptrace\script\build_release.bat"
if errorlevel 1 (
    echo ERROR: deeptrace Release build failed.
    exit /b 1
)

rem ---- 2. CLI Release ----
call "%~dp0build_release.bat"
if errorlevel 1 (
    echo ERROR: deeptrace_cli Release build failed.
    exit /b 1
)

cd /d "%~dp0.."

set "BIN_DIR=%CD%\out\bin\Release"
set "DIST_DIR=%CD%\out\dist"
set "STAGE=%DIST_DIR%\stage"
set "ARCHIVE=%DIST_DIR%\deeptrace_cli-%VERSION%-win64.zip"

if not exist "%BIN_DIR%\deeptrace_cli.exe" (
    echo ERROR: %BIN_DIR%\deeptrace_cli.exe not found.
    exit /b 1
)

rem ---- 3. 收集产物 ----
if exist "%STAGE%" rmdir /s /q "%STAGE%"
mkdir "%STAGE%" >nul 2>&1
copy /y "%BIN_DIR%\deeptrace_cli.exe" "%STAGE%\" >nul
if errorlevel 1 (
    echo ERROR: copy deeptrace_cli.exe failed.
    rmdir /s /q "%STAGE%"
    exit /b 1
)

rem 随包附带 LICENSE(MIT,开源分发惯例)
copy /y "..\LICENSE" "%STAGE%\" >nul 2>&1
if errorlevel 1 (
    echo WARNING: LICENSE copy failed, continuing.
)

rem ---- 4. 压缩为 zip ----
if exist "%ARCHIVE%" del "%ARCHIVE%"
powershell -NoProfile -ExecutionPolicy Bypass -Command "Compress-Archive -Path '%STAGE%\*' -DestinationPath '%ARCHIVE%'"
if errorlevel 1 (
    echo ERROR: zip packaging failed.
    rmdir /s /q "%STAGE%"
    exit /b 1
)
if not exist "%ARCHIVE%" (
    echo ERROR: %ARCHIVE% was not created.
    rmdir /s /q "%STAGE%"
    exit /b 1
)
rmdir /s /q "%STAGE%"

rem ---- 5. 输出结果 ----
echo.
echo [OK] package: %ARCHIVE%
for %%F in ("%ARCHIVE%") do echo [OK] size: %%~zF bytes
exit /b 0
