#!/usr/bin/env python3
"""Generate compact Chebyshev position ephemeris rows for the almanac DB."""

from __future__ import annotations

import argparse
import math
import os
from pathlib import Path
import struct
import sys

try:
    import numpy as np
except ModuleNotFoundError as exc:  # pragma: no cover
    print(f"Missing NumPy dependency for Chebyshev generation: {exc}", file=sys.stderr)
    raise SystemExit(1)

try:
    import spiceypy as sp
except ModuleNotFoundError as exc:  # pragma: no cover
    print(f"Missing SPICE dependency for Chebyshev generation: {exc}", file=sys.stderr)
    raise SystemExit(1)


AU_KM = 149597870.7
KERNEL_NAMES = ("naif0012.tls", "pck00011.tpc", "de440.bsp")
TARGETS = {
    "EARTH_BARYCENTER": ("EARTH BARYCENTER", "SUN", "SUN", "NONE"),
    "MOON": ("MOON", "EARTH", "EARTH", "NONE"),
    "MERCURY": ("MERCURY BARYCENTER", "SUN", "SUN", "NONE"),
    "VENUS": ("VENUS BARYCENTER", "SUN", "SUN", "NONE"),
    "MARS": ("MARS BARYCENTER", "SUN", "SUN", "NONE"),
    "JUPITER": ("JUPITER BARYCENTER", "SUN", "SUN", "NONE"),
    "SATURN": ("SATURN BARYCENTER", "SUN", "SUN", "NONE"),
}
DEFAULT_BODY_OPTIONS = {
    "EARTH_BARYCENTER": (16.0, 12),
    "MOON": (8.0, 9),
    "MERCURY": (8.0, 13),
    "VENUS": (16.0, 9),
    "MARS": (32.0, 10),
    "JUPITER": (32.0, 7),
    "SATURN": (32.0, 6),
}
BODY_REF_IDS = {
    "SUN": 1,
    "EARTH_BARYCENTER": 2,
    "MOON": 3,
    "MERCURY": 4,
    "VENUS": 5,
    "MARS": 6,
    "JUPITER": 7,
    "SATURN": 8,
    "EARTH": 9,
}
FRAME_IDS = {
    "ECLIPJ2000": 1,
}


def kernel_dir(explicit: str) -> Path:
    if explicit:
        return Path(explicit).expanduser()
    env_value = os.environ.get("MARS_ALMANAC_KERNEL_DIR", "").strip()
    if env_value:
        return Path(env_value).expanduser()
    home_dir = Path.home() / ".mars" / "almanac-kernels"
    if home_dir.exists():
        return home_dir
    return Path("/tmp/mars-almanac-kernels")


def load_spice(path: Path) -> None:
    missing = [name for name in KERNEL_NAMES if not (path / name).exists()]
    if missing:
        raise RuntimeError(f"missing SPICE kernels in {path}: {', '.join(missing)}")
    sp.kclear()
    for name in KERNEL_NAMES:
        sp.furnsh(str(path / name))


def jd_from_gregorian(year: int, month: int, day: int) -> float:
    a = (14 - month) // 12
    y = year + 4800 - a
    m = month + 12 * a - 3
    jdn = day + ((153 * m + 2) // 5) + 365 * y + y // 4 - y // 100 + y // 400 - 32045
    return float(jdn) - 0.5


def state_position(body_code: str, jd: float) -> np.ndarray:
    target, observer, _center_code, aberration = TARGETS[body_code]
    et = sp.unitim(jd, "JDTDB", "ET")
    state, _ = sp.spkezr(target, et, "ECLIPJ2000", aberration, observer)
    return np.array(state[:3], dtype=float) / AU_KM


def state_positions(body_code: str, jds: np.ndarray) -> np.ndarray:
    target, observer, _center_code, aberration = TARGETS[body_code]
    ets = [sp.unitim(float(jd), "JDTDB", "ET") for jd in jds]
    states, _ = sp.spkezr(target, ets, "ECLIPJ2000", aberration, observer)
    return np.array(states, dtype=float)[:, :3] / AU_KM


def fit_record(body_code: str,
               start_jd: float,
               end_jd: float,
               degree: int,
               sample_count: int,
               validation_count: int) -> tuple[bytes, float]:
    mid_jd = 0.5 * (start_jd + end_jd)
    radius_days = 0.5 * (end_jd - start_jd)
    sample_jds = np.linspace(start_jd, end_jd, sample_count)
    x = (sample_jds - mid_jd) / radius_days
    samples = state_positions(body_code, sample_jds)
    coeff = [
        np.polynomial.chebyshev.chebfit(x, samples[:, component], degree)
        for component in range(3)
    ]

    max_residual = 0.0
    validation_jds = np.linspace(start_jd, end_jd, validation_count)
    validation_x = (validation_jds - mid_jd) / radius_days
    validation_truth = state_positions(body_code, validation_jds)
    for truth, xx in zip(validation_truth, validation_x):
        fitted = np.array([
            np.polynomial.chebyshev.chebval(xx, coeff[component])
            for component in range(3)
        ])
        max_residual = max(max_residual, float(np.linalg.norm(fitted - truth)))

    packed = b"".join(
        struct.pack("<d", float(value))
        for component in range(3)
        for value in coeff[component]
    )
    return packed, max_residual


def sql_quote(text: str) -> str:
    return "'" + text.replace("'", "''") + "'"


def iter_body_options(args: argparse.Namespace) -> list[tuple[str, float, int]]:
    if args.body:
        bodies = [body.strip() for body in args.body.split(",") if body.strip()]
        unknown = [body for body in bodies if body not in TARGETS]
        if unknown:
            raise RuntimeError(f"unknown body code(s): {', '.join(unknown)}")
        if len(bodies) > 1 and (args.window_days or args.degree):
            raise RuntimeError("--window-days and --degree are only supported with a single --body")
        if len(bodies) == 1:
            body = bodies[0]
            return [(body, args.window_days or DEFAULT_BODY_OPTIONS[body][0],
                     args.degree or DEFAULT_BODY_OPTIONS[body][1])]
        return [(body, DEFAULT_BODY_OPTIONS[body][0], DEFAULT_BODY_OPTIONS[body][1]) for body in bodies]
    return [
        (body, window, degree)
        for body, (window, degree) in DEFAULT_BODY_OPTIONS.items()
    ]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--body", default="", help="Body code, comma-separated body codes, or blank for all")
    parser.add_argument("--start-year", type=int, default=1550)
    parser.add_argument("--end-year", type=int, default=2649)
    parser.add_argument("--window-days", type=float, default=0.0)
    parser.add_argument("--degree", type=int, default=0)
    parser.add_argument("--sample-count", type=int, default=0)
    parser.add_argument("--validation-count", type=int, default=0)
    parser.add_argument("--model-id-base", type=int, default=1)
    parser.add_argument("--kernel-dir", default="")
    parser.add_argument("--output", default="packaging/almanac-db/mars_almanac_chebyshev.sql")
    args = parser.parse_args()

    load_spice(kernel_dir(args.kernel_dir))

    start_jd = jd_from_gregorian(args.start_year, 1, 1)
    end_jd = jd_from_gregorian(args.end_year + 1, 1, 1)
    series_id = args.model_id_base
    series_rows = []
    diagnostics = []
    body_options = []

    for body_code, window_days, degree in iter_body_options(args):
        center_code = TARGETS[body_code][2]
        body_rows = int(math.ceil((end_jd - start_jd) / window_days))
        body_options.append((series_id, body_code, window_days, degree, body_rows))
        series_rows.append(
            f"({series_id}, {BODY_REF_IDS[body_code]}, {BODY_REF_IDS[center_code]}, {FRAME_IDS['ECLIPJ2000']}, "
            f"{start_jd:.8f}, {end_jd:.8f}, {window_days:.8f}, {body_rows}, {degree}, 3)"
        )
        series_id += 1

    out = Path(args.output)
    with out.open("w", encoding="utf-8") as handle:
        handle.write(
            "-- Generated by tools/generate_almanac_chebyshev_sql.py\n"
            "-- Position-only Chebyshev coefficients in AU, frame ECLIPJ2000.\n"
            "INSERT INTO almanac_chebyshev_position_series "
            "(series_id, body_ref_id, center_ref_id, frame_id, start_jd, end_jd, segment_span_days, "
            "segment_count, degree, component_count) VALUES\n"
            + ",\n".join(series_rows)
            + ";\n\n"
            "INSERT INTO almanac_chebyshev_position_segment "
            "(series_id, segment_index, coefficient_blob) VALUES\n"
        )

        first_segment = True
        for body_series_id, body_code, window_days, degree, expected_rows in body_options:
            jd = start_jd
            body_rows = 0
            body_max_residual = 0.0
            print(
                f"{body_code}: fitting {expected_rows} rows "
                f"(window {window_days:g} days, degree {degree})",
                file=sys.stderr,
                flush=True,
            )
            sample_count = max(degree + 3, args.sample_count or degree + 8)
            validation_count = max(degree + 5, args.validation_count or degree + 12)
            while jd < end_jd - 1.0e-9:
                record_end = min(jd + window_days, end_jd)
                blob, residual = fit_record(body_code, jd, record_end, degree, sample_count, validation_count)
                body_max_residual = max(body_max_residual, residual)
                if not first_segment:
                    handle.write(",\n")
                handle.write(
                    f"({body_series_id}, {body_rows}, X'{blob.hex()}')"
                )
                first_segment = False
                body_rows += 1
                if body_rows % 500 == 0 or body_rows == expected_rows:
                    print(
                        f"{body_code}: {body_rows}/{expected_rows} rows, "
                        f"max residual {body_max_residual:.6e} AU",
                        file=sys.stderr,
                        flush=True,
                    )
                jd = record_end
            diagnostics.append((body_code, body_rows, body_max_residual))
        handle.write(";\n")

    for body_code, row_count, residual in diagnostics:
        print(
            f"{body_code}: {row_count} rows, max fit residual {residual:.6e} AU",
            file=sys.stderr,
        )
    print(f"Wrote {len(series_rows)} series and {sum(row_count for _, row_count, _ in diagnostics)} compact Chebyshev rows to {out}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
