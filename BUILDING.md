# Building Instruments

## Windows

1. Make sure you use [Visual Studio][vs] 2017 or later.
Make sure you have `Visual Studio with C++` selected from download page.

2. Make sure you have [Perl], [CMake] (3.8 or later), [Ninja], [Golang] and [NASM] installed.

  * A recent version of Perl is required.
    On Windows, [Active State Perl](http://www.activestate.com/activeperl/) has been reported to work, as has MSYS Perl.
    [Strawberry Perl](http://strawberryperl.com/) also works but it adds GCC to `PATH`,
    which can confuse some build tools when identifying the compiler
    (removing `C:\Strawberry\c\bin` from `PATH` should resolve any problems).

3. Run Developer Command Line from Visual Studio subdirectory in Start Menu.

4. Compile the program with default configuration.
```
mkdir build
cd build
cmake -G Ninja -DGUI=on ..
ninja yass
```

## macOS/MacOS X

1. Make sure you have [Xcode Command Line Tools][xcode-commandline] installed ([Xcode] if possible):
```
xcode-select --install
```
2. Install the rest build tools...
(for [MacPorts])
```
    sudo port install ninja cmake go
```
(for [Homebrew])
```
    brew install ninja cmake go
```

3. Compile the program with default configuration.
```
mkdir build
cd build
cmake -G Ninja -DGUI=on ..
ninja yass
```


## Debian/Ubuntu
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
3. Install golang manually:
```
wget https://go.dev/dl/go1.16.13.linux-amd64.tar.gz
sudo tar -C /usr/local -xzf go1.16.13.linux-amd64.tar.gz
echo 'export PATH=$PATH:/usr/local/go/bin' >> ~/.bashrc
```
4. Logout and login
5. Compile the program with default configuration.
```
mkdir build
cd build
cmake -G Ninja -DGUI=on ..
ninja yass
```

Notes: please make sure you have GCC (6.1 or above) and CMake (3.8 or above).
You might want to give these APT/PPA sites a look if the requirements are not meet:
- PPA for Ubuntu Toolchain: https://launchpad.net/~ubuntu-toolchain-r/+archive/ubuntu/test
- Kitware CMake: https://apt.kitware.com/

## Fedora/RHEL/CentOS
1. Install GNU C++ Compiler:
```
sudo dnf install gcc make python bash coreutils diffutils patch
```
2. Install below dependencies:
```
sudo yum install -y \
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
cmake -G Ninja -DGUI=on ..
ninja yass
```

Notes: please make sure you have GCC (6.1 or above) and CMake (3.8 or above).
You might want to enable CodeReady (for RHEL), PowerTools (for CentOS) and EPEL repo for your distribution:
- CodeReady (for RHEL): `subscription-manager repos --enable codeready-builder-for-rhel-8-x86_64-rpms`
- PowerTools (for CentOS): `yum install -y dnf-plugins-core && dnf config-manager --set-enabled PowerTools`
- EPEL: `yum install -y https://dl.fedoraproject.org/pub/epel/epel-release-latest-8.noarch.rpm`


## General/Packaging

There are scripts to Windows and macOS/MacOS X packages.

Make sure you have [Python 3][py3] installed on your system.

On Windows:
```
python ./scripts/build.py
```

On Mac
```
./scripts/build.py
```

## Debian/Packaging

There are scripts to packaging debs.

1. Install Packaging Tools
```
sudo apt-get install build-essential fakeroot devscripts
```

2. Generate Packages
```
sudo apt-get install build-essential fakeroot devscripts
./scripts/build-deb.sh
```

## Fedora/Packaging

There are scripts to packaging rpms.

1. Install Packaging Tools
```
sudo dnf install gcc rpm-build rpm-devel rpmlint make python bash coreutils diffutils patch rpmdevtools
```

2. Generate Packages
```
./scripts/build-rpm.sh
```

[vs]: https://visualstudio.microsoft.com/downloads/
[Perl]: https://www.perl.org/get.html
[CMake]: https://cmake.org/download/
[Ninja]: https://ninja-build.org/
[Golang]: https://go.dev/dl/
[NASM]: https://www.nasm.us/
[xcode-commandline]: https://developer.apple.com/download/more/
[Xcode]: https://apps.apple.com/us/app/xcode/id497799835?mt=12
[vcpkg]: https://github.com/microsoft/vcpkg
[MacPorts]: https://www.macports.org/install.php
[HomeBrew]: https://brew.sh
[py3]: https://www.python.org/downloads/
