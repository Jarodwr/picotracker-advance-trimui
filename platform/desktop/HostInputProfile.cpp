// Copyright (c) 2026 Jarod Wright and contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#include "HostInputProfile.h"

#include <cctype>

namespace {

struct ControlName {
  const char *name;
  TrimuiControl control;
};

const ControlName kControlNames[] = {
    {"dpad_up", TrimuiControl::DpadUp},
    {"dpad_down", TrimuiControl::DpadDown},
    {"dpad_left", TrimuiControl::DpadLeft},
    {"dpad_right", TrimuiControl::DpadRight},
    {"east", TrimuiControl::East},
    {"south", TrimuiControl::South},
    {"west", TrimuiControl::West},
    {"north", TrimuiControl::North},
    {"l1", TrimuiControl::L1},
    {"r1", TrimuiControl::R1},
    {"l2", TrimuiControl::L2},
    {"r2", TrimuiControl::R2},
    {"start", TrimuiControl::Start},
    {"select", TrimuiControl::Select},
    {"menu", TrimuiControl::Menu},
    {"l3", TrimuiControl::L3},
    {"r3", TrimuiControl::R3},
};

// ADR 0016 section 1 defaults, in AdvanceNames() order.
const TrimuiControl kDefaultControls[] = {
    TrimuiControl::DpadUp, TrimuiControl::DpadDown, TrimuiControl::DpadLeft,
    TrimuiControl::DpadRight, TrimuiControl::East, TrimuiControl::South,
    TrimuiControl::L1,       TrimuiControl::R1,    TrimuiControl::Start,
    TrimuiControl::Select,
};

std::string Lower(std::string s) {
  for (auto &c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}

std::string Trim(const std::string &s) {
  size_t b = 0, e = s.size();
  while (b < e && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
  while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
  return s.substr(b, e - b);
}

bool ParseBool(const std::string &v, bool &out) {
  if (v == "true" || v == "1" || v == "yes" || v == "on") { out = true; return true; }
  if (v == "false" || v == "0" || v == "no" || v == "off") { out = false; return true; }
  return false;
}

} // namespace

const std::vector<HostInputName> &AdvanceNames() {
  static const std::vector<HostInputName> names = {
      {"up", EPBT_UP},       {"down", EPBT_DOWN}, {"left", EPBT_LEFT},
      {"right", EPBT_RIGHT}, {"a", EPBT_A},       {"b", EPBT_B},
      {"l", EPBT_L},         {"r", EPBT_R},       {"start", EPBT_START},
      {"select", EPBT_SELECT},
  };
  return names;
}

const char *TrimuiControlName(TrimuiControl control) {
  for (const auto &c : kControlNames)
    if (c.control == control) return c.name;
  return "";
}

bool ParseTrimuiControl(const std::string &name, TrimuiControl &out) {
  const std::string n = Lower(name);
  for (const auto &c : kControlNames) {
    if (n == c.name) { out = c.control; return true; }
  }
  return false;
}

HostInputProfile HostInputProfile::Defaults() {
  HostInputProfile p;
  const auto &names = AdvanceNames();
  for (size_t i = 0; i < names.size(); ++i)
    p.entries.push_back({names[i].button, kDefaultControls[i], false});
  return p;
}

TrimuiControl HostInputProfile::ControlFor(GUIEventPadButtonType button) const {
  for (const auto &e : entries)
    if (e.button == button) return e.unbound ? TrimuiControl::Count : e.control;
  return TrimuiControl::Count;
}

HostInputParseResult ParseHostInputProfile(const std::string &text) {
  HostInputParseResult result;
  result.profile = HostInputProfile::Defaults();
  std::vector<bool> seen(result.profile.entries.size(), false);
  bool seenStick = false;

  auto reject = [&](int line, const std::string &t, const char *why) {
    result.rejected.push_back({line, t, why});
  };

  size_t pos = 0;
  int lineNo = 0;
  // Skip a UTF-8 byte order mark.
  if (text.compare(0, 3, "\xEF\xBB\xBF") == 0) pos = 3;
  while (pos <= text.size()) {
    size_t nl = text.find('\n', pos);
    if (nl == std::string::npos) nl = text.size();
    ++lineNo;
    const std::string line = Trim(text.substr(pos, nl - pos));
    pos = nl + 1;
    if (line.empty() || line[0] == '#' || line[0] == ';') continue;
    if (line[0] == '[') continue; // section headers carry no meaning

    const size_t eq = line.find('=');
    if (eq == std::string::npos) { reject(lineNo, line, "missing '='"); continue; }
    const std::string key = Lower(Trim(line.substr(0, eq)));
    const std::string value = Lower(Trim(line.substr(eq + 1)));
    if (key.empty()) { reject(lineNo, line, "empty name"); continue; }
    if (value.empty()) { reject(lineNo, line, "empty value"); continue; }

    if (key == "left_stick_as_dpad") {
      bool b = false;
      if (!ParseBool(value, b)) { reject(lineNo, line, "invalid boolean"); continue; }
      if (seenStick) { reject(lineNo, line, "duplicate key"); continue; }
      seenStick = true;
      result.profile.leftStickAsDpad = b;
      continue;
    }

    const auto &names = AdvanceNames();
    size_t idx = names.size();
    for (size_t i = 0; i < names.size(); ++i)
      if (key == names[i].name) idx = i;
    if (idx == names.size()) { reject(lineNo, line, "unknown Advance button"); continue; }

    HostInputProfile::Entry candidate = result.profile.entries[idx];
    if (value == "none") {
      candidate.unbound = true;
    } else if (ParseTrimuiControl(value, candidate.control)) {
      candidate.unbound = false;
    } else {
      reject(lineNo, line, "unknown Trimui control");
      continue;
    }
    if (seen[idx]) { reject(lineNo, line, "duplicate key"); continue; }
    seen[idx] = true;
    result.profile.entries[idx] = candidate;
  }
  return result;
}
