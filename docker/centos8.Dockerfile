FROM centos:8

# Install requirements : update repo and install all requirements
RUN yum clean all && \
  rm -rf /var/cache/yum && rm -rf /var/cache/dnf && \
  sed -i 's/mirrorlist/#mirrorlist/g' /etc/yum.repos.d/CentOS-Linux-* && \
  sed -i 's|#baseurl=http://mirror.centos.org|baseurl=http://vault.centos.org|g' /etc/yum.repos.d/CentOS-Linux-* && \
  yum update -y && \
  yum install -y dnf-plugins-core epel-release && \
  dnf config-manager --set-enabled powertools && \
  yum install -y --allowerasing gcc gcc-c++ libstdc++-static \
    git make python39 bash coreutils \
    rpm-build rpm-devel rpmlint diffutils patch rpmdevtools \
    cmake ninja-build pkg-config perl golang \
    libunwind-devel gtk3-devel gtkmm30-devel && \
  yum clean all && \
  rm -rf /var/cache/yum && rm -rf /var/cache/dnf
