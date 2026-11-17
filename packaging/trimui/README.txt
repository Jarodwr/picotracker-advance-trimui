PicoTracker for the Trimui Smart Pro (tg5040, NextUI)

INSTALL: Extract the zip at the root of the SD card, so that the folder
Tools/tg5040/PicoTracker.pak appears. Eject the card, put it in the device and
start PicoTracker from the Tools list. The first run creates the data folder
.userdata/tg5040/PicoTracker/ with projects/ and samples/. To use your own
samples, copy them into samples/ from a computer. All loaded samples share a
384 MiB budget (as 16-bit audio in memory); a sample that does not fit in what is
left fails to load. To change the button mapping,
copy PicoTracker.pak/share/input.ini.example to .userdata/tg5040/PicoTracker/input.ini
and edit it. The log of the last two runs is in that folder under logs/; its 'video driver:' line names the display driver in use.
The app uses the device's own SDL2 library, so it needs the stock NextUI firmware.

EXIT: hold Select + Start + L1 + R1 together. The Menu button belongs to the
launcher and is not used. Sticks are ignored unless input.ini enables them.

UPDATE AND REMOVE: Delete Tools/tg5040/PicoTracker.pak, then extract the new zip
over the SD card root. Your projects and samples are kept because they live in
.userdata/, outside the pak. To uninstall, delete the pak; to remove your data as
well, delete .userdata/tg5040/PicoTracker/. Licences for everything in the app
are in PicoTracker.pak/licenses/NOTICE.md. PicoTracker is free software under the GPL-3.0-only (licenses/wrapper-GPL-3.0-only.txt); its source is at https://github.com/Jarodwr/picotracker-advance-trimui and the firmware it is built from is named in licenses/NOTICE.md.
