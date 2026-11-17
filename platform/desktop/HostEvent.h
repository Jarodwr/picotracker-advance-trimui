/*
 * Copyright (c) 2026 Jarod Wright and contributors
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "UIFramework/Interfaces/I_GUIWindowFactory.h"

#include <SDL.h>

#include <functional>

class GUIWindow;
class HostTimerService;
class SDLJoystickSource;

class HostEventManager final : public EventManager {
public:
  explicit HostEventManager(HostTimerService &timers) : timers_(timers) {}
  void SetWindow(GUIWindow *window) { window_ = window; }
  // Runs every loop iteration on the firmware thread (audio/MIDI pumps).
  void SetPump(std::function<void()> pump) { pump_ = std::move(pump); }
  // Optional joystick source (ADR 0016); keyboard handling is unchanged.
  void SetJoystickSource(SDLJoystickSource *joystick) { joystick_ = joystick; }
  bool QuitRequested() const { return quit_; }
  bool Handle(const SDL_Event &event);
  int MainLoop() override;
  void PostQuitMessage() override { quit_ = true; }
  int GetKeyCode(const char *name) override;

private:
  HostTimerService &timers_;
  GUIWindow *window_ = nullptr;
  SDLJoystickSource *joystick_ = nullptr;
  bool quit_ = false;
  std::function<void()> pump_;
};

class HostWindowFactory final : public I_GUIWindowFactory {
public:
  HostWindowFactory(SDL_Renderer *renderer, HostEventManager &events)
      : renderer_(renderer), events_(events) {}
  I_GUIWindowImp &CreateWindowImp(GUICreateWindowParams &) override;
  EventManager *GetEventManager() override { return &events_; }

private:
  SDL_Renderer *renderer_;
  HostEventManager &events_;
};
