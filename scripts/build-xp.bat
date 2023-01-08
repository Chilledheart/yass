@echo off
setlocal

REM Script for building the YASS on Windows,
REM
REM Usage: build-xp.bat

REM Prerequisites:
REM
REM   Visual Studio 2022, CMake, Ninja,
REM   Visual Studio 2022 SDK and python.exe -u.
REM

set VisualStudioInstallerFolder="%ProgramFiles(x86)%\Microsoft Visual Studio\Installer"
if %PROCESSOR_ARCHITECTURE%==x86 set VisualStudioInstallerFolder="%ProgramFiles%\Microsoft Visual Studio\Installer"

pushd %VisualStudioInstallerFolder%
for /f "usebackq tokens=*" %%i in (`vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
  set VisualStudioInstallDir=%%i
)
popd

set VCToolsVersion=14.16.27012
set "WindowsSDKVersion=10.0.10240.0\"
set "WindowsSdkDir=C:\Program Files (x86)\Windows Kits\10"
set "VCINSTALLDIR=%VisualStudioInstallDir%\VC"

REM
REM Generate build helper
REM
cd tools
go build
cd ..

REM
REM Generate static x86 binary
REM
set "CC=%CD%\third_party\llvm-build\Release+Asserts\bin\clang-cl.exe"
set "CXX=%CD%\third_party\llvm-build\Release+Asserts\bin\clang-cl.exe"
set Platform=x86
set VSCMD_ARG_TGT_ARCH=x86
set MSVC_CRT_LINKAGE=static

call "%~dp0callxp-%Platform%.cmd"

tools\build -alsologtostderr -v 2 "-msvc-tgt-arch=%Platform%" "-msvc-crt-linkage=%MSVC_CRT_LINKAGE%" -msvc-allow-xp
if %ERRORLEVEL% NEQ 0 exit /B %ERRORLEVEL%

goto :eof
