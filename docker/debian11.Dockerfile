FROM debian:11

ENV DEBIAN_FRONTEND=noninteractive

RUN echo "deb http://deb.debian.org/debian bullseye-backports main" >> /etc/apt/sources.list

RUN apt-get update -qq && \
  apt-get install -y curl && \
  curl -fsSL https://cli.github.com/packages/githubcli-archive-keyring.gpg | dd of=/etc/apt/trusted.gpg.d/githubcli-archive-keyring.gpg && \
  echo "deb [arch=$(dpkg --print-architecture) signed-by=/etc/apt/trusted.gpg.d/githubcli-archive-keyring.gpg] https://cli.github.com/packages stable main" | tee /etc/apt/sources.list.d/github-cli.list > /dev/null && \
  apt-get install -y software-properties-common apt-transport-https wget && \
  apt-get update -qq && \
  apt-get install -y git build-essential fakeroot devscripts debhelper gh && \
  apt-get install -y cmake ninja-build golang libgtk-3-dev libgtkmm-3.0-dev && \
  apt-get clean && \
  rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*
