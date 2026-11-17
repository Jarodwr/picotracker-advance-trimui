// Copyright (c) 2026 Jarod Wright and contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#include "HostInputProfile.h"

#include <cstdio>
#include <cstdlib>

#define CHECK(cond)                                                            \
  do {                                                                         \
    if (!(cond)) {                                                             \
      std::fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__,    \
                   #cond);                                                     \
      std::exit(1);                                                            \
    }                                                                          \
  } while (0)

namespace {

bool IsDefault(const HostInputProfile &p, GUIEventPadButtonType b,
               TrimuiControl c) {
  return p.ControlFor(b) == c;
}

void CheckDefaultTable(const HostInputProfile &p) {
  // ADR 0016 section 1 table.
  CHECK(IsDefault(p, EPBT_UP, TrimuiControl::DpadUp));
  CHECK(IsDefault(p, EPBT_DOWN, TrimuiControl::DpadDown));
  CHECK(IsDefault(p, EPBT_LEFT, TrimuiControl::DpadLeft));
  CHECK(IsDefault(p, EPBT_RIGHT, TrimuiControl::DpadRight));
  CHECK(IsDefault(p, EPBT_A, TrimuiControl::East));
  CHECK(IsDefault(p, EPBT_B, TrimuiControl::South));
  CHECK(IsDefault(p, EPBT_L, TrimuiControl::L1));
  CHECK(IsDefault(p, EPBT_R, TrimuiControl::R1));
  CHECK(IsDefault(p, EPBT_START, TrimuiControl::Start));
  CHECK(IsDefault(p, EPBT_SELECT, TrimuiControl::Select));
  CHECK(p.entries.size() == 10);
  CHECK(!p.leftStickAsDpad);
}

} // namespace

int main() {
  // Defaults and their agreement with HostInputMap's control -> pad table.
  const HostInputProfile defaults = HostInputProfile::Defaults();
  CheckDefaultTable(defaults);
  for (const auto &e : defaults.entries)
    CHECK(HostInputMap::PadButtonFor(e.control) == e.button);

  // Stable names.
  const auto &names = AdvanceNames();
  CHECK(names.size() == 10);
  CHECK(std::string(names[0].name) == "up" && std::string(names[9].name) == "select");
  TrimuiControl c;
  CHECK(ParseTrimuiControl("L1", c) && c == TrimuiControl::L1);
  CHECK(std::string(TrimuiControlName(TrimuiControl::DpadUp)) == "dpad_up");
  for (int i = 0; i < static_cast<int>(TrimuiControl::Count); ++i) {
    const auto ctl = static_cast<TrimuiControl>(i);
    TrimuiControl back;
    CHECK(ParseTrimuiControl(TrimuiControlName(ctl), back) && back == ctl);
  }
  CHECK(!ParseTrimuiControl("turbo", c));

  // Empty, comments only, garbage: defaults.
  auto r = ParseHostInputProfile("");
  CheckDefaultTable(r.profile);
  CHECK(r.rejected.empty());
  r = ParseHostInputProfile("# hi\n; there\n\n[input]\n");
  CheckDefaultTable(r.profile);
  CHECK(r.rejected.empty());
  r = ParseHostInputProfile("\x01\x02garbage\nnot an ini\n");
  CheckDefaultTable(r.profile);
  CHECK(r.rejected.size() == 2);
  CHECK(r.rejected[0].line == 1 && r.rejected[1].line == 2);

  // Valid, with CRLF, BOM, case and spacing.
  r = ParseHostInputProfile(
      "\xEF\xBB\xBF# profile\r\nA = South\r\n  b=EAST  \r\nl = l2\r\n"
      "left_stick_as_dpad = TRUE\r\nstart = none\r\n");
  CHECK(r.rejected.empty());
  CHECK(r.profile.ControlFor(EPBT_A) == TrimuiControl::South);
  CHECK(r.profile.ControlFor(EPBT_B) == TrimuiControl::East);
  CHECK(r.profile.ControlFor(EPBT_L) == TrimuiControl::L2);
  CHECK(r.profile.ControlFor(EPBT_START) == TrimuiControl::Count);
  CHECK(r.profile.ControlFor(EPBT_R) == TrimuiControl::R1); // untouched
  CHECK(r.profile.leftStickAsDpad);

  // Partial: only one line, the rest default.
  r = ParseHostInputProfile("select = menu");
  CHECK(r.rejected.empty());
  CHECK(r.profile.ControlFor(EPBT_SELECT) == TrimuiControl::Menu);
  CHECK(r.profile.ControlFor(EPBT_START) == TrimuiControl::Start);

  // Malformed lines are rejected with reasons; good lines still apply.
  r = ParseHostInputProfile("a east\n= south\nb =\nl = r1\nleft_stick_as_dpad = maybe\n");
  CHECK(r.rejected.size() == 4);
  CHECK(r.rejected[0].line == 1 && r.rejected[0].reason == "missing '='");
  CHECK(r.rejected[1].line == 2 && r.rejected[1].reason == "empty name");
  CHECK(r.rejected[2].line == 3 && r.rejected[2].reason == "empty value");
  CHECK(r.rejected[3].line == 5 && r.rejected[3].reason == "invalid boolean");
  CHECK(r.profile.ControlFor(EPBT_L) == TrimuiControl::R1);
  CHECK(!r.profile.leftStickAsDpad);
  CHECK(r.profile.ControlFor(EPBT_A) == TrimuiControl::East);

  // Unknown names keep the default.
  r = ParseHostInputProfile("x = east\na = turbo\nb = south\n");
  CHECK(r.rejected.size() == 2);
  CHECK(r.rejected[0].reason == "unknown Advance button");
  CHECK(r.rejected[1].reason == "unknown Trimui control");
  CHECK(r.rejected[1].text == "a = turbo");
  CHECK(r.profile.ControlFor(EPBT_A) == TrimuiControl::East);

  // Duplicate keys: first wins, later ones rejected (even if they would be bad).
  r = ParseHostInputProfile("a = north\nA = west\nleft_stick_as_dpad = on\n"
                            "left_stick_as_dpad = off\n");
  CHECK(r.rejected.size() == 2);
  CHECK(r.rejected[0].line == 2 && r.rejected[0].reason == "duplicate key");
  CHECK(r.rejected[1].line == 4 && r.rejected[1].reason == "duplicate key");
  CHECK(r.profile.ControlFor(EPBT_A) == TrimuiControl::North);
  CHECK(r.profile.leftStickAsDpad);

  std::puts("host_input_profile_check: ok");
  return 0;
}
