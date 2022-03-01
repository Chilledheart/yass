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

REM Use Visual Studio 2017's toolchain for (x86, x64)

set VCToolsVersion=14.16
set Winsdk=10.0.19041.0
set "WindowsSDKVersion=%Winsdk%\"

set vsdevcmd=C:\Program Files (x86)\Microsoft Visual Studio\2019\Professional\Common7\Tools\VsDevCmd.bat

REM
REM Generate static x86 binary
REM
set "VSCMD_START_DIR=%CD%"
set "CC=%CD%\third_party\llvm-build\Release+Asserts\bin\clang-cl.exe"
set "CXX=%CD%\third_party\llvm-build\Release+Asserts\bin\clang-cl.exe"
set Platform=x86
set MSVC_CRT_LINKAGE=static

call "%vsdevcmd%" -arch=%Platform% -host_arch=amd64 -winsdk=%Winsdk% -no_logo -vcvars_ver=%VCToolsVersion%

python.exe -u .\scripts\build.py || exit /b

REM
REM Generate static x64 binary
REM
set "VSCMD_START_DIR=%CD%"
set "CC=%CD%\third_party\llvm-build\Release+Asserts\bin\clang-cl.exe"
set "CXX=%CD%\third_party\llvm-build\Release+Asserts\bin\clang-cl.exe"
set Platform=x64
set MSVC_CRT_LINKAGE=static

call "%vsdevcmd%" -arch=%Platform% -host_arch=amd64 -winsdk=%Winsdk% -no_logo -vcvars_ver=%VCToolsVersion%

python.exe -u .\scripts\build.py || exit /b

REM skip ARM build
goto :BuildARM64

REM
REM Generate static arm binary
REM
set VCToolsVersion=14.29
set "VSCMD_START_DIR=%CD%"
set "CC=%CD%\third_party\llvm-build\Release+Asserts\bin\clang-cl.exe"
set "CXX=%CD%\third_party\llvm-build\Release+Asserts\bin\clang-cl.exe"
set Platform=arm
set MSVC_CRT_LINKAGE=static

call "%vsdevcmd%" -arch=%Platform% -host_arch=amd64 -winsdk=%Winsdk% -no_logo -vcvars_ver=%VCToolsVersion%

python.exe -u .\scripts\build.py || exit /b

:BuildARM64

REM
REM Generate static Arm64 binary
REM
REM Use Visual Studio 2019's toolchain for ARM64 target

set VCToolsVersion=14.29

set "VSCMD_START_DIR=%CD%"
set "CC=%CD%\third_party\llvm-build\Release+Asserts\bin\clang-cl.exe"
set "CXX=%CD%\third_party\llvm-build\Release+Asserts\bin\clang-cl.exe"
set Platform=arm64
set MSVC_CRT_LINKAGE=static

call "%vsdevcmd%" -arch=%Platform% -host_arch=amd64 -winsdk=%Winsdk% -no_logo -vcvars_ver=%VCToolsVersion%

python.exe -u .\scripts\build.py || exit /b

goto :eof
