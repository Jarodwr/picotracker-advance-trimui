// Copyright (c) 2026 Jarod Wright and contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#include "HostLegend.h"

#include <fstream>
#include <sstream>

namespace {

std::string Pad(std::string s, size_t width) {
  if (s.size() < width) s.append(width - s.size(), ' ');
  return s;
}

// Right-aligns `right` in a full-width line after `left`.
std::string Title(const std::string &left, const std::string &right) {
  std::string line = left;
  if (line.size() + right.size() < static_cast<size_t>(kLegendCols))
    line.append(kLegendCols - line.size() - right.size(), ' ');
  return line + right;
}

std::string Clip(const std::string &s) {
  return s.size() > static_cast<size_t>(kLegendCols) ? s.substr(0, kLegendCols)
                                                     : s;
}

struct Row {
  GUIEventPadButtonType button;
  const char *advance;
  const char *does;
};

// One row per Advance pad button, in the order of the controls table.
const Row kRows[] = {
    {EPBT_UP, "Up", "Cursor up"},
    {EPBT_DOWN, "Down", "Cursor down"},
    {EPBT_LEFT, "Left", "Cursor left"},
    {EPBT_RIGHT, "Right", "Cursor right"},
    {EPBT_A, "ENTER", "Enter / insert"},
    {EPBT_B, "EDIT", "Edit / copy"},
    {EPBT_L, "ALT", "Modifier key"},
    {EPBT_R, "NAV", "Screens modifier"},
    {EPBT_START, "PLAY", "Play / stop"},
    {EPBT_SELECT, "-", "Not used"},
};

std::string DpadLabel(TrimuiControl c) {
  switch (c) {
  case TrimuiControl::DpadUp: return "D-up";
  case TrimuiControl::DpadDown: return "D-down";
  case TrimuiControl::DpadLeft: return "D-left";
  case TrimuiControl::DpadRight: return "D-right";
  default: return "";
  }
}

std::string Label(const HostInputProfile &p, GUIEventPadButtonType b) {
  return LegendControlLabel(p.ControlFor(b));
}

std::vector<std::string> ControlsPage(const HostInputProfile &p) {
  std::vector<std::string> lines;
  lines.push_back(Title("HOME: CONTROLS", "L/R 1/2"));
  lines.push_back("");
  lines.push_back("Pad     Adv    Does");
  for (const auto &row : kRows)
    lines.push_back(Clip(Pad(Label(p, row.button), 8) + Pad(row.advance, 7) +
                         row.does));
  lines.push_back("");
  lines.push_back("Quit at once: hold");
  lines.push_back("Select+Start+L1+R1 (saves)");
  lines.push_back("Home or B: close this screen");
  lines.push_back("Menu / Select+Start: open it");
  lines.push_back("");
  lines.push_back("> Quit PicoTracker (A)");
  return lines;
}

std::vector<std::string> CombosPage(const HostInputProfile &p) {
  std::vector<std::string> lines;
  lines.push_back(Title("HOME: COMBOS", "L/R 2/2"));
  lines.push_back("");
  lines.push_back(Clip("NAV=" + Label(p, EPBT_R) + " ALT=" + Label(p, EPBT_L)));
  lines.push_back(
      Clip("ENTER=" + Label(p, EPBT_A) + " EDIT=" + Label(p, EPBT_B)));
  lines.push_back(Clip("PLAY=" + Label(p, EPBT_START)));
  lines.push_back("");
  lines.push_back("Screens:");
  lines.push_back(" NAV+Right/Left next/prev");
  lines.push_back(" Song: NAV+Up Project");
  lines.push_back(" Song: NAV+Down Mixer");
  lines.push_back("");
  lines.push_back("Play / stop:");
  lines.push_back(" PLAY       here");
  lines.push_back(" NAV+PLAY   in song");
  lines.push_back(" hold PLAY  panic stop");
  lines.push_back("");
  lines.push_back("Notes and values:");
  lines.push_back(" ENTER       insert/last");
  lines.push_back(" ENTER twice next unused");
  lines.push_back(" ENTER+arrows +-1 / +-10");
  lines.push_back(" ALT+ENTER   cut / paste");
  lines.push_back(" ALT+EDIT    select");
  lines.push_back("");
  lines.push_back("> Quit PicoTracker (A)");
  return lines;
}

} // namespace

std::string LegendControlLabel(TrimuiControl control) {
  const std::string dpad = DpadLabel(control);
  if (!dpad.empty()) return dpad;
  switch (control) {
  case TrimuiControl::East: return "A";
  case TrimuiControl::South: return "B";
  case TrimuiControl::West: return "X";
  case TrimuiControl::North: return "Y";
  case TrimuiControl::L1: return "L1";
  case TrimuiControl::R1: return "R1";
  case TrimuiControl::L2: return "L2";
  case TrimuiControl::R2: return "R2";
  case TrimuiControl::Start: return "Start";
  case TrimuiControl::Select: return "Select";
  case TrimuiControl::Menu: return "Menu";
  case TrimuiControl::L3: return "L3";
  case TrimuiControl::R3: return "R3";
  default: return "--";
  }
}

std::vector<std::string> BuildLegendPage(const HostInputProfile &profile,
                                         int page) {
  return page == 0 ? ControlsPage(profile) : CombosPage(profile);
}

HostInputProfile LoadLegendProfile(const std::string &path) {
  std::ifstream file(path, std::ios::binary);
  if (!file) return HostInputProfile::Defaults();
  std::stringstream text;
  text << file.rdbuf();
  return ParseHostInputProfile(text.str()).profile;
}
