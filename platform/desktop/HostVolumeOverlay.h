/*
 * Copyright (c) 2026 Jarod Wright and contributors
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

// C-63: small "Volume NN%" bar the host paints over the 720x720 frame for
// about 1.5 s after the system volume changes. Composited by SDLWindow at
// Flush time like the quit dialog; the firmware never sees it.

#include <cstdint>

class HostVolumeOverlay {
public:
  static constexpr uint32_t kShowMs = 1500;

  // The volume (0-100) changed at nowMs: show it, or restart the timer.
  void Show(int volume, uint32_t nowMs);
  // True when visibility changed since the last call, so the host redraws.
  // Hides the overlay once kShowMs has passed.
  bool Tick(uint32_t nowMs);
  bool IsVisible() const { return visible_; }
  int Volume() const { return volume_; }

  void Draw(uint32_t *pixels, int size) const;

private:
  bool visible_ = false;
  bool changed_ = false;
  int volume_ = 0;
  uint32_t shownMs_ = 0;
};
