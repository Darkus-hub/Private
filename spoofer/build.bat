@echo off
setlocal enabledelayedexpansion
set ROOT=%~dp0
if not exist "%ROOT%out" mkdir "%ROOT%out"
if not exist "%ROOT%obj" mkdir "%ROOT%obj"

set VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set VSPATH=%%i
if not defined VSPATH echo [x] no VC tools & exit /b 1
call "%VSPATH%\VC\Auxiliary\Build\vcvars64.bat" >nul

set WK=%ProgramFiles(x86)%\Windows Kits\10
set WVER=
for /f "delims=" %%v in ('dir /b /ad "%WK%\Lib" 2^>nul ^| findstr /b /r "10\."') do set WVER=%%v
if not defined WVER echo [x] no WDK & exit /b 1
set KM=%WK%\Lib\%WVER%\km\x64
set KI=%WK%\Include\%WVER%

pushd "%ROOT%obj"
cl /nologo /kernel /GS- /GR- /X /I"%KI%\km" /I"%KI%\shared" /I"%ROOT%driver" /c "%ROOT%driver\entry.c" "%ROOT%driver\hooks_storage.c" "%ROOT%driver\hooks_reg.c" || (popd & exit /b 1)
link /nologo /DRIVER /SUBSYSTEM:NATIVE /ENTRY:DriverEntry /OUT:"%ROOT%out\spoof.sys" *.obj "%KM%\BufferOverflowK.lib" "%KM%\wdm.lib" "%KM%\ntoskrnl.lib" "%KM%\hal.lib" || (popd & exit /b 1)
popd

cl /nologo /EHsc /O2 /DUNICODE /D_UNICODE "%ROOT%loader\main.cpp" /link /OUT:"%ROOT%out\loader.exe" advapi32.lib || exit /b 1
echo [ok]
