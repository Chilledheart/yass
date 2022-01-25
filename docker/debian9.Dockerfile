FROM debian:9

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update -qq && \
  apt-get install -y git build-essential fakeroot devscripts debhelper && \
  apt-get install -y cmake golang libunwind-dev libgtk-3-dev libgtkmm-3.0-dev && \
  apt-get install -y wget make dh-autoreconf libcurl4-gnutls-dev libexpat1-dev gettext libz-dev libssl-dev gettext install-info && \
  rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*
RUN wget https://mirrors.edge.kernel.org/pub/software/scm/git/git-2.35.0.tar.gz && \
  tar -xzf git-2.35.0.tar.gz && \
  cd git-* && \
  make configure && \
  ./configure --prefix=/usr/local && \
  make -j2 all && \
  make install && \
  cd .. && \
  rm -rf git*
