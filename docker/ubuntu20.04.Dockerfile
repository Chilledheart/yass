FROM ubuntu:20.04

ENV DEBIAN_FRONTEND=noninteractive

RUN sed -i 's/http:\/\/archive\.ubuntu\.com\/ubuntu\//http:\/\/azure\.archive\.ubuntu\.com\/ubuntu\//g' /etc/apt/sources.list && \
    sed -i 's/http:\/\/[a-z][a-z]\.archive\.ubuntu\.com\/ubuntu\//http:\/\/azure\.archive\.ubuntu\.com\/ubuntu\//g' /etc/apt/sources.list

RUN apt-get update -qq && \
  apt-get install -y curl && \
  curl -fsSL https://cli.github.com/packages/githubcli-archive-keyring.gpg | dd of=/etc/apt/trusted.gpg.d/githubcli-archive-keyring.gpg && \
  echo "deb [arch=$(dpkg --print-architecture) signed-by=/etc/apt/trusted.gpg.d/githubcli-archive-keyring.gpg] https://cli.github.com/packages stable main" | tee /etc/apt/sources.list.d/github-cli.list > /dev/null && \
  apt-get install -y software-properties-common apt-transport-https wget && \
  apt-get update -qq && \
  apt-get install -y git build-essential fakeroot devscripts debhelper gh && \
  apt-get install -y cmake ninja-build golang libunwind-dev libgtk-3-dev libgtkmm-3.0-dev && \
  rm -rf /var/lib/apt/lists/*
