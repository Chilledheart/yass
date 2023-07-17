FROM alpine:3.18
RUN apk add --no-cache git bash perl curl go tar coreutils && \
  apk add --no-cache build-base binutils-gold linux-headers cmake ninja-build curl-dev && \
  apk add --no-cache gtk+3.0-dev gettext && \
  apk add --no-cache llvm clang lld && \
  ln -sf /usr/lib/ninja-build/bin/ninja /usr/bin/ninja
