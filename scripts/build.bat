@echo off
setlocal

REM Script for building the YASS on Windows,
REM
REM Usage: build.bat

REM Prerequisites:
REM
REM   Visual Studio 2019, CMake, Ninja,
REM   Visual Studio 2019 SDK and Python 3.
REM
REM VCToolsVersion:PlatformToolchainversion:VisualStudioVersion
REM   14.30-14.3?:v143:Visual Studio 2019
REM   14.20-14.29:v142:Visual Studio 2019
REM   14.10-14.19:v141:Visual Studio 2017
REM   14.00-14.00:v140:Visual Studio 2015

REM You need to modify the paths below:

REM Use Visual Studio 2019's toolchain for (x86, x64, arm64)

set VisualStudioInstallerFolder="%ProgramFiles(x86)%\Microsoft Visual Studio\Installer"
if %PROCESSOR_ARCHITECTURE%==x86 set VisualStudioInstallerFolder="%ProgramFiles%\Microsoft Visual Studio\Installer"

pushd %VisualStudioInstallerFolder%
for /f "usebackq tokens=*" %%i in (`vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
  set VisualStudioInstallDir=%%i
)
popd

set VCToolsVersion=
set Winsdk=10.0.19041.0
set "WindowsSDKVersion=%Winsdk%\"
set "vsdevcmd=%VisualStudioInstallDir%\Common7\Tools\VsDevCmd.bat"

REM
REM Generate build helper
REM
cd tools
go build
cd ..

REM
REM Generate static x86 binary
REM
set "VSCMD_START_DIR=%CD%"
set "CC=%CD%\third_party\llvm-build\Release+Asserts\bin\clang-cl.exe"
set "CXX=%CD%\third_party\llvm-build\Release+Asserts\bin\clang-cl.exe"
set Platform=x86
set MSVC_CRT_LINKAGE=static

call "%vsdevcmd%" -arch=%Platform% -host_arch=amd64 -winsdk=%Winsdk% -no_logo -vcvars_ver=%VCToolsVersion%

tools\build -alsologtostderr -v 2
if %ERRORLEVEL% NEQ 0 exit /B %ERRORLEVEL%

REM
REM Generate static x64 binary
REM
set "VSCMD_START_DIR=%CD%"
set "CC=%CD%\third_party\llvm-build\Release+Asserts\bin\clang-cl.exe"
set "CXX=%CD%\third_party\llvm-build\Release+Asserts\bin\clang-cl.exe"
set Platform=x64
set MSVC_CRT_LINKAGE=static

call "%vsdevcmd%" -arch=%Platform% -host_arch=amd64 -winsdk=%Winsdk% -no_logo -vcvars_ver=%VCToolsVersion%

tools\build -alsologtostderr -v 2
if %ERRORLEVEL% NEQ 0 exit /B %ERRORLEVEL%

REM skip ARM build
goto :BuildARM64

REM
REM Generate static arm binary
REM
set VCToolsVersion=
set "VSCMD_START_DIR=%CD%"
set "CC=%CD%\third_party\llvm-build\Release+Asserts\bin\clang-cl.exe"
set "CXX=%CD%\third_party\llvm-build\Release+Asserts\bin\clang-cl.exe"
set Platform=arm
set MSVC_CRT_LINKAGE=static

call "%vsdevcmd%" -arch=%Platform% -host_arch=amd64 -winsdk=%Winsdk% -no_logo -vcvars_ver=%VCToolsVersion%

tools\build -alsologtostderr -v 2
if %ERRORLEVEL% NEQ 0 exit /B %ERRORLEVEL%

:BuildARM64

REM
REM Generate static Arm64 binary
REM
REM Use Visual Studio 2019's toolchain for ARM64 target

set VCToolsVersion=

set "VSCMD_START_DIR=%CD%"
set "CC=%CD%\third_party\llvm-build\Release+Asserts\bin\clang-cl.exe"
set "CXX=%CD%\third_party\llvm-build\Release+Asserts\bin\clang-cl.exe"
set Platform=arm64
set MSVC_CRT_LINKAGE=static

call "%vsdevcmd%" -arch=%Platform% -host_arch=amd64 -winsdk=%Winsdk% -no_logo -vcvars_ver=%VCToolsVersion%

tools\build -alsologtostderr -v 2
if %ERRORLEVEL% NEQ 0 exit /B %ERRORLEVEL%

goto :eof
