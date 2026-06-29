#!/usr/bin/env python3
"""Generate SPICE-backed apparent Moon data for AstroNav 2000-2040.ods.

The workbook consumes these rows directly from a hidden-ish data sheet.  Values
are fitted in the true equator/equinox of date so the Moon calculation does not
depend on the workbook's lower-precision precession/nutation transform.
"""

from __future__ import annotations

import datetime as dt
import json
import math
from pathlib import Path

import numpy as np
import spiceypy as sp

try:
    import erfa
except ImportError as exc:  # pragma: no cover - helper script dependency guard
    raise SystemExit("pyerfa is required: install it in the SPICE helper venv") from exc


AU_KM = 149_597_870.700
J2000_JD = 2451545.0
KERNEL_DIR = Path("/tmp/mars-almanac-kernels")
OUT_PATH = Path("tools/moon_state_2000_2064.json")

START_DATE = dt.date(2000, 1, 1)
# Exclusive civil-date end, with an extra day so 2064-12-31 GMT plus Delta-T is covered.
END_DATE = dt.date(2065, 1, 2)
SEGMENT_SPAN_DAYS = 8.0
DEGREE = 10
SAMPLES_PER_SEGMENT = DEGREE + 8
POST_2040_YEARS = [*range(2041, 2051), *range(2060, 2065)]


def furnish_kernels() -> None:
    for filename in ("naif0012.tls", "pck00011.tpc", "de440s.bsp"):
        kernel = KERNEL_DIR / filename
        if not kernel.exists():
            raise SystemExit(f"Missing SPICE kernel: {kernel}")
        sp.furnsh(str(kernel))


def jd_from_date(value: dt.date) -> float:
    a = (14 - value.month) // 12
    y = value.year + 4800 - a
    m = value.month + 12 * a - 3
    jdn = value.day + ((153 * m + 2) // 5) + 365 * y + y // 4 - y // 100 + y // 400 - 32045
    return float(jdn) - 0.5


def jd_from_datetime(value: dt.datetime) -> float:
    return jd_from_date(value.date()) + (
        value.hour + value.minute / 60 + value.second / 3600 + value.microsecond / 3_600_000_000
    ) / 24


def delta_t_seconds(year: int) -> float:
    """Enough for workbook validation sampling; the workbook supplies its own TDB JD."""
    if year < 2050:
        t = year - 2000
        return 62.92 + 0.32217 * t + 0.005589 * t * t
    u = (year - 1820) / 100
    return -20 + 32 * u * u


def true_obliquity(jd_tdb: float) -> float:
    date1 = 2400000.5
    date2 = jd_tdb - date1
    _dpsi, deps = erfa.nut06a(date1, date2)
    return erfa.obl06(date1, date2) + deps


def apparent_moon_components(jd_tdb: float) -> np.ndarray:
    """Return true-date apparent unit vector, distance AU, and ecliptic longitude."""
    et = (jd_tdb - J2000_JD) * 86400.0
    state, _lt = sp.spkezr("MOON", et, "J2000", "LT+S", "EARTH")
    pos = np.array(state[:3], dtype=float)
    distance_au = float(np.linalg.norm(pos) / AU_KM)

    rbpn = erfa.pnm06a(2400000.5, jd_tdb - 2400000.5)
    true_pos = rbpn @ (pos / np.linalg.norm(pos))
    true_pos = true_pos / np.linalg.norm(true_pos)

    eps = true_obliquity(jd_tdb)
    cos_eps = math.cos(eps)
    sin_eps = math.sin(eps)
    ecl_y = true_pos[1] * cos_eps + true_pos[2] * sin_eps
    ecl_lon = math.atan2(ecl_y, true_pos[0])

    return np.array([true_pos[0], true_pos[1], true_pos[2], distance_au, ecl_lon], dtype=float)


def generate_rows() -> list[dict[str, object]]:
    rows: list[dict[str, object]] = []
    start_jd = jd_from_date(START_DATE)
    end_jd = jd_from_date(END_DATE)
    current = start_jd
    index = 1
    while current < end_jd - 1e-9:
        segment_end = min(current + SEGMENT_SPAN_DAYS, end_jd)
        mid_jd = (current + segment_end) / 2
        half_span = (segment_end - current) / 2
        xs = np.linspace(-1.0, 1.0, SAMPLES_PER_SEGMENT)
        jds = mid_jd + xs * half_span
        values = np.array([apparent_moon_components(float(jd)) for jd in jds])
        values[:, 4] = np.unwrap(values[:, 4])
        values[:, 4] = np.degrees(values[:, 4])

        coeffs = []
        for component in range(values.shape[1]):
            coeffs.append(
                [float(value) for value in np.polynomial.polynomial.polyfit(xs, values[:, component], DEGREE)]
            )

        rows.append(
            {
                "index": index,
                "start_jd": current,
                "end_jd": segment_end,
                "mid_jd": mid_jd,
                "span_days": half_span,
                "coefficients": coeffs,
            }
        )
        current = segment_end
        index += 1
    return rows


def eval_row(row: dict[str, object], jd_tdb: float) -> np.ndarray:
    x = (jd_tdb - float(row["mid_jd"])) / float(row["span_days"])
    coeffs = row["coefficients"]
    out = np.array([np.polynomial.polynomial.polyval(x, component) for component in coeffs], dtype=float)
    out[:3] /= np.linalg.norm(out[:3])
    return out


def find_row(rows: list[dict[str, object]], jd_tdb: float) -> dict[str, object]:
    start_jd = float(rows[0]["start_jd"])
    raw_index = int(math.floor((jd_tdb - start_jd) / SEGMENT_SPAN_DAYS))
    index = min(max(raw_index, 0), len(rows) - 1)
    row = rows[index]
    if float(row["start_jd"]) <= jd_tdb <= float(row["end_jd"]):
        return row
    # Last segment can be shorter; fall back to a tiny linear search at edges.
    for row in rows:
        if float(row["start_jd"]) <= jd_tdb <= float(row["end_jd"]):
            return row
    raise ValueError(f"No Moon state row covers JD {jd_tdb}")


def angle_diff_arcsec(a: float, b: float) -> float:
    return abs((a - b + math.pi) % (2 * math.pi) - math.pi) * 206264.80624709636


def ra_dec(vec: np.ndarray) -> tuple[float, float]:
    vec = vec / np.linalg.norm(vec)
    return math.atan2(vec[1], vec[0]) % (2 * math.pi), math.asin(vec[2])


def workbook_gha_aries(jd_gmt: float) -> float:
    """Match the workbook's existing GHA Aries calculation closely enough for validation."""
    t = (jd_gmt - J2000_JD) / 36525.0
    eps = (((0.000000504 * t - 0.00000016) * t - 0.0130042) * t + 23.439291) * math.pi / 180
    d = jd_gmt - 2449352.5
    psi1 = (241.1 - 0.053 * d) * math.pi / 180
    psi2 = (198.9 + 1.971 * d) * math.pi / 180
    del_psi = (-0.0048 * math.sin(psi1) - 0.0004 * math.sin(psi2)) * math.pi / 180

    jo = math.floor(jd_gmt + 0.5) - 0.5
    ut = jd_gmt - jo
    t0 = (jo - J2000_JD) / 36525.0
    gmst = ((0.093104 - 0.0000062 * t0) * t0 + 8640184.812866) * t0 + 24110.54841
    gmst = gmst / 3600 + 1.00273791 * ut * 24
    return (gmst * 15 + del_psi * math.cos(eps) * 180 / math.pi) % 360


def oracle_gha_aries(jd_gmt: float, jd_tdb: float) -> float:
    return math.degrees(
        erfa.gst06a(2400000.5, jd_gmt - 2400000.5, 2400000.5, jd_tdb - 2400000.5)
    ) % 360


def validation_summary(rows: list[dict[str, object]]) -> dict[str, object]:
    in_range_worst = 0.0
    in_range_date = ""
    post_years: dict[str, float] = {}

    sample_years = [*range(2000, 2041), *POST_2040_YEARS]
    for year in sample_years:
        year_worst = 0.0
        date = dt.date(year, 1, 1)
        while date.year == year:
            jd_gmt = jd_from_datetime(dt.datetime.combine(date, dt.time(22, 0)))
            jd_tdb = jd_gmt + delta_t_seconds(year) / 86400.0

            got = eval_row(find_row(rows, jd_tdb), jd_tdb)
            ref = apparent_moon_components(jd_tdb)
            got_ra, got_dec = ra_dec(got[:3])
            ref_ra, ref_dec = ra_dec(ref[:3])
            got_gha = math.radians((workbook_gha_aries(jd_gmt) - math.degrees(got_ra)) % 360)
            ref_gha = math.radians((oracle_gha_aries(jd_gmt, jd_tdb) - math.degrees(ref_ra)) % 360)

            sample_worst = max(
                angle_diff_arcsec(got_ra, ref_ra),
                abs(got_dec - ref_dec) * 206264.80624709636,
                angle_diff_arcsec(got_gha, ref_gha),
            )
            year_worst = max(year_worst, sample_worst)
            if 2000 <= year <= 2040 and sample_worst > in_range_worst:
                in_range_worst = sample_worst
                in_range_date = date.isoformat()
            date += dt.timedelta(days=7)
        if year in POST_2040_YEARS:
            post_years[str(year)] = year_worst

    return {
        "sampling": "weekly at 22:00 GMT",
        "in_range_worst_arcsec": in_range_worst,
        "in_range_worst_date": in_range_date,
        "post_2040_worst_arcsec_by_year": post_years,
    }


def main() -> None:
    furnish_kernels()
    rows = generate_rows()
    model = {
        "description": "SPICE DE440s apparent geocentric Moon in true equator/equinox of date",
        "kernel": "de440s.bsp",
        "aberration": "LT+S",
        "frame": "true_equator_equinox_of_date",
        "start_date_gmt": START_DATE.isoformat(),
        "end_date_gmt_exclusive": END_DATE.isoformat(),
        "start_jd": jd_from_date(START_DATE),
        "end_jd": jd_from_date(END_DATE),
        "segment_span_days": SEGMENT_SPAN_DAYS,
        "degree": DEGREE,
        "components": ["unit_x", "unit_y", "unit_z", "distance_au", "ecliptic_longitude_deg"],
        "rows": rows,
        "validation": validation_summary(rows),
    }
    OUT_PATH.write_text(json.dumps(model, separators=(",", ":")), encoding="utf-8")
    print(f"Wrote {OUT_PATH} with {len(rows)} Moon-state segments")
    print(json.dumps(model["validation"], indent=2))


if __name__ == "__main__":
    main()
