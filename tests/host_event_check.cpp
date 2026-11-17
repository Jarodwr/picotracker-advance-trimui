// Copyright (c) 2026 Jarod Wright and contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#include "HostEvent.h"
#include "HostTimer.h"
#include "SDLJoystickSource.h"
#include "UIFramework/SimpleBaseClasses/GUIWindow.h"

#include <SDL.h>
#include <cassert>
#include <chrono>
#include <thread>
#include <vector>

class ProbeWindow final : public GUIWindow {
public:
  explicit ProbeWindow(I_GUIWindowImp &imp) : GUIWindow(imp) {}
  int cursor = 0;
  int releases = 0;

  bool onEvent(GUIEvent &event) override {
    if (event.GetType() == ET_PADBUTTONDOWN && event.GetValue() == EPBT_DOWN)
      ++cursor;
    if (event.GetType() == ET_PADBUTTONUP && event.GetValue() == EPBT_DOWN)
      ++releases;
    return true;
  }
  void onUpdate(bool) override {}
  void AnimationUpdate() override {}
};

// Records every pad event as (type, button).
class RecordWindow final : public GUIWindow {
public:
  explicit RecordWindow(I_GUIWindowImp &imp) : GUIWindow(imp) {}
  std::vector<std::pair<int, int>> events;
  int quits = 0;

  bool onEvent(GUIEvent &event) override {
    if (event.GetType() == ET_SYSQUIT) ++quits;
    else events.emplace_back(event.GetType(), event.GetValue());
    return true;
  }
  void onUpdate(bool) override {}
  void AnimationUpdate() override {}
};

static SDL_Event Joy(Uint32 type, int index, int value = 0) {
  SDL_Event e{};
  e.type = type;
  if (type == SDL_JOYBUTTONDOWN || type == SDL_JOYBUTTONUP) e.jbutton.button = index;
  else if (type == SDL_JOYHATMOTION) { e.jhat.hat = index; e.jhat.value = value; }
  else if (type == SDL_JOYAXISMOTION) { e.jaxis.axis = index; e.jaxis.value = value; }
  else e.jdevice.which = index;
  return e;
}

static void JoystickChecks(HostEventManager &events, RecordWindow &window,
                           SDLJoystickSource &joystick) {
  events.SetWindow(&window);
  events.SetJoystickSource(&joystick);
  auto press = [&](const SDL_Event &down, const SDL_Event &up, int pad) {
    window.events.clear();
    assert(events.Handle(down));
    assert(window.events.size() == 1);
    assert((window.events[0] == std::pair<int, int>(ET_PADBUTTONDOWN, pad)));
    assert(events.Handle(up));
    assert(window.events.size() == 2);
    assert((window.events[1] == std::pair<int, int>(ET_PADBUTTONUP, pad)));
  };
  // Every default mapping.
  press(Joy(SDL_JOYHATMOTION, 0, SDL_HAT_UP), Joy(SDL_JOYHATMOTION, 0, SDL_HAT_CENTERED), EPBT_UP);
  press(Joy(SDL_JOYHATMOTION, 0, SDL_HAT_DOWN), Joy(SDL_JOYHATMOTION, 0, SDL_HAT_CENTERED), EPBT_DOWN);
  press(Joy(SDL_JOYHATMOTION, 0, SDL_HAT_LEFT), Joy(SDL_JOYHATMOTION, 0, SDL_HAT_CENTERED), EPBT_LEFT);
  press(Joy(SDL_JOYHATMOTION, 0, SDL_HAT_RIGHT), Joy(SDL_JOYHATMOTION, 0, SDL_HAT_CENTERED), EPBT_RIGHT);
  const struct { int button; int pad; } buttons[] = {
      {1, EPBT_A}, {0, EPBT_B}, {4, EPBT_L}, {5, EPBT_R}, {7, EPBT_START}, {6, EPBT_SELECT}};
  for (const auto &b : buttons)
    press(Joy(SDL_JOYBUTTONDOWN, b.button), Joy(SDL_JOYBUTTONUP, b.button), b.pad);
  // Controls with no Advance equivalent produce nothing.
  window.events.clear();
  for (int inert : {2, 3, 8, 9, 10, 11, 12}) {
    events.Handle(Joy(SDL_JOYBUTTONDOWN, inert));
    events.Handle(Joy(SDL_JOYBUTTONUP, inert));
  }
  assert(window.events.empty());

  // Left stick is off by default, then drives the D-pad when the profile asks.
  events.Handle(Joy(SDL_JOYAXISMOTION, 0, 32000));
  assert(window.events.empty());
  joystick.ApplyProfile(ParseHostInputProfile("left_stick_as_dpad = true\n").profile);
  events.Handle(Joy(SDL_JOYAXISMOTION, 0, 32000));
  assert(window.events.size() == 1 && window.events[0].second == EPBT_RIGHT);
  events.Handle(Joy(SDL_JOYAXISMOTION, 0, 0));
  assert(window.events.size() == 2 && window.events[1].first == ET_PADBUTTONUP);

  // A profile remap moves the Trimui control behind an Advance button.
  joystick.ApplyProfile(ParseHostInputProfile("a = south\nb = east\n").profile);
  press(Joy(SDL_JOYBUTTONDOWN, 0), Joy(SDL_JOYBUTTONUP, 0), EPBT_A);
  press(Joy(SDL_JOYBUTTONDOWN, 1), Joy(SDL_JOYBUTTONUP, 1), EPBT_B);
  joystick.ApplyProfile(HostInputProfile::Defaults());
  press(Joy(SDL_JOYBUTTONDOWN, 1), Joy(SDL_JOYBUTTONUP, 1), EPBT_A);

  // Hot-plug: unplugging releases what was held, and input works after re-add.
  window.events.clear();
  events.Handle(Joy(SDL_JOYBUTTONDOWN, 1));
  events.Handle(Joy(SDL_JOYDEVICEREMOVED, 7));
  assert(window.events.size() == 2 && window.events[1].first == ET_PADBUTTONUP);
  events.Handle(Joy(SDL_JOYDEVICEADDED, 7)); // no such device: logged, ignored
  press(Joy(SDL_JOYBUTTONDOWN, 1), Joy(SDL_JOYBUTTONUP, 1), EPBT_A);

  // Exit chord: Select+Start+L1+R1 quits and presents no stray press.
  assert(!events.QuitRequested());
  for (int b : {6, 7, 4, 5}) events.Handle(Joy(SDL_JOYBUTTONDOWN, b));
  assert(events.QuitRequested());
  assert(window.quits == 1);
}

class TickObserver final : public I_Observer {
public:
  int ticks = 0;
  void Update(Observable &, I_ObservableData *) override { ++ticks; }
};

int main() {
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_JOYSTICK) == 0);
  SDL_Window *native = SDL_CreateWindow("input-check", 0, 0, 720, 720,
                                        SDL_WINDOW_HIDDEN);
  assert(native);
  SDL_Renderer *renderer = SDL_CreateRenderer(native, -1, SDL_RENDERER_SOFTWARE);
  assert(renderer);
  {
    HostTimerService timers;
    HostEventManager events(timers);
    HostWindowFactory factory(renderer, events);
    GUICreateWindowParams params{"input-check"};
    ProbeWindow window(factory.CreateWindowImp(params));
    events.SetWindow(&window);

    SDL_Event input{};
    input.type = SDL_KEYDOWN;
    input.key.keysym.sym = SDLK_DOWN;
    assert(events.Handle(input));
    assert(window.cursor == 1);
    input.type = SDL_KEYUP;
    assert(events.Handle(input));
    assert(window.releases == 1);

    {
      // Joystick source on a fresh manager so the keyboard checks above stay as is.
      HostEventManager joyEvents(timers);
      RecordWindow recorder(factory.CreateWindowImp(params));
      SDLJoystickSource joystick;
      joystick.ApplyProfile(HostInputProfile::Defaults());
      JoystickChecks(joyEvents, recorder, joystick);
    }

    auto *timer = timers.CreateTimer();
    TickObserver observer;
    timer->AddObserver(observer);
    timer->SetPeriod(1.0f);
    assert(timer->Start());
    std::this_thread::sleep_for(std::chrono::milliseconds(3));
    timers.Pump();
    assert(observer.ticks == 1);
    timer->Stop();
    timer->RemoveObserver(observer);
  }
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(native);
  SDL_Quit();
}
