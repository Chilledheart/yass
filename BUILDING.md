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
    libgoogle-glog-dev \
    libgflags-dev \
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

    google-glog +universal gflags +universal libsodium +universal jsoncpp +universal jpeg +universal tiff +universal libpng +universal zlib +universal libiconv +universal expat +universal
```
2. Install [HomeBrew] and dependencies...
```
    ninja    cmake    glog    gflags    libsodium    jsoncpp    wxwidgets
```

3. use script to build Release App under `build` directory.
```
scripts/build.py
```

## Windows/vcpkg

1. Make sure you use Visual Studio 2019 or later to get vcpkg work.

2. Run [vcpkg][vcpkg] to install required dependencies.
```
vcpkg install glog:x64-windows-static gflags:x64-windows-static libsodium:x64-windows-static jsoncpp:x64-windows-static wxwidgets:x64-windows-static
```
or
```
vcpkg install expat:arm64-windows-static libjpeg-turbo:arm64-windows-static liblzma:arm64-windows-static libpng:arm64-windows-static tiff:arm64-windows-static zlib:arm64-windows-static glog:arm64-windows-static gflags:arm64-windows-static libsodium:arm64-windows-static jsoncpp:arm64-windows-static wxwidgets:arm64-windows-static
```
or
```
vcpkg install expat:x86-windows-static libjpeg-turbo:x86-windows-static liblzma:x86-windows-static libpng:x86-windows-static tiff:x86-windows-static zlib:x86-windows-static glog:x86-windows-static gflags:x86-windows-static libsodium:x86-windows-static jsoncpp:x86-windows-static wxwidgets:x86-windows-static
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

git clean -xfd
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
```

(for Linux)
```
wget https://golang.org/dl/go1.16.linux-amd64.tar.gz
sudo tar -C /usr/local -xzf go1.16.linux-amd64.tar.gz
echo 'export PATH=$PATH:/usr/local/go/bin' >> ~/.bashrc
sudo apt-get install libunwind-dev nasm
```
Relogin and run:
```
go version
git clean -xfd
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
git clean -xfd
mkdir build
cd build
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=10.10 \
  -DCMAKE_OSX_ARCHITECTURES="x86_64" ..
ninja crypto
cp -fv crypto/libcrypto.a ../x64-libcrypto.a
```
(and then arm64 slice)
```
git clean -xfd
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=10.10 \
  -DCMAKE_OSX_ARCHITECTURES="arm64" ..
ninja crypto
cp -fv crypto/libcrypto.a ../arm64-libcrypto.a
lipo -create ../arm64-libcrypto.a ../x64-libcrypto.a -output crypto/libcrypto.a
lipo -info crypto/libcrypto.a
cd ..
```
Built universal target at ``crypto/libcrypto.a``

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
make -j8
sudo make install
cd ..
```


[vcpkg]: https://github.com/microsoft/vcpkg
[MacPorts]: https://www.macports.org/install.php
[HomeBrew]: https://brew.sh


## asio
