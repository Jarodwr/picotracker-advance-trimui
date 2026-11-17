// Copyright (c) 2026 Jarod Wright and contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#include "CoreMidiIn.h"

#include <algorithm>

#include <CoreMIDI/CoreMIDI.h>
#include <CoreFoundation/CoreFoundation.h>
#include <mach/mach_time.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <thread>

// The packet-list API is deprecated in favour of event lists, but it hands us
// the raw MIDI 1.0 byte stream the upstream parser consumes.
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

namespace {

constexpr size_t kRingSize = 8192; // power of two
constexpr auto kRescanInterval = std::chrono::seconds(1);

struct RingEntry {
  uint32_t timestampUs;
  uint8_t byte;
};

uint32_t HostTimeToMicros(MIDITimeStamp hostTime) {
  static const mach_timebase_info_data_t timebase = [] {
    mach_timebase_info_data_t info;
    mach_timebase_info(&info);
    return info;
  }();
  if (hostTime == 0)
    hostTime = mach_absolute_time();
  unsigned __int128 ns = (unsigned __int128)hostTime * timebase.numer /
                         timebase.denom;
  // Wraps every ~71 minutes; the firmware only compares nearby timestamps.
  return (uint32_t)(ns / 1000);
}

std::string CFToString(CFStringRef text) {
  if (text == nullptr)
    return {};
  char buffer[256];
  if (!CFStringGetCString(text, buffer, sizeof buffer, kCFStringEncodingUTF8))
    return {};
  return buffer;
}

std::string EndpointName(MIDIEndpointRef endpoint) {
  CFStringRef text = nullptr;
  std::string name;
  if (MIDIObjectGetStringProperty(endpoint, kMIDIPropertyDisplayName, &text) ==
          noErr &&
      text != nullptr) {
    name = CFToString(text);
    CFRelease(text);
  }
  return name;
}

} // namespace

// State shared with CoreMIDI callbacks. It outlives the CoreMIDI client: the
// owner only frees it after the client (and with it every port) is disposed.
struct CoreMidiInShared {
  RingEntry ring[kRingSize];
  std::atomic<size_t> head{0}; // written by producers under lock_
  std::atomic<size_t> tail{0}; // written by the consumer
  std::atomic_flag lock = ATOMIC_FLAG_INIT;
  std::atomic<bool> accepting{false};
  std::atomic<int> inFlight{0};
  std::atomic<unsigned> dropped{0};
  std::atomic<bool> devicesChanged{false};

  // Copies one packet's bytes. A packet that does not fit is dropped whole so
  // the parser never sees half a message from an overflow.
  void Push(const MIDIPacket &packet) {
    uint32_t ts = HostTimeToMicros(packet.timeStamp);
    while (lock.test_and_set(std::memory_order_acquire)) {
    }
    size_t h = head.load(std::memory_order_relaxed);
    size_t t = tail.load(std::memory_order_acquire);
    if (kRingSize - (h - t) < packet.length) {
      dropped.fetch_add(packet.length, std::memory_order_relaxed);
    } else {
      for (UInt16 i = 0; i < packet.length; ++i)
        ring[(h + i) & (kRingSize - 1)] = {ts, packet.data[i]};
      head.store(h + packet.length, std::memory_order_release);
    }
    lock.clear(std::memory_order_release);
  }

  bool Pop(RingEntry &out) {
    size_t t = tail.load(std::memory_order_relaxed);
    if (t == head.load(std::memory_order_acquire))
      return false;
    out = ring[t & (kRingSize - 1)];
    tail.store(t + 1, std::memory_order_release);
    return true;
  }

  void Clear() { tail.store(head.load(std::memory_order_acquire)); }
};

struct CoreMidiIn::Impl {
  CoreMidiInOptions options;
  CoreMidiInShared shared;
  MIDIClientRef client = 0;
  MIDIPortRef port = 0;
  MIDIEndpointRef virtualDestination = 0;
  MIDIEndpointRef connected = 0;
  std::string connectedName;
  bool started = false;
  std::chrono::steady_clock::time_point lastScan{};

  static void ReadProc(const MIDIPacketList *list, void *refCon, void *) {
    auto *shared = static_cast<CoreMidiInShared *>(refCon);
    shared->inFlight.fetch_add(1);
    if (shared->accepting.load()) {
      const MIDIPacket *packet = &list->packet[0];
      for (UInt32 i = 0; i < list->numPackets; ++i) {
        shared->Push(*packet);
        packet = MIDIPacketNext(packet);
      }
    }
    shared->inFlight.fetch_sub(1);
  }

  static void NotifyProc(const MIDINotification *message, void *refCon) {
    auto *shared = static_cast<CoreMidiInShared *>(refCon);
    switch (message->messageID) {
    case kMIDIMsgSetupChanged:
    case kMIDIMsgObjectAdded:
    case kMIDIMsgObjectRemoved:
      shared->devicesChanged.store(true);
      break;
    default:
      break;
    }
  }

  // Waits for callbacks that already started, after accepting was cleared.
  void Quiesce() {
    shared.accepting.store(false);
    for (int i = 0; i < 1000 && shared.inFlight.load() != 0; ++i)
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }

  MIDIEndpointRef FindSource() const {
    ItemCount count = MIDIGetNumberOfSources();
    for (ItemCount i = 0; i < count; ++i) {
      MIDIEndpointRef source = MIDIGetSource(i);
      if (source == 0)
        continue;
      const std::string name = EndpointName(source);
      if (std::find(options.excludeNames.begin(), options.excludeNames.end(),
                    name) != options.excludeNames.end())
        continue;
      if (options.sourceName.empty() || name == options.sourceName)
        return source;
    }
    return 0;
  }

  bool SourceExists(MIDIEndpointRef endpoint) const {
    ItemCount count = MIDIGetNumberOfSources();
    for (ItemCount i = 0; i < count; ++i)
      if (MIDIGetSource(i) == endpoint)
        return true;
    return false;
  }

  void Disconnect() {
    if (connected != 0) {
      MIDIPortDisconnectSource(port, connected);
      connected = 0;
      connectedName.clear();
    }
  }

  // Reconciles the connection with the sources that exist right now.
  void Rescan() {
    lastScan = std::chrono::steady_clock::now();
    if (options.virtualOnly || port == 0)
      return;
    if (connected != 0 && !SourceExists(connected))
      Disconnect();
    if (connected != 0)
      return;
    MIDIEndpointRef source = FindSource();
    if (source == 0)
      return;
    if (MIDIPortConnectSource(port, source, &shared) == noErr) {
      connected = source;
      connectedName = EndpointName(source);
    }
  }
};

CoreMidiIn::CoreMidiIn(const char *name, CoreMidiInOptions options)
    : MidiInDevice(name), impl_(new Impl) {
  impl_->options = std::move(options);
}

CoreMidiIn::~CoreMidiIn() { closeDriver(); }

bool CoreMidiIn::Start() { return MidiInDevice::Start(); }

void CoreMidiIn::Stop() { MidiInDevice::Stop(); }

bool CoreMidiIn::initDriver() {
  Impl &s = *impl_;
  if (s.client != 0)
    return true;
  CFStringRef clientName = CFSTR("picoTracker MIDI In");
  if (MIDIClientCreate(clientName, Impl::NotifyProc, &s.shared, &s.client) !=
      noErr) {
    s.client = 0;
    return false;
  }
  if (!s.options.virtualOnly &&
      MIDIInputPortCreate(s.client, CFSTR("Input"), Impl::ReadProc, &s.shared,
                          &s.port) != noErr) {
    closeDriver();
    return false;
  }
  if (!s.options.virtualName.empty()) {
    CFStringRef virtualName = CFStringCreateWithCString(
        nullptr, s.options.virtualName.c_str(), kCFStringEncodingUTF8);
    OSStatus status = MIDIDestinationCreate(
        s.client, virtualName, Impl::ReadProc, &s.shared,
        &s.virtualDestination);
    CFRelease(virtualName);
    if (status != noErr) {
      closeDriver();
      return false;
    }
  }
  return true;
}

void CoreMidiIn::closeDriver() {
  Impl &s = *impl_;
  s.Quiesce();
  s.connected = 0;
  s.connectedName.clear();
  // Disposing the client disposes its ports and virtual endpoint, after which
  // CoreMIDI no longer calls ReadProc or NotifyProc.
  if (s.client != 0) {
    MIDIClientDispose(s.client);
    s.client = 0;
    s.port = 0;
    s.virtualDestination = 0;
  }
  s.shared.Clear();
}

bool CoreMidiIn::startDriver() {
  Impl &s = *impl_;
  if (s.client == 0)
    return false;
  s.shared.Clear();
  s.shared.accepting.store(true);
  s.started = true;
  s.Rescan();
  // A missing source is not an error: it can appear later and poll() connects.
  return true;
}

void CoreMidiIn::stopDriver() {
  Impl &s = *impl_;
  s.started = false;
  s.Quiesce();
  s.Disconnect();
  s.shared.Clear();
}

void CoreMidiIn::poll() {
  Impl &s = *impl_;
  if (!s.started)
    return;
  // Notifications need a run loop on the creating thread, which the firmware
  // thread may not run, so a slow rescan backs up the change flag.
  bool changed = s.shared.devicesChanged.exchange(false);
  if (changed || std::chrono::steady_clock::now() - s.lastScan >=
                     kRescanInterval)
    s.Rescan();
  RingEntry entry;
  while (s.shared.Pop(entry)) {
    processMidiData(entry.byte, entry.timestampUs);
    if (!s.started)
      break; // a handler stopped the device
  }
}

bool CoreMidiIn::IsConnected() const {
  return impl_->connected != 0 || (impl_->options.virtualOnly &&
                                   impl_->virtualDestination != 0);
}

std::string CoreMidiIn::ConnectedSourceName() const {
  return impl_->connectedName;
}

unsigned CoreMidiIn::DroppedBytes() const {
  return impl_->shared.dropped.load();
}

std::vector<std::string> CoreMidiIn::ListSources() {
  std::vector<std::string> names;
  ItemCount count = MIDIGetNumberOfSources();
  for (ItemCount i = 0; i < count; ++i)
    names.push_back(EndpointName(MIDIGetSource(i)));
  return names;
}
