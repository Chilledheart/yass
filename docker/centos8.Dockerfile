FROM centos:8

# centos 8 is eol, replacing vault mirrors
RUN sed -i 's/mirrorlist/#mirrorlist/g' /etc/yum.repos.d/CentOS-Linux-* && \
    sed -i 's|#baseurl=http://mirror.centos.org|baseurl=http://d36uatko69830t.cloudfront.net|g' /etc/yum.repos.d/CentOS-Linux-*
# Install requirements : update repo and install all requirements
RUN yum clean all && \
  rm -rf /var/cache/yum && rm -rf /var/cache/dnf && \
  yum install -y yum-utils epel-release && \
  yum-config-manager --add-repo https://cli.github.com/packages/rpm/gh-cli.repo && \
  yum-config-manager --enable powertools && \
  yum install -y --allowerasing gcc gcc-c++ libatomic-static \
    git make python39 bash coreutils gh ncurses-compat-libs \
    rpm-build rpm-devel rpmlint diffutils patch rpmdevtools \
    cmake ninja-build pkg-config perl golang \
    gtk3-devel && \
  yum clean all && \
  rm -rf /var/cache/yum && rm -rf /var/cache/dnf
