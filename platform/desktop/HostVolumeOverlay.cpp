// Copyright (c) 2026 Jarod Wright and contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#include "HostVolumeOverlay.h"

#include "Adapters/adv/display/font.h"

#include <algorithm>
#include <cstdio>

namespace {

constexpr uint32_t kBorder = 0xFFFFFFFFu;
constexpr uint32_t kFill = 0xFF101820u;
constexpr uint32_t kBar = 0xFF58C878u;
constexpr uint32_t kInk = 0xFFFFFFFFu;

void Text(uint32_t *pixels, int size, const char *text, int x, int y) {
  for (; *text; ++text, x += FONT_CHAR_WIDTH) {
    const unsigned code = static_cast<unsigned char>(*text);
    if (code < 32 || code - 32 >= FONT_CHAR_COUNT) continue;
    const unsigned glyph = code - 32;
    for (int row = 0; row < FONT_CHAR_HEIGHT; ++row) {
      if (y + row < 0 || y + row >= size) continue;
      for (int col = 0; col < FONT_CHAR_WIDTH; ++col)
        if (FONT[glyph][row][col] && x + col >= 0 && x + col < size)
          pixels[(y + row) * size + x + col] = kInk;
    }
  }
}

} // namespace

void HostVolumeOverlay::Show(int volume, uint32_t nowMs) {
  volume_ = std::clamp(volume, 0, 100);
  shownMs_ = nowMs;
  if (!visible_) changed_ = true;
  visible_ = true;
}

bool HostVolumeOverlay::Tick(uint32_t nowMs) {
  if (visible_ && nowMs - shownMs_ >= kShowMs) {
    visible_ = false;
    changed_ = true;
  }
  const bool changed = changed_;
  changed_ = false;
  return changed;
}

void HostVolumeOverlay::Draw(uint32_t *pixels, int size) const {
  const int w = 360, h = 90;
  const int x0 = (size - w) / 2, y0 = size - h - 40;
  for (int y = y0; y < y0 + h; ++y)
    for (int x = x0; x < x0 + w; ++x) {
      const bool edge = x < x0 + 3 || x >= x0 + w - 3 || y < y0 + 3 ||
                        y >= y0 + h - 3;
      pixels[y * size + x] = edge ? kBorder : kFill;
    }
  char label[24];
  std::snprintf(label, sizeof(label), "Volume %d%%", volume_);
  Text(pixels, size, label, x0 + 20, y0 + 14);
  const int barX = x0 + 20, barY = y0 + 58, barW = w - 40, barH = 16;
  const int filled = barW * volume_ / 100;
  for (int y = barY; y < barY + barH; ++y)
    for (int x = barX; x < barX + barW; ++x)
      pixels[y * size + x] = x < barX + filled ? kBar : 0xFF384450u;
}
