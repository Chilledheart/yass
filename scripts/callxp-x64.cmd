@echo off

REM Followed the guide of Windows XP Targeting with C++ in Visual Studio 2012
REM Targeting from the Command Line
REM https://devblogs.microsoft.com/cppblog/windows-xp-targeting-with-c-in-visual-studio-2012/

set _path=
call :set_path "C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\bin\amd64"
call :set_path "C:\Program Files (x86)\Microsoft SDKs\Windows\v7.1A\bin\x64"
call :set_path "C:\Program Files (x86)\Microsoft SDKs\Windows\v10.0A\bin\NETFX 4.6.1 Tools\x64"
call :set_path "C:\Program Files (x86)\Microsoft Visual Studio 14.0\Common7\Tools\bin"
call :set_path "C:\Program Files (x86)\Microsoft Visual Studio 14.0\Common7\Tools"
call :set_path "C:\Program Files (x86)\Microsoft Visual Studio 14.0\Common7\IDE"
call :set_path "C:\Program Files (x86)\HTML Help Workshop"
call :set_path "C:\Program Files (x86)\MSBuild\14.0\bin\amd64"
call :set_path "C:\Windows\Microsoft.NET\Framework\v4.0.30319"
call :set_path "C:\Windows\System32"
call :set_path "C:\Program Files (x86)\Microsoft Visual Studio 14.0\Common7\IDE\CommonExtensions\Microsoft\TeamFoundation\Team Explorer\NativeBinaries\x64"

set _include=
call :set_include "C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\include"
call :set_include "C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\atlmfc\include"
call :set_include "C:\Program Files (x86)\Windows Kits\10\Include\10.0.10240.0\ucrt"
call :set_include "C:\Program Files (x86)\Microsoft SDKs\Windows\v7.1A\include"

set _lib=
call :set_lib "C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\lib\amd64"
call :set_lib "C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\atlmfc\lib\amd64"
call :set_lib "C:\Program Files (x86)\Windows Kits\10\lib\10.0.10240.0\ucrt\x64"
call :set_lib "C:\Program Files (x86)\Microsoft SDKs\Windows\v7.1A\lib\x64"

if defined _CALLXP_PREVIOUS_PATH (
  set "PATH=%_CALLXP_PREVIOUS_PATH%"
) else (
  set "_CALLXP_PREVIOUS_PATH=%PATH%"
)
if defined _CALLXP_PREVIOUS_INCLUDE (
  set "INCLUDE=%_CALLXP_PREVIOUS_INCLUDE%"
) else (
  set "_CALLXP_PREVIOUS_INCLUDE=%INCLUDE%"
)
if defined _CALLXP_PREVIOUS_LIB (
  set "LIB=%_CALLXP_PREVIOUS_LIB%"
) else (
  set "_CALLXP_PREVIOUS_LIB=%LIB%"
)

set "PATH=%_path%;%PATH%"
set "INCLUDE=%_include%;%INCLUDE%"
set "LIB=%_lib%;%LIB%"

set _path=
set _include=
set _lib=

goto :eof

:-------------------
:set_path
set "_path=%_path%;%~1"
goto :eof

:-------------------
:set_include
set "_include=%_include%;%~1"
goto :eof

:-------------------
:set_lib
set "_lib=%_lib%;%~1"
goto :eof
