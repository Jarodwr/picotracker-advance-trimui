# 0019: Trimui battery level and volume

## Battery (C-62)

The indicator the firmware draws comes from `System::GetBatteryState()`
(percentage, charging, error), not from `battery_health()`. The host stub
returned `error = true`, so no real level was ever shown. `battery_health()` is
only the "Battery health" (state of health) line on the Device view.

`HostBattery` reads the AXP2202 power supply, the same files NextUI's
`batmon.elf`/`nextui.elf` use:

- `/sys/class/power_supply/axp2202-battery/capacity` (0-100) -> percentage
- `.../voltage_now` (microvolts) -> `voltage_mv`; `.../temp` (tenths of C)
- charging: battery `status == Charging`, or `axp2202-usb/online == 1` while
  the battery is not Full (NextUI treats usb online as charging)

Reads are cached for 5 seconds and are plain sysfs file reads from the UI
thread. If the files are missing (macOS, Linux CI) the reading has
`error = true`, as before. `PICOTRACKER_POWER_SUPPLY_ROOT` overrides the sysfs
directory for tests.

`battery_health()` has no numeric source on this device (sysfs only exposes
`health=Good`), so it returns -1 ("NA") when a battery is present and the old
value 0 off-device.

## Volume indicator (C-63)

The volume buttons never reach the app. On the device (NextUI 20261013-0,
probed read-only 2026-11-17) `keymon.elf` holds `/dev/input/event0-3` itself
and `audiomon.elf` runs beside it; the app only gets the pad. keymon keeps the
volume in NextUI's settings block, a 132-byte file mmap'd at
`/tmp/shm/SharedSettings` (persisted to `.userdata/tg5040/msettings.bin`), and
applies it to the ALSA mixer (`digital volume` 0-63 read 34 while the settings
speaker volume read 9 of 20). The firmware's own volume UI only reacts to its
own events, so nothing was drawn.

Decision: `HostVolumeFile` reads that shared file (a plain read of a tmpfs
file; no ALSA link, no subprocess): int 4 is the speaker volume, int 3 the
headphones volume (used when the jack flag, int 8, is set), both 0-20, shown as
a percentage. Anything outside 0-20 or a short file reads as unknown, so a
settings layout change shows nothing rather than a wrong number.
`HostVolumeWatcher` polls every 200 ms on the main loop pump, treats the first
valid read as the baseline (no overlay at start-up) and reports changes.
`HostVolumeOverlay` draws a bar and "Volume NN%" for 1.5 s through a second
hook next to the quit dialog's in `SDLWindow::Flush` (the quit dialog draws on
top). The app never writes the mixer or the settings, so the app's
OUTPUTVOLUME and the system volume cannot fight. Off-device the file does not
exist, the reading stays unknown and the overlay never appears.
`PICOTRACKER_VOLUME_FILE` overrides the path for tests.

Not verified: the buttons themselves (not pressable from here), and that
keymon updates the mmap'd file before the mixer on every press. The
`amixer cset` route does not change the settings file, so the device test
uses a copy of the file (see the card note).
