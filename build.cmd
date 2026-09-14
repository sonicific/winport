@echo off
setlocal

pushd "%~dp0"

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo Visual Studio Build Tools were not found.
    popd
    exit /b 1
)

set "VSINSTALL="
for /f "usebackq delims=" %%I in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSINSTALL=%%I"
if not defined VSINSTALL (
    echo The MSVC C++ toolchain was not found.
    popd
    exit /b 1
)

call "%VSINSTALL%\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 (
    popd
    exit /b 1
)

if not exist "build\app" mkdir "build\app"
if not exist "build\tests" mkdir "build\tests"
if not exist "build\smoke" mkdir "build\smoke"
if not exist "build\bluetooth-smoke" mkdir "build\bluetooth-smoke"
if not exist "dist" mkdir "dist"

rc /nologo /fo "build\app\app.res" "resources\app.rc"
if errorlevel 1 goto :fail

cl /nologo /std:c++20 /W4 /permissive- /EHsc /O2 /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN /DNOMINMAX /D_WIN32_WINNT=0x0601 /Isrc /Iresources /Fo:build\app\ src\app_window.cpp src\bluetooth_dialog.cpp src\bluetooth_manager.cpp src\com_port_manager.cpp src\device_manager.cpp src\device_rules.cpp src\device_safety.cpp src\main.cpp src\process_runner.cpp src\settings.cpp src\system_restore.cpp src\win32_helpers.cpp "build\app\app.res" /Fe:"dist\Sonic79Reconstructed.exe" /link /SUBSYSTEM:WINDOWS /MANIFEST:EMBED /MANIFESTINPUT:resources\app.manifest Advapi32.lib Bthprops.lib Comctl32.lib Cfgmgr32.lib Dwmapi.lib Ole32.lib Setupapi.lib Shell32.lib User32.lib Uxtheme.lib
if errorlevel 1 goto :fail

cl /nologo /std:c++20 /W4 /permissive- /EHsc /O2 /DUNICODE /D_UNICODE /DNOMINMAX /Isrc /Fo:build\tests\ tests\core_tests.cpp src\device_rules.cpp src\device_safety.cpp src\process_runner.cpp src\win32_helpers.cpp /Fe:"build\tests\core_tests.exe" /link Advapi32.lib Shell32.lib User32.lib
if errorlevel 1 goto :fail
"build\tests\core_tests.exe"
if errorlevel 1 goto :fail

cl /nologo /std:c++20 /W4 /permissive- /EHsc /O2 /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN /DNOMINMAX /D_WIN32_WINNT=0x0601 /Isrc /Fo:build\smoke\ tests\enumeration_smoke.cpp src\com_port_manager.cpp src\device_manager.cpp src\device_rules.cpp src\device_safety.cpp src\process_runner.cpp src\win32_helpers.cpp /Fe:"build\smoke\enumeration_smoke.exe" /link Advapi32.lib Cfgmgr32.lib Setupapi.lib Shell32.lib User32.lib
if errorlevel 1 goto :fail
"build\smoke\enumeration_smoke.exe"
if errorlevel 1 goto :fail

cl /nologo /std:c++20 /W4 /permissive- /EHsc /O2 /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN /DNOMINMAX /Isrc /Fo:build\bluetooth-smoke\ tests\bluetooth_smoke.cpp src\bluetooth_manager.cpp /Fe:"build\bluetooth-smoke\bluetooth_smoke.exe" /link Bthprops.lib
if errorlevel 1 goto :fail
"build\bluetooth-smoke\bluetooth_smoke.exe"
if errorlevel 1 goto :fail

start "" /wait "dist\Sonic79Reconstructed.exe" --smoke-test
if errorlevel 1 goto :fail

echo.
echo Build and tests succeeded: dist\Sonic79Reconstructed.exe
popd
exit /b 0

:fail
echo.
echo Build or tests failed.
popd
exit /b 1
