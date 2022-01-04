@echo off
setlocal

REM Script for building the YASS on Windows,
REM
REM Usage: build.bat

REM Prerequisites:
REM
REM   Visual Studio 2019, CMake, Ninja,
REM   Visual Studio 2019 SDK and Python.
REM

REM You need to modify the paths below:
set vsdevcmd=C:\Program Files (x86)\Microsoft Visual Studio\2019\Enterprise\Common7\Tools\VsDevCmd.bat

set "VSCMD_START_DIR=%CD%"
call "%vsdevcmd%" "-arch=%Platform%" -host_arch=amd64
set CC=
set CXX=

python -u .\scripts\build.py || exit /b
