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
    ninja
    cmake
    google-glog
    gflags
    libsodium
    jsoncpp
    wxWidgets-3.2
```
2. Install [HomeBrew] and dependencies...
```
    ninja
    cmake
    glog
    gflags
    libsodium
    jsoncpp
    wxwidgets
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
```
mkdir build
cd build
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=10.9 ..
ninja crypto
```
or
```
mkdir build
cd build
cmake -G "Visual Studio 16 2019" -A x64 -T v142 ..
cmake --build . --config Debug
cmake --build . --config Release
```

[vcpkg]: https://github.com/microsoft/vcpkg
[MacPorts]: https://www.macports.org/install.php
[HomeBrew]: https://brew.sh


## asio