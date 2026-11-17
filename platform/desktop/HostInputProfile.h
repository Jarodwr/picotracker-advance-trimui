/*
 * Copyright (c) 2026 Jarod Wright and contributors
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

// ADR 0016 section 4: the Trimui input profile loaded from <data root>/input.ini.
// Pure C++, no SDL. Lines are `ADVANCE_NAME = trimui_name` plus the
// `left_stick_as_dpad` flag; anything that does not parse is rejected line by
// line and the compiled default stays in force for that entry.

#include "HostInputMap.h"

#include <string>
#include <vector>

struct HostInputProfile {
  // Which Trimui control drives each Advance pad button. `unbound` is set by
  // the value `none`.
  struct Entry {
    GUIEventPadButtonType button = EPBT_INVALID;
    TrimuiControl control = TrimuiControl::Count;
    bool unbound = false;
  };

  std::vector<Entry> entries; // one per Advance button, in AdvanceNames() order
  bool leftStickAsDpad = false;

  // The compiled default profile (the ADR 0016 section 1 table).
  static HostInputProfile Defaults();

  // Control for an Advance button; TrimuiControl::Count when unbound/unknown.
  TrimuiControl ControlFor(GUIEventPadButtonType button) const;
};

struct HostInputRejectedLine {
  int line = 0;        // 1-based
  std::string text;    // trimmed source line
  std::string reason;
};

struct HostInputParseResult {
  HostInputProfile profile;
  std::vector<HostInputRejectedLine> rejected;
};

// Parse input.ini text. Never fails: bad lines are listed in `rejected` and
// leave the default for that entry; empty or garbage text yields the defaults.
HostInputParseResult ParseHostInputProfile(const std::string &text);

// Stable symbolic names (lower case, as written in input.ini).
struct HostInputName {
  const char *name;
  GUIEventPadButtonType button;
};
const std::vector<HostInputName> &AdvanceNames(); // up down left right a b l r start select
const char *TrimuiControlName(TrimuiControl control);
// Case-insensitive; false when unknown.
bool ParseTrimuiControl(const std::string &name, TrimuiControl &out);
