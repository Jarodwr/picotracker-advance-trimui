/*
 * Copyright (c) 2026 Jarod Wright and contributors
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

// C-60/C-64: the host-drawn Home screen. Menu (or held Select+Start) opens a
// controls legend (HostLegend) with a "Quit PicoTracker" item; A on that item
// asks "Quit PicoTracker?" and A answers yes. Pure C++ (no SDL): it sits
// between HostInputMap and the firmware window, swallows every pad event while
// it is open, and tells the host when the person said yes.

#include "HostInputMap.h"
#include "HostInputProfile.h"

#include <array>
#include <cstdint>
#include <vector>

class HostQuitDialog {
public:
  // Input ignored after opening, so a button mash cannot confirm.
  static constexpr uint32_t kIgnoreMs = 2500;
  // Input ignored after the confirmation box appears (the Home screen's own
  // window already guards the A that opens it).
  static constexpr uint32_t kConfirmIgnoreMs = 500;

  // The live input profile the legend shows; the compiled defaults until set.
  void SetProfile(const HostInputProfile &profile) { profile_ = profile; }

  bool IsOpen() const { return open_; }
  // True once A confirmed (or the hard-exit chord fired); the host quits.
  bool QuitConfirmed() const { return confirmed_; }
  // True when the dialog opened, closed or confirmed since the last call, so
  // the host knows to redraw.
  bool TakeChanged();

  // Filter one batch of actions from HostInputMap. Returns what the firmware
  // may see: nothing while open; on open, releases for pad buttons the
  // firmware still holds; QuitRequest and the A/B/L/R answers are consumed.
  // A hard-exit Quit passes through and sets QuitConfirmed.
  std::vector<HostInputAction> Process(const std::vector<HostInputAction> &in,
                                       uint32_t nowMs);

  // Paints the Home screen (and the confirmation over it) over a size x size ARGB frame (the 720x720 host frame).
  void Draw(uint32_t *pixels, int size) const;

private:
  static constexpr int kPadButtons = 16;
  void Open(uint32_t nowMs, std::vector<HostInputAction> &out);

  bool open_ = false;
  bool asking_ = false; // the "Quit PicoTracker?" box is showing
  int page_ = 0;
  HostInputProfile profile_ = HostInputProfile::Defaults();
  uint32_t askedMs_ = 0;
  bool confirmed_ = false;
  bool changed_ = false;
  uint32_t openedMs_ = 0;
  std::array<bool, kPadButtons> forwarded_{}; // presses the firmware holds
};
