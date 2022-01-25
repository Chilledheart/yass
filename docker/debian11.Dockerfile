FROM debian:11

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update -qq && \
  apt-get install -y git build-essential fakeroot devscripts debhelper && \
  apt-get install -y cmake golang libunwind-dev libgtk-3-dev libgtkmm-3.0-dev && \
  apt-get clean && \
  rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*
