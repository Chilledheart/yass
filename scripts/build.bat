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
REM   14.00-14.09:v140:Visual Studio 2015
REM Currently Visual Studio 2017 is not supported

REM You need to modify the paths below:

REM Use Visual Studio 2019's toolchain

set VCToolsVersion=14.29

set vsdevcmd=C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\Tools\VsDevCmd.bat

set "VSCMD_START_DIR=%CD%"
call "%vsdevcmd%" -arch=x86 -host_arch=amd64
set CC=
set CXX=
set Platform=x86
set MSVC_CRT_LINKAGE=dynamic
set CMAKE_EXTRA_OPTIONS=

call :BuildBoringSSL %Platform% %CMAKE_EXTRA_OPTIONS%

python.exe -u .\scripts\build.py || exit /b

set "VSCMD_START_DIR=%CD%"
call "%vsdevcmd%" -arch=amd64 -host_arch=amd64
set CC=
set CXX=
set Platform=x64
set MSVC_CRT_LINKAGE=dynamic
set CMAKE_EXTRA_OPTIONS=

call :BuildBoringSSL %Platform% %CMAKE_EXTRA_OPTIONS%

python.exe -u .\scripts\build.py || exit /b

set "VSCMD_START_DIR=%CD%"
call "%vsdevcmd%" -arch=arm64 -host_arch=amd64
set CC=
set CXX=
set MSVC_CRT_LINKAGE=dynamic
set Platform=arm64

set CMAKE_EXTRA_OPTIONS=-DOPENSSL_NO_ASM=on

call :BuildBoringSSL %Platform% %CMAKE_EXTRA_OPTIONS%

python.exe -u .\scripts\build.py || exit /b

exit /b 0

:BuildBoringSSL

cd third_party\boringssl
mkdir "%Platform%-%MSVC_CRT_LINKAGE%"
mkdir "%Platform%-%MSVC_CRT_LINKAGE%\debug"

rem check the existing debug lib
if not exist "%Platform%-%MSVC_CRT_LINKAGE%\debug\crypto.lib" (
  mkdir build
  cd build
  cmake -G "Ninja" -DCMAKE_BUILD_TYPE=Debug -DCMAKE_MSVC_CRT_LINKAGE=%MSVC_CRT_LINKAGE% %CMAKE_EXTRA_OPTIONS% .. || exit /b 1
  ninja crypto || exit /b 1
  copy /y crypto\crypto.lib "..\%Platform%-%MSVC_CRT_LINKAGE%\debug\crypto.lib"
  copy /y crypto\crypto.pdb "..\%Platform%-%MSVC_CRT_LINKAGE%\debug\crypto.pdb"
  copy /y crypto\fipsmodule\CMakeFiles\fipsmodule.dir\vc140.pdb "..\%Platform%-%MSVC_CRT_LINKAGE%\debug\vc140.pdb"
  cd ..

  rmdir build /s /q
)

rem check the existing release lib
if not exist "%Platform%-%MSVC_CRT_LINKAGE%\crypto.lib" (
  mkdir build
  cd build
  cmake -G "Ninja" -DCMAKE_BUILD_TYPE=Release -DCMAKE_MSVC_CRT_LINKAGE=%MSVC_CRT_LINKAGE% %CMAKE_EXTRA_OPTIONS% .. || exit /b 1
  ninja crypto || exit /b 1
  copy /y crypto\crypto.lib "..\%Platform%-%MSVC_CRT_LINKAGE%\crypto.lib"
  copy /y crypto\crypto.pdb "..\%Platform%-%MSVC_CRT_LINKAGE%\crypto.pdb"
  copy /y crypto\fipsmodule\CMakeFiles\fipsmodule.dir\vc140.pdb "..\%Platform%-%MSVC_CRT_LINKAGE%\vc140.pdb"
  cd ..
  rmdir build /s /q
)

cd ..\..
