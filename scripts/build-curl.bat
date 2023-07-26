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
curl -L -O https://github.com/curl/curl/releases/download/curl-8_2_0/curl-8.2.0.zip
if %ERRORLEVEL% NEQ 0 exit /B %ERRORLEVEL%
"C:\Program Files\7-Zip\7z.exe" x curl-8.2.0.zip -aoa
if %ERRORLEVEL% NEQ 0 exit /B %ERRORLEVEL%
del /s /q curl-8.2.0.zip

:Build
set Platform=x86
call "%vsdevcmd%" -arch=%Platform% -host_arch=%Platform% -winsdk=%Winsdk% -no_logo -vcvars_ver=%VCToolsVersion%

cd curl-8.2.0\winbuild
nmake /f Makefile.vc mode=static RTLIBCFG=static debug=no VC=16
if %ERRORLEVEL% NEQ 0 exit /B %ERRORLEVEL%
cd ..\builds
"C:\Program Files\7-Zip\7z.exe" a -tzip libcurl-vc16-x86-release-static-ipv6-sspi-schannel.zip libcurl-vc16-x86-release-static-ipv6-sspi-schannel
if %ERRORLEVEL% NEQ 0 exit /B %ERRORLEVEL%
xcopy /F /S /E /I libcurl-vc16-x86-release-static-ipv6-sspi-schannel ..\..\libcurl-vc16-x86-release-static-ipv6-sspi-schannel
if %ERRORLEVEL% NEQ 0 exit /B %ERRORLEVEL%
move libcurl-vc16-x86-release-static-ipv6-sspi-schannel.zip ..\..\..\
if %ERRORLEVEL% NEQ 0 exit /B %ERRORLEVEL%
cd ..\winbuild

set Platform=x64
call "%vsdevcmd%" -arch=%Platform% -host_arch=%Platform% -winsdk=%Winsdk% -no_logo -vcvars_ver=%VCToolsVersion%

cd curl-8.2.0\winbuild
nmake /f Makefile.vc mode=static RTLIBCFG=static debug=no VC=16
if %ERRORLEVEL% NEQ 0 exit /B %ERRORLEVEL%
cd ..\builds
"C:\Program Files\7-Zip\7z.exe" a -tzip libcurl-vc16-x64-release-static-ipv6-sspi-schannel.zip libcurl-vc16-x64-release-static-ipv6-sspi-schannel
if %ERRORLEVEL% NEQ 0 exit /B %ERRORLEVEL%
xcopy /F /S /E /I libcurl-vc16-x64-release-static-ipv6-sspi-schannel ..\..\libcurl-vc16-x64-release-static-ipv6-sspi-schannel
if %ERRORLEVEL% NEQ 0 exit /B %ERRORLEVEL%
move libcurl-vc16-x64-release-static-ipv6-sspi-schannel.zip ..\..\..\
if %ERRORLEVEL% NEQ 0 exit /B %ERRORLEVEL%
cd ..\winbuild

cd ..\..\
del /s /q curl-8.2.0
