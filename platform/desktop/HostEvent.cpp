// Copyright (c) 2026 Jarod Wright and contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#include "HostEvent.h"

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

bool HostEventManager::Handle(const SDL_Event &event) {
  if (event.type == SDL_QUIT ||
      (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)) {
    quit_ = true;
    if (window_) {
      GUIEvent quit(0, ET_SYSQUIT, SDL_GetTicks());
      window_->DispatchEvent(quit);
    }
    return true;
  }
  if (joystick_ && SDLJoystickSource::Handles(event.type)) {
    for (const auto &action : joystick_->Handle(event)) {
      if (action.type == HostInputAction::Type::Quit) {
        quit_ = true;
        if (window_) {
          GUIEvent quit(0, ET_SYSQUIT, SDL_GetTicks());
          window_->DispatchEvent(quit);
        }
      } else if (window_) {
        GUIEvent mapped(action.button,
                        action.type == HostInputAction::Type::PadPress
                            ? ET_PADBUTTONDOWN
                            : ET_PADBUTTONUP,
                        SDL_GetTicks());
        window_->DispatchEvent(mapped);
      }
    }
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
  return *new SDLWindow(renderer_);
}
