FROM opensuse/leap:15

# get from content of https://cli.github.com/packages/rpm/gh-cli.repo
RUN rpm --import 'https://keyserver.ubuntu.com/pks/lookup?op=get&search=0x23F3D4EA75716059'
# Install requirements : update repo and install all requirements
RUN zypper cc -a && \
  zypper addrepo https://cli.github.com/packages/rpm/gh-cli.repo && \
  zypper --gpg-auto-import-keys ref && \
  zypper install -y gcc gcc-c++ systemd \
    git make python3 bash coreutils gh \
    rpm-build rpm-devel rpmlint diffutils patch rpmdevtools \
    cmake ninja pkg-config perl golang \
    gtk3-devel gtk4-devel libqt5-qtbase-devel \
    zlib-devel c-ares-devel libnghttp2-devel curl-devel \
    http-parser-devel mbedtls-devel && \
  zypper cc -a

# Install cmake 3.28.5
RUN curl -L https://github.com/Kitware/CMake/releases/download/v3.28.5/cmake-3.28.5-linux-x86_64.tar.gz | \
  tar -C /usr/local --strip-components=1 --gz -xf - && \
  cmake --version
