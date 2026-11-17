// Copyright (c) 2026 Jarod Wright and contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#include "HostSystem.h"
#include <cstdio>

void HostSystem::SystemPutChar(int c) { std::putchar(c); }

uint32_t HostSystem::Micros() {
  auto elapsed = std::chrono::steady_clock::now() - start_;
  return static_cast<uint32_t>(
      std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count());
}
