FROM ubuntu:18.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update -qq && \
  apt-get install -y curl && \
  curl -fsSL https://cli.github.com/packages/githubcli-archive-keyring.gpg | dd of=/etc/apt/trusted.gpg.d/githubcli-archive-keyring.gpg && \
  echo "deb [arch=$(dpkg --print-architecture) signed-by=/etc/apt/trusted.gpg.d/githubcli-archive-keyring.gpg] https://cli.github.com/packages stable main" | tee /etc/apt/sources.list.d/github-cli.list > /dev/null && \
  apt-get install -y software-properties-common apt-transport-https wget && \
  add-apt-repository ppa:git-core/ppa && \
  wget -O - https://apt.kitware.com/keys/kitware-archive-latest.asc 2>/dev/null | apt-key add - && \
  apt-add-repository 'deb https://apt.kitware.com/ubuntu/ bionic main' && \
  apt-get update -qq && \
  apt-get install -y git build-essential fakeroot devscripts debhelper gh && \
  apt-get install -y cmake ninja-build golang libunwind-dev libgtk-3-dev libgtkmm-3.0-dev && \
  apt-get install -y -t bionic-backports debhelper && \
  rm -rf /var/lib/apt/lists/*
