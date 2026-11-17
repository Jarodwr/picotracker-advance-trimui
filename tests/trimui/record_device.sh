#!/bin/sh

# Copyright (c) 2026 Jarod Wright and contributors
#
# SPDX-License-Identifier: GPL-3.0-only

# Runs ON the Trimui (busybox sh), started over ssh by tests/trimui/record_check.sh. Plays the
# pak's launch.sh with a virtual gamepad (uinput_inject in the same directory): opens the Record
# screen (Edit+Play = B+Start), starts a recording (Start), records for $1 seconds, stops it
# (Start), then leaves with the Select+Start+L1+R1 chord. Writes everything it learned to $2/.
#
# usage: record_device.sh SECONDS OUT_DIR TOOLS_DIR SOURCE
#   TOOLS_DIR holds uinput_inject (never on the SD card; record_check.sh uses /tmp).
#   SOURCE is the RecordView source for the run: 1 = Line In (stereo file), 2 = Mic (mono file).
#   It is written to RECORDSOURCE in the data root's .config.xml and the file is put back after.
set -u
seconds="$1"
out="$2"
tools="$3"
source_value="$4"
sd=/mnt/SDCARD
pak="$sd/Tools/tg5040/PicoTracker.pak"
data="${USERDATA_PATH:-$sd/.userdata/tg5040}/PicoTracker"
fifo="$tools/pad.fifo"
mkdir -p "$out"

config="$data/.config.xml"
config_saved=0
if [ -f "$config" ]; then cp "$config" "$tools/config.saved" && config_saved=1; fi
set_source() {
  # Rewrites (or adds) the one RECORDSOURCE line; the app reads it at boot.
  if [ -f "$config" ] && grep -q '<RECORDSOURCE ' "$config"; then
    sed "s|<RECORDSOURCE VALUE=\"[0-9]*\"/>|<RECORDSOURCE VALUE=\"$1\"/>|" "$tools/config.saved" >"$config"
  elif [ -f "$config" ]; then
    sed "s|</CONFIG>|    <RECORDSOURCE VALUE=\"$1\"/>\n</CONFIG>|" "$tools/config.saved" >"$config"
  fi
}
restore_config() {
  if [ "$config_saved" = 1 ]; then cp "$tools/config.saved" "$config"; fi
}

mixer() { amixer -c 0 contents 2>/dev/null; }
mixer >"$out/mixer.before"
rm -f "$data/recordings/REC01.wav"

rm -f "$fifo"
mkfifo "$fifo"
"$tools/uinput_inject" "$fifo" >"$out/inject.log" 2>&1 &
inject_pid=$!
i=0
while ! grep -q ready "$out/inject.log" 2>/dev/null && [ "$i" -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
grep -q ready "$out/inject.log" || { echo "virtual gamepad not created"; cat "$out/inject.log"; exit 1; }
pad() { printf '%s\n' "$@" >"$fifo"; }

set_source "$source_value"
# SDL drops joystick events while the app window has no focus unless this is set (the emulated
# smoke needs it too); launch.sh does not export it, so the check does.
export SDL_JOYSTICK_ALLOW_BACKGROUND_EVENTS=1

# The pak's launcher saves and restores the capture mixer around the app, so it is what runs here.
sh "$pak/launch.sh" &
app_pid=$!
sleep 8 # boot and let SDL open the pad

pad "button 0 7" # Edit (B) + Play (Start): Record screen
sleep 3
wc -c <"$data/logs/picotracker.log" >"$out/log.rec_screen"
wc -c <"$data/logs/picotracker.log" >"$out/log.rec_start"
pad "button 7" # Play: start recording
sleep 1
mixer >"$out/mixer.during"
sleep "$seconds"
pad "button 7" # Play: stop and save
sleep 4
wc -c <"$data/logs/picotracker.log" >"$out/log.rec_end"
ls -l "$data/recordings" >"$out/recordings.ls" 2>&1
if [ -f "$data/recordings/REC01.wav" ]; then cp "$data/recordings/REC01.wav" "$out/REC01.wav"; fi

# Leave: the chord is repeated because the first one can arrive while the app is on another screen.
tries=0
while kill -0 "$app_pid" 2>/dev/null && [ "$tries" -lt 15 ]; do
  pad chord
  tries=$((tries + 1))
  sleep 2
done
if kill -0 "$app_pid" 2>/dev/null; then
  echo timeout >"$out/exit.status"
  killall picotracker 2>/dev/null
  wait "$app_pid" 2>/dev/null
else
  wait "$app_pid"
  echo "$?" >"$out/exit.status"
fi
pad quit
sleep 1
kill "$inject_pid" 2>/dev/null
rm -f "$fifo"
restore_config
mixer >"$out/mixer.after"
cp "$data/logs/picotracker.log" "$out/picotracker.log" 2>/dev/null
