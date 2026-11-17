/*
 * Copyright (c) 2018 Discodirt
 * Copyright (c) 2024 xiphonics, inc.
 * Copyright (c) 2026 Jarod Wright and contributors
 *
 * SPDX-License-Identifier: BSD-3-Clause AND GPL-3.0-only
 *
 * BSD-3-Clause applies to the declarations mirrored from the upstream picoTracker
 * firmware; GPL-3.0-only applies to the wrapper's own additions. Both apply to
 * the file as a whole.
 */

#pragma once

#include <cstdint>

enum RecordSource { AllOff, LineIn, Mic, USBIn };

constexpr int LINEIN_GAIN_MINDB = -6;
constexpr int LINEIN_GAIN_MAXDB = 24;
constexpr int MIC_GAIN_MINDB = 0;
constexpr int MIC_GAIN_MAXDB = 20;

bool StartRecording(const char *filename, uint8_t threshold, uint32_t milliseconds);
void StopRecording();
void CancelRecording();
void RequestStopRecording();
bool WaitForRecordingStop(uint32_t timeoutMs);
void FinishStopRecording();
void StartMonitoring();
void StopMonitoring();
void SetInputSource(RecordSource source);
void SetLineInGain(int32_t gainDb);
void SetMicGain(int32_t gainDb);
bool IsRecordingActive();
bool IsSavingRecording();
uint8_t GetSavingProgressPercent();
bool DidLastRecordingCaptureAudio();
