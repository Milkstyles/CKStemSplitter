"""Frozen command-line entry point for the bundled CK Stem Splitter engine.

The htdemucs_ft_vocals specialist produces only vocals. demucs-onnx's built-in
--karaoke mixing expects drums/bass/other to exist, so CK handles karaoke mode
by running the vocal specialist normally and subtracting the vocal estimate
from the decoded source mix.
"""

from __future__ import annotations

import sys
import time
import traceback
from pathlib import Path

import numpy as np
import soundfile as sf
import soxr
from demucs_onnx.cli import main as demucs_main


def _separate_fixed_vst_window(input_path: Path, output_path: Path, session) -> None:
    """Run exactly one model inference; the VST performs the overlap-add."""
    from demucs_onnx.inference import (
        N_SAMPLES,
        SAMPLE_RATE,
        SOURCES,
        load_audio,
        resample_to_native,
        write_audio,
    )

    mix, native_sr = load_audio(input_path, target_sr=SAMPLE_RATE)
    native_frames = sf.info(str(input_path)).frames
    if mix.shape[1] < N_SAMPLES:
        mix = np.pad(mix, ((0, 0), (0, N_SAMPLES - mix.shape[1])))
    elif mix.shape[1] > N_SAMPLES:
        mix = mix[:, :N_SAMPLES]

    prediction = session.run(
        ["stems"], {"mix": mix[np.newaxis, ...].astype(np.float32, copy=False)},
    )[0][0]
    vocals = prediction[SOURCES.index("vocals")].astype(np.float32, copy=False)
    if native_sr != SAMPLE_RATE:
        vocals = resample_to_native(vocals, SAMPLE_RATE, native_sr)
    if vocals.shape[1] < native_frames:
        vocals = np.pad(vocals, ((0, 0), (0, native_frames - vocals.shape[1])))
    elif vocals.shape[1] > native_frames:
        vocals = vocals[:, :native_frames]
    write_audio(output_path, vocals, native_sr)


def _serve(argv: list[str]) -> int:
    """Keep one ONNX session warm and process sequential VST chunk jobs.

    Protocol (one worker per VST render directory):
      * worker writes ``server.ready`` after the model session is loaded;
      * VST writes ``chunk-N/input.wav`` then ``chunk-N/request.ready``;
      * worker writes ``chunk-N/output/vocals.wav`` then ``done.ready``;
      * failures are isolated in ``chunk-N/error.txt``;
      * VST writes ``shutdown.ready`` when the render instance is released.
    """
    if len(argv) < 2:
        print("usage: ckstem-engine serve QUEUE_DIR [--cache-dir DIR] [--providers cpu]", file=sys.stderr)
        return 2

    queue_dir = Path(argv[1])
    cache_dir: Path | None = None
    providers = "cpu"
    index = 2
    while index < len(argv):
        if argv[index] == "--cache-dir" and index + 1 < len(argv):
            cache_dir = Path(argv[index + 1])
            index += 2
        elif argv[index] == "--providers" and index + 1 < len(argv):
            providers = argv[index + 1]
            index += 2
        else:
            print(f"unknown serve argument: {argv[index]}", file=sys.stderr)
            return 2

    queue_dir.mkdir(parents=True, exist_ok=True)
    try:
        from demucs_onnx.inference import prewarm, session_pool

        if providers != "cpu":
            raise ValueError("The stable VST worker currently supports the CPU provider only")
        resolved_providers = ["CPUExecutionProvider"]
        model_paths = prewarm(
            ["htdemucs_ft_vocals"],
            precision="fp16weights",
            providers=resolved_providers,
            cache_dir=cache_dir,
        )
        vocal_model_path = model_paths["vocals"]
        vocal_session = session_pool().get(vocal_model_path, resolved_providers)
        (queue_dir / "server.ready").write_text(
            ",".join(resolved_providers), encoding="utf-8",
        )
    except Exception:
        (queue_dir / "server.error.txt").write_text(traceback.format_exc(), encoding="utf-8")
        return 3

    while not (queue_dir / "shutdown.ready").exists():
        requests = sorted(queue_dir.glob("chunk-*/request.ready"))
        if not requests:
            time.sleep(0.01)
            continue

        for request in requests:
            job_dir = request.parent
            done = job_dir / "done.ready"
            error = job_dir / "error.txt"
            if done.exists() or error.exists():
                request.unlink(missing_ok=True)
                continue
            try:
                output_dir = job_dir / "output"
                output_dir.mkdir(parents=True, exist_ok=True)
                _separate_fixed_vst_window(
                    job_dir / "input.wav", output_dir / "vocals.wav", vocal_session,
                )
                done.write_text("ok", encoding="utf-8")
            except Exception:
                error.write_text(traceback.format_exc(), encoding="utf-8")
            finally:
                request.unlink(missing_ok=True)

    return 0


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
    if argv and argv[0] == "serve":
        return _serve(argv)
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
