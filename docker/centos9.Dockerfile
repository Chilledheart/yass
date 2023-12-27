FROM rockylinux:9

# Install requirements : update repo and install all requirements
RUN yum clean all && \
  rm -rf /var/cache/yum && rm -rf /var/cache/dnf && \
  yum install -y yum-utils epel-release && \
  yum-config-manager --add-repo https://cli.github.com/packages/rpm/gh-cli.repo && \
  yum-config-manager --enable crb && \
  yum install -y --allowerasing gcc gcc-c++ \
    git make python39 bash coreutils gh \
    rpm-build rpm-devel rpmlint diffutils patch rpmdevtools \
    cmake ninja-build pkg-config golang \
    gtk4-devel zlib-devel c-ares-devel libnghttp2-devel curl-devel \
    http-parser-devel json-devel mbedtls-devel && \
  yum clean all && \
  rm -rf /var/cache/yum && rm -rf /var/cache/dnf
