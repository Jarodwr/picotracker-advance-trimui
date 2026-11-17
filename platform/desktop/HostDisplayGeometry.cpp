// Copyright (c) 2026 Jarod Wright and contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#include "HostDisplayGeometry.h"

#include <algorithm>
#include <cstring>

HostDisplayLayout ComputeDisplayLayout(int panelW, int panelH) {
  const int shortSide = std::max(0, std::min(panelW, panelH));
  if (shortSide < kHostFrameSize) {
    const int size = shortSide;
    return {1, (panelW - size) / 2, (panelH - size) / 2, size, true};
  }
  const int scale = shortSide / kHostFrameSize;
  const int size = kHostFrameSize * scale;
  return {scale, (panelW - size) / 2, (panelH - size) / 2, size, false};
}

bool IsHeadlessVideoDriver(const char *driver) {
  return driver && (std::strcmp(driver, "offscreen") == 0 ||
                    std::strcmp(driver, "dummy") == 0);
}

bool ShouldRefuseHeadlessPanel(const char *driver, bool driverRequested,
                               bool allowHeadless) {
  if (!driver) return true;
  return IsHeadlessVideoDriver(driver) && !driverRequested && !allowHeadless;
}
