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
    libwxgtk3.0-dev (optional)

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

    google-glog +universal gflags +universal libsodium +universal jsoncpp +universal
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
3. use script to build Release App under `build` directory.
```
scripts/build.py
```

## boringssl

For boringssl, you should only need crypto target.

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

(for macOS)
```
set(CMAKE_ASM_FLAGS "-mmacosx-version-min=10.10 ${CMAKE_ASM_FLAGS}")
```

Run these commands to build crypto target.
```
mkdir build
cd build
git clean -xfd
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=10.10 \
  -DCMAKE_OSX_ARCHITECTURES="x86_64" ..
ninja crypto
cp -fv crypto/libcrypto.a ../x64-libcrypto.a
```

Building a universal target
```

git clean -xfd
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=10.10 \
  -DCMAKE_OSX_ARCHITECTURES="arm64" ..
ninja crypto
cp -fv crypto/libcrypto.a ../arm64-libcrypto.a
lipo -create ../arm64-libcrypto.a ../x64-libcrypto.a -output crypto/libcrypto.a
```
or
```
mkdir build
cd build
cmake -G "Visual Studio 16 2019" -A x64 -T v142 ..
cmake --build . --config Debug
cmake --build . --config Release
```
## wxWidgets for macports
```
git submodule update --init
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