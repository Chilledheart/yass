FROM fedora:37

# Install requirements : update repo and install all requirements
RUN yum clean all && \
  rm -rf /var/cache/yum && rm -rf /var/cache/dnf && \
  yum install -y yum-utils && \
  yum-config-manager --add-repo https://cli.github.com/packages/rpm/gh-cli.repo && \
  yum install -y gcc gcc-c++ \
    git make python3 bash coreutils gh \
    rpm-build rpm-devel rpmlint diffutils patch rpmdevtools \
    cmake ninja-build pkg-config perl golang \
    gtk4-devel zlib-devel c-ares-devel libnghttp2-devel curl-devel \
    http-parser-devel xxhash-devel json-devel && \
  yum clean all && \
  rm -rf /var/cache/yum && rm -rf /var/cache/dnf

# Install toolchain
RUN yum clean all && \
  rm -rf /var/cache/yum && rm -rf /var/cache/dnf && \
  yum install -y clang lld && \
  yum clean all && \
  rm -rf /var/cache/yum && rm -rf /var/cache/dnf
