# Building Instruments

## Windows

1. Make sure you use [Visual Studio][visualstudio] 2019 (or 2022).

  * Make sure you have `Visual Studio with C++` selected from download page.

2. Make sure you have Perl, [CMake] (3.12 or later), [Ninja], [Golang] and [NASM] installed and put them in `PATH`.

  * A recent version of Perl is required.
    On Windows, [Active State Perl](http://www.activestate.com/activeperl/) has been reported to work, as has MSYS Perl.
    [Strawberry Perl](http://strawberryperl.com/) also works but it adds [GCC] to `PATH`,
    which can confuse some build tools when identifying the compiler
    (removing `C:\Strawberry\c\bin` from `PATH` should resolve any problems).

3. Make sure you have `clang-cl` in `PATH`:

  * Download and Run [LLVM installer][llvm-win64] from GitHub Binary download page.

  * Choose `Add LLVM to System Path`.

4. Run `x64 Native Tools Command Prompt for VS 2019 (or 2022)` in Start Menu.

5. Compile the program with Release configuration.

```
mkdir build
cd build
set CC=clang-cl
set CXX=clang-cl
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -DGUI=on ..
ninja yass
```

## Mingw-w64

1. Install MSYS2 Package from [official site][msys2].
2. Run MSYS2 CLANG64 in Start Menu.
3. Install required tools
```
pacman -S mingw-w64-clang-x86_64-clang \
          mingw-w64-clang-x86_64-gcc-compat \
          mingw-w64-clang-x86_64-perl \
          mingw-w64-clang-x86_64-go \
          mingw-w64-clang-x86_64-cmake \
          mingw-w64-clang-x86_64-ninja \
          mingw-w64-clang-x86_64-nasm \
          git
```

Notes: you might need to get `GOROOT` manually after install `mingw-w64-x86_64-go`
package by running:
```
export GOROOT=/clang64/lib/go
export GOPATH=/clang64
```

4. Compiling the program.
```
mkdir build
cd build
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -DGUI=on ..
ninja yass
```
5. Enjoy
6. Same way with 32-bit build by replacing `clang64` with `clang32` and `x86_64` with `i686`.

## macOS/Mac OS X

1. Make sure you have [Xcode Command Line Tools][xcode-commandline] installed ([Xcode] if possible):

Run in Terminal:
```
xcode-select --install
```
2. Install the required build tools...

(for [Homebrew] users)

Run in Terminal: ``brew install ninja cmake go p7zip``

(for [MacPorts] users)

Run in Terminal: ``sudo port install ninja cmake go p7zip``

(for people who don't use [MacPorts] or [Homebrew])

1. [CMake]

Run in Terminal:
```
cd $TMPDIR
curl -L -O https://github.com/Kitware/CMake/releases/download/v3.25.1/cmake-3.25.1-macos10.10-universal.dmg
hdiutil attach cmake-3.25.1-macos10.10-universal.dmg
ditto /Volumes/cmake-3.25.1-macos10.10-universal/CMake.app /Applications/CMake.app
hdiutil detach /Volumes/cmake-3.25.1-macos10.10-universal
echo 'export PATH="/Applications/CMake.app/Contents/bin:${PATH}"' >> .zprofile
export PATH="/Applications/CMake.app/Contents/bin:${PATH}"
```
2. [Ninja]

Run in Terminal:
```
cd $TMPDIR
curl -L -O https://github.com/ninja-build/ninja/releases/download/v1.11.1/ninja-mac.zip
unzip ninja-mac.zip
sudo install -m 755 ninja /usr/local/bin/ninja
```
3. [Golang]

Run in Terminal:
```
cd $TMPDIR
# Change to https://go.dev/dl/go1.18.10.darwin-arm64.tar.gz if you are using Apple Silicon
curl -L -O https://go.dev/dl/go1.18.10.darwin-amd64.tar.gz
sudo tar -C /usr/local -xzf go1.18.10.darwin-amd64.tar.gz
echo 'export PATH="/usr/local/go/bin:${PATH}"' >> .zprofile
export PATH="/usr/local/go/bin:${PATH}"
```

3. Compile the program with Release configuration.
```
mkdir build
cd build
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -DGUI=on ..
ninja yass
```


## Debian/Ubuntu
1. Install GNU C++ Compiler:
```
sudo apt-get install -y build-essential
```
2. Install required dependencies:
```
sudo apt-get install -y \
    cmake \
    ninja-build \
    pkg-config \
    perl \
    libgtk-3-dev
```

Notes: please make sure you have [GCC] (11.0 or above) or [Clang] (12.0 or above) and [CMake] (3.12 or above).
  You might want to give these APT/PPA sites a look if the requirements are not meet:

* [PPA for Ubuntu Toolchain](https://launchpad.net/~ubuntu-toolchain-r/+archive/ubuntu/test)
* [Kitware CMake](https://apt.kitware.com/)

3. Install Golang manually:
```
cd /tmp
wget https://go.dev/dl/go1.18.10.linux-amd64.tar.gz
sudo tar -C /usr/local -xzf go1.18.10.linux-amd64.tar.gz
echo 'export PATH="/usr/local/go/bin:${PATH}"' >> ~/.bashrc
export PATH="/usr/local/go/bin:${PATH}"
```
4. Compile the program with Release configuration.
```
mkdir build
cd build
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -DGUI=on ..
ninja yass
```

## Fedora/RHEL/CentOS
1. Install GNU C++ Compiler:
```
sudo yum install -y gcc gcc-c++ libatomic-static \
    make python bash coreutils diffutils patch
```
2. Install required dependencies:
```
sudo yum install -y \
    cmake \
    ninja-build \
    pkg-config \
    perl \
    gtk3-devel \
    golang
```

Notes: please make sure you have [GCC] (11.0 or above) or [Clang] (12.0 or above) and [CMake] (3.12 or above).
  You might want to enable CodeReady (for RHEL), PowerTools (for CentOS) and EPEL repo before above commands:

* CodeReady (for RHEL): `subscription-manager repos --enable codeready-builder-for-rhel-8-x86_64-rpms`
* PowerTools (for CentOS): `yum install -y dnf-plugins-core && dnf config-manager --set-enabled PowerTools`
* EPEL: `yum install -y https://dl.fedoraproject.org/pub/epel/epel-release-latest-8.noarch.rpm`

3. Compile the program with Release configuration.
```
mkdir build
cd build
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -DGUI=on ..
ninja yass
```

## FreeBSD
1. Install Clang Compiler (Optional):
Use system compiler otherwise you might need to install it by manually:
```
pkg install llvm13
```
Notes: please make sure you have [Clang] (12.0 or above) and [CMake] (3.12 or above).

2. Install required dependencies:
```
pkg install -y \
    cmake \
    ninja \
    pkgconf \
    perl5 \
    gtk3 \
    go
```

Notes: please install src.txz package of system otherwise you might need to create symbolics of unwind.h like below:
Notes 2: not required since FreeBSD 13.1
```
ln -sf /usr/include/c++/v1/unwind.h /usr/include/unwind.h
ln -sf /usr/include/c++/v1/unwind-arm.h /usr/include/unwind-arm.h
ln -sf /usr/include/c++/v1/unwind-itanium.h /usr/include/unwind-itanium.h
```

3. Compile the program with Release configuration.
```
mkdir build
cd build
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -DGUI=on ..
ninja yass
```

## Windows/Packaging

Make sure you have [Golang] installed on your system.

Run in `x64 Native Tools Command Prompt for VS 2019 (or 2022)` in Start Menu:
```
cd tools
go build
cd ..
./tools/build
```

## macOS/Packaging

Make sure you have [Golang] installed on your system.

Run in `Terminal`:
```
cd tools
go build
cd ..
./tools/build
```

## Debian/Packaging

1. Install Packaging Tools
```
sudo apt-get install -y git build-essential fakeroot devscripts debhelper
```

2. Install [Clang] and put its binaries in `PATH`

3. Generate Packages
```
export CC=clang
export CXX=clang++
./scripts/build-deb.sh
```

## Fedora/Packaging

1. Install Packaging Tools
```
sudo yum install -y gcc gcc-c++ libatomic-static \
    rpm-build rpm-devel rpmlint make python bash coreutils diffutils patch rpmdevtools
```

2. Install [Clang] and put its binaries in `PATH`

3. Generate Packages under current parent directory
```
export CC=clang
export CXX=clang++
./scripts/build-rpm.sh
```

## FreeBSD/Packaging

Make sure you have [Golang] installed on your system.

Run in Terminal:
```
cd tools
go build
cd ..
./tools/build
```

[visualstudio]: https://visualstudio.microsoft.com/downloads/
[Perl]: https://www.perl.org/get.html
[Clang]: https://clang.llvm.org/
[CMake]: https://cmake.org/download/
[Ninja]: https://ninja-build.org/
[Golang]: https://go.dev/dl/
[GCC]: https://gcc.gnu.org/
[NASM]: https://www.nasm.us/
[xcode-commandline]: https://developer.apple.com/download/more/
[Xcode]: https://apps.apple.com/us/app/xcode/id497799835?mt=12
[MacPorts]: https://www.macports.org/install.php
[HomeBrew]: https://docs.brew.sh/Installation
[python]: https://www.python.org/downloads/
[llvm-win64]: https://github.com/llvm/llvm-project/releases/download/llvmorg-15.0.6/LLVM-15.0.6-win64.exe
[msys2]: https://www.msys2.org/
