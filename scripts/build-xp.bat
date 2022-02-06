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
set "VCINSTALLDIR=C:\Program Files (x86)\Microsoft Visual Studio\2019\Enterprise\VC"
set "WindowsSDKVersion=10.0.10240.0\"
set "WindowsSdkDir=C:\Program Files (x86)\Windows Kits\10"

REM
REM Generate static x86 binary
REM
set "CC=%CD%\third_party\llvm-build\Release+Asserts\bin\clang-cl.exe"
set "CXX=%CD%\third_party\llvm-build\Release+Asserts\bin\clang-cl.exe"
set Platform=x86
set VSCMD_ARG_TGT_ARCH=x86
set MSVC_CRT_LINKAGE=static
set COMPILER_TARGET=i686-pc-windows-msvc
set ALLOW_XP=on

call "%~dp0callxp-%Platform%.cmd"

python.exe -u .\scripts\build.py || exit /b
call :RenameTarball

REM
REM Generate dynamic x86 binary
REM
set "CC=%CD%\third_party\llvm-build\Release+Asserts\bin\clang-cl.exe"
set "CXX=%CD%\third_party\llvm-build\Release+Asserts\bin\clang-cl.exe"
set Platform=x86
set VSCMD_ARG_TGT_ARCH=x86
set MSVC_CRT_LINKAGE=dynamic
set COMPILER_TARGET=i686-pc-windows-msvc
set ALLOW_XP=on

call "%~dp0callxp-%Platform%.cmd"

python.exe -u .\scripts\build.py || exit /b
call :RenameTarball

goto :eof

:RenameTarball
python.exe -c "import subprocess, os; check_string_output = lambda command: subprocess.check_output(command, stderr=subprocess.STDOUT).decode().strip(); p = os.getenv('Platform'); l = os.getenv('MSVC_CRT_LINKAGE'); t = check_string_output(['git', 'describe', '--tags', 'HEAD']); os.rename('yass.zip', f'yass-win-xp-release-{p}-{l}-{t}.zip'); os.rename('yass-debuginfo.zip', f'yass-win-xp-release-{p}-{l}-{t}-debuginfo.zip');"
