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
set ASM=
set Platform=x86
set VSCMD_ARG_TGT_ARCH=x86
set MSVC_CRT_LINKAGE=static
set COMPILER_TARGET=i686-pc-windows-msvc
set ALLOW_XP=on
set CMAKE_EXTRA_OPTIONS=-DALLOW_XP=on

call "%~dp0callxp-%Platform%.cmd"

call :BuildBoringSSL

python.exe -u .\scripts\build.py || exit /b

set CC=
set CXX=
set ASM=
set Platform=x64
set VSCMD_ARG_TGT_ARCH=x64
set MSVC_CRT_LINKAGE=static
set COMPILER_TARGET=x86_64-pc-windows-msvc
set ALLOW_XP=on
set CMAKE_EXTRA_OPTIONS=-DALLOW_XP=on

call "%~dp0callxp-%Platform%.cmd"

call :BuildBoringSSL

python.exe -u .\scripts\build.py || exit /b

set CC=
set CXX=
set ASM=
set Platform=x86
set VSCMD_ARG_TGT_ARCH=x86
set MSVC_CRT_LINKAGE=dynamic
set COMPILER_TARGET=i686-pc-windows-msvc
set ALLOW_XP=on
set CMAKE_EXTRA_OPTIONS=-DALLOW_XP=on

call "%~dp0callxp-%Platform%.cmd"

call :BuildBoringSSL

python.exe -u .\scripts\build.py || exit /b

set CC=
set CXX=
set ASM=
set Platform=x64
set VSCMD_ARG_TGT_ARCH=x64
set MSVC_CRT_LINKAGE=dynamic
set COMPILER_TARGET=x86_64-pc-windows-msvc
set ALLOW_XP=on
set CMAKE_EXTRA_OPTIONS=-DALLOW_XP=on

call "%~dp0callxp-%Platform%.cmd"

call :BuildBoringSSL

python.exe -u .\scripts\build.py || exit /b

goto :eof

:BuildBoringSSL

cd third_party\boringssl
mkdir "%Platform%-%MSVC_CRT_LINKAGE%-xp"
mkdir "%Platform%-%MSVC_CRT_LINKAGE%-xp\debug"

rem check the existing debug lib
if not exist "%Platform%-%MSVC_CRT_LINKAGE%-xp\debug\crypto.lib" (
  rmdir build /s /q
  mkdir build
  cd build
  cmake -G "Ninja" -DCMAKE_BUILD_TYPE=Debug -DCMAKE_MSVC_CRT_LINKAGE=%MSVC_CRT_LINKAGE% -DCMAKE_C_COMPILER_TARGET=%COMPILER_TARGET% -DCMAKE_CXX_COMPILER_TARGET=%COMPILER_TARGET% %CMAKE_EXTRA_OPTIONS% .. || exit /b 1
  ninja -v crypto || exit /b 1
  copy /y crypto\crypto.lib "..\%Platform%-%MSVC_CRT_LINKAGE%-xp\debug\crypto.lib"
  copy /y crypto\crypto.pdb "..\%Platform%-%MSVC_CRT_LINKAGE%-xp\debug\crypto.pdb"
  copy /y crypto\fipsmodule\CMakeFiles\fipsmodule.dir\vc140.pdb "..\%Platform%-%MSVC_CRT_LINKAGE%-xp\debug\vc140.pdb"
  cd ..
)

rem check the existing release lib
if not exist "%Platform%-%MSVC_CRT_LINKAGE%-xp\crypto.lib" (
  rmdir build /s /q
  mkdir build
  cd build
  cmake -G "Ninja" -DCMAKE_BUILD_TYPE=Release -DCMAKE_MSVC_CRT_LINKAGE=%MSVC_CRT_LINKAGE% -DCMAKE_C_COMPILER_TARGET=%COMPILER_TARGET% -DCMAKE_CXX_COMPILER_TARGET=%COMPILER_TARGET% %CMAKE_EXTRA_OPTIONS% .. || exit /b 1
  ninja crypto || exit /b 1
  copy /y crypto\crypto.lib "..\%Platform%-%MSVC_CRT_LINKAGE%-xp\crypto.lib"
  copy /y crypto\crypto.pdb "..\%Platform%-%MSVC_CRT_LINKAGE%-xp\crypto.pdb"
  copy /y crypto\fipsmodule\CMakeFiles\fipsmodule.dir\vc140.pdb "..\%Platform%-%MSVC_CRT_LINKAGE%-xp\vc140.pdb"
  cd ..
)

cd ..\..

goto :eof
