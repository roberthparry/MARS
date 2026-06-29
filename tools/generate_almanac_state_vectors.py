#!/usr/bin/env python3
"""Generate polynomial state-vector rows from SPICE for the almanac database."""

from __future__ import annotations

import argparse
import json
import math
import os
from pathlib import Path
import sys

try:
    import numpy as np
except ModuleNotFoundError as exc:  # pragma: no cover
    print(f"Missing NumPy dependency for state-vector generation: {exc}", file=sys.stderr)
    raise SystemExit(1)

try:
    import spiceypy as sp
except ModuleNotFoundError as exc:  # pragma: no cover
    print(f"Missing SPICE dependency for state-vector generation: {exc}", file=sys.stderr)
    raise SystemExit(1)


SPICE_KERNEL_FILENAMES = ("naif0012.tls", "pck00011.tpc", "de440.bsp")
AU_KM = 149597870.7
TARGET_CODES = {
    "EARTH": "EARTH",
    "EARTH_BARYCENTER": "EARTH BARYCENTER",
    "MOON": "MOON",
    "MARS": "MARS BARYCENTER",
    "JUPITER": "JUPITER BARYCENTER",
    "SATURN": "SATURN BARYCENTER",
    "MERCURY": "MERCURY BARYCENTER",
    "VENUS": "VENUS BARYCENTER",
}


def spice_kernel_dir(explicit: str | None) -> Path:
    if explicit:
        return Path(explicit).expanduser()
    env_value = os.environ.get("MARS_ALMANAC_KERNEL_DIR", "").strip()
    if env_value:
        return Path(env_value).expanduser()
    if (Path.home() / ".mars" / "almanac-kernels").exists():
        return Path.home() / ".mars" / "almanac-kernels"
    return Path("/tmp/mars-almanac-kernels")


def load_spice(kernel_dir: Path) -> None:
    missing = [name for name in SPICE_KERNEL_FILENAMES if not (kernel_dir / name).exists()]
    if missing:
        raise RuntimeError(f"missing SPICE kernels in {kernel_dir}: {', '.join(missing)}")
    sp.kclear()
    for name in SPICE_KERNEL_FILENAMES:
        sp.furnsh(str(kernel_dir / name))


def jd_from_gregorian(year: int, month: int, day: int) -> float:
    a = (14 - month) // 12
    y = year + 4800 - a
    m = month + 12 * a - 3
    jdn = day + ((153 * m + 2) // 5) + 365 * y + y // 4 - y // 100 + y // 400 - 32045
    return float(jdn) - 0.5


def state_eclipj2000(body_code: str, jd: float) -> np.ndarray:
    et = sp.unitim(jd, "JDTDB", "ET")
    observer = "EARTH" if body_code == "MOON" else "SUN"
    state, _ = sp.spkezr(TARGET_CODES[body_code], et, "ECLIPJ2000", "NONE", observer)
    return np.array(state, dtype=float) / AU_KM


def fit_segment(body_code: str,
                start_jd: float,
                end_jd: float,
                degree: int,
                sample_count: int,
                validation_count: int) -> tuple[dict[str, object], float, float]:
    mid_jd = (start_jd + end_jd) / 2.0
    span_days = (end_jd - start_jd) / 2.0
    sample_jds = np.linspace(start_jd, end_jd, sample_count)
    x = (sample_jds - mid_jd) / span_days
    samples = np.array([state_eclipj2000(body_code, jd) for jd in sample_jds])
    coefficients = [
        np.polynomial.polynomial.polyfit(x, samples[:, component], degree).tolist()
        for component in range(6)
    ]

    max_position_residual = 0.0
    max_velocity_residual = 0.0
    validation_jds = np.linspace(start_jd, end_jd, validation_count)
    validation_x = (validation_jds - mid_jd) / span_days
    for jd, xx in zip(validation_jds, validation_x):
        truth = state_eclipj2000(body_code, jd)
        fitted = np.array([
            np.polynomial.polynomial.polyval(xx, coefficients[component])
            for component in range(6)
        ])
        max_position_residual = max(max_position_residual, float(np.linalg.norm(fitted[:3] - truth[:3])))
        max_velocity_residual = max(max_velocity_residual, float(np.linalg.norm(fitted[3:] - truth[3:])))

    return {
        "start_jd": start_jd,
        "end_jd": end_jd,
        "mid_jd": mid_jd,
        "span_days": span_days,
        "coefficients": coefficients,
    }, max_position_residual, max_velocity_residual


def generate_model(args: argparse.Namespace) -> dict[str, object]:
    start_jd = jd_from_gregorian(args.start_year, 1, 1)
    end_jd = jd_from_gregorian(args.end_year + 1, 1, 1)
    rows = []
    jd = start_jd
    index = args.model_id_base
    max_position_residual = 0.0
    max_velocity_residual = 0.0

    while jd < end_jd - 1.0e-9:
        segment_end = min(jd + args.window_days, end_jd)
        row, pos_residual, vel_residual = fit_segment(
            args.body,
            jd,
            segment_end,
            args.degree,
            max(args.degree + 3, args.sample_count),
            max(args.degree + 5, args.validation_count),
        )
        row["index"] = index
        rows.append(row)
        max_position_residual = max(max_position_residual, pos_residual)
        max_velocity_residual = max(max_velocity_residual, vel_residual)
        jd = segment_end
        index += 1

    return {
        "description": (
            f"SPICE DE440 {args.body} ECLIPJ2000 state, AU and AU/day. "
            "Coefficients c0..cN in X=(JD-mid_jd)/span_days."
        ),
        "body_code": args.body,
        "degree": args.degree,
        "start_jd": start_jd,
        "end_jd": end_jd,
        "window_days": args.window_days,
        "rows": rows,
        "max_position_fit_residual_au": max_position_residual,
        "max_velocity_fit_residual_au_per_day": max_velocity_residual,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--body", default="EARTH_BARYCENTER", choices=sorted(TARGET_CODES))
    parser.add_argument("--start-year", type=int, default=1550)
    parser.add_argument("--end-year", type=int, default=2649)
    parser.add_argument("--window-days", type=float, default=180.0)
    parser.add_argument("--degree", type=int, default=9)
    parser.add_argument("--sample-count", type=int, default=21)
    parser.add_argument("--validation-count", type=int, default=31)
    parser.add_argument("--model-id-base", type=int, default=1)
    parser.add_argument("--kernel-dir", default="", help="Directory containing NAIF kernels")
    parser.add_argument("--output", default="tools/earth_barycenter_state_1550_2650.json")
    args = parser.parse_args()

    if args.degree != 9:
        raise RuntimeError("almanac.c currently expects ten c0..c9 coefficients per state component")

    load_spice(spice_kernel_dir(args.kernel_dir.strip() or None))
    model = generate_model(args)
    Path(args.output).write_text(json.dumps(model, indent=2) + "\n", encoding="utf-8")
    print(
        f"Generated {len(model['rows'])} {args.body} state-vector rows; "
        f"max position residual {model['max_position_fit_residual_au']:.6e} AU",
        file=sys.stderr,
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
