@echo off
setlocal

REM Script for download toolchains
REM
REM Usage: download-toolchain.bat

mkdir third_party

echo "Install dependency: prebuilt nasm"

cd third_party
curl -O https://www.nasm.us/pub/nasm/releasebuilds/2.15.05/win64/nasm-2.15.05-win64.zip
"C:\Program Files\7-Zip\7z.exe" x nasm-2.15.05-win64.zip
rename nasm-2.15.05 nasm
cd ..

echo "Install dependency: prebuilt clang and clang-tidy binaries"

python -u scripts\download-clang-prebuilt-binaries.py

echo "Install dependency: wixtoolset 3"

curl -L -O https://github.com/wixtoolset/wix3/releases/download/wix3112rtm/wix311-binaries.zip
"C:\Program Files\7-Zip\7z.exe" x "-othird_party\wix311" wix311-binaries.zip
