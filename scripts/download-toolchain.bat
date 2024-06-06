@echo off
setlocal

REM Script for download toolchains
REM
REM Usage: download-toolchain.bat

echo "Install dependency: prebuilt nasm"

cd third_party
curl -C - -L -O https://www.nasm.us/pub/nasm/releasebuilds/2.16.03/win64/nasm-2.16.03-win64.zip
if %ERRORLEVEL% NEQ 0 exit /B %ERRORLEVEL%
"C:\Program Files\7-Zip\7z.exe" x nasm-2.16.03-win64.zip -aoa
if %ERRORLEVEL% NEQ 0 exit /B %ERRORLEVEL%
rmdir /s /q nasm
rename nasm-2.16.03 nasm
if %ERRORLEVEL% NEQ 0 exit /B %ERRORLEVEL%
cd ..
del /s /q third_party\nasm-2*.zip

echo "Install dependency: prebuilt clang and clang-tidy binaries"

python -u scripts\download-clang-prebuilt-binaries.py
if %ERRORLEVEL% NEQ 0 exit /B %ERRORLEVEL%
del /s /q third_party\llvm-build\Release+Asserts\*.tgz

echo "Install dependency: wixtoolset 3"

cd third_party
curl -C - -L -O https://github.com/wixtoolset/wix3/releases/download/wix3112rtm/wix311-binaries.zip
if %ERRORLEVEL% NEQ 0 exit /B %ERRORLEVEL%
rmdir /s /q wix311
"C:\Program Files\7-Zip\7z.exe" x "-owix311" wix311-binaries.zip -aoa
if %ERRORLEVEL% NEQ 0 exit /B %ERRORLEVEL%
cd ..
del /s /q third_party\wix311*.zip

echo "Install dependency: nsis"

cd third_party
curl -C - -L -o nsis-3.10.zip "https://sourceforge.net/projects/nsis/files/NSIS%%203/3.10/nsis-3.10.zip/download"
if %ERRORLEVEL% NEQ 0 exit /B %ERRORLEVEL%
"C:\Program Files\7-Zip\7z.exe" x nsis-3.10.zip -aoa
if %ERRORLEVEL% NEQ 0 exit /B %ERRORLEVEL%
rmdir /s /q nsis
rename nsis-3.10 nsis
if %ERRORLEVEL% NEQ 0 exit /B %ERRORLEVEL%
cd ..
del /s /q third_party\nsis-*.zip
