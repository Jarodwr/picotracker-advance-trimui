// Copyright (c) 2026 Jarod Wright and contributors
//
// SPDX-License-Identifier: GPL-3.0-only

// C-64: content checks for the Home screen legend. Linked into
// host_quit_dialog_check (which calls HostLegendChecks, so check-macos.sh and
// check-linux.sh run it) and buildable alone with HOST_LEGEND_STANDALONE.

#include "HostLegend.h"

#include <cstdio>
#include <cstdlib>
#include <string>

#define LEGEND_CHECK(cond)                                                     \
  do {                                                                         \
    if (!(cond)) {                                                             \
      std::fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__,    \
                   #cond);                                                     \
      std::exit(1);                                                            \
    }                                                                          \
  } while (0)

namespace {

std::string Joined(const std::vector<std::string> &lines) {
  std::string all;
  for (const auto &l : lines) all += l + "\n";
  return all;
}

int Count(const std::string &text, const std::string &what) {
  int n = 0;
  for (size_t at = text.find(what); at != std::string::npos;
       at = text.find(what, at + 1))
    ++n;
  return n;
}

void NothingOverflows() {
  // Defaults and a profile that uses the widest labels everywhere.
  auto wide = HostInputProfile::Defaults();
  for (auto &e : wide.entries) e.control = TrimuiControl::DpadRight;
  for (const auto &profile : {HostInputProfile::Defaults(), wide}) {
    for (int page = 0; page < kLegendPages; ++page) {
      const auto lines = BuildLegendPage(profile, page);
      LEGEND_CHECK(static_cast<int>(lines.size()) <= kLegendRows);
      for (const auto &line : lines)
        LEGEND_CHECK(static_cast<int>(line.size()) <= kLegendCols);
      // Every page ends with the Quit item.
      LEGEND_CHECK(lines.back() == "> Quit PicoTracker (A)");
    }
  }
}

void EveryMappedControlAppearsOnce() {
  const auto profile = HostInputProfile::Defaults();
  const auto page = Joined(BuildLegendPage(profile, 0));
  // Each default Smart Pro control has one row on page 1. "A" and "B" are
  // matched as whole words in the label column.
  for (const auto &entry : profile.entries) {
    const std::string label = LegendControlLabel(entry.control);
    int rows = 0;
    for (const auto &line : BuildLegendPage(profile, 0))
      if (line.compare(0, label.size() + 1, label + " ") == 0) ++rows;
    LEGEND_CHECK(rows == 1);
  }
  LEGEND_CHECK(Count(page, "ENTER") == 1 && Count(page, "EDIT") == 1);
  LEGEND_CHECK(Count(page, "PLAY") == 1 && Count(page, "NAV") == 1 &&
               Count(page, "ALT") == 1);
  LEGEND_CHECK(page.find("Select+Start+L1+R1") != std::string::npos);
}

void DefaultsMatchTheFirmwareNames() {
  const auto p = HostInputProfile::Defaults();
  LEGEND_CHECK(BuildLegendPage(p, 0)[3].rfind("D-up", 0) == 0);
  const auto page = Joined(BuildLegendPage(p, 0));
  // A=ENTER, B=EDIT, L1=ALT, R1=NAV, Start=PLAY (EventDispatcher mapping).
  LEGEND_CHECK(page.find("A       ENTER") != std::string::npos);
  LEGEND_CHECK(page.find("B       EDIT") != std::string::npos);
  LEGEND_CHECK(page.find("L1      ALT") != std::string::npos);
  LEGEND_CHECK(page.find("R1      NAV") != std::string::npos);
  LEGEND_CHECK(page.find("Start   PLAY") != std::string::npos);
  const auto combos = Joined(BuildLegendPage(p, 1));
  LEGEND_CHECK(combos.find("NAV=R1 ALT=L1") != std::string::npos);
  LEGEND_CHECK(combos.find("ENTER=A EDIT=B") != std::string::npos);
  LEGEND_CHECK(combos.find("PLAY=Start") != std::string::npos);
}

void RemapChangesTheText() {
  const auto before = BuildLegendPage(HostInputProfile::Defaults(), 0);
  const auto parsed = ParseHostInputProfile("a = north\nr = r2\nselect = none\n");
  LEGEND_CHECK(parsed.rejected.empty());
  const auto after = BuildLegendPage(parsed.profile, 0);
  LEGEND_CHECK(before != after);
  const auto text = Joined(after);
  LEGEND_CHECK(text.find("Y       ENTER") != std::string::npos);
  LEGEND_CHECK(text.find("R2      NAV") != std::string::npos);
  LEGEND_CHECK(text.find("--      -") != std::string::npos); // Select unbound
  LEGEND_CHECK(text.find("A       ENTER") == std::string::npos);
  const auto combos = Joined(BuildLegendPage(parsed.profile, 1));
  LEGEND_CHECK(combos.find("NAV=R2") != std::string::npos);
  LEGEND_CHECK(combos.find("ENTER=Y") != std::string::npos);
}

void MissingFileFallsBackToDefaults() {
  const auto p = LoadLegendProfile("/nonexistent/path/input.ini");
  LEGEND_CHECK(BuildLegendPage(p, 0) ==
               BuildLegendPage(HostInputProfile::Defaults(), 0));
}

} // namespace

void HostLegendChecks() {
  NothingOverflows();
  EveryMappedControlAppearsOnce();
  DefaultsMatchTheFirmwareNames();
  RemapChangesTheText();
  MissingFileFallsBackToDefaults();
}

#ifdef HOST_LEGEND_STANDALONE
int main() {
  HostLegendChecks();
  std::puts("host_legend_check passed");
  return 0;
}
#endif
