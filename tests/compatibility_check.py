# Copyright (c) 2026 Jarod Wright and contributors
#
# SPDX-License-Identifier: GPL-3.0-only

"""Exercise project persistence and WAV loading with the pinned firmware build."""

import hashlib
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import wave


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run(executable: Path, root: Path, *extra: str) -> str:
    result = subprocess.run(
        [str(executable), "--smoke", "--data-dir", str(root), *extra],
        capture_output=True,
        text=True,
        env={**os.environ, "SDL_VIDEODRIVER": "dummy"},
        check=True,
    )
    assert "firmware frame hash:" in result.stderr
    return result.stderr


def make_wave(path: Path, frames: int) -> None:
    with wave.open(str(path), "wb") as output:
        output.setnchannels(1)
        output.setsampwidth(2)
        output.setframerate(44100)
        output.writeframes(b"\0\0" * frames)


def main() -> None:
    executable = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="picotracker-compat-") as temporary:
        root = Path(temporary)
        first = run(executable, root)
        project = root / "projects/.untitled/ptsav.dat"
        assert project.is_file() and "create new project" in first
        original_project = digest(project)
        second = run(executable, root)
        assert "Successfully opened file: /projects/.untitled/ptsav.dat" in second
        assert digest(project) == original_project

        samples = root / "projects/.untitled/samples"
        small = samples / "a-small.wav"
        large = samples / "b-large.wav"
        make_wave(small, 100_000)
        make_wave(large, 500_000)
        original_samples = digest(small), digest(large)
        capacity = run(
            executable, root, "--sample-memory-mib", "1",
            "--sample-probe", "/projects/.untitled/samples/a-small.wav",
            "--sample-probe", "/projects/.untitled/samples/b-large.wav",
        )
        assert "sample probe /projects/.untitled/samples/a-small.wav: index=" in capacity
        assert "sample probe /projects/.untitled/samples/b-large.wav: index=-1" in capacity
        assert (digest(small), digest(large)) == original_samples
        assert digest(project) == original_project
        print("project create/reload and WAV capacity checks passed")


if __name__ == "__main__":
    main()
