#!/usr/bin/env python3
"""Generate deterministic PCM fixtures and compare equal-format WAV renders."""

from __future__ import annotations

import argparse
import json
import math
import random
import sys
import wave
from array import array
from pathlib import Path
from typing import Callable, Iterable


SAMPLE_RATES = (44_100, 48_000, 96_000)
PCM_SCALE = 32_767


def clamp(value: float) -> float:
    return max(-1.0, min(1.0, value))


def pcm(value: float) -> int:
    return int(round(clamp(value) * PCM_SCALE))


def write_wav(path: Path, sample_rate: int, channels: int, frames: Iterable[tuple[float, ...]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    samples = array("h")
    for frame in frames:
        if len(frame) != channels:
            raise ValueError(f"Expected {channels} channels, got {len(frame)}")
        samples.extend(pcm(value) for value in frame)
    with wave.open(str(path), "wb") as wav:
        wav.setnchannels(channels)
        wav.setsampwidth(2)
        wav.setframerate(sample_rate)
        wav.writeframes(samples.tobytes())


def mono_frames(sample_rate: int, seconds: float, fn: Callable[[int, float], float]):
    for index in range(round(sample_rate * seconds)):
        yield (fn(index, index / sample_rate),)


def silence(sample_rate: int):
    return mono_frames(sample_rate, 1.0, lambda _index, _time: 0.0)


def impulse(sample_rate: int):
    impulse_index = round(sample_rate * 0.1)
    return mono_frames(sample_rate, 2.0, lambda index, _time: 0.8 if index == impulse_index else 0.0)


def sweep(sample_rate: int):
    duration = 5.0
    start_hz = 20.0
    end_hz = min(20_000.0, sample_rate * 0.45)
    ratio = end_hz / start_hz
    k = duration / math.log(ratio)
    scale = 2.0 * math.pi * start_hz * k

    def render(_index: int, time: float) -> float:
        phase = scale * (math.exp(time / k) - 1.0)
        fade = min(1.0, time / 0.02, (duration - time) / 0.02)
        return 0.25 * max(0.0, fade) * math.sin(phase)

    return mono_frames(sample_rate, duration, render)


def transients(sample_rate: int):
    duration = 4.0
    rng = random.Random(0x4C4145)
    burst_length = max(1, round(sample_rate * 0.025))
    samples = [0.0] * round(sample_rate * duration)
    for start in range(round(sample_rate * 0.25), len(samples), round(sample_rate * 0.5)):
        for offset in range(min(burst_length, len(samples) - start)):
            envelope = math.exp(-8.0 * offset / burst_length)
            samples[start + offset] += 0.65 * envelope * rng.uniform(-1.0, 1.0)
    return ((value,) for value in samples)


def stereo(sample_rate: int):
    duration = 3.0
    for index in range(round(sample_rate * duration)):
        time = index / sample_rate
        left = 0.25 * math.sin(2.0 * math.pi * 440.0 * time)
        right = 0.25 * math.sin(2.0 * math.pi * 880.0 * time)
        if index == round(sample_rate * 0.5):
            left += 0.5
        if index == round(sample_rate * 1.5):
            right += 0.5
        yield (left, right)


FIXTURES = {
    "silence": (1, silence),
    "impulse": (1, impulse),
    "sweep": (1, sweep),
    "transients": (1, transients),
    "stereo": (2, stereo),
}


def read_wav(path: Path):
    with wave.open(str(path), "rb") as wav:
        metadata = {
            "channels": wav.getnchannels(),
            "sample_width": wav.getsampwidth(),
            "sample_rate": wav.getframerate(),
            "frames": wav.getnframes(),
        }
        if metadata["sample_width"] != 2:
            raise ValueError(f"Only 16-bit PCM is currently supported: {path}")
        samples = array("h")
        samples.frombytes(wav.readframes(metadata["frames"]))
    channels = [samples[index :: metadata["channels"]] for index in range(metadata["channels"])]
    return metadata, channels


def channel_metrics(samples: array) -> dict[str, float]:
    values = [sample / PCM_SCALE for sample in samples]
    if not values:
        return {"peak": 0.0, "rms": 0.0, "dc": 0.0}
    return {
        "peak": max(abs(value) for value in values),
        "rms": math.sqrt(sum(value * value for value in values) / len(values)),
        "dc": sum(values) / len(values),
    }


def generate(output: Path) -> int:
    manifest = {"format": "PCM16 WAV", "sample_rates": list(SAMPLE_RATES), "fixtures": []}
    for sample_rate in SAMPLE_RATES:
        for name, (channels, factory) in FIXTURES.items():
            path = output / f"{name}-{sample_rate}.wav"
            write_wav(path, sample_rate, channels, factory(sample_rate))
            metadata, decoded = read_wav(path)
            manifest["fixtures"].append({"file": path.name, **metadata, "metrics": [channel_metrics(ch) for ch in decoded]})
    (output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(f"Generated {len(manifest['fixtures'])} fixtures in {output}")
    return 0


def analyze(path: Path) -> int:
    metadata, channels = read_wav(path)
    print(json.dumps({"file": str(path), **metadata, "metrics": [channel_metrics(ch) for ch in channels]}, indent=2))
    return 0


def compare(reference: Path, candidate: Path) -> int:
    ref_meta, ref_channels = read_wav(reference)
    candidate_meta, candidate_channels = read_wav(candidate)
    if ref_meta != candidate_meta:
        print(json.dumps({"error": "WAV formats do not match", "reference": ref_meta, "candidate": candidate_meta}, indent=2))
        return 2

    comparisons = []
    for ref, current in zip(ref_channels, candidate_channels):
        ref_values = [value / PCM_SCALE for value in ref]
        current_values = [value / PCM_SCALE for value in current]
        errors = [a - b for a, b in zip(ref_values, current_values)]
        ref_energy = sum(value * value for value in ref_values)
        current_energy = sum(value * value for value in current_values)
        denominator = math.sqrt(ref_energy * current_energy)
        comparisons.append({
            "max_abs_error": max((abs(value) for value in errors), default=0.0),
            "rmse": math.sqrt(sum(value * value for value in errors) / len(errors)) if errors else 0.0,
            "correlation": (sum(a * b for a, b in zip(ref_values, current_values)) / denominator) if denominator else None,
        })
    print(json.dumps({"reference": str(reference), "candidate": str(candidate), **ref_meta, "comparisons": comparisons}, indent=2))
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    generate_parser = commands.add_parser("generate")
    generate_parser.add_argument("--out", type=Path, required=True)
    analyze_parser = commands.add_parser("analyze")
    analyze_parser.add_argument("wav", type=Path)
    compare_parser = commands.add_parser("compare")
    compare_parser.add_argument("reference", type=Path)
    compare_parser.add_argument("candidate", type=Path)
    args = parser.parse_args()
    if args.command == "generate":
        return generate(args.out)
    if args.command == "analyze":
        return analyze(args.wav)
    return compare(args.reference, args.candidate)


if __name__ == "__main__":
    sys.exit(main())
