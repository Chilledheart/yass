@echo off

REM Followed the guide of Windows XP Targeting with C++ in Visual Studio 2012
REM Targeting from the Command Line
REM https://devblogs.microsoft.com/cppblog/windows-xp-targeting-with-c-in-visual-studio-2012/

set _path=
call :set_path "%VCINSTALLDIR%\Tools\MSVC\14.16.27023\bin\HostX86\x86"
call :set_path "C:\Program Files (x86)\Microsoft SDKs\Windows\v7.1A\bin"
call :set_path "C:\Program Files (x86)\Microsoft SDKs\Windows\v10.0A\bin\NETFX 4.6.1 Tools"
call :set_path "%VCINSTALLDIR%\..\Common7\IDE\CommonExtensions\Microsoft\TeamFoundation\Team Explorer"
call :set_path "%VCINSTALLDIR%\..\MSBuild\Current\Bin"
call :set_path "C:\Windows\SysWow64"

set _include=
call :set_include "%VCINSTALLDIR%\Tools\MSVC\14.16.27023\include"
call :set_include "%VCINSTALLDIR%\Tools\MSVC\14.16.27023\atlmfc\include"
call :set_include "C:\Program Files (x86)\Windows Kits\10\Include\10.0.10240.0\ucrt"
call :set_include "C:\Program Files (x86)\Microsoft SDKs\Windows\v7.1A\include"

set _lib=
call :set_lib "%VCINSTALLDIR%\Tools\MSVC\14.16.27023\lib\x86"
call :set_lib "%VCINSTALLDIR%\Tools\MSVC\14.16.27023\atlmfc\lib\x86"
call :set_lib "C:\Program Files (x86)\Windows Kits\10\lib\10.0.10240.0\ucrt\x86"
call :set_lib "C:\Program Files (x86)\Microsoft SDKs\Windows\v7.1A\lib"

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
set "_path=%~1;%_path%"
goto :eof

:-------------------
:set_include
set "_include=%~1;%_include%"
goto :eof

:-------------------
:set_lib
set "_lib=%~1;%_lib%"
goto :eof
