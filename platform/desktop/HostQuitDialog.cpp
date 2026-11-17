// Copyright (c) 2026 Jarod Wright and contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#include "HostQuitDialog.h"

#include "HostLegend.h"

#include "Adapters/adv/display/font.h"

#include <algorithm>
#include <cstring>

namespace {

constexpr uint32_t kBorder = 0xFFFFFFFFu;
constexpr uint32_t kFill = 0xFF101820u;
constexpr uint32_t kInk = 0xFFFFFFFFu;

bool Tracked(GUIEventPadButtonType b, int count) {
  return static_cast<int>(b) >= 0 && static_cast<int>(b) < count;
}

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

bool HostQuitDialog::TakeChanged() {
  const bool changed = changed_;
  changed_ = false;
  return changed;
}

void HostQuitDialog::Open(uint32_t nowMs, std::vector<HostInputAction> &out) {
  open_ = true;
  asking_ = false;
  page_ = 0;
  openedMs_ = nowMs;
  changed_ = true;
  for (int b = 0; b < kPadButtons; ++b) {
    if (!forwarded_[b]) continue;
    forwarded_[b] = false;
    out.push_back({HostInputAction::Type::PadRelease,
                   static_cast<GUIEventPadButtonType>(b)});
  }
}

std::vector<HostInputAction>
HostQuitDialog::Process(const std::vector<HostInputAction> &in,
                        uint32_t nowMs) {
  std::vector<HostInputAction> out;
  for (const auto &action : in) {
    switch (action.type) {
    case HostInputAction::Type::Quit:
      confirmed_ = true; // hard exit: no dialog, no filtering
      out.push_back(action);
      break;
    case HostInputAction::Type::QuitRequest:
      if (!open_) Open(nowMs, out);
      else {
        open_ = false;
        asking_ = false;
        changed_ = true;
      }
      break;
    case HostInputAction::Type::PadPress:
      if (open_) {
        if (asking_) {
          if (nowMs - askedMs_ < kConfirmIgnoreMs) break;
          if (action.button == EPBT_A) confirmed_ = true;
          if (action.button == EPBT_A || action.button == EPBT_B) {
            open_ = false;
            asking_ = false;
            changed_ = true;
          }
        } else if (action.button == EPBT_B) {
          open_ = false;
          changed_ = true;
        } else if (action.button == EPBT_L || action.button == EPBT_R) {
          const int next = action.button == EPBT_L ? page_ - 1 : page_ + 1;
          if (next >= 0 && next < kLegendPages) {
            page_ = next;
            changed_ = true;
          }
        } else if (action.button == EPBT_A && nowMs - openedMs_ >= kIgnoreMs) {
          asking_ = true;
          askedMs_ = nowMs;
          changed_ = true;
        }
        break;
      }
      if (Tracked(action.button, kPadButtons)) forwarded_[action.button] = true;
      out.push_back(action);
      break;
    case HostInputAction::Type::PadRelease:
      // A release is only forwarded for a press the firmware saw.
      if (!Tracked(action.button, kPadButtons) || !forwarded_[action.button])
        break;
      forwarded_[action.button] = false;
      out.push_back(action);
      break;
    }
  }
  return out;
}

void HostQuitDialog::Draw(uint32_t *pixels, int size) const {
  // Home screen: the legend on the firmware's font grid over the whole frame.
  for (int y = 0; y < size; ++y)
    for (int x = 0; x < size; ++x) {
      const bool edge = x < 2 || x >= size - 2 || y < 2 || y >= size - 2;
      pixels[y * size + x] = edge ? kBorder : kFill;
    }
  const auto lines = BuildLegendPage(profile_, page_);
  for (size_t row = 0; row < lines.size(); ++row)
    Text(pixels, size, lines[row].c_str(), 8,
         static_cast<int>(row) * FONT_CHAR_HEIGHT);
  if (!asking_) return;

  const int w = 460, h = 190;
  const int x0 = (size - w) / 2, y0 = (size - h) / 2;
  for (int y = y0; y < y0 + h; ++y)
    for (int x = x0; x < x0 + w; ++x) {
      const bool edge = x < x0 + 4 || x >= x0 + w - 4 || y < y0 + 4 ||
                        y >= y0 + h - 4;
      pixels[y * size + x] = edge ? kBorder : kFill;
    }
  Text(pixels, size, "Quit PicoTracker?", x0 + (w - 17 * FONT_CHAR_WIDTH) / 2,
       y0 + 35);
  Text(pixels, size, "A = Yes   B = No", x0 + (w - 16 * FONT_CHAR_WIDTH) / 2,
       y0 + 110);
}
