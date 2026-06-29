#!/usr/bin/env python3
"""Generate SPICE-backed oracle values for almanac regression tests."""

from __future__ import annotations

import argparse
import math
import os
from pathlib import Path
import sys

try:
    import erfa
except ModuleNotFoundError as exc:  # pragma: no cover
    print(f"Missing ERFA dependency for almanac oracle generation: {exc}", file=sys.stderr)
    raise SystemExit(1)

try:
    import spiceypy as sp
except ModuleNotFoundError as exc:  # pragma: no cover
    print(f"Missing SPICE dependency for almanac oracle generation: {exc}", file=sys.stderr)
    raise SystemExit(1)


KERNEL_NAMES = ("naif0012.tls", "pck00011.tpc", "de440.bsp")
DEFAULT_CASES = (
    ("SUN", "SUN", 1551, 1, 1, 0, 0, 0.0),
    ("MOON", "MOON", 1551, 1, 1, 0, 0, 0.0),
    ("MARS", "MARS BARYCENTER", 1551, 1, 1, 0, 0, 0.0),
    ("JUPITER", "JUPITER BARYCENTER", 1551, 1, 1, 0, 0, 0.0),
    ("SUN", "SUN", 1850, 1, 2, 0, 0, 0.0),
    ("MOON", "MOON", 1850, 1, 2, 0, 0, 0.0),
    ("MARS", "MARS BARYCENTER", 1850, 1, 2, 0, 0, 0.0),
    ("JUPITER", "JUPITER BARYCENTER", 1850, 1, 2, 0, 0, 0.0),
    ("SUN", "SUN", 2002, 1, 1, 22, 0, 0.0),
    ("MOON", "MOON", 2002, 1, 1, 22, 0, 0.0),
    ("MARS", "MARS BARYCENTER", 2002, 1, 1, 22, 0, 0.0),
    ("JUPITER", "JUPITER BARYCENTER", 2002, 1, 1, 22, 0, 0.0),
    ("SUN", "SUN", 2400, 1, 1, 0, 0, 0.0),
    ("MOON", "MOON", 2400, 1, 1, 0, 0, 0.0),
    ("MARS", "MARS BARYCENTER", 2400, 1, 1, 0, 0, 0.0),
    ("JUPITER", "JUPITER BARYCENTER", 2400, 1, 1, 0, 0, 0.0),
    ("SUN", "SUN", 2649, 1, 1, 0, 0, 0.0),
    ("MOON", "MOON", 2649, 1, 1, 0, 0, 0.0),
    ("MARS", "MARS BARYCENTER", 2649, 1, 1, 0, 0, 0.0),
    ("JUPITER", "JUPITER BARYCENTER", 2649, 1, 1, 0, 0, 0.0),
)


def jd_from_mars_civil(year: int, month: int, day: int, hour: int, minute: int, second: float) -> float:
    is_gregorian = (
        year > 1582
        or (year == 1582 and month > 10)
        or (year == 1582 and month == 10 and day >= 15)
    )
    y = year + (1 if year < 0 else 0)
    m = month
    if m <= 2:
        y -= 1
        m += 12

    b = 0
    if is_gregorian:
        a = int(y / 100)
        b = 2 - a + int(a / 4)

    jdn = int(1461 * y / 4) + b + int(306001 * (m + 1) / 10000) + day + 1720995
    return jdn + (hour - 12) / 24.0 + minute / 1440.0 + second / 86400.0


def mars_delta_t_seconds(year: int) -> float:
    if year < 1800:
        u = year - 1700
        return (((-0.0000000851788756 * u + 0.00013336) * u - 0.0059285) * u + 0.1603) * u + 8.83
    if year < 1860:
        u = year - 1800
        return (((((0.000000000875 * u - 0.0000001699) * u + 0.0000121272) * u - 0.00037436) * u + 0.0041116) * u + 0.0068612) * u + 13.72
    if year < 1900:
        u = year - 1860
        return ((((0.0000042886428 * u - 0.0004473624) * u + 0.01680668) * u - 0.251754) * u + 0.5737) * u + 7.62
    if year < 1920:
        u = year - 1900
        return (((-0.000197 * u + 0.0061966) * u - 0.0598939) * u + 1.494119) * u - 2.79
    if year < 1941:
        u = year - 1920
        return ((0.0020936 * u - 0.076100) * u + 0.84493) * u + 21.20
    if year < 1961:
        u = year - 1950
        return ((0.000392618767177 * u - 0.004291845493562231) * u + 0.407) * u + 29.107
    if year < 1986:
        u = year - 1975
        return ((-0.00139275766016713 * u - 0.00384615384615385) * u + 1.067) * u + 45.45
    if year < 2005:
        u = year - 2000
        return ((((0.00002373599 * u + 0.000651814) * u + 0.0017275) * u - 0.060374) * u + 0.3345) * u + 63.86
    if year < 2050:
        u = year - 2000
        return (0.005589 * u + 0.32217) * u + 62.92
    u = (year - 1820) / 100.0
    if year < 2150:
        return -20.0 + 32.0 * u * u - 0.5628 * (2150 - year)
    return -20.0 + 32.0 * u * u


def jd_tdb_from_mars_civil(year: int, month: int, day: int, hour: int, minute: int, second: float) -> float:
    jd_tt = jd_from_mars_civil(year, month, day, hour, minute, second) + mars_delta_t_seconds(year) / 86400.0
    g_degrees = 357.53 + 0.9856003 * (jd_tt - 2451545.0)
    g_radians = math.radians(g_degrees)
    correction_seconds = 0.001657 * math.sin(g_radians) + 0.000022 * math.sin(2.0 * g_radians)
    return jd_tt + correction_seconds / 86400.0


def kernel_dir(explicit: str | None) -> Path:
    if explicit:
        return Path(explicit).expanduser()
    env_value = os.environ.get("MARS_ALMANAC_KERNEL_DIR", "").strip()
    if env_value:
        return Path(env_value).expanduser()
    if (Path.home() / ".mars" / "almanac-kernels").exists():
        return Path.home() / ".mars" / "almanac-kernels"
    return Path("/tmp/mars-almanac-kernels")


def load_spice(kernels: Path) -> None:
    missing = [name for name in KERNEL_NAMES if not (kernels / name).exists()]
    if missing:
        raise RuntimeError(f"missing SPICE kernels in {kernels}: {', '.join(missing)}")
    sp.kclear()
    for name in KERNEL_NAMES:
        sp.furnsh(str(kernels / name))


def apparent_sha_dec(spice_target: str,
                     year: int,
                     month: int,
                     day: int,
                     hour: int,
                     minute: int,
                     second: float) -> tuple[float, float, float]:
    et = sp.unitim(jd_tdb_from_mars_civil(year, month, day, hour, minute, second), "JDTDB", "ET")
    state, _light_time = sp.spkezr(spice_target, et, "J2000", "LT+S", "EARTH")
    radius = math.sqrt(sum(component * component for component in state[:3]))
    direction = [component / radius for component in state[:3]]

    tt_jd = sp.unitim(et, "ET", "JDTDT")
    rbpn = erfa.pnm06a(2400000.5, tt_jd - 2400000.5)
    true_direction = [
        sum(rbpn[row][column] * direction[column] for column in range(3))
        for row in range(3)
    ]
    ra_degrees = math.degrees(math.atan2(true_direction[1], true_direction[0])) % 360.0
    dec_degrees = math.degrees(math.atan2(true_direction[2], math.hypot(true_direction[0], true_direction[1])))
    sha_degrees = (360.0 - ra_degrees) % 360.0
    return sha_degrees, dec_degrees, radius / 149597870.7


def render_header() -> str:
    lines = [
        "/* Generated by tools/generate_almanac_spice_oracle.py.",
        " * Oracle: SPICE DE440 apparent target vector from Earth (LT+S),",
        " * rotated to true equator/equinox of date with ERFA IAU 2006/2000A.",
        " * Dates are interpreted as MARS civil GMT, including the Julian/Gregorian switch.",
        " */",
        "#ifndef TESTS_ALMANAC_SPICE_ORACLE_H",
        "#define TESTS_ALMANAC_SPICE_ORACLE_H",
        "",
        "typedef struct almanac_spice_oracle_case_t {",
        "    const char *body_code;",
        "    int year;",
        "    int month;",
        "    int day;",
        "    int hour;",
        "    int minute;",
        "    double second;",
        "    double sha_degrees;",
        "    double declination_degrees;",
        "    double geocentric_distance_au;",
        "} almanac_spice_oracle_case_t;",
        "",
        "static const almanac_spice_oracle_case_t ALMANAC_SPICE_ORACLE_CASES[] = {",
    ]

    for body_code, spice_target, year, month, day, hour, minute, second in DEFAULT_CASES:
        sha, dec, distance = apparent_sha_dec(spice_target, year, month, day, hour, minute, second)
        lines.append(
            f'    {{"{body_code}", {year}, {month}, {day}, {hour}, {minute}, {second:.1f}, '
            f"{sha:.12f}, {dec:.12f}, {distance:.12f}}},"
        )

    lines += [
        "};",
        "",
        "static const unsigned int ALMANAC_SPICE_ORACLE_CASE_COUNT =",
        "    (unsigned int)(sizeof(ALMANAC_SPICE_ORACLE_CASES) / sizeof(ALMANAC_SPICE_ORACLE_CASES[0]));",
        "",
        "#endif",
    ]
    return "\n".join(lines) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--kernel-dir", default="", help="Directory containing NAIF kernels")
    parser.add_argument("--output", default="tests/almanac/spice_oracle.h", help="Header to write")
    args = parser.parse_args()

    load_spice(kernel_dir(args.kernel_dir.strip() or None))
    Path(args.output).write_text(render_header(), encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
