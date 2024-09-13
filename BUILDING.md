# Building Instruments

## Windows (MinGW)

1. Install [Chocolatey][chocolatey] Package Manager.

Run the following command in ADMINISTRATIVE powershell:
```
Set-ExecutionPolicy Bypass -Scope Process -Force; [System.Net.ServicePointManager]::SecurityProtocol = [System.Net.ServicePointManager]::SecurityProtocol -bor 3072; iex ((New-Object System.Net.WebClient).DownloadString('https://community.chocolatey.org/install.ps1'))
```

2. Install required tools via [Chocolatey].

Run the following command in ADMINISTRATIVE shell:
```
choco install 7zip.install
choco install git.install
choco install strawberryperl
choco install cmake.portable
choco install ninja
choco install golang
```

3. Open `Git Bash` from Start Menu and run

```
git clone https://github.com/Chilledheart/yass
cd yass
./scripts/build-mingw.sh
```

## Windows (MSYS2)

1. Download and run MSYS2 installer from [MSYS2 site][msys2].
2. Install required tools

Run `MSYS2 CLANG64` in Start Menu:
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

Notes: you might need to get `GOROOT` manually after install `mingw-w64-clang-x86_64-go`
package by running:
```
export GOROOT=/clang64/lib/go
export GOPATH=/clang64
```

3. Compiling the program.

Run `MSYS2 CLANG64` in Start Menu:
```
git clone https://github.com/Chilledheart/yass
cd yass
mkdir build
cd build
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -DGUI=on ..
ninja yass
```

## Windows (MSVC)

1. Make sure you have [Git for Windows][gitforwindows] installed.
2. Make sure you have [Perl], [CMake] (3.16 or later), [Ninja], [Golang] and [NASM] installed and put them in `PATH`.

  * A recent version of Perl is required.
    On Windows, [Active State Perl](http://www.activestate.com/activeperl/) has been reported to work, as has MSYS Perl.
    [Strawberry Perl](http://strawberryperl.com/) also works but it adds [GCC] to `PATH`,
    which can confuse some build tools when identifying the compiler
    (removing `C:\Strawberry\c\bin` from `PATH` should resolve any problems).

3. Make sure you use [Visual Studio][visualstudio] 2019 (or 2022).

  * Make sure you have `Visual Studio with C++` selected from download page.

4. Make sure you have `clang-cl` in `PATH`:

  * Download and Run [LLVM installer][llvm-win64] from GitHub Binary download page.

  * Choose `Add LLVM to System Path`.

Notes: please make sure you have [LLVM][llvm-win64] (17.0 or above).

5. Compile the program with Release configuration.

Run `x64 Native Tools Command Prompt for VS 2019 (or 2022)` in Start Menu:
```
git clone https://github.com/Chilledheart/yass
cd yass
mkdir build
cd build
set CC=clang-cl
set CXX=clang-cl
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -DGUI=on ..
ninja yass
```

## macOS

1. Make sure you have both of [Xcode] and [Homebrew] installed:

Run in `Terminal`:
```
xcode-select -s /Applications/Xcode.app
xcode-select --install
xcodebuild -runFirstLaunch
```

Run in `Terminal`:
```
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
```
2. Install the required build tools via homebrew

Run in `Terminal`:
```
brew install ninja cmake go p7zip
```

3. Compile the program with Release configuration.
```
git clone https://github.com/Chilledheart/yass
cd yass
mkdir build
cd build
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -DGUI=on ..
ninja yass
```

## Debian/Ubuntu
1. Install GNU C++ Compiler:

Run in `Console`:
```
sudo apt-get install -y build-essential git
```

2. Install required dependencies:

Run in `Console`:
```
sudo apt-get install -y cmake ninja-build pkg-config perl gettext libgtk-3-dev golang
```

Notes: please make sure you have [GCC] (7.1 or above) and [CMake] (3.16 or above).

You might want to give these APT/PPA sites a look if the requirements are not meet:

* [PPA for Ubuntu Toolchain](https://launchpad.net/~ubuntu-toolchain-r/+archive/ubuntu/test)
* [Kitware CMake](https://apt.kitware.com/)

3. Compile the program with Release configuration.

Run in `Console`:
```
git clone https://github.com/Chilledheart/yass
cd yass
mkdir build
cd build
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -DGUI=on -DENABLE_GOLD=off -DUSE_LIBCXX=off -DUSE_GTK4=off ..
ninja yass
```

## Fedora/RHEL/CentOS/AlmaLinux/Rocky Linux
1. Install GNU C++ Compiler:

Run in `Console`:
```
sudo yum install -y gcc gcc-c++ make python bash coreutils diffutils patch git
```
2. Install required dependencies:

Run in `Console`:
```
sudo yum install -y cmake ninja-build pkg-config perl gtk3-devel gettext golang
```
or (for RHEL/CentOS users)
```
sudo yum install -y cmake3 ninja-build pkg-config perl gtk3-devel gettext golang
```

Notes: please make sure you have [GCC] (7.1 or above) and [CMake] (3.16 or above).

You might want to enable CodeReady (for RHEL), PowerTools (for CentOS) and EPEL repo before above commands:

* CodeReady (for RHEL 7):
```
subscription-manager repos --enable rhel-*-optional-rpms \
                           --enable rhel-*-extras-rpms \
                           --enable rhel-ha-for-rhel-*-server-rpms
```

* CodeReady (for RHEL 8): `subscription-manager repos --enable codeready-builder-for-rhel-8-x86_64-rpms`
* PowerTools (for CentOS 8): `yum-config-manager --enable powertools`
* [EPEL] 8: `yum install -y https://dl.fedoraproject.org/pub/epel/epel-release-latest-8.noarch.rpm`

* CodeReady (for RHEL 9): `subscription-manager repos --enable codeready-builder-for-rhel-9-x86_64-rpms`
* CRB (for CentOS 9): `yum-config-manager --enable crb`
* [EPEL] 9: `yum install -y https://dl.fedoraproject.org/pub/epel/epel-release-latest-9.noarch.rpm`

3. Compile the program with Release configuration.

Run in `Console`:
```
git clone https://github.com/Chilledheart/yass
cd yass
mkdir build
cd build
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -DGUI=on -DENABLE_GOLD=off -DUSE_LIBCXX=off -DUSE_GTK4=off ..
ninja yass
```

## FreeBSD
1. Install Clang Compiler:

It is impossible to upgrade system compiler without upgrading OS,
so you have to install latest [Clang] (not required since FreeBSD 14.1):

Run in `Console`:
```
pkg install llvm18-lite
```

2. Install required dependencies:

Run in `Console`:
```
pkg install -y git cmake ninja pkgconf perl5 gettext gtk3 go
```

3. Compile the program with Release configuration.

Run in `Console`:
```
git clone https://github.com/Chilledheart/yass
cd yass
export PATH="/usr/local/llvm18/bin:$PATH"
export CC=clang
export CXX=clang++
mkdir build
cd build
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -DGUI=on -DUSE_GTK4=off ..
ninja yass
```

[chocolatey]: https://chocolatey.org/install#individual
[gitforwindows]: https://gitforwindows.org/
[visualstudio]: https://visualstudio.microsoft.com/downloads/
[Perl]: https://www.perl.org/get.html
[Clang]: https://clang.llvm.org/
[CMake]: https://cmake.org/download/
[Ninja]: https://ninja-build.org/
[Golang]: https://go.dev/dl/
[GCC]: https://gcc.gnu.org/
[NASM]: https://www.nasm.us/
[Xcode]: https://apps.apple.com/us/app/xcode/id497799835?mt=12
[HomeBrew]: https://docs.brew.sh/Installation
[llvm-win64]: https://github.com/llvm/llvm-project/releases/download/llvmorg-18.1.8/LLVM-18.1.8-win64.exe
[msys2]: https://www.msys2.org/
[EPEL]: https://docs.fedoraproject.org/en-US/epel
