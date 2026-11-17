// Copyright (c) 2026 Jarod Wright and contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#include "HostDisplayGeometry.h"

#include <cstdio>
#include <cstdlib>

static void Expect(int w, int h, int scale, int x, int y, bool fractional) {
  const HostDisplayLayout l = ComputeDisplayLayout(w, h);
  if (l.scale != scale || l.x != x || l.y != y || l.fractional != fractional) {
    std::fprintf(stderr, "%dx%d: got scale %d x %d y %d frac %d\n", w, h,
                 l.scale, l.x, l.y, l.fractional);
    std::exit(1);
  }
}

int main() {
  Expect(1280, 720, 1, 280, 0, false);
  Expect(1024, 768, 1, 152, 24, false);
  Expect(720, 720, 1, 0, 0, false);
  Expect(1440, 1440, 2, 0, 0, false);
  Expect(1440, 900, 1, 360, 90, false);
  // 640x480: fractional fit, 480px square centred horizontally.
  Expect(640, 480, 1, 80, 0, true);
  if (ComputeDisplayLayout(640, 480).size != 480) return 1;
  if (ComputeDisplayLayout(1280, 720).size != 720) return 1;
  // Real Smart Pro panel as the vendor mali driver reports it.
  Expect(1280, 720, 1, 280, 0, false);
  // Portrait report of the same panel still centres the frame.
  Expect(720, 1280, 1, 0, 280, false);
  // Driver guard.
  if (!ShouldRefuseHeadlessPanel("offscreen", false, false)) return 1;
  if (!ShouldRefuseHeadlessPanel("dummy", false, false)) return 1;
  if (ShouldRefuseHeadlessPanel("dummy", true, false)) return 1;
  if (ShouldRefuseHeadlessPanel("offscreen", false, true)) return 1;
  if (ShouldRefuseHeadlessPanel("mali", false, false)) return 1;
  if (ShouldRefuseHeadlessPanel("KMSDRM", false, false)) return 1;
  if (!ShouldRefuseHeadlessPanel(nullptr, true, true)) return 1;
  std::puts("host_display_layout_check passed");
  return 0;
}
