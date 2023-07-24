@echo off
setlocal

REM Script for download toolchains (mingw)
REM
REM Usage: download-toolchain-mingw.bat [arch]

cd /D "%~dp0"
cd ..\third_party

if "%1"=="" goto DownloadI686
if "%1"=="i686" goto DownloadI686
if "%1"=="x86_64" goto DownloadX86_64

echo Unsupported architecture: %1
exit /B 1

:DownloadX86_64
echo Select architecture: %1
curl -L -O https://github.com/brechtsanders/winlibs_mingw/releases/download/7.5.0-7.0.0-r1/winlibs-x86_64-posix-seh-gcc-7.5.0-mingw-w64-7.0.0-r1.7z
if %ERRORLEVEL% NEQ 0 exit /B %ERRORLEVEL%
"C:\Program Files\7-Zip\7z.exe" x winlibs-x86_64-posix-seh-gcc-7.5.0-mingw-w64-7.0.0-r1.7z -aoa
if %ERRORLEVEL% NEQ 0 exit /B %ERRORLEVEL%
del /s /q winlibs-x86_64-posix-seh-gcc-7.5.0-mingw-w64-7.0.0-r1.7z
exit /B 0

:DownloadI686
echo Select architecture: %1
curl -L -O https://github.com/brechtsanders/winlibs_mingw/releases/download/7.5.0-7.0.0-r1/winlibs-i686-posix-dwarf-gcc-7.5.0-mingw-w64-7.0.0-r1.7z
if %ERRORLEVEL% NEQ 0 exit /B %ERRORLEVEL%
"C:\Program Files\7-Zip\7z.exe" x winlibs-i686-posix-dwarf-gcc-7.5.0-mingw-w64-7.0.0-r1.7z -aoa
if %ERRORLEVEL% NEQ 0 exit /B %ERRORLEVEL%
del /s /q winlibs-i686-posix-dwarf-gcc-7.5.0-mingw-w64-7.0.0-r1.7z
exit /B 0
