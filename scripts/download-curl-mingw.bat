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
curl -L -O https://curl.se/windows/dl-8.2.0_1/curl-8.2.0_1-win64-mingw.zip
if %ERRORLEVEL% NEQ 0 exit /B %ERRORLEVEL%
"C:\Program Files\7-Zip\7z.exe" x curl-8.2.0_1-win64-mingw.zip -aoa
if %ERRORLEVEL% NEQ 0 exit /B %ERRORLEVEL%
REM xcopy /F curl-8.2.0_1-win64-mingw\bin\libcurl-x64.dll libcurl-x64.dll
del /s /q curl-8.2.0_1-win64-mingw.zip
exit /B 0

:DownloadI686
echo Select architecture: %1
curl -L -O https://curl.se/windows/dl-8.2.0_1/curl-8.2.0_1-win32-mingw.zip
if %ERRORLEVEL% NEQ 0 exit /B %ERRORLEVEL%
"C:\Program Files\7-Zip\7z.exe" x curl-8.2.0_1-win32-mingw.zip -aoa
if %ERRORLEVEL% NEQ 0 exit /B %ERRORLEVEL%
REM xcopy /F curl-8.2.0_1-win32-mingw\bin\libcurl.dll libcurl.dll
del /s /q curl-8.2.0_1-win32-mingw.zip
exit /B 0
