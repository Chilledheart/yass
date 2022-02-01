@echo off
setlocal

REM Script for building the YASS on Windows,
REM
REM Usage: build.bat

REM Prerequisites:
REM
REM   Visual Studio 2022, CMake, Ninja,
REM   Visual Studio 2022 SDK and python.exe -u.
REM
REM VCToolsVersion:PlatformToolchainversion:VisualStudioVersion
REM   14.30-14.3?:v143:Visual Studio 2022
REM   14.20-14.29:v142:Visual Studio 2019
REM   14.10-14.19:v141:Visual Studio 2017
REM   14.00-14.00:v140:Visual Studio 2015

REM You need to modify the paths below:

REM Use Visual Studio 2015's toolchain for (x86, x64)

set VCToolsVersion=14.0
set Winsdk=10.0.19041.0
set "WindowsSDKVersion=%Winsdk%\"

set vsdevcmd=C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\Tools\VsDevCmd.bat

set "VSCMD_START_DIR=%CD%"
set CC=
set CXX=
set ASM=
set Platform=x86
set MSVC_CRT_LINKAGE=dynamic
set COMPILER_TARGET=i686-pc-windows-msvc

call "%vsdevcmd%" -arch=%Platform% -host_arch=amd64 -winsdk=%Winsdk%

call :BuildBoringSSL
python.exe -u .\scripts\build.py || exit /b

move "yass.zip" "yass-msvc-release-%Platform%-%MSVC_CRT_LINKAGE%.zip"
move "yass-debuginfo.zip" "yass-msvc-release-%Platform%-%MSVC_CRT_LINKAGE%-debuginfo.zip"

set "VSCMD_START_DIR=%CD%"
set CC=
set CXX=
set ASM=
set Platform=x64
set MSVC_CRT_LINKAGE=dynamic
set COMPILER_TARGET=x86_64-pc-windows-msvc

call "%vsdevcmd%" -arch=%Platform% -host_arch=amd64 -winsdk=%Winsdk%

call :BuildBoringSSL
python.exe -u .\scripts\build.py || exit /b

move "yass.zip" "yass-msvc-release-%Platform%-%MSVC_CRT_LINKAGE%.zip"
move "yass-debuginfo.zip" "yass-msvc-release-%Platform%-%MSVC_CRT_LINKAGE%-debuginfo.zip"

REM Use Visual Studio 2019's toolchain for ARM64 target

set VCToolsVersion=14.29

set "VSCMD_START_DIR=%CD%"
set CC=
set CXX=
set ASM=
set Platform=arm64
set MSVC_CRT_LINKAGE=dynamic
set COMPILER_TARGET=arm64-pc-windows-msvc

call "%vsdevcmd%" -arch=%Platform% -host_arch=amd64 -winsdk=%Winsdk%

call :BuildBoringSSL
python.exe -u .\scripts\build.py || exit /b

move "yass.zip" "yass-msvc-release-%Platform%-%MSVC_CRT_LINKAGE%.zip"
move "yass-debuginfo.zip" "yass-msvc-release-%Platform%-%MSVC_CRT_LINKAGE%-debuginfo.zip"

goto :eof

:BuildBoringSSL

REM When you pass -DCMAKE_C_COMPILER= with an absolute path you need to use forward slashes.  That is setting a value directly into CMakeCache.txt so no automatic slash conversion is done.
set "CC=%CD%\third_party\llvm-build\Release+Asserts\bin\clang-cl.exe"
set "CXX=%CD%\third_party\llvm-build\Release+Asserts\bin\clang-cl.exe"

call "%~dp0build-boringssl.bat"

move "yass.zip" "yass-msvc-release-%Platform%-%MSVC_CRT_LINKAGE%.zip"
move "yass-debuginfo.zip" "yass-msvc-release-%Platform%-%MSVC_CRT_LINKAGE%-debuginfo.zip"

set "CC="
set "CXX="
