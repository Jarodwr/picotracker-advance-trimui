# Copyright (c) 2026 Jarod Wright and contributors
#
# SPDX-License-Identifier: GPL-3.0-only

# Linux backend: SDL audio plus explicitly unsupported MIDI.
# Included from the top-level CMakeLists.txt after picotracker_desktop exists.
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
  return()
endif()

target_sources(picotracker_desktop PRIVATE platform/linux/LinuxBackend.cpp)
target_include_directories(picotracker_desktop PRIVATE
  platform/desktop platform/linux)
target_compile_definitions(picotracker_desktop PRIVATE
  PICOTRACKER_LINUX_BACKEND)
