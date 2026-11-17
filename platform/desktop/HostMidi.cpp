// Copyright (c) 2026 Jarod Wright and contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#include "HostMidi.h"

HostMidiService::HostMidiService(MidiInDevice *input, MidiOutDevice *output0,
                                 MidiOutDevice *output1)
    : nullIn_{HostNullMidiIn("No MIDI in"), HostNullMidiIn("No MIDI in")},
      nullOut_{HostNullMidiOut("No MIDI out"), HostNullMidiOut("No MIDI out")} {
  outList_.push_back(output0 ? output0 : &nullOut_[0]);
  outList_.push_back(output1 ? output1 : &nullOut_[1]);
  inList_.push_back(input ? input : &nullIn_[0]);
  inList_.push_back(&nullIn_[1]);
}

void HostMidiService::Poll() {
  for (auto *device : inList_)
    if (device) device->poll();
  if (hook_) hook_();
}
