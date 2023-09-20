FROM opensuse/leap:15

# Install requirements : update repo and install all requirements
RUN zypper cc -a && \
  zypper addrepo https://cli.github.com/packages/rpm/gh-cli.repo && \
  zypper --gpg-auto-import-keys ref && \
  zypper install -y gcc gcc-c++ systemd \
    git make python3 bash coreutils gh \
    rpm-build rpm-devel rpmlint diffutils patch rpmdevtools \
    cmake ninja pkg-config perl golang \
    gtk4-devel zlib-devel c-ares-devel libnghttp2-devel curl-devel \
    http-parser-devel xxhash-devel mbedtls-devel && \
  zypper cc -a
