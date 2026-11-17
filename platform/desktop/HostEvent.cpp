// Copyright (c) 2026 Jarod Wright and contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#include "HostEvent.h"

#include "HostQuitDialog.h"
#include "HostVolumeOverlay.h"
#include "HostTimer.h"
#include "SDLJoystickSource.h"
#include "SDLWindow.h"
#include "UIFramework/SimpleBaseClasses/GUIWindow.h"

#include <cstring>

namespace {

GUIEventPadButtonType MapKey(SDL_Keycode key) {
  switch (key) {
  case SDLK_LEFT: return EPBT_LEFT;
  case SDLK_DOWN: return EPBT_DOWN;
  case SDLK_RIGHT: return EPBT_RIGHT;
  case SDLK_UP: return EPBT_UP;
  case SDLK_a: return EPBT_L;
  case SDLK_x: return EPBT_B;
  case SDLK_z: return EPBT_A;
  case SDLK_s: return EPBT_R;
  case SDLK_RETURN: return EPBT_START;
  case SDLK_TAB: return EPBT_SELECT;
  default: return EPBT_INVALID;
  }
}

} // namespace

void HostEventManager::RequestQuit() {
  if (quit_) return;
  if (flush_) flush_();
  quit_ = true;
  if (window_) {
    GUIEvent quit(0, ET_SYSQUIT, SDL_GetTicks());
    window_->DispatchEvent(quit);
  }
}

// Joystick actions go through the quit dialog (when installed): it swallows
// pad events while open and consumes the QuitRequest and its A/B answers.
void HostEventManager::Dispatch(const std::vector<HostInputAction> &input) {
  const Uint32 now = SDL_GetTicks();
  const auto actions = dialog_ ? dialog_->Process(input, now) : input;
  for (const auto &action : actions) {
    if (action.type == HostInputAction::Type::Quit) {
      RequestQuit();
    } else if (action.type == HostInputAction::Type::QuitRequest) {
      // Without a dialog installed a Menu press does nothing.
    } else if (window_) {
      GUIEvent mapped(action.button,
                      action.type == HostInputAction::Type::PadPress
                          ? ET_PADBUTTONDOWN
                          : ET_PADBUTTONUP,
                      now);
      window_->DispatchEvent(mapped);
    }
  }
  if (dialog_) {
    if (dialog_->TakeChanged() && redraw_) redraw_();
    if (dialog_->QuitConfirmed()) RequestQuit();
  }
}

bool HostEventManager::Handle(const SDL_Event &event) {
  if (event.type == SDL_QUIT ||
      (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)) {
    RequestQuit();
    return true;
  }
  if (joystick_ && SDLJoystickSource::Handles(event.type)) {
    Dispatch(joystick_->Handle(event));
    return true;
  }
  if (event.type != SDL_KEYDOWN && event.type != SDL_KEYUP) return false;
  if (event.type == SDL_KEYDOWN && event.key.repeat) return false;
  const auto button = MapKey(event.key.keysym.sym);
  if (button == EPBT_INVALID) return false;
  if (window_) {
    GUIEvent mapped(button, event.type == SDL_KEYDOWN ? ET_PADBUTTONDOWN
                                                     : ET_PADBUTTONUP,
                    SDL_GetTicks());
    window_->DispatchEvent(mapped);
  }
  return true;
}

int HostEventManager::MainLoop() {
  Uint32 lastClock = SDL_GetTicks();
  while (!quit_) {
    SDL_Event event;
    if (SDL_WaitEventTimeout(&event, pump_ ? 4 : 16)) Handle(event);
    while (SDL_PollEvent(&event)) Handle(event);
    if (joystick_) Dispatch(joystick_->Tick(SDL_GetTicks()));
    timers_.Pump();
    if (pump_) pump_();
    const Uint32 now = SDL_GetTicks();
    if (window_ && now - lastClock >= PICO_CLOCK_INTERVAL) {
      window_->ClockTick();
      window_->Update(false);
      lastClock = now;
    }
  }
  return 0;
}

int HostEventManager::GetKeyCode(const char *name) {
  return name ? static_cast<int>(SDL_GetKeyFromName(name)) : -1;
}

I_GUIWindowImp &HostWindowFactory::CreateWindowImp(GUICreateWindowParams &) {
  window_ = new SDLWindow(renderer_);
  window_->SetOverlay(dialog_);
  window_->SetVolumeOverlay(volume_);
  return *window_;
}

void HostWindowFactory::Refresh() {
  if (window_) window_->Refresh();
}
