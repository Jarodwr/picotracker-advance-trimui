/*
 * Copyright (c) 2026 Jarod Wright and contributors
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

// Host implementation of the firmware's record.h on top of HostAudioCapture.
// A worker thread drains the capture ring into RAM (at most 30 s, like the
// firmware's record RAM), then writes the WAV through the installed
// FileSystem on stop. Without a configured capture the first source use opens
// the SDL capture device; if that fails, StartRecording returns false.

#include "HostAudioCapture.h"

#include <cstdint>
#include <memory>

// Replace the capture used by recording (tests inject a fake-backed one). Stops
// any session first. Pass nullptr to go back to the lazily opened SDL capture.
void HostRecordSetCapture(std::unique_ptr<HostAudioCapture> capture);

// Gain applied to recorded samples, in dB, after clamping to record.h's limits
// for the active source. Exposed for checks.
int32_t HostRecordLineInGainDb();
int32_t HostRecordMicGainDb();
