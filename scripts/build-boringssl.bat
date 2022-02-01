@echo off
setlocal

cd "%~dp0..\third_party\boringssl"

echo "CC: %CC%"
echo "CXX: %CXX%"
echo "ASM: %ASM%"
echo "Platform: %Platform%"
echo "MSVC_CRT_LINKAGE: %MSVC_CRT_LINKAGE%"
echo "COMPILER_TARGET: %COMPILER_TARGET%"
echo "ALLOW_XP: %ALLOW_XP%"

if defined ALLOW_XP (call :SetOutputDirSuffix)
if defined ALLOW_XP (call :SetCmakeExtraOptions)

REM When you pass -DCMAKE_C_COMPILER= with an absolute path you need to use forward slashes.
REM What is setting a value directly into CMakeCache.txt so no automatic slash conversion is done.
set "CMAKE_CC=%CC%"
set "CMAKE_CXX=%CXX%"

set "OUTPUT_DIR=%Platform%-%MSVC_CRT_LINKAGE%%OUTPUT_DIR_SUFFIX%"

mkdir "%OUTPUT_DIR%"
mkdir "%OUTPUT_DIR%\debug"

rem check the existing debug lib
if not exist "%OUTPUT_DIR%\debug\ssl.lib" (
  rmdir build /s /q
  mkdir build
  cd build
  set "CC=%CMAKE_CC%"
  set "CXX=%CMAKE_CXX%"
  cmake -G "Ninja" -DCMAKE_BUILD_TYPE=Debug -DCMAKE_MSVC_CRT_LINKAGE=%MSVC_CRT_LINKAGE% -DCMAKE_C_COMPILER_TARGET=%COMPILER_TARGET% -DCMAKE_CXX_COMPILER_TARGET=%COMPILER_TARGET% "-DCMAKE_ASM_FLAGS=--target=%COMPILER_TARGET%" %CMAKE_EXTRA_OPTIONS% .. || exit /b 1
  set CC=
  set CXX=
  ninja crypto ssl || exit /b 1
  copy /y crypto\crypto.lib "..\%OUTPUT_DIR%\debug\crypto.lib"
  copy /y crypto\crypto.pdb "..\%OUTPUT_DIR%\debug\crypto.pdb"
  copy /y crypto\fipsmodule\CMakeFiles\fipsmodule.dir\vc140.pdb "..\%OUTPUT_DIR%\debug\vc140.pdb"
  copy /y ssl\ssl.lib "..\%OUTPUT_DIR%\debug\ssl.lib"
  copy /y ssl\ssl.pdb "..\%OUTPUT_DIR%\debug\ssl.pdb"
  cd ..
)

rem check the existing release lib
if not exist "%OUTPUT_DIR%\ssl.lib" (
  rmdir build /s /q
  mkdir build
  cd build
  set "CC=%CMAKE_CC%"
  set "CXX=%CMAKE_CXX%"
  cmake -G "Ninja" -DCMAKE_BUILD_TYPE=Release -DCMAKE_MSVC_CRT_LINKAGE=%MSVC_CRT_LINKAGE% -DCMAKE_C_COMPILER_TARGET=%COMPILER_TARGET% -DCMAKE_CXX_COMPILER_TARGET=%COMPILER_TARGET% "-DCMAKE_ASM_FLAGS=--target=%COMPILER_TARGET%" %CMAKE_EXTRA_OPTIONS% .. || exit /b 1
  set CC=
  set CXX=
  ninja crypto ssl || exit /b 1
  copy /y crypto\crypto.lib "..\%OUTPUT_DIR%\crypto.lib"
  copy /y crypto\crypto.pdb "..\%OUTPUT_DIR%\crypto.pdb"
  copy /y crypto\fipsmodule\CMakeFiles\fipsmodule.dir\vc140.pdb "..\%OUTPUT_DIR%\vc140.pdb"
  copy /y ssl\ssl.lib "..\%OUTPUT_DIR%\ssl.lib"
  copy /y ssl\ssl.pdb "..\%OUTPUT_DIR%\ssl.pdb"
  cd ..
)

cd ..\..

cd "%~dp0..\"

goto :eof

:SetOutputDirSuffix
set OUTPUT_DIR_SUFFIX=-xp
goto :eof

:SetCmakeExtraOptions
set CMAKE_EXTRA_OPTIONS=-DALLOW_XP=on
goto :eof
