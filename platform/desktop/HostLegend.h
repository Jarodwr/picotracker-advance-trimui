/*
 * Copyright (c) 2026 Jarod Wright and contributors
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

// C-64: text of the Home screen's controls legend. Pure C++ (no SDL, no
// pixels): HostQuitDialog draws the lines on the firmware's 22x30 font grid,
// which on the 720x720 host frame is 32 columns by 24 rows. The Smart Pro side
// comes from the live HostInputProfile (so an input.ini remap changes the
// text); the Advance side is the firmware's own naming (EventDispatcher maps
// A=ENTER, B=EDIT, L=ALT, R=NAV, Start=PLAY; Select is unused by the firmware).

#include "HostInputProfile.h"

#include <string>
#include <vector>

constexpr int kLegendCols = 32; // 720 / FONT_CHAR_WIDTH (22), rounded down
constexpr int kLegendRows = 24; // 720 / FONT_CHAR_HEIGHT (30)
constexpr int kLegendPages = 2;

// Short label of a Trimui control as printed on the Smart Pro ("A" is the
// east face button, "B" the south one); "--" for Count (unbound).
std::string LegendControlLabel(TrimuiControl control);

// The lines of one page (0-based), each at most kLegendCols characters and at
// most kLegendRows lines. The last line is the "Quit PicoTracker" item.
std::vector<std::string> BuildLegendPage(const HostInputProfile &profile,
                                         int page);

// input.ini at `path` parsed like SDLJoystickSource does (so the legend shows
// what the joystick uses); the compiled defaults when it is missing.
HostInputProfile LoadLegendProfile(const std::string &path);
