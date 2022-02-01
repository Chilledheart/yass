FROM debian:9

ENV DEBIAN_FRONTEND=noninteractive

RUN echo "deb http://deb.debian.org/debian stretch-backports main" >> /etc/apt/sources.list

RUN apt-get update -qq && \
  apt-get install -y software-properties-common apt-transport-https wget && \
  apt-get install -y git build-essential fakeroot devscripts debhelper && \
  apt-get install -y cmake ninja-build golang libunwind-dev libgtk-3-dev libgtkmm-3.0-dev && \
  apt-get install -y -t stretch-backports debhelper && \
  rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*
# backport for newer git, from Xenial PPA
RUN apt-key adv --keyserver keyserver.ubuntu.com --recv-keys 'E1DD270288B4E6030699E45FA1715D88E1DF1F24' && \
  apt-add-repository 'deb https://ppa.launchpadcontent.net/git-core/ppa/ubuntu xenial main' && \
  apt-get update -qq && \
  apt-get install -y git && \
  rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*
# backport for newer cmake, from kitware Xenial Repository
RUN wget http://archive.ubuntu.com/ubuntu/pool/main/o/openssl/libssl1.0.0_1.0.2g-1ubuntu4_amd64.deb && \
  dpkg -i libssl1.0.0_1.0.2g-1ubuntu4_amd64.deb && rm -f *.deb && \
  wget -O - https://apt.kitware.com/keys/kitware-archive-latest.asc 2>/dev/null | apt-key add - && \
  apt-add-repository 'deb https://apt.kitware.com/ubuntu/ xenial main' && \
  apt-get update -qq && \
  apt-get install -y cmake && \
  rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*
