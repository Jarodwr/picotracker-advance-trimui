// Copyright (c) 2026 Jarod Wright and contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#include "HostDisplay.h"
#include "Adapters/adv/display/display.h"

#include <SDL.h>
#include <algorithm>
#include <vector>

namespace {
constexpr int kSize = 720;

SDL_Renderer *renderer = nullptr;
SDL_Texture *texture = nullptr;
std::vector<uint32_t> backBuffer;

uint32_t Pack(uint16_t rgb565) {
  const uint32_t red = ((rgb565 >> 11) & 31) * 255 / 31;
  const uint32_t green = ((rgb565 >> 5) & 63) * 255 / 63;
  const uint32_t blue = (rgb565 & 31) * 255 / 31;
  return 0xFF000000u | (red << 16) | (green << 8) | blue;
}
} // namespace

void HostDisplayInstall(SDL_Renderer *value) {
  renderer = value;
  if (texture) SDL_DestroyTexture(texture);
  texture = renderer ? SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                                         SDL_TEXTUREACCESS_STREAMING, kSize, kSize)
                     : nullptr;
  backBuffer.assign(kSize * kSize, 0xFF000000u);
}

void display_draw_mono_bitmap_rgb565(uint16_t x, uint16_t y,
                                     const uint8_t *bitmap, uint16_t width,
                                     uint16_t height, uint16_t row_bytes,
                                     uint16_t fg_color565,
                                     uint16_t bg_color565) {
  if (!renderer || !texture || !bitmap) return;
  const uint32_t fg = Pack(fg_color565);
  const uint32_t bg = Pack(bg_color565);
  const int columns = std::min<int>(width, kSize - x);
  const int rows = std::min<int>(height, kSize - y);
  if (columns <= 0 || rows <= 0) return;
  for (int row = 0; row < rows; ++row) {
    const uint8_t *source = bitmap + row * row_bytes;
    uint32_t *target = &backBuffer[(y + row) * kSize + x];
    for (int column = 0; column < columns; ++column) {
      target[column] =
          (source[column >> 3] & (0x80u >> (column & 7u))) ? fg : bg;
    }
  }
  // One upload and one present per bitmap.
  SDL_UpdateTexture(texture, nullptr, backBuffer.data(),
                    kSize * sizeof(uint32_t));
  SDL_RenderCopy(renderer, texture, nullptr, nullptr);
  SDL_RenderPresent(renderer);
}
