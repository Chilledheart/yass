FROM ubuntu:20.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update -qq && \
  apt-get install -y git build-essential fakeroot devscripts debhelper && \
  apt-get install -y cmake ninja-build golang libunwind-dev libgtk-3-dev libgtkmm-3.0-dev && \
  rm -rf /var/lib/apt/lists/*
