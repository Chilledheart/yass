FROM ubuntu:18.04

ENV DEBIAN_FRONTEND=noninteractive

RUN echo "deb http://archive.ubuntu.com/ubuntu bionic-backports main" >> /etc/apt/sources.list

RUN apt-get update -qq && \
  apt-get install -y software-properties-common && \
  add-apt-repository ppa:git-core/ppa && \
  apt-get update -qq && \
  apt-get install -y git build-essential fakeroot devscripts debhelper && \
  apt-get install -y cmake ninja-build golang libunwind-dev libgtk-3-dev libgtkmm-3.0-dev && \
  apt-get install -y -t bionic-backports debhelper && \
  rm -rf /var/lib/apt/lists/*
