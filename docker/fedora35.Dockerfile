FROM fedora:35

# Install requirements : update repo and install all requirements
RUN yum clean all && \
  rm -rf /var/cache/yum && rm -rf /var/cache/dnf && \
  yum install -y dnf-plugins-core && \
  dnf config-manager --add-repo https://cli.github.com/packages/rpm/gh-cli.repo && \
  yum install -y gcc gcc-c++ libatomic-static \
    git make python39 bash coreutils gh ncurses-compat-libs \
    rpm-build rpm-devel rpmlint diffutils patch rpmdevtools \
    cmake ninja-build pkg-config perl golang \
    gtk3-devel && \
  yum clean all && \
  rm -rf /var/cache/yum && rm -rf /var/cache/dnf
