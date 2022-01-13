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

REM You need to modify the paths below:
set vsdevcmd=C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\Tools\VsDevCmd.bat

set "VSCMD_START_DIR=%CD%"
call "%vsdevcmd%" -arch=x86 -host_arch=amd64
set CC=
set CXX=
set Platform=x86

cd third_party\boringssh
mkdir "%Platform%"
mkdir "%Platform%\debug"

if not exist "%Platform%\debug\crypto.lib" (
  mkdir build
  cd build
  cmake -G "Ninja" -DCMAKE_BUILD_TYPE=Debug ..
  ninja crypto
  copy /y crypto\crypto.lib "..\%Platform%\debug\crypto.lib"
  cd ..
  rmdir build /s /q
)

if not exist "%Platform%\crypto.lib" (
  mkdir build
  cd build
  cmake -G "Ninja" -DCMAKE_BUILD_TYPE=Release ..
  ninja crypto
  copy /y crypto\crypto.lib "..\%Platform%\crypto.lib"
  cd ..
  rmdir build /s /q
)

python.exe -u .\scripts\build.py || exit /b

set "VSCMD_START_DIR=%CD%"
call "%vsdevcmd%" -arch=amd64 -host_arch=amd64
set CC=
set CXX=
set Platform=x64

cd third_party\boringssh
mkdir "%Platform%"
mkdir "%Platform%\debug"

if not exist "%Platform%\debug\crypto.lib" (
  mkdir build
  cd build
  cmake -G "Ninja" -DCMAKE_BUILD_TYPE=Debug ..
  ninja crypto
  copy /y crypto\crypto.lib "..\%Platform%\debug\crypto.lib"
  cd ..
  rmdir build /s /q
)

if not exist "%Platform%\crypto.lib" (
  mkdir build
  cd build
  cmake -G "Ninja" -DCMAKE_BUILD_TYPE=Release ..
  ninja crypto
  copy /y crypto\crypto.lib "..\%Platform%\crypto.lib"
  cd ..
  rmdir build /s /q
)

python.exe -u .\scripts\build.py || exit /b

set "VSCMD_START_DIR=%CD%"
call "%vsdevcmd%" -arch=arm64 -host_arch=amd64
set CC=
set CXX=
set Platform=arm64

cd third_party\boringssh
mkdir "%Platform%"
mkdir "%Platform%\debug"

if not exist "%Platform%\debug\crypto.lib" (
  mkdir build
  cd build
  cmake -G "Ninja" -DCMAKE_BUILD_TYPE=Debug ..
  ninja crypto
  copy /y crypto\crypto.lib "..\%Platform%\debug\crypto.lib"
  cd ..
  rmdir build /s /q
)

if not exist "%Platform%\crypto.lib" (
  mkdir build
  cd build
  cmake -G "Ninja" -DCMAKE_BUILD_TYPE=Release ..
  ninja crypto
  copy /y crypto\crypto.lib "..\%Platform%\crypto.lib"
  cd ..
  rmdir build /s /q
)

python.exe -u .\scripts\build.py || exit /b
