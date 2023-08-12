FROM centos:7

# replacing vault mirrors
RUN sed -i 's|^mirrorlist|#mirrorlist|g' /etc/yum.repos.d/CentOS-* && \
    sed -i 's|^#baseurl=http://mirror.centos.org/centos|baseurl=http://d36uatko69830t.cloudfront.net/centos|g' /etc/yum.repos.d/CentOS-*

# Install requirements : update repo and install all requirements
RUN yum clean all && \
  rm -rf /var/cache/yum && rm -rf /var/cache/dnf && \
  yum install -y yum-utils epel-release && \
  yum clean all && \
  rm -rf /var/cache/yum && rm -rf /var/cache/dnf

# replacing epel mirrors
RUN sed -i 's|^metalink=|#metalink=|g' /etc/yum.repos.d/epel* && \
    sed -i 's|^#baseurl=http://download.fedoraproject.org/pub|baseurl=http://dl.fedoraproject.org/pub|g' /etc/yum.repos.d/epel*

# Install requirements : update repo and install all requirements
RUN yum clean all && \
  rm -rf /var/cache/yum && rm -rf /var/cache/dnf && \
  yum-config-manager --add-repo https://cli.github.com/packages/rpm/gh-cli.repo && \
  yum install -y https://packages.endpointdev.com/rhel/7/os/x86_64/endpoint-repo.x86_64.rpm &&\
  yum install -y gcc gcc-c++ \
    git make python3 bash coreutils gh ncurses-libs \
    rpm-build rpm-devel rpmlint diffutils patch rpmdevtools \
    cmake3 ninja-build pkgconfig perl golang \
    gtk3-devel zlib-devel c-ares-devel libnghttp2-devel curl-devel \
    http-parser-devel && \
  yum clean all && \
  rm -rf /var/cache/yum && rm -rf /var/cache/dnf
