/*
 * Copyright (c) 2026 Jarod Wright and contributors
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "UIFramework/Interfaces/I_GUIWindowImp.h"
#include <SDL.h>
#include <cstdint>
#include <vector>

class SDLWindow final : public I_GUIWindowImp {
public:
  explicit SDLWindow(SDL_Renderer *renderer);
  ~SDLWindow() override;
  void PushEvent(GUIEvent &event) override;
  void DrawRect(GUIRect &rect) override;
  void SetColor(GUIColor &color) override { foreground_ = color; }
  void DrawChar(char c, const GUIPoint &pos, const GUITextProperties &props) override;
  void DrawString(const char *value, const GUIPoint &pos,
                  const GUITextProperties &props, bool overlay) override;
  GUIRect GetRect() override;
  void Invalidate() override {}
  // Uploads the CPU back buffer and presents, but only if something was drawn
  // since the last flush.
  void Flush() override;
  void Lock() override {}
  void Unlock() override {}
  void Clear(GUIColor &color, bool overlay) override;
  void ClearTextRect(GUIRect &rect) override;

private:
  static constexpr int kSize = 720;
  void FillPixels(int x, int y, int width, int height, const GUIColor &color);

  SDL_Renderer *renderer_;
  SDL_Texture *texture_ = nullptr;
  std::vector<uint32_t> pixels_;
  bool dirty_ = true;
  GUIColor foreground_{255, 255, 255};
  GUIColor background_{0, 0, 0};
};
