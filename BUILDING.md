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
    libjemalloc-dev \
    libboost-all-dev \
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
    boost
    ninja
    cmake
    google-glog
    gflags
    libsodium
    jsoncpp
    jemalloc
    wxWidgets-3.2
```
2. Install Homebrew and dependencies...
```
    boost
    ninja
    cmake
    glog
    gflags
    libsodium
    jsoncpp
    jemalloc
    wxwidgets
```

3. use script to build Release App under `build` directory.
```
scripts/build.py
```

## Windows/vcpkg

1. Make sure you use Visual Studio 2015 Update 3 or later to get vcpkg work.

2. Run [vcpkg][vcpkg] to install required dependencies.
```
vcpkg install boost-asio:x86-windows-static-xp boost-filesystem:x86-windows-static-xp boost-system:x86-windows-static-xp glog:x86-windows-static-xp gflags:x86-windows-static-xp libsodium:x86-windows-static-xp jsoncpp:x86-windows-static-xp wxwidgets:x86-windows-static-xp
```
3. use script to build Release App under `build` directory.
```
scripts/build.py
```

[vcpkg]: https://github.com/microsoft/vcpkg
[MacPorts]: https://www.macports.org/install.php
