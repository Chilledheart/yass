# Building Instruments

## GeneralInstallation/Debian/Ubuntu
1. Install GNU C++ Compiler:
```
sudo apt-get install -y build-essential
```
2. Install below dependencies:
```
sudo apt-get install -y \
    cmake \
    ninja-build \
    pkg-config \
    perl \
    libunwind-dev \
    libgtk-3-dev \
    libgtkmm-3.0-dev

```
3. Install golang:
```
wget https://go.dev/dl/go1.16.13.linux-amd64.tar.gz
sudo tar -C /usr/local -xzf go1.16.13.linux-amd64.tar.gz
echo 'export PATH=$PATH:/usr/local/go/bin' >> ~/.bashrc
```
4. Compile the program with default configuration.
```
mkdir build
cd build
cmake -G Ninja..
ninja
```

## Debian/Packaging

```
sudo apt-get install build-essential fakeroot devscripts
./scripts/build-deb.sh
```

## GeneralInstallation/Fedora
1. Install GNU C++ Compiler:
```
sudo dnf install gcc make python bash coreutils diffutils patch
```
2. Install below dependencies:
```
sudo dnf install \
    cmake \
    ninja-build \
    pkg-config \
    perl \
    libunwind-devel \
    gtk3-devel \
    gtkmm30-devel \
    golang
```
3. Compile the program with default configuration.
```
mkdir build
cd build
cmake -G Ninja..
ninja
```

## Fedora/Packaging

```
sudo dnf install gcc make python bash coreutils diffutils patch
./scripts/build-rpm.sh
```

## macOS

1. Make sure you have Xcode Command Line Tools installed (Xcode if possible):
```
xcode-select --install
```
2. Install [MacPorts] and dependencies...
```
    ninja cmake go
```
2. Install [HomeBrew] and dependencies...
```
    ninja cmake go
```

3. use script to build Release App under `build` directory.
```
scripts/build.py
```

## Windows

1. Make sure you use Visual Studio 2019 or later.

2. use script to build Release App under `build` directory.
```
scripts/build.py
```

## boringssl

For boringssl, you should only need crypto target.

Requirement of building boringssl:

  * [CMake](https://cmake.org/download/) 3.5 or later is required.

  * A recent version of Perl is required. On Windows,
    [Active State Perl](http://www.activestate.com/activeperl/) has been
    reported to work, as has MSYS Perl.
    [Strawberry Perl](http://strawberryperl.com/) also works but it adds GCC
    to `PATH`, which can confuse some build tools when identifying the compiler
    (removing `C:\Strawberry\c\bin` from `PATH` should resolve any problems).
    If Perl is not found by CMake, it may be configured explicitly by setting
    `PERL_EXECUTABLE`.

  * Building with [Ninja](https://ninja-build.org/) instead of Make is
    recommended, because it makes builds faster. On Windows, CMake's Visual
    Studio generator may also work, but it not tested regularly and requires
    recent versions of CMake for assembly support.

  * On Windows only, [NASM](https://www.nasm.us/) is required. If not found
    by CMake, it may be configured explicitly by setting
    `CMAKE_ASM_NASM_COMPILER`.

  * C and C++ compilers with C++11 support are required. On Windows, MSVC 14
    (Visual Studio 2015) or later with Platform SDK 8.1 or later are supported,
    but newer versions are recommended. We will drop support for Visual Studio
    2015 in March 2022, five years after the release of Visual Studio 2017.
    Recent versions of GCC (6.1+) and Clang should work on non-Windows
    platforms, and maybe on Windows too.

  * The most recent stable version of [Go](https://golang.org/dl/) is required.
    Note Go is exempt from the five year support window. If not found by CMake,
    the go executable may be configured explicitly by setting `GO_EXECUTABLE`.

(for Windows)
```
cd third_party/boringssl
mkdir build
mkdir debug
cd build

cmake -G "Ninja" -DCMAKE_BUILD_TYPE=Debug ..
ninja crypto
copy /y crypto\crypto.lib ..\debug\crypto.lib

cmake -G "Ninja" -DCMAKE_BUILD_TYPE=Release ..
ninja crypto
copy /y crypto\crypto.lib ..\crypto.lib
cd ..
rmdir build /s /q
```

(for Linux)
Run:
```
cd third_party/boringssl
mkdir build
cd build
cmake -G "Ninja" -DCMAKE_BUILD_TYPE=Release ..
ninja crypto
cp -fv crypto/libcrypto.a ../libcrypto.a
cd ..
```

(for macOS)
```
set(CMAKE_ASM_FLAGS "-mmacosx-version-min=10.10 ${CMAKE_ASM_FLAGS}")
```
Run these commands to build crypto target.
(first x86_64 slice)
```
cd third_party/boringssl
mkdir build
cd build
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=10.10 \
  -DCMAKE_OSX_ARCHITECTURES="x86_64" ..
ninja crypto
cp -fv crypto/libcrypto.a ../x64-libcrypto.a
cd ..
```
(and then arm64 slice)
```
cd third_party/boringssl
mkdir build
cd build
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=10.10 \
  -DCMAKE_OSX_ARCHITECTURES="arm64" ..
ninja crypto
cp -fv crypto/libcrypto.a ../arm64-libcrypto.a
cd ..
```
(create fat binary)
```
cd third_party/boringssl
lipo -create arm64-libcrypto.a x64-libcrypto.a -output libcrypto.a
lipo -info libcrypto.a
```
Built universal target at ``libcrypto.a``

## Updating boringssl

Follow ``https://chromium.googlesource.com/chromium/src/+/HEAD/DEPS``
Use ``boringssl_revision`` field's value to rebase TOT.

[vcpkg]: https://github.com/microsoft/vcpkg
[MacPorts]: https://www.macports.org/install.php
[HomeBrew]: https://brew.sh
