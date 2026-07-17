#!/usr/bin/env python3
"""Make diagram backgrounds transparent for the almanac Markdown preview."""

from __future__ import annotations

import argparse
import re
from collections import Counter
from pathlib import Path

import numpy as np
from PIL import Image


IMAGE_RE = re.compile(r"!\[([^]]*)\]\(<([^>]+)>\)")


def border_pixels(image: np.ndarray) -> np.ndarray:
    return np.concatenate(
        (
            image[0, :, :],
            image[-1, :, :],
            image[:, 0, :],
            image[:, -1, :],
        )
    )


def dominant_opaque_border_color(image: np.ndarray) -> tuple[int, int, int] | None:
    border = border_pixels(image)
    opaque = border[border[:, 3] >= 128, :3]
    if len(opaque) < max(8, len(border) // 20):
        return None
    return Counter(map(tuple, opaque.tolist())).most_common(1)[0][0]


def remove_background(image: np.ndarray, background: tuple[int, int, int]) -> np.ndarray:
    result = image.copy()
    rgb = result[:, :, :3].astype(np.float32)
    bg = np.asarray(background, dtype=np.float32)
    distance = np.sqrt(np.sum((rgb - bg) ** 2, axis=2))

    # Preserve linework while smoothly removing antialiased background pixels.
    inner, outer = 7.0, 42.0
    coverage = np.clip((distance - inner) / (outer - inner), 0.0, 1.0)
    result[:, :, 3] = np.rint(result[:, :, 3].astype(np.float32) * coverage).astype(
        np.uint8
    )
    return result


def output_path(source: Path) -> Path:
    return source.with_name(f"{source.stem}-md{source.suffix}")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("markdown", type=Path)
    args = parser.parse_args()

    markdown = args.markdown.resolve()
    text = markdown.read_text(encoding="utf-8")
    replacements: dict[str, str] = {}
    converted = 0
    already_transparent = 0

    for alt, relative in IMAGE_RE.findall(text):
        if "table" in alt.lower():
            continue

        linked = markdown.parent / relative
        if linked.stem.endswith("-md"):
            source = linked.with_name(f"{linked.stem[:-3]}{linked.suffix}")
            target = linked
        else:
            source = linked
            target = output_path(source)
        with Image.open(source) as opened:
            rgba = np.asarray(opened.convert("RGBA"))

        background = dominant_opaque_border_color(rgba)
        if background is None:
            already_transparent += 1
            continue

        converted_image = remove_background(rgba, background)
        Image.fromarray(converted_image, mode="RGBA").save(target, optimize=True)
        replacements[relative] = target.relative_to(markdown.parent).as_posix()
        converted += 1

    for source, target in replacements.items():
        text = text.replace(f"](<{source}>)", f"](<{target}>)")

    markdown.write_text(text, encoding="utf-8")
    print(f"converted: {converted}")
    print(f"already transparent: {already_transparent}")


if __name__ == "__main__":
    main()
