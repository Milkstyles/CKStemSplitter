"""Frozen command-line entry point for the bundled CK Stem Splitter engine.

The htdemucs_ft_vocals specialist produces only vocals. demucs-onnx's built-in
--karaoke mixing expects drums/bass/other to exist, so CK handles karaoke mode
by running the vocal specialist normally and subtracting the vocal estimate
from the decoded source mix.
"""

from __future__ import annotations

import sys
from pathlib import Path

import numpy as np
import soundfile as sf
import soxr
from demucs_onnx.cli import main as demucs_main


def _make_karaoke(source_path: Path, vocals_path: Path, karaoke_path: Path) -> None:
    mix, mix_sr = sf.read(str(source_path), dtype="float32", always_2d=True)
    vocals, vocals_sr = sf.read(str(vocals_path), dtype="float32", always_2d=True)

    if mix_sr != vocals_sr:
        mix = soxr.resample(mix, mix_sr, vocals_sr, quality="HQ")
        mix_sr = vocals_sr

    if mix.shape[1] == 1 and vocals.shape[1] == 2:
        mix = np.repeat(mix, 2, axis=1)
    elif vocals.shape[1] == 1 and mix.shape[1] == 2:
        vocals = np.repeat(vocals, 2, axis=1)

    channels = min(mix.shape[1], vocals.shape[1])
    mix = mix[:, :channels]
    vocals = vocals[:, :channels]

    sample_count = min(len(mix), len(vocals))
    instrumental = mix[:sample_count] - vocals[:sample_count]
    instrumental = np.clip(instrumental, -1.0, 1.0)

    karaoke_path.parent.mkdir(parents=True, exist_ok=True)
    sf.write(str(karaoke_path), instrumental, mix_sr, subtype="FLOAT")


def main() -> int:
    argv = sys.argv[1:]
    ck_karaoke = (
        len(argv) >= 3
        and argv[0] == "separate"
        and "--karaoke" in argv
        and "htdemucs_ft_vocals" in argv
    )

    source_path: Path | None = None
    output_dir: Path | None = None

    if ck_karaoke:
        source_path = Path(argv[1])
        output_dir = Path(argv[2])
        argv = [arg for arg in argv if arg != "--karaoke"]
        sys.argv = [sys.argv[0], *argv]

    result = demucs_main()
    exit_code = 0 if result is None else int(result)

    if exit_code == 0 and ck_karaoke and source_path is not None and output_dir is not None:
        vocals_path = output_dir / "vocals.wav"
        karaoke_path = output_dir / "karaoke.wav"
        if not vocals_path.is_file():
            raise FileNotFoundError(f"Expected vocal stem was not created: {vocals_path}")
        _make_karaoke(source_path, vocals_path, karaoke_path)

    return exit_code


if __name__ == "__main__":
    raise SystemExit(main())
