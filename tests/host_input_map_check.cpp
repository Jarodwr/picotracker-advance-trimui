// Copyright (c) 2026 Jarod Wright and contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#include "HostInputMap.h"

#include <cstdio>
#include <cstdlib>
#include <vector>

using Actions = std::vector<HostInputAction>;
using Type = HostInputAction::Type;

#define CHECK(cond)                                                            \
  do {                                                                         \
    if (!(cond)) {                                                             \
      std::fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__,    \
                   #cond);                                                     \
      std::exit(1);                                                            \
    }                                                                          \
  } while (0)

namespace {

HostInputAction Press(GUIEventPadButtonType b) { return {Type::PadPress, b}; }
HostInputAction Release(GUIEventPadButtonType b) {
  return {Type::PadRelease, b};
}
const HostInputAction kQuit{Type::Quit, EPBT_INVALID};

bool Is(const Actions &got, const Actions &want) { return got == want; }

InputSource Hat(InputSource::Direction d) { return InputSource::Hat(0, d); }
InputSource Btn(int i) { return InputSource::Button(i); }

// Default source for each chord member.
const int kSelect = 6, kStart = 7, kL1 = 4, kR1 = 5;

void DefaultMappings() {
  struct Row {
    InputSource source;
    GUIEventPadButtonType pad;
  };
  const Row rows[] = {
      {Hat(InputSource::HatUp), EPBT_UP},
      {Hat(InputSource::HatDown), EPBT_DOWN},
      {Hat(InputSource::HatLeft), EPBT_LEFT},
      {Hat(InputSource::HatRight), EPBT_RIGHT},
      {Btn(1), EPBT_A}, // east face button
      {Btn(0), EPBT_B}, // south face button
      {Btn(kL1), EPBT_L},
      {Btn(kR1), EPBT_R},
      {Btn(kStart), EPBT_START},
      {Btn(kSelect), EPBT_SELECT},
  };
  HostInputMap map;
  for (const auto &row : rows) {
    CHECK(map.ResolvePadButton(row.source) == row.pad);
    CHECK(Is(map.Feed(row.source, true), {Press(row.pad)}));
    CHECK(Is(map.Feed(row.source, false), {Release(row.pad)}));
  }
}

void UnboundControls() {
  HostInputMap map;
  // X, Y, L3, R3, buttons past the device's 11, and anything unknown.
  for (const int b : {2, 3, 9, 10, 11, 12, 99}) {
    CHECK(map.ResolvePadButton(Btn(b)) == EPBT_INVALID);
    CHECK(map.Feed(Btn(b), true).empty());
    CHECK(map.Feed(Btn(b), false).empty());
  }
  CHECK(map.ResolvePadButton(Hat(InputSource::HatUp)) == EPBT_UP);
  CHECK(map.ResolvePadButton(InputSource::Hat(1, InputSource::HatUp)) ==
        EPBT_INVALID);
  // Duplicate transitions are ignored.
  CHECK(Is(map.Feed(Btn(0), true), {Press(EPBT_B)}));
  CHECK(map.Feed(Btn(0), true).empty());
  CHECK(Is(map.Feed(Btn(0), false), {Release(EPBT_B)}));
  CHECK(map.Feed(Btn(0), false).empty());
}

void Rebinding() {
  HostInputMap map;
  map.Unbind(Btn(0));
  map.Bind(Btn(2), TrimuiControl::South); // X now acts as B
  CHECK(map.ResolvePadButton(Btn(0)) == EPBT_INVALID);
  CHECK(Is(map.Feed(Btn(2), true), {Press(EPBT_B)}));
  CHECK(Is(map.Feed(Btn(2), false), {Release(EPBT_B)}));
  // Two sources on one control: one release only when the last goes up.
  map.Bind(Btn(3), TrimuiControl::South);
  CHECK(Is(map.Feed(Btn(2), true), {Press(EPBT_B)}));
  CHECK(map.Feed(Btn(3), true).empty());
  CHECK(map.Feed(Btn(2), false).empty());
  CHECK(Is(map.Feed(Btn(3), false), {Release(EPBT_B)}));
  // Unbinding while held still releases.
  CHECK(Is(map.Feed(Btn(2), true), {Press(EPBT_B)}));
  map.Unbind(Btn(2));
  CHECK(Is(map.Feed(Btn(2), false), {Release(EPBT_B)}));
}

void ChordQuits() {
  HostInputMap map;
  CHECK(Is(map.Feed(Btn(kSelect), true), {Press(EPBT_SELECT)}));
  CHECK(Is(map.Feed(Btn(kStart), true), {Press(EPBT_START)}));
  CHECK(Is(map.Feed(Btn(kL1), true), {Press(EPBT_L)}));
  // Completing the chord retracts what was delivered, then quits; the final
  // member is not forwarded.
  const Actions done = map.Feed(Btn(kR1), true);
  CHECK(Is(done, {Release(EPBT_SELECT), Release(EPBT_START), Release(EPBT_L),
                  kQuit}));
  // Nothing from the chord members until all four are up, and no second Quit.
  CHECK(map.Feed(Btn(kR1), false).empty());
  CHECK(map.Feed(Btn(kR1), true).empty());
  CHECK(map.Feed(Btn(kR1), false).empty());
  CHECK(map.Feed(Btn(kL1), false).empty());
  // Non-members still work while the latch is up.
  CHECK(Is(map.Feed(Btn(0), true), {Press(EPBT_B)}));
  CHECK(Is(map.Feed(Btn(0), false), {Release(EPBT_B)}));
  CHECK(map.Feed(Btn(kStart), false).empty());
  CHECK(map.Feed(Btn(kSelect), false).empty());
  // Latch cleared: members are ordinary again.
  CHECK(Is(map.Feed(Btn(kL1), true), {Press(EPBT_L)}));
  CHECK(Is(map.Feed(Btn(kL1), false), {Release(EPBT_L)}));
}

void ChordOrderAndExtras() {
  // Any order works, and an unrelated button held alongside is untouched.
  HostInputMap map;
  CHECK(Is(map.Feed(Btn(1), true), {Press(EPBT_A)}));
  CHECK(Is(map.Feed(Btn(kR1), true), {Press(EPBT_R)}));
  CHECK(Is(map.Feed(Btn(kStart), true), {Press(EPBT_START)}));
  CHECK(Is(map.Feed(Btn(kL1), true), {Press(EPBT_L)}));
  CHECK(Is(map.Feed(Btn(kSelect), true),
           {Release(EPBT_START), Release(EPBT_L), Release(EPBT_R), kQuit}));
  CHECK(Is(map.Feed(Btn(1), false), {Release(EPBT_A)}));
}

void HalfChord() {
  // A half-held chord released again: ordinary events, no Quit.
  HostInputMap map;
  CHECK(Is(map.Feed(Btn(kSelect), true), {Press(EPBT_SELECT)}));
  CHECK(Is(map.Feed(Btn(kStart), true), {Press(EPBT_START)}));
  CHECK(Is(map.Feed(Btn(kL1), true), {Press(EPBT_L)}));
  CHECK(Is(map.Feed(Btn(kL1), false), {Release(EPBT_L)}));
  CHECK(Is(map.Feed(Btn(kStart), false), {Release(EPBT_START)}));
  CHECK(Is(map.Feed(Btn(kSelect), false), {Release(EPBT_SELECT)}));
  // Three members plus a different button is not the chord.
  CHECK(Is(map.Feed(Btn(kSelect), true), {Press(EPBT_SELECT)}));
  CHECK(Is(map.Feed(Btn(kStart), true), {Press(EPBT_START)}));
  CHECK(Is(map.Feed(Btn(kL1), true), {Press(EPBT_L)}));
  CHECK(Is(map.Feed(Btn(1), true), {Press(EPBT_A)}));
  CHECK(Is(map.ReleaseAll(), {Release(EPBT_A), Release(EPBT_L),
                              Release(EPBT_START), Release(EPBT_SELECT)}));
  CHECK(map.ReleaseAll().empty());
}

void ReleaseAllClears() {
  HostInputMap map;
  map.Feed(Btn(0), true);
  map.Feed(Hat(InputSource::HatUp), true);
  const Actions r = map.ReleaseAll();
  CHECK(r.size() == 2);
  CHECK(map.ReleaseAll().empty());
  CHECK(Is(map.Feed(Btn(0), true), {Press(EPBT_B)})); // fresh press works
}

void StickIgnoredByDefault() {
  HostInputMap map;
  CHECK(!map.LeftStickAsDpad());
  for (const float v : {1.0f, -1.0f, 0.6f, 0.0f}) {
    CHECK(map.FeedAxis(HostInputMap::kLeftStickX, v).empty());
    CHECK(map.FeedAxis(HostInputMap::kLeftStickY, v).empty());
  }
  CHECK(map.ResolvePadButton(InputSource::Axis(0, 1)) == EPBT_INVALID);
  CHECK(map.Feed(InputSource::Axis(0, 1), true).empty());
}

void RightStickAlwaysIgnored() {
  HostInputMap map;
  map.SetLeftStickAsDpad(true);
  for (const int axis : {2, 3, 4, 5, 7}) { // right stick, triggers, other
    CHECK(map.FeedAxis(axis, 1.0f).empty());
    CHECK(map.FeedAxis(axis, -1.0f).empty());
    CHECK(map.ResolvePadButton(InputSource::Axis(axis, 1)) == EPBT_INVALID);
  }
}

void StickHysteresis() {
  HostInputMap map;
  CHECK(map.SetLeftStickAsDpad(true).empty());
  CHECK(map.ResolvePadButton(InputSource::Axis(0, 1)) == EPBT_RIGHT);
  CHECK(map.ResolvePadButton(InputSource::Axis(0, -1)) == EPBT_LEFT);
  CHECK(map.ResolvePadButton(InputSource::Axis(1, -1)) == EPBT_UP);
  CHECK(map.ResolvePadButton(InputSource::Axis(1, 1)) == EPBT_DOWN);

  const int x = HostInputMap::kLeftStickX;
  CHECK(map.FeedAxis(x, 0.49f).empty()); // below the press threshold
  CHECK(Is(map.FeedAxis(x, 0.50f), {Press(EPBT_RIGHT)}));
  CHECK(map.FeedAxis(x, 0.80f).empty());
  CHECK(map.FeedAxis(x, 0.36f).empty()); // inside the band: still held
  CHECK(map.FeedAxis(x, 0.50f).empty());
  CHECK(Is(map.FeedAxis(x, 0.34f), {Release(EPBT_RIGHT)}));
  CHECK(map.FeedAxis(x, 0.45f).empty()); // must climb back to 50% to press
  CHECK(Is(map.FeedAxis(x, 0.55f), {Press(EPBT_RIGHT)}));
  // Straight through to the other side: release then press.
  CHECK(Is(map.FeedAxis(x, -0.9f), {Release(EPBT_RIGHT), Press(EPBT_LEFT)}));
  CHECK(Is(map.FeedAxis(x, 0.0f), {Release(EPBT_LEFT)}));

  // Y axis: SDL positive is down.
  const int y = HostInputMap::kLeftStickY;
  CHECK(Is(map.FeedAxis(y, 1.0f), {Press(EPBT_DOWN)}));
  CHECK(Is(map.FeedAxis(y, 0.0f), {Release(EPBT_DOWN)}));
  CHECK(Is(map.FeedAxis(y, -0.5f), {Press(EPBT_UP)}));
  CHECK(Is(map.FeedAxis(y, -0.2f), {Release(EPBT_UP)}));
}

void StickAndHatShareDirection() {
  HostInputMap map;
  map.SetLeftStickAsDpad(true);
  CHECK(Is(map.Feed(Hat(InputSource::HatRight), true), {Press(EPBT_RIGHT)}));
  CHECK(map.FeedAxis(0, 1.0f).empty());
  CHECK(map.Feed(Hat(InputSource::HatRight), false).empty());
  CHECK(Is(map.FeedAxis(0, 0.0f), {Release(EPBT_RIGHT)}));
}

void StickFlagOffReleases() {
  HostInputMap map;
  map.SetLeftStickAsDpad(true);
  CHECK(Is(map.FeedAxis(0, -1.0f), {Press(EPBT_LEFT)}));
  CHECK(Is(map.SetLeftStickAsDpad(false), {Release(EPBT_LEFT)}));
  CHECK(map.FeedAxis(0, -1.0f).empty());
  CHECK(map.FeedAxis(0, 0.0f).empty());
}

} // namespace

const HostInputAction kRequest{Type::QuitRequest, EPBT_INVALID};
const int kMenu = 8;

void MenuRequestsDialog() {
  HostInputMap map;
  CHECK(Is(map.Feed(Btn(kMenu), true), {kRequest}));
  CHECK(map.Feed(Btn(kMenu), false).empty());
  CHECK(Is(map.Feed(Btn(kMenu), true), {kRequest})); // each press asks again
  CHECK(map.Feed(Btn(kMenu), true).empty());          // duplicate ignored
  CHECK(map.Feed(Btn(kMenu), false).empty());
  // Menu during a latched chord is swallowed.
  for (int b : {kSelect, kStart, kL1, kR1}) map.Feed(Btn(b), true);
  CHECK(map.Feed(Btn(kMenu), true).empty());
}

void SelectStartHold() {
  HostInputMap map;
  CHECK(map.Tick(0).empty());
  CHECK(Is(map.Feed(Btn(kSelect), true), {Press(EPBT_SELECT)}));
  CHECK(Is(map.Feed(Btn(kStart), true), {Press(EPBT_START)}));
  CHECK(map.Tick(100).empty()); // timing starts at the first tick seeing both
  CHECK(map.Tick(100 + HostInputMap::kComboHoldMs - 1).empty());
  // Held long enough: pad releases (no stuck buttons), then the request.
  CHECK(Is(map.Tick(100 + HostInputMap::kComboHoldMs),
           {Release(EPBT_SELECT), Release(EPBT_START), kRequest}));
  CHECK(map.Tick(5000).empty()); // latched: asks once per hold
  CHECK(map.Feed(Btn(kSelect), false).empty());
  CHECK(map.Tick(6000).empty()); // Start still down, still latched
  CHECK(map.Feed(Btn(kStart), false).empty());
  CHECK(map.Tick(7000).empty());
  // After both are up a new hold works again.
  map.Feed(Btn(kSelect), true);
  map.Feed(Btn(kStart), true);
  map.Tick(8000);
  CHECK(Is(map.Tick(8000 + HostInputMap::kComboHoldMs),
           {Release(EPBT_SELECT), Release(EPBT_START), kRequest}));
}

void SelectStartShortTapDoesNotAsk() {
  HostInputMap map;
  map.Feed(Btn(kSelect), true);
  map.Feed(Btn(kStart), true);
  map.Tick(0);
  map.Tick(500);
  CHECK(Is(map.Feed(Btn(kStart), false), {Release(EPBT_START)}));
  CHECK(map.Tick(5000).empty()); // timing restarted, only Select is down
  map.Feed(Btn(kStart), true);
  CHECK(map.Tick(5100).empty());
  CHECK(map.Tick(5100 + HostInputMap::kComboHoldMs - 1).empty());
}

void ComboThenChordStillQuits() {
  HostInputMap map;
  map.Feed(Btn(kSelect), true);
  map.Feed(Btn(kStart), true);
  map.Tick(0);
  map.Tick(HostInputMap::kComboHoldMs); // dialog request
  map.Feed(Btn(kL1), true);
  CHECK(Is(map.Feed(Btn(kR1), true), {Release(EPBT_L), kQuit}));
}

int main() {
  MenuRequestsDialog();
  SelectStartHold();
  SelectStartShortTapDoesNotAsk();
  ComboThenChordStillQuits();
  DefaultMappings();
  UnboundControls();
  Rebinding();
  ChordQuits();
  ChordOrderAndExtras();
  HalfChord();
  ReleaseAllClears();
  StickIgnoredByDefault();
  RightStickAlwaysIgnored();
  StickHysteresis();
  StickAndHatShareDirection();
  StickFlagOffReleases();
  std::puts("host_input_map_check passed");
  return 0;
}
