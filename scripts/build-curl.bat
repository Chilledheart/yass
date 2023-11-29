@echo off
setlocal

REM Script for building the curl on Windows,
REM
REM Usage: build-curl.bat

set VisualStudioInstallerFolder="%ProgramFiles(x86)%\Microsoft Visual Studio\Installer"
if %PROCESSOR_ARCHITECTURE%==x86 set VisualStudioInstallerFolder="%ProgramFiles%\Microsoft Visual Studio\Installer"

pushd %VisualStudioInstallerFolder%
for /f "usebackq tokens=*" %%i in (`vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
  set VisualStudioInstallDir=%%i
)
popd

set VCToolsVersion=14.29
set Winsdk=10.0.19041.0
set "WindowsSDKVersion=%Winsdk%\"
set "vsdevcmd=%VisualStudioInstallDir%\Common7\Tools\VsDevCmd.bat"
set CL=/MP

cd /D "%~dp0"
cd ..\third_party

:DownloadCurl
REM
REM download source tarball
REM
curl -L -O https://github.com/curl/curl/releases/download/curl-8_4_0/curl-8.4.0.zip
if %ERRORLEVEL% NEQ 0 exit /B %ERRORLEVEL%
"C:\Program Files\7-Zip\7z.exe" x curl-8.4.0.zip -aoa
if %ERRORLEVEL% NEQ 0 exit /B %ERRORLEVEL%
del /s /q curl-8.4.0.zip

:Build
REM
REM x86 build
REM
set Platform=x86
call "%vsdevcmd%" -arch=%Platform% -host_arch=amd64 -winsdk=%Winsdk% -no_logo -vcvars_ver=%VCToolsVersion%

cd curl-8.4.0\winbuild
nmake /f Makefile.vc mode=static MACHINE=x86 RTLIBCFG=static debug=no VC=16 ENABLE_UNICODE=yes
if %ERRORLEVEL% NEQ 0 exit /B %ERRORLEVEL%
cd ..\builds
"C:\Program Files\7-Zip\7z.exe" a -tzip libcurl-vc16-x86-release-static-ipv6-sspi-schannel.zip libcurl-vc16-x86-release-static-ipv6-sspi-schannel
if %ERRORLEVEL% NEQ 0 exit /B %ERRORLEVEL%
xcopy /F /S /E /I libcurl-vc16-x86-release-static-ipv6-sspi-schannel ..\..\libcurl-vc16-x86-release-static-ipv6-sspi-schannel
if %ERRORLEVEL% NEQ 0 exit /B %ERRORLEVEL%
move libcurl-vc16-x86-release-static-ipv6-sspi-schannel.zip ..\..\..\
if %ERRORLEVEL% NEQ 0 exit /B %ERRORLEVEL%
cd ..\winbuild

REM
REM x64 build
REM
set Platform=x64
call "%vsdevcmd%" -arch=%Platform% -host_arch=amd64 -winsdk=%Winsdk% -no_logo -vcvars_ver=%VCToolsVersion%

cd curl-8.4.0\winbuild
nmake /f Makefile.vc mode=static MACHINE=x64 RTLIBCFG=static debug=no VC=16 ENABLE_UNICODE=yes
if %ERRORLEVEL% NEQ 0 exit /B %ERRORLEVEL%
cd ..\builds
"C:\Program Files\7-Zip\7z.exe" a -tzip libcurl-vc16-x64-release-static-ipv6-sspi-schannel.zip libcurl-vc16-x64-release-static-ipv6-sspi-schannel
if %ERRORLEVEL% NEQ 0 exit /B %ERRORLEVEL%
xcopy /F /S /E /I libcurl-vc16-x64-release-static-ipv6-sspi-schannel ..\..\libcurl-vc16-x64-release-static-ipv6-sspi-schannel
if %ERRORLEVEL% NEQ 0 exit /B %ERRORLEVEL%
move libcurl-vc16-x64-release-static-ipv6-sspi-schannel.zip ..\..\..\
if %ERRORLEVEL% NEQ 0 exit /B %ERRORLEVEL%
cd ..\winbuild

REM
REM arm64 build
REM
set Platform=arm64
call "%vsdevcmd%" -arch=%Platform% -host_arch=amd64 -winsdk=%Winsdk% -no_logo -vcvars_ver=%VCToolsVersion%
cd curl-8.4.0\winbuild
nmake /f Makefile.vc mode=static MACHINE=arm64 RTLIBCFG=static debug=no VC=16 ENABLE_UNICODE=yes
if %ERRORLEVEL% NEQ 0 exit /B %ERRORLEVEL%
cd ..\builds
"C:\Program Files\7-Zip\7z.exe" a -tzip libcurl-vc16-arm64-release-static-ipv6-sspi-schannel.zip libcurl-vc16-arm64-release-static-ipv6-sspi-schannel
if %ERRORLEVEL% NEQ 0 exit /B %ERRORLEVEL%
xcopy /F /S /E /I libcurl-vc16-arm64-release-static-ipv6-sspi-schannel ..\..\libcurl-vc16-arm64-release-static-ipv6-sspi-schannel
if %ERRORLEVEL% NEQ 0 exit /B %ERRORLEVEL%
move libcurl-vc16-arm64-release-static-ipv6-sspi-schannel.zip ..\..\..\
if %ERRORLEVEL% NEQ 0 exit /B %ERRORLEVEL%
cd ..\winbuild

REM
REM cleanup
REM
cd ..\..\
del /s /q curl-8.4.0
