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

set VCToolsVersion=14.0
set "VCINSTALLDIR=C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC"
set "WindowsSDKVersion=10.0.10240.0\"
set "WindowsSdkDir=C:\Program Files (x86)\Windows Kits\10"

set CC=
set CXX=
set Platform=x86
set VSCMD_ARG_TGT_ARCH=x86
set MSVC_CRT_LINKAGE=static
set COMPILER_TARGET=i686-pc-windows-msvc
set ALLOW_XP=on

call "%~dp0callxp-%Platform%.cmd"

call :BuildBoringSSL

python.exe -u .\scripts\build.py || exit /b

set CC=
set CXX=
set Platform=x64
set VSCMD_ARG_TGT_ARCH=x64
set MSVC_CRT_LINKAGE=static
set COMPILER_TARGET=x86_64-pc-windows-msvc
set ALLOW_XP=on

call "%~dp0callxp-%Platform%.cmd"

call :BuildBoringSSL

python.exe -u .\scripts\build.py || exit /b

set CC=
set CXX=
set Platform=x86
set VSCMD_ARG_TGT_ARCH=x86
set MSVC_CRT_LINKAGE=dynamic
set COMPILER_TARGET=i686-pc-windows-msvc
set ALLOW_XP=on

call "%~dp0callxp-%Platform%.cmd"

call :BuildBoringSSL

python.exe -u .\scripts\build.py || exit /b

set CC=
set CXX=
set Platform=x64
set VSCMD_ARG_TGT_ARCH=x64
set MSVC_CRT_LINKAGE=dynamic
set COMPILER_TARGET=x86_64-pc-windows-msvc
set ALLOW_XP=on

call "%~dp0callxp-%Platform%.cmd"

call :BuildBoringSSL

python.exe -u .\scripts\build.py || exit /b

goto :eof

:BuildBoringSSL

call "%~dp0build-boringssl.bat"
