// Copyright (c) 2026 Jarod Wright and contributors
//
// SPDX-License-Identifier: GPL-3.0-only

#include <SDL.h>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include "SDLWindow.h"
#include "HostDisplayGeometry.h"
#include "HostSystem.h"
#include "HostFileSystem.h"
#include "HostEvent.h"
#include "HostQuitDialog.h"
#include "HostVolume.h"
#include "HostVolumeOverlay.h"
#include "SDLJoystickSource.h"
#include "HostTimer.h"
#include "HostDisplay.h"
#include "HostAudio.h"
#include "HostMidi.h"
#include "HostSamplePool.h"
#if defined(__APPLE__)
#include "MacBackend.h"
#elif defined(PICOTRACKER_LINUX_BACKEND)
#include "LinuxBackend.h"
#endif
#include "Application/Application.h"
#include "Application/AppWindow.h"
#include "Services/Audio/Audio.h"
#include "Services/Midi/MidiService.h"
#include <cstdlib>
#include <vector>

// Reads the 720x720 logical frame. With a logical size set, SDL reads back
// window pixels (the frame times the integer scale), so sample it back down.
static bool ReadFrame(SDL_Renderer *renderer, std::vector<uint32_t> &frame) {
  int outW = 0, outH = 0;
  SDL_GetRendererOutputSize(renderer, &outW, &outH);
  const HostDisplayLayout layout = ComputeDisplayLayout(outW, outH);
  if (layout.fractional) {
    // Fractional fit has no exact readback; only the native path is checked.
    return false;
  }
  const int scale = layout.scale;
  const int size = kHostFrameSize * scale;
  std::vector<uint32_t> raw(static_cast<size_t>(size) * size);
  SDL_Rect area{0, 0, kHostFrameSize, kHostFrameSize};
  SDL_Rect *rect = scale == 1 ? nullptr : &area;
  if (SDL_RenderReadPixels(renderer, rect, SDL_PIXELFORMAT_ARGB8888,
                           raw.data(), size * sizeof(uint32_t)) != 0)
    return false;
  frame.resize(kHostFrameSize * kHostFrameSize);
  for (int y = 0; y < kHostFrameSize; ++y)
    for (int x = 0; x < kHostFrameSize; ++x)
      frame[y * kHostFrameSize + x] = raw[(y * scale) * size + x * scale];
  return true;
}

static uint64_t FrameHash(SDL_Renderer *renderer) {
  std::vector<uint32_t> pixels;
  if (!ReadFrame(renderer, pixels)) return 0;
  uint64_t hash = 1469598103934665603ULL;
  for (const auto pixel : pixels) {
    hash ^= pixel;
    hash *= 1099511628211ULL;
  }
  return hash;
}

static bool SaveFrame(SDL_Renderer *renderer, const char *path) {
  std::vector<uint32_t> pixels;
  if (!ReadFrame(renderer, pixels)) return false;
  SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormatFrom(
      pixels.data(), 720, 720, 32, 720 * sizeof(uint32_t), SDL_PIXELFORMAT_ARGB8888);
  if (!surface) return false;
  const bool saved = SDL_SaveBMP(surface, path) == 0;
  SDL_FreeSurface(surface);
  return saved;
}

int main(int argc, char **argv) {
  bool smoke = false;
  bool trimui = false;
  bool allowHeadless = false;
  const char *dataDirectory = nullptr;
  const char *screenshot = nullptr;
  const char *inputProfile = nullptr;
  bool logInput = false;
  bool captureEnabled = true;
  std::vector<const char *> sampleProbes;
  uint32_t sampleMemory = HostSamplePool::DefaultLimitBytes;
  for (int index = 1; index < argc; ++index) {
    if (std::strcmp(argv[index], "--smoke") == 0) smoke = true;
    else if (std::strcmp(argv[index], "--display") == 0 && index + 1 < argc) {
      const char *mode = argv[++index];
      if (std::strcmp(mode, "trimui") == 0) trimui = true;
      else if (std::strcmp(mode, "desktop") == 0) trimui = false;
      else {
        std::fprintf(stderr, "display must be trimui or desktop\n");
        return 2;
      }
    }
    else if (std::strcmp(argv[index], "--allow-headless") == 0) allowHeadless = true;
    else if (std::strcmp(argv[index], "--data-dir") == 0 && index + 1 < argc)
      dataDirectory = argv[++index];
    else if (std::strcmp(argv[index], "--log-input") == 0) logInput = true;
    else if (std::strcmp(argv[index], "--no-capture") == 0) captureEnabled = false;
    else if (std::strcmp(argv[index], "--input-profile") == 0 && index + 1 < argc)
      inputProfile = argv[++index];
    else if (std::strcmp(argv[index], "--screenshot") == 0 && index + 1 < argc)
      screenshot = argv[++index];
    else if (std::strcmp(argv[index], "--sample-probe") == 0 && index + 1 < argc)
      sampleProbes.push_back(argv[++index]);
    else if (std::strcmp(argv[index], "--sample-memory-mib") == 0 && index + 1 < argc) {
      char *end = nullptr;
      const auto mib = std::strtoul(argv[++index], &end, 10);
      if (!end || *end || mib == 0 || mib > 2047) {
        std::fprintf(stderr, "sample memory must be 1..2047 MiB\n");
        return 2;
      }
      sampleMemory = static_cast<uint32_t>(mib) * 1024U * 1024U;
    }
    else {
      std::fprintf(stderr, "usage: %s [--smoke] [--display trimui|desktop] [--allow-headless] [--data-dir PATH] [--sample-memory-mib N] [--input-profile PATH] [--log-input] [--no-capture] [--screenshot BMP] [--sample-probe FIRMWARE_PATH]\n", argv[0]);
      return 2;
    }
  }
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_JOYSTICK) != 0) {
    std::fprintf(stderr, "SDL init failed: %s\n", SDL_GetError());
    return 1;
  }
  const char *videoDriver = SDL_GetCurrentVideoDriver();
  std::fprintf(stderr, "video driver: %s\n", videoDriver ? videoDriver : "none");
  if (trimui) {
    const char *requested = std::getenv("SDL_VIDEODRIVER");
    if (ShouldRefuseHeadlessPanel(videoDriver, requested && *requested,
                                  allowHeadless)) {
      std::fprintf(stderr,
                   "error: --display trimui got the '%s' video driver, which "
                   "draws no picture on the panel. Run with the vendor SDL "
                   "(LD_LIBRARY_PATH=/usr/trimui/lib), or set SDL_VIDEODRIVER "
                   "or pass --allow-headless to run without a picture.\n",
                   videoDriver ? videoDriver : "none");
      SDL_Quit();
      return 3;
    }
  }

  SDL_Window *window = nullptr;
  if (trimui) {
    // Native fullscreen panel; the 720x720 frame is scaled by the renderer.
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    window = SDL_CreateWindow("picoTracker Advance", SDL_WINDOWPOS_UNDEFINED,
                              SDL_WINDOWPOS_UNDEFINED, 0, 0,
                              SDL_WINDOW_SHOWN | SDL_WINDOW_FULLSCREEN_DESKTOP);
  } else {
    window = SDL_CreateWindow("picoTracker Advance desktop prototype",
                              SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                              720, 720, SDL_WINDOW_SHOWN);
  }
  if (!window) {
    std::fprintf(stderr, "Window creation failed: %s\n", SDL_GetError());
    SDL_Quit();
    return 1;
  }

  SDL_Renderer *renderer = nullptr;
  if (trimui) {
    // C-37: GLES with vsync is the path NextUI itself uses on the Smart Pro
    // (software cannot hold 60 fps); fall back to software when unavailable.
    renderer = SDL_CreateRenderer(
        window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
  } else {
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
  }
  if (!renderer) {
    std::fprintf(stderr, "Renderer creation failed: %s\n", SDL_GetError());
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 1;
  }
  if (trimui) {
    SDL_RenderSetLogicalSize(renderer, kHostFrameSize, kHostFrameSize);
    SDL_RenderSetIntegerScale(renderer, SDL_TRUE);
    SDL_RendererInfo info{};
    SDL_GetRendererInfo(renderer, &info);
    std::fprintf(stderr, "renderer: %s\n", info.name ? info.name : "unknown");
    // Real panel size from the display, not a driver default.
    int panelW = 0, panelH = 0;
    SDL_GetRendererOutputSize(renderer, &panelW, &panelH);
    SDL_DisplayMode mode{};
    if (SDL_GetDesktopDisplayMode(SDL_GetWindowDisplayIndex(window), &mode) == 0)
      std::fprintf(stderr, "display mode: %dx%d @%dHz (%s)\n", mode.w, mode.h,
                   mode.refresh_rate, mode.w >= mode.h ? "landscape" : "portrait");
    const HostDisplayLayout layout = ComputeDisplayLayout(panelW, panelH);
    std::fprintf(stderr, "display: trimui panel %dx%d scale %d at %d,%d%s\n",
                 panelW, panelH, layout.scale, layout.x, layout.y,
                 layout.fractional ? " (fractional)" : "");
  }

  HostSystem system;
  System::Install(&system);
  char *defaultDataDirectory = nullptr;
  if (!dataDirectory) {
    defaultDataDirectory = SDL_GetPrefPath("picoTracker", "Advance Desktop");
    dataDirectory = defaultDataDirectory;
  }
  const std::string inputProfilePath =
      inputProfile ? std::string(inputProfile)
                   : (std::filesystem::path(dataDirectory ? dataDirectory : "") /
                      "input.ini").string();
  HostFileSystem filesystem(dataDirectory ? dataDirectory : "");
  SDL_free(defaultDataDirectory);
  if (!filesystem.valid()) {
    std::fprintf(stderr, "Could not initialize desktop data directory\n");
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 1;
  }
  FileSystem::Install(&filesystem);
  HostTimerService timers;
  TimerService::Install(&timers);
  HostEventManager events(timers);
  SDLJoystickSource joystick;
  joystick.SetLogInput(logInput);
  joystick.LoadProfileFile(inputProfilePath);
  events.SetJoystickSource(&joystick);
  HostWindowFactory factory(renderer, events);
  HostQuitDialog quitDialog;
  factory.SetQuitDialog(&quitDialog);
  // C-63: indicator for volume changes made by NextUI's keymon. Inert without
  // the shared settings file (every off-device run).
  HostVolumeFile volumeFile;
  HostVolumeWatcher volumeWatcher(volumeFile);
  HostVolumeOverlay volumeOverlay;
  factory.SetVolumeOverlay(&volumeOverlay);
  auto volumeTick = [&] {
    const uint32_t now = SDL_GetTicks();
    if (volumeWatcher.Poll(now)) volumeOverlay.Show(volumeWatcher.Volume(), now);
    if (volumeOverlay.Tick(now)) factory.Refresh();
  };
  events.SetQuitDialog(&quitDialog, [&factory] { factory.Refresh(); });
  std::fprintf(stderr,
               "controls: Menu (or hold Select+Start 1 s) opens the quit "
               "dialog, A = yes, B = no; Select+Start+L1+R1 exits at once\n");
  I_GUIWindowFactory::Install(&factory);
  HostDisplayInstall(renderer);
  AudioSettings audioSettings;
  audioSettings.bufferSize_ = 1024;
  audioSettings.preBufferCount_ = 8;
#if defined(__APPLE__) || defined(PICOTRACKER_LINUX_BACKEND)
  // Real SDL audio (plus CoreMIDI on macOS, unsupported MIDI on Linux); each
  // falls back to a silent/null device so the application still boots when
  // hardware is absent.
#if defined(__APPLE__)
  MacBackend backend(audioSettings);
#else
  LinuxBackendOptions backendOptions;
  backendOptions.captureEnabled = captureEnabled;
  LinuxBackend backend(audioSettings, std::move(backendOptions));
#endif
  backend.Install();
  events.SetPump([&backend, &volumeTick] {
    backend.Pump();
    volumeTick();
  });
#else
  // No platform backend yet: silent audio and no MIDI devices (ADR 0013).
  HostMidiService midi;
  MidiService::Install(&midi);
  HostAudio audio(audioSettings, false);
  Audio::Install(&audio);
  events.SetPump(volumeTick);
#endif
  HostSamplePool samples(sampleMemory);
  SamplePool::Install(&samples);
  events.Init();
  GUICreateWindowParams params{};
  params.title = "picoTracker Advance";
  auto *application = Application::GetInstance();
  if (!application->Init(params) || !application->GetWindow()) {
    std::fprintf(stderr, "Firmware application failed to initialize\n");
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 1;
  }
#if defined(__APPLE__) || defined(PICOTRACKER_LINUX_BACKEND)
  {
    const PlatformStatus platform = backend.Status();
    std::fprintf(stderr,
                 "audio: %s (%s%s%s)\nmidi in: %s\nmidi out: %s\n",
                 platform.audioAvailable ? "available" : "silent",
                 platform.audioApi.c_str(), platform.audioDevice.empty() ? "" : ", ",
                 platform.audioDevice.c_str(),
                 platform.midiInAvailable ? platform.midiInName.c_str() : "none",
                 platform.midiOutAvailable ? platform.midiOutName.c_str() : "none");
#if !defined(__APPLE__)
    std::fprintf(stderr, "recording: %s%s%s\n",
                 platform.recordingAvailable ? "available" : "unavailable",
                 platform.recordingError.empty() ? "" : " - ",
                 platform.recordingError.c_str());
#endif
  }
#endif
  events.SetWindow(application->GetWindow());
  // Any quit saves first: PowerDown() autosaves the open project the way the
  // firmware does before cutting power, then HostSystem::PowerDown returns.
  events.SetQuitFlush([application] {
    if (auto *app = static_cast<AppWindow *>(application->GetWindow()))
      app->PowerDown();
  });
  application->GetWindow()->Update(true);
  application->GetWindow()->ClockTick();
  for (const char *path : sampleProbes) {
    const int index = samples.LoadProjectSample(path);
    std::fprintf(stderr, "sample probe %s: index=%d remaining=%u\n", path,
                 index, samples.GetAvailableSampleStorageSpace());
  }
  if (screenshot && !SaveFrame(renderer, screenshot)) {
    std::fprintf(stderr, "Could not save screenshot: %s\n", SDL_GetError());
    return 1;
  }
  if (smoke) {
    const uint64_t first = FrameHash(renderer);
    SDL_Event down{};
    down.type = SDL_KEYDOWN;
    down.key.keysym.sym = SDLK_DOWN;
    events.Handle(down);
    application->GetWindow()->ClockTick();
    const uint64_t second = FrameHash(renderer);
    std::fprintf(stderr, "firmware frame hash: %llx -> %llx\n",
                 static_cast<unsigned long long>(first),
                 static_cast<unsigned long long>(second));
    if (!first || first == second) return 1;
  } else events.MainLoop();

  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}
