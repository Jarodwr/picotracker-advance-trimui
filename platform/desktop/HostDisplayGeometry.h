/*
 * Copyright (c) 2026 Jarod Wright and contributors
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

// Pure display layout maths (ADR 0016 section 5). No SDL dependency.
struct HostDisplayLayout {
  int scale;        // integer scale of the 720x720 frame (>= 1)
  int x;            // left offset of the frame in panel pixels
  int y;            // top offset of the frame in panel pixels
  int size;         // rendered side length of the frame in panel pixels
  bool fractional;  // panel smaller than 720 on an axis: frame is fitted down
};

constexpr int kHostFrameSize = 720;

// Largest integer scale that fits, centred; fractional fit if the panel is
// smaller than the frame on either axis.
HostDisplayLayout ComputeDisplayLayout(int panelW, int panelH);

// True for SDL video drivers that draw nowhere (no picture on a panel).
bool IsHeadlessVideoDriver(const char *driver);

// Guard for --display trimui: a headless driver is refused unless it was asked
// for (SDL_VIDEODRIVER set, or --allow-headless). A null driver is refused.
bool ShouldRefuseHeadlessPanel(const char *driver, bool driverRequested,
                               bool allowHeadless);
