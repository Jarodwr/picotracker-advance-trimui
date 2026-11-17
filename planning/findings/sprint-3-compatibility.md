# Sprint 3 project and WAV compatibility

Tested against the pinned `xiphonics/advance-firmware` v3.2.1 source. `tests/compatibility_check.py` uses a disposable data root and calls the actual desktop firmware executable.

The first boot created `.config.xml` and `/projects/.untitled/ptsav.dat` using upstream persistence. A second process loaded that project successfully. The project file's SHA-256 stayed unchanged across the load. This checks a create → save → reload cycle through the pinned firmware, without mutating a user project.

The test generates 44.1 kHz, mono, 16-bit PCM WAVs accepted by the pinned `WavFile` parser. With `--sample-memory-mib 1`, a 200,000-byte sample loaded, while a 1,000,000-byte sample was rejected once the pool lacked capacity. Project and WAV file hashes stayed unchanged. This verifies loading and refusal, not a sample export or other WAV format variants.

The public v3.2.1 source is the compatibility baseline. We have no later Advance firmware fixture in this repository, so differences from a newer private or unpublished firmware cannot be measured yet. A supplied project from such a version should be copied into a disposable root before testing.
