FROM centos:7

# replacing vault mirrors
RUN sed -i 's|^mirrorlist|#mirrorlist|g' /etc/yum.repos.d/CentOS-* && \
    sed -i 's|^#baseurl=http://mirror.centos.org/centos|baseurl=http://d36uatko69830t.cloudfront.net/centos|g' /etc/yum.repos.d/CentOS-*

# Install requirements : update repo and install all requirements
RUN ulimit -n 1024 && yum clean all && \
  rm -rf /var/cache/yum && rm -rf /var/cache/dnf && \
  yum install -y epel-release && \
  yum clean all && \
  rm -rf /var/cache/yum && rm -rf /var/cache/dnf

# replacing epel mirrors
RUN sed -i 's|^metalink=|#metalink=|g' /etc/yum.repos.d/epel* && \
    sed -i 's|^#baseurl=http://download.fedoraproject.org/pub|baseurl=http://dl.fedoraproject.org/pub|g' /etc/yum.repos.d/epel*

# Install requirements : update repo and install all requirements
RUN ulimit -n 1024 && yum clean all && \
  rm -rf /var/cache/yum && rm -rf /var/cache/dnf && \
  yum install -y yum-utils && \
  yum-config-manager --add-repo https://cli.github.com/packages/rpm/gh-cli.repo && \
  yum install -y https://packages.endpointdev.com/rhel/7/os/x86_64/endpoint-repo.x86_64.rpm &&\
  yum install -y gcc gcc-c++ \
    git make python3 bash coreutils gh \
    rpm-build rpm-devel rpmlint diffutils patch rpmdevtools \
    cmake3 ninja-build pkgconfig perl golang \
    gtk3-devel qt5-qtbase-devel zlib-devel curl-devel && \
  yum clean all && \
  rm -rf /var/cache/yum && rm -rf /var/cache/dnf

# Install cmake 3.28.6
RUN curl -L https://github.com/Kitware/CMake/releases/download/v3.28.6/cmake-3.28.6-linux-x86_64.tar.gz | \
  tar -C /usr/local --strip-components=1 --gz -xf - && \
  ln -sf cmake /usr/local/bin/cmake3 && \
  cmake --version && \
  cmake3 --version

RUN sed -i 's#Q_DECL_CONSTEXPR inline QFlags(Zero = Q_NULLPTR) Q_DECL_NOTHROW : i(0) {}#Q_DECL_CONSTEXPR inline QFlags() Q_DECL_NOTHROW : i(0) {}\n    Q_DECL_CONSTEXPR inline QFlags(Zero) Q_DECL_NOTHROW : i(0) {}#g' /usr/include/qt5/QtCore/qflags.h
