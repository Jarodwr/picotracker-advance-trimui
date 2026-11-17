# Copyright (c) 2026 Jarod Wright and contributors
#
# SPDX-License-Identifier: GPL-3.0-only

# Cross toolchain for the Trimui Smart Pro (TG5040, aarch64 Linux, glibc 2.23).
# Used inside the image from docker/trimui/Dockerfile; see ADR 0015.
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

# Wrappers around GCC 13 that point at the glibc 2.23 sysroot and link
# libstdc++ and libgcc statically.
set(CMAKE_C_COMPILER /usr/local/bin/trimui-cc)
set(CMAKE_CXX_COMPILER /usr/local/bin/trimui-c++)
set(CMAKE_AR /usr/bin/aarch64-linux-gnu-ar)
set(CMAKE_RANLIB /usr/bin/aarch64-linux-gnu-ranlib)

# The shared SDL2 2.30.8 link stub lives here (the device's vendor SDL2 is used at run time).
set(CMAKE_FIND_ROOT_PATH /opt/trimui-sysroot)
set(CMAKE_PREFIX_PATH /opt/trimui-sysroot)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE BOTH)

# compat.o supplies the few glibc > 2.23 symbols the static GCC runtime wants.
set(CMAKE_EXE_LINKER_FLAGS_INIT "-Wl,--as-needed /opt/trimui-compat/compat.o")
# libstdc++.a needs libpthread, which --as-needed drops once SDL is a shared library.
set(CMAKE_CXX_STANDARD_LIBRARIES_INIT "-ldl -Wl,--no-as-needed -lpthread -Wl,--as-needed")
# No build-tree rpath: the stub's directory does not exist on the device.
set(CMAKE_SKIP_RPATH ON)
