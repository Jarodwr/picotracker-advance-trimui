/*
 * Copyright (c) 2026 Jarod Wright and contributors
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "UIFramework/Interfaces/I_GUIWindowFactory.h"

#include "HostInputMap.h"

#include <SDL.h>

#include <functional>
#include <vector>

class GUIWindow;
class HostTimerService;
class SDLJoystickSource;
class SDLWindow;
class HostQuitDialog;
class HostVolumeOverlay;

class HostEventManager final : public EventManager {
public:
  explicit HostEventManager(HostTimerService &timers) : timers_(timers) {}
  void SetWindow(GUIWindow *window) { window_ = window; }
  // Runs every loop iteration on the firmware thread (audio/MIDI pumps).
  void SetPump(std::function<void()> pump) { pump_ = std::move(pump); }
  // Optional joystick source (ADR 0016); keyboard handling is unchanged.
  void SetJoystickSource(SDLJoystickSource *joystick) { joystick_ = joystick; }
  // C-60: confirmation dialog between the joystick map and the firmware.
  // `redraw` runs when it opens or closes; `flush` saves the project before
  // any quit (dialog or hard exit) and runs once.
  void SetQuitDialog(HostQuitDialog *dialog, std::function<void()> redraw) {
    dialog_ = dialog;
    redraw_ = std::move(redraw);
  }
  void SetQuitFlush(std::function<void()> flush) { flush_ = std::move(flush); }
  bool QuitRequested() const { return quit_; }
  bool Handle(const SDL_Event &event);
  int MainLoop() override;
  void PostQuitMessage() override { quit_ = true; }
  int GetKeyCode(const char *name) override;

private:
  HostTimerService &timers_;
  GUIWindow *window_ = nullptr;
  SDLJoystickSource *joystick_ = nullptr;
  void RequestQuit();
  void Dispatch(const std::vector<HostInputAction> &actions);

  HostQuitDialog *dialog_ = nullptr;
  bool quit_ = false;
  std::function<void()> pump_;
  std::function<void()> redraw_;
  std::function<void()> flush_;
};

class HostWindowFactory final : public I_GUIWindowFactory {
public:
  HostWindowFactory(SDL_Renderer *renderer, HostEventManager &events)
      : renderer_(renderer), events_(events) {}
  I_GUIWindowImp &CreateWindowImp(GUICreateWindowParams &) override;
  // C-60: the quit dialog the window composites over the frame.
  void SetQuitDialog(const HostQuitDialog *dialog) { dialog_ = dialog; }
  // C-63: the volume indicator the window composites under the dialog.
  void SetVolumeOverlay(const HostVolumeOverlay *volume) { volume_ = volume; }
  // Repaint the window now (the dialog opened or closed).
  void Refresh();
  EventManager *GetEventManager() override { return &events_; }

private:
  SDL_Renderer *renderer_;
  HostEventManager &events_;
  const HostQuitDialog *dialog_ = nullptr;
  const HostVolumeOverlay *volume_ = nullptr;
  SDLWindow *window_ = nullptr;
};
