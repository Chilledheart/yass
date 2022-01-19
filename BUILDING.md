# Building Instruments

## GeneralInstallation/Ubuntu
1. Install GNU C++ Compiler:
```
apt-get install -y build-essential
```
2. Install below dependencies:
```
apt-get install -y \
    cmake \
    ninja-build \
    libsodium-dev \
    libjsoncpp-dev \
    libwxgtk3.0-gtk3-dev (optional)

```
3. Compile the program with default configuration.
```
mkdir build
cd build
cmake -G Ninja..
ninja
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

(for Windows)
Add these code at CMakeLists.txt to target static build.
```
set(CompilerFlags
        CMAKE_CXX_FLAGS
        CMAKE_CXX_FLAGS_DEBUG
        CMAKE_CXX_FLAGS_RELEASE
        CMAKE_C_FLAGS
        CMAKE_C_FLAGS_DEBUG
        CMAKE_C_FLAGS_RELEASE
        )
foreach(CompilerFlag ${CompilerFlags})
  string(REPLACE "/MD" "/MT" ${CompilerFlag} "${${CompilerFlag}}")
endforeach()
```
And run:
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
```
wget https://go.dev/dl/go1.16.13.linux-amd64.tar.gz
sudo tar -C /usr/local -xzf go1.16.13.linux-amd64.tar.gz
echo 'export PATH=$PATH:/usr/local/go/bin' >> ~/.bashrc
sudo apt-get install libunwind-dev
```
Relogin and run:
```
cd third_party/boringssl
mkdir build
cd build
cmake -G "Ninja" -DCMAKE_BUILD_TYPE=Release ..
ninja crypto
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
