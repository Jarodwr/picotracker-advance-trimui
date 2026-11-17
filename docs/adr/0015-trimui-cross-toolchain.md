# ADR 0015: Trimui cross toolchain: GCC 13 against a glibc 2.23 sysroot

Status: accepted (C-9). Note: two ADRs carry the number 0015 (this one and `0015-macos-bundle.md`); see the docs index in the README.

## Context

ADR 0014 showed that the vendor Linaro GCC 7.5 cannot build the firmware (C++23, designated initialisers, `<filesystem>`) and recommended a newer cross compiler against an old-glibc sysroot, untested. This ADR records the toolchain that was built and checked. The target is the Trimui Smart Pro (TG5040, aarch64 Linux, glibc) running NextUI (ADR 0017); the device's own glibc and SDL2 versions are **not** verified (see "Not verified").

## Decision

`scripts/build-trimui.sh` builds the wrapper in the Docker image from `docker/trimui/Dockerfile`:

| Piece | Choice | Why |
| --- | --- | --- |
| Compiler | Ubuntu 24.04 `g++-13-aarch64-linux-gnu` (GCC 13.3) | Builds the pinned firmware unmodified with `-std=gnu++23`. |
| libc | glibc **2.23** headers and libraries from Ubuntu 16.04 arm64 `libc6`, `libc6-dev`, `linux-libc-dev`, in `/opt/trimui-glibc` | The lowest practical floor. Later aarch64 glibcs version `powf`, `expf`, `logf`, `log2f` at 2.27, and ADR 0017 suggests the device may be nearer 2.25. At 2.23 the binary needs only `GLIBC_2.17`. |
| C++ runtime | `libstdc++` and `libgcc` linked statically | The device needs no GCC runtime. |
| SDL2 | 2.26.1 built from source as a static library, in `/opt/trimui-sysroot` | 2.26.1 is the version of the vendor/union TG5040 sysroot (ADR 0014). Static linking is ADR 0017 section 4's preferred option. KMSDRM and ALSA are built in with `dlopen` loading (the ALSA probe needs a stub `libasound` in the sysroot, see ADR 0018), and the dummy video and audio drivers are included. X11, Wayland, PulseAudio, GL/GLES and Vulkan are off. |

Compiler wrappers (`trimui-cc`, `trimui-c++`) give the exact `--sysroot`, `-nostdinc`, include and library paths. They are needed because a native-layout GCC ignores `--sysroot` for its multiarch default directories, and the extracted glibc's absolute symlinks (such as `libm.so`) would otherwise resolve to the host. The Dockerfile rewrites them to relative links.

`cmake/toolchains/trimui-smart-pro.cmake` selects those wrappers and the SDL2 prefix. It adds `/opt/trimui-compat/compat.o` to the link, built from `docker/trimui/compat.c`. `CMakeLists.txt` and `platform/` are untouched.

### Things that did not work

- **Zig `cc` (clang 18, libc++) targeting `aarch64-linux-gnu.2.27`**, as ADR 0014 suggested, fails: `InstrumentView.cpp` uses `etl::string<strlen("...")>`, which GCC folds but clang rejects as a non-constant expression. A force-included `strlen` macro broke libc++ and ETL. Fixing it means editing the firmware (ADR 0001), so GCC is required.
- **The stock libstdc++ 13 headers** use `pthread_cond_clockwait`, `pthread_mutex_clocklock` and the rwlock clock variants (glibc 2.30). The Dockerfile copies the headers and turns those three `_GLIBCXX_USE_PTHREAD_*` switches off. The standard library then uses the older `pthread_cond_timedwait` paths.
- **SDL2's dynamic API** (`SDL_dynapi`) is enabled by default but its source is left out of a static archive, so every `SDL_*` symbol was unresolved. The Dockerfile forces it off in the downloaded source.

### Compat shims

The static GCC 13 runtime, and the SDL and libstdc++ code that uses randomness, reference a few symbols newer than glibc 2.23. `compat.c` supplies them: `__libc_single_threaded` (2.32, always "multi-threaded"), `__isoc23_strtoul` (2.38), `_dl_find_object` (2.35, implemented with `dl_iterate_phdr` so the unwinder and C++ exceptions work), `arc4random` (2.36), and `getrandom` and `getentropy` (2.25, via the kernel syscall).

## Result

Built with `scripts/build-trimui.sh` on an arm64 Docker host (`build/trimui/picotracker_desktop`, 5.6 MB, not stripped):

```
ELF 64-bit LSB pie executable, ARM aarch64, dynamically linked, interpreter /lib/ld-linux-aarch64.so.1
```

- Shared libraries needed: `libdl.so.2`, `libm.so.6`, `libpthread.so.0`, `libc.so.6`, `ld-linux-aarch64.so.1`. Nothing else: no `libSDL2`, `libstdc++` or `libgcc_s`.
- Required glibc symbol versions: **`GLIBC_2.17` only**. The script prints the list after each build.
- `PICOTRACKER_TRIMUI_SMOKE=1 scripts/build-trimui.sh` runs `--smoke` with the SDL dummy drivers in an Ubuntu 16.04 arm64 container (real glibc 2.23). It passes: the firmware boots and renders its first frame.
- Reproducible: the image build is pinned (Ubuntu 24.04 packages, SDL 2.26.1, Ubuntu 16.04 libc packages) and two clean builds in the same image gave the same SHA-256. The libc packages come from the live Ubuntu `xenial` archive without a version pin, so the image is only reproducible while those packages stay at their final versions.

Whether these requirements are available on the device is **not confirmed**. `libc.so.6`, `libm.so.6`, `libpthread.so.0` and `libdl.so.2` at glibc 2.17 or newer are in every glibc userland, and no TG5040 image is likely to be older than 2.23. The checked claims from the research report (NextUI, aarch64, glibc) are consistent with this, but the report does not state the version.

## Not verified

Update: the device is reported (NextUI 20261013-0) to run glibc 2.33 on kernel 4.9.191 with no system `libSDL2`, `libdrm` or `libgbm` and only `libasound.so.2`, so the glibc floor and static SDL choices fit. The binary has still not been run on the device; the items below remain open unless stated.

- The device's glibc version. The binary needs only `GLIBC_2.17`, so any glibc 2.17 or newer should do. This was not run on a device.
- Whether KMSDRM or ALSA works on the device. SDL loads `libdrm`, `libgbm` and `libasound` with `dlopen` and the device has not been checked for them. The video and audio drivers the device really uses (and the PowerVR GE8300 path in ADR 0016) are still open, so a fallback may need a vendor-specific backend or a rebuild of SDL2 with other options.
- Behaviour with the 2.23 libc on kernels used by the device. The smoke run uses the host kernel.
- `_dl_find_object` shim: C++ exception unwinding was exercised only through the smoke run.
- The binary is built with `-O3` and no `-mcpu` tuning; the Cortex-A53 target is not tuned.
- The x86-64 Docker host path (cross GCC and qemu smoke) was not run; only an arm64 host was used.

## Gaps for other cards

- A packaging card must copy `build/trimui/picotracker_desktop` to `bin/picotracker` per ADR 0017 (and strip it if size matters). ADR 0017 section 4 mentions a Linaro sysroot; the sysroot here is Ubuntu's glibc 2.23.
- Licence notices for SDL2 (zlib) and the libstdc++ runtime exception (GPL with runtime exception, statically linked) belong in the `licenses/` folder from ADR 0017.
- If the device lacks usable `libdrm`/`libgbm`/`libasound` for SDL, rebuild SDL2 in the Dockerfile with the right backend options.

Update (C-49): SDL2 is no longer linked statically. `docker/trimui/Dockerfile` builds a shared SDL 2.26.1 (SONAME `libSDL2-2.0.so.0`) used only as a link stub and never shipped; the binary resolves the device's vendor SDL2 from `/usr/trimui/lib`, which `launch.sh` puts on `LD_LIBRARY_PATH`. `main.cpp` tries `SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC` first and falls back to software; `SDL_RENDER_DRIVER=software` is gone from the launchers. The emulated smoke sets `LD_LIBRARY_PATH` to the stub (`build/trimui/sdl-stub`). Not yet verified on the device.

## Amendment (C-39): link stub and headers at the device's SDL 2.30.8

The C-37 display probe (card report) showed the static SDL 2.26.1 had no usable video driver on the device (offscreen fallback, black screen) and recommended the device's own `libSDL2-2.0.so.0` 2.30.8, which carries the vendor `mali` driver. C-49 landed the dynamic link; C-37 also notes that the stub should match the device's version so the headers, struct layouts and symbols are the ones the vendor library has. C-39 therefore raises `SDL2_VERSION` in `docker/trimui/Dockerfile` from 2.26.1 to **2.30.8**. The stub is still never shipped, and nothing vendor is downloaded or committed.

Result of `scripts/build-trimui.sh`: needed libraries `libSDL2-2.0.so.0`, `libpthread.so.0`, `libm.so.6`, `libc.so.6`, `ld-linux-aarch64.so.1`; glibc ceiling still `GLIBC_2.17` (device 2.33); no rpath. Two clean builds gave the same SHA-256. The `--smoke` boot on glibc 2.23 and `tests/trimui/emulated_smoke.sh` run against the stub. Not run on the device: it was unresponsive after the C-37 fb0 hang, so a run of this binary against `/usr/trimui/lib` is still open.
