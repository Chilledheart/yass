# Building Instruments

## Windows

1. Make sure you use [Visual Studio][vs] 2017 or later.
Make sure you have `Visual Studio with C++` selected from download page.

2. Make sure you have [Perl], [CMake] (3.8 or later), [Ninja], [Golang] and [NASM] installed and put them in `PATH`.

  * A recent version of Perl is required.
    On Windows, [Active State Perl](http://www.activestate.com/activeperl/) has been reported to work, as has MSYS Perl.
    [Strawberry Perl](http://strawberryperl.com/) also works but it adds GCC to `PATH`,
    which can confuse some build tools when identifying the compiler
    (removing `C:\Strawberry\c\bin` from `PATH` should resolve any problems).

3. Run Developer Command Line from Visual Studio subdirectory in Start Menu.

4. Make sure you have clang-cl in PATH:

Download and run LLVM installer from GitHub Binary download page.
- https://github.com/llvm/llvm-project/releases/download/llvmorg-13.0.0/LLVM-13.0.0-win64.exe

Make sure you choose "Add LLVM to System Path".

5. Compile the program with default configuration.
Run in Console:
```
mkdir build
cd build
set CC=clang-cl
set CXX=clang-cl
cmake -G Ninja -DGUI=on ..
ninja yass
```

## macOS/MacOS X

1. Make sure you have [Xcode Command Line Tools][xcode-commandline] installed ([Xcode] if possible):
Run in Terminal:
```
xcode-select --install
```
2. Install the rest build tools...

(for people who don't use [MacPorts] or [Homebrew])

1. [CMake]
Run in Terminal:
```
curl -L -O https://github.com/Kitware/CMake/releases/download/v3.22.2/cmake-3.22.2-macos-universal.dmg
hdiutil attach cmake-3.22.2-macos-universal.dmg
ditto /Volumes/cmake-3.22.2-macos-universal/CMake.app /Applications/CMake.app
hdiutil detach /Volumes/cmake-3.22.2-macos-universal
echo 'export PATH="/Applications/CMake.app/Contents/bin:${PATH}"' >> .zprofile
export PATH="/Applications/CMake.app/Contents/bin:${PATH}"
```
2. [Ninja]
Run in Terminal:
```
cd $TMPDIR
curl -L -O https://github.com/ninja-build/ninja/releases/download/v1.10.2/ninja-mac.zip
unzip ninja-mac.zip
install -m 755 ninja /usr/local/bin/ninja
```
3. Golang
Run in Terminal:
```
cd $TMPDIR
# Change to https://go.dev/dl/go1.16.13.darwin-arm64.tar.gz if you are using Apple Silicon-based Mac
curl -L -O https://go.dev/dl/go1.16.13.darwin-amd64.tar.gz
tar -C /usr/local -xf go1.16.13.darwin-amd64.tar.gz
echo 'export PATH="/usr/local/go/bin:${PATH}"' >> .zprofile
export PATH="/usr/local/go/bin:${PATH}"
```

(for [Homebrew] users)

```
    brew install ninja cmake go
```

(for [MacPorts] users)

```
    sudo port install ninja cmake go
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
echo 'export PATH="/usr/local/go/bin:${PATH}"' >> ~/.bashrc
export PATH="/usr/local/go/bin:${PATH}"
```
4. Compile the program with default configuration.
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
