// Copyright (c) 2026 Jarod Wright and contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#include "CoreMidiOut.h"

#include <algorithm>

#include <CoreMIDI/CoreMIDI.h>
#include <CoreFoundation/CoreFoundation.h>

#include <chrono>
#include <cstdint>

#pragma clang diagnostic ignored "-Wdeprecated-declarations"

namespace {

constexpr auto kRetryInterval = std::chrono::seconds(1);

std::string EndpointName(MIDIEndpointRef endpoint) {
  CFStringRef text = nullptr;
  std::string name;
  if (MIDIObjectGetStringProperty(endpoint, kMIDIPropertyDisplayName, &text) ==
          noErr &&
      text != nullptr) {
    char buffer[256];
    if (CFStringGetCString(text, buffer, sizeof buffer,
                           kCFStringEncodingUTF8))
      name = buffer;
    CFRelease(text);
  }
  return name;
}

// Wire length for a message, from its status byte. Returns 0 for status bytes
// that cannot be sent as a short message.
size_t EncodeMessage(const MidiMessage &m, uint8_t out[3]) {
  uint8_t status = m.status_;
  if (status < 0x80)
    return 0;
  size_t length;
  if (status >= 0xF8)
    length = 1;
  else if (status >= 0xF0) {
    // System common: only tune request and song position/select are short.
    switch (status) {
    case 0xF1:
    case 0xF3:
      length = 2;
      break;
    case 0xF2:
      length = 3;
      break;
    case 0xF6:
      length = 1;
      break;
    default:
      return 0; // SysEx start/end travel through SendRawBytes
    }
  } else if (status >= 0xC0 && status < 0xE0)
    length = 2;
  else
    length = 3;
  out[0] = status;
  out[1] = m.data1_ == MidiMessage::UNUSED_BYTE ? 0 : (m.data1_ & 0x7F);
  out[2] = m.data2_ == MidiMessage::UNUSED_BYTE ? 0 : (m.data2_ & 0x7F);
  return length;
}

} // namespace

struct CoreMidiOut::Impl {
  CoreMidiOutOptions options;
  MIDIClientRef client = 0;
  MIDIPortRef port = 0;
  MIDIEndpointRef virtualSource = 0;
  MIDIEndpointRef destination = 0;
  std::string destinationName;
  bool started = false;
  std::chrono::steady_clock::time_point lastScan{};

  bool IsVirtual() const { return virtualSource != 0; }
  bool Available() const { return IsVirtual() || destination != 0; }

  bool DestinationExists(MIDIEndpointRef endpoint) const {
    ItemCount count = MIDIGetNumberOfDestinations();
    for (ItemCount i = 0; i < count; ++i)
      if (MIDIGetDestination(i) == endpoint)
        return true;
    return false;
  }

  void Rescan() {
    lastScan = std::chrono::steady_clock::now();
    if (IsVirtual() || client == 0)
      return;
    if (destination != 0 && !DestinationExists(destination)) {
      destination = 0;
      destinationName.clear();
    }
    if (destination != 0)
      return;
    ItemCount count = MIDIGetNumberOfDestinations();
    for (ItemCount i = 0; i < count; ++i) {
      MIDIEndpointRef candidate = MIDIGetDestination(i);
      if (candidate == 0)
        continue;
      std::string name = EndpointName(candidate);
      if (std::find(options.excludeNames.begin(), options.excludeNames.end(),
                    name) != options.excludeNames.end())
        continue;
      if (options.destinationName.empty() || name == options.destinationName) {
        destination = candidate;
        destinationName = name;
        return;
      }
    }
  }

  // Sends a ready packet list. A failure means the endpoint went away, so the
  // device is marked unavailable until a rescan finds it again.
  void Transmit(const MIDIPacketList *list) {
    OSStatus status = noErr;
    if (IsVirtual())
      status = MIDIReceived(virtualSource, list);
    else if (destination != 0)
      status = MIDISend(port, destination, list);
    if (status != noErr && !IsVirtual()) {
      destination = 0;
      destinationName.clear();
    }
  }

  // Retries the connection at a bounded rate. Returns true when sendable.
  bool Ready() {
    if (!started || client == 0)
      return false;
    if (!Available() &&
        std::chrono::steady_clock::now() - lastScan >= kRetryInterval)
      Rescan();
    return Available();
  }
};

CoreMidiOut::CoreMidiOut(const char *name, CoreMidiOutOptions options)
    : MidiOutDevice(name), impl_(new Impl) {
  impl_->options = std::move(options);
}

CoreMidiOut::~CoreMidiOut() { Close(); }

bool CoreMidiOut::Init() {
  Impl &s = *impl_;
  if (s.client != 0)
    return true;
  if (MIDIClientCreate(CFSTR("picoTracker MIDI Out"), nullptr, nullptr,
                       &s.client) != noErr) {
    s.client = 0;
    return false;
  }
  if (!s.options.virtualName.empty()) {
    CFStringRef virtualName = CFStringCreateWithCString(
        nullptr, s.options.virtualName.c_str(), kCFStringEncodingUTF8);
    OSStatus status =
        MIDISourceCreate(s.client, virtualName, &s.virtualSource);
    CFRelease(virtualName);
    if (status != noErr) {
      Close();
      return false;
    }
  } else if (MIDIOutputPortCreate(s.client, CFSTR("Output"), &s.port) !=
             noErr) {
    Close();
    return false;
  }
  s.Rescan();
  return true;
}

void CoreMidiOut::Close() {
  Impl &s = *impl_;
  s.started = false;
  if (s.client != 0) {
    MIDIClientDispose(s.client); // also disposes the port and virtual source
    s.client = 0;
  }
  s.port = 0;
  s.virtualSource = 0;
  s.destination = 0;
  s.destinationName.clear();
}

bool CoreMidiOut::Start() {
  if (impl_->client == 0)
    return false;
  impl_->started = true;
  impl_->Rescan();
  return true; // no destination yet is not an error; sends drop until found
}

void CoreMidiOut::Stop() { impl_->started = false; }

void CoreMidiOut::Poll() {
  if (impl_->started)
    impl_->Rescan();
}

void CoreMidiOut::SendMessage(MidiMessage &m) {
  Impl &s = *impl_;
  if (!s.Ready())
    return;
  uint8_t bytes[3];
  size_t length = EncodeMessage(m, bytes);
  if (length == 0)
    return;
  alignas(MIDIPacketList) Byte buffer[64];
  auto *list = reinterpret_cast<MIDIPacketList *>(buffer);
  MIDIPacket *packet = MIDIPacketListInit(list);
  MIDIPacketListAdd(list, sizeof buffer, packet, 0, length, bytes);
  s.Transmit(list); // timestamp 0 means "now"; MidiMessage carries none
}

void CoreMidiOut::SendQueue(
    etl::vector<MidiMessage, MIDI_MAX_MESG_QUEUE> &queue) {
  Impl &s = *impl_;
  if (!s.Ready())
    return;
  alignas(MIDIPacketList) Byte buffer[1024];
  auto *list = reinterpret_cast<MIDIPacketList *>(buffer);
  MIDIPacket *packet = MIDIPacketListInit(list);
  for (auto &message : queue) {
    uint8_t bytes[3];
    size_t length = EncodeMessage(message, bytes);
    if (length == 0)
      continue;
    MIDIPacket *next =
        MIDIPacketListAdd(list, sizeof buffer, packet, 0, length, bytes);
    if (next == nullptr) {
      // List full: flush it and start a new one, preserving message order.
      s.Transmit(list);
      if (!s.Available())
        return;
      packet = MIDIPacketListInit(list);
      next = MIDIPacketListAdd(list, sizeof buffer, packet, 0, length, bytes);
    }
    packet = next;
  }
  if (list->numPackets > 0)
    s.Transmit(list);
}

void CoreMidiOut::SendRawBytes(const uint8_t *data, uint8_t len) {
  Impl &s = *impl_;
  if (data == nullptr || len == 0 || !s.Ready())
    return;
  alignas(MIDIPacketList) Byte buffer[512];
  auto *list = reinterpret_cast<MIDIPacketList *>(buffer);
  MIDIPacket *packet = MIDIPacketListInit(list);
  if (MIDIPacketListAdd(list, sizeof buffer, packet, 0, len, data) != nullptr)
    s.Transmit(list);
}

bool CoreMidiOut::IsAvailable() const {
  return impl_->started && impl_->Available();
}

std::string CoreMidiOut::ConnectedDestinationName() const {
  return impl_->IsVirtual() ? impl_->options.virtualName
                            : impl_->destinationName;
}

std::vector<std::string> CoreMidiOut::ListDestinations() {
  std::vector<std::string> names;
  ItemCount count = MIDIGetNumberOfDestinations();
  for (ItemCount i = 0; i < count; ++i)
    names.push_back(EndpointName(MIDIGetDestination(i)));
  return names;
}
