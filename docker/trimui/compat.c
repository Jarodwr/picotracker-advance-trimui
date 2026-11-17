/*
 * Copyright (c) 2026 Jarod Wright and contributors
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

// Shims for glibc symbols that the GCC 13 static runtime (libstdc++.a,
// libgcc_eh.a) references but glibc 2.23 on the device does not have. Linked
// into the final binary by cmake/toolchains/trimui-smart-pro.cmake.
#define _GNU_SOURCE
#include <link.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <errno.h>

// glibc 2.32: libstdc++ reads it to skip atomics in single-threaded programs.
// Always "multi-threaded" is the safe value.
char __libc_single_threaded = 0;

// glibc 2.38 redirects strtoul to this under C23 feature macros.
unsigned long __isoc23_strtoul(const char *s, char **end, int base) {
  return strtoul(s, end, base);
}

// glibc 2.25 added getrandom() and getentropy(); the kernel syscall predates
// them. glibc 2.36 added arc4random(), which libstdc++'s random_device uses.
ssize_t getrandom(void *buf, size_t len, unsigned flags) {
  return syscall(SYS_getrandom, buf, len, flags);
}

int getentropy(void *buf, size_t len) {
  char *p = buf;
  while (len) {
    size_t n = len > 256 ? 256 : len;
    ssize_t r = getrandom(p, n, 0);
    if (r < 0) {
      if (errno == EINTR) continue;
      return -1;
    }
    p += r;
    len -= (size_t)r;
  }
  return 0;
}

uint32_t arc4random(void) {
  uint32_t v;
  getentropy(&v, sizeof v);
  return v;
}

// glibc 2.35: used by libgcc's unwinder to find .eh_frame_hdr for a pc.
struct trimui_dl_find_object {
  unsigned long long dlfo_flags;
  void *dlfo_map_start;
  void *dlfo_map_end;
  struct link_map *dlfo_link_map;
  void *dlfo_eh_frame;
  unsigned long long reserved[7];
};

struct find_ctx {
  uintptr_t pc;
  struct trimui_dl_find_object *out;
  int found;
};

static int find_cb(struct dl_phdr_info *info, size_t size, void *data) {
  (void)size;
  struct find_ctx *c = data;
  uintptr_t lo = UINTPTR_MAX, hi = 0;
  void *eh = 0;
  for (int i = 0; i < info->dlpi_phnum; i++) {
    const ElfW(Phdr) *p = &info->dlpi_phdr[i];
    if (p->p_type == PT_LOAD) {
      uintptr_t s = info->dlpi_addr + p->p_vaddr;
      if (s < lo) lo = s;
      if (s + p->p_memsz > hi) hi = s + p->p_memsz;
    } else if (p->p_type == PT_GNU_EH_FRAME) {
      eh = (void *)(info->dlpi_addr + p->p_vaddr);
    }
  }
  if (c->pc < lo || c->pc >= hi) return 0;
  c->out->dlfo_flags = 0;
  c->out->dlfo_map_start = (void *)lo;
  c->out->dlfo_map_end = (void *)hi;
  c->out->dlfo_link_map = 0;
  c->out->dlfo_eh_frame = eh;
  c->found = 1;
  return 1;
}

int _dl_find_object(void *pc, struct trimui_dl_find_object *result) {
  struct find_ctx c = {(uintptr_t)pc, result, 0};
  dl_iterate_phdr(find_cb, &c);
  return c.found ? 0 : -1;
}
