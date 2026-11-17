/*
 * Copyright (c) 2026 Jarod Wright and contributors
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "Services/Midi/MidiInDevice.h"
#include "Services/Midi/MidiOutDevice.h"
#include "Services/Midi/MidiService.h"

#include <functional>
#include <string>

// Output slot with no device behind it. Messages are dropped and the slot
// identifies itself as unavailable, so an old saved "output" setting that
// selects it can never reach anything.
class HostNullMidiOut final : public MidiOutDevice {
public:
  explicit HostNullMidiOut(const char *name = "No MIDI out")
      : MidiOutDevice(name) {}
  bool Init() override { return true; }
  void Close() override {}
  bool Start() override { return true; }
  void Stop() override {}
  void SendQueue(etl::vector<MidiMessage, MIDI_MAX_MESG_QUEUE> &) override {}
  void SendMessage(MidiMessage &) override {}
  void SendRawBytes(const uint8_t *, uint8_t) override {}
};

class HostNullMidiIn final : public MidiInDevice {
public:
  explicit HostNullMidiIn(const char *name = "No MIDI in")
      : MidiInDevice(name) {}
  bool Start() override { return MidiInDevice::Start(); }
  void Stop() override { MidiInDevice::Stop(); }
  void poll() override {}

protected:
  bool initDriver() override { return true; }
  void closeDriver() override {}
  bool startDriver() override { return true; }
  void stopDriver() override {}
  MidiInputSource getInputSource() const override {
    return MidiInputSource::Usb;
  }
};

// Availability of MIDI for diagnostics and settings.
struct HostMidiStatus {
  bool inputAvailable = false;
  bool outputAvailable = false;
  std::string inputName;  // connected endpoint, empty if none
  std::string outputName; // connected endpoint, empty if none
};

// MidiService with a fixed device layout. Upstream selects output slot 0,
// slot 1 or both from a saved setting without checking the list length, so
// both slots are always populated; a slot without a device is a null device.
// Devices are owned by the caller and must outlive the service.
class HostMidiService final : public MidiService {
public:
  // Any pointer may be null, giving a null device in that slot.
  HostMidiService(MidiInDevice *input = nullptr, MidiOutDevice *output0 = nullptr,
                  MidiOutDevice *output1 = nullptr);

  // Firmware-thread pump: drains device callbacks into upstream parsing.
  void Poll();
  // Called from Poll() after the input devices, e.g. to rescan outputs.
  void SetPollHook(std::function<void()> hook) { hook_ = std::move(hook); }

private:
  HostNullMidiIn nullIn_[2];
  HostNullMidiOut nullOut_[2];
  std::function<void()> hook_;
};
