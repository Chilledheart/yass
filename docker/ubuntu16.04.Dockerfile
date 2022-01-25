FROM ubuntu:16.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update -qq && \
  apt-get install -y software-properties-common apt-transport-https wget && \
  add-apt-repository ppa:ubuntu-toolchain-r/test && \
  add-apt-repository ppa:git-core/ppa && \
  wget -O - https://apt.kitware.com/keys/kitware-archive-latest.asc 2>/dev/null | gpg --dearmor - | sudo tee /usr/share/keyrings/kitware-archive-keyring.gpg > /dev/null && \
  echo 'deb [signed-by=/usr/share/keyrings/kitware-archive-keyring.gpg] https://apt.kitware.com/ubuntu/ xenial main' | sudo tee /etc/apt/sources.list.d/kitware.list >/dev/null && \
  apt-get update -qq && \
  rm /usr/share/keyrings/kitware-archive-keyring.gpg && \
  apt-get install -y kitware-archive-keyring && \
  apt-get install -y gcc-7 g++-7 && \
  apt-get install -y git build-essential fakeroot devscripts debhelper && \
  apt-get install -y cmake golang libunwind-dev libgtk-3-dev libgtkmm-3.0-dev && \
  apt-get clean && \
  rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*

RUN update-alternatives \
    --install /usr/bin/gcc gcc /usr/bin/gcc-7 60 \
    --slave /usr/bin/g++ g++ /usr/bin/g++-7  \
    --slave /usr/bin/gcc-ar gcc-ar /usr/bin/gcc-ar-7 \
    --slave /usr/bin/gcc-nm gcc-nm /usr/bin/gcc-nm-7 \
    --slave /usr/bin/gcc-ranlib gcc-ranlib /usr/bin/gcc-ranlib-7
