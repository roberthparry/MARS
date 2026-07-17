#!/usr/bin/env python3
"""Extract a scanned almanac figure as themed linework on transparency."""

from __future__ import annotations

import argparse
from pathlib import Path

import numpy as np
from PIL import Image


FOREGROUND = np.array((232, 239, 233), dtype=np.uint8)


def parse_crop(value: str) -> tuple[int, int, int, int]:
    fields = tuple(int(field) for field in value.split(","))
    if len(fields) != 4:
        raise argparse.ArgumentTypeError("crop must be left,top,right,bottom")
    return fields


def extract(
    source: Path,
    target: Path,
    crop: tuple[int, int, int, int],
    background_cutoff: int,
    solid_cutoff: int,
) -> None:
    with Image.open(source) as opened:
        gray = np.asarray(opened.convert("L").crop(crop), dtype=np.float32)

    # Suppress pale paper texture and show-through while retaining antialiased ink.
    coverage = np.clip(
        (background_cutoff - gray) / (background_cutoff - solid_cutoff),
        0.0,
        1.0,
    )
    alpha = np.rint(coverage * 255.0).astype(np.uint8)
    rgba = np.empty((*gray.shape, 4), dtype=np.uint8)
    rgba[:, :, :3] = FOREGROUND
    rgba[:, :, 3] = alpha

    target.parent.mkdir(parents=True, exist_ok=True)
    Image.fromarray(rgba, mode="RGBA").save(target, optimize=True)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=Path)
    parser.add_argument("target", type=Path)
    parser.add_argument("--crop", required=True, type=parse_crop)
    parser.add_argument("--background-cutoff", type=int, default=195)
    parser.add_argument("--solid-cutoff", type=int, default=85)
    args = parser.parse_args()
    extract(
        args.source,
        args.target,
        args.crop,
        args.background_cutoff,
        args.solid_cutoff,
    )


if __name__ == "__main__":
    main()
