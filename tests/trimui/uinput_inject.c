/*
 * Copyright (c) 2026 Jarod Wright and contributors
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

/* Virtual gamepad for tests/trimui/record_check.sh on the device itself (no Python there).
 * Same device shape and the same FIFO commands as tests/trimui/uinput_inject.py: buttons 0-12 are
 * consecutive key codes from BTN_SOUTH, the D-pad is hat 0, the left stick is axes 0/1.
 *
 *   hat up|down|left|right   press and release a D-pad direction
 *   button N [N...]          press the SDL buttons together, then release
 *   hold N hat up|down|left|right   hold button N while tapping a D-pad direction (Edit+Right...)
 *   chord                    Select(6)+Start(7)+L1(4)+R1(5)
 *   quit                     destroy the device and exit
 *
 * Build (see record_check.sh): trimui-cc -O2 -o uinput_inject uinput_inject.c
 * usage: uinput_inject FIFO      prints "ready" once the device exists.
 */
#include <fcntl.h>
#include <linux/uinput.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define BTN_FIRST 0x130
#define BUTTONS 13

static int fd;

static void emit(int type, int code, int value) {
  struct input_event ev;
  memset(&ev, 0, sizeof ev);
  ev.type = (unsigned short)type;
  ev.code = (unsigned short)code;
  ev.value = value;
  if (write(fd, &ev, sizeof ev) != (ssize_t)sizeof ev) perror("uinput write");
}
static void sync_(void) { emit(EV_SYN, SYN_REPORT, 0); }

static void tap(const int *keys, int n, useconds_t hold) {
  for (int i = 0; i < n; i++) { emit(EV_KEY, BTN_FIRST + keys[i], 1); sync_(); }
  usleep(hold);
  for (int i = n - 1; i >= 0; i--) { emit(EV_KEY, BTN_FIRST + keys[i], 0); sync_(); }
  usleep(hold);
}

static void hat(int axis, int value) {
  emit(EV_ABS, axis, value); sync_(); usleep(150000);
  emit(EV_ABS, axis, 0); sync_(); usleep(150000);
}

int main(int argc, char **argv) {
  if (argc != 2) { fprintf(stderr, "usage: %s FIFO\n", argv[0]); return 2; }
  fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
  if (fd < 0) { perror("/dev/uinput"); return 1; }
  ioctl(fd, UI_SET_EVBIT, EV_KEY);
  ioctl(fd, UI_SET_EVBIT, EV_ABS);
  ioctl(fd, UI_SET_EVBIT, EV_SYN);
  for (int i = 0; i < BUTTONS; i++) ioctl(fd, UI_SET_KEYBIT, BTN_FIRST + i);
  const int axes[] = {ABS_X, ABS_Y, ABS_HAT0X, ABS_HAT0Y};
  struct uinput_user_dev setup;
  memset(&setup, 0, sizeof setup);
  snprintf(setup.name, UINPUT_MAX_NAME_SIZE, "TRIMUI Player1 (emulated)");
  setup.id.bustype = 0x03; setup.id.vendor = 0x1234; setup.id.product = 0x5678; setup.id.version = 1;
  for (int i = 0; i < 4; i++) {
    ioctl(fd, UI_SET_ABSBIT, axes[i]);
    setup.absmin[axes[i]] = i < 2 ? -32768 : -1;
    setup.absmax[axes[i]] = i < 2 ? 32767 : 1;
  }
  if (write(fd, &setup, sizeof setup) != (ssize_t)sizeof setup) { perror("setup"); return 1; }
  if (ioctl(fd, UI_DEV_CREATE) < 0) { perror("UI_DEV_CREATE"); return 1; }
  puts("ready"); fflush(stdout);

  for (;;) {
    FILE *commands = fopen(argv[1], "r"); /* the writer closes after each batch */
    if (!commands) { perror(argv[1]); return 1; }
    char line[128];
    while (fgets(line, sizeof line, commands)) {
      char *word = strtok(line, " \n");
      if (!word) continue;
      if (!strcmp(word, "hat")) {
        const char *dir = strtok(NULL, " \n");
        if (!dir) continue;
        if (!strcmp(dir, "up")) hat(ABS_HAT0Y, -1);
        else if (!strcmp(dir, "down")) hat(ABS_HAT0Y, 1);
        else if (!strcmp(dir, "left")) hat(ABS_HAT0X, -1);
        else if (!strcmp(dir, "right")) hat(ABS_HAT0X, 1);
      } else if (!strcmp(word, "button")) {
        int keys[BUTTONS], n = 0;
        for (char *arg; n < BUTTONS && (arg = strtok(NULL, " \n"));) keys[n++] = atoi(arg);
        tap(keys, n, 150000);
      } else if (!strcmp(word, "hold")) {
        const char *button = strtok(NULL, " \n");
        const char *kind = strtok(NULL, " \n");
        const char *dir = strtok(NULL, " \n");
        if (!button || !kind || !dir || strcmp(kind, "hat")) continue;
        emit(EV_KEY, BTN_FIRST + atoi(button), 1); sync_(); usleep(150000);
        if (!strcmp(dir, "up")) hat(ABS_HAT0Y, -1);
        else if (!strcmp(dir, "down")) hat(ABS_HAT0Y, 1);
        else if (!strcmp(dir, "left")) hat(ABS_HAT0X, -1);
        else if (!strcmp(dir, "right")) hat(ABS_HAT0X, 1);
        emit(EV_KEY, BTN_FIRST + atoi(button), 0); sync_(); usleep(150000);
      } else if (!strcmp(word, "chord")) {
        const int keys[] = {6, 7, 4, 5};
        tap(keys, 4, 300000);
      } else if (!strcmp(word, "quit")) {
        ioctl(fd, UI_DEV_DESTROY);
        return 0;
      }
    }
    fclose(commands);
  }
}
