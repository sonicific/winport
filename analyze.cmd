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

if not exist "build\analyze" mkdir "build\analyze"

cl /nologo /std:c++20 /W4 /WX /wd28285 /wd6553 /permissive- /EHsc /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN /DNOMINMAX /D_WIN32_WINNT=0x0601 /analyze /c /Isrc /Iresources /Fo:build\analyze\ src\app_window.cpp src\bluetooth_dialog.cpp src\bluetooth_manager.cpp src\com_port_manager.cpp src\device_manager.cpp src\device_rules.cpp src\device_safety.cpp src\main.cpp src\process_runner.cpp src\settings.cpp src\system_restore.cpp src\win32_helpers.cpp
if errorlevel 1 (
    echo.
    echo Static analysis failed.
    popd
    exit /b 1
)

echo.
echo MSVC static analysis passed with warnings treated as errors.
popd
exit /b 0
