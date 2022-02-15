FROM ubuntu:16.04

ENV DEBIAN_FRONTEND=noninteractive

RUN sed -i 's/http:\/\/archive\.ubuntu\.com\/ubuntu\//http:\/\/azure\.archive\.ubuntu\.com\/ubuntu\//g' /etc/apt/sources.list && \
    sed -i 's/http:\/\/[a-z][a-z]\.archive\.ubuntu\.com\/ubuntu\//http:\/\/azure\.archive\.ubuntu\.com\/ubuntu\//g' /etc/apt/sources.list

RUN apt-get update -qq && \
  apt-get install -y curl && \
  curl -fsSL https://cli.github.com/packages/githubcli-archive-keyring.gpg | dd of=/etc/apt/trusted.gpg.d/githubcli-archive-keyring.gpg && \
  echo "deb [arch=$(dpkg --print-architecture) signed-by=/etc/apt/trusted.gpg.d/githubcli-archive-keyring.gpg] https://cli.github.com/packages stable main" | tee /etc/apt/sources.list.d/github-cli.list > /dev/null && \
  apt-get install -y software-properties-common apt-transport-https wget && \
  add-apt-repository ppa:ubuntu-toolchain-r/test && \
  add-apt-repository ppa:git-core/ppa && \
  wget -O - https://apt.kitware.com/keys/kitware-archive-latest.asc 2>/dev/null | apt-key add - && \
  apt-add-repository 'deb https://apt.kitware.com/ubuntu/ xenial main' && \
  apt-get update -qq && \
  apt-get install -y gcc-7 g++-7 && \
  apt-get install -y git build-essential fakeroot devscripts debhelper gh && \
  apt-get install -y cmake ninja-build golang libunwind-dev libgtk-3-dev libgtkmm-3.0-dev && \
  apt-get install -y -t xenial-backports debhelper && \
  apt-get clean && \
  rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*

RUN update-alternatives \
  --install /usr/bin/gcc gcc /usr/bin/gcc-7 60 \
  --slave /usr/bin/g++ g++ /usr/bin/g++-7  \
  --slave /usr/bin/gcc-ar gcc-ar /usr/bin/gcc-ar-7 \
  --slave /usr/bin/gcc-nm gcc-nm /usr/bin/gcc-nm-7 \
  --slave /usr/bin/gcc-ranlib gcc-ranlib /usr/bin/gcc-ranlib-7
