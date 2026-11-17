# Copyright (c) 2026 Jarod Wright and contributors
#
# SPDX-License-Identifier: GPL-3.0-only

# Reproducible cross-build environment for the Trimui Smart Pro (aarch64, glibc).
# See docs/adr/0015-trimui-cross-toolchain.md.
#
# Compiler: Ubuntu 24.04 GCC 13 (C++23, designated initialisers, constexpr
# strlen as the firmware needs). Target libc: glibc 2.23 headers and libraries
# unpacked from Ubuntu 16.04 arm64 packages, so the binary needs nothing newer
# on the device. libstdc++ and libgcc are linked statically. SDL2 2.30.8 is
# built as a shared library against that sysroot, used only as a link stub: the
# binary links libSDL2-2.0.so.0 dynamically and the device's own vendor SDL2
# (/usr/trimui/lib, GLES and vsync on the PowerVR GPU) satisfies it at run time.
# The stub is never shipped in the pak.
FROM ubuntu:24.04

ARG SDL2_VERSION=2.30.8

RUN apt-get update -qq && DEBIAN_FRONTEND=noninteractive apt-get install -y -qq --no-install-recommends \
      ca-certificates curl cmake ninja-build make python3 binutils file \
      g++-13-aarch64-linux-gnu binutils-aarch64-linux-gnu \
      libdrm-dev libgbm-dev libasound2-dev \
    && rm -rf /var/lib/apt/lists/*

# glibc 2.23 sysroot from xenial (libc6, libc6-dev, linux-libc-dev), arm64. 2.23
# is the floor: later aarch64 glibcs version powf/expf/logf at 2.27, which an
# older device libc would not have.
RUN set -e; K=/usr/share/keyrings/ubuntu-archive-keyring.gpg; \
    mkdir -p /tmp/apt/lists/partial /tmp/apt/cache/archives/partial /opt/trimui-glibc; \
    echo "deb [arch=arm64 signed-by=$K] http://ports.ubuntu.com/ubuntu-ports xenial main" > /tmp/apt/sources.list; \
    echo "deb [arch=arm64 signed-by=$K] http://ports.ubuntu.com/ubuntu-ports xenial-updates main" >> /tmp/apt/sources.list; \
    APT="-o Dir::Etc::sourcelist=/tmp/apt/sources.list -o Dir::Etc::sourceparts=/dev/null -o Dir::State::lists=/tmp/apt/lists -o Dir::Cache=/tmp/apt/cache -o APT::Architecture=arm64 -o APT::Architectures=arm64 -o Dir::State::status=/dev/null"; \
    apt-get $APT update -qq; \
    cd /tmp/apt && apt-get $APT download libc6 libc6-dev linux-libc-dev; \
    for d in /tmp/apt/*.deb; do dpkg-deb -x "$d" /opt/trimui-glibc; done; \
    # Absolute symlinks (libm.so -> /lib/.../libm.so.6) would resolve to the host: make them sysroot-relative. \
    find /opt/trimui-glibc -type l | while read -r l; do t="$(readlink "$l")"; \
      case "$t" in /*) ln -sfr "/opt/trimui-glibc$t" "$l";; esac; done; \
    rm -rf /tmp/apt

# Copy of the libstdc++ headers with the glibc >= 2.30 clock APIs disabled
# (pthread_cond_clockwait and friends do not exist in glibc 2.23).
RUN mkdir -p /opt/trimui-cxx && cp -a /usr/include/c++/13 /opt/trimui-cxx/c++ \
    && cp -a /usr/include/aarch64-linux-gnu/c++/13 /opt/trimui-cxx/arch \
    && sed -i -E 's@^#define (_GLIBCXX_USE_PTHREAD_(COND_CLOCKWAIT|MUTEX_CLOCKLOCK|RWLOCK_CLOCKLOCK)) 1@/* #undef \1 (glibc < 2.30) */@' \
         /opt/trimui-cxx/arch/bits/c++config.h \
    && ! grep -q '^#define _GLIBCXX_USE_PTHREAD_.*CLOCK' /opt/trimui-cxx/arch/bits/c++config.h

# Headers for the SDL backends that are loaded with dlopen on the device.
RUN mkdir -p /opt/trimui-devhdrs && cp -a /usr/include/alsa /usr/include/libdrm /usr/include/gbm.h \
      /usr/include/xf86drm.h /usr/include/xf86drmMode.h /opt/trimui-devhdrs/

# Compiler wrappers: CMake needs single executables for CC and CXX. Native
# GCC ignores --sysroot for its multiarch default paths, so headers, start
# files and libraries are listed explicitly and the host's are never used.
ENV TRIMUI_GCC_FLAGS="--sysroot=/opt/trimui-glibc -nostdinc \
 -isystem /usr/lib/gcc/aarch64-linux-gnu/13/include \
 -isystem /opt/trimui-glibc/usr/include/aarch64-linux-gnu -isystem /opt/trimui-glibc/usr/include \
 -isystem /opt/trimui-devhdrs -isystem /opt/trimui-devhdrs/libdrm \
 -B/opt/trimui-glibc/usr/lib/aarch64-linux-gnu -L/opt/trimui-glibc/usr/lib/aarch64-linux-gnu \
 -L/opt/trimui-glibc/lib/aarch64-linux-gnu -static-libgcc"
RUN printf '%s\n' '#!/bin/sh' 'exec aarch64-linux-gnu-gcc-13 $TRIMUI_GCC_FLAGS "$@"' > /usr/local/bin/trimui-cc \
    && printf '%s\n' '#!/bin/sh' \
      'exec aarch64-linux-gnu-g++-13 -nostdinc++ -isystem /opt/trimui-cxx/c++ -isystem /opt/trimui-cxx/arch $TRIMUI_GCC_FLAGS -static-libstdc++ "$@"' \
      > /usr/local/bin/trimui-c++ \
    && chmod +x /usr/local/bin/trimui-*

# SDL's CMake probes ALSA by linking -lasound, which the glibc sysroot lacks, and
# silently drops the backend. This stub only satisfies that probe: the real
# libasound is loaded with dlopen on the device (SDL_ALSA_SHARED) and the stub
# is never linked into the firmware. SDL derives the name it dlopens from the
# real file name, hence libasound.so.2.0.0 behind the libasound.so link.
RUN printf '%s\n' 'int snd_pcm_recover(void) { return 0; }' > /tmp/asound-stub.c \
    && trimui-cc -shared -fPIC -Wl,-soname,libasound.so.2 \
         -o /opt/trimui-glibc/usr/lib/aarch64-linux-gnu/libasound.so.2.0.0 /tmp/asound-stub.c \
    && ln -s libasound.so.2.0.0 /opt/trimui-glibc/usr/lib/aarch64-linux-gnu/libasound.so \
    && rm /tmp/asound-stub.c

# Shared SDL2 2.30.8, a link stub only (SONAME libSDL2-2.0.so.0). The device's
# vendor library replaces it at run time; keep the version equal to the vendor's
# so the symbols and struct layouts match. Video and audio backends that need
# vendor libraries are loaded with dlopen, so only headers are needed here.
RUN curl -fsSL "https://github.com/libsdl-org/SDL/releases/download/release-${SDL2_VERSION}/SDL2-${SDL2_VERSION}.tar.gz" \
      | tar -xz -C /tmp \
    && cmake -S /tmp/SDL2-${SDL2_VERSION} -B /tmp/sdl-build -G Ninja \
         -DCMAKE_SYSTEM_NAME=Linux -DCMAKE_SYSTEM_PROCESSOR=aarch64 \
         -DCMAKE_C_COMPILER=/usr/local/bin/trimui-cc \
         -DCMAKE_AR=/usr/bin/aarch64-linux-gnu-ar -DCMAKE_RANLIB=/usr/bin/aarch64-linux-gnu-ranlib \
         -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/opt/trimui-sysroot \
         -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
         -DSDL_SHARED=ON -DSDL_STATIC=OFF -DSDL_TEST=OFF \
         -DSDL_X11=OFF -DSDL_WAYLAND=OFF -DSDL_PULSEAUDIO=OFF -DSDL_JACK=OFF \
         -DSDL_PIPEWIRE=OFF -DSDL_SNDIO=OFF -DSDL_ESD=OFF -DSDL_ARTS=OFF -DSDL_NAS=OFF \
         -DSDL_OPENGL=OFF -DSDL_OPENGLES=OFF -DSDL_VULKAN=OFF -DSDL_LIBUDEV=OFF \
         -DSDL_DBUS=OFF -DSDL_IBUS=OFF -DSDL_HIDAPI=OFF \
         -DSDL_KMSDRM=ON -DSDL_KMSDRM_SHARED=ON -DSDL_ALSA=ON -DSDL_ALSA_SHARED=ON \
         -DSDL_DUMMYVIDEO=ON -DSDL_DUMMYAUDIO=ON \
    && cmake --build /tmp/sdl-build && cmake --install /tmp/sdl-build \
    && mkdir -p /opt/trimui-sdl-stub && cp -a /opt/trimui-sysroot/lib/libSDL2-2.0.so* /opt/trimui-sdl-stub/ \
    && rm -rf /tmp/SDL2-* /tmp/sdl-build

# Shims for symbols newer than glibc 2.27 that the static GCC runtime pulls in.
COPY compat.c /tmp/compat.c
RUN mkdir -p /opt/trimui-compat && trimui-cc -O2 -fPIC -c /tmp/compat.c -o /opt/trimui-compat/compat.o \
    && rm /tmp/compat.c

WORKDIR /src
