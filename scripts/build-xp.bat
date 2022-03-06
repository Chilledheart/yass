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

set VCToolsVersion=14.16.27012
set "VCINSTALLDIR=C:\Program Files (x86)\Microsoft Visual Studio\2019\Professional\VC"
set "WindowsSDKVersion=10.0.10240.0\"
set "WindowsSdkDir=C:\Program Files (x86)\Windows Kits\10"

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
set MSVC_ALLOW_XP=1

call "%~dp0callxp-%Platform%.cmd"

tools\build -alsologtostderr -v 2 || exit /b

goto :eof
