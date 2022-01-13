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
    ninja cmake python27 go

    libsodium jsoncpp jpeg tiff libpng zlib libiconv expat +universal
```
2. Install [HomeBrew] and dependencies...
```
    ninja    cmake    libsodium    jsoncpp    wxwidgets
```

3. use script to build Release App under `build` directory.
```
scripts/build.py
```

## Windows/vcpkg

1. Make sure you use Visual Studio 2019 or later to get vcpkg work.

2. Run [vcpkg][vcpkg] to install required dependencies.
```
vcpkg install libsodium:x64-windows-static wxwidgets:x64-windows-static
```
or
```
vcpkg install libsodium:arm64-windows-static wxwidgets:arm64-windows-static
```
or
```
vcpkg install libsodium:x86-windows-static wxwidgets:x86-windows-static
```
3. use script to build Release App under `build` directory.
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

## wxWidgets for macOS
```
git submodule update --init --recursive
mkdir build
cd build
CFLAGS="-I/opt/local/include" \
CXXFLAGS="-I/opt/local/include" \
LDFLAGS="-L/opt/local/lib" \
LIBS="-L/opt/local/lib -ljpeg -ltiff -lexpat -liconv" \
  ../configure --enable-universal_binary=x86_64,arm64 \
  --with-cxx=14 \
  --with-libpng \
  --with-libjpeg \
  --with-libtiff \
  --with-zlib \
  --with-expat \
  --with-macosx-version-min=10.10 \
  --with-osx \
  --prefix /opt/wxWidgets
make -j $(sysctl -n hw.ncpu)
sudo make install
cd ..
```


[vcpkg]: https://github.com/microsoft/vcpkg
[MacPorts]: https://www.macports.org/install.php
[HomeBrew]: https://brew.sh
