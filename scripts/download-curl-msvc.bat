@echo off
setlocal

REM Script for download toolchains (msvc)
REM
REM Usage: download-toolchain-msvc.bat [arch]

cd /D "%~dp0"
cd ..\third_party

if "%1"=="" goto DownloadI686
if "%1"=="i686" goto DownloadI686
if "%1"=="x86" goto DownloadI686
if "%1"=="x86_64" goto DownloadX86_64
if "%1"=="x64" goto DownloadX86_64

echo Unsupported architecture: %1
exit /B 1

:DownloadX86_64
echo Select architecture: %1
curl -L -O https://github.com/Chilledheart/curl/releases/download/curl-8_2_0/libcurl-vc17-x64-release-static-ipv6-sspi-schannel.zip
if %ERRORLEVEL% NEQ 0 exit /B %ERRORLEVEL%
"C:\Program Files\7-Zip\7z.exe" x libcurl-vc17-x64-release-static-ipv6-sspi-schannel.zip -aoa
if %ERRORLEVEL% NEQ 0 exit /B %ERRORLEVEL%
del /s /q libcurl-vc17-x64-release-static-ipv6-sspi-schannel.zip
exit /B 0

:DownloadI686
echo Select architecture: %1
curl -L -O https://github.com/Chilledheart/curl/releases/download/curl-8_2_0/libcurl-vc17-x86-release-static-ipv6-sspi-schannel.zip
if %ERRORLEVEL% NEQ 0 exit /B %ERRORLEVEL%
"C:\Program Files\7-Zip\7z.exe" x libcurl-vc17-x86-release-static-ipv6-sspi-schannel.zip -aoa
if %ERRORLEVEL% NEQ 0 exit /B %ERRORLEVEL%
del /s /q libcurl-vc17-x86-release-static-ipv6-sspi-schannel.zip
exit /B 0
