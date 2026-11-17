# ADR 0010: Start with silent host services and a bounded sample pool

Status: accepted

The desktop wrapper installs the firmware's MIDI service without physical devices and supplies a silent `AudioOut`. This lets the shared application and project code run while SDL audio and host MIDI remain separate milestones. The UI can identify the output as `silent` / `No audio device`; no playback is promised yet.

Host samples use one allocation per loaded WAV with a configurable memory budget. The default is 128 MiB; `--sample-memory-mib` accepts 1–2047 MiB. The upper bound follows upstream 32-bit signed sample sizes. The pool enforces its byte budget and upstream sample-count limit without changing the firmware's WAV or project format. A failed allocation or excess sample leaves the pool unchanged.

The wrapper owns these adapters. The pinned firmware submodule stays unmodified. Real audio output and MIDI device discovery can replace the silent adapters later without changing project persistence.
