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
REM Generate dynamic x86 binary
REM
set "VSCMD_START_DIR=%CD%"
set "CC=%CD%\third_party\llvm-build\Release+Asserts\bin\clang-cl.exe"
set "CXX=%CD%\third_party\llvm-build\Release+Asserts\bin\clang-cl.exe"
set ASM=
set Platform=x86
set MSVC_CRT_LINKAGE=dynamic
set COMPILER_TARGET=i686-pc-windows-msvc

call "%vsdevcmd%" -arch=%Platform% -host_arch=amd64 -winsdk=%Winsdk% -no_logo -vcvars_ver=%VCToolsVersion%

python.exe -u .\scripts\build.py || exit /b
call :RenameTarball

REM
REM Generate dynamic x64 binary
REM
set "VSCMD_START_DIR=%CD%"
set "CC=%CD%\third_party\llvm-build\Release+Asserts\bin\clang-cl.exe"
set "CXX=%CD%\third_party\llvm-build\Release+Asserts\bin\clang-cl.exe"
set ASM=
set Platform=x64
set MSVC_CRT_LINKAGE=dynamic
set COMPILER_TARGET=x86_64-pc-windows-msvc

call "%vsdevcmd%" -arch=%Platform% -host_arch=amd64 -winsdk=%Winsdk% -no_logo -vcvars_ver=%VCToolsVersion%

python.exe -u .\scripts\build.py || exit /b
call :RenameTarball

REM skip ARM build
goto :BuildARM64

REM
REM Generate dynamic arm binary
REM
set VCToolsVersion=14.29
set "VSCMD_START_DIR=%CD%"
set "CC=%CD%\third_party\llvm-build\Release+Asserts\bin\clang-cl.exe"
set "CXX=%CD%\third_party\llvm-build\Release+Asserts\bin\clang-cl.exe"
set ASM=
set Platform=arm
set MSVC_CRT_LINKAGE=dynamic
set COMPILER_TARGET=arm-pc-windows-msvc

call "%vsdevcmd%" -arch=%Platform% -host_arch=amd64 -winsdk=%Winsdk% -no_logo -vcvars_ver=%VCToolsVersion%

python.exe -u .\scripts\build.py || exit /b
call :RenameTarball

:BuildARM64

REM
REM Generate dynamic Arm64 binary
REM
REM Use Visual Studio 2019's toolchain for ARM64 target

set VCToolsVersion=14.29

set "VSCMD_START_DIR=%CD%"
set "CC=%CD%\third_party\llvm-build\Release+Asserts\bin\clang-cl.exe"
set "CXX=%CD%\third_party\llvm-build\Release+Asserts\bin\clang-cl.exe"
set ASM=
set Platform=arm64
set MSVC_CRT_LINKAGE=dynamic
set COMPILER_TARGET=arm64-pc-windows-msvc

call "%vsdevcmd%" -arch=%Platform% -host_arch=amd64 -winsdk=%Winsdk% -no_logo -vcvars_ver=%VCToolsVersion%

python.exe -u .\scripts\build.py || exit /b
call :RenameTarball

goto :eof

:RenameTarball
python.exe -c "import subprocess, os; check_string_output = lambda command: subprocess.check_output(command, stderr=subprocess.STDOUT).decode().strip(); p = os.getenv('Platform'); l = os.getenv('MSVC_CRT_LINKAGE'); t = check_string_output(['git', 'describe', '--tags', 'HEAD']); os.rename('yass.zip', f'yass-win-release-{p}-{l}-{t}.zip'); os.rename('yass.msi', f'yass-win-release-{p}-{l}-{t}.msi'); os.rename('yass-debuginfo.zip', f'yass-win-release-{p}-{l}-{t}-debuginfo.zip');"
