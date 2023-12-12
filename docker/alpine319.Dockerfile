FROM alpine:3.19
RUN sed -i 's/dl-cdn.alpinelinux.org/mirrors.ustc.edu.cn/g' /etc/apk/repositories
RUN apk add --no-cache git bash perl curl go tar && \
  apk add --no-cache build-base binutils-gold linux-headers cmake ninja-build curl-dev && \
  apk add --no-cache gtk+3.0-dev gettext && \
  apk add --no-cache llvm clang lld && \
  ln -sf /usr/lib/ninja-build/bin/ninja /usr/bin/ninja
# https://github.com/cli/cli/blob/trunk/docs/install_linux.md
RUN echo "@community http://dl-cdn.alpinelinux.org/alpine/edge/community" >> /etc/apk/repositories
RUN sed -i 's/dl-cdn.alpinelinux.org/mirrors.ustc.edu.cn/g' /etc/apk/repositories
RUN apk add --no-cache github-cli@community
