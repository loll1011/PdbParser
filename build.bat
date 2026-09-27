@echo off
setlocal

set "VSROOT=E:\Program Files\Microsoft Visual Studio"
set "KITS=E:\Windows Kits\10"
set "SDKVER=10.0.28000.0"

if not exist "%VSROOT%\VC\Auxiliary\Build\vcvars64.bat" (
    echo [ERROR] Visual Studio toolchain not found: %VSROOT%
    exit /b 1
)
if not exist "%KITS%\Lib\%SDKVER%\um\x64\DbgHelp.Lib" (
    echo [ERROR] Windows SDK DbgHelp not found: %KITS%
    exit /b 1
)
if not exist "%KITS%\Debuggers\x64\msdia140.dll" (
    echo [ERROR] WDK Debuggers x64\msdia140.dll not found.
    exit /b 1
)

call "%VSROOT%\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1

set "INCLUDE=%KITS%\Include\%SDKVER%\ucrt;%KITS%\Include\%SDKVER%\um;%KITS%\Include\%SDKVER%\shared;%INCLUDE%"
set "LIB=%KITS%\Lib\%SDKVER%\ucrt\x64;%KITS%\Lib\%SDKVER%\um\x64;%LIB%"

cl /nologo /std:c++17 /utf-8 /EHsc /O2 /W4 PdbParser.cpp /Fe:PdbParser.exe
if errorlevel 1 (
    echo [ERROR] Build failed.
    exit /b 1
)

copy /Y "%KITS%\Debuggers\x64\dbghelp.dll" "%CD%\dbghelp.dll" >nul
copy /Y "%KITS%\Debuggers\x64\symsrv.dll" "%CD%\symsrv.dll" >nul
copy /Y "%KITS%\Debuggers\x64\msdia140.dll" "%CD%\msdia140.dll" >nul

echo [OK] PdbParser.exe generated.
exit /b 0
