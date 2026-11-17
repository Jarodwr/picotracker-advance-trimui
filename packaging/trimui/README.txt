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

HOME SCREEN AND QUIT: press the Menu button, or hold Select + Start for one
second, to open the Home screen. It shows what each Smart Pro button does in
PicoTracker (below, with the default mapping; if you remap buttons in input.ini
the screen shows your mapping). L and R switch between its two pages; Home or B
closes it. The game does not see any button while it is open. To quit, press A on
"> Quit PicoTracker", then A on the question "Quit PicoTracker?" (B = No).
Buttons pressed in the first 2.5 seconds after the Home screen opens are ignored
for the Quit item. Quitting saves your project the same way the device does when it
powers down (an autosave; your last explicit save is kept) and returns to NextUI.
EMERGENCY EXIT: hold Select + Start + L1 + R1 together; it quits at once, with no
question, but still saves. If Menu does nothing (NextUI may take it), use
Select + Start. Sticks are ignored unless input.ini enables them.

Page 1 of the Home screen (Pad = Smart Pro button, Adv = Advance button):

  Pad     Adv    Does
  D-up    Up     Cursor up
  D-down  Down   Cursor down
  D-left  Left   Cursor left
  D-right Right  Cursor right
  A       ENTER  Enter / insert
  B       EDIT   Edit / copy
  L1      ALT    Modifier key
  R1      NAV    Screens modifier
  Start   PLAY   Play / stop
  Select  -      Not used

Page 2 (combos):

  NAV=R1 ALT=L1
  ENTER=A EDIT=B
  PLAY=Start
  Screens:
   NAV+Right/Left next/prev
   Song: NAV+Up Project
   Song: NAV+Down Mixer
  Play / stop:
   PLAY       here
   NAV+PLAY   in song
   hold PLAY  panic stop
  Notes and values:
   ENTER       insert/last
   ENTER twice next unused
   ENTER+arrows +-1 / +-10
   ALT+ENTER   cut / paste
   ALT+EDIT    select

VOLUME: the volume buttons are handled by NextUI, not by PicoTracker. When the
volume changes, PicoTracker shows a small bar and percentage at the bottom for
about 1.5 seconds; it never changes the volume itself.

UPDATE AND REMOVE: Delete Tools/tg5040/PicoTracker.pak, then extract the new zip
over the SD card root. Your projects and samples are kept because they live in
.userdata/, outside the pak. To uninstall, delete the pak; to remove your data as
well, delete .userdata/tg5040/PicoTracker/. Licences for everything in the app
are in PicoTracker.pak/licenses/NOTICE.md. PicoTracker is free software under the GPL-3.0-only (licenses/wrapper-GPL-3.0-only.txt); its source is at https://github.com/Jarodwr/picotracker-advance-trimui and the firmware it is built from is named in licenses/NOTICE.md.
