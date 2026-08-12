@echo off
setlocal
rem ============================================================
rem  Compile all examples under docs/api/v2.13.0/Examples (link Debug deeptrace.lib)
rem  Usage: build_examples.bat
rem  Prerequisite: deeptrace library built (deeptrace\script\build_debug.bat)
rem ============================================================

set "VSWHERE=C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe"
if exist "%VSWHERE%" (
    for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -property installationPath`) do set "VSDIR=%%i"
) else (
    set "VSDIR=C:\Program Files\Microsoft Visual Studio\2022\Community"
)
call "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" >nul

cd /d "%~dp0"
set "ROOT=%~dp0..\..\..\..\.."
set "INC=%ROOT%\deeptrace\include"
set "DTLIB=%ROOT%\deeptrace\out\lib\Debug\deeptrace.lib"
set "KSLIB=%ROOT%\deeptrace\out\build\debug\third_party\keystone\lib\keystone.lib"
set "CSLIB=%ROOT%\deeptrace\out\lib\Debug\capstone.lib"

for %%F in (getting_started.cpp session_lifecycle.cpp read_write_memory.cpp debug_breakpoints.cpp pointer_chain_scan.cpp) do (
    echo [compile] %%~nF ...
    cl /nologo /std:c++20 /EHsc /MDd /I "%INC%" "%%F" "%DTLIB%" "%KSLIB%" "%CSLIB%" ^
        /link /out:%%~nF.exe || exit /b 1
)
echo.
echo [OK] all examples compiled.
exit /b 0
