@echo off
setlocal

set "VSWHERE=C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe"
if exist "%VSWHERE%" (
    for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -property installationPath`) do set "VSDIR=%%i"
) else (
    set "VSDIR=C:\Program Files\Microsoft Visual Studio\2022\Community"
)

call "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" >nul

set "CMAKE_BIN=%VSDIR%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin"
set "NINJA_BIN=%VSDIR%\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja"
set "PATH=%CMAKE_BIN%;%NINJA_BIN%;%PATH%"

cd /d "%~dp0.."

cmake --preset debug
if errorlevel 1 exit /b 1
cmake --build --preset debug
exit /b %errorlevel%
