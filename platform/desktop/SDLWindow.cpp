// Copyright (c) 2026 Jarod Wright and contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#include "SDLWindow.h"
#include "Adapters/adv/display/font.h"
#include "UIFramework/SimpleBaseClasses/GUIWindow.h"
#include <algorithm>

void SDLWindow::PushEvent(GUIEvent &event) {
  if (_window) _window->DispatchEvent(event);
}

static uint32_t pack(const GUIColor &color) {
  return 0xFF000000u | (static_cast<uint32_t>(color._r) << 16) |
         (static_cast<uint32_t>(color._g) << 8) | color._b;
}

SDLWindow::SDLWindow(SDL_Renderer *renderer)
    : renderer_(renderer), pixels_(kSize * kSize, 0xFF000000u) {
  texture_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_ARGB8888,
                               SDL_TEXTUREACCESS_STREAMING, kSize, kSize);
}

SDLWindow::~SDLWindow() {
  if (texture_) SDL_DestroyTexture(texture_);
}

void SDLWindow::FillPixels(int x, int y, int width, int height,
                           const GUIColor &color) {
  const int x1 = std::min(x + width, kSize);
  const int y1 = std::min(y + height, kSize);
  x = std::max(x, 0);
  y = std::max(y, 0);
  if (x >= x1 || y >= y1) return;
  const uint32_t value = pack(color);
  for (int row = y; row < y1; ++row) {
    std::fill(pixels_.begin() + row * kSize + x, pixels_.begin() + row * kSize + x1,
              value);
  }
  dirty_ = true;
}

void SDLWindow::Flush() {
  if (!dirty_ || !texture_) return;
  SDL_UpdateTexture(texture_, nullptr, pixels_.data(), kSize * sizeof(uint32_t));
  SDL_RenderCopy(renderer_, texture_, nullptr, nullptr);
  SDL_RenderPresent(renderer_);
  dirty_ = false;
}

void SDLWindow::DrawRect(GUIRect &rect) {
  FillPixels(rect.Left() * 720 / 320, rect.Top() * 720 / 240,
             rect.Width() * 720 / 320, rect.Height() * 720 / 240, foreground_);
}

void SDLWindow::DrawChar(char c, const GUIPoint &pos, const GUITextProperties &props) {
  const unsigned code = static_cast<unsigned char>(c);
  if (code < 32) return;
  const unsigned glyph = code - 32;
  if (glyph >= FONT_CHAR_COUNT) return;
  const int x = static_cast<int>(pos._x / 8) * FONT_CHAR_WIDTH + 8;
  const int y = static_cast<int>(pos._y / 8) * FONT_CHAR_HEIGHT;
  const GUIColor &ink = props.invert_ ? background_ : foreground_;
  const GUIColor &paper = props.invert_ ? foreground_ : background_;
  FillPixels(x, y, FONT_CHAR_WIDTH, FONT_CHAR_HEIGHT, paper);
  const uint32_t inkValue = pack(ink);
  for (int row = 0; row < FONT_CHAR_HEIGHT; ++row) {
    if (y + row < 0 || y + row >= kSize) continue;
    for (int col = 0; col < FONT_CHAR_WIDTH; ++col) {
      if (x + col < 0 || x + col >= kSize) continue;
      if (FONT[glyph][row][col]) pixels_[(y + row) * kSize + x + col] = inkValue;
    }
  }
}

void SDLWindow::DrawString(const char *value, const GUIPoint &pos,
                           const GUITextProperties &props, bool overlay) {
  (void)overlay;
  for (int index = 0; value[index]; ++index) {
    DrawChar(value[index], GUIPoint(pos._x + index * 8, pos._y), props);
  }
}

GUIRect SDLWindow::GetRect() { return GUIRect(0, 0, 320, 240); }

void SDLWindow::Clear(GUIColor &color, bool overlay) {
  (void)overlay;
  background_ = color;
  FillPixels(0, 0, kSize, kSize, color);
}

void SDLWindow::ClearTextRect(GUIRect &rect) {
  FillPixels(static_cast<int>(rect.Left() / 8) * FONT_CHAR_WIDTH + 8,
             static_cast<int>(rect.Top() / 8) * FONT_CHAR_HEIGHT,
             static_cast<int>(rect.Width() / 8) * FONT_CHAR_WIDTH,
             static_cast<int>(rect.Height() / 8) * FONT_CHAR_HEIGHT, background_);
}
