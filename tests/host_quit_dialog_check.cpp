// Copyright (c) 2026 Jarod Wright and contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#include "HostQuitDialog.h"

#include <cstdio>
#include <cstdlib>
#include <vector>

void HostLegendChecks(); // tests/host_legend_check.cpp

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
const HostInputAction kRequest{Type::QuitRequest, EPBT_INVALID};
const HostInputAction kQuit{Type::Quit, EPBT_INVALID};
const uint32_t kWait = HostQuitDialog::kIgnoreMs;

void ClosedPassesPadEvents() {
  HostQuitDialog d;
  CHECK((d.Process({Press(EPBT_A), Release(EPBT_A)}, 0) ==
         Actions{Press(EPBT_A), Release(EPBT_A)}));
  CHECK(!d.IsOpen() && !d.QuitConfirmed() && !d.TakeChanged());
}

void OpenSwallowsAndReleasesHeld() {
  HostQuitDialog d;
  d.Process({Press(EPBT_L), Press(EPBT_UP)}, 0);
  // The firmware still holds L and Up: they are released as the dialog opens.
  const auto out = d.Process({kRequest}, 10);
  CHECK(d.IsOpen() && d.TakeChanged());
  CHECK(out.size() == 2);
  CHECK(out[0] == Release(EPBT_L) || out[1] == Release(EPBT_L));
  CHECK(out[0] == Release(EPBT_UP) || out[1] == Release(EPBT_UP));
  // No pad events leak while open, in either direction.
  CHECK(d.Process({Press(EPBT_UP), Release(EPBT_UP), Press(EPBT_START),
                   Release(EPBT_START), Release(EPBT_L)},
                  20)
            .empty());
  CHECK(d.IsOpen());
}

void IgnoreWindow() {
  HostQuitDialog d;
  d.Process({kRequest}, 1000);
  d.TakeChanged();
  // An A mash during the window neither asks nor quits.
  CHECK(d.Process({Press(EPBT_A), Release(EPBT_A), Press(EPBT_A),
                   Release(EPBT_A)},
                  1000 + kWait - 1)
            .empty());
  CHECK(d.IsOpen() && !d.QuitConfirmed() && !d.TakeChanged());
  // Just after it, A on the Quit item asks; it does not quit yet.
  CHECK(d.Process({Press(EPBT_A)}, 1000 + kWait).empty());
  CHECK(!d.QuitConfirmed() && d.IsOpen() && d.TakeChanged());
  // The question has its own short ignore window, then A confirms.
  const uint32_t asked = 1000 + kWait;
  CHECK(d.Process({Press(EPBT_A)}, asked + HostQuitDialog::kConfirmIgnoreMs - 1)
            .empty());
  CHECK(!d.QuitConfirmed() && d.IsOpen());
  CHECK(d.Process({Press(EPBT_A)}, asked + HostQuitDialog::kConfirmIgnoreMs)
            .empty());
  CHECK(d.QuitConfirmed() && !d.IsOpen() && d.TakeChanged());
}

void YesQuits() {
  HostQuitDialog d;
  d.Process({kRequest}, 0);
  d.Process({Press(EPBT_A)}, kWait + 5);
  CHECK(!d.QuitConfirmed());
  d.Process({Press(EPBT_A)}, kWait + 5 + HostQuitDialog::kConfirmIgnoreMs);
  CHECK(d.QuitConfirmed());
}

void NoInQuestionCloses() {
  HostQuitDialog d;
  d.Process({kRequest}, 0);
  d.Process({Press(EPBT_A)}, kWait);
  d.TakeChanged();
  const uint32_t t = kWait + HostQuitDialog::kConfirmIgnoreMs;
  CHECK(d.Process({Press(EPBT_B)}, t).empty());
  CHECK(!d.IsOpen() && !d.QuitConfirmed() && d.TakeChanged());
}

void BClosesHomeImmediately() {
  HostQuitDialog d;
  d.Process({kRequest}, 0);
  d.TakeChanged();
  CHECK(d.Process({Press(EPBT_B)}, 10).empty());
  CHECK(!d.IsOpen() && !d.QuitConfirmed() && d.TakeChanged());
}

void PagesSwitchWithLAndR() {
  HostQuitDialog d;
  d.Process({kRequest}, 0);
  d.TakeChanged();
  CHECK(d.Process({Press(EPBT_R), Release(EPBT_R)}, 5).empty());
  CHECK(d.IsOpen() && d.TakeChanged());
  // Past the last page nothing changes; L goes back.
  CHECK(d.Process({Press(EPBT_R)}, 6).empty() && !d.TakeChanged());
  CHECK(d.Process({Press(EPBT_L)}, 7).empty() && d.TakeChanged());
  CHECK(d.Process({Press(EPBT_L)}, 8).empty() && !d.TakeChanged());
}

void NoCloses() {
  HostQuitDialog d;
  d.Process({kRequest}, 0);
  d.TakeChanged();
  CHECK(d.Process({Press(EPBT_B), Release(EPBT_B)}, kWait + 5).empty());
  CHECK(!d.IsOpen() && !d.QuitConfirmed() && d.TakeChanged());
  // Back to normal: pad events flow again; B's release from the dialog did
  // not reach the firmware, and a fresh press does.
  CHECK((d.Process({Press(EPBT_B), Release(EPBT_B)}, kWait + 10) ==
         Actions{Press(EPBT_B), Release(EPBT_B)}));
}

void DoubleMenuCloses() {
  HostQuitDialog d;
  d.Process({kRequest}, 0);
  d.TakeChanged();
  // Menu again closes it, even inside the ignore window.
  CHECK(d.Process({kRequest}, 100).empty());
  CHECK(!d.IsOpen() && !d.QuitConfirmed() && d.TakeChanged());
  // The same Menu press after the window also closes and does not confirm.
  d.Process({kRequest}, 200);
  CHECK(d.Process({kRequest}, 200 + kWait).empty());
  CHECK(!d.IsOpen() && !d.QuitConfirmed());
}

void HardExitWhileOpen() {
  HostQuitDialog d;
  d.Process({kRequest}, 0);
  // The chord inside the ignore window still exits, with no dialog answer.
  CHECK((d.Process({kQuit}, 5) == Actions{kQuit}));
  CHECK(d.QuitConfirmed());
}

void HardExitWithoutDialog() {
  HostQuitDialog d;
  CHECK((d.Process({kQuit}, 0) == Actions{kQuit}));
  CHECK(d.QuitConfirmed() && !d.IsOpen());
}

void DrawsHomeScreen() {
  HostQuitDialog d;
  const int size = 720;
  std::vector<uint32_t> frame(size * size, 0xFF000000u);
  d.Draw(frame.data(), size);
  // The Home screen covers the whole frame.
  CHECK(frame[0] != 0xFF000000u && frame[size * size - 1] != 0xFF000000u);
  CHECK(frame[(size / 2) * size + size / 2] != 0xFF000000u);
  int lit = 0;
  for (const auto p : frame) lit += p == 0xFFFFFFFFu;
  CHECK(lit > 500); // text and border
}

} // namespace

int main() {
  ClosedPassesPadEvents();
  OpenSwallowsAndReleasesHeld();
  IgnoreWindow();
  YesQuits();
  NoInQuestionCloses();
  BClosesHomeImmediately();
  PagesSwitchWithLAndR();
  NoCloses();
  DoubleMenuCloses();
  HardExitWhileOpen();
  HardExitWithoutDialog();
  DrawsHomeScreen();
  HostLegendChecks();
  std::puts("host_quit_dialog_check passed");
  return 0;
}
