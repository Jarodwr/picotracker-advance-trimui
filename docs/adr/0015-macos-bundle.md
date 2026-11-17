# ADR 0015: Package a local macOS app bundle

Status: accepted

The card that produced this record named it `0014-macos-bundle.md`; 0014 was already taken by the Trimui toolchain ADR, so this is 0015.

## Context

`picotracker_desktop` links SDL2 from Homebrew. Homebrew's `sdl2` is currently `sdl2-compat`, which implements the SDL2 API on top of SDL3 and `dlopen`s `libSDL3.dylib` from `@loader_path` or `@executable_path`. The built executable also carries a Homebrew rpath. The binary therefore only runs on a machine with the same Homebrew prefix, and `otool -L` alone does not show the SDL3 dependency.

## Decision

`cmake --build <dir> --target picotracker_macos_app` runs `packaging/macos/make-app.sh`, which writes `<dir>/bundle/picoTracker Advance Desktop.app`. The target is macOS-only and not part of `all`.

- `Contents/MacOS/picotracker_desktop` is the built executable. `Info.plist` is generated from `packaging/macos/Info.plist.in` (bundle id `org.picotracker.advance-desktop`).
- `Contents/Frameworks/` holds the transitive closure of non-system dylibs found with `otool -L` (currently `libSDL2-2.0.0.dylib`). Each is given an `@rpath/<name>` install name and every reference is rewritten with `install_name_tool`. System libraries and frameworks (`/usr/lib`, `/System`) stay external.
- `libSDL3.dylib` is copied in under that exact name because sdl2-compat loads it dynamically. The script does this only if the bundled SDL2 library references `@loader_path/libSDL3.dylib`.
- All pre-existing rpaths are deleted from the executable and replaced with `@executable_path/../Frameworks`. Without this, `/opt/homebrew/lib` wins and the bundle silently uses the host's SDL. Verified with `DYLD_PRINT_LIBRARIES=1` that both SDL libraries load from inside a copied bundle.
- Everything is ad-hoc signed (`codesign --sign -`), which arm64 requires after `install_name_tool`. This is not Developer ID signing or notarization; a bundle copied from another machine may need Gatekeeper's quarantine bypass.
- `Contents/Resources/licenses/` carries `THIRD_PARTY_NOTICES.md` (source of truth: `packaging/macos/`), the upstream `LICENSING.md` and `LICENSES/`, the license files of Braids, ETL, yxml and TinyXML2, and the SDL2 compat and SDL3 licenses (zlib) found in the Homebrew prefix. libsamplerate's sources ship without a license file; the notice file records it as BSD-2-Clause with a pointer upstream.

## Data directory

No change to the app: without `--data-dir`, `SDL_GetPrefPath("picoTracker", "Advance Desktop")` is used (`~/Library/Application Support/picoTracker/Advance Desktop/`, ADR 0008). That path is independent of the bundle location, so moving or replacing the `.app` keeps projects and config. Checked by running `--smoke` twice from a copied bundle: the second run loads the first run's `.config.xml` and `.untitled` project.

## Consequences

- The bundle is portable across Macs of the same architecture and at least the deployment target (default 11.0, or `CMAKE_OSX_DEPLOYMENT_TARGET`), subject to the Homebrew SDL builds' own minimum OS. It is not universal: arm64 only on an arm64 build.
- If Homebrew's SDL changes how it locates SDL3, the `libSDL3.dylib` step needs revisiting; the script fails the build if the library cannot be found.
- Distribution to other users still needs Developer ID signing, notarization and a decision on GPL-3.0 obligations for newer upstream contributions (see `LICENSING.md`). Out of scope here.
