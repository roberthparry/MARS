#!/usr/bin/env python3
"""
Small local GUI for experimenting with MARS mathematics.

The app serves a single browser page on localhost and evaluates expressions by
delegating to a scratch binary such as build/release/scratch/mars_lab. The
browser gives us a proper GUI surface without adding a desktop toolkit
dependency to the project.
"""

from __future__ import annotations

import argparse
import datetime as py_datetime
import errno
import ipaddress
from decimal import Decimal, InvalidOperation, localcontext
import html
import http.server
import json
import math
import os
from pathlib import Path
import re
import secrets
import socket
import subprocess
import sys
import tempfile
import threading
import urllib.error
import urllib.parse
import urllib.request
import webbrowser
import shutil


ROOT = Path(__file__).resolve().parents[1]
JURISDICTION_DB_SOURCE_DIR = ROOT / "packaging" / "jurisdiction-db"
COUNTRY_JURISDICTIONS_SQL = JURISDICTION_DB_SOURCE_DIR / "mars_country_jurisdictions.sql"
TARGET_SUBDIVISIONS_SQL = JURISDICTION_DB_SOURCE_DIR / "mars_target_subdivisions.sql"
JURISDICTION_DB_PATH_ENV = "MARS_JURISDICTION_DB_PATH"
JURISDICTION_DB_KEY_ENV = "MARS_JURISDICTION_DB_KEY"
LEGACY_HOLIDAY_DB_PATH_ENV = "MARS_HOLIDAY_DB_PATH"
LEGACY_HOLIDAY_DB_KEY_ENV = "MARS_HOLIDAY_DB_KEY"


def detect_system_locale_country_code() -> str:
    for key in ("LC_ALL", "LC_MESSAGES", "LANG"):
        raw = os.environ.get(key, "").strip()
        if not raw:
            continue
        match = re.match(r"^[A-Za-z]+(?:[_-]([A-Za-z]{2}))(?:[.@].*)?$", raw)
        if not match:
            continue
        return match.group(1).upper()
    return ""


def detect_system_timezone_name() -> str:
    tz_env = os.environ.get("TZ", "").strip()
    if tz_env:
        return tz_env
    localtime_path = "/etc/localtime"
    try:
        resolved = os.path.realpath(localtime_path)
    except OSError:
        return ""
    marker = "/zoneinfo/"
    if marker in resolved:
        return resolved.split(marker, 1)[1]
    return ""


def infer_defaults_from_timezone() -> tuple[str, str, str]:
    timezone_name = detect_system_timezone_name()
    candidates = [
        ("Australia/", ("-33.8688", "151.2093", "AU")),
        ("Pacific/Auckland", ("-36.8485", "174.7633", "NZ")),
        ("Africa/Johannesburg", ("-26.2041", "28.0473", "ZA")),
        ("Europe/Amsterdam", ("52.3676", "4.9041", "NL")),
        ("Europe/Copenhagen", ("55.6761", "12.5683", "DK")),
        ("Europe/Dublin", ("53.3498", "-6.2603", "IE")),
        ("Europe/Lisbon", ("38.7223", "-9.1393", "PT")),
        ("Europe/Rome", ("41.9028", "12.4964", "IT")),
        ("Europe/Athens", ("37.9838", "23.7275", "GR")),
        ("Europe/Berlin", ("52.52", "13.405", "DE")),
        ("Europe/Paris", ("48.8566", "2.3522", "FR")),
        ("Europe/London", ("51.5074", "-0.1278", "GB-ENG")),
        ("America/Toronto", ("43.6532", "-79.3832", "CA")),
        ("America/Vancouver", ("49.2827", "-123.1207", "CA")),
        ("America/Halifax", ("44.6488", "-63.5752", "CA")),
        ("America/", ("40.7128", "-74.006", "US")),
    ]
    for prefix, defaults in candidates:
        if timezone_name.startswith(prefix):
            return defaults
    return ("51.5074", "-0.1278", "GB-ENG")


def locale_country_to_holiday_jurisdiction(country_code: str) -> str:
    country_code = str(country_code or "").strip().upper()
    if not country_code:
        return ""
    if country_code == "GB":
        return "GB-ENG"
    return country_code


DEFAULT_TIMEZONE_LATITUDE, DEFAULT_TIMEZONE_LONGITUDE, DEFAULT_HOLIDAY_JURISDICTION_FROM_TIMEZONE = infer_defaults_from_timezone()
DEFAULT_HOLIDAY_JURISDICTION_FROM_LOCALE = locale_country_to_holiday_jurisdiction(
    detect_system_locale_country_code()
)


def mars_home_dir() -> Path:
    custom = os.environ.get("MARS_HOME", "").strip()
    if custom:
        return Path(custom).expanduser()
    return Path.home() / ".mars"


def config_env_path(name: str) -> Path:
    return mars_home_dir() / "config" / name


def read_env_like_value(path: Path, variable_name: str) -> str:
    try:
        lines = path.read_text(encoding="utf-8").splitlines()
    except OSError:
        return ""

    prefix = f"{variable_name}="
    export_prefix = f"export {variable_name}="
    for raw_line in lines:
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue
        if line.startswith(export_prefix):
            value = line[len(export_prefix):].strip()
        elif line.startswith(prefix):
            value = line[len(prefix):].strip()
        else:
            continue
        if len(value) >= 2 and ((value[0] == "'" and value[-1] == "'") or (value[0] == '"' and value[-1] == '"')):
            value = value[1:-1]
        return value.strip()
    return ""


def default_jurisdiction_db_path() -> Path:
    return mars_home_dir() / "jurisdiction" / "mars_jurisdiction_rules.db"


def jurisdiction_db_runtime_env() -> dict[str, str]:
    env_updates: dict[str, str] = {}
    configured_path = os.environ.get(JURISDICTION_DB_PATH_ENV, "").strip()
    if not configured_path:
        configured_path = os.environ.get(LEGACY_HOLIDAY_DB_PATH_ENV, "").strip()
    db_path = Path(configured_path).expanduser() if configured_path else default_jurisdiction_db_path()
    env_updates[JURISDICTION_DB_PATH_ENV] = str(db_path)
    env_updates[LEGACY_HOLIDAY_DB_PATH_ENV] = str(db_path)

    configured_key = os.environ.get(JURISDICTION_DB_KEY_ENV, "").strip()
    if not configured_key:
        configured_key = os.environ.get(LEGACY_HOLIDAY_DB_KEY_ENV, "").strip()
    if configured_key:
        env_updates[JURISDICTION_DB_KEY_ENV] = configured_key
        env_updates[LEGACY_HOLIDAY_DB_KEY_ENV] = configured_key

    return env_updates


def load_holiday_jurisdiction_options() -> list[tuple[str, str]]:
    country_pattern = re.compile(
        r"\s*\('([^']+)', NULL, 'country', '[^']+', NULL, '[^']+', '([^']*)',"
    )
    subdivision_pattern = re.compile(
        r"\s*\('([^']+)', '([^']+)', 'subdivision', '[^']+', '[^']+', '[^']+', '([^']*)',"
    )
    country_names: dict[str, str] = {}
    subdivisions_by_parent: dict[str, list[tuple[str, str]]] = {}

    try:
        lines = COUNTRY_JURISDICTIONS_SQL.read_text(encoding="utf-8").splitlines()
    except OSError:
        lines = []

    for line in lines:
        match = country_pattern.match(line)
        if not match:
            continue
        code, name = match.groups()
        if code == "GB":
            name = "United Kingdom"
        country_names[code] = name

    try:
        subdivision_lines = TARGET_SUBDIVISIONS_SQL.read_text(encoding="utf-8").splitlines()
    except OSError:
        subdivision_lines = []

    for line in subdivision_lines:
        match = subdivision_pattern.match(line)
        if not match:
            continue
        code, parent_code, name = match.groups()
        subdivisions_by_parent.setdefault(parent_code, []).append((code, name))

    options: list[tuple[str, str]] = []
    for country_code, country_name in sorted(country_names.items(), key=lambda item: item[1]):
        if country_code == "GB":
            options.append(("GB-ENG", "United Kingdom - England"))
            options.extend(
                (code, f"United Kingdom - {name}")
                for code, name in sorted(subdivisions_by_parent.get("GB", []), key=lambda item: item[1])
            )
            continue
        options.append((country_code, country_name))
        options.extend(
            (code, f"{country_name} - {name}")
            for code, name in sorted(subdivisions_by_parent.get(country_code, []), key=lambda item: item[1])
        )

    return options


HOLIDAY_JURISDICTION_OPTIONS = load_holiday_jurisdiction_options()
VALID_HOLIDAY_JURISDICTIONS = {code for code, _ in HOLIDAY_JURISDICTION_OPTIONS}
DEFAULT_HOLIDAY_JURISDICTION = (
    DEFAULT_HOLIDAY_JURISDICTION_FROM_LOCALE
    if DEFAULT_HOLIDAY_JURISDICTION_FROM_LOCALE in VALID_HOLIDAY_JURISDICTIONS
    else DEFAULT_HOLIDAY_JURISDICTION_FROM_TIMEZONE
)
HOLIDAY_JURISDICTION_OPTIONS_HTML = "\n".join(
    (
        f'          <option value="{html.escape(code)}" selected>{html.escape(name)}</option>'
        if code == DEFAULT_HOLIDAY_JURISDICTION
        else f'          <option value="{html.escape(code)}">{html.escape(name)}</option>'
    )
    for code, name in HOLIDAY_JURISDICTION_OPTIONS
)
ALMANAC_BODY_OPTIONS = (
    ("MOON", "Moon"),
    ("SUN", "Sun"),
    ("VENUS", "Venus"),
    ("MARS", "Mars"),
    ("JUPITER", "Jupiter"),
    ("SATURN", "Saturn"),
    ("SIRIUS", "Sirius"),
    ("POLARIS", "Polaris"),
)


def infer_holiday_jurisdiction(latitude: float, longitude: float) -> str:
    boxes = [
        ("AU", -44.5, -10.0, 112.0, 154.5),
        ("NZ", -48.5, -33.0, 165.0, 179.9),
        ("ZA", -35.5, -21.0, 16.0, 33.5),
        ("NL", 50.0, 54.2, 3.0, 8.0),
        ("DK", 54.0, 58.0, 7.5, 15.5),
        ("IE", 51.0, 55.8, -11.0, -5.0),
        ("PT", 36.5, 42.5, -10.0, -6.0),
        ("GR", 34.0, 42.5, 19.0, 29.5),
        ("IT", 35.0, 47.5, 6.0, 19.0),
        ("FR", 41.0, 51.5, -5.5, 10.5),
        ("DE", 47.0, 55.5, 5.0, 16.5),
        ("GB-ENG", 49.5, 56.2, -7.8, 2.2),
        ("CA", 41.0, 84.5, -141.5, -52.0),
        ("US", 18.0, 72.0, -171.0, -66.0),
    ]
    for jurisdiction, min_lat, max_lat, min_lon, max_lon in boxes:
        if min_lat <= latitude <= max_lat and min_lon <= longitude <= max_lon:
            return jurisdiction
    return DEFAULT_HOLIDAY_JURISDICTION


def normalize_holiday_jurisdiction(value: str) -> str:
    jurisdiction = str(value or "").strip()
    if jurisdiction in VALID_HOLIDAY_JURISDICTIONS:
        return jurisdiction
    return DEFAULT_HOLIDAY_JURISDICTION


LAB_APP_NAME = os.environ.get("MARS_LAB_APP_NAME", "MARS Lab").strip() or "MARS Lab"
LAB_SHORT_NAME = os.environ.get("MARS_LAB_SHORT_NAME", LAB_APP_NAME).strip() or LAB_APP_NAME
LAB_DESCRIPTION = os.environ.get(
    "MARS_LAB_DESCRIPTION",
    "Explore MARS mathematics with rendered TeX.",
).strip() or "Explore MARS mathematics with rendered TeX."
LAB_SUBTITLE = os.environ.get(
    "MARS_LAB_SUBTITLE",
    "Switch between expression, equation, matrix, integrator, and datetime experiments. Each mode runs through a local MARS scratch binary and shows the result on the right.",
).strip() or "Switch between expression, equation, matrix, integrator, and datetime experiments. Each mode runs through a local MARS scratch binary and shows the result on the right."
LAB_THEME = os.environ.get("MARS_LAB_THEME", "mars").strip().lower() or "mars"
DEFAULT_SCRATCH_TARGET = os.environ.get("MARS_LAB_SCRATCH_TARGET", "scratch/mars_lab").strip() or "scratch/mars_lab"
DEFAULT_BIN = ROOT / os.environ.get("MARS_LAB_BINARY", "build/release/scratch/mars_lab")
DEFAULT_MATRIX_BIN = ROOT / "build" / "release" / "scratch" / "matrix_lab"
DEFAULT_INTEGRATOR_BIN = ROOT / "build" / "release" / "scratch" / "integrator_lab"
DEFAULT_EQUATION_BIN = ROOT / "build" / "release" / "scratch" / "equation_lab"
DEFAULT_DATETIME_BIN = ROOT / "build" / "release" / "scratch" / "datetime_lab"
DEFAULT_ALMANAC_BIN = ROOT / "build" / "release" / "scratch" / "almanac_lab"
DEFAULT_HOLIDAY_BIN = ROOT / "build" / "release" / "scratch" / "holiday_lab"
STATE_FILE = ROOT / os.environ.get("MARS_LAB_STATE_FILE", ".mars_lab_state.json")
LAB_ICON_FILE = ROOT / "packaging" / "linux" / "mars-lab.svg"
LAB_FAVICON_FILE = LAB_ICON_FILE
LAB_TOUCH_ICON_FILE = ROOT / "packaging" / "linux" / "icon-concepts" / "wizard-prism-180.png"
LAB_ICON_192_FILE = ROOT / "packaging" / "linux" / "icon-concepts" / "wizard-prism-192.png"
LAB_ICON_512_FILE = ROOT / "packaging" / "linux" / "icon-concepts" / "wizard-prism-512.png"
DEFAULT_EXPRESSION = "{e^(sin(x))|x=pi/2}"
DEFAULT_EQUATION = "{ x^2 + y^2 = 5 | x = 1, y = 1 }"
DEFAULT_EQUATION_VARIABLE = "x"
DEFAULT_MATRIX = "(1, 2; 3, 4)"
DEFAULT_MATRIX_OPERATION = "inverse"
DEFAULT_INTEGRATOR_EXPRESSION = "{ exp(-x^2) | x = ? }"
DEFAULT_INTEGRATOR_BOUNDS = "x = 0 .. 1"
DEFAULT_INTEGRATOR_INTERVAL_CAP = 5000
DEFAULT_DATETIME_DATE = py_datetime.date.today().isoformat()
DEFAULT_DATETIME_TEXT = "Calendar and solar calculations, with optional holiday lookup"
DEFAULT_DATETIME_LATITUDE = DEFAULT_TIMEZONE_LATITUDE
DEFAULT_DATETIME_LONGITUDE = DEFAULT_TIMEZONE_LONGITUDE
DEFAULT_DATETIME_ELEVATION = "0"
DEFAULT_DATETIME_GMT_OFFSET = ""
DEFAULT_ALMANAC_TEXT = "Navigation almanac worksheet"
DEFAULT_ALMANAC_DATE = DEFAULT_DATETIME_DATE
DEFAULT_ALMANAC_TIME = py_datetime.datetime.now(py_datetime.timezone.utc).strftime("%H:%M:%S")
DEFAULT_ALMANAC_ZONE = "0"
DEFAULT_ALMANAC_LATITUDE = DEFAULT_TIMEZONE_LATITUDE
DEFAULT_ALMANAC_LONGITUDE = DEFAULT_TIMEZONE_LONGITUDE
DEFAULT_ALMANAC_BODY = "MOON"
ALMANAC_BODY_OPTIONS_HTML = "\n".join(
    (
        f'          <option value="{html.escape(code)}" selected>{html.escape(name)}</option>'
        if code == DEFAULT_ALMANAC_BODY
        else f'          <option value="{html.escape(code)}">{html.escape(name)}</option>'
    )
    for code, name in ALMANAC_BODY_OPTIONS
)
MIN_INTEGRATOR_INTERVAL_CAP = 500
MAX_INTEGRATOR_INTERVAL_CAP = 100000
INTEGRATOR_INTERVAL_CAP_CHOICES = (500, 5000, 20000, 50000, 100000)
MAX_VALUE_PRECISION_BITS = 1_048_576
MAX_VALUE_PRECISION_DIGITS = math.ceil(MAX_VALUE_PRECISION_BITS * math.log10(2))
INTEGRATOR_ERROR_DISPLAY_DIGITS = 4
COMPACT_BINDING_VALUE_LIMIT = 20
COMPACT_BINDING_VALUE_KEEP = 16
QR_VERSION = 5
QR_SIZE = 17 + 4 * QR_VERSION
QR_DATA_CODEWORDS = 108
QR_EC_CODEWORDS = 26
QR_EC_LEVEL_L = 1
QR_MASK_PATTERN = 0
CONTROL_TOKEN = os.environ.get("MARS_LAB_CONTROL_TOKEN") or secrets.token_urlsafe(24)
CONTROL_QUERY_PARAM = os.environ.get("MARS_LAB_CONTROL_QUERY_PARAM", "mars_lab_control")
CONTROL_COOKIE = os.environ.get("MARS_LAB_CONTROL_COOKIE", "mars_lab_control")

LAB_THEME_COLOR = "#071913"
LAB_MANIFEST_BACKGROUND = "#f6f0e5"
LAB_MANIFEST_THEME = "#0b4f8a"
LAB_BODY_CLASS = ""
LAB_THEME_OVERRIDES = ""

if LAB_THEME == "to-be-announced":
    LAB_THEME_COLOR = "#f7a8d9"
    LAB_MANIFEST_BACKGROUND = "#fff5fb"
    LAB_MANIFEST_THEME = "#f7a8d9"
    LAB_BODY_CLASS = "theme-to-be-announced"
    LAB_THEME_OVERRIDES = r"""
    body.theme-to-be-announced {
      color: #31143d;
      font-family: "Georgia", "Iowan Old Style", "Palatino Linotype", serif;
      background:
        radial-gradient(circle at 18% 18%, rgba(255, 255, 255, 0.92), transparent 12rem),
        radial-gradient(circle at 84% 14%, rgba(255, 214, 244, 0.78), transparent 15rem),
        radial-gradient(circle at 76% 78%, rgba(190, 240, 255, 0.54), transparent 20rem),
        linear-gradient(160deg, #fff7fd 0%, #ffe6f5 26%, #f6e6ff 52%, #dbf6ff 74%, #fff4cf 100%);
    }

    body.theme-to-be-announced::before {
      opacity: 1;
      background:
        radial-gradient(circle at 14% 16%, rgba(255, 255, 255, 0.96) 0 3.5rem, transparent 6rem),
        radial-gradient(circle at 84% 18%, rgba(255, 255, 255, 0.88) 0 3rem, transparent 5.2rem),
        radial-gradient(circle at 26% 68%, rgba(255, 255, 255, 0.86) 0 2.4rem, transparent 4.2rem),
        radial-gradient(circle at 72% 62%, rgba(255, 255, 255, 0.82) 0 2.8rem, transparent 4.6rem),
        radial-gradient(circle at 50% 10%, rgba(255, 255, 255, 0.7) 0 2.2rem, transparent 4rem),
        radial-gradient(circle at 10% 82%, rgba(255, 222, 245, 0.52) 0 9rem, transparent 14rem),
        radial-gradient(circle at 88% 72%, rgba(195, 241, 255, 0.48) 0 10rem, transparent 16rem),
        repeating-radial-gradient(circle at 50% 50%, rgba(255, 255, 255, 0.28) 0 2px, transparent 2px 16px);
      mask: none;
    }

    body.theme-to-be-announced::after {
      height: 14rem;
      opacity: 0.94;
      background:
        radial-gradient(circle at 20% 82%, rgba(255, 186, 227, 0.56) 0 6rem, transparent 8rem),
        radial-gradient(circle at 80% 74%, rgba(187, 237, 255, 0.58) 0 6.4rem, transparent 8.8rem),
        linear-gradient(0deg, rgba(255, 224, 244, 0.82), rgba(255, 255, 255, 0.12) 54%, transparent 90%);
    }

    body.theme-to-be-announced .celtic-backdrop {
      overflow: visible;
    }

    body.theme-to-be-announced .aurora {
      top: 0.4rem;
      height: 16rem;
      opacity: 0.94;
      background:
        radial-gradient(circle at 22% 52%, rgba(255, 255, 255, 0.84) 0 1.5rem, transparent 1.7rem),
        radial-gradient(circle at 34% 28%, rgba(255, 255, 255, 0.7) 0 1rem, transparent 1.2rem),
        radial-gradient(circle at 66% 38%, rgba(255, 255, 255, 0.76) 0 1.15rem, transparent 1.35rem),
        linear-gradient(104deg, transparent 0 8%, rgba(255, 175, 223, 0.78) 12%, rgba(255, 226, 248, 0.36) 24%, transparent 40%),
        linear-gradient(116deg, transparent 0 22%, rgba(203, 184, 255, 0.64) 28%, rgba(188, 244, 255, 0.32) 42%, transparent 58%),
        linear-gradient(128deg, transparent 0 36%, rgba(255, 236, 174, 0.58) 42%, rgba(255, 212, 234, 0.24) 54%, transparent 70%);
      filter: blur(0.2px);
      transform: skewY(-4deg);
    }

    body.theme-to-be-announced .standing-stones {
      bottom: 1.4rem;
      height: 13rem;
      opacity: 0.92;
    }

    body.theme-to-be-announced .stone {
      width: clamp(3.6rem, 5vw, 4.8rem);
      height: clamp(7.2rem, 14vw, 11rem);
      border-radius: 58% 42% 48% 52% / 14% 14% 8% 8%;
      background:
        linear-gradient(160deg, rgba(255, 255, 255, 0.9), rgba(255, 211, 240, 0.88) 34%, rgba(204, 242, 255, 0.88) 70%, rgba(255, 246, 196, 0.86));
      border: 1px solid rgba(255, 255, 255, 0.84);
      box-shadow:
        0 0.8rem 1.6rem rgba(182, 120, 188, 0.18),
        inset 0.55rem 0.35rem 1rem rgba(255, 255, 255, 0.72);
    }

    body.theme-to-be-announced .stone:nth-child(2),
    body.theme-to-be-announced .stone:nth-child(5) {
      height: 12.2rem;
    }

    body.theme-to-be-announced .stone:nth-child(1)::before,
    body.theme-to-be-announced .stone:nth-child(2)::before,
    body.theme-to-be-announced .stone:nth-child(3)::before,
    body.theme-to-be-announced .stone:nth-child(4)::before,
    body.theme-to-be-announced .stone:nth-child(5)::before,
    body.theme-to-be-announced .stone:nth-child(6)::before {
      content: "";
      position: absolute;
      inset: 18% 22%;
      border-radius: 999px 999px 28% 28%;
      background:
        linear-gradient(180deg, rgba(255,255,255,0.94), rgba(255,184,221,0.86) 52%, rgba(173, 234, 255, 0.78));
      box-shadow:
        0 0 0 1px rgba(255,255,255,0.7),
        0 0 1.1rem rgba(255, 170, 221, 0.34);
      clip-path: polygon(46% 0%, 62% 17%, 88% 18%, 69% 36%, 76% 62%, 50% 48%, 24% 62%, 31% 36%, 12% 18%, 38% 17%);
      opacity: 0.96;
    }

    body.theme-to-be-announced .chariot-wheel {
      right: 5vw;
      bottom: 2rem;
      width: 8rem;
      height: 8rem;
      opacity: 0.9;
      border: none;
      background:
        radial-gradient(circle at 50% 50%, rgba(255,255,255,0.95) 0 0.72rem, transparent 0.9rem),
        radial-gradient(circle at 50% 50%, transparent 0 2rem, rgba(255, 188, 227, 0.8) 2.06rem 2.26rem, transparent 2.34rem),
        radial-gradient(circle at 50% 50%, transparent 0 3.42rem, rgba(181, 234, 255, 0.86) 3.5rem 3.72rem, transparent 3.84rem),
        conic-gradient(from 0deg,
          rgba(255, 188, 227, 0.86) 0deg 18deg,
          transparent 18deg 42deg,
          rgba(188, 244, 255, 0.84) 42deg 60deg,
          transparent 60deg 84deg,
          rgba(255, 235, 170, 0.88) 84deg 102deg,
          transparent 102deg 126deg,
          rgba(212, 190, 255, 0.84) 126deg 144deg,
          transparent 144deg 168deg,
          rgba(255, 188, 227, 0.86) 168deg 186deg,
          transparent 186deg 210deg,
          rgba(188, 244, 255, 0.84) 210deg 228deg,
          transparent 228deg 252deg,
          rgba(255, 235, 170, 0.88) 252deg 270deg,
          transparent 270deg 294deg,
          rgba(212, 190, 255, 0.84) 294deg 312deg,
          transparent 312deg 336deg,
          rgba(255, 188, 227, 0.86) 336deg 360deg);
      box-shadow:
        0 0 1.4rem rgba(240, 148, 210, 0.32),
        inset 0 0 1rem rgba(255,255,255,0.6);
    }

    body.theme-to-be-announced h1 {
      color: #8a2b74;
      text-shadow:
        0 0 1.2rem rgba(255, 198, 231, 0.92),
        0 0 2rem rgba(199, 236, 255, 0.52);
    }

    body.theme-to-be-announced .subtitle,
    body.theme-to-be-announced .status,
    body.theme-to-be-announced .precision-label,
    body.theme-to-be-announced label,
    body.theme-to-be-announced .help-card,
    body.theme-to-be-announced .mobile-panel {
      color: #5d2b67;
    }

    body.theme-to-be-announced .status {
      background: rgba(255,255,255,0.64);
      border-color: rgba(247, 168, 217, 0.56);
      box-shadow: 0 0.5rem 1.2rem rgba(191, 132, 188, 0.16);
    }

    body.theme-to-be-announced .lab-topbar,
    body.theme-to-be-announced #workspacePanel,
    body.theme-to-be-announced #resultPanel,
    body.theme-to-be-announced .help-card,
    body.theme-to-be-announced .mobile-panel,
    body.theme-to-be-announced .mode-panel,
    body.theme-to-be-announced .value-card,
    body.theme-to-be-announced .rendered,
    body.theme-to-be-announced .raw-block,
    body.theme-to-be-announced textarea,
    body.theme-to-be-announced select {
      background: linear-gradient(180deg, rgba(255,255,255,0.82), rgba(255,247,253,0.66));
      border-color: rgba(230, 167, 216, 0.44);
      box-shadow: 0 0.9rem 2rem rgba(179, 129, 179, 0.12);
      color: #421c4f;
    }

    body.theme-to-be-announced textarea,
    body.theme-to-be-announced select,
    body.theme-to-be-announced .raw-block,
    body.theme-to-be-announced code {
      color: #51245e;
    }

    body.theme-to-be-announced .mode-panel select {
      color-scheme: light;
    }

    body.theme-to-be-announced .mode-panel select option {
      color: #51245e;
      background: #fff7fd;
    }

    body.theme-to-be-announced .mode-panel select option:checked {
      color: #421c4f;
      background: #ffd6f4;
    }

    body.theme-to-be-announced .select-shell .select-button,
    body.theme-to-be-announced .select-shell .select-menu {
      color: #51245e;
      background:
        linear-gradient(180deg, rgba(255,255,255,0.92), rgba(255,247,253,0.78));
      border-color: rgba(230, 167, 216, 0.48);
      box-shadow: 0 0.9rem 2rem rgba(179, 129, 179, 0.12);
    }

    body.theme-to-be-announced .select-shell .select-option {
      color: #51245e;
    }

    body.theme-to-be-announced .select-shell .select-option:hover,
    body.theme-to-be-announced .select-shell .select-option:focus-visible {
      background: rgba(247, 168, 217, 0.22);
    }

    body.theme-to-be-announced .select-shell .select-option.selected {
      color: #421c4f;
      background: #ffd6f4;
    }

    body.theme-to-be-announced .mode-tab,
    body.theme-to-be-announced .card-action,
    body.theme-to-be-announced button {
      background:
        linear-gradient(180deg, rgba(255,255,255,0.92), rgba(255,226,246,0.92));
      border-color: rgba(234, 154, 214, 0.62);
      color: #7d2d7b;
      box-shadow: 0 0.4rem 1rem rgba(198, 144, 194, 0.16);
    }

    body.theme-to-be-announced .mode-tab.active,
    body.theme-to-be-announced .card-action:hover,
    body.theme-to-be-announced button:hover {
      background:
        linear-gradient(180deg, rgba(255, 238, 248, 0.98), rgba(213, 244, 255, 0.96));
      color: #5b2081;
      transform: translateY(-1px);
    }

    body.theme-to-be-announced .mode-tab.active {
      box-shadow:
        0 0 0 1px rgba(255,255,255,0.84),
        0 0.7rem 1.5rem rgba(171, 214, 255, 0.26);
    }

    body.theme-to-be-announced .rendered {
      background:
        radial-gradient(circle at top right, rgba(255,255,255,0.8), transparent 8rem),
        linear-gradient(180deg, rgba(255,255,255,0.92), rgba(255,245,252,0.82));
    }
    """


INDEX_HTML = r"""<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>__LAB_NAME__</title>
  <link rel="icon" type="image/svg+xml" href="/favicon.svg">
  <link rel="apple-touch-icon" sizes="180x180" href="/apple-touch-icon.png">
  <link rel="icon" type="image/png" sizes="192x192" href="/icon-192.png">
  <link rel="icon" type="image/png" sizes="512x512" href="/icon-512.png">
  <link rel="manifest" href="/manifest.webmanifest">
  <meta name="theme-color" content="__THEME_COLOR__">
  <style>
    :root {
      color-scheme: light;
      --ink: #f3f8f2;
      --muted: #bed3c0;
      --paper: #071913;
      --panel: #091f17;
      --line: rgba(233, 244, 239, 0.34);
      --accent: #cfa052;
      --accent-2: #71c6b4;
      --code: #f3f8f2;
      --stone: #173f32;
      --mist: #e9f4ef;
      --torc: #cfa052;
      --oak: #263920;
      --shadow: rgba(0, 0, 0, 0.28);
    }

    * { box-sizing: border-box; }

    body {
      position: relative;
      margin: 0;
      min-height: 100vh;
      color: var(--ink);
      font: 16px/1.5 "Iowan Old Style", "Palatino Linotype", "Book Antiqua", serif;
      background:
        radial-gradient(circle at 15% 18%, rgba(107, 176, 167, 0.24), transparent 18rem),
        radial-gradient(circle at 83% 13%, rgba(196, 131, 48, 0.24), transparent 19rem),
        linear-gradient(145deg, #061612, #123326 46%, #263920);
    }

    body::before {
      content: "";
      position: fixed;
      inset: 0;
      pointer-events: none;
      opacity: 0.62;
      background:
        radial-gradient(ellipse at 34% 10%, rgba(180, 255, 219, 0.46) 0 6rem, transparent 18rem),
        radial-gradient(ellipse at 66% 12%, rgba(142, 119, 255, 0.34) 0 5rem, transparent 17rem),
        linear-gradient(108deg, transparent 0 3%, rgba(137, 255, 211, 0.78) 8%, rgba(113, 198, 180, 0.16) 18%, transparent 32%),
        linear-gradient(116deg, transparent 0 12%, rgba(199, 151, 255, 0.58) 18%, rgba(137, 255, 211, 0.2) 30%, transparent 46%),
        linear-gradient(126deg, transparent 0 24%, rgba(255, 218, 125, 0.42) 31%, rgba(137, 255, 211, 0.16) 44%, transparent 58%),
        radial-gradient(ellipse at 50% 104%, rgba(0, 0, 0, 0.52) 0 4.8rem, transparent 5rem),
        linear-gradient(180deg, transparent 0 68%, rgba(4, 18, 12, 0.44) 68%),
        radial-gradient(circle at 74% 22%, transparent 0 4.2rem, rgba(196, 131, 48, 0.44) 4.3rem 4.46rem, transparent 4.58rem),
        radial-gradient(circle at 30% 54%, transparent 0 6.4rem, rgba(107, 176, 167, 0.36) 6.5rem 6.66rem, transparent 6.78rem),
        linear-gradient(102deg, transparent 0 8%, rgba(113, 198, 180, 0.64) 8.5% 11.6%, transparent 12.5%),
        linear-gradient(112deg, transparent 0 16%, rgba(185, 136, 235, 0.48) 16.4% 19.2%, transparent 20%),
        linear-gradient(124deg, transparent 0 25%, rgba(207, 160, 82, 0.34) 25.3% 27.5%, transparent 28.2%),
        linear-gradient(98deg, transparent 0 34%, rgba(89, 184, 179, 0.42) 34.2% 36.6%, transparent 37.3%),
        repeating-linear-gradient(64deg, transparent 0 25px, rgba(233, 244, 239, 0.13) 26px 27px),
        radial-gradient(circle at 50% 50%, transparent 0 7rem, rgba(233, 244, 239, 0.22) 7.1rem 7.22rem, transparent 7.35rem);
      mask:
        linear-gradient(#000 0 0) top / 100% 24px no-repeat,
        linear-gradient(#000 0 0) bottom / 100% 24px no-repeat,
        linear-gradient(#000 0 0) left / 24px 100% no-repeat,
        linear-gradient(#000 0 0) right / 24px 100% no-repeat,
        linear-gradient(#000 0 0);
    }

    body::after {
      content: "";
      position: fixed;
      inset: auto 0 0;
      height: 12rem;
      pointer-events: none;
      opacity: 0.72;
      background:
        linear-gradient(82deg, transparent 0 8%, rgba(3, 13, 9, 0.95) 8.2% 10.9%, transparent 11.2%),
        linear-gradient(96deg, transparent 0 17%, rgba(3, 13, 9, 0.92) 17.2% 20.3%, transparent 20.6%),
        linear-gradient(76deg, transparent 0 27%, rgba(3, 13, 9, 0.9) 27.2% 30.1%, transparent 30.4%),
        linear-gradient(101deg, transparent 0 38%, rgba(3, 13, 9, 0.92) 38.2% 41.6%, transparent 41.9%),
        linear-gradient(88deg, transparent 0 53%, rgba(3, 13, 9, 0.94) 53.2% 56.3%, transparent 56.6%),
        linear-gradient(94deg, transparent 0 67%, rgba(3, 13, 9, 0.9) 67.2% 70.1%, transparent 70.4%),
        radial-gradient(circle at 77% 77%, transparent 0 1.75rem, rgba(3, 13, 9, 0.96) 1.84rem 2.04rem, transparent 2.16rem),
        radial-gradient(circle at 83% 77%, transparent 0 1.75rem, rgba(3, 13, 9, 0.96) 1.84rem 2.04rem, transparent 2.16rem),
        linear-gradient(8deg, transparent 0 75%, rgba(3, 13, 9, 0.88) 75.2% 77%, transparent 77.3%),
        linear-gradient(0deg, rgba(3, 13, 9, 0.85), rgba(3, 13, 9, 0.38) 18%, transparent 65%);
    }

    .celtic-backdrop {
      position: fixed;
      inset: 0;
      z-index: 0;
      pointer-events: none;
      overflow: hidden;
    }

    .aurora {
      position: absolute;
      left: -8vw;
      right: -8vw;
      top: 1.2rem;
      height: 12rem;
      opacity: 0.9;
      background:
        linear-gradient(105deg, transparent 0 7%, rgba(144, 255, 216, 0.82) 12%, rgba(144, 255, 216, 0.16) 24%, transparent 38%),
        linear-gradient(118deg, transparent 0 17%, rgba(190, 143, 255, 0.58) 23%, rgba(144, 255, 216, 0.22) 36%, transparent 54%),
        linear-gradient(130deg, transparent 0 34%, rgba(244, 207, 102, 0.42) 40%, rgba(144, 255, 216, 0.16) 53%, transparent 70%);
      filter: blur(0.4px);
      transform: skewY(-6deg);
    }

    .standing-stones {
      position: absolute;
      left: 0;
      right: 0;
      bottom: 2.4rem;
      height: 12rem;
      opacity: 0.64;
    }

    .stone {
      position: absolute;
      bottom: 0;
      width: clamp(2.6rem, 4vw, 4rem);
      height: clamp(7rem, 13vw, 11rem);
      border-radius: 54% 46% 40% 42% / 12% 14% 6% 8%;
      background: linear-gradient(145deg, rgba(27, 40, 35, 0.94), rgba(89, 107, 95, 0.68));
      border: 1px solid rgba(233, 244, 239, 0.14);
      box-shadow: inset 0.6rem 0.4rem 1.1rem rgba(233, 244, 239, 0.08);
    }

    .stone:nth-child(1) { left: 6%; height: 8rem; transform: rotate(-6deg); }
    .stone:nth-child(2) { left: 13%; height: 11rem; transform: rotate(4deg); }
    .stone:nth-child(3) { left: 22%; height: 7.8rem; transform: rotate(-3deg); }
    .stone:nth-child(4) { right: 24%; height: 8.5rem; transform: rotate(5deg); }
    .stone:nth-child(5) { right: 14%; height: 11.5rem; transform: rotate(-5deg); }
    .stone:nth-child(6) { right: 6%; height: 7.6rem; transform: rotate(7deg); }

    .chariot-wheel {
      position: absolute;
      right: 7vw;
      bottom: 3.3rem;
      width: 6.2rem;
      height: 6.2rem;
      opacity: 0.48;
      border: 0.32rem solid rgba(4, 13, 9, 0.9);
      border-radius: 999px;
      background:
        linear-gradient(0deg, transparent 46%, rgba(4, 13, 9, 0.9) 47% 53%, transparent 54%),
        linear-gradient(60deg, transparent 46%, rgba(4, 13, 9, 0.9) 47% 53%, transparent 54%),
        linear-gradient(120deg, transparent 46%, rgba(4, 13, 9, 0.9) 47% 53%, transparent 54%);
      box-shadow: 0 0 0 0.18rem rgba(196, 131, 48, 0.18);
    }

    header,
    main {
      position: relative;
    }

    header {
      z-index: 20;
      padding: 1.5rem clamp(1rem, 3vw, 2rem) 0.75rem;
      display: flex;
      align-items: end;
      justify-content: space-between;
      gap: 1rem;
    }

    h1 {
      margin: 0;
      font-size: clamp(1.8rem, 4vw, 3.2rem);
      line-height: 1;
      letter-spacing: -0.045em;
      color: #f3f8f2;
      text-shadow: 0 0 1rem rgba(113, 198, 180, 0.32);
    }

    .subtitle {
      margin: 0.45rem 0 0;
      color: var(--muted);
      max-width: 52rem;
    }

    .status {
      min-width: 9rem;
      text-align: right;
      color: var(--muted);
      font-size: 0.95rem;
    }

    main {
      z-index: 1;
      display: grid;
      grid-template-columns: minmax(18rem, 0.92fr) minmax(22rem, 1.08fr);
      gap: 1rem;
      padding: 0.75rem clamp(1rem, 3vw, 2rem) 2rem;
    }

    .lab-topbar {
      grid-column: 1 / -1;
      display: flex;
      flex-wrap: wrap;
      align-items: center;
      justify-content: space-between;
      gap: 0.8rem;
      padding: 0.35rem 0 0.2rem;
    }

    .lab-tabs {
      display: flex;
      flex-wrap: wrap;
      gap: 0.7rem;
      align-items: center;
    }

    .precision-toolbar {
      display: flex;
      flex-wrap: wrap;
      align-items: center;
      justify-content: flex-end;
      gap: 0.45rem;
      padding: 0.35rem 0 0.2rem;
    }

    .precision-label {
      color: var(--muted);
      font: 0.72rem/1.1 "Cascadia Code", "Fira Code", "DejaVu Sans Mono", monospace;
      letter-spacing: 0.1em;
      text-transform: uppercase;
    }

    .mode-tab {
      border: 1px solid rgba(233, 244, 239, 0.24);
      border-radius: 999px;
      padding: 0.78rem 1.15rem;
      color: #d9ead6;
      background:
        linear-gradient(180deg, rgba(12, 41, 31, 0.82), rgba(7, 23, 18, 0.82));
      font: 0.86rem/1.1 "Cascadia Code", "Fira Code", "DejaVu Sans Mono", monospace;
      letter-spacing: 0.1em;
      text-transform: uppercase;
      cursor: pointer;
      transition:
        transform 160ms ease,
        border-color 160ms ease,
        background 160ms ease,
        box-shadow 160ms ease,
        color 160ms ease;
    }

    .mode-tab:hover {
      transform: translateY(-1px);
      border-color: rgba(233, 244, 239, 0.38);
      color: #f7fff1;
    }

    .mode-tab:focus-visible {
      outline: 0;
      border-color: color-mix(in srgb, var(--accent), var(--line) 25%);
      box-shadow: 0 0 0 3px rgba(113, 198, 180, 0.18);
    }

    .mode-tab.active {
      border-color: rgba(227, 180, 87, 0.76);
      color: #10190f;
      background:
        linear-gradient(135deg, rgba(233, 187, 90, 0.96), rgba(140, 216, 184, 0.94));
      box-shadow: 0 12px 30px rgba(0, 0, 0, 0.22);
    }

    section {
      background: rgba(8, 29, 22, 0.78);
      border: 2px solid rgba(233, 244, 239, 0.34);
      border-radius: 22px;
      box-shadow: 0 18px 55px var(--shadow);
      overflow: hidden;
    }

    .panel-head {
      display: flex;
      align-items: center;
      justify-content: space-between;
      gap: 1rem;
      padding: 0.9rem 1rem;
      border-bottom: 2px solid rgba(233, 244, 239, 0.28);
      background:
        linear-gradient(90deg, rgba(196, 131, 48, 0.08), rgba(113, 198, 180, 0.08));
    }

    h2 {
      margin: 0;
      font-size: 0.95rem;
      text-transform: uppercase;
      letter-spacing: 0.13em;
      color: #d7e7b7;
    }

    textarea {
      width: 100%;
      min-height: 11rem;
      resize: vertical;
      border: 0;
      outline: 0;
      padding: 1rem;
      color: var(--code);
      background: transparent;
      font: 1.05rem/1.5 "Cascadia Code", "Fira Code", "DejaVu Sans Mono", monospace;
    }

    body.datetime-mode #expr,
    body.almanac-mode #expr {
      display: none;
    }

    body.datetime-mode #resultPane .zoom-action,
    body.almanac-mode #resultPane .zoom-action {
      display: none;
    }

    .secondary-editor {
      min-height: 6.5rem;
      border-top: 1px solid rgba(233, 244, 239, 0.12);
      border-bottom: 1px solid rgba(233, 244, 239, 0.12);
      background: rgba(0, 0, 0, 0.08);
    }

    .mode-panel {
      display: grid;
      gap: 0.55rem;
      padding: 0 1rem 1rem;
    }

    .mode-panel label {
      color: #bed3c0;
      font: 0.78rem/1.2 "Cascadia Code", "DejaVu Sans Mono", monospace;
      letter-spacing: 0.1em;
      text-transform: uppercase;
    }

    .integrator-bound-stack {
      display: grid;
      gap: 0.45rem;
    }

    .integrator-bound-row {
      display: grid;
      grid-template-columns: minmax(5rem, 0.58fr) minmax(5.5rem, 0.74fr) repeat(2, minmax(0, 1fr)) auto auto;
      gap: 0.7rem;
      align-items: end;
    }

    .datetime-grid {
      display: grid;
      grid-template-columns: repeat(3, minmax(9rem, 1fr));
      gap: 0.8rem;
      align-items: end;
    }

    .datetime-field-groups {
      display: grid;
      gap: 0.85rem;
    }

    .datetime-field-group {
      display: grid;
      gap: 0.7rem;
      border: 1px solid rgba(233, 244, 239, 0.16);
      border-radius: 20px;
      padding: 0.85rem;
      background:
        linear-gradient(135deg, rgba(7, 28, 24, 0.42), rgba(24, 59, 44, 0.24));
      box-shadow: inset 0 1px 0 rgba(233, 244, 239, 0.08);
    }

    .datetime-field-group-title {
      color: #ebc171;
      font: 0.72rem/1.15 "Cascadia Code", "DejaVu Sans Mono", monospace;
      font-weight: 700;
      letter-spacing: 0.14em;
      text-transform: uppercase;
    }

    .datetime-grid.two-up {
      grid-template-columns: repeat(2, minmax(9rem, 1fr));
    }

    .datetime-briefing {
      display: grid;
      grid-template-columns: auto minmax(0, 1fr);
      gap: 0.85rem;
      align-items: center;
      border: 1px solid rgba(228, 168, 88, 0.34);
      border-radius: 20px;
      padding: 0.85rem;
      background:
        radial-gradient(circle at 1.4rem 1.4rem, rgba(234, 174, 87, 0.3), transparent 2.5rem),
        linear-gradient(135deg, rgba(86, 36, 24, 0.5), rgba(15, 56, 48, 0.44));
      box-shadow:
        inset 0 1px 0 rgba(255, 231, 184, 0.14),
        0 16px 34px rgba(0, 0, 0, 0.16);
    }

    .datetime-orbit {
      position: relative;
      width: 3.25rem;
      height: 3.25rem;
      border-radius: 999px;
      background:
        radial-gradient(circle at 35% 30%, #f2c06f 0 0.38rem, transparent 0.42rem),
        radial-gradient(circle at 62% 66%, rgba(64, 21, 16, 0.48) 0 0.36rem, transparent 0.4rem),
        radial-gradient(circle at 44% 46%, #c46139, #772f24 68%, #351914 100%);
      box-shadow:
        0 0 0 1px rgba(255, 218, 149, 0.24),
        0 0 28px rgba(217, 111, 57, 0.3);
    }

    .datetime-orbit::after {
      content: "";
      position: absolute;
      inset: 0.28rem -0.7rem;
      border: 1px solid rgba(240, 195, 103, 0.46);
      border-left-color: transparent;
      border-right-color: transparent;
      border-radius: 50%;
      transform: rotate(-18deg);
    }

    .datetime-briefing-kicker {
      color: #efc36a;
      font: 0.72rem/1.2 "Cascadia Code", "DejaVu Sans Mono", monospace;
      letter-spacing: 0.14em;
      text-transform: uppercase;
    }

    .datetime-briefing-title {
      margin-top: 0.12rem;
      color: #f3ead1;
      font: 1.1rem/1.2 Georgia, "Times New Roman", serif;
    }

    .datetime-briefing-copy {
      margin-top: 0.3rem;
      color: rgba(233, 244, 239, 0.78);
      font-size: 0.85rem;
      line-height: 1.45;
    }

    .almanac-sheet {
      display: grid;
      gap: 0.9rem;
      color: #f3ead1;
      font: 0.84rem/1.4 "Cascadia Code", "DejaVu Sans Mono", monospace;
    }

    .almanac-sheet-header {
      display: grid;
      gap: 0.18rem;
      padding: 0.95rem 1rem;
      border: 1px solid rgba(236, 195, 106, 0.26);
      border-radius: 18px;
      background:
        linear-gradient(180deg, rgba(76, 50, 21, 0.44), rgba(18, 36, 27, 0.34));
      box-shadow: inset 0 1px 0 rgba(255, 239, 190, 0.09);
    }

    .almanac-sheet-title {
      color: #f0c873;
      font: 700 0.8rem/1.2 "Cascadia Code", "DejaVu Sans Mono", monospace;
      letter-spacing: 0.12em;
      text-transform: uppercase;
    }

    .almanac-sheet-note {
      color: rgba(233, 244, 239, 0.76);
      font-size: 0.8rem;
    }

    .almanac-grid-table {
      width: 100%;
      border-collapse: collapse;
      overflow: hidden;
      border: 1px solid rgba(233, 244, 239, 0.14);
      border-radius: 18px;
      background: rgba(8, 24, 19, 0.46);
    }

    .almanac-grid-table th,
    .almanac-grid-table td {
      padding: 0.62rem 0.7rem;
      border-bottom: 1px solid rgba(233, 244, 239, 0.08);
      text-align: left;
      vertical-align: top;
    }

    .almanac-grid-table thead th {
      color: #efc36a;
      font-size: 0.72rem;
      letter-spacing: 0.12em;
      text-transform: uppercase;
      background: rgba(36, 61, 40, 0.52);
    }

    .almanac-grid-table tbody tr:last-child td {
      border-bottom: 0;
    }

    .almanac-grid-table tbody tr.selected {
      background: rgba(231, 177, 80, 0.11);
    }

    .almanac-grid-table tbody tr.reference {
      background: rgba(120, 179, 160, 0.08);
    }

    .almanac-grid-table td.body-name {
      color: #f4ecda;
      font-weight: 700;
    }

    .almanac-grid-table td.reference-name {
      color: #cde3d5;
    }

    .almanac-grid-table td.number {
      white-space: nowrap;
    }

    .integrator-bound-field {
      display: grid;
      gap: 0.4rem;
    }

    .integrator-bound-field.disabled label,
    .integrator-bound-field.disabled input {
      opacity: 0.48;
    }

    .integrator-bound-toggle,
    .integrator-bound-add,
    .integrator-bound-remove {
      align-self: end;
      white-space: nowrap;
    }

    .integrator-bound-actions {
      display: flex;
      gap: 0.5rem;
      justify-content: flex-start;
      margin-top: 0.35rem;
    }

    .integrator-bound-summary {
      color: #bed3c0;
      font: 0.72rem/1.3 "Cascadia Code", "DejaVu Sans Mono", monospace;
      letter-spacing: 0.04em;
    }

    .mode-panel input {
      width: 100%;
      border: 1px solid rgba(233, 244, 239, 0.28);
      border-radius: 999px;
      outline: 0;
      padding: 0.65rem 0.9rem;
      color: var(--code);
      background: rgba(0, 0, 0, 0.14);
      font: 0.95rem/1.25 "Cascadia Code", "Fira Code", "DejaVu Sans Mono", monospace;
    }

    .integrator-bound-field input {
      color: var(--code);
      background: rgba(0, 0, 0, 0.14);
      box-shadow: 0 0 0 1000px rgba(0, 0, 0, 0.14) inset;
      caret-color: var(--code);
      color-scheme: dark;
    }

    .datetime-controls input[type="date"]::-webkit-calendar-picker-indicator {
      filter: invert(1) sepia(0.4) saturate(0.8) hue-rotate(76deg);
      opacity: 0.78;
    }

    .mars-date-shell {
      position: relative;
      display: grid;
      grid-template-columns: minmax(0, 1fr) auto;
      gap: 0.45rem;
      align-items: center;
    }

    .mars-date-shell input {
      min-width: 0;
    }

    .mars-date-button {
      display: inline-flex;
      align-items: center;
      justify-content: center;
      width: 2.7rem;
      min-width: 2.7rem;
      min-height: 2.7rem;
      border: 1px solid rgba(233, 244, 239, 0.24);
      border-radius: 999px;
      background:
        linear-gradient(180deg, rgba(26, 64, 47, 0.96), rgba(10, 31, 23, 0.98));
      color: #efc36a;
      box-shadow:
        inset 0 1px 0 rgba(255, 240, 199, 0.08),
        0 9px 24px rgba(0, 0, 0, 0.22);
      cursor: pointer;
      transition: transform 120ms ease, border-color 140ms ease, box-shadow 140ms ease;
    }

    .mars-date-button:hover {
      transform: translateY(-1px);
      border-color: rgba(233, 244, 239, 0.36);
    }

    .mars-date-button:focus-visible,
    .mars-date-shell.open .mars-date-button {
      outline: none;
      border-color: color-mix(in srgb, var(--accent), var(--line) 18%);
      box-shadow:
        inset 0 1px 0 rgba(255, 240, 199, 0.1),
        0 0 0 3px rgba(113, 198, 180, 0.18),
        0 12px 28px rgba(0, 0, 0, 0.24);
    }

    .mars-date-button::before {
      content: "CAL";
      display: inline-block;
      font: 0.62rem/1 "Cascadia Code", "DejaVu Sans Mono", monospace;
      letter-spacing: 0.11em;
      font-weight: 700;
      text-align: center;
    }

    .mars-date-picker {
      position: fixed;
      z-index: 120;
      width: min(100%, 24rem);
      border: 1px solid rgba(233, 244, 239, 0.22);
      border-radius: 24px;
      padding: 0.8rem;
      background:
        radial-gradient(circle at top left, rgba(205, 135, 60, 0.14), transparent 34%),
        linear-gradient(145deg, rgba(8, 30, 24, 0.98), rgba(13, 47, 34, 0.98));
      box-shadow:
        inset 0 1px 0 rgba(255, 243, 214, 0.08),
        0 24px 70px rgba(0, 0, 0, 0.4);
      backdrop-filter: blur(10px);
    }

    .mars-date-picker.hidden {
      display: none;
    }

    .mars-date-picker-head {
      display: grid;
      grid-template-columns: auto minmax(0, 1fr) auto;
      gap: 0.55rem;
      align-items: center;
      margin-bottom: 0.7rem;
    }

    .mars-date-picker-title {
      text-align: center;
      color: #f3ead1;
      font: 0.98rem/1.2 Georgia, "Times New Roman", serif;
      letter-spacing: 0.03em;
    }

    .mars-date-picker-nav {
      min-width: 2.3rem;
      min-height: 2.3rem;
      border: 1px solid rgba(233, 244, 239, 0.22);
      border-radius: 999px;
      background: rgba(0, 0, 0, 0.14);
      color: #d7e7b7;
      font: 0.98rem/1 "Cascadia Code", "DejaVu Sans Mono", monospace;
      cursor: pointer;
    }

    .mars-date-picker-nav:hover,
    .mars-date-picker-nav:focus-visible {
      border-color: rgba(239, 195, 106, 0.46);
      outline: none;
      box-shadow: 0 0 0 3px rgba(113, 198, 180, 0.16);
    }

    .mars-date-picker-weekdays,
    .mars-date-picker-grid {
      display: grid;
      grid-template-columns: repeat(7, minmax(0, 1fr));
      gap: 0.35rem;
    }

    .mars-date-picker-weekdays {
      margin-bottom: 0.45rem;
    }

    .mars-date-picker-weekday {
      text-align: center;
      color: rgba(215, 231, 183, 0.82);
      font: 0.66rem/1.2 "Cascadia Code", "DejaVu Sans Mono", monospace;
      letter-spacing: 0.1em;
      text-transform: uppercase;
    }

    .mars-date-picker-day {
      min-height: 2.45rem;
      border: 1px solid rgba(233, 244, 239, 0.14);
      border-radius: 13px;
      background: rgba(0, 0, 0, 0.14);
      color: var(--code);
      font: 0.85rem/1 "Cascadia Code", "DejaVu Sans Mono", monospace;
      cursor: pointer;
      transition: transform 120ms ease, border-color 140ms ease, background 140ms ease;
    }

    .mars-date-picker-day:hover,
    .mars-date-picker-day:focus-visible {
      transform: translateY(-1px);
      border-color: rgba(239, 195, 106, 0.42);
      outline: none;
      background: rgba(33, 82, 55, 0.52);
    }

    .mars-date-picker-day.outside {
      color: rgba(233, 244, 239, 0.26);
      background: rgba(0, 0, 0, 0.08);
    }

    .mars-date-picker-day.today {
      border-color: rgba(113, 198, 180, 0.36);
      color: #d7f0d7;
    }

    .mars-date-picker-day.selected {
      border-color: rgba(239, 195, 106, 0.58);
      background:
        linear-gradient(180deg, rgba(205, 135, 60, 0.24), rgba(66, 123, 86, 0.24));
      color: #fff3cf;
      box-shadow: inset 0 1px 0 rgba(255, 241, 196, 0.12);
    }

    .mars-date-picker-foot {
      display: flex;
      justify-content: space-between;
      gap: 0.6rem;
      margin-top: 0.72rem;
    }

    .mars-date-picker-foot button {
      flex: 1;
      min-height: 2.35rem;
      border-radius: 999px;
      border: 1px solid rgba(233, 244, 239, 0.2);
      background: rgba(0, 0, 0, 0.14);
      color: #bed3c0;
      font: 0.76rem/1 "Cascadia Code", "DejaVu Sans Mono", monospace;
      letter-spacing: 0.08em;
      text-transform: uppercase;
      cursor: pointer;
    }

    .mars-date-picker-foot button:hover,
    .mars-date-picker-foot button:focus-visible {
      outline: none;
      border-color: rgba(239, 195, 106, 0.4);
      box-shadow: 0 0 0 3px rgba(113, 198, 180, 0.14);
    }

    .mars-date-picker-anchor {
      position: relative;
    }

    .datetime-local {
      display: grid;
      gap: 0.55rem;
      margin-top: 0.35rem;
      border: 1px solid rgba(233, 244, 239, 0.22);
      border-radius: 18px;
      padding: 0.8rem;
      background:
        linear-gradient(135deg, rgba(6, 22, 18, 0.44), rgba(28, 61, 42, 0.38));
      box-shadow: inset 0 1px 0 rgba(233, 244, 239, 0.12);
    }

    .datetime-local-title {
      color: #d7e7b7;
      font: 0.78rem/1.2 "Cascadia Code", "DejaVu Sans Mono", monospace;
      letter-spacing: 0.1em;
      text-transform: uppercase;
    }

    .datetime-local-body {
      max-height: 20rem;
      overflow: auto;
      color: var(--code);
      white-space: normal;
      font: 0.82rem/1.45 "Cascadia Code", "Fira Code", "DejaVu Sans Mono", monospace;
      scrollbar-color: rgba(207, 160, 82, 0.74) rgba(7, 25, 19, 0.62);
    }

    .datetime-section-grid {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(16rem, 1fr));
      gap: 0.75rem;
      white-space: normal;
      font: 0.86rem/1.45 "Cascadia Code", "Fira Code", "DejaVu Sans Mono", monospace;
    }

    .datetime-section {
      border: 1px solid rgba(233, 244, 239, 0.2);
      border-radius: 15px;
      overflow: hidden;
      background:
        linear-gradient(135deg, rgba(7, 25, 19, 0.42), rgba(35, 73, 49, 0.26));
    }

    .datetime-section summary {
      display: flex;
      align-items: center;
      justify-content: space-between;
      gap: 0.7rem;
      padding: 0.62rem 0.75rem;
      color: #f3e4a4;
      cursor: pointer;
      font-weight: 800;
      letter-spacing: 0.04em;
      background: rgba(196, 131, 48, 0.09);
    }

    .datetime-section summary::-webkit-details-marker {
      display: none;
    }

    .datetime-section summary::after {
      content: "+";
      color: #d7e7b7;
      font-weight: 900;
    }

    .datetime-section[open] summary::after {
      content: "-";
    }

    .datetime-section-rows {
      display: grid;
      gap: 0.4rem;
      padding: 0.7rem 0.75rem 0.8rem;
    }

    .datetime-row {
      display: grid;
      grid-template-columns: minmax(8rem, 1fr) auto;
      gap: 0.7rem;
      align-items: start;
      border-bottom: 1px solid rgba(233, 244, 239, 0.09);
      padding-bottom: 0.38rem;
    }

    .datetime-row:last-child {
      border-bottom: 0;
      padding-bottom: 0;
    }

    .datetime-row-label {
      color: #d7e7b7;
      font-weight: 800;
    }

    .datetime-row-value {
      color: var(--code);
      text-align: right;
      overflow-wrap: anywhere;
    }

    .integrator-bound-field input:-webkit-autofill,
    .integrator-bound-field input:-webkit-autofill:hover,
    .integrator-bound-field input:-webkit-autofill:focus {
      -webkit-text-fill-color: var(--code);
      box-shadow: 0 0 0 1000px rgba(0, 0, 0, 0.14) inset;
    }

    .mode-panel input:focus {
      border-color: color-mix(in srgb, var(--accent), var(--line) 25%);
      box-shadow: 0 0 0 3px rgba(113, 198, 180, 0.18);
    }

    .mode-panel input::placeholder {
      color: rgba(233, 244, 239, 0.14);
      opacity: 1;
    }

    .mode-panel input::-webkit-input-placeholder {
      color: rgba(233, 244, 239, 0.14);
    }

    .integrator-bound-field input::placeholder {
      color: rgba(233, 244, 239, 0.10) !important;
      opacity: 1;
    }

    .integrator-bound-field input::-webkit-input-placeholder {
      color: rgba(233, 244, 239, 0.10) !important;
      -webkit-text-fill-color: rgba(233, 244, 239, 0.10) !important;
    }

    .integrator-bound-field input:placeholder-shown {
      color: rgba(233, 244, 239, 0.10) !important;
      -webkit-text-fill-color: rgba(233, 244, 239, 0.10) !important;
    }

    .integrator-bound-field input:focus {
      color: var(--code);
      -webkit-text-fill-color: var(--code);
    }

    .integrator-bound-field input:-webkit-autofill:focus {
      box-shadow:
        0 0 0 1000px rgba(0, 0, 0, 0.14) inset,
        0 0 0 3px rgba(113, 198, 180, 0.18);
    }

    .mode-panel select {
      width: 100%;
      border: 1px solid rgba(233, 244, 239, 0.28);
      border-radius: 999px;
      outline: 0;
      padding: 0.65rem 0.9rem;
      color: var(--code);
      background-color: rgba(0, 0, 0, 0.14);
      font: 0.95rem/1.25 "Cascadia Code", "Fira Code", "DejaVu Sans Mono", monospace;
      appearance: none;
      -webkit-appearance: none;
      color-scheme: dark;
      background-image:
        linear-gradient(45deg, transparent 50%, rgba(233, 244, 239, 0.88) 50%),
        linear-gradient(135deg, rgba(233, 244, 239, 0.88) 50%, transparent 50%),
        linear-gradient(180deg, rgba(16, 51, 38, 0.96), rgba(9, 28, 21, 0.96));
      background-position:
        calc(100% - 1.25rem) calc(50% - 0.12rem),
        calc(100% - 0.9rem) calc(50% - 0.12rem),
        0 0;
      background-size: 0.38rem 0.38rem, 0.38rem 0.38rem, 100% 100%;
      background-repeat: no-repeat;
      box-shadow:
        inset 0 0 0 1px rgba(255, 255, 255, 0.02),
        0 0.55rem 1.2rem rgba(0, 0, 0, 0.14);
      padding-right: 2.4rem;
    }

    .mode-panel select:hover {
      border-color: rgba(233, 244, 239, 0.42);
    }

    .mode-panel select:focus {
      border-color: color-mix(in srgb, var(--accent), var(--line) 25%);
      box-shadow: 0 0 0 3px rgba(113, 198, 180, 0.18);
    }

    .mode-panel select option {
      color: var(--code);
      background: #071913;
    }

    .mode-panel select option:checked {
      color: #10190f;
      background: #cfa052;
    }

    .select-shell {
      position: relative;
      width: 100%;
    }

    .select-shell > select.select-native-source {
      position: absolute;
      left: 0;
      top: 0;
      width: 1px;
      height: 1px;
      opacity: 0;
      pointer-events: none;
    }

    .select-button {
      display: flex;
      align-items: center;
      justify-content: space-between;
      gap: 1rem;
      width: 100%;
      border: 1px solid rgba(233, 244, 239, 0.28);
      border-radius: 999px;
      outline: 0;
      padding: 0.65rem 0.9rem;
      color: var(--code);
      background:
        linear-gradient(180deg, rgba(16, 51, 38, 0.96), rgba(9, 28, 21, 0.96));
      box-shadow:
        inset 0 0 0 1px rgba(255, 255, 255, 0.02),
        0 0.55rem 1.2rem rgba(0, 0, 0, 0.14);
      font: 0.95rem/1.25 "Cascadia Code", "Fira Code", "DejaVu Sans Mono", monospace;
      font-weight: 400;
      text-align: left;
      cursor: pointer;
    }

    .select-button::after {
      content: "";
      width: 0.48rem;
      height: 0.48rem;
      border-right: 2px solid rgba(233, 244, 239, 0.86);
      border-bottom: 2px solid rgba(233, 244, 239, 0.86);
      transform: rotate(45deg) translateY(-0.12rem);
      flex: 0 0 auto;
    }

    .select-button:hover {
      border-color: rgba(233, 244, 239, 0.42);
    }

    .select-button:focus-visible,
    .select-shell.open .select-button {
      border-color: color-mix(in srgb, var(--accent), var(--line) 25%);
      box-shadow: 0 0 0 3px rgba(113, 198, 180, 0.18);
    }

    .select-menu {
      position: absolute;
      left: 0;
      right: 0;
      top: calc(100% + 0.4rem);
      z-index: 40;
      display: grid;
      gap: 0.3rem;
      max-height: min(19rem, 48vh);
      padding: 0.35rem;
      border: 1px solid rgba(233, 244, 239, 0.32);
      border-radius: 18px;
      color: var(--code);
      background:
        linear-gradient(180deg, rgba(16, 51, 38, 0.98), rgba(7, 25, 19, 0.98));
      box-shadow:
        0 1.2rem 2.4rem rgba(0, 0, 0, 0.32),
        inset 0 0 0 1px rgba(255, 255, 255, 0.03);
      scrollbar-color: rgba(207, 160, 82, 0.74) rgba(7, 25, 19, 0.62);
    }

    .select-menu.hidden {
      display: none;
    }

    .select-search {
      width: 100%;
      border: 1px solid rgba(233, 244, 239, 0.22);
      border-radius: 12px;
      padding: 0.55rem 0.7rem;
      color: var(--code);
      background: rgba(0, 0, 0, 0.18);
      font: 0.84rem/1.2 "Cascadia Code", "Fira Code", "DejaVu Sans Mono", monospace;
      outline: 0;
    }

    .select-search::placeholder {
      color: rgba(215, 231, 183, 0.52);
    }

    .select-search:focus {
      border-color: color-mix(in srgb, var(--accent), var(--line) 25%);
      box-shadow: 0 0 0 3px rgba(113, 198, 180, 0.14);
    }

    .select-options {
      display: grid;
      gap: 0.18rem;
      max-height: min(15.5rem, 38vh);
      overflow-y: auto;
      padding-right: 0.08rem;
      scrollbar-color: rgba(207, 160, 82, 0.74) rgba(7, 25, 19, 0.62);
    }

    .select-option {
      width: 100%;
      border: 0;
      border-radius: 12px;
      padding: 0.55rem 0.7rem;
      color: var(--code);
      background: transparent;
      box-shadow: none;
      font: 0.9rem/1.25 "Cascadia Code", "Fira Code", "DejaVu Sans Mono", monospace;
      font-weight: 400;
      text-align: left;
      cursor: pointer;
    }

    .select-option:hover,
    .select-option:focus-visible {
      outline: 0;
      color: #f8fff8;
      background: rgba(113, 198, 180, 0.16);
    }

    .select-option.selected {
      color: #10190f;
      background: linear-gradient(135deg, rgba(233, 187, 90, 0.96), rgba(140, 216, 184, 0.94));
    }

    .select-option.hidden {
      display: none;
    }

    .select-empty {
      display: none;
      border-radius: 12px;
      padding: 0.65rem 0.7rem;
      color: rgba(215, 231, 183, 0.72);
      background: rgba(0, 0, 0, 0.14);
      font: 0.82rem/1.3 "Cascadia Code", "Fira Code", "DejaVu Sans Mono", monospace;
    }

    .select-empty.visible {
      display: block;
    }

    .mode-hint {
      margin: -0.15rem 0 0;
      color: var(--muted);
      font-size: 0.88rem;
    }

    .target-row {
      display: grid;
      grid-template-columns: auto minmax(0, 1fr);
      align-items: center;
      gap: 0.75rem;
      padding: 0 1rem 1rem;
    }

    .target-row label {
      color: #bed3c0;
      font: 0.78rem/1.2 "Cascadia Code", "DejaVu Sans Mono", monospace;
      letter-spacing: 0.1em;
      text-transform: uppercase;
    }

    .target-row input {
      width: 100%;
      border: 1px solid rgba(233, 244, 239, 0.28);
      border-radius: 999px;
      outline: 0;
      padding: 0.65rem 0.9rem;
      color: var(--code);
      background: rgba(0, 0, 0, 0.14);
      font: 0.95rem/1.25 "Cascadia Code", "Fira Code", "DejaVu Sans Mono", monospace;
    }

    .target-row input:focus {
      border-color: color-mix(in srgb, var(--accent), var(--line) 25%);
      box-shadow: 0 0 0 3px rgba(113, 198, 180, 0.18);
    }

    .goal-start-fields {
      display: contents;
    }

    .controls {
      display: flex;
      flex-wrap: wrap;
      gap: 0.65rem;
      padding: 0 1rem 1rem;
    }

    .variable-values {
      display: grid;
      gap: 0.7rem;
      padding: 0 1rem 1rem;
    }

    .variable-value-box {
      display: grid;
      grid-template-columns: minmax(2.4rem, auto) minmax(0, 1fr) auto;
      align-items: start;
      gap: 0.7rem;
      border: 1px solid rgba(233, 244, 239, 0.28);
      border-radius: 18px;
      padding: 0.55rem;
      background:
        linear-gradient(135deg, rgba(6, 22, 18, 0.54), rgba(26, 61, 45, 0.52));
      box-shadow: inset 0 1px 0 rgba(233, 244, 239, 0.18);
    }

    .constant-value-box {
      border-color: rgba(123, 211, 209, 0.38);
      background:
        linear-gradient(135deg, rgba(7, 31, 33, 0.58), rgba(19, 72, 68, 0.48));
    }

    .variable-value-name {
      display: inline-grid;
      min-width: 2.15rem;
      min-height: 2.15rem;
      place-items: center;
      border-radius: 999px;
      color: #061612;
      background: #cfa052;
      font-weight: 700;
      font-family: "Cascadia Code", "DejaVu Sans Mono", monospace;
    }

    .constant-value-name {
      color: #062022;
      background: #7bd3d1;
    }

    .variable-value-text {
      min-width: 0;
      max-height: 4.6rem;
      overflow: hidden;
      border: 1px solid rgba(233, 244, 239, 0.22);
      border-radius: 14px;
      padding: 0.5rem 0.7rem;
      color: #f3f8f2;
      background: rgba(0, 0, 0, 0.14);
      overflow-wrap: anywhere;
      white-space: pre-wrap;
      font: 0.82rem/1.35 "Cascadia Code", "Fira Code", "DejaVu Sans Mono", monospace;
    }

    .binding-value-input {
      width: 100%;
      outline: none;
    }

    .binding-value-input:focus {
      border-color: rgba(207, 160, 82, 0.72);
      box-shadow: 0 0 0 3px rgba(207, 160, 82, 0.16);
    }

    .constant-value-box .binding-value-input:focus {
      border-color: rgba(123, 211, 209, 0.76);
      box-shadow: 0 0 0 3px rgba(123, 211, 209, 0.15);
    }

    .variable-value-box.expanded .variable-value-text {
      max-height: none;
      overflow: visible;
    }

    .variable-value-actions {
      display: flex;
      flex-direction: column;
      gap: 0.45rem;
      align-self: stretch;
    }

    .variable-copy {
      min-width: 4.1rem;
    }

    .variable-expand {
      min-width: 4.1rem;
    }

    .header-side {
      display: grid;
      justify-items: end;
      gap: 0.55rem;
    }

    .mobile-card {
      position: relative;
      z-index: 30;
      width: min(24rem, calc(100vw - 2rem));
    }

    .mobile-card summary {
      display: inline-flex;
      align-items: center;
      gap: 0.45rem;
      list-style: none;
      border-radius: 999px;
      padding: 0.42rem 0.78rem;
      color: #061612;
      background: #cfa052;
      font: 0.76rem/1.1 "Cascadia Code", "DejaVu Sans Mono", monospace;
      font-weight: 700;
      letter-spacing: 0.04em;
      text-transform: uppercase;
      cursor: pointer;
      user-select: none;
    }

    .mobile-card summary::-webkit-details-marker {
      display: none;
    }

    .mobile-card summary::before {
      content: "";
      width: 0.48rem;
      height: 0.48rem;
      border-radius: 999px;
      background: #71c6b4;
      box-shadow: 0 0 0 4px rgba(113, 198, 180, 0.18);
    }

    .mobile-panel {
      position: absolute;
      right: 0;
      top: calc(100% + 0.5rem);
      display: grid;
      grid-template-columns: minmax(0, 1fr) auto;
      align-items: center;
      gap: 1rem;
      width: min(31rem, calc(100vw - 2rem));
      padding: 0.75rem;
      border: 2px solid rgba(233, 244, 239, 0.34);
      border-radius: 18px;
      background:
        linear-gradient(135deg, rgba(8, 29, 22, 0.99), rgba(18, 51, 38, 0.98));
      box-shadow: 0 20px 58px rgba(0, 0, 0, 0.34);
    }

    .mobile-copy {
      display: grid;
      gap: 0.28rem;
      min-width: 0;
    }

    .mobile-copy strong {
      color: #d7e7b7;
      font: 0.82rem/1.2 "Cascadia Code", "DejaVu Sans Mono", monospace;
      letter-spacing: 0.08em;
      text-transform: uppercase;
    }

    .mobile-copy span {
      color: var(--muted);
      font-size: 0.92rem;
    }

    .mobile-copy code {
      min-width: 0;
      overflow-wrap: anywhere;
      color: var(--code);
      font: 0.9rem/1.35 "Cascadia Code", "Fira Code", "DejaVu Sans Mono", monospace;
    }

    .mobile-actions {
      display: flex;
      flex-direction: column;
      align-items: center;
      gap: 0.55rem;
    }

    .mobile-qr {
      width: 9rem;
      height: 9rem;
      padding: 0.45rem;
      border-radius: 16px;
      background: #fffdf4;
      border: 2px solid rgba(233, 244, 239, 0.24);
    }

    .mobile-qr svg {
      display: block;
      width: 100%;
      height: 100%;
    }

    button {
      border: 2px solid #243238;
      border-radius: 999px;
      padding: 0.7rem 1rem;
      color: #061612;
      background: var(--accent);
      font-weight: 700;
      cursor: pointer;
      box-shadow: 0 0.38rem 0 rgba(0, 0, 0, 0.2), 0 10px 24px rgba(196, 131, 48, 0.16);
    }

    button.secondary {
      color: #d7e7b7;
      background: rgba(113, 198, 180, 0.12);
      box-shadow: 0 0.18rem 0 rgba(0, 0, 0, 0.18);
    }

    button:disabled {
      cursor: not-allowed;
      opacity: 0.55;
    }

    .output-grid {
      display: grid;
      gap: 1rem;
      padding: 1rem;
    }

    .card {
      --result-zoom: 1;
      --render-base-scale: 2;
      --render-zoom: 2;
      --result-base-font-rem: 0.92;
      --result-font-size: 0.92rem;
      --render-base-font-rem: 1.78;
      --render-font-size: 1.78rem;
      --render-base-margin-rem: 5;
      --render-margin-bottom: 5rem;
      border: 2px solid rgba(233, 244, 239, 0.28);
      border-radius: 18px;
      background: rgba(8, 29, 22, 0.62);
      overflow: hidden;
    }

    .output-grid.card-expanded {
      min-height: clamp(26rem, 68vh, 54rem);
      grid-template-rows: minmax(0, 1fr);
    }

    .output-grid.card-expanded .card:not(.expanded-card) {
      display: none;
    }

    .card.expanded-card {
      display: flex;
      flex-direction: column;
      min-height: 0;
    }

    .card.expanded-card > pre,
    .card.expanded-card > .rendered {
      flex: 1 1 auto;
      min-height: 0;
    }

    .card-title {
      display: flex;
      align-items: center;
      justify-content: space-between;
      gap: 0.75rem;
      padding: 0.55rem 0.75rem;
      color: var(--muted);
      background: rgba(196, 131, 48, 0.08);
      border-bottom: 2px solid rgba(233, 244, 239, 0.22);
      font: 0.78rem/1.2 "Cascadia Code", "DejaVu Sans Mono", monospace;
      text-transform: uppercase;
      letter-spacing: 0.1em;
    }

    .card-action {
      padding: 0.38rem 0.65rem;
      color: #d7e7b7;
      background: rgba(113, 198, 180, 0.12);
      box-shadow: none;
      font: 0.72rem/1.1 "Cascadia Code", "DejaVu Sans Mono", monospace;
      letter-spacing: 0.04em;
      text-transform: uppercase;
      min-width: 4.25rem;
      transition: background 150ms ease, color 150ms ease, transform 150ms ease;
    }

    .zoom-action {
      min-width: 2.25rem;
      padding-inline: 0.48rem;
    }

    .zoom-reset {
      min-width: 3.65rem;
    }

    .zoom-action:disabled {
      opacity: 0.42;
    }

    .card-action.copied {
      color: #061612;
      background: var(--accent);
      transform: translateY(-1px);
    }

    .card-action.copy-failed {
      color: white;
      background: #991b1b;
    }

    .card-actions {
      display: flex;
      align-items: center;
      justify-content: flex-end;
      flex-wrap: wrap;
      gap: 0.45rem;
    }

    .expandable-title {
      display: grid;
      grid-template-columns: 1fr auto 1fr;
      align-items: center;
    }

    .digit-actions {
      justify-content: center;
      grid-column: 2;
    }

    .top-card-copy {
      justify-self: end;
      grid-column: 3;
    }

    .top-card-actions {
      justify-self: end;
      grid-column: 3;
    }

    .value-title {
      display: grid;
      grid-template-columns: 1fr auto 1fr;
      align-items: center;
    }

    .precision-actions {
      justify-content: center;
      grid-column: 2;
    }

    .value-copy {
      justify-self: end;
      grid-column: 3;
    }

    .value-card-actions {
      justify-self: end;
      grid-column: 3;
    }

    pre {
      margin: 0;
      padding: 0.9rem;
      overflow: auto;
      min-height: 2.75rem;
    }

    .card pre,
    .card .rendered,
    .matrix-pretty {
      scrollbar-width: thin;
      scrollbar-color: rgba(227, 180, 87, 0.72) rgba(8, 29, 22, 0.62);
    }

    .card pre::-webkit-scrollbar,
    .card .rendered::-webkit-scrollbar,
    .matrix-pretty::-webkit-scrollbar {
      width: 0.72rem;
      height: 0.72rem;
    }

    .card pre::-webkit-scrollbar-track,
    .card .rendered::-webkit-scrollbar-track,
    .matrix-pretty::-webkit-scrollbar-track {
      border-radius: 999px;
      background:
        linear-gradient(180deg, rgba(8, 29, 22, 0.76), rgba(18, 53, 39, 0.58));
      box-shadow: inset 0 0 0 1px rgba(233, 244, 239, 0.12);
    }

    .card pre::-webkit-scrollbar-thumb,
    .card .rendered::-webkit-scrollbar-thumb,
    .matrix-pretty::-webkit-scrollbar-thumb {
      border: 2px solid rgba(8, 29, 22, 0.82);
      border-radius: 999px;
      background:
        linear-gradient(135deg, rgba(227, 180, 87, 0.94), rgba(113, 198, 180, 0.82));
      box-shadow: inset 0 0 0 1px rgba(255, 250, 220, 0.18);
    }

    .card pre::-webkit-scrollbar-thumb:hover,
    .card .rendered::-webkit-scrollbar-thumb:hover,
    .matrix-pretty::-webkit-scrollbar-thumb:hover {
      background:
        linear-gradient(135deg, rgba(247, 205, 112, 0.98), rgba(139, 222, 195, 0.92));
    }

    pre {
      color: var(--code);
      white-space: pre-wrap;
      font-family: "Cascadia Code", "DejaVu Sans Mono", monospace;
      font-size: var(--result-font-size);
      line-height: 1.45;
    }

    #value {
      overflow: auto;
      white-space: pre-wrap;
      overflow-wrap: anywhere;
      word-break: break-all;
    }

    .matrix-pretty {
      overflow-x: auto;
      overflow-y: visible;
      white-space: pre-wrap;
      font: 1.05rem/1.5 "Cascadia Code", "DejaVu Sans Mono", monospace;
    }

    .matrix-display {
      display: inline-grid;
      grid-template-columns: auto auto auto;
      align-items: stretch;
      gap: 0.45rem;
      color: var(--code);
      font-variant-ligatures: none;
    }

    .matrix-bracket {
      display: flex;
      align-items: center;
      color: #f0f5d6;
      font: 2.7rem/1 Georgia, "Times New Roman", serif;
      transform: scaleY(1.18);
    }

    .matrix-grid {
      display: grid;
      align-items: center;
      gap: 0.3rem 1.25rem;
      padding: 0.35rem 0.1rem;
    }

    .matrix-cell {
      min-width: 2.5rem;
      text-align: right;
      white-space: nowrap;
      font: 1.22rem/1.45 "Cascadia Code", "DejaVu Sans Mono", monospace;
    }

    .matrix-section-heading {
      color: var(--cream);
      font-weight: 800;
      letter-spacing: 0.08em;
      text-transform: lowercase;
    }

    .rendered {
      margin: 0;
      min-height: 12rem;
      padding: 2.1rem 1.6rem 3rem;
      overflow: auto;
      font-size: var(--render-font-size);
    }

    .rendered-zoom-frame {
      position: relative;
      display: inline-block;
      min-width: max-content;
      min-height: 1px;
    }

    .rendered svg {
      display: block;
      max-width: none;
      height: auto;
      overflow: visible;
      transform: scale(var(--render-zoom));
      transform-origin: left top;
      margin-bottom: var(--render-margin-bottom);
      filter: brightness(0) saturate(100%) invert(82%) sepia(39%) saturate(540%) hue-rotate(354deg) brightness(98%) contrast(92%) drop-shadow(0 0 0.65rem rgba(113, 198, 180, 0.28));
    }

    .error,
    .rendered.error,
    #rendered.error {
      color: #ffd99a !important;
      background:
        radial-gradient(circle at 14% 18%, rgba(229, 173, 87, 0.16), transparent 34%),
        linear-gradient(135deg, rgba(73, 23, 25, 0.88), rgba(38, 12, 19, 0.78)) !important;
      border-color: rgba(229, 173, 87, 0.42) !important;
      box-shadow:
        inset 0 0 0 1px rgba(255, 232, 181, 0.07),
        0 0 1.35rem rgba(153, 27, 27, 0.22) !important;
      text-shadow: 0 0 0.7rem rgba(255, 204, 112, 0.16) !important;
    }

    .rendered.error,
    #rendered.error {
      font-family: Georgia, "Times New Roman", serif;
    }

    .help-pane {
      padding: 1rem;
    }

    .help-card {
      padding: 1rem;
      border: 2px solid rgba(233, 244, 239, 0.24);
      border-radius: 18px;
      background: rgba(8, 29, 22, 0.62);
    }

    .help-card + .help-card {
      margin-top: 1rem;
    }

    .help-card h3 {
      margin: 0 0 0.65rem;
      color: var(--accent-2);
      font-size: 1rem;
      letter-spacing: 0.04em;
      text-transform: uppercase;
    }

    .help-card p {
      margin: 0 0 0.7rem;
      color: var(--muted);
    }

    .help-card .help-kicker {
      margin: 0 0 0.8rem;
      color: #d7e7b7;
      font: 0.76rem/1.2 "Cascadia Code", "DejaVu Sans Mono", monospace;
      letter-spacing: 0.1em;
      text-transform: uppercase;
    }

    .help-card ul {
      margin: 0;
      padding-left: 1.25rem;
    }

    .help-card li {
      margin: 0.35rem 0;
    }

    .help-card code {
      color: var(--code);
      font: 0.92rem/1.35 "Cascadia Code", "DejaVu Sans Mono", monospace;
      background: rgba(113, 198, 180, 0.12);
      border-radius: 6px;
      padding: 0.08rem 0.25rem;
    }

    .hidden {
      display: none;
    }

    .card.hidden {
      display: none !important;
    }

    @media (max-width: 900px) {
      body {
        font-size: 15px;
      }

      header {
        display: block;
        padding: 1rem 0.75rem 0.45rem;
      }

      .header-side {
        justify-items: start;
        margin-top: 0.5rem;
      }

      .status {
        text-align: left;
      }

      main {
        grid-template-columns: 1fr;
        gap: 0.75rem;
        padding: 0.5rem 0.75rem 1.25rem;
      }

      .lab-topbar {
        align-items: stretch;
      }

      .precision-toolbar {
        justify-content: flex-start;
      }

      section {
        border-radius: 17px;
      }

      .panel-head {
        padding: 0.7rem 0.8rem;
      }

      h2 {
        font-size: 0.82rem;
      }

      textarea {
        min-height: 8.5rem;
        padding: 0.85rem;
        font-size: 0.96rem;
        line-height: 1.45;
      }

      .integrator-bound-row {
        grid-template-columns: 1fr;
      }

      .datetime-grid {
        grid-template-columns: 1fr;
      }

      .datetime-grid.two-up {
        grid-template-columns: 1fr;
      }

      .datetime-field-group {
        padding: 0.75rem;
      }

      .target-row {
        padding: 0 0.75rem 0.75rem;
      }

      .target-row input {
        min-height: 2.75rem;
      }

      .controls {
        display: grid;
        grid-template-columns: repeat(3, minmax(0, 1fr));
        gap: 0.5rem;
        padding: 0 0.75rem 0.75rem;
      }

      .controls button {
        width: 100%;
        min-height: 2.75rem;
        padding: 0.65rem 0.45rem;
      }

      .derivative-controls {
        grid-template-columns: repeat(2, minmax(0, 1fr));
      }

      .variable-values {
        padding: 0 0.75rem 0.75rem;
      }

      .variable-value-box {
        grid-template-columns: minmax(2.2rem, auto) minmax(0, 1fr);
      }

      .variable-value-actions {
        grid-column: 1 / -1;
        flex-direction: row;
      }

      .variable-value-actions button {
        min-height: 2.5rem;
      }

      .variable-copy {
        display: none;
      }

      .output-grid {
        gap: 0.75rem;
        padding: 0.75rem;
      }

      .card {
        border-radius: 15px;
        --render-base-scale: 1.35;
        --render-zoom: 1.35;
        --result-base-font-rem: 0.82;
        --result-font-size: 0.82rem;
        --render-base-font-rem: 1.3;
        --render-font-size: 1.3rem;
        --render-base-margin-rem: 2.5;
        --render-margin-bottom: 2.5rem;
      }

      .mobile-result-extra {
        display: none;
      }

      #resultPane .copy-result {
        display: none;
      }

      .card-title {
        padding: 0.5rem 0.6rem;
        gap: 0.45rem;
        font-size: 0.68rem;
        letter-spacing: 0.08em;
      }

      .card-action {
        min-height: 2.25rem;
        padding: 0.45rem 0.55rem;
      }

      pre {
        padding: 0.75rem;
        font-size: 0.82rem;
      }

      .rendered {
        min-height: 8rem;
        padding: 1.35rem 1rem 2rem;
        font-size: var(--render-font-size);
      }

      .rendered svg {
        margin-bottom: var(--render-margin-bottom);
      }

      .mobile-panel {
        position: static;
        margin-top: 0.5rem;
        grid-template-columns: 1fr;
      }

      .mobile-actions {
        flex-direction: row;
        justify-content: space-between;
      }
    }

    @media (max-width: 560px) {
      h1 {
        font-size: 2rem;
      }

      .subtitle {
        display: none;
      }

      .mobile-card {
        width: 100%;
      }

      .mobile-card summary {
        padding: 0.38rem 0.68rem;
        font-size: 0.7rem;
      }

      .mobile-copy code {
        font-size: 0.78rem;
      }

      .mobile-actions {
        flex-wrap: wrap;
        justify-content: flex-start;
      }

      .mobile-qr {
        display: none;
      }

      main {
        padding: 0.45rem 0.5rem 1rem;
      }

      textarea {
        min-height: 7rem;
        font-size: 0.9rem;
      }

      .target-row {
        grid-template-columns: 1fr;
        gap: 0.35rem;
      }

      .goal-start-fields {
        display: grid;
        grid-template-columns: 1fr;
        gap: 0.35rem;
      }

      .controls {
        grid-template-columns: repeat(2, minmax(0, 1fr));
      }

      .expandable-title,
      .value-title {
        grid-template-columns: 1fr;
      }

      .digit-actions,
      .precision-actions,
      .top-card-copy,
      .value-copy {
        grid-column: auto;
        justify-self: stretch;
        justify-content: center;
      }

      .digit-actions,
      .precision-actions {
        order: 3;
      }

      .top-card-copy,
      .value-copy {
        order: 2;
      }

      .precision-actions {
        display: grid;
        grid-template-columns: repeat(2, minmax(0, 1fr));
      }

      .rendered {
        min-height: 6.5rem;
        padding: 1rem 0.75rem 1.75rem;
        font-size: var(--render-font-size);
      }

      .rendered svg {
        transform: scale(var(--render-zoom));
        margin-bottom: var(--render-margin-bottom);
      }

      .help-pane {
        padding: 0.75rem;
      }

      .help-card {
        padding: 0.8rem;
      }
    }
__THEME_OVERRIDES__
  </style>
</head>
<body class="__BODY_CLASS__">
  <div class="celtic-backdrop" aria-hidden="true">
    <div class="aurora"></div>
    <div class="standing-stones">
      <span class="stone"></span>
      <span class="stone"></span>
      <span class="stone"></span>
      <span class="stone"></span>
      <span class="stone"></span>
      <span class="stone"></span>
    </div>
    <div class="chariot-wheel"></div>
  </div>
  <header>
    <div>
      <h1>__LAB_NAME__</h1>
      <p class="subtitle" id="subtitle">__LAB_SUBTITLE__</p>
    </div>
    <div class="header-side">
      <div class="status" id="status">Ready</div>
      <details class="mobile-card __MOBILE_CARD_CLASS__" id="mobileAccess">
        <summary>Mobile</summary>
        <div class="mobile-panel">
          <div class="mobile-copy">
            <strong id="mobileTitle">__MOBILE_TITLE__</strong>
            <span id="mobileHint">__MOBILE_HINT__</span>
            <code id="mobileUrl">__MOBILE_URL__</code>
          </div>
          <div class="mobile-actions">
            <div class="mobile-qr" id="mobileQr" aria-label="QR code for mobile access">__MOBILE_QR_SVG__</div>
            <button class="card-action copy-result" type="button" data-copy-target="mobile">Copy URL</button>
          </div>
        </div>
      </details>
    </div>
  </header>
  <main>
    <div class="lab-topbar">
      <div class="lab-tabs" role="tablist" aria-label="__LAB_NAME__ mode selector">
        <button class="mode-tab active" id="modeTabExpression" type="button" role="tab" aria-selected="true" aria-controls="workspacePanel" data-mode="expression">Expression</button>
        <button class="mode-tab" id="modeTabEquation" type="button" role="tab" aria-selected="false" aria-controls="workspacePanel" data-mode="equation">Equation</button>
        <button class="mode-tab" id="modeTabMatrix" type="button" role="tab" aria-selected="false" aria-controls="workspacePanel" data-mode="matrix">Matrix</button>
        <button class="mode-tab" id="modeTabIntegrator" type="button" role="tab" aria-selected="false" aria-controls="workspacePanel" data-mode="integrator">Integrator</button>
        <button class="mode-tab" id="modeTabDatetime" type="button" role="tab" aria-selected="false" aria-controls="workspacePanel" data-mode="datetime">Datetime</button>
        <button class="mode-tab" id="modeTabAlmanac" type="button" role="tab" aria-selected="false" aria-controls="workspacePanel" data-mode="almanac">Almanac</button>
      </div>
      <div class="precision-toolbar" aria-label="Precision controls">
        <span class="precision-label">Precision</span>
        <button class="card-action" id="lessPrecision" type="button">Less precision</button>
        <button class="card-action" id="morePrecision" type="button">More precision</button>
      </div>
    </div>
    <section id="workspacePanel">
      <div class="panel-head">
        <h2 id="leftPaneTitle">Workspace</h2>
        <button class="card-action" id="inputCopy" type="button">Copy</button>
      </div>
      <textarea id="expr" spellcheck="false" aria-labelledby="leftPaneTitle">__INITIAL_EXPRESSION__</textarea>
      <div class="mode-panel hidden" id="matrixControls">
        <label id="matrixOperationLabel" for="matrixOperation">Matrix operation</label>
        <select id="matrixOperation">
          <option value="eval">Evaluate</option>
          <option value="inverse" selected>Inverse</option>
          <option value="eigenvalues">Eigenvalues</option>
          <option value="eigendecompose">Eigendecompose</option>
          <option value="charpoly">Characteristic polynomial</option>
          <option value="det">Determinant</option>
          <option value="trace">Trace</option>
          <option value="rank">Rank</option>
          <option value="simplify">Simplify symbolic</option>
          <option value="solve">Solve A X = B</option>
        </select>
        <label class="hidden" for="matrixOperand" id="matrixOperandLabel">Right-hand side matrix</label>
        <textarea class="hidden secondary-editor" id="matrixOperand" spellcheck="false" placeholder="(1; 0)"></textarea>
      </div>
      <div class="mode-panel hidden" id="equationControls">
        <p class="mode-hint">Enter an equation such as <code>{ M = E - e*sin(E) | E = 1.5; M = 1.5, e = 0.0167 }</code>. Bindings after <code>|</code> decide which symbols are variables, which are constants, and which starting values numeric fallback should use.</p>
      </div>
      <div class="mode-panel hidden" id="integratorControls">
        <div class="integrator-bound-stack" id="integratorBoundStack"></div>
        <div class="integrator-bound-actions">
          <button class="card-action integrator-bound-add" id="integratorAddBound" type="button">+ Integral</button>
          <span class="integrator-bound-summary">Use <code>Bound</code> rows for variables you actually want to integrate. Use <code>Free</code> only for symbols that appear in the integrand but should stay as parameters.</span>
        </div>
        <label for="integratorIntervalCap">Work budget ceiling</label>
        <select id="integratorIntervalCap">
          <option value="500">Up to 500</option>
          <option value="5000" selected>Up to 5,000</option>
          <option value="20000">Up to 20,000</option>
          <option value="50000">Up to 50,000</option>
          <option value="100000">Up to 100,000</option>
        </select>
        <p class="mode-hint">Examples: use <code>x = 0 .. 1</code> for a definite integral, leave only <code>x</code> with both bounds blank for an antiderivative, or use <code>Free</code> for a parameter such as <code>a</code> in <code>exp(-a*x^2)</code>. Blank spare rows are ignored unless their variable appears in the integrand.</p>
      </div>
      <div class="mode-panel hidden datetime-controls" id="datetimeControls">
        <div class="datetime-briefing" aria-hidden="true">
          <div class="datetime-orbit"></div>
          <div>
            <div class="datetime-briefing-kicker">Sol calendar uplink</div>
            <div class="datetime-briefing-title">Plan an Earth date from the MARS observatory.</div>
            <div class="datetime-briefing-copy">Choose a civil date, a range, and an observation point. The core datetime module handles the calendar and solar calculations; this panel is only the flight deck.</div>
          </div>
        </div>
        <div class="datetime-field-groups">
          <div class="datetime-field-group">
            <div class="datetime-field-group-title">Holiday calendar</div>
            <div class="datetime-grid">
              <div class="integrator-bound-field">
                <label for="datetimeJurisdiction">Holiday country or jurisdiction</label>
                <select id="datetimeJurisdiction">
__HOLIDAY_JURISDICTION_OPTIONS__
                </select>
              </div>
            </div>
          </div>
          <div class="datetime-field-group">
            <div class="datetime-field-group-title">Selected date</div>
            <div class="datetime-grid">
              <div class="integrator-bound-field">
                <label for="datetimeDate">Date</label>
                <div class="mars-date-shell">
                  <input id="datetimeDate" type="text" inputmode="numeric" placeholder="YYYY-MM-DD" autocomplete="off">
                  <button class="mars-date-button" type="button" data-date-target="datetimeDate" aria-label="Open date picker for selected date"></button>
                </div>
              </div>
              <div class="integrator-bound-field">
                <label for="datetimeJdn">Julian Day Number</label>
                <input id="datetimeJdn" inputmode="numeric" pattern="[0-9]*" placeholder="e.g. 2460117">
              </div>
              <div class="integrator-bound-field">
                <label for="datetimeYear">Calendar year</label>
                <input id="datetimeYear" type="number" min="1" max="9999" step="1">
              </div>
            </div>
          </div>
          <div class="datetime-field-group">
            <div class="datetime-field-group-title">Date range</div>
            <div class="datetime-grid two-up">
              <div class="integrator-bound-field">
                <label for="datetimeStart">Start date</label>
                <div class="mars-date-shell">
                  <input id="datetimeStart" type="text" inputmode="numeric" placeholder="YYYY-MM-DD" autocomplete="off">
                  <button class="mars-date-button" type="button" data-date-target="datetimeStart" aria-label="Open date picker for start date"></button>
                </div>
              </div>
              <div class="integrator-bound-field">
                <label for="datetimeEnd">End date</label>
                <div class="mars-date-shell">
                  <input id="datetimeEnd" type="text" inputmode="numeric" placeholder="YYYY-MM-DD" autocomplete="off">
                  <button class="mars-date-button" type="button" data-date-target="datetimeEnd" aria-label="Open date picker for end date"></button>
                </div>
              </div>
            </div>
          </div>
          <div class="datetime-field-group">
            <div class="datetime-field-group-title">Observatory</div>
            <div class="datetime-grid">
              <div class="integrator-bound-field">
                <label for="datetimeLatitude">Latitude</label>
                <input id="datetimeLatitude" inputmode="decimal" placeholder="51.5074">
              </div>
              <div class="integrator-bound-field">
                <label for="datetimeLongitude">Longitude</label>
                <input id="datetimeLongitude" inputmode="decimal" placeholder="-0.1278">
              </div>
              <div class="integrator-bound-field">
                <label for="datetimeElevation">Elevation metres</label>
                <input id="datetimeElevation" inputmode="decimal" placeholder="0">
              </div>
              <div class="integrator-bound-field">
                <label for="datetimeGmtOffset">GMT offset, including daylight saving</label>
                <input id="datetimeGmtOffset" inputmode="decimal" placeholder="blank for jurisdiction local, 1 for BST">
              </div>
            </div>
          </div>
        </div>
        <p class="mode-hint">Blank GMT offset uses the selected jurisdiction's local offset for the selected date. Enter a value yourself only if you want to override that, including daylight saving where applicable.</p>
        <div class="datetime-local hidden" id="datetimeLocal">
          <div class="datetime-local-title">Local</div>
          <div class="datetime-local-body" id="datetimeLocalBody"></div>
        </div>
        <div class="mars-date-picker hidden" id="marsDatePicker" role="dialog" aria-label="Date picker">
          <div class="mars-date-picker-head">
            <button class="mars-date-picker-nav" id="marsDatePickerPrev" type="button" aria-label="Previous month">‹</button>
            <div class="mars-date-picker-title" id="marsDatePickerTitle"></div>
            <button class="mars-date-picker-nav" id="marsDatePickerNext" type="button" aria-label="Next month">›</button>
          </div>
          <div class="mars-date-picker-weekdays" id="marsDatePickerWeekdays"></div>
          <div class="mars-date-picker-grid" id="marsDatePickerGrid"></div>
          <div class="mars-date-picker-foot">
            <button id="marsDatePickerToday" type="button">Today</button>
            <button id="marsDatePickerClose" type="button">Close</button>
          </div>
        </div>
      </div>
      <div class="mode-panel hidden almanac-controls" id="almanacControls">
        <div class="almanac-briefing" aria-hidden="true">
          <div>
            <div class="datetime-briefing-kicker">AstroNav worksheet</div>
            <div class="datetime-briefing-title">Location of Navigational Bodies</div>
            <div class="datetime-briefing-copy">Enter date and time in GMT, then zone, latitude, and longitude. The worksheet starts out in the voice of AstroNav 2000-2040 and uses the live almanac engine underneath.</div>
          </div>
        </div>
        <div class="datetime-field-groups">
          <div class="datetime-field-group">
            <div class="datetime-field-group-title">Set the moment</div>
            <div class="datetime-grid">
              <div class="integrator-bound-field">
                <label for="almanacDate">Date</label>
                <div class="mars-date-shell">
                  <input id="almanacDate" type="text" inputmode="numeric" placeholder="YYYY-MM-DD" autocomplete="off">
                  <button class="mars-date-button" type="button" data-date-target="almanacDate" aria-label="Open date picker for almanac date"></button>
                </div>
              </div>
              <div class="integrator-bound-field">
                <label for="almanacTime">GMT time</label>
                <input id="almanacTime" type="text" inputmode="decimal" placeholder="17:47:05.8">
              </div>
              <div class="integrator-bound-field">
                <label for="almanacZone">Zone</label>
                <input id="almanacZone" inputmode="decimal" placeholder="0">
              </div>
            </div>
          </div>
          <div class="datetime-field-group">
            <div class="datetime-field-group-title">Observer</div>
            <div class="datetime-grid">
              <div class="integrator-bound-field">
                <label for="almanacLatitude">Latitude</label>
                <input id="almanacLatitude" inputmode="decimal" placeholder="51.5074">
              </div>
              <div class="integrator-bound-field">
                <label for="almanacLongitude">Longitude</label>
                <input id="almanacLongitude" inputmode="decimal" placeholder="-0.1278">
              </div>
              <div class="integrator-bound-field">
                <label for="almanacBody">Body</label>
                <select id="almanacBody">
__ALMANAC_BODY_OPTIONS__
                </select>
              </div>
            </div>
          </div>
        </div>
        <p class="mode-hint">For 2000-2040 GMT, the workbook intention is navigation-grade output within 6 arc-seconds after rounding to the nearest arc-second for the listed navigation bodies.</p>
      </div>
      <div class="target-row hidden" id="targetRow">
        <label for="goalTarget">Target</label>
        <input id="goalTarget" spellcheck="false" value="0">
      </div>
      <div class="controls">
        <button id="run">Evaluate</button>
        <button class="secondary" id="back">Back</button>
        <button class="secondary" id="forward">Forward</button>
        <button class="secondary" id="help">Help</button>
        <button class="secondary" id="goalSeek">Goal seek</button>
        <button class="secondary" id="clear">Clear</button>
      </div>
      <div class="controls derivative-controls" id="derivativeButtons"></div>
      <div class="variable-values hidden" id="variableValues"></div>
    </section>

    <section>
      <div class="panel-head">
        <h2 id="rightPaneTitle">Result</h2>
        <button class="card-action result-use-input hidden" id="resultUseInput" type="button">Use as input</button>
      </div>
      <div class="output-grid" id="resultPane">
        <div class="card result-card">
          <div class="card-title expandable-title">
            <span id="renderedTitle">Rendered TeX</span>
            <span class="card-actions digit-actions">
              <button class="card-action more-digits hidden" id="renderedMore">Show more digits</button>
            </span>
            <span class="card-actions top-card-actions">
              <button class="card-action zoom-action" type="button" data-zoom-step="-1" title="Zoom out">−</button>
              <button class="card-action zoom-action zoom-reset" type="button" data-zoom-reset title="Reset zoom">100%</button>
              <button class="card-action zoom-action" type="button" data-zoom-step="1" title="Zoom in">+</button>
              <button class="card-action expand-card" data-expand-card>Expand</button>
              <button class="card-action copy-result" id="renderedCopy" data-copy-target="rendered">Copy</button>
            </span>
          </div>
          <div class="rendered" id="rendered"></div>
        </div>
        <div class="card result-card mobile-result-extra">
          <div class="card-title expandable-title">
            <span id="parsedTitle">Expression</span>
            <span class="card-actions digit-actions">
              <button class="card-action more-digits hidden" id="parsedMore">Show more digits</button>
            </span>
            <span class="card-actions top-card-actions">
              <button class="card-action zoom-action" type="button" data-zoom-step="-1" title="Zoom out">−</button>
              <button class="card-action zoom-action zoom-reset" type="button" data-zoom-reset title="Reset zoom">100%</button>
              <button class="card-action zoom-action" type="button" data-zoom-step="1" title="Zoom in">+</button>
              <button class="card-action expand-card" data-expand-card>Expand</button>
              <button class="card-action copy-result" data-copy-target="expression">Copy</button>
            </span>
          </div>
          <pre id="parsed"></pre>
        </div>
        <div class="card result-card mobile-result-extra">
          <div class="card-title expandable-title">
            <span id="functionTitle">Function</span>
            <span class="card-actions digit-actions">
              <button class="card-action more-digits hidden" id="functionMore">Show more digits</button>
            </span>
            <span class="card-actions top-card-actions">
              <button class="card-action zoom-action" type="button" data-zoom-step="-1" title="Zoom out">−</button>
              <button class="card-action zoom-action zoom-reset" type="button" data-zoom-reset title="Reset zoom">100%</button>
              <button class="card-action zoom-action" type="button" data-zoom-step="1" title="Zoom in">+</button>
              <button class="card-action expand-card" data-expand-card>Expand</button>
              <button class="card-action copy-result" data-copy-target="function">Copy</button>
            </span>
          </div>
          <pre id="functionStyle"></pre>
        </div>
        <div class="card result-card" id="valueCard">
          <div class="card-title value-title">
            <span id="valueTitle">Value</span>
            <span class="card-actions value-card-actions">
              <button class="card-action zoom-action" type="button" data-zoom-step="-1" title="Zoom out">−</button>
              <button class="card-action zoom-action zoom-reset" type="button" data-zoom-reset title="Reset zoom">100%</button>
              <button class="card-action zoom-action" type="button" data-zoom-step="1" title="Zoom in">+</button>
              <button class="card-action expand-card" data-expand-card>Expand</button>
              <button class="card-action copy-result" data-copy-target="value">Copy</button>
            </span>
          </div>
          <pre id="value"></pre>
        </div>
      </div>
      <div class="help-pane hidden" id="helpPane">
        <div class="help-card" data-help-modes="expression,equation,matrix,integrator,datetime">
          <div class="help-kicker">Start Here</div>
          <p>MARS Lab works best when you type the mathematical object itself in the main editor, then use the controls underneath to tell the lab what kind of job you want.</p>
          <ul>
            <li><code>Expression</code>: type a formula, then set variable values in the binding boxes.</li>
            <li><code>Equation</code>: type an equation and choose which variable to solve for.</li>
            <li><code>Matrix</code>: type a matrix expression and pick an operation.</li>
            <li><code>Integrator</code>: type the integrand, then add one row per variable you want to integrate over.</li>
            <li><code>Datetime</code>: choose dates, a year, and a location to calculate calendar and solar facts.</li>
          </ul>
        </div>
        <div class="help-card" data-help-modes="datetime">
          <div class="help-kicker">Datetime Workflow</div>
          <p>Use the calendar controls for the main date, date range, observance year, and location.</p>
          <ul>
            <li><code>Date</code> drives weekday, moon phase, estimated and almanac-backed actual sunrise/sunset, solar declination, inclination, and maximum altitude.</li>
            <li><code>Start date</code> and <code>End date</code> drive the days-between result.</li>
            <li><code>Year</code> drives Christian, Chinese, Hindu, Buddhist, Muslim, Jewish, Cherokee, Mayan, Aztec, and Ethiopian calendar views.</li>
            <li><code>Holiday country or jurisdiction</code> is only used when you want the optional local holiday panel.</li>
            <li><code>GMT offset</code> should include daylight saving. Leave it blank to use the selected jurisdiction's local offset for the selected date.</li>
            <li>Ramadan, Eid al-Fitr, and Muslim New Year use the civil Islamic calendar; Hindu and Buddhist observances are estimated from India-window lunar events, so observed dates can differ locally.</li>
            <li>Weather, humidity, wind, and rain chance appear only when a weather API key is configured and the selected date is supported by the provider.</li>
          </ul>
        </div>
        <div class="help-card" data-help-modes="integrator">
          <div class="help-kicker">Integrator Workflow</div>
          <p>The integrator editor is split into two parts: the expression at the top is the integrand, and the rows underneath describe the nested integrals.</p>
          <ul>
            <li>A <code>Bound</code> row means "integrate with respect to this variable".</li>
            <li>A <code>Free</code> row means "keep this symbol as a parameter".</li>
            <li>Do not add rows for variables that are not in the integrand. A row named <code>y</code> means "integrate with respect to y", not "reserve a possible future variable".</li>
            <li>Row order matters. The first row is the outermost integral shown on screen, and later rows sit further inside.</li>
            <li>Use <code>+</code> and <code>-</code> on each row to insert or remove nested integrals without retyping everything.</li>
          </ul>
        </div>
        <div class="help-card" data-help-modes="integrator">
          <div class="help-kicker">Rows That Count</div>
          <p>The lab sends only active rows to the integrator.</p>
          <ul>
            <li>A row with both bounds filled is active, such as <code>x = 0 .. 1</code>.</li>
            <li>A row with only an upper bound is active, such as <code>t = x</code>, and gives an upper-limit integral.</li>
            <li>A blank <code>Bound</code> row is active only when its variable appears in the integrand, or when it is the only bound row.</li>
            <li>A <code>Free</code> row is active only when its symbol appears in the integrand.</li>
            <li>Spare blank rows that do not match the integrand are ignored and pruned after evaluation.</li>
          </ul>
        </div>
        <div class="help-card" data-help-modes="integrator">
          <div class="help-kicker">Common Recipes</div>
          <ul>
            <li>Antiderivative: integrand <code>sin(x)</code>, row <code>x</code>, both bounds blank.</li>
            <li>Upper-limit form: integrand <code>sin(t)</code>, row <code>t</code>, lower blank, upper <code>x</code>.</li>
            <li>Definite integral: integrand <code>x^2</code>, row <code>x</code>, lower <code>0</code>, upper <code>1</code>.</li>
            <li>Parameterized integral: integrand <code>exp(-a*x^2)</code>, row <code>a</code> as <code>Free</code>, row <code>x</code> with bounds.</li>
            <li>Nested integral: add one bound row per variable, for example <code>x = 0 .. 1</code>, <code>y = 0 .. 1</code>, <code>z = 0 .. 1</code>.</li>
          </ul>
        </div>
        <div class="help-card" data-help-modes="integrator">
          <div class="help-kicker">Reading Results</div>
          <ul>
            <li><code>Rendered TeX</code> shows the integral exactly as the lab interpreted it.</li>
            <li><code>Exact result</code> explains the path taken, for example symbolic reduction, convergence, or a work-budget cap.</li>
            <li><code>Integral</code> shows the numeric value and the reported integration error estimate.</li>
            <li><code>work used</code> tells you how much of the numeric budget the solver spent. A value of <code>1</code> often means a fast closed-form path or a very simple special case.</li>
          </ul>
        </div>
        <div class="help-card" data-help-modes="expression,equation,matrix,integrator">
          <h3>Expression Shape</h3>
          <p>Type the expression body in the editor. Variable and constant bindings appear as editable boxes underneath.</p>
          <ul>
            <li><code>x/pi</code> becomes <code>{ x/π | x = ? }</code>, but the editor keeps showing <code>x/pi</code>.</li>
            <li>Set <code>x</code> to <code>pi/2</code> in the binding box instead of writing the binding into the editor.</li>
            <li>Raw expr binding syntax still works when you paste it, but the lab keeps the large editor focused on the expression body.</li>
          </ul>
        </div>
        <div class="help-card" data-help-modes="expression,equation,matrix,integrator">
          <h3>Bindings</h3>
          <p>Yellow boxes are variables and blue boxes are constants.</p>
          <ul>
            <li><code>x = ?</code> means unknown / <code>NAN</code>, so derivative buttons appear for <code>x</code>.</li>
            <li><code>; H = 163</code> marks <code>H</code> as a constant, so no derivative button is made for it.</li>
            <li>Binding values can use arithmetic: <code>3/2*pi</code>, <code>(pi^2)/2</code>, <code>3/2+pi/7+3*i/5</code>.</li>
          </ul>
        </div>
        <div class="help-card" data-help-modes="expression">
          <h3>Goal Seek</h3>
          <p>Goal seek changes variable bindings so the expression value reaches the value in the <code>Target</code> field.</p>
          <ul>
            <li>Leave a variable binding box blank to mark it as unknown.</li>
            <li>Constants are not changed by goal seek.</li>
            <li>If there is one variable, goal seek solves that variable directly.</li>
            <li>If there are several variables, goal seek moves them together by the smallest local step it can find.</li>
            <li>Current variable binding values become the starting point automatically.</li>
            <li>Use the binding boxes in the main editor to seed a particular crossing or branch, for example target <code>27</code> with expression <code>{ x^x | x = 3 }</code>.</li>
            <li>For several variables, set whichever variable bindings you want to seed. Blank or unknown bindings still fall back to the solver's defaults.</li>
            <li>It only reports success when <code>abs(value - target)</code> is within the current working precision.</li>
          </ul>
        </div>
        <div class="help-card" data-help-modes="expression,equation,matrix,integrator">
          <h3>Constants And Functions</h3>
          <ul>
            <li>Built-in constants include <code>pi</code>/<code>π</code>, <code>e</code>, <code>i</code>, <code>phi</code>/<code>φ</code>, and <code>gamma</code>/<code>γ</code>.</li>
            <li><code>ln(x)</code> is natural log; <code>log(x)</code>, <code>lg(x)</code>, and <code>log10(x)</code> are base-10 log.</li>
            <li>Versine/haversine family names include <code>versin</code>, <code>coversin</code>, <code>haversin</code>, <code>hacoversin</code>, and their <code>arc...</code>/<code>arch...</code> inverses.</li>
            <li><code>W(x)</code>, <code>W0(x)</code>, and <code>W_0(x)</code> mean <code>W₀(x)</code>. Use <code>W-1(x)</code> for <code>W₋₁(x)</code>.</li>
            <li>Standard gamma notation is supported in display: <code>gamma(x)</code> shows as <code>Γ(x)</code>, and polygamma shows as <code>ψ⁽ⁿ⁾(x)</code>.</li>
          </ul>
        </div>
        <div class="help-card" data-help-modes="expression,equation,integrator">
          <h3>Unary Functions</h3>
          <ul>
            <li>Elementary: <code>abs(x)</code>, <code>floor(x)</code>, <code>ceil(x)</code>, <code>sqrt(x)</code>, <code>exp(x)</code>, <code>ln(x)</code>, <code>log(x)</code>, <code>lg(x)</code>, <code>log10(x)</code>.</li>
            <li>Trigonometric: <code>sin(x)</code>, <code>cos(x)</code>, <code>tan(x)</code>, <code>asin(x)</code>, <code>acos(x)</code>, <code>atan(x)</code>.</li>
            <li>Versine/haversine: <code>versin(x)</code>, <code>vercos(x)</code>, <code>coversin(x)</code>, <code>covercos(x)</code>, <code>haversin(x)</code>, <code>havercos(x)</code>, <code>hacoversin(x)</code>, <code>hacovercos(x)</code>.</li>
            <li>Versine/haversine inverses: <code>arcversin(x)</code>, <code>arcvercos(x)</code>, <code>arccoversin(x)</code>, <code>arccovercos(x)</code>, <code>archaversin(x)</code>, <code>archavercos(x)</code>, <code>archacoversin(x)</code>, <code>archacovercos(x)</code>.</li>
            <li>Hyperbolic: <code>sinh(x)</code>, <code>cosh(x)</code>, <code>tanh(x)</code>, <code>asinh(x)</code>, <code>acosh(x)</code>, <code>atanh(x)</code>.</li>
            <li>Gamma family: <code>gamma(x)</code>, <code>gammainv(x)</code>, <code>lgamma(x)</code>, <code>digamma(x)</code>, <code>trigamma(x)</code>, <code>polygamma(n, x)</code>.</li>
            <li>Error functions: <code>erf(x)</code>, <code>erfc(x)</code>, <code>erfinv(x)</code>, <code>erfcinv(x)</code>.</li>
            <li>Exponential integrals: <code>Ei(x)</code>, <code>E1(x)</code>.</li>
          </ul>
        </div>
        <div class="help-card" data-help-modes="expression,equation,integrator">
          <h3>Lambert W And Normal Functions</h3>
          <ul>
            <li>Principal Lambert W: <code>W(x)</code>, <code>W0(x)</code>, <code>W_0(x)</code>, <code>W₀(x)</code>, <code>productlog(x)</code>, <code>lambert_w0(x)</code>.</li>
            <li>Minus-one Lambert W branch: <code>W-1(x)</code>, <code>W_-1(x)</code>, <code>W₋₁(x)</code>, <code>lambert_wm1(x)</code>.</li>
            <li>Normal distribution: <code>normal_pdf(x)</code>, <code>normal_cdf(x)</code>, <code>normal_logpdf(x)</code>.</li>
          </ul>
        </div>
        <div class="help-card" data-help-modes="expression,equation,integrator">
          <h3>Binary Functions</h3>
          <ul>
            <li><code>pow(x, y)</code> is equivalent to <code>x^y</code>.</li>
            <li><code>atan2(y, x)</code>, <code>hypot(x, y)</code>.</li>
            <li><code>beta(x, y)</code>, <code>logbeta(x, y)</code>, <code>binomial(n, k)</code>.</li>
            <li>Incomplete gamma: <code>gammainc_lower(s, x)</code>, <code>gammainc_upper(s, x)</code>, <code>gammainc_P(s, x)</code>, <code>gammainc_Q(s, x)</code>.</li>
            <li>Exact integer helpers: <code>gcd(a, b)</code>, <code>lcm(a, b)</code>, <code>mod(a, b)</code>, <code>modinv(a, b)</code>.</li>
            <li>Bitwise helpers use function syntax: <code>AND(a, b)</code>, <code>OR(a, b)</code>, <code>XOR(a, b)</code>, <code>NOT(a)</code>, <code>SHL(a, n)</code>, <code>SHR(a, n)</code>.</li>
          </ul>
        </div>
        <div class="help-card" data-help-modes="expression">
          <h3>Value-Only Helpers</h3>
          <ul>
            <li>Discrete functions evaluate normally but are not differentiable, so derivative buttons are hidden when they control the expression.</li>
            <li>Available helpers include <code>factorial(n)</code>, <code>n!</code>, <code>fibonacci(n)</code>, <code>partition(n)</code>, <code>isqrt(n)</code>, <code>is_prime(n)</code>, <code>next_prime(n)</code>, and <code>prev_prime(n)</code>.</li>
            <li><code>factors(n)</code> returns the original value and shows its factorisation as constant bindings, for example <code>factors(360)</code> becomes <code>{ a₀³·a₁²·a₂ |; a₀ = 2, a₁ = 3, a₂ = 5 }</code>.</li>
          </ul>
        </div>
        <div class="help-card" data-help-modes="expression,equation,integrator">
          <h3>Distribution Functions</h3>
          <ul>
            <li>Normal distribution: <code>normal_pdf(x)</code>, <code>normal_cdf(x)</code>, <code>normal_logpdf(x)</code>.</li>
            <li>Beta distribution: <code>beta_pdf(x, a, b)</code>, <code>logbeta_pdf(x, a, b)</code>.</li>
          </ul>
        </div>
        <div class="help-card" data-help-modes="expression,equation,matrix,integrator">
          <h3>Shortcuts</h3>
          <ul>
            <li><code>Ctrl+Enter</code> evaluates the expression.</li>
            <li>Derivative buttons appear from the current variable bindings.</li>
            <li>Enter the goal-seek target in the left pane's <code>Target</code> field.</li>
            <li>Use the binding boxes in the editor when goal seek needs better initial guesses.</li>
            <li><code>Goal seek</code> changes all variable bindings together to move the value towards that target.</li>
            <li>More/Less precision changes the displayed value precision without changing the expression.</li>
          </ul>
        </div>
      </div>
    </section>
  </main>

  <script>
    const expr = document.getElementById('expr');
    const subtitle = document.getElementById('subtitle');
    const leftPaneTitle = document.getElementById('leftPaneTitle');
    const modeTabs = Array.from(document.querySelectorAll('.mode-tab'));
    const matrixControls = document.getElementById('matrixControls');
    const matrixOperation = document.getElementById('matrixOperation');
    const matrixOperand = document.getElementById('matrixOperand');
    const matrixOperandLabel = document.getElementById('matrixOperandLabel');
    const equationControls = document.getElementById('equationControls');
    const equationVariable = null;
    const integratorControls = document.getElementById('integratorControls');
    const integratorBoundStack = document.getElementById('integratorBoundStack');
    const integratorAddBound = document.getElementById('integratorAddBound');
    const integratorIntervalCap = document.getElementById('integratorIntervalCap');
    const datetimeControls = document.getElementById('datetimeControls');
    const datetimeDate = document.getElementById('datetimeDate');
    const datetimeJdn = document.getElementById('datetimeJdn');
    const datetimeStart = document.getElementById('datetimeStart');
    const datetimeEnd = document.getElementById('datetimeEnd');
    const datetimeYear = document.getElementById('datetimeYear');
    const datetimeJurisdiction = document.getElementById('datetimeJurisdiction');
    const datetimeLatitude = document.getElementById('datetimeLatitude');
    const datetimeLongitude = document.getElementById('datetimeLongitude');
    const datetimeElevation = document.getElementById('datetimeElevation');
    const datetimeGmtOffset = document.getElementById('datetimeGmtOffset');
    const datetimeLocal = document.getElementById('datetimeLocal');
    const datetimeLocalBody = document.getElementById('datetimeLocalBody');
    const almanacControls = document.getElementById('almanacControls');
    const almanacDate = document.getElementById('almanacDate');
    const almanacTime = document.getElementById('almanacTime');
    const almanacZone = document.getElementById('almanacZone');
    const almanacLatitude = document.getElementById('almanacLatitude');
    const almanacLongitude = document.getElementById('almanacLongitude');
    const almanacBody = document.getElementById('almanacBody');
    const marsDatePicker = document.getElementById('marsDatePicker');
    const marsDatePickerTitle = document.getElementById('marsDatePickerTitle');
    const marsDatePickerWeekdays = document.getElementById('marsDatePickerWeekdays');
    const marsDatePickerGrid = document.getElementById('marsDatePickerGrid');
    const marsDatePickerPrev = document.getElementById('marsDatePickerPrev');
    const marsDatePickerNext = document.getElementById('marsDatePickerNext');
    const marsDatePickerToday = document.getElementById('marsDatePickerToday');
    const marsDatePickerClose = document.getElementById('marsDatePickerClose');
    const marsDateButtons = Array.from(document.querySelectorAll('[data-date-target]'));
    const helpCards = Array.from(document.querySelectorAll('#helpPane .help-card'));
    const run = document.getElementById('run');
    const back = document.getElementById('back');
    const forward = document.getElementById('forward');
    const help = document.getElementById('help');
    const goalSeek = document.getElementById('goalSeek');
    const clear = document.getElementById('clear');
    const targetRow = document.getElementById('targetRow');
    const goalTarget = document.getElementById('goalTarget');
    const lessPrecision = document.getElementById('lessPrecision');
    const morePrecision = document.getElementById('morePrecision');
    const derivativeButtons = document.getElementById('derivativeButtons');
    const variableValues = document.getElementById('variableValues');
    const mobileAccess = document.getElementById('mobileAccess');
    const mobileTitle = document.getElementById('mobileTitle');
    const mobileHint = document.getElementById('mobileHint');
    const mobileUrl = document.getElementById('mobileUrl');
    const mobileQr = document.getElementById('mobileQr');
    const controlToken = __CONTROL_TOKEN__;
    enhanceRoundedSelect(matrixOperation);
    enhanceRoundedSelect(datetimeJurisdiction, {
      searchable: true,
      searchPlaceholder: 'Search jurisdictions',
      emptyText: 'No matching jurisdiction'
    });
    enhanceRoundedSelect(almanacBody);
    let datetimeLocalRefreshSequence = 0;
    let datetimeEvaluationSequence = 0;
    let almanacEvaluationSequence = 0;
    const statusEl = document.getElementById('status');
    const inputCopy = document.getElementById('inputCopy');
    const rightPaneTitle = document.getElementById('rightPaneTitle');
    const resultUseInput = document.getElementById('resultUseInput');
    const resultPane = document.getElementById('resultPane');
    const helpPane = document.getElementById('helpPane');
    const rendered = document.getElementById('rendered');
    const renderedTitle = document.getElementById('renderedTitle');
    const renderedCopy = document.getElementById('renderedCopy');
    const renderedMore = document.getElementById('renderedMore');
    const parsed = document.getElementById('parsed');
    const parsedTitle = document.getElementById('parsedTitle');
    const parsedMore = document.getElementById('parsedMore');
    const functionStyle = document.getElementById('functionStyle');
    const functionTitle = document.getElementById('functionTitle');
    const functionMore = document.getElementById('functionMore');
    const valueCard = document.getElementById('valueCard');
    const value = document.getElementById('value');
    const valueTitle = document.getElementById('valueTitle');
    const copyButtons = Array.from(document.querySelectorAll('.copy-result'));
    const moreDigitButtons = Array.from(document.querySelectorAll('.more-digits'));
    const resultCards = Array.from(document.querySelectorAll('.result-card'));
    const expandCardButtons = Array.from(document.querySelectorAll('[data-expand-card]'));
    const zoomButtons = Array.from(document.querySelectorAll('[data-zoom-step], [data-zoom-reset]'));
    const RESULT_ZOOM_LEVELS = [0.5, 0.67, 0.8, 1, 1.25, 1.5, 2, 3, 4, 6, 8];
    const RESULT_ZOOM_DEFAULT_INDEX = 3;
    let lastTex = '';
    let lastDerivativeExpression = '';
    let currentVariables = [];
    let currentBindingKinds = new Map();
    let currentDifferentiable = true;
    function createEmptyModeHistory() {
      return {
        expression: [],
        equation: [],
        matrix: [],
        integrator: [],
        datetime: [],
        almanac: []
      };
    }

    let expressionHistory = createEmptyModeHistory();
    let forwardHistory = createEmptyModeHistory();
    const modeCommittedState = {
      expression: null,
      equation: null,
      matrix: null,
      integrator: null,
      datetime: null,
      almanac: null
    };
    let workingPrecisionBits = 256;
    let fullExpressionText = '';
    let displayedExpressionText = '';

    if (controlToken && window.location.search.includes('__CONTROL_QUERY_PREFIX__')) {
      window.history.replaceState(null, '', window.location.pathname + window.location.hash);
    }
    let lastEvaluationInputText = '';
    let bindingValueCache = new Map();
    const DOUBLE_PRECISION_BITS = 53;
    const DOUBLE_PRECISION_DIGITS = 17;
    const QFLOAT_PRECISION_BITS = 106;
    const MAX_PRECISION_BITS = 1048576;
    const MODE_DEFAULT_PRECISION_BITS = {
      expression: 256,
      equation: 256,
      matrix: 256,
      integrator: DOUBLE_PRECISION_BITS,
      datetime: DOUBLE_PRECISION_BITS,
      almanac: DOUBLE_PRECISION_BITS
    };
    const modePrecisionBits = {
      expression: MODE_DEFAULT_PRECISION_BITS.expression,
      equation: MODE_DEFAULT_PRECISION_BITS.equation,
      matrix: MODE_DEFAULT_PRECISION_BITS.matrix,
      integrator: MODE_DEFAULT_PRECISION_BITS.integrator,
      datetime: MODE_DEFAULT_PRECISION_BITS.datetime,
      almanac: MODE_DEFAULT_PRECISION_BITS.almanac
    };
    const START_FORBIDDEN_PATTERN = /[=,;|{}]/;
    const COMPACT_BINDING_VALUE_LIMIT = 20;
    const COMPACT_BINDING_VALUE_KEEP = 16;
    const DEFAULT_EXPRESSION_TEXT = __DEFAULT_EXPRESSION__;
    const DEFAULT_EQUATION_TEXT = __DEFAULT_EQUATION__;
    const DEFAULT_EQUATION_VARIABLE_TEXT = __DEFAULT_EQUATION_VARIABLE__;
    const DEFAULT_MATRIX_TEXT = __DEFAULT_MATRIX__;
    const DEFAULT_INTEGRATOR_TEXT = __DEFAULT_INTEGRATOR__;
    const DEFAULT_INTEGRATOR_BOUNDS_TEXT = __DEFAULT_INTEGRATOR_BOUNDS__;
    const DEFAULT_INTEGRATOR_INTERVAL_CAP = __DEFAULT_INTEGRATOR_INTERVAL_CAP__;
    const DEFAULT_DATETIME_TEXT = __DEFAULT_DATETIME_TEXT__;
    const DEFAULT_DATETIME_DATE = __DEFAULT_DATETIME_DATE__;
    const DEFAULT_DATETIME_JURISDICTION = __DEFAULT_DATETIME_JURISDICTION__;
    const DEFAULT_DATETIME_LATITUDE = __DEFAULT_DATETIME_LATITUDE__;
    const DEFAULT_DATETIME_LONGITUDE = __DEFAULT_DATETIME_LONGITUDE__;
    const DEFAULT_DATETIME_ELEVATION = __DEFAULT_DATETIME_ELEVATION__;
    const DEFAULT_DATETIME_GMT_OFFSET = __DEFAULT_DATETIME_GMT_OFFSET__;
    const DEFAULT_ALMANAC_TEXT = __DEFAULT_ALMANAC_TEXT__;
    const DEFAULT_ALMANAC_DATE = __DEFAULT_ALMANAC_DATE__;
    const DEFAULT_ALMANAC_TIME = __DEFAULT_ALMANAC_TIME__;
    const DEFAULT_ALMANAC_ZONE = __DEFAULT_ALMANAC_ZONE__;
    const DEFAULT_ALMANAC_LATITUDE = __DEFAULT_ALMANAC_LATITUDE__;
    const DEFAULT_ALMANAC_LONGITUDE = __DEFAULT_ALMANAC_LONGITUDE__;
    const DEFAULT_ALMANAC_BODY = __DEFAULT_ALMANAC_BODY__;
    if (datetimeJurisdiction && !datetimeJurisdiction.value)
      datetimeJurisdiction.value = DEFAULT_DATETIME_JURISDICTION;
    let datetimeAutoGmtOffset = String(datetimeGmtOffset && datetimeGmtOffset.value || DEFAULT_DATETIME_GMT_OFFSET);
    let datetimeGmtOffsetTouched = false;
    const HOLIDAY_JURISDICTION_SET = new Set(__HOLIDAY_JURISDICTION_CODES__);
    const LAB_MODE_STORAGE_KEY = 'mars.exprLab.lastMode';
    let currentLabMode = 'expression';
    const modeEditorText = {
      expression: DEFAULT_EXPRESSION_TEXT,
      equation: DEFAULT_EQUATION_TEXT,
      matrix: DEFAULT_MATRIX_TEXT,
      integrator: DEFAULT_INTEGRATOR_TEXT,
      datetime: DEFAULT_DATETIME_TEXT,
      almanac: DEFAULT_ALMANAC_TEXT
    };
    const modeResultState = {
      expression: null,
      equation: null,
      matrix: null,
      integrator: null,
      datetime: null,
      almanac: null
    };

    marsDateButtons.forEach((button) => {
      button.addEventListener('click', () => {
        const targetId = String(button.dataset.dateTarget || '').trim();
        const input = targetId ? document.getElementById(targetId) : null;
        if (!input)
          return;
        if (marsDatePickerState.input === input && marsDatePicker && !marsDatePicker.classList.contains('hidden')) {
          closeMarsDatePicker({restoreFocus: true});
          return;
        }
        openMarsDatePicker(input, button);
      });
    });

    if (datetimeGmtOffset) {
      datetimeGmtOffset.addEventListener('input', () => {
        datetimeGmtOffsetTouched = true;
      });
    }

    if (marsDatePickerPrev)
      marsDatePickerPrev.addEventListener('click', () => shiftMarsDatePickerMonth(-1));
    if (marsDatePickerNext)
      marsDatePickerNext.addEventListener('click', () => shiftMarsDatePickerMonth(1));
    if (marsDatePickerToday)
      marsDatePickerToday.addEventListener('click', () => {
        if (!marsDatePickerState.input)
          return;
        commitMarsDateValue(marsDatePickerState.input, marsTodayIsoDate());
        closeMarsDatePicker({restoreFocus: true});
      });
    if (marsDatePickerClose)
      marsDatePickerClose.addEventListener('click', () => closeMarsDatePicker({restoreFocus: true}));

    document.addEventListener('click', (event) => {
      if (!marsDatePicker || marsDatePicker.classList.contains('hidden'))
        return;
      if (marsDatePicker.contains(event.target))
        return;
      if (event.target.closest && event.target.closest('.mars-date-shell'))
        return;
      closeMarsDatePicker();
    });

    window.addEventListener('resize', () => {
      if (marsDatePicker && !marsDatePicker.classList.contains('hidden'))
        placeMarsDatePicker(marsDatePickerState.shell);
    });

    function precisionDigitsForBits(bits) {
      if (bits <= DOUBLE_PRECISION_BITS)
        return DOUBLE_PRECISION_DIGITS;
      return Math.ceil(bits * Math.LOG10E * Math.LN2);
    }

    function requestedPrecisionBits() {
      const mode = currentMode();
      const bits = modePrecisionBits[mode] ?? workingPrecisionBits;
      return Math.max(DOUBLE_PRECISION_BITS, Math.min(MAX_PRECISION_BITS, bits));
    }

    function precisionStatusText() {
      const bits = requestedPrecisionBits();
      const digits = requestedValuePrecision();
      return `${digits} digits / ${bits} bits`;
    }

    function setStatus(text) {
      statusEl.textContent = `${text} · ${precisionStatusText()}`;
    }

    function currentMode() {
      return currentLabMode;
    }

    function syncModeTabs() {
      modeTabs.forEach((tab) => {
        const active = tab.dataset.mode === currentLabMode;
        tab.classList.toggle('active', active);
        tab.setAttribute('aria-selected', active ? 'true' : 'false');
        tab.tabIndex = active ? 0 : -1;
      });
    }

    function setMode(mode, options = {}) {
      const nextMode = mode === 'equation' || mode === 'matrix' || mode === 'integrator' || mode === 'datetime' || mode === 'almanac'
        ? mode
        : 'expression';
      const changed = nextMode !== currentLabMode;
      currentLabMode = nextMode;
      workingPrecisionBits = modePrecisionBits[currentLabMode] || workingPrecisionBits;
      syncModeTabs();
      if (!changed && !options.force)
        return false;
      return true;
    }

    function captureCurrentModeEditor() {
      commitVisibleBindingInputs();
      const mode = currentMode();
      if (mode === 'expression')
        modeEditorText.expression = currentExpressionText() || expr.value.trim() || modeEditorText.expression;
      else if (mode === 'equation') {
        modeEditorText.equation = currentExpressionText() || expr.value.trim() || modeEditorText.equation;
        saveLastEquationState();
      }
      else if (mode === 'matrix') {
        modeEditorText.matrix = currentExpressionText() || expr.value.trim() || modeEditorText.matrix;
        saveLastMatrixState();
      } else if (mode === 'integrator') {
        modeEditorText.integrator = currentExpressionText() || expr.value.trim() || modeEditorText.integrator;
        saveLastIntegratorState();
      } else if (mode === 'datetime') {
        modeEditorText.datetime = DEFAULT_DATETIME_TEXT;
        saveLastDatetimeState();
      } else {
        modeEditorText.almanac = DEFAULT_ALMANAC_TEXT;
        saveLastAlmanacState();
      }
    }

    function restoreModeEditor(mode) {
      if (mode === 'expression') {
        setExpressionEditor(modeEditorText.expression || DEFAULT_EXPRESSION_TEXT);
      } else if (mode === 'equation') {
        setExpressionEditor(modeEditorText.equation || DEFAULT_EQUATION_TEXT);
      } else if (mode === 'matrix') {
        const text = modeEditorText.matrix || DEFAULT_MATRIX_TEXT;
        if (bindingParts(text))
          setExpressionEditor(text);
        else {
          expr.value = text;
          clearExpressionSource();
          clearVariableValues();
        }
      } else {
        if (mode === 'datetime') {
          expr.value = DEFAULT_DATETIME_TEXT;
          clearExpressionSource();
          clearVariableValues();
          return;
        }
        if (mode === 'almanac') {
          expr.value = DEFAULT_ALMANAC_TEXT;
          clearExpressionSource();
          clearVariableValues();
          return;
        }
        const text = modeEditorText.integrator || DEFAULT_INTEGRATOR_TEXT;
        if (bindingParts(text))
          setExpressionEditor(text);
        else {
          expr.value = text;
          clearExpressionSource();
          clearVariableValues();
        }
      }
    }

    function setResultTitles(renderedText, parsedText, functionText, valueText) {
      renderedTitle.textContent = renderedText;
      parsedTitle.textContent = parsedText;
      functionTitle.textContent = functionText;
      valueTitle.textContent = valueText;
    }

    function setValueCardVisible(visible) {
      if (!valueCard)
        return;
      if (!visible && valueCard.classList.contains('expanded-card'))
        collapseResultCards();
      valueCard.classList.toggle('hidden', !visible);
    }

    function snapshotElementState(element) {
      return {
        className: element.className,
        style: element.style.cssText,
        innerHTML: element.innerHTML,
        dataset: {...element.dataset}
      };
    }

    function restoreElementState(element, state) {
      element.className = state.className || '';
      element.style.cssText = state.style || '';
      element.innerHTML = state.innerHTML || '';
      Object.keys(element.dataset).forEach((key) => {
        delete element.dataset[key];
      });
      Object.entries(state.dataset || {}).forEach(([key, value]) => {
        element.dataset[key] = value;
      });
    }

    function snapshotButtonState(button) {
      return {
        className: button.className,
        textContent: button.textContent,
        disabled: !!button.disabled,
        dataset: {...button.dataset}
      };
    }

    function restoreButtonState(button, state) {
      button.className = state.className || '';
      button.textContent = state.textContent || '';
      button.disabled = !!state.disabled;
      Object.keys(button.dataset).forEach((key) => {
        delete button.dataset[key];
      });
      Object.entries(state.dataset || {}).forEach(([key, value]) => {
        button.dataset[key] = value;
      });
    }

    function hasResultContent() {
      return Boolean(
        rendered.innerHTML.trim() ||
        parsed.textContent.trim() ||
        functionStyle.textContent.trim() ||
        value.textContent.trim()
      );
    }

    function saveCurrentModeResultState(mode = currentMode()) {
      if (!hasResultContent()) {
        modeResultState[mode] = null;
        return;
      }

      modeResultState[mode] = {
        rendered: snapshotElementState(rendered),
        parsed: snapshotElementState(parsed),
        functionStyle: snapshotElementState(functionStyle),
        value: snapshotElementState(value),
        renderedMore: snapshotButtonState(renderedMore),
        parsedMore: snapshotButtonState(parsedMore),
        functionMore: snapshotButtonState(functionMore),
        resultInputText: resultUseInput.dataset.inputText || '',
        lastTex,
        lastDerivativeExpression,
        currentVariables: [...currentVariables],
        currentDifferentiable
      };
    }

    function restoreModeResultState(mode = currentMode()) {
      const state = modeResultState[mode];
      if (!state) {
        clearResultPane();
        return;
      }

      collapseResultCards();
      restoreElementState(rendered, state.rendered);
      restoreElementState(parsed, state.parsed);
      restoreElementState(functionStyle, state.functionStyle);
      restoreElementState(value, state.value);
      restoreButtonState(renderedMore, state.renderedMore);
      restoreButtonState(parsedMore, state.parsedMore);
      restoreButtonState(functionMore, state.functionMore);
      setResultInputText(state.resultInputText || '');
      lastTex = state.lastTex || '';
      lastDerivativeExpression = state.lastDerivativeExpression || '';
      currentVariables = Array.isArray(state.currentVariables) ? [...state.currentVariables] : [];
      currentDifferentiable = state.currentDifferentiable !== false;
      renderDerivativeButtons(currentVariables);
    }

    function syncMatrixControls() {
      syncRoundedSelect(matrixOperation);
      const needsOperand = currentMode() === 'matrix' && matrixOperation.value === 'solve';
      matrixOperand.classList.toggle('hidden', !needsOperand);
      matrixOperandLabel.classList.toggle('hidden', !needsOperand);
    }

    function syncModeUI() {
      const mode = currentMode();
      const expressionMode = mode === 'expression';
      const equationMode = mode === 'equation';
      const matrixMode = mode === 'matrix';
      const integratorMode = mode === 'integrator';
      const datetimeMode = mode === 'datetime';
      const almanacMode = mode === 'almanac';

      document.body.classList.toggle('datetime-mode', datetimeMode);
      document.body.classList.toggle('almanac-mode', almanacMode);
      matrixControls.classList.toggle('hidden', !matrixMode);
      equationControls.classList.toggle('hidden', !equationMode);
      integratorControls.classList.toggle('hidden', !integratorMode);
      datetimeControls.classList.toggle('hidden', !datetimeMode);
      almanacControls.classList.toggle('hidden', !almanacMode);
      if (datetimeLocal)
        datetimeLocal.classList.toggle('hidden', !datetimeMode || !String(datetimeLocalBody?.textContent || '').trim());
      targetRow.classList.toggle('hidden', !expressionMode || targetRow.classList.contains('hidden'));
      derivativeButtons.classList.toggle('hidden', !expressionMode);
      goalSeek.classList.toggle('hidden', !expressionMode);

      if (expressionMode) {
        leftPaneTitle.textContent = 'Expression';
        subtitle.textContent = 'Switch between expression, equation, matrix, and integrator experiments. Each mode runs through a local MARS scratch binary and shows the result on the right.';
        setResultTitles('Rendered TeX', 'Expression', 'Function', 'Value');
        setValueCardVisible(true);
      } else if (equationMode) {
        leftPaneTitle.textContent = 'Equation';
        subtitle.textContent = 'Enter an equation on the left. The lab tries symbolic isolation first, then numeric solving for all variable bindings.';
        setResultTitles('Rendered TeX', 'Equation', 'Solutions', 'Details');
        setValueCardVisible(true);
      } else if (matrixMode) {
        leftPaneTitle.textContent = 'Matrix';
        subtitle.textContent = 'Enter a matrix expression on the left, choose an operation, and inspect both the formatted result and the raw matrix output.';
        setResultTitles('Rendered TeX', 'Result', 'Layout', 'Summary');
        setValueCardVisible(false);
      } else if (integratorMode) {
        leftPaneTitle.textContent = 'Integrator';
        subtitle.textContent = 'Enter an integrand expression on the left, stack one or more integral rows, and use Free when a symbol should stay as a parameter. Leave both bounds blank for an antiderivative, or leave lower blank and fill upper to evaluate it there.';
        setResultTitles('Rendered TeX', 'Integrand', 'Exact result', 'Integral');
        setValueCardVisible(true);
      } else if (almanacMode) {
        leftPaneTitle.textContent = 'Almanac';
        subtitle.textContent = 'Enter date and time in GMT, then zone, latitude, and longitude. This starts out like AstroNav 2000-2040 while using the live almanac engine underneath.';
        setResultTitles('Worksheet', 'Selected Body', 'All Bodies', 'Notes');
        setValueCardVisible(true);
      } else {
        leftPaneTitle.textContent = 'Datetime';
        subtitle.textContent = 'Choose dates, a year, and a location. MARS datetime calculates calendar observances, moon phase, solar times, and optional local weather, with jurisdiction holidays added when available.';
        setResultTitles('Overview', 'Date Range', 'Calendar', 'Solar And Moon');
        setValueCardVisible(true);
      }

      syncMatrixControls();
      syncHelpCards();
      updateHistoryButtons();
    }

    function syncHelpCards() {
      const mode = currentMode();

      helpCards.forEach((card) => {
        const modes = String(card.dataset.helpModes || '')
          .split(',')
          .map((item) => item.trim())
          .filter(Boolean);
        const visible = !modes.length || modes.includes(mode);
        card.classList.toggle('hidden', !visible);
      });
    }

    function applyLabMode(mode) {
      setMode(validLabMode(mode), {force: true});
      restoreModeEditor(currentMode());
      if (currentMode() === 'integrator')
        renderIntegratorRows(activeIntegratorRows());
      if (currentMode() === 'datetime') {
        restoreDatetimeDefaultsIfBlank();
        refreshDatetimeJurisdictionLocation().then(() => {
          if (currentMode() === 'datetime')
            saveLastDatetimeState();
        });
      }
      if (currentMode() === 'almanac') {
        restoreAlmanacDefaultsIfBlank();
        saveLastAlmanacState();
      }
      syncModeUI();
      if (currentMode() === 'integrator' && currentIntegratorBoundRows().length === 0)
        resetIntegratorBoundsToDefault();
      if (currentMode() === 'integrator' && integratorIntervalCap)
        integratorIntervalCap.value = String(validIntegratorIntervalCap(integratorIntervalCap.value));
    }

    function showResults() {
      resultPane.classList.remove('hidden');
      helpPane.classList.add('hidden');
      rightPaneTitle.textContent = 'Result';
      resultUseInput.classList.toggle('hidden', !resultUseInput.dataset.inputText);
      help.textContent = 'Help';
    }

    function showHelp() {
      resultPane.classList.add('hidden');
      helpPane.classList.remove('hidden');
      rightPaneTitle.textContent = 'Help';
      resultUseInput.classList.add('hidden');
      help.textContent = 'Result';
      setStatus('Help');
    }

    function toggleHelp() {
      if (helpPane.classList.contains('hidden'))
        showHelp();
      else {
        showResults();
        setStatus('Ready');
      }
    }

    function variableNamesFromBindings(bindings) {
      return (Array.isArray(bindings) ? bindings : [])
        .filter((binding) => String(binding.kind || 'variable') !== 'constant')
        .map((binding) => String(binding.name || '').trim())
        .filter(Boolean);
    }

    function visibleBindingsForCurrentMode(bindings) {
      if (currentMode() !== 'integrator')
        return Array.isArray(bindings) ? bindings : [];
      return integratorEditableBindings(bindings);
    }

    function showTargetEntry() {
      targetRow.classList.remove('hidden');
      goalSeek.textContent = 'Run goal seek';
      goalTarget.focus();
      goalTarget.select();
      setStatus('Enter target');
    }

    function hideTargetEntry() {
      targetRow.classList.add('hidden');
      goalSeek.textContent = 'Goal seek';
    }

    function syncRoundedSelect(select) {
      if (select && typeof select.__marsSyncRoundedSelect === 'function')
        select.__marsSyncRoundedSelect();
    }

    function setSelectValue(select, value) {
      if (!select)
        return;
      select.value = value;
      syncRoundedSelect(select);
    }

    function enhanceRoundedSelect(select, options = {}) {
      if (!select)
        return null;

      const label = document.querySelector(`label[for="${select.id}"]`);
      const shell = document.createElement('div');
      const button = document.createElement('button');
      const menu = document.createElement('div');
      const searchable = options && options.searchable;
      const searchInput = searchable ? document.createElement('input') : null;
      const optionsWrap = document.createElement('div');
      const emptyState = document.createElement('div');

      shell.className = 'select-shell';
      button.type = 'button';
      button.className = 'select-button';
      button.setAttribute('aria-haspopup', 'listbox');
      button.setAttribute('aria-expanded', 'false');
      if (label && label.id)
        button.setAttribute('aria-labelledby', label.id);
      else
        button.setAttribute('aria-label', 'Select option');

      menu.className = 'select-menu hidden';
      menu.setAttribute('role', 'listbox');
      optionsWrap.className = 'select-options';
      emptyState.className = 'select-empty';
      emptyState.textContent = (options && options.emptyText) || 'No matches';

      if (searchInput) {
        searchInput.type = 'search';
        searchInput.className = 'select-search';
        searchInput.placeholder = (options && options.searchPlaceholder) || 'Search';
        searchInput.setAttribute('aria-label', searchInput.placeholder);
        searchInput.autocomplete = 'off';
        searchInput.spellcheck = false;
        menu.appendChild(searchInput);
      }

      select.classList.add('select-native-source');
      select.tabIndex = -1;
      select.setAttribute('aria-hidden', 'true');
      select.parentNode.insertBefore(shell, select);
      shell.appendChild(select);
      shell.appendChild(button);
      shell.appendChild(menu);
      menu.appendChild(optionsWrap);
      menu.appendChild(emptyState);

      const optionButtons = Array.from(select.options).map((option) => {
        const item = document.createElement('button');
        item.type = 'button';
        item.className = 'select-option';
        item.setAttribute('role', 'option');
        item.dataset.value = option.value;
        item.textContent = option.textContent;
        item.addEventListener('click', () => {
          const changed = select.value !== option.value;
          select.value = option.value;
          sync();
          close();
          button.focus();
          if (changed)
            select.dispatchEvent(new Event('change', {bubbles: true}));
        });
        optionsWrap.appendChild(item);
        return item;
      });

      function visibleOptionButtons() {
        return optionButtons.filter((item) => !item.classList.contains('hidden'));
      }

      function selectedOption() {
        return select.selectedOptions[0] || select.options[select.selectedIndex] || select.options[0];
      }

      function filterOptions() {
        const query = String(searchInput && searchInput.value || '').trim().toLowerCase();
        let visibleCount = 0;

        optionButtons.forEach((item) => {
          const haystack = `${item.textContent || ''} ${item.dataset.value || ''}`.toLowerCase();
          const visible = !query || haystack.includes(query);
          item.classList.toggle('hidden', !visible);
          if (visible)
            visibleCount += 1;
        });
        emptyState.classList.toggle('visible', visibleCount === 0);
      }

      function sync() {
        const selected = selectedOption();
        button.textContent = selected ? selected.textContent : '';
        optionButtons.forEach((item) => {
          const selectedItem = item.dataset.value === select.value;
          item.classList.toggle('selected', selectedItem);
          item.setAttribute('aria-selected', selectedItem ? 'true' : 'false');
        });
        filterOptions();
      }

      function close() {
        shell.classList.remove('open');
        button.setAttribute('aria-expanded', 'false');
        menu.classList.add('hidden');
      }

      function open() {
        if (searchInput)
          searchInput.value = '';
        sync();
        shell.classList.add('open');
        button.setAttribute('aria-expanded', 'true');
        menu.classList.remove('hidden');
      }

      function focusSelectedOption() {
        const visible = visibleOptionButtons();
        const selected = visible.find((item) => item.dataset.value === select.value);
        (selected || visible[0] || searchInput || button).focus();
      }

      function focusRelativeOption(step) {
        const visible = visibleOptionButtons();
        if (!visible.length)
          return;
        const currentIndex = visible.indexOf(document.activeElement);
        const selectedIndex = visible.findIndex((item) => item.dataset.value === select.value);
        const index = currentIndex >= 0 ? currentIndex : Math.max(0, selectedIndex);
        const nextIndex = (index + step + visible.length) % visible.length;
        visible[nextIndex].focus();
      }

      button.addEventListener('click', () => {
        if (shell.classList.contains('open'))
          close();
        else
          open();
      });

      button.addEventListener('keydown', (event) => {
        if (event.key === 'ArrowDown' || event.key === 'Enter' || event.key === ' ') {
          event.preventDefault();
          open();
          if (searchInput)
            searchInput.focus();
          else
            focusSelectedOption();
        } else if (event.key === 'Escape') {
          close();
        }
      });

      if (searchInput) {
        searchInput.addEventListener('input', () => {
          filterOptions();
        });
        searchInput.addEventListener('keydown', (event) => {
          if (event.key === 'Escape') {
            event.preventDefault();
            close();
            button.focus();
          } else if (event.key === 'ArrowDown') {
            event.preventDefault();
            focusSelectedOption();
          }
        });
      }

      menu.addEventListener('keydown', (event) => {
        if (event.key === 'Escape') {
          event.preventDefault();
          close();
          button.focus();
        } else if (event.key === 'ArrowDown') {
          event.preventDefault();
          focusRelativeOption(1);
        } else if (event.key === 'ArrowUp') {
          event.preventDefault();
          focusRelativeOption(-1);
        }
      });

      if (label)
        label.addEventListener('click', (event) => {
          event.preventDefault();
          button.focus();
        });

      document.addEventListener('click', (event) => {
        if (!shell.contains(event.target))
          close();
      });

      select.addEventListener('change', sync);
      select.__marsSyncRoundedSelect = sync;
      sync();
      return {sync, close};
    }

    const MARS_DATE_WEEKDAYS = ['Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat', 'Sun'];
    const MARS_DATE_MONTHS = [
      'January', 'February', 'March', 'April', 'May', 'June',
      'July', 'August', 'September', 'October', 'November', 'December'
    ];
    const marsDatePickerState = {
      input: null,
      button: null,
      shell: null,
      year: 0,
      month: 0
    };

    function parseMarsIsoDate(text) {
      const match = String(text || '').trim().match(/^(\d{4})-(\d{2})-(\d{2})$/);
      if (!match)
        return null;
      const year = Number(match[1]);
      const month = Number(match[2]);
      const day = Number(match[3]);
      if (!Number.isInteger(year) || !Number.isInteger(month) || !Number.isInteger(day))
        return null;
      const probe = new Date(Date.UTC(year, month - 1, day));
      if (probe.getUTCFullYear() !== year || probe.getUTCMonth() !== month - 1 || probe.getUTCDate() !== day)
        return null;
      return {year, month, day};
    }

    function marsDaysInMonth(year, month) {
      return new Date(Date.UTC(year, month, 0)).getUTCDate();
    }

    function marsIsoDate(year, month, day) {
      return `${String(year).padStart(4, '0')}-${String(month).padStart(2, '0')}-${String(day).padStart(2, '0')}`;
    }

    function marsTodayIsoDate() {
      const now = new Date();
      return marsIsoDate(now.getFullYear(), now.getMonth() + 1, now.getDate());
    }

    function marsStartWeekdayIndex(year, month) {
      const weekday = new Date(Date.UTC(year, month - 1, 1)).getUTCDay();
      return (weekday + 6) % 7;
    }

    function closeMarsDatePicker({restoreFocus = false} = {}) {
      if (!marsDatePicker)
        return;
      marsDatePicker.classList.add('hidden');
      if (marsDatePickerState.shell)
        marsDatePickerState.shell.classList.remove('open');
      if (restoreFocus && marsDatePickerState.button)
        marsDatePickerState.button.focus();
      marsDatePickerState.input = null;
      marsDatePickerState.button = null;
      marsDatePickerState.shell = null;
    }

    function placeMarsDatePicker(shell) {
      if (!marsDatePicker || !shell)
        return;
      const rect = shell.getBoundingClientRect();
      const width = rect.width;
      const margin = 12;
      const viewportWidth = window.innerWidth || document.documentElement.clientWidth || 0;
      const left = Math.max(margin, Math.min(rect.left, viewportWidth - width - margin));
      let top = rect.bottom + 8;

      marsDatePicker.style.width = `${width}px`;
      marsDatePicker.style.left = `${left}px`;
      marsDatePicker.style.top = `${top}px`;

      const pickerRect = marsDatePicker.getBoundingClientRect();
      const viewportHeight = window.innerHeight || document.documentElement.clientHeight || 0;
      if (pickerRect.bottom > viewportHeight - margin) {
        top = Math.max(margin, rect.top - pickerRect.height - 8);
        marsDatePicker.style.top = `${top}px`;
      }
    }

    function commitMarsDateValue(input, value) {
      if (!input)
        return;
      const changed = input.value !== value;
      input.value = value;
      if (changed)
        input.dispatchEvent(new Event('change', {bubbles: true}));
    }

    function renderMarsDatePicker() {
      if (!marsDatePicker || !marsDatePickerGrid || !marsDatePickerTitle || !marsDatePickerWeekdays)
        return;

      const {input, year, month} = marsDatePickerState;
      const selected = parseMarsIsoDate(input && input.value);
      const today = parseMarsIsoDate(marsTodayIsoDate());
      const daysInMonth = marsDaysInMonth(year, month);
      const startIndex = marsStartWeekdayIndex(year, month);
      const previousMonth = month === 1 ? 12 : month - 1;
      const previousYear = month === 1 ? year - 1 : year;
      const previousDays = marsDaysInMonth(previousYear, previousMonth);
      let nextDay = 1;

      marsDatePickerTitle.textContent = `${MARS_DATE_MONTHS[month - 1]} ${year}`;
      marsDatePickerWeekdays.replaceChildren(...MARS_DATE_WEEKDAYS.map((name) => {
        const cell = document.createElement('div');
        cell.className = 'mars-date-picker-weekday';
        cell.textContent = name;
        return cell;
      }));

      const dayButtons = [];
      for (let slot = 0; slot < 42; slot += 1) {
        const button = document.createElement('button');
        let displayDay;
        let buttonYear = year;
        let buttonMonth = month;

        button.type = 'button';
        button.className = 'mars-date-picker-day';

        if (slot < startIndex) {
          displayDay = previousDays - startIndex + slot + 1;
          buttonYear = previousYear;
          buttonMonth = previousMonth;
          button.classList.add('outside');
        } else if (slot >= startIndex + daysInMonth) {
          displayDay = nextDay++;
          buttonMonth = month === 12 ? 1 : month + 1;
          buttonYear = month === 12 ? year + 1 : year;
          button.classList.add('outside');
        } else {
          displayDay = slot - startIndex + 1;
        }

        const isoValue = marsIsoDate(buttonYear, buttonMonth, displayDay);
        button.textContent = String(displayDay);
        button.dataset.isoDate = isoValue;

        if (today && today.year === buttonYear && today.month === buttonMonth && today.day === displayDay)
          button.classList.add('today');
        if (selected && selected.year === buttonYear && selected.month === buttonMonth && selected.day === displayDay)
          button.classList.add('selected');

        button.addEventListener('click', () => {
          commitMarsDateValue(input, isoValue);
          closeMarsDatePicker({restoreFocus: true});
        });
        dayButtons.push(button);
      }

      marsDatePickerGrid.replaceChildren(...dayButtons);
      marsDatePicker.classList.remove('hidden');
      placeMarsDatePicker(marsDatePickerState.shell);
    }

    function openMarsDatePicker(input, button) {
      if (!input || !button || !marsDatePicker)
        return;

      const shell = button.closest('.mars-date-shell');
      const parsed = parseMarsIsoDate(input.value) || parseMarsIsoDate(marsTodayIsoDate());
      marsDatePickerState.input = input;
      marsDatePickerState.button = button;
      marsDatePickerState.shell = shell;
      marsDatePickerState.year = parsed.year;
      marsDatePickerState.month = parsed.month;
      if (shell)
        shell.classList.add('open');
      renderMarsDatePicker();
    }

    function shiftMarsDatePickerMonth(delta) {
      if (!marsDatePickerState.input)
        return;
      let nextMonth = marsDatePickerState.month + delta;
      let nextYear = marsDatePickerState.year;
      while (nextMonth < 1) {
        nextMonth += 12;
        nextYear -= 1;
      }
      while (nextMonth > 12) {
        nextMonth -= 12;
        nextYear += 1;
      }
      marsDatePickerState.year = nextYear;
      marsDatePickerState.month = nextMonth;
      renderMarsDatePicker();
    }

    function splitTopLevel(text, separator) {
      const parts = [];
      let start = 0;
      let depth = 0;
      for (let i = 0; i < text.length; i++) {
        const ch = text[i];
        if (ch === '(' || ch === '[' || ch === '{') depth++;
        else if (ch === ')' || ch === ']' || ch === '}') depth = Math.max(0, depth - 1);
        else if (ch === separator && depth === 0) {
          parts.push(text.slice(start, i));
          start = i + 1;
        }
      }
      parts.push(text.slice(start));
      return parts;
    }

    function indexOfTopLevel(text, needle) {
      let depth = 0;
      for (let i = 0; i < text.length; i++) {
        const ch = text[i];
        if (ch === '(' || ch === '[' || ch === '{') depth++;
        else if (ch === ')' || ch === ']' || ch === '}') depth = Math.max(0, depth - 1);
        else if (ch === needle && depth === 0) return i;
      }
      return -1;
    }

    function lastIndexOfTopLevel(text, needle) {
      let depth = 0;
      let found = -1;
      for (let i = 0; i < text.length; i++) {
        const ch = text[i];
        if (ch === '(' || ch === '[' || ch === '{') depth++;
        else if (ch === ')' || ch === ']' || ch === '}') depth = Math.max(0, depth - 1);
        else if (ch === needle && depth === 0) found = i;
      }
      return found;
    }

    function variablesFromExpression(text) {
      text = String(text || '').trim();
      if (text.startsWith('{') && text.endsWith('}')) {
        text = text.slice(1, -1).trim();
      }

      const pipe = lastIndexOfTopLevel(text, '|');
      if (pipe < 0) return [];

      let bindings = text.slice(pipe + 1).trim();
      const semi = indexOfTopLevel(bindings, ';');
      if (semi >= 0) bindings = bindings.slice(0, semi);

      return splitTopLevel(bindings, ',')
        .map((part) => {
          const eq = indexOfTopLevel(part, '=');
          return eq >= 0 ? part.slice(0, eq).trim() : '';
        })
        .filter(Boolean);
    }

    function compareBindingNames(left, right) {
      const leftName = String((left && left.name) || left || '');
      const rightName = String((right && right.name) || right || '');
      return leftName.localeCompare(rightName, undefined, {numeric: true, sensitivity: 'base'}) ||
        leftName.localeCompare(rightName);
    }

    function sortedAssignmentParts(parts) {
      return [...(parts || [])].sort((left, right) => {
        const leftEq = indexOfTopLevel(left, '=');
        const rightEq = indexOfTopLevel(right, '=');
        const leftName = leftEq >= 0 ? left.slice(0, leftEq).trim() : String(left || '').trim();
        const rightName = rightEq >= 0 ? right.slice(0, rightEq).trim() : String(right || '').trim();
        return compareBindingNames(leftName, rightName);
      });
    }

    function expressionWithSortedConstants(text) {
      const normalized = expressionForEditor(text).trim();
      const parts = bindingParts(normalized);
      if (!parts || !parts.constants)
        return normalized;

      const variableAssignments = splitTopLevel(parts.variables, ',')
        .map((part) => part.trim())
        .filter(Boolean);
      const constantAssignments = sortedAssignmentParts(
        splitTopLevel(parts.constants, ',')
          .map((part) => part.trim())
          .filter(Boolean)
      );

      let bindingText = variableAssignments.join(', ');
      if (constantAssignments.length) {
        const constants = constantAssignments.join(', ');
        bindingText = bindingText ? `${bindingText}; ${constants}` : `; ${constants}`;
      }

      return `{ ${parts.body} | ${bindingText} }`;
    }

    function canGoalSeek() {
      return currentVariables.length > 0;
    }

    function derivativeExpressionFromLine(line) {
      const match = String(line || '').match(/^d\/d[^=]*=\s*(.+)$/);
      return match ? match[1].trim() : '';
    }

    function integralExpressionFromLine(line) {
      const match = String(line || '').match(/^∫d[^=]*=\s*(.+)$/);
      return match ? match[1].trim() : '';
    }

    function expressionForEvaluation(text) {
      return String(text || '').replace(/(=\s*)\?/g, '$1NAN');
    }

    function expressionForEditor(text) {
      return String(text || '')
        .replace(/(=\s*)NAN\b/g, '$1?');
    }

    function restoreCompactBindingValues(text) {
      text = String(text || '').trim();
      if (!text.includes('...'))
        return text;

      const parts = bindingParts(text);
      if (!parts)
        return text;

      const restoreValues = new Map(bindingValueCache);
      const fullParts = bindingParts(expr.dataset.fullExpression || fullExpressionText);
      if (fullParts) {
        [fullParts.variables, fullParts.constants].forEach((assignmentsText) => {
          splitTopLevel(assignmentsText, ',').forEach((part) => {
            const eq = indexOfTopLevel(part, '=');
            if (eq < 0)
              return;

            const name = part.slice(0, eq).trim();
            const value = part.slice(eq + 1).trim();
            if (name && value && !restoreValues.has(name))
              restoreValues.set(name, value);
          });
        });
      }

      if (restoreValues.size === 0)
        return text;

      let changed = false;
      function restoreAssignments(assignmentsText) {
        return splitTopLevel(assignmentsText, ',')
          .map((part) => {
            const eq = indexOfTopLevel(part, '=');
            if (eq < 0)
              return part.trim();

            const name = part.slice(0, eq).trim();
            const valueText = part.slice(eq + 1).trim();
            const cached = restoreValues.get(name);
            if (cached && valueText.endsWith('...') && cached.startsWith(valueText.slice(0, -3))) {
              changed = true;
              return `${name} = ${cached}`;
            }
            return part.trim();
          })
          .filter(Boolean)
          .join(', ');
      }

      const variables = restoreAssignments(parts.variables);
      const constants = restoreAssignments(parts.constants);
      let bindingText = variables;
      if (constants)
        bindingText = bindingText ? `${bindingText}; ${constants}` : `; ${constants}`;

      return changed ? `{ ${parts.body} | ${bindingText} }` : text;
    }

    function currentExpressionText() {
      const text = expr.value.trim();
      const full = expr.dataset.fullExpression || fullExpressionText;
      const compact = expr.dataset.displayExpression || displayedExpressionText;
      if (full && compact && text === compact)
        return full;
      if (text.includes('...'))
        return restoreCompactBindingValues(text);
      return expressionWithEditorBody(text);
    }

    function expressionWithEditorBody(bodyText) {
      const body = String(bodyText || '').trim();
      if (!body)
        return '';
      if (bindingParts(body))
        return body;
      if (currentMode() === 'equation')
        return body;

      const full = expr.dataset.fullExpression || fullExpressionText;
      const parts = bindingParts(full);
      if (!parts)
        return body;
      if (!bodyReferencesBindingNames(body, compactExpressionForEditor(full).bindings || []))
        return body;

      let bindingText = parts.variables;
      if (parts.constants)
        bindingText = bindingText ? `${bindingText}; ${parts.constants}` : `; ${parts.constants}`;
      return bindingText ? `{ ${body} | ${bindingText} }` : body;
    }

    function expressionBodyForEditor(fullText) {
      const text = expressionForEditor(fullText).trim();
      const parts = bindingParts(text);
      return parts ? parts.body : text;
    }

    function clearExpressionSource() {
      fullExpressionText = '';
      displayedExpressionText = '';
      lastEvaluationInputText = '';
      bindingValueCache = new Map();
      delete expr.dataset.fullExpression;
      delete expr.dataset.displayExpression;
      clearGoalSeekRequest();
    }

    function clearGoalSeekRequest() {
      delete expr.dataset.goalSeekSource;
      delete expr.dataset.goalSeekTarget;
    }

    function currentGoalSeekSource() {
      const source = expr.dataset.goalSeekSource || '';

      if (!source)
        return '';
      if (expr.value.trim() !== (expr.dataset.displayExpression || displayedExpressionText))
        return '';
      return source;
    }

    function expressionHasSolvedVariables(text) {
      const parts = bindingParts(text);

      if (!parts)
        return false;

      return splitTopLevel(parts.variables, ',').some((part) => {
        const eq = indexOfTopLevel(part, '=');
        if (eq < 0)
          return false;

        const valueText = part.slice(eq + 1).trim();
        return valueText && valueText !== '?' && !/^NAN$/i.test(valueText);
      });
    }

    function bindingParts(text) {
      text = String(text || '').trim();
      const wrapped = text.startsWith('{') && text.endsWith('}');
      if (wrapped)
        text = text.slice(1, -1).trim();

      const pipe = lastIndexOfTopLevel(text, '|');
      if (pipe < 0)
        return null;

      const body = text.slice(0, pipe).trim();
      const bindings = text.slice(pipe + 1).trim();
      const semi = indexOfTopLevel(bindings, ';');
      const variables = semi >= 0 ? bindings.slice(0, semi).trim() : bindings;
      const constants = semi >= 0 ? bindings.slice(semi + 1).trim() : '';
      return {wrapped, body, variables, constants};
    }

    function compactBindingValue(valueText) {
      const text = String(valueText || '').trim();
      if (!text || text === '?' || /^NAN$/i.test(text))
        return {display: text, shortened: false};
      if (text.includes('...'))
        return {display: text, shortened: false};
      if (text.length <= COMPACT_BINDING_VALUE_LIMIT)
        return {display: text, shortened: false};
      return {display: compactLongNumericTokens(text), shortened: true};
    }

    function compactLongNumericTokens(text) {
      return String(text || '').replace(
        /(^|[^A-Za-z0-9_.])([+-]?(?:\d+\.\d+|\d{21,})(?:[Ee][+-]?\d+)?)/g,
        (match, prefix, numberText) => {
          if (numberText.includes('...') || numberText.length <= COMPACT_BINDING_VALUE_LIMIT)
            return match;
          return `${prefix}${numberText.slice(0, COMPACT_BINDING_VALUE_KEEP)}...`;
        }
      );
    }

    function compactExpressionForEditor(fullText) {
      const full = expressionForEditor(fullText);
      const parts = bindingParts(full);
      if (!parts) {
        const display = compactLongNumericTokens(full);
        return {display, bindings: [], shortened: display !== full};
      }

      const bindingValues = [];
      let shortened = false;
      const body = compactLongNumericTokens(parts.body);
      shortened = shortened || body !== parts.body;

      function compactAssignments(assignmentsText, kind) {
        const rows = splitTopLevel(assignmentsText, ',')
          .map((part) => {
            const eq = indexOfTopLevel(part, '=');
            if (eq < 0) {
              const text = part.trim();
              return text ? {name: text, text, bind: false} : null;
            }

            const name = part.slice(0, eq).trim();
            const valueText = part.slice(eq + 1).trim();
            const compact = compactBindingValue(valueText);
            shortened = shortened || compact.shortened;
            return name
              ? {name, value: valueText, display: compact.display, kind, text: `${name} = ${compact.display}`, bind: true}
              : null;
          })
          .filter(Boolean);

        if (kind === 'constant')
          rows.sort(compareBindingNames);
        rows.forEach((row) => {
          if (row.bind && row.name)
            bindingValues.push({
              name: row.name,
              value: row.value || '',
              display: row.display || '',
              kind
            });
        });
        return rows.map((row) => row.text);
      }

      const variableAssignments = compactAssignments(parts.variables, 'variable');
      const constantAssignments = compactAssignments(parts.constants, 'constant');

      let bindingText = variableAssignments.join(', ');
      if (constantAssignments.length) {
        const constants = constantAssignments.join(', ');
        bindingText = bindingText ? `${bindingText}; ${constants}` : `; ${constants}`;
      }

      return {
        display: `{ ${body} | ${bindingText} }`,
        bindings: bindingValues,
        shortened
      };
    }

    function expressionWithBindings(bodyText, bindings) {
      const body = String(bodyText || '').trim();
      if (!body)
        return '';
      if (!Array.isArray(bindings) || !bindings.length)
        return body;

      const variableAssignments = [];
      const constantAssignments = [];
      bindings.forEach((binding) => {
        const name = String(binding && binding.name || '').trim();
        if (!name)
          return;

        let valueText = String(binding && (binding.value ?? binding.display) || '').trim();
        if (!valueText || /^NAN$/i.test(valueText))
          valueText = '?';

        const assignment = `${name} = ${valueText}`;
        if (String(binding && binding.kind || 'variable').trim() === 'constant')
          constantAssignments.push(assignment);
        else
          variableAssignments.push(assignment);
      });

      constantAssignments.sort(compareBindingNames);
      let bindingText = variableAssignments.join(', ');
      if (constantAssignments.length) {
        const constants = constantAssignments.join(', ');
        bindingText = bindingText ? `${bindingText}; ${constants}` : `; ${constants}`;
      }
      return bindingText ? `{ ${body} | ${bindingText} }` : body;
    }

    function bodyReferencesBindingNames(bodyText, bindings) {
      const body = String(bodyText || '').trim();
      if (!body || !Array.isArray(bindings) || !bindings.length)
        return true;

      return bindings.every((binding) => {
        const name = String(binding && binding.name || '').trim();
        return !name || body.includes(name);
      });
    }

    function replaceBindingValueInExpression(sourceExpression, kind, targetName, valueText) {
      const parts = bindingParts(sourceExpression);
      if (!parts || !targetName)
        return sourceExpression;

      let changed = false;
      function replaceAssignments(assignmentsText, shouldReplace) {
        return splitTopLevel(assignmentsText, ',')
          .map((part) => {
            const eq = indexOfTopLevel(part, '=');
            if (eq < 0)
              return part.trim();

            const name = part.slice(0, eq).trim();
            if (!shouldReplace || name !== targetName)
              return part.trim();

            changed = true;
            return `${name} = ${valueText}`;
          })
          .filter(Boolean)
          .join(', ');
      }

      const variables = replaceAssignments(parts.variables, kind !== 'constant');
      const constants = replaceAssignments(parts.constants, kind === 'constant');
      if (!changed)
        return sourceExpression;

      let bindingText = variables;
      if (constants)
        bindingText = bindingText ? `${bindingText}; ${constants}` : `; ${constants}`;
      return `{ ${parts.body} | ${bindingText} }`;
    }

    function replaceBindingKindInExpression(sourceExpression, targetName, nextKind) {
      const parts = bindingParts(sourceExpression);
      if (!parts || !targetName)
        return sourceExpression;

      let movedAssignment = '';
      function removeAssignment(assignmentsText, shouldRemove) {
        return splitTopLevel(assignmentsText, ',')
          .map((part) => part.trim())
          .filter(Boolean)
          .filter((part) => {
            const eq = indexOfTopLevel(part, '=');
            const name = eq >= 0 ? part.slice(0, eq).trim() : part.trim();
            if (!shouldRemove || name !== targetName)
              return true;
            movedAssignment = part;
            return false;
          })
          .join(', ');
      }

      const variables = removeAssignment(parts.variables, nextKind === 'constant');
      const constants = removeAssignment(parts.constants, nextKind !== 'constant');
      if (!movedAssignment)
        return sourceExpression;

      const nextVariables = nextKind === 'constant'
        ? variables
        : [variables, movedAssignment].filter(Boolean).join(', ');
      const nextConstants = nextKind === 'constant'
        ? sortedAssignmentParts(
          [...splitTopLevel(constants, ','), movedAssignment]
            .map((part) => part.trim())
            .filter(Boolean)
        ).join(', ')
        : constants;
      let bindingText = nextVariables;
      if (nextConstants)
        bindingText = bindingText ? `${bindingText}; ${nextConstants}` : `; ${nextConstants}`;
      return `{ ${parts.body} | ${bindingText} }`;
    }

    function applyUpdatedBindingExpression(updated) {
      if (currentMode() === 'expression' || currentMode() === 'equation') {
        setExpressionEditor(updated);
        return;
      }

      if (bindingParts(updated))
        setExpressionEditor(updated);
      else {
        expr.value = expressionForEditor(updated).trim();
        clearExpressionSource();
        clearVariableValues();
      }
    }

    function saveCurrentModeEditorState() {
      if (currentMode() === 'expression')
        saveLastExpression(currentExpressionText() || expr.value.trim());
      else if (currentMode() === 'equation')
        saveLastEquationState();
      else if (currentMode() === 'matrix')
        saveLastMatrixState();
      else
        saveLastIntegratorState();
    }

    function normalisedBindingInputValue(input) {
      const text = String(input.value || '').trim();
      return text || '?';
    }

    function commitBindingInput(input) {
      const name = input.dataset.bindingName || '';
      const kind = input.dataset.bindingKind || 'variable';
      const valueText = normalisedBindingInputValue(input);
      const current = currentExpressionText();
      const updated = replaceBindingValueInExpression(current, kind, name, valueText);

      input.value = (valueText === '?' || /^NAN$/i.test(valueText)) ? '' : valueText;
      input.title = valueText;

      if (updated === current)
        return;

      applyUpdatedBindingExpression(updated);
      refreshVariableValuesFromEditor();
      updateHistoryButtons();
      saveCurrentModeEditorState();
    }

    function commitVisibleBindingInputs() {
      const inputs = Array.from(variableValues.querySelectorAll('.binding-value-input'));
      if (!inputs.length)
        return false;

      let current = currentExpressionText();
      let updated = current;

      inputs.forEach((input) => {
        const name = input.dataset.bindingName || '';
        const kind = input.dataset.bindingKind || 'variable';
        const valueText = normalisedBindingInputValue(input);
        updated = replaceBindingValueInExpression(updated, kind, name, valueText);
      });

      if (!updated || updated === current)
        return false;

      applyUpdatedBindingExpression(updated);
      refreshVariableValuesFromEditor();
      updateHistoryButtons();
      saveCurrentModeEditorState();
      return true;
    }

    function toggleBindingKind(binding) {
      const current = currentExpressionText();
      const name = String(binding && binding.name || '').trim();
      const currentKind = String(binding && binding.kind || 'variable').trim() || 'variable';
      if (!current || !name)
        return;

      const nextKind = currentKind === 'constant' ? 'variable' : 'constant';
      const updated = replaceBindingKindInExpression(current, name, nextKind);
      if (updated === current)
        return;

      applyUpdatedBindingExpression(updated);
      refreshVariableValuesFromEditor();
      updateHistoryButtons();
      saveCurrentModeEditorState();
    }

    function displayValueForBinding(binding) {
      const value = String(binding.value || binding.display || '').trim();
      return (value === '?' || /^NAN$/i.test(value)) ? '' : value;
    }

    function displayEquationValue(valueText) {
      const value = String(valueText || '').trim();
      return /^NAN$/i.test(value) ? 'unresolved' : value;
    }

    function fullValueForBinding(binding) {
      const value = String(binding.value || binding.display || '').trim();
      return (value === '?' || /^NAN$/i.test(value)) ? '' : value;
    }

    function clearVariableValues() {
      variableValues.replaceChildren();
      variableValues.classList.add('hidden');
      currentBindingKinds = new Map();
    }

    function refreshVariableValuesFromEditor() {
      const compact = compactExpressionForEditor(currentExpressionText());
      const bindings = visibleBindingsForCurrentMode(compact.bindings || []);
      renderVariableValues(bindings);
      currentVariables = variableNamesFromBindings(bindings);
      renderDerivativeButtons(currentVariables);
    }

    function renderVariableValues(bindings) {
      variableValues.replaceChildren();
      bindingValueCache = new Map();
      currentBindingKinds = new Map();
      if (!bindings.length) {
        variableValues.classList.add('hidden');
        return;
      }

      const variableBindings = [];
      const constantBindings = [];
      bindings.forEach((binding) => {
        const kind = binding.kind || 'variable';
        if (kind === 'constant')
          constantBindings.push(binding);
        else
          variableBindings.push(binding);
      });
      constantBindings.sort(compareBindingNames);

      [...variableBindings, ...constantBindings].forEach((binding) => {
        const kind = binding.kind || 'variable';
        currentBindingKinds.set(binding.name, kind);
        const displayValue = displayValueForBinding(binding);
        const fullValue = fullValueForBinding(binding);
        if (fullValue)
          bindingValueCache.set(binding.name, fullValue);

        const box = document.createElement('div');
        box.className = kind === 'constant'
          ? 'variable-value-box constant-value-box'
          : 'variable-value-box';

        const name = document.createElement('span');
        name.className = kind === 'constant'
          ? 'variable-value-name constant-value-name'
          : 'variable-value-name';
        name.textContent = binding.name;

        const text = document.createElement('input');
        text.className = 'variable-value-text binding-value-input';
        text.type = 'text';
        text.value = displayValue;
        text.title = fullValue || binding.value || '?';
        text.dataset.bindingName = binding.name;
        text.dataset.bindingKind = kind;
        text.autocomplete = 'off';
        text.spellcheck = false;
        text.addEventListener('keydown', (event) => {
          if ((event.ctrlKey || event.metaKey) && event.key === 'Enter') {
            event.preventDefault();
            commitBindingInput(text);
            evaluateFromKeyboard();
          } else if (event.key === 'Enter') {
            event.preventDefault();
            text.blur();
          } else if (event.key === 'Escape') {
            event.preventDefault();
            text.value = displayValue;
            text.blur();
          }
        });
        text.addEventListener('change', () => commitBindingInput(text));

        const actions = document.createElement('div');
        actions.className = 'variable-value-actions';

        const copy = document.createElement('button');
        copy.className = 'card-action variable-copy';
        copy.type = 'button';
        copy.textContent = 'Copy';
        copy.addEventListener('click', async () => {
          try {
            await writeClipboardText(text.value);
            flashCopyButton(copy, true);
            setStatus(`Copied ${binding.name}`);
            setTimeout(() => setStatus('Ready'), 1000);
          } catch (err) {
            flashCopyButton(copy, false);
            setStatus(String(err));
          }
        });

        const toggle = document.createElement('button');
        toggle.className = 'card-action variable-toggle';
        toggle.type = 'button';
        toggle.textContent = kind === 'constant' ? 'Variable' : 'Constant';
        toggle.title = kind === 'constant'
          ? `Treat ${binding.name} as a variable`
          : `Treat ${binding.name} as a constant`;
        toggle.addEventListener('click', () => toggleBindingKind(binding));

        actions.append(toggle, copy);
        box.append(name, text, actions);
        variableValues.appendChild(box);
      });

      variableValues.classList.remove('hidden');
    }

    function assignmentValuesByName(assignmentsText) {
      const values = new Map();
      splitTopLevel(assignmentsText || '', ',').forEach((part) => {
        const eq = indexOfTopLevel(part, '=');
        if (eq < 0)
          return;

        const name = part.slice(0, eq).trim();
        const valueText = part.slice(eq + 1).trim();
        if (name && valueText)
          values.set(name, valueText);
      });
      return values;
    }

    function solvedStartValuesForGoalSeek(sourceExpression, solvedExpression, providedStart = {}) {
      const start = {...providedStart};
      const sourceParts = bindingParts(sourceExpression);
      const solvedParts = bindingParts(solvedExpression);
      if (!sourceParts || !solvedParts)
        return start;

      const sourceVariables = new Set(assignmentValuesByName(sourceParts.variables).keys());
      const solvedVariables = assignmentValuesByName(solvedParts.variables);
      sourceVariables.forEach((name) => {
        const cachedValue = bindingValueCache.get(name);
        const solvedValue = cachedValue || solvedVariables.get(name) || '';
        if (!solvedValue || solvedValue === '?' || /^NAN$/i.test(solvedValue))
          return;
        start[name] = solvedValue;
      });

      return start;
    }

    function goalSeekExpressionAndStarts(sourceExpression, providedStart = {}) {
      const parts = bindingParts(sourceExpression);
      const start = {...providedStart};

      if (!parts)
        return {expression: sourceExpression, start};

      let changed = false;
      const variables = splitTopLevel(parts.variables, ',')
        .map((part) => {
          const eq = indexOfTopLevel(part, '=');
          if (eq < 0)
            return part.trim();

          const name = part.slice(0, eq).trim();
          const valueText = part.slice(eq + 1).trim();
          if (!name)
            return part.trim();

          if (valueText && valueText !== '?' && !/^NAN$/i.test(valueText) && !start[name])
            start[name] = valueText;

          changed = changed || valueText !== '?';
          return `${name} = ?`;
        })
        .filter(Boolean)
        .join(', ');

      let bindingText = variables;
      if (parts.constants)
        bindingText = bindingText ? `${bindingText}; ${parts.constants}` : `; ${parts.constants}`;

      return {
        expression: changed ? `{ ${parts.body} | ${bindingText} }` : sourceExpression,
        start
      };
    }

    function setExpressionEditor(fullText, evaluatedBindings = null, editorBodyText = null) {
      const compact = compactExpressionForEditor(fullText);
      const editorBindings = Array.isArray(evaluatedBindings) ? evaluatedBindings : [];
      const defaultEditorBody = expressionBodyForEditor(fullText);
      let editorBody = editorBodyText === null || editorBodyText === undefined
        ? defaultEditorBody
        : expressionForEditor(editorBodyText).trim();
      if (!bodyReferencesBindingNames(editorBody, editorBindings))
        editorBody = defaultEditorBody;
      const fullEditorText = expressionForEditor(fullText).trim();
      fullExpressionText = bindingParts(fullEditorText)
        ? fullEditorText
        : expressionWithBindings(editorBody, editorBindings) || fullEditorText;
      displayedExpressionText = editorBody;
      expr.dataset.fullExpression = fullExpressionText;
      expr.dataset.displayExpression = displayedExpressionText;
      expr.value = displayedExpressionText;
      const bindings = visibleBindingsForCurrentMode((editorBindings.length)
        ? editorBindings
        : compact.bindings);
      renderVariableValues(bindings || []);
      currentVariables = variableNamesFromBindings(bindings || []);
      renderDerivativeButtons(currentVariables);
    }

    function integratorEditableBindings(bindings) {
      const boundNames = currentIntegratorBoundNames();
      return (Array.isArray(bindings) ? bindings : [])
        .filter((binding) => !boundNames.has(String(binding && binding.name || '').trim()));
    }

    function applyIntegratorBindingState(data, fallbackExpression) {
      const bindingExpression = expressionWithSortedConstants(
        String(data && data.binding_expression || fallbackExpression || '').trim()
      );
      const editorBody = String(data && data.expression || expr.value || '').trim();
      const editableBindings = integratorEditableBindings(data && data.binding_values);

      if (bindingExpression && bindingParts(bindingExpression)) {
        setExpressionEditor(
          bindingExpression,
          editableBindings,
          editorBody || null
        );
        if (!editableBindings.length)
          clearVariableValues();
        modeEditorText.integrator = bindingExpression;
      } else if (editableBindings.length) {
        renderVariableValues(editableBindings);
      } else {
        clearVariableValues();
      }
    }

    function validPrecisionBits(bits, fallback) {
      const parsed = parseInt(String(bits), 10);
      if (!Number.isFinite(parsed))
        return fallback;
      return Math.max(DOUBLE_PRECISION_DIGITS, Math.min(MAX_PRECISION_BITS, parsed));
    }

    function validIntegratorIntervalCap(value) {
      const parsed = parseInt(String(value), 10);
      if (!Number.isFinite(parsed))
        return DEFAULT_INTEGRATOR_INTERVAL_CAP;
      const allowed = [500, 5000, 20000, 50000, 100000];
      return allowed.includes(parsed) ? parsed : DEFAULT_INTEGRATOR_INTERVAL_CAP;
    }

    function validDateText(value, fallback = DEFAULT_DATETIME_DATE) {
      const text = String(value || '').trim();
      return /^\d{4}-\d{2}-\d{2}$/.test(text) ? text : fallback;
    }

    function validDatetimeJurisdiction(value, fallback = DEFAULT_DATETIME_JURISDICTION) {
      const jurisdiction = String(value || '').trim();
      return HOLIDAY_JURISDICTION_SET.has(jurisdiction) ? jurisdiction : fallback;
    }

    function restoreDatetimeDefaultsIfBlank() {
      if (datetimeDate && !datetimeDate.value)
        datetimeDate.value = DEFAULT_DATETIME_DATE;
      if (datetimeStart && !datetimeStart.value)
        datetimeStart.value = datetimeDate?.value || DEFAULT_DATETIME_DATE;
      if (datetimeEnd && !datetimeEnd.value)
        datetimeEnd.value = datetimeDate?.value || DEFAULT_DATETIME_DATE;
      if (datetimeYear && !datetimeYear.value)
        datetimeYear.value = String((datetimeDate?.value || DEFAULT_DATETIME_DATE).slice(0, 4));
      if (datetimeJurisdiction && !datetimeJurisdiction.value)
        setSelectValue(datetimeJurisdiction, DEFAULT_DATETIME_JURISDICTION);
      if (datetimeLatitude && !datetimeLatitude.value)
        datetimeLatitude.value = DEFAULT_DATETIME_LATITUDE;
      if (datetimeLongitude && !datetimeLongitude.value)
        datetimeLongitude.value = DEFAULT_DATETIME_LONGITUDE;
      if (datetimeElevation && !datetimeElevation.value)
        datetimeElevation.value = DEFAULT_DATETIME_ELEVATION;
    }

    function currentDatetimeState() {
      restoreDatetimeDefaultsIfBlank();
      const currentOffsetText = String(datetimeGmtOffset && datetimeGmtOffset.value || '').trim();
      const effectiveOffsetText = (!datetimeGmtOffsetTouched || currentOffsetText === datetimeAutoGmtOffset)
        ? ''
        : currentOffsetText;
      return {
        date: validDateText(datetimeDate && datetimeDate.value),
        jdn: String(datetimeJdn && datetimeJdn.value || '').trim(),
        start: validDateText(datetimeStart && datetimeStart.value, datetimeDate && datetimeDate.value || DEFAULT_DATETIME_DATE),
        end: validDateText(datetimeEnd && datetimeEnd.value, datetimeDate && datetimeDate.value || DEFAULT_DATETIME_DATE),
        year: String(datetimeYear && datetimeYear.value || (datetimeDate && datetimeDate.value || DEFAULT_DATETIME_DATE).slice(0, 4)).trim(),
        jurisdiction: validDatetimeJurisdiction(datetimeJurisdiction && datetimeJurisdiction.value),
        latitude: String(datetimeLatitude && datetimeLatitude.value || DEFAULT_DATETIME_LATITUDE).trim(),
        longitude: String(datetimeLongitude && datetimeLongitude.value || DEFAULT_DATETIME_LONGITUDE).trim(),
        elevation: String(datetimeElevation && datetimeElevation.value || DEFAULT_DATETIME_ELEVATION).trim(),
        gmt_offset: effectiveOffsetText
      };
    }

    function restoreAlmanacDefaultsIfBlank() {
      if (almanacDate && !almanacDate.value)
        almanacDate.value = DEFAULT_ALMANAC_DATE;
      if (almanacTime && !almanacTime.value)
        almanacTime.value = DEFAULT_ALMANAC_TIME;
      if (almanacZone && !almanacZone.value)
        almanacZone.value = DEFAULT_ALMANAC_ZONE;
      if (almanacLatitude && !almanacLatitude.value)
        almanacLatitude.value = DEFAULT_ALMANAC_LATITUDE;
      if (almanacLongitude && !almanacLongitude.value)
        almanacLongitude.value = DEFAULT_ALMANAC_LONGITUDE;
      if (almanacBody && !almanacBody.value)
        setSelectValue(almanacBody, DEFAULT_ALMANAC_BODY);
    }

    function validAlmanacBody(value, fallback = DEFAULT_ALMANAC_BODY) {
      const raw = String(value || '').trim();
      const allowed = Array.from((almanacBody && almanacBody.options) || []).map((option) => option.value);
      if (allowed.includes(raw))
        return raw;
      return fallback;
    }

    function currentAlmanacState() {
      restoreAlmanacDefaultsIfBlank();
      return {
        date: validDateText(almanacDate && almanacDate.value, DEFAULT_ALMANAC_DATE),
        time: String(almanacTime && almanacTime.value || DEFAULT_ALMANAC_TIME).trim(),
        zone: String(almanacZone && almanacZone.value || DEFAULT_ALMANAC_ZONE).trim(),
        latitude: String(almanacLatitude && almanacLatitude.value || DEFAULT_ALMANAC_LATITUDE).trim(),
        longitude: String(almanacLongitude && almanacLongitude.value || DEFAULT_ALMANAC_LONGITUDE).trim(),
        body: validAlmanacBody(almanacBody && almanacBody.value, DEFAULT_ALMANAC_BODY)
      };
    }

    function almanacSummaryText(state = currentAlmanacState()) {
      return [
        'AstroNav 2000-2040 Navigation Almanac',
        `Date: ${state.date}`,
        `GMT time: ${state.time}`,
        `Zone: ${state.zone}`,
        `Latitude: ${state.latitude}`,
        `Longitude: ${state.longitude}`,
        `Body: ${state.body}`
      ].join('\n');
    }

    function datetimeSummaryText(state = currentDatetimeState()) {
      return [
        'MARS datetime observation',
        `Date: ${state.date}`,
        state.jdn ? `Julian Day Number: ${state.jdn}` : '',
        `Range: ${state.start} to ${state.end}`,
        `Year: ${state.year}`,
        `Holiday jurisdiction: ${state.jurisdiction}`,
        `Location: ${state.latitude}, ${state.longitude}`,
        `GMT offset: ${state.gmt_offset || 'local machine offset'}`
      ].filter(Boolean).join('\n');
    }

    function setDatetimeLocalText(text, sections = null) {
      const body = String(text || '').trim();
      if (datetimeLocalBody)
        renderDatetimeSections(datetimeLocalBody, null, sections, body);
      if (datetimeLocal)
        datetimeLocal.classList.toggle('hidden', !body || currentMode() !== 'datetime');
    }

    function validMatrixOperation(value) {
      const operation = String(value || '').trim();
      const allowed = Array.from(matrixOperation.options).map((option) => option.value);
      return allowed.includes(operation) ? operation : 'inverse';
    }

    function validLabMode(value) {
      const mode = String(value || '').trim();
      return mode === 'equation' || mode === 'matrix' || mode === 'integrator' || mode === 'datetime' || mode === 'almanac' ? mode : 'expression';
    }

    function applySavedState(data) {
      const saved = String(data.expression || '').trim();
      if (saved && !saved.includes('...')) {
        modeEditorText.expression = saved;
        setExpressionEditor(saved);
      }

      const savedMatrix = String(data.matrix || '').trim();
      if (savedMatrix && !savedMatrix.includes('...'))
        modeEditorText.matrix = savedMatrix;

      const savedEquation = String(data.equation || '').trim();
      if (savedEquation && !savedEquation.includes('...'))
        modeEditorText.equation = expressionWithSortedConstants(savedEquation);

      const savedEquationVariable = String(data.equation_variable || '').trim();
      if (equationVariable)
        equationVariable.value = savedEquationVariable || DEFAULT_EQUATION_VARIABLE_TEXT;

      const savedMatrixOperation = validMatrixOperation(data.matrix_operation);
      if (matrixOperation)
        matrixOperation.value = savedMatrixOperation;

      const savedMatrixOperand = String(data.matrix_operand || '').trim();
      if (matrixOperand)
        matrixOperand.value = savedMatrixOperand;

      const savedIntegrator = String(data.integrator_expression || '').trim();
      if (savedIntegrator && !savedIntegrator.includes('...')) {
        modeEditorText.integrator = savedIntegrator;
        expr.dataset.savedIntegratorExpression = savedIntegrator;
      }

      const savedBounds = String(data.integrator_bounds || '').trim();
      if (savedBounds)
        restoreIntegratorBoundsText(savedBounds);

      const savedCap = validIntegratorIntervalCap(data.integrator_interval_cap);
      if (integratorIntervalCap)
        integratorIntervalCap.value = String(savedCap);

      if (datetimeDate)
        datetimeDate.value = validDateText(data.datetime_date, DEFAULT_DATETIME_DATE);
      if (datetimeJdn)
        datetimeJdn.value = String(data.datetime_jdn || '');
      if (datetimeStart)
        datetimeStart.value = validDateText(data.datetime_start, datetimeDate?.value || DEFAULT_DATETIME_DATE);
      if (datetimeEnd)
        datetimeEnd.value = validDateText(data.datetime_end, datetimeDate?.value || DEFAULT_DATETIME_DATE);
      if (datetimeYear)
        datetimeYear.value = String(data.datetime_year || (datetimeDate?.value || DEFAULT_DATETIME_DATE).slice(0, 4));
      if (datetimeJurisdiction)
        setSelectValue(datetimeJurisdiction, validDatetimeJurisdiction(data.datetime_jurisdiction, DEFAULT_DATETIME_JURISDICTION));
      if (datetimeLatitude)
        datetimeLatitude.value = String(data.datetime_latitude || DEFAULT_DATETIME_LATITUDE);
      if (datetimeLongitude)
        datetimeLongitude.value = String(data.datetime_longitude || DEFAULT_DATETIME_LONGITUDE);
      if (datetimeElevation)
        datetimeElevation.value = String(data.datetime_elevation || DEFAULT_DATETIME_ELEVATION);
      if (datetimeGmtOffset) {
        datetimeGmtOffset.value = String(data.datetime_gmt_offset || DEFAULT_DATETIME_GMT_OFFSET);
        datetimeAutoGmtOffset = String(datetimeGmtOffset.value || '').trim();
        datetimeGmtOffsetTouched = false;
      }
      if (almanacDate)
        almanacDate.value = validDateText(data.almanac_date, DEFAULT_ALMANAC_DATE);
      if (almanacTime)
        almanacTime.value = String(data.almanac_time || DEFAULT_ALMANAC_TIME).trim() || DEFAULT_ALMANAC_TIME;
      if (almanacZone)
        almanacZone.value = String(data.almanac_zone || DEFAULT_ALMANAC_ZONE).trim();
      if (almanacLatitude)
        almanacLatitude.value = String(data.almanac_latitude || DEFAULT_ALMANAC_LATITUDE).trim();
      if (almanacLongitude)
        almanacLongitude.value = String(data.almanac_longitude || DEFAULT_ALMANAC_LONGITUDE).trim();
      if (almanacBody)
        setSelectValue(almanacBody, validAlmanacBody(data.almanac_body, DEFAULT_ALMANAC_BODY));

      if (data.precision_bits && typeof data.precision_bits === 'object') {
        Object.entries(data.precision_bits).forEach(([mode, bits]) => {
          if (modePrecisionBits[mode] !== undefined)
            modePrecisionBits[mode] = validPrecisionBits(bits, modePrecisionBits[mode]);
        });
      } else if (data.precision_bits !== undefined) {
        modePrecisionBits.expression = validPrecisionBits(data.precision_bits, modePrecisionBits.expression);
      }
      workingPrecisionBits = modePrecisionBits[currentMode()] || workingPrecisionBits;

      applyLabMode(validLabMode(data.lab_mode));
    }

    async function loadLastState() {
      try {
        const response = await fetch('/state');
        const data = await response.json();
        applySavedState(data || {});
        if (String(data.expression || '').trim())
          return;
      } catch (_) {
        // Fall back to localStorage below.
      }

      try {
        const saved = localStorage.getItem('mars.exprLab.lastExpression');
        if (saved && !saved.includes('...'))
          setExpressionEditor(saved);
        const matrixText = localStorage.getItem('mars.exprLab.lastMatrix');
        if (matrixText && !matrixText.includes('...'))
          modeEditorText.matrix = matrixText;
        const matrixOperationText = localStorage.getItem('mars.exprLab.lastMatrixOperation');
        if (matrixOperation && matrixOperationText)
          matrixOperation.value = validMatrixOperation(matrixOperationText);
        const matrixOperandText = localStorage.getItem('mars.exprLab.lastMatrixOperand');
        if (matrixOperand && matrixOperandText !== null)
          matrixOperand.value = matrixOperandText;
        const equationText = localStorage.getItem('mars.exprLab.lastEquation');
        if (equationText && !equationText.includes('...'))
          modeEditorText.equation = expressionWithSortedConstants(equationText);
        const equationVariableText = localStorage.getItem('mars.exprLab.lastEquationVariable');
        if (equationVariable && equationVariableText)
          equationVariable.value = equationVariableText;
        const integratorExpression = localStorage.getItem('mars.exprLab.lastIntegratorExpression');
        if (integratorExpression && !integratorExpression.includes('...'))
          modeEditorText.integrator = integratorExpression;
        const integratorBoundsText = localStorage.getItem('mars.exprLab.lastIntegratorBounds');
        if (integratorBoundsText)
          restoreIntegratorBoundsText(integratorBoundsText);
        const integratorCap = localStorage.getItem('mars.exprLab.lastIntegratorIntervalCap');
        if (integratorIntervalCap && integratorCap)
          integratorIntervalCap.value = String(validIntegratorIntervalCap(integratorCap));
        const datetimeStateText = localStorage.getItem('mars.exprLab.lastDatetimeState');
        if (datetimeStateText) {
          const state = JSON.parse(datetimeStateText);
          if (datetimeDate)
            datetimeDate.value = validDateText(state.date, DEFAULT_DATETIME_DATE);
          if (datetimeJdn)
            datetimeJdn.value = String(state.jdn || '');
          if (datetimeStart)
            datetimeStart.value = validDateText(state.start, datetimeDate?.value || DEFAULT_DATETIME_DATE);
          if (datetimeEnd)
            datetimeEnd.value = validDateText(state.end, datetimeDate?.value || DEFAULT_DATETIME_DATE);
          if (datetimeYear)
            datetimeYear.value = String(state.year || (datetimeDate?.value || DEFAULT_DATETIME_DATE).slice(0, 4));
        if (datetimeJurisdiction)
          setSelectValue(datetimeJurisdiction, validDatetimeJurisdiction(state.jurisdiction, DEFAULT_DATETIME_JURISDICTION));
        if (datetimeLatitude)
          datetimeLatitude.value = String(state.latitude || DEFAULT_DATETIME_LATITUDE);
        if (datetimeLongitude)
          datetimeLongitude.value = String(state.longitude || DEFAULT_DATETIME_LONGITUDE);
        if (datetimeElevation)
          datetimeElevation.value = String(state.elevation || DEFAULT_DATETIME_ELEVATION);
        if (datetimeGmtOffset) {
          datetimeGmtOffset.value = String(state.gmt_offset || DEFAULT_DATETIME_GMT_OFFSET);
          datetimeAutoGmtOffset = String(datetimeGmtOffset.value || '').trim();
          datetimeGmtOffsetTouched = false;
        }
        }
        const almanacStateText = localStorage.getItem('mars.exprLab.lastAlmanacState');
        if (almanacStateText) {
          const state = JSON.parse(almanacStateText);
          if (almanacDate)
            almanacDate.value = validDateText(state.date, DEFAULT_ALMANAC_DATE);
          if (almanacTime)
            almanacTime.value = String(state.time || DEFAULT_ALMANAC_TIME).trim() || DEFAULT_ALMANAC_TIME;
          if (almanacZone)
            almanacZone.value = String(state.zone || DEFAULT_ALMANAC_ZONE).trim();
          if (almanacLatitude)
            almanacLatitude.value = String(state.latitude || DEFAULT_ALMANAC_LATITUDE).trim();
          if (almanacLongitude)
            almanacLongitude.value = String(state.longitude || DEFAULT_ALMANAC_LONGITUDE).trim();
          if (almanacBody)
            setSelectValue(almanacBody, validAlmanacBody(state.body, DEFAULT_ALMANAC_BODY));
        }
        const labMode = localStorage.getItem(LAB_MODE_STORAGE_KEY);
        if (labMode)
          applyLabMode(labMode);
      } catch (_) {
        // Private browsing or locked-down webviews can disable localStorage.
      }
    }

    function saveLabState(patch) {
      const payload = {...patch};
      fetch('/state', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify(payload)
      }).catch(() => {
        // Persistence is helpful, not essential.
      });
    }

    function savePrecisionState() {
      saveLabState({precision_bits: modePrecisionBits});
    }

    function saveLastLabMode(mode = currentMode()) {
      const labMode = validLabMode(mode);
      try {
        localStorage.setItem(LAB_MODE_STORAGE_KEY, labMode);
      } catch (_) {
        // The lab still works fine without persistence.
      }
      saveLabState({lab_mode: labMode});
    }

    function saveLastExpression(text) {
      text = String(text || '').trim();
      if (text.includes('...') && fullExpressionText)
        text = fullExpressionText;
      if (text.includes('...'))
        return;

      try {
        if (text)
          localStorage.setItem('mars.exprLab.lastExpression', text);
      } catch (_) {
        // The lab still works fine without persistence.
      }

      if (!text)
        return;

      saveLabState({
        expression: text,
        precision_bits: modePrecisionBits
      });
    }

    function saveLastMatrixState() {
      const text = String(currentExpressionText() || expr.value || '').trim();
      const operation = validMatrixOperation(matrixOperation && matrixOperation.value);
      const operand = String(matrixOperand && matrixOperand.value || '').trim();
      if (text)
        modeEditorText.matrix = text;

      try {
        if (text)
          localStorage.setItem('mars.exprLab.lastMatrix', text);
        localStorage.setItem('mars.exprLab.lastMatrixOperation', operation);
        localStorage.setItem('mars.exprLab.lastMatrixOperand', operand);
      } catch (_) {
        // The lab still works fine without persistence.
      }

      saveLabState({
        matrix: text,
        matrix_operation: operation,
        matrix_operand: operand,
        precision_bits: modePrecisionBits
      });
    }

    function saveLastEquationState() {
      const text = expressionWithSortedConstants(String(currentExpressionText() || expr.value || '').trim());
      if (text)
        modeEditorText.equation = text;

      try {
        if (text)
          localStorage.setItem('mars.exprLab.lastEquation', text);
      } catch (_) {
        // The lab still works fine without persistence.
      }

      saveLabState({
        equation: text,
        precision_bits: modePrecisionBits
      });
    }

    function saveLastIntegratorState() {
      const text = expressionWithSortedConstants(String(currentExpressionText() || expr.value || '').trim());
      const bounds = currentIntegratorBoundsText();
      const cap = requestedIntegratorIntervalCap();
      if (text)
        modeEditorText.integrator = text;

      try {
        if (text)
          localStorage.setItem('mars.exprLab.lastIntegratorExpression', text);
        if (bounds)
          localStorage.setItem('mars.exprLab.lastIntegratorBounds', bounds);
        localStorage.setItem('mars.exprLab.lastIntegratorIntervalCap', String(cap));
      } catch (_) {
        // The lab still works fine without persistence.
      }

      saveLabState({
        integrator_expression: text,
        integrator_bounds: bounds,
        integrator_interval_cap: cap,
        precision_bits: modePrecisionBits
      });
    }

    function saveLastDatetimeState() {
      const state = currentDatetimeState();
      modeEditorText.datetime = DEFAULT_DATETIME_TEXT;

      try {
        localStorage.setItem('mars.exprLab.lastDatetimeState', JSON.stringify(state));
      } catch (_) {
        // The lab still works fine without persistence.
      }

      saveLabState({
        datetime_date: state.date,
        datetime_jdn: state.jdn,
        datetime_start: state.start,
        datetime_end: state.end,
        datetime_year: state.year,
        datetime_jurisdiction: state.jurisdiction,
        datetime_latitude: state.latitude,
        datetime_longitude: state.longitude,
        datetime_elevation: state.elevation,
        datetime_gmt_offset: state.gmt_offset,
        precision_bits: modePrecisionBits
      });
    }

    function saveLastAlmanacState() {
      const state = currentAlmanacState();
      modeEditorText.almanac = DEFAULT_ALMANAC_TEXT;

      try {
        localStorage.setItem('mars.exprLab.lastAlmanacState', JSON.stringify(state));
      } catch (_) {
        // The lab still works fine without persistence.
      }

      saveLabState({
        almanac_date: state.date,
        almanac_time: state.time,
        almanac_zone: state.zone,
        almanac_latitude: state.latitude,
        almanac_longitude: state.longitude,
        almanac_body: state.body,
        precision_bits: modePrecisionBits
      });
    }

    function modeHistoryStack(store, mode = currentMode()) {
      return store[mode] || [];
    }

    function currentHistoryLength() {
      return modeHistoryStack(expressionHistory).length;
    }

    function currentForwardHistoryLength() {
      return modeHistoryStack(forwardHistory).length;
    }

    function historyStateForMode(mode = currentMode(), textOverride = null) {
      let text = String(
        textOverride === null || textOverride === undefined
          ? (currentExpressionText() || expr.value || '')
          : textOverride
      ).trim();
      const state = {mode, text};

      if (mode === 'equation' && equationVariable) {
        state.variable = String(equationVariable.value || DEFAULT_EQUATION_VARIABLE_TEXT).trim() ||
          DEFAULT_EQUATION_VARIABLE_TEXT;
      } else if (mode === 'matrix') {
        state.operation = matrixOperation.value;
        state.operand = String(matrixOperand.value || '').trim();
      } else if (mode === 'integrator') {
        state.bounds = currentIntegratorBoundsText();
        state.intervalCap = String(validIntegratorIntervalCap(
          integratorIntervalCap && integratorIntervalCap.value
        ));
      } else if (mode === 'datetime') {
        state.datetime = currentDatetimeState();
        if (textOverride === null || textOverride === undefined)
          text = datetimeSummaryText(state.datetime);
        state.text = text || DEFAULT_DATETIME_TEXT;
      } else if (mode === 'almanac') {
        state.almanac = currentAlmanacState();
        if (textOverride === null || textOverride === undefined)
          text = almanacSummaryText(state.almanac);
        state.text = text || DEFAULT_ALMANAC_TEXT;
      }

      return state;
    }

    function historyStatesEqual(left, right) {
      return JSON.stringify(left || null) === JSON.stringify(right || null);
    }

    function previousModeStateForHistory(nextState) {
      const previous = modeCommittedState[nextState && nextState.mode || currentMode()];

      if (!previous || historyStatesEqual(previous, nextState))
        return null;
      return previous;
    }

    function commitModeState(mode = currentMode(), textOverride = null) {
      modeCommittedState[mode] = historyStateForMode(mode, textOverride);
    }

    function restoreHistoryState(state) {
      if (!state)
        return;

      if (state.mode === 'equation' && equationVariable) {
        equationVariable.value = String(state.variable || DEFAULT_EQUATION_VARIABLE_TEXT).trim() ||
          DEFAULT_EQUATION_VARIABLE_TEXT;
      } else if (state.mode === 'matrix') {
        matrixOperation.value = state.operation || 'inverse';
        matrixOperand.value = String(state.operand || '').trim();
      } else if (state.mode === 'integrator') {
        restoreIntegratorBoundsText(state.bounds || DEFAULT_INTEGRATOR_BOUNDS_TEXT);
        if (integratorIntervalCap)
          integratorIntervalCap.value = String(validIntegratorIntervalCap(state.intervalCap));
      } else if (state.mode === 'datetime') {
        const datetimeState = state.datetime || {};
        if (datetimeDate)
          datetimeDate.value = validDateText(datetimeState.date, DEFAULT_DATETIME_DATE);
        if (datetimeJdn)
          datetimeJdn.value = String(datetimeState.jdn || '');
        if (datetimeStart)
          datetimeStart.value = validDateText(datetimeState.start, datetimeDate?.value || DEFAULT_DATETIME_DATE);
        if (datetimeEnd)
          datetimeEnd.value = validDateText(datetimeState.end, datetimeDate?.value || DEFAULT_DATETIME_DATE);
        if (datetimeYear)
          datetimeYear.value = String(datetimeState.year || (datetimeDate?.value || DEFAULT_DATETIME_DATE).slice(0, 4));
        if (datetimeJurisdiction)
          setSelectValue(datetimeJurisdiction, validDatetimeJurisdiction(datetimeState.jurisdiction, DEFAULT_DATETIME_JURISDICTION));
        if (datetimeLatitude)
          datetimeLatitude.value = String(datetimeState.latitude || DEFAULT_DATETIME_LATITUDE);
        if (datetimeLongitude)
          datetimeLongitude.value = String(datetimeState.longitude || DEFAULT_DATETIME_LONGITUDE);
        if (datetimeElevation)
          datetimeElevation.value = String(datetimeState.elevation || DEFAULT_DATETIME_ELEVATION);
        if (datetimeGmtOffset) {
          datetimeGmtOffset.value = String(datetimeState.gmt_offset || DEFAULT_DATETIME_GMT_OFFSET);
          datetimeAutoGmtOffset = String(datetimeGmtOffset.value || '').trim();
          datetimeGmtOffsetTouched = false;
        }
      } else if (state.mode === 'almanac') {
        const almanacState = state.almanac || {};
        if (almanacDate)
          almanacDate.value = validDateText(almanacState.date, DEFAULT_ALMANAC_DATE);
        if (almanacTime)
          almanacTime.value = String(almanacState.time || DEFAULT_ALMANAC_TIME).trim() || DEFAULT_ALMANAC_TIME;
        if (almanacZone)
          almanacZone.value = String(almanacState.zone || DEFAULT_ALMANAC_ZONE).trim();
        if (almanacLatitude)
          almanacLatitude.value = String(almanacState.latitude || DEFAULT_ALMANAC_LATITUDE).trim();
        if (almanacLongitude)
          almanacLongitude.value = String(almanacState.longitude || DEFAULT_ALMANAC_LONGITUDE).trim();
        if (almanacBody)
          setSelectValue(almanacBody, validAlmanacBody(almanacState.body, DEFAULT_ALMANAC_BODY));
      }

      if (state.mode === 'datetime') {
        expr.value = DEFAULT_DATETIME_TEXT;
        clearExpressionSource();
        clearVariableValues();
      } else if (state.mode === 'almanac') {
        expr.value = DEFAULT_ALMANAC_TEXT;
        clearExpressionSource();
        clearVariableValues();
      } else {
        applyUpdatedBindingExpression(state.text || '');
      }
    }

    function clearForwardHistory(mode = currentMode()) {
      forwardHistory[mode] = [];
    }

    function evaluateCurrentMode(options = {}) {
      if (currentMode() === 'equation') {
        evaluateEquation(options);
        return;
      }
      if (currentMode() === 'matrix') {
        evaluateMatrix(options);
        return;
      }
      if (currentMode() === 'integrator') {
        evaluateIntegrator(options);
        return;
      }
      if (currentMode() === 'datetime') {
        evaluateDatetime(options);
        return;
      }
      if (currentMode() === 'almanac') {
        evaluateAlmanac(options);
        return;
      }
      evaluateExpression(options);
    }

    function setBusy(isBusy) {
      const expressionMode = currentMode() === 'expression';
      run.disabled = isBusy;
      back.disabled = isBusy || currentHistoryLength() === 0;
      forward.disabled = isBusy || currentForwardHistoryLength() === 0;
      goalSeek.disabled = isBusy || !expressionMode || !canGoalSeek();
      goalSeek.title = goalSeek.disabled && !isBusy && expressionMode
        ? 'Goal seek needs at least one variable binding'
        : '';
      goalTarget.disabled = isBusy;
      lessPrecision.disabled = isBusy || atMinimumPrecision();
      morePrecision.disabled = isBusy || atMaximumPrecision();
      morePrecision.title = !isBusy && atMaximumPrecision()
        ? 'Already at the current maximum precision setting'
        : '';
      Array.from((integratorBoundStack || document.createElement('div')).querySelectorAll('input, button')).forEach((control) => {
        if (isBusy) {
          if (!control.disabled)
            control.dataset.busyDisabled = '1';
          control.disabled = true;
        } else if (control.dataset.busyDisabled === '1') {
          control.disabled = false;
          delete control.dataset.busyDisabled;
        }
      });
      Array.from((datetimeControls || document.createElement('div')).querySelectorAll('input, button, select')).forEach((control) => {
        if (isBusy) {
          if (!control.disabled)
            control.dataset.busyDisabled = '1';
          control.disabled = true;
        } else if (control.dataset.busyDisabled === '1') {
          control.disabled = false;
          delete control.dataset.busyDisabled;
        }
      });
      Array.from((almanacControls || document.createElement('div')).querySelectorAll('input, button, select')).forEach((control) => {
        if (isBusy) {
          if (!control.disabled)
            control.dataset.busyDisabled = '1';
          control.disabled = true;
        } else if (control.dataset.busyDisabled === '1') {
          control.disabled = false;
          delete control.dataset.busyDisabled;
        }
      });
      if (equationVariable)
        equationVariable.disabled = isBusy;
      if (integratorIntervalCap)
        integratorIntervalCap.disabled = isBusy;
      if (integratorAddBound)
        integratorAddBound.disabled = isBusy;
      copyButtons.forEach((button) => {
        button.disabled = isBusy;
      });
      moreDigitButtons.forEach((button) => {
        button.disabled = isBusy;
      });
      Array.from(variableValues.querySelectorAll('button')).forEach((button) => {
        button.disabled = isBusy;
      });
      Array.from(variableValues.querySelectorAll('input')).forEach((input) => {
        input.disabled = isBusy;
      });
      Array.from(derivativeButtons.querySelectorAll('button')).forEach((button) => {
        button.disabled = isBusy;
      });
    }

    function updateHistoryButtons() {
      const expressionMode = currentMode() === 'expression';
      back.disabled = currentHistoryLength() === 0;
      forward.disabled = currentForwardHistoryLength() === 0;
      lessPrecision.disabled = atMinimumPrecision();
      morePrecision.disabled = atMaximumPrecision();
      goalSeek.disabled = !expressionMode || !canGoalSeek();
      goalSeek.title = goalSeek.disabled && expressionMode
        ? 'Goal seek needs at least one variable binding'
        : '';
      morePrecision.title = atMaximumPrecision()
        ? 'Already at the current maximum precision setting'
        : '';
    }

    function pushExpressionHistory(entry) {
      const snapshot = typeof entry === 'string'
        ? historyStateForMode(currentMode(), entry)
        : (entry || historyStateForMode());
      const stack = modeHistoryStack(expressionHistory, snapshot.mode);
      const previous = stack[stack.length - 1];

      if (snapshot && snapshot.text && !historyStatesEqual(snapshot, previous))
        stack.push(snapshot);
      clearForwardHistory(snapshot.mode);
      updateHistoryButtons();
    }

    function renderDerivativeButtons(variables) {
      derivativeButtons.replaceChildren();
      if (!currentDifferentiable) return;
      variables.forEach((name) => {
        const derivativeButton = document.createElement('button');
        derivativeButton.className = 'secondary';
        derivativeButton.type = 'button';
        derivativeButton.textContent = `${name} derivative`;
        derivativeButton.addEventListener('click', () => takeDerivative(name));
        derivativeButtons.appendChild(derivativeButton);

        const integralButton = document.createElement('button');
        integralButton.className = 'secondary';
        integralButton.type = 'button';
        integralButton.textContent = `${name} integral`;
        integralButton.addEventListener('click', () => takeIntegral(name));
        derivativeButtons.appendChild(integralButton);
      });
    }

    async function fetchEvaluation(text, wrt = '', action = '') {
      const precision = requestedValuePrecision();
      const response = await fetch('/eval', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify({expression: expressionForEvaluation(text), wrt, precision, action})
      });
      const data = await response.json();
      return {response, data};
    }

    function cleanIntegratorBoundValue(value) {
      const text = String(value || '').trim();
      return /^blank\s+for\s+(none|antiderivative)$/i.test(text) ? '' : text;
    }

    function normaliseIntegratorRowKind(kind) {
      return String(kind || '').trim().toLowerCase() === 'free' ? 'free' : 'bound';
    }

    function sanitizeIntegratorRow(row, fallbackName = 'x') {
      const safe = row || {};
      return {
        kind: normaliseIntegratorRowKind(safe.kind),
        name: String(safe.name || fallbackName).trim() || fallbackName,
        lo: cleanIntegratorBoundValue(safe.lo),
        hi: cleanIntegratorBoundValue(safe.hi),
      };
    }

    function integratorDefaultVariableName(rows = []) {
      const taken = new Set(
        (Array.isArray(rows) ? rows : [])
          .map((row) => String(row && row.name || '').trim())
          .filter(Boolean)
      );
      const preferred = ['x', 'y', 'z', 't', 'u', 'v', 'w', 'r', 's'];
      for (const name of preferred) {
        if (!taken.has(name))
          return name;
      }
      for (let i = 1; i < 100; i += 1) {
        const name = `x${i}`;
        if (!taken.has(name))
          return name;
      }
      return 'x';
    }

    function integratorRowText(row) {
      const safe = sanitizeIntegratorRow(row);
      if (safe.kind === 'free')
        return `free ${safe.name}`;
      if (safe.lo && safe.hi)
        return `${safe.name} = ${safe.lo} .. ${safe.hi}`;
      if (safe.hi)
        return `${safe.name} = ${safe.hi}`;
      return safe.name;
    }

    function integratorBoundsTextFromRows(rows) {
      return (Array.isArray(rows) ? rows : [])
        .map((row) => integratorRowText(row))
        .filter(Boolean)
        .join('\n');
    }

    function parseIntegratorBoundsText(text) {
      const rows = [];
      for (const rawLine of String(text || '').split(/\n+/)) {
        const line = rawLine.trim();
        if (!line)
          continue;
        let match = line.match(/^free\s*(?::|\s)\s*(.+)$/i);
        if (match) {
          rows.push(sanitizeIntegratorRow({
            kind: 'free',
            name: match[1].trim(),
            lo: '',
            hi: '',
          }));
          continue;
        }
        match = line.match(/^([^:=]+?)\s*(?:=|:)\s*(.+?)\s*\.\.\s*(.+)$/);
        if (match) {
          rows.push(sanitizeIntegratorRow({
            kind: 'bound',
            name: match[1].trim(),
            lo: match[2].trim(),
            hi: match[3].trim(),
          }));
          continue;
        }
        match = line.match(/^([^:=]+?)\s*(?:=|:)\s*(.+)$/);
        if (match) {
          rows.push(sanitizeIntegratorRow({
            kind: 'bound',
            name: match[1].trim(),
            lo: '',
            hi: match[2].trim(),
          }));
          continue;
        }
        if (!/[=:]/.test(line) && !line.includes('..')) {
          rows.push(sanitizeIntegratorRow({
            kind: 'bound',
            name: line.trim(),
            lo: '',
            hi: '',
          }));
          continue;
        }
        throw new Error(`Bad bound line: ${line}`);
      }
      if (!rows.length)
        rows.push({kind: 'bound', name: 'x', lo: '0', hi: '1'});
      return rows;
    }

    function integratorFallbackRows() {
      return [{kind: 'bound', name: 'x', lo: '0', hi: '1'}];
    }

    function integratorBlankRows() {
      return [{kind: 'bound', name: 'x', lo: '', hi: ''}];
    }

    function currentIntegratorRows() {
      const rows = Array.from((integratorBoundStack || document.createElement('div')).querySelectorAll('.integrator-bound-row'))
        .map((row) => sanitizeIntegratorRow({
          kind: row.dataset.kind || 'bound',
          name: row.querySelector('[data-integrator-name]')?.value || '',
          lo: row.querySelector('[data-integrator-lower]')?.value || '',
          hi: row.querySelector('[data-integrator-upper]')?.value || '',
        }));
      return rows.length ? rows : integratorFallbackRows();
    }

    function escapeRegexLiteral(text) {
      return String(text || '').replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
    }

    function integratorExpressionReferencesName(expressionText, name) {
      const needle = String(name || '').trim();
      if (!needle)
        return false;
      const body = expressionBodyForEditor(expressionText || currentExpressionText() || expr.value || '');
      if (!body)
        return false;
      if (needle.startsWith('[') && needle.endsWith(']'))
        return body.includes(needle);
      const pattern = new RegExp(`(^|[^A-Za-z0-9_])${escapeRegexLiteral(needle)}(?=$|[^A-Za-z0-9_])`);
      return pattern.test(body);
    }

    function activeIntegratorBoundRows(rows = currentIntegratorRows(), expressionText = '') {
      const boundRows = (Array.isArray(rows) ? rows : [])
        .filter((row) => normaliseIntegratorRowKind(row.kind) === 'bound');
      const activeRows = boundRows.filter((row) =>
        row.lo ||
        row.hi ||
        boundRows.length === 1 ||
        integratorExpressionReferencesName(expressionText, row.name)
      );
      return activeRows.length ? activeRows : integratorFallbackRows();
    }

    function activeIntegratorRows(rows = currentIntegratorRows(), expressionText = '') {
      const activeBounds = activeIntegratorBoundRows(rows, expressionText);
      const activeBoundSet = new Set(activeBounds);
      const activeRows = (Array.isArray(rows) ? rows : [])
        .filter((row) => {
          if (normaliseIntegratorRowKind(row.kind) === 'free')
            return integratorExpressionReferencesName(expressionText, row.name);
          return activeBoundSet.has(row);
        });
      return activeRows.length ? activeRows : integratorFallbackRows();
    }

    function currentIntegratorBoundRows() {
      return activeIntegratorBoundRows();
    }

    function currentIntegratorBoundNames() {
      return new Set(currentIntegratorBoundRows().map((row) => row.name));
    }

    function renderIntegratorRows(rows) {
      const sourceRows = Array.isArray(rows) && rows.length ? rows : integratorFallbackRows();
      const safeRows = sourceRows.map((row, index) =>
        sanitizeIntegratorRow(row, integratorDefaultVariableName(sourceRows.slice(0, index)))
      );
      integratorBoundStack.replaceChildren();

      safeRows.forEach((row, index) => {
        const boundCount = safeRows.filter((entry) => entry.kind !== 'free').length;
        const item = document.createElement('div');
        item.className = 'integrator-bound-row';
        item.dataset.kind = row.kind;
        item.dataset.index = String(index);

        const toggle = document.createElement('button');
        toggle.className = 'card-action integrator-bound-toggle';
        toggle.type = 'button';
        toggle.textContent = row.kind === 'free' ? 'Bound' : 'Free';
        toggle.title = row.kind === 'free'
          ? `Integrate with respect to ${row.name}`
          : `Leave ${row.name} free`;
        toggle.addEventListener('click', () => {
          commitVisibleBindingInputs();
          const nextRows = currentIntegratorRows();
          const target = nextRows[index];
          if (!target)
            return;
          target.kind = target.kind === 'free' ? 'bound' : 'free';
          target.lo = target.kind === 'free' ? '' : target.lo;
          target.hi = target.kind === 'free' ? '' : target.hi;
          if (nextRows.filter((entry) => entry.kind !== 'free').length === 0)
            nextRows.push({kind: 'bound', name: integratorDefaultVariableName(nextRows), lo: '', hi: ''});
          renderIntegratorRows(nextRows);
          refreshVariableValuesFromEditor();
          updateHistoryButtons();
          if (currentMode() === 'integrator')
            saveLastIntegratorState();
        });

        const makeField = (labelText, value, datasetKey, placeholder = '', disabled = false) => {
          const field = document.createElement('div');
          field.className = disabled ? 'integrator-bound-field disabled' : 'integrator-bound-field';
          const label = document.createElement('label');
          label.textContent = labelText;
          const input = document.createElement('input');
          input.spellcheck = false;
          input.autocomplete = 'off';
          input.value = value;
          input.placeholder = placeholder;
          input.dataset[datasetKey] = '1';
          input.disabled = disabled;
          input.addEventListener('keydown', (event) => {
            if (event.key === 'Enter') {
              event.preventDefault();
              input.blur();
            } else if (event.key === 'Escape') {
              event.preventDefault();
              input.value = value;
              input.blur();
            }
          });
          input.addEventListener('change', () => {
            if (datasetKey !== 'integratorName')
              input.value = cleanIntegratorBoundValue(input.value);
            else
              input.value = String(input.value || row.name || 'x').trim() || row.name || 'x';
            if (currentMode() === 'integrator') {
              refreshVariableValuesFromEditor();
              updateHistoryButtons();
              saveLastIntegratorState();
            }
          });
          field.append(label, input);
          return field;
        };

        const nameField = makeField('Variable', row.name, 'integratorName');
        const lowerField = makeField('Lower bound', row.lo, 'integratorLower', 'blank for none', row.kind === 'free');
        const upperField = makeField('Upper bound', row.hi, 'integratorUpper', 'blank for none', row.kind === 'free');

        const add = document.createElement('button');
        add.className = 'card-action integrator-bound-add';
        add.type = 'button';
        add.textContent = '+';
        add.title = 'Add another integral row';
        add.addEventListener('click', () => {
          commitVisibleBindingInputs();
          const nextRows = currentIntegratorRows();
          nextRows.splice(index + 1, 0, {
            kind: 'bound',
            name: integratorDefaultVariableName(nextRows),
            lo: '',
            hi: '',
          });
          renderIntegratorRows(nextRows);
          refreshVariableValuesFromEditor();
          updateHistoryButtons();
          if (currentMode() === 'integrator')
            saveLastIntegratorState();
        });

        const remove = document.createElement('button');
        remove.className = 'card-action integrator-bound-remove';
        remove.type = 'button';
        remove.textContent = '−';
        remove.title = 'Remove this row';
        remove.disabled = safeRows.length === 1 || (row.kind !== 'free' && boundCount === 1);
        remove.addEventListener('click', () => {
          commitVisibleBindingInputs();
          const nextRows = currentIntegratorRows();
          nextRows.splice(index, 1);
          if (!nextRows.length)
            nextRows.push({kind: 'bound', name: 'x', lo: '', hi: ''});
          if (nextRows.filter((entry) => entry.kind !== 'free').length === 0)
            nextRows.push({kind: 'bound', name: integratorDefaultVariableName(nextRows), lo: '', hi: ''});
          renderIntegratorRows(nextRows);
          refreshVariableValuesFromEditor();
          updateHistoryButtons();
          if (currentMode() === 'integrator')
            saveLastIntegratorState();
        });

        item.append(toggle, nameField, lowerField, upperField, add, remove);
        integratorBoundStack.appendChild(item);
      });
    }

    function applyIntegratorResultBound(data) {
      const responseBounds = Array.isArray(data && data.bounds) ? data.bounds : [];
      if (!responseBounds.length)
        return;
      const parameterNames = new Set(
        variableNamesFromBindings(data && data.binding_values)
          .map((name) => String(name || '').trim())
          .filter(Boolean)
      );
      const previousRows = currentIntegratorRows();
      const mergedRows = [];
      let boundIndex = 0;

      previousRows.forEach((row) => {
        if (row.kind === 'free') {
          if (parameterNames.has(String(row.name || '').trim()))
            mergedRows.push(row);
          return;
        }
        if (boundIndex < responseBounds.length)
          mergedRows.push(responseBounds[boundIndex++]);
      });

      while (boundIndex < responseBounds.length)
        mergedRows.push(responseBounds[boundIndex++]);

      renderIntegratorRows(mergedRows);
    }

    function restoreIntegratorBoundsText(text) {
      try {
        renderIntegratorRows(parseIntegratorBoundsText(text || DEFAULT_INTEGRATOR_BOUNDS_TEXT));
      } catch (_) {
        renderIntegratorRows(integratorFallbackRows());
      }
    }

    function currentIntegratorBoundsText() {
      return integratorBoundsTextFromRows(activeIntegratorRows());
    }

    function resetIntegratorBoundsToDefault() {
      restoreIntegratorBoundsText(DEFAULT_INTEGRATOR_BOUNDS_TEXT);
    }

    function resetIntegratorBoundsToBlank() {
      renderIntegratorRows(integratorBlankRows());
    }

    function requestedIntegratorIntervalCap() {
      const raw = parseInt(String(integratorIntervalCap && integratorIntervalCap.value || DEFAULT_INTEGRATOR_INTERVAL_CAP), 10);
      if (!Number.isFinite(raw))
        return DEFAULT_INTEGRATOR_INTERVAL_CAP;
      return raw;
    }

    async function fetchMatrixEvaluation() {
      saveLastMatrixState();
      const matrixText = currentExpressionText() || expr.value.trim();
      const response = await fetch('/matrix-eval', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify({
          matrix: matrixText,
          operation: matrixOperation.value,
          operand: matrixOperand.value.trim(),
          precision: requestedValuePrecision()
        })
      });
      const data = await response.json();
      return {response, data};
    }

    async function fetchEquationEvaluation() {
      saveLastEquationState();
      const equationText = currentExpressionText() || expr.value.trim();
      const response = await fetch('/equation-eval', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify({
          equation: equationText,
          precision: requestedValuePrecision()
        })
      });
      const data = await response.json();
      return {response, data};
    }

    async function fetchIntegratorEvaluation() {
      const expressionText = currentExpressionText() || expr.value.trim();
      const rows = currentIntegratorRows();
      const activeRows = activeIntegratorRows(rows, expressionText);
      const bounds = activeIntegratorBoundRows(rows, expressionText);
      renderIntegratorRows(activeRows);
      bounds.forEach((bound) => {
        if (bound.lo && !bound.hi)
          throw new Error(`A one-sided bound for ${bound.name} should be entered as an upper bound. Leave lower blank and put the value in upper.`);
      });
      saveLastIntegratorState();
      const response = await fetch('/integrator-eval', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify({
          expression: expressionText,
          bounds,
          precision: requestedValuePrecision(),
          max_intervals: requestedIntegratorIntervalCap()
        })
      });
      const data = await response.json();
      return {response, data};
    }

    async function fetchDatetimeEvaluation() {
      const state = currentDatetimeState();
      saveLastDatetimeState();
      const response = await fetch('/datetime-eval', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify(state)
      });
      const raw = await response.text();
      let data;
      try {
        data = raw ? JSON.parse(raw) : {};
      } catch (_) {
        throw new Error(raw || `Datetime request failed with HTTP ${response.status}`);
      }
      return {response, data};
    }

    async function fetchAlmanacEvaluation() {
      const state = currentAlmanacState();
      saveLastAlmanacState();
      const response = await fetch('/almanac-eval', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify(state)
      });
      const raw = await response.text();
      let data;
      try {
        data = raw ? JSON.parse(raw) : {};
      } catch (_) {
        throw new Error(raw || `Almanac request failed with HTTP ${response.status}`);
      }
      return {response, data};
    }

    function escapeHtml(text) {
      return String(text || '')
        .replace(/&/g, '&amp;')
        .replace(/</g, '&lt;')
        .replace(/>/g, '&gt;')
        .replace(/"/g, '&quot;')
        .replace(/'/g, '&#39;');
    }

    function renderAlmanacWorksheet(target, data) {
      const rows = Array.isArray(data.rows) ? data.rows : [];
      const worksheetTitle = escapeHtml(data.worksheet_title || 'AstroNav 2000-2040 Navigation Almanac');
      const momentText = escapeHtml(data.moment_text || '');
      const observerText = escapeHtml(data.observer_text || '');
      const bodyText = escapeHtml(data.body_text || '');
      const rangeText = escapeHtml(data.range_text || '');
      target.innerHTML = `
        <div class="almanac-sheet">
          <div class="almanac-sheet-header">
            <div class="almanac-sheet-title">${worksheetTitle}</div>
            <div>${momentText}</div>
            <div>${observerText}</div>
            <div>${bodyText}</div>
            <div class="almanac-sheet-note">${rangeText}</div>
          </div>
          <table class="almanac-grid-table">
            <thead>
              <tr>
                <th>Body</th>
                <th>Kind</th>
                <th>Declination</th>
                <th>GHA</th>
                <th>SHA</th>
                <th>LHA</th>
                <th>RA</th>
              </tr>
            </thead>
            <tbody>
              ${rows.map((row) => {
                const classes = [
                  row.code === data.selected_code ? 'selected' : '',
                  row.kind === 'reference' ? 'reference' : ''
                ].filter(Boolean).join(' ');
                const nameClass = row.kind === 'reference' ? 'reference-name' : 'body-name';
                return `
                  <tr class="${classes}">
                    <td class="${nameClass}">${escapeHtml(row.name || row.code || '')}</td>
                    <td>${escapeHtml(row.kind || '')}</td>
                    <td class="number">${escapeHtml(row.declination || '')}</td>
                    <td class="number">${escapeHtml(row.gha || '')}</td>
                    <td class="number">${escapeHtml(row.sha || '')}</td>
                    <td class="number">${escapeHtml(row.lha || '')}</td>
                    <td class="number">${escapeHtml(row.right_ascension || '')}</td>
                  </tr>`;
              }).join('')}
            </tbody>
          </table>
        </div>`;
    }

    async function refreshDatetimeLocalHolidays() {
      if (currentMode() !== 'datetime')
        return;

      const refreshId = ++datetimeLocalRefreshSequence;
      setStatus('Refreshing holidays...');
      try {
        const {response, data} = await fetchDatetimeEvaluation();
        if (refreshId !== datetimeLocalRefreshSequence || currentMode() !== 'datetime')
          return;
        if (!response.ok || !data.ok) {
          setStatus('Error');
          return;
        }
        setDatetimeLocalText(data.local || '', data.local_sections || []);
        setStatus('Ready');
      } catch (_) {
        if (refreshId !== datetimeLocalRefreshSequence || currentMode() !== 'datetime')
          return;
        setStatus('Error');
      }
    }

    async function refreshDatetimeJurisdictionLocation() {
      if (!datetimeJurisdiction)
        return;

      const jurisdiction = validDatetimeJurisdiction(datetimeJurisdiction.value, DEFAULT_DATETIME_JURISDICTION);
      const date = validDateText(datetimeDate && datetimeDate.value, DEFAULT_DATETIME_DATE);
      try {
        const response = await fetch('/datetime-jurisdiction-location', {
          method: 'POST',
          headers: {'Content-Type': 'application/json'},
          body: JSON.stringify({jurisdiction, date})
        });
        const data = await response.json();
        if (!response.ok || !data.ok)
          return;
        if (datetimeLatitude && data.latitude)
          datetimeLatitude.value = String(data.latitude);
        if (datetimeLongitude && data.longitude)
          datetimeLongitude.value = String(data.longitude);
        if (datetimeGmtOffset) {
          const currentOffset = String(datetimeGmtOffset.value || '').trim();
          const suggestedOffset = String(data.gmt_offset || '').trim();
          if (!datetimeGmtOffsetTouched || !currentOffset || currentOffset === datetimeAutoGmtOffset) {
            datetimeGmtOffset.value = suggestedOffset;
            datetimeAutoGmtOffset = suggestedOffset;
            datetimeGmtOffsetTouched = false;
          }
        }
      } catch (_) {
        // Keep the current location if the helper is unavailable.
      }
    }

    async function triggerDatetimeAutoEvaluation({refreshJurisdiction = false} = {}) {
      if (currentMode() !== 'datetime')
        return;
      if (refreshJurisdiction)
        await refreshDatetimeJurisdictionLocation();
      saveLastDatetimeState();
      await evaluateDatetime({skipHistoryUpdate: true});
      updateHistoryButtons();
    }

    function estimateValuePrecision() {
      const style = getComputedStyle(value);
      const canvas = estimateValuePrecision.canvas || document.createElement('canvas');
      const context = canvas.getContext('2d');
      const padLeft = parseFloat(style.paddingLeft) || 0;
      const padRight = parseFloat(style.paddingRight) || 0;
      let charWidth = 9;

      estimateValuePrecision.canvas = canvas;
      if (context) {
        context.font = style.font;
        charWidth = context.measureText('0123456789'.repeat(8)).width / 80 || charWidth;
      }

      const usableWidth = Math.max(0, value.clientWidth - padLeft - padRight);
      const chars = Math.floor(usableWidth / charWidth);

      return Math.max(96, Math.min(220, chars - 3));
    }

    function requestedValuePrecision() {
      return precisionDigitsForBits(requestedPrecisionBits());
    }

    function atMinimumPrecision() {
      return requestedPrecisionBits() <= DOUBLE_PRECISION_BITS;
    }

    function atMaximumPrecision() {
      return requestedPrecisionBits() >= MAX_PRECISION_BITS;
    }

    function setRequestedPrecisionBits(bits) {
      const mode = currentMode();
      const clamped = Math.max(DOUBLE_PRECISION_BITS, Math.min(MAX_PRECISION_BITS, bits));
      modePrecisionBits[mode] = clamped;
      workingPrecisionBits = clamped;
    }

    function nextPrecisionStepBits(current) {
      if (current < QFLOAT_PRECISION_BITS)
        return QFLOAT_PRECISION_BITS;
      if (current < 256)
        return 256;
      return Math.min(MAX_PRECISION_BITS, Math.ceil((current + 1) / 128) * 128);
    }

    function previousPrecisionStepBits(current) {
      if (current <= QFLOAT_PRECISION_BITS)
        return DOUBLE_PRECISION_BITS;
      if (current <= 256)
        return QFLOAT_PRECISION_BITS;
      return Math.max(256, Math.floor((current - 1) / 128) * 128);
    }

    function copyTextForTarget(target) {
      if (target === 'rendered') return rendered.classList.contains('error') ? rendered.textContent : lastTex;
      if (target === 'expression') return parsedExpressionText();
      if (target === 'function') return functionStyle.dataset.fullText || functionStyle.textContent;
      if (target === 'value') return value.textContent;
      if (target === 'mobile') {
        const url = mobileUrl ? mobileUrl.textContent.trim() : '';
        return /^https?:\/\//.test(url) ? url : '';
      }
      return '';
    }

    function parsedExpressionText() {
      return expressionForEditor(
        parsed.dataset.fullText ||
        parsed.dataset.displayText ||
        parsed.textContent ||
        ''
      ).trim();
    }

    function setResultInputText(text) {
      const inputText = expressionForEditor(String(text || '')).trim();
      resultUseInput.dataset.inputText = inputText;
      resultUseInput.disabled = !inputText;
      resultUseInput.classList.toggle('hidden', !inputText);
      resultUseInput.title = inputText
        ? 'Send this result to the input pane'
        : 'No reusable result is available';
    }

    function resultExpressionTextForInput() {
      return (resultUseInput.dataset.inputText || parsedExpressionText()).trim();
    }

    function sendResultExpressionToInput() {
      const resultText = resultExpressionTextForInput();
      if (!resultText)
        return;

      const current = historyStateForMode();
      const next = historyStateForMode(currentMode(), resultText);
      if (current.text && !historyStatesEqual(current, next))
        pushExpressionHistory(current);

      clearGoalSeekRequest();
      hideTargetEntry();
      applyUpdatedBindingExpression(resultText);
      saveCurrentModeEditorState();
      updateHistoryButtons();
      expr.focus();
      setStatus('Result sent to input');
    }

    async function refreshMobileAccess() {
      if (!mobileAccess || !mobileUrl || !mobileQr)
        return;

      try {
        const headers = controlToken ? {'X-Dval-Lab-Control': controlToken} : {};
        const response = await fetch('/mobile-access', {cache: 'no-store', headers});
        if (!response.ok)
          return;
        const data = await response.json();
        const url = String(data.url || '');
        const canControl = Boolean(data.control);
        mobileAccess.classList.remove('hidden');
        if (mobileTitle)
          mobileTitle.textContent = String(data.title || 'Mobile access');
        if (mobileHint)
          mobileHint.textContent = String(data.hint || '');
        mobileUrl.textContent = url || 'Unavailable';
        mobileQr.innerHTML = String(data.qr || '');
      } catch (err) {
        // Network state changes are expected; keep the last known QR until the next poll.
      }
    }

    function resetMoreDigitsButton(button, canExpand) {
      button.classList.toggle('hidden', !canExpand);
      button.textContent = 'Show more digits';
      button.dataset.expanded = 'false';
    }

    function hasAbbreviatedValue(text) {
      return String(text || '').includes('...');
    }

    function resultZoomIndex(card) {
      const raw = Number(card && card.dataset.zoomIndex);
      if (Number.isFinite(raw))
        return Math.max(0, Math.min(RESULT_ZOOM_LEVELS.length - 1, Math.round(raw)));
      return RESULT_ZOOM_DEFAULT_INDEX;
    }

    function svgLengthPixels(value) {
      const match = String(value || '').trim().match(/^([+-]?(?:\d+(?:\.\d*)?|\.\d+))\s*(px|pt|pc|in|cm|mm)?$/i);
      if (!match)
        return 0;

      const amount = Number.parseFloat(match[1]);
      if (!Number.isFinite(amount))
        return 0;

      const unit = String(match[2] || 'px').toLowerCase();
      if (unit === 'pt') return amount * 96 / 72;
      if (unit === 'pc') return amount * 16;
      if (unit === 'in') return amount * 96;
      if (unit === 'cm') return amount * 96 / 2.54;
      if (unit === 'mm') return amount * 96 / 25.4;
      return amount;
    }

    function svgIntrinsicSize(svg) {
      const attrWidth = svgLengthPixels(svg.getAttribute('width'));
      const attrHeight = svgLengthPixels(svg.getAttribute('height'));
      let viewWidth = 0;
      let viewHeight = 0;
      if (svg.viewBox && svg.viewBox.baseVal) {
        viewWidth = svg.viewBox.baseVal.width;
        viewHeight = svg.viewBox.baseVal.height;
      }

      const previousTransform = svg.style.transform;
      svg.style.transform = 'none';
      const rect = svg.getBoundingClientRect();
      svg.style.transform = previousTransform;

      return {
        width: Math.max(1, attrWidth || rect.width || viewWidth || 1),
        height: Math.max(1, attrHeight || rect.height || viewHeight || 1)
      };
    }

    function updateRenderedZoomFrame(card, scale) {
      const frame = card ? card.querySelector('.rendered-zoom-frame') : null;
      const svg = frame ? frame.querySelector('svg') : null;
      if (!frame || !svg)
        return;

      if (!svg.dataset.baseWidth || !svg.dataset.baseHeight) {
        const intrinsic = svgIntrinsicSize(svg);
        svg.dataset.baseWidth = String(intrinsic.width);
        svg.dataset.baseHeight = String(intrinsic.height);
      }

      const width = Number(svg.dataset.baseWidth) || 1;
      const height = Number(svg.dataset.baseHeight) || 1;
      frame.style.width = `${width * scale}px`;
      frame.style.height = `${height * scale}px`;
      svg.style.width = `${width}px`;
      svg.style.height = `${height}px`;
      svg.style.transform = `scale(${scale})`;
    }

    function applyResultZoom(card) {
      if (!card)
        return;

      const index = resultZoomIndex(card);
      const zoom = RESULT_ZOOM_LEVELS[index];
      const computed = getComputedStyle(card);
      const renderBase = Number.parseFloat(computed.getPropertyValue('--render-base-scale')) || 2;
      const textBase = Number.parseFloat(computed.getPropertyValue('--result-base-font-rem')) || 0.92;
      const renderFontBase = Number.parseFloat(computed.getPropertyValue('--render-base-font-rem')) || 1.78;
      const marginBase = Number.parseFloat(computed.getPropertyValue('--render-base-margin-rem')) || 5;
      const renderScale = renderBase * zoom;

      card.dataset.zoomIndex = String(index);
      card.style.setProperty('--result-zoom', String(zoom));
      card.style.setProperty('--result-font-size', `${textBase * zoom}rem`);
      card.style.setProperty('--render-font-size', `${renderFontBase * zoom}rem`);
      card.style.setProperty('--render-zoom', String(renderScale));
      card.style.setProperty('--render-margin-bottom', `${marginBase * Math.max(1, zoom)}rem`);
      updateRenderedZoomFrame(card, renderScale);
      card.querySelectorAll('[data-zoom-reset]').forEach((button) => {
        button.textContent = `${Math.round(zoom * 100)}%`;
        button.setAttribute('aria-label', `Reset zoom from ${Math.round(zoom * 100)}%`);
      });
      card.querySelectorAll('[data-zoom-step="-1"]').forEach((button) => {
        button.disabled = index <= 0;
      });
      card.querySelectorAll('[data-zoom-step="1"]').forEach((button) => {
        button.disabled = index >= RESULT_ZOOM_LEVELS.length - 1;
      });
    }

    function setResultZoom(card, index) {
      if (!card)
        return;
      card.dataset.zoomIndex = String(Math.max(0, Math.min(RESULT_ZOOM_LEVELS.length - 1, index)));
      applyResultZoom(card);
    }

    function stepResultZoom(card, direction) {
      setResultZoom(card, resultZoomIndex(card) + (direction < 0 ? -1 : 1));
    }

    function collapseResultCards() {
      resultPane.classList.remove('card-expanded');
      document.querySelectorAll('.result-card.expanded-card')
        .forEach((card) => card.classList.remove('expanded-card'));
      expandCardButtons.forEach((button) => {
        button.textContent = 'Expand';
        button.setAttribute('aria-expanded', 'false');
      });
    }

    function toggleResultCardExpansion(button) {
      const card = button.closest('.result-card');
      if (!card)
        return;

      const isExpanded = card.classList.contains('expanded-card');
      collapseResultCards();
      if (isExpanded)
        return;

      resultPane.classList.add('card-expanded');
      card.classList.add('expanded-card');
      button.textContent = 'Collapse';
      button.setAttribute('aria-expanded', 'true');
    }

    function setRenderedContent(svg, fallbackText = '') {
      const card = rendered.closest('.result-card');
      rendered.replaceChildren();
      if (svg) {
        const frame = document.createElement('div');
        frame.className = 'rendered-zoom-frame';
        frame.innerHTML = svg;
        rendered.appendChild(frame);
      } else {
        rendered.textContent = fallbackText;
      }
      if (card)
        requestAnimationFrame(() => applyResultZoom(card));
    }

    function clearRenderedError() {
      rendered.classList.remove('error');
      rendered.style.color = '';
      rendered.style.background = '';
      rendered.style.borderColor = '';
      rendered.style.boxShadow = '';
      rendered.style.textShadow = '';
      rendered.style.fontFamily = '';
    }

    function setRenderedError(message) {
      rendered.replaceChildren();
      rendered.textContent = message || 'Evaluation failed';
      rendered.classList.add('error');
      rendered.style.color = '#ffd99a';
      rendered.style.background =
        'radial-gradient(circle at 14% 18%, rgba(229, 173, 87, 0.16), transparent 34%), ' +
        'linear-gradient(135deg, rgba(73, 23, 25, 0.88), rgba(38, 12, 19, 0.78))';
      rendered.style.borderColor = 'rgba(229, 173, 87, 0.42)';
      rendered.style.boxShadow =
        'inset 0 0 0 1px rgba(255, 232, 181, 0.07), 0 0 1.35rem rgba(153, 27, 27, 0.22)';
      rendered.style.textShadow = '0 0 0.7rem rgba(255, 204, 112, 0.16)';
      rendered.style.fontFamily = 'Georgia, "Times New Roman", serif';
    }

    function renderMatrixSectionHeadings(element, text) {
      const source = String(text || '');
      const lines = source.split('\n');
      const hasHeadings = lines.some((line) => /^(?:\s*)(eigenvalues|eigenvectors)(?:\s*)$/i.test(line));
      if (!hasHeadings) {
        element.textContent = source;
        return;
      }

      element.replaceChildren();
      lines.forEach((line, index) => {
        const match = line.match(/^(\s*)(eigenvalues|eigenvectors)(\s*)$/i);
        if (match) {
          element.appendChild(document.createTextNode(match[1]));
          const heading = document.createElement('span');
          heading.className = 'matrix-section-heading';
          heading.textContent = match[2].toLowerCase();
          element.appendChild(heading);
          element.appendChild(document.createTextNode(match[3]));
        } else {
          element.appendChild(document.createTextNode(line));
        }
        if (index + 1 < lines.length)
          element.appendChild(document.createTextNode('\n'));
      });
    }

    function setExpandableText(element, button, displayText, fullText) {
      renderMatrixSectionHeadings(element, displayText || fullText || '');
      element.dataset.displayText = displayText || '';
      element.dataset.fullText = fullText || '';
      resetMoreDigitsButton(
        button,
        !!fullText && !!displayText && fullText !== displayText && hasAbbreviatedValue(displayText)
      );
    }

    function renderDatetimeSections(element, button, sections, fallbackText = '') {
      const text = String(fallbackText || '').trim();
      const items = Array.isArray(sections) ? sections : [];

      element.replaceChildren();
      element.dataset.displayText = text;
      element.dataset.fullText = text;
      if (button)
        resetMoreDigitsButton(button, false);

      if (!items.length) {
        element.textContent = text;
        return;
      }

      const grid = document.createElement('div');
      grid.className = 'datetime-section-grid';
      items.forEach((section) => {
        const rows = Array.isArray(section && section.rows) ? section.rows : [];
        if (!rows.length)
          return;

        const details = document.createElement('details');
        details.className = 'datetime-section';
        if (section.open !== false)
          details.open = true;

        const summary = document.createElement('summary');
        summary.textContent = String(section.title || 'Calendar');
        details.appendChild(summary);

        const body = document.createElement('div');
        body.className = 'datetime-section-rows';
        rows.forEach((row) => {
          const labelText = String(row && row.label || '').trim();
          const valueText = String(row && row.value || '').trim();
          if (!labelText && !valueText)
            return;

          const line = document.createElement('div');
          line.className = 'datetime-row';
          const label = document.createElement('span');
          label.className = 'datetime-row-label';
          label.textContent = labelText;
          const value = document.createElement('span');
          value.className = 'datetime-row-value';
          value.textContent = valueText || 'unavailable';
          line.append(label, value);
          body.appendChild(line);
        });

        details.appendChild(body);
        grid.appendChild(details);
      });

      if (grid.childElementCount)
        element.appendChild(grid);
      else
        element.textContent = text;
    }

    function parseMatrixResultText(text) {
      const source = String(text || '').trim();
      if (!source.startsWith('(') || !source.endsWith(')'))
        return null;

      const body = source.slice(1, -1).trim();
      if (!body)
        return [[]];

      const rows = splitTopLevel(body, ';')
        .map((row) => splitTopLevel(row, ',').map((cell) => cell.trim()));
      if (!rows.length)
        return null;

      const cols = rows[0].length;
      if (!cols || rows.some((row) => row.length !== cols))
        return null;
      return rows;
    }

    function setMatrixPrettyResult(resultText, prettyText) {
      const rows = parseMatrixResultText(resultText);
      functionStyle.classList.add('matrix-pretty');
      functionStyle.dataset.displayText = prettyText || resultText || '';
      functionStyle.dataset.fullText = prettyText || resultText || '';
      resetMoreDigitsButton(functionMore, false);

      if (!rows) {
        renderMatrixSectionHeadings(functionStyle, prettyText || resultText || '');
        return;
      }

      functionStyle.replaceChildren();
      const display = document.createElement('span');
      display.className = 'matrix-display';

      const left = document.createElement('span');
      left.className = 'matrix-bracket';
      left.textContent = '(';
      display.appendChild(left);

      const grid = document.createElement('span');
      grid.className = 'matrix-grid';
      grid.style.gridTemplateColumns = `repeat(${rows[0].length}, max-content)`;
      rows.forEach((row) => {
        row.forEach((cellText) => {
          const cell = document.createElement('span');
          cell.className = 'matrix-cell';
          cell.textContent = cellText;
          grid.appendChild(cell);
        });
      });
      display.appendChild(grid);

      const right = document.createElement('span');
      right.className = 'matrix-bracket';
      right.textContent = ')';
      display.appendChild(right);

      functionStyle.appendChild(display);
    }

    function setRenderedResult(data) {
      const displayTex = data.display_tex || data.tex || '';
      const fullDisplayTex = data.full_display_tex || data.tex || '';

      clearRenderedError();
      lastTex = data.tex || '';
      rendered.dataset.displayTex = displayTex;
      rendered.dataset.fullTex = fullDisplayTex;
      rendered.dataset.displaySvg = data.svg || '';
      rendered.dataset.fullSvg = '';
      rendered.dataset.renderError = data.render_error || '';
      setRenderedContent(data.svg || '', data.render_error || 'No rendered TeX available');
      resetMoreDigitsButton(
        renderedMore,
        !!fullDisplayTex &&
          !!displayTex &&
          fullDisplayTex !== displayTex &&
          hasAbbreviatedValue(displayTex)
      );
    }

    async function renderTexSvg(tex) {
      const response = await fetch('/render_tex', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify({tex})
      });
      const data = await response.json();
      if (!response.ok || !data.ok)
        throw new Error(data.error || 'Could not render TeX');
      return data;
    }

    function toggleTextDigits(element, button) {
      const expanded = button.dataset.expanded === 'true';
      if (expanded) {
        renderMatrixSectionHeadings(element, element.dataset.displayText || element.textContent);
        button.textContent = 'Show more digits';
        button.dataset.expanded = 'false';
      } else {
        renderMatrixSectionHeadings(element, element.dataset.fullText || element.textContent);
        button.textContent = 'Show fewer digits';
        button.dataset.expanded = 'true';
      }
    }

    async function toggleRenderedDigits() {
      const expanded = renderedMore.dataset.expanded === 'true';

      if (expanded) {
        setRenderedContent(rendered.dataset.displaySvg || '', rendered.dataset.renderError || '');
        renderedMore.textContent = 'Show more digits';
        renderedMore.dataset.expanded = 'false';
        return;
      }

      if (!rendered.dataset.fullSvg) {
        renderedMore.disabled = true;
        setStatus('Rendering full TeX...');
        try {
          const data = await renderTexSvg(rendered.dataset.fullTex || lastTex);
          rendered.dataset.fullSvg = data.svg || '';
          rendered.dataset.fullRenderError = data.render_error || '';
        } catch (err) {
          rendered.dataset.fullRenderError = String(err);
        } finally {
          renderedMore.disabled = false;
          setStatus('Ready');
        }
      }

      setRenderedContent(
        rendered.dataset.fullSvg || '',
        rendered.dataset.fullRenderError || 'No rendered TeX available'
      );
      renderedMore.textContent = 'Show fewer digits';
      renderedMore.dataset.expanded = 'true';
    }

    async function writeClipboardText(text) {
      if (navigator.clipboard && window.isSecureContext) {
        await navigator.clipboard.writeText(text);
        return;
      }

      const area = document.createElement('textarea');
      area.value = text;
      area.setAttribute('readonly', '');
      area.style.position = 'fixed';
      area.style.left = '-9999px';
      area.style.top = '0';
      document.body.appendChild(area);
      area.select();
      const ok = document.execCommand('copy');
      document.body.removeChild(area);
      if (!ok)
        throw new Error('Copy was blocked by the browser');
    }

    function flashCopyButton(button, ok) {
      const original = button.dataset.originalLabel || button.textContent;
      button.dataset.originalLabel = original;
      button.classList.remove('copied', 'copy-failed');
      button.classList.add(ok ? 'copied' : 'copy-failed');
      button.textContent = ok ? 'Copied' : 'Failed';

      clearTimeout(button.copyResetTimer);
      button.copyResetTimer = setTimeout(() => {
        button.textContent = original;
        button.classList.remove('copied', 'copy-failed');
      }, 1200);
    }

    function clearResultPane() {
      collapseResultCards();
      rendered.replaceChildren();
      rendered.textContent = '';
      clearRenderedError();
      setDatetimeLocalText('');
      resetMoreDigitsButton(renderedMore, false);
      clearResultDetails();
    }

    function clearResultDetails(options = {}) {
      parsed.classList.remove('matrix-pretty');
      functionStyle.classList.remove('matrix-pretty');
      value.classList.remove('matrix-pretty');
      parsed.textContent = '';
      functionStyle.textContent = '';
      resetMoreDigitsButton(parsedMore, false);
      resetMoreDigitsButton(functionMore, false);
      delete parsed.dataset.fullText;
      delete parsed.dataset.displayText;
      delete functionStyle.dataset.fullText;
      delete functionStyle.dataset.displayText;
      setResultInputText('');
      value.textContent = '';
      lastTex = '';
      lastDerivativeExpression = '';
      currentVariables = [];
      currentDifferentiable = true;
      renderDerivativeButtons(currentVariables);
      if (!options.keepBindings)
        clearVariableValues();
    }

    async function evaluateExpression(options = {}) {
      commitVisibleBindingInputs();
      const editorText = currentExpressionText();
      const editorBodyText = String(expr.value || '').trim();
      const text = options.reuseLastInput && lastEvaluationInputText
        ? lastEvaluationInputText
        : editorText;
      const nextState = historyStateForMode(currentMode(), text);
      const previousState = !options.skipHistoryUpdate
        ? previousModeStateForHistory(nextState)
        : null;
      if (!text) return;
      showResults();
      setBusy(true);
      setStatus('Evaluating...');
      try {
        if (previousState)
          pushExpressionHistory(previousState);
        const {response, data} = await fetchEvaluation(text);

        if (!response.ok || !data.ok) {
          setRenderedError(data.error || 'Evaluation failed');
          resetMoreDigitsButton(renderedMore, false);
          clearResultDetails({keepBindings: true});
          commitModeState();
          setStatus('Error');
          return;
        }

        if (data.partial_error) {
          setRenderedError(data.error || 'Evaluation failed');
          resetMoreDigitsButton(renderedMore, false);
        } else {
          setRenderedResult(data);
        }
        if (data.expression && !data.partial_error)
          setExpressionEditor(
            data.expression || editorText || text,
            data.binding_values || null,
            editorBodyText || null
          );
        else if (data.binding_values)
          renderVariableValues(data.binding_values || []);
        setExpandableText(
          parsed,
          parsedMore,
          data.display_expression || (data.expression ? compactExpressionForEditor(data.expression).display : ''),
          data.full_display_expression || data.expression || ''
        );
        setResultInputText(data.full_display_expression || data.expression || '');
        setExpandableText(
          functionStyle,
          functionMore,
          data.display_function || data.function || '',
          data.full_display_function || data.function || ''
        );
        value.textContent = data.value_note
          ? `${data.value || ''}\n${data.value_note}`
          : (data.value || '');
        lastEvaluationInputText = data.partial_error ? text : (data.expression || text);
        if (!data.partial_error)
          saveLastExpression(editorText || fullExpressionText || expr.value.trim());
        lastDerivativeExpression = derivativeExpressionFromLine(data.derivative);
        {
          const variableBindings = variableNamesFromBindings(data.binding_values || []);
          currentVariables = variableBindings.length
            ? variableBindings
            : variablesFromExpression(data.expression || '');
        }
        currentDifferentiable = String(data.differentiable || 'yes').trim().toLowerCase() !== 'no';
        renderDerivativeButtons(currentVariables);
        commitModeState();
        setStatus(data.partial_error ? 'Error' : 'Ready');
      } catch (err) {
        setRenderedError(String(err));
        resetMoreDigitsButton(renderedMore, false);
        clearResultDetails({keepBindings: true});
        commitModeState();
        setStatus('Error');
      } finally {
        setBusy(false);
        if (!options.skipHistoryUpdate)
          updateHistoryButtons();
      }
    }

    async function evaluateMatrix(options = {}) {
      commitVisibleBindingInputs();
      const text = currentExpressionText() || expr.value.trim();
      const nextState = historyStateForMode(currentMode(), text);
      const previousState = !options.skipHistoryUpdate
        ? previousModeStateForHistory(nextState)
        : null;
      if (!text)
        return;
      showResults();
      setBusy(true);
      setStatus('Evaluating matrix...');
      try {
        if (previousState)
          pushExpressionHistory(previousState);
        const {response, data} = await fetchMatrixEvaluation();
        if (!response.ok || !data.ok) {
          setRenderedError(data.error || 'Matrix evaluation failed');
          resetMoreDigitsButton(renderedMore, false);
          clearResultDetails({keepBindings: true});
          commitModeState();
          setStatus('Error');
          return;
        }

        clearResultDetails({keepBindings: true});
        clearRenderedError();
        lastTex = data.tex || '';
        rendered.dataset.displayTex = data.tex || '';
        rendered.dataset.fullTex = data.tex || '';
        rendered.dataset.displaySvg = data.svg || '';
        rendered.dataset.fullSvg = '';
        rendered.dataset.renderError = data.render_error || '';
        setRenderedContent(
          data.svg || '',
          data.render_error || (data.tex || 'No rendered TeX available')
        );
        resetMoreDigitsButton(renderedMore, false);
        setExpandableText(parsed, parsedMore, data.result || '', data.result || '');
        setResultInputText(data.result || '');
        setMatrixPrettyResult(data.result || '', data.pretty || '');
        value.textContent = '';
        setValueCardVisible(false);
        if (Array.isArray(data.binding_values))
          renderVariableValues(data.binding_values);
        else
          clearVariableValues();
        modeEditorText.matrix = text;
        saveLastMatrixState();
        currentVariables = [];
        currentDifferentiable = false;
        renderDerivativeButtons(currentVariables);
        commitModeState();
        setStatus('Ready');
      } catch (err) {
        setRenderedError(String(err));
        resetMoreDigitsButton(renderedMore, false);
        clearResultDetails({keepBindings: true});
        commitModeState();
        setStatus('Error');
      } finally {
        setBusy(false);
        if (!options.skipHistoryUpdate)
          updateHistoryButtons();
      }
    }

    async function evaluateEquation(options = {}) {
      commitVisibleBindingInputs();
      const text = String(currentExpressionText() || expr.value || '').trim();
      const nextState = historyStateForMode(currentMode(), text);
      const previousState = !options.skipHistoryUpdate
        ? previousModeStateForHistory(nextState)
        : null;
      if (!text)
        return;
      showResults();
      setBusy(true);
      setStatus('Solving equation...');
      try {
        if (previousState)
          pushExpressionHistory(previousState);
        const {response, data} = await fetchEquationEvaluation();
        if (!response.ok || !data.ok) {
          setRenderedError(data.error || 'Equation solving failed');
          resetMoreDigitsButton(renderedMore, false);
          clearResultDetails({keepBindings: true});
          commitModeState();
          setStatus('Error');
          return;
        }

        clearResultDetails({keepBindings: true});
        clearRenderedError();
        lastTex = data.tex || '';
        rendered.dataset.displayTex = data.display_tex || data.tex || '';
        rendered.dataset.fullTex = data.full_display_tex || data.tex || '';
        rendered.dataset.displaySvg = data.svg || '';
        rendered.dataset.fullSvg = '';
        rendered.dataset.renderError = data.render_error || '';
        setRenderedContent(data.svg || '', data.render_error || (data.tex || 'No rendered TeX available'));
        resetMoreDigitsButton(
          renderedMore,
          !!data.full_display_tex &&
            !!data.display_tex &&
            data.full_display_tex !== data.display_tex &&
            hasAbbreviatedValue(data.display_tex)
        );
        setExpandableText(
          parsed,
          parsedMore,
          data.display_equation || data.equation || '',
          data.full_display_equation || data.equation || ''
        );
        setResultInputText(data.full_display_equation || data.equation || '');
        {
          const valueLines = [];
          const residualValue = displayEquationValue(data.error);
          const solutionLines = String(data.solutions || '')
            .split('\n')
            .map((line) => line.trim())
            .filter(Boolean);
          const numericSolutionLines = Array.isArray(data.numeric_solutions)
            ? data.numeric_solutions.map((line) => String(line).trim()).filter(Boolean)
            : [];
          const combinedSolutionLines = [...solutionLines];
          if (numericSolutionLines.length) {
            if (combinedSolutionLines.length)
              combinedSolutionLines.push('');
            numericSolutionLines.forEach((line) => combinedSolutionLines.push(`numeric: ${line}`));
          }
          setExpandableText(
            functionStyle,
            functionMore,
            combinedSolutionLines.join('\n') || data.status || '',
            combinedSolutionLines.join('\n') || data.status || ''
          );
          if (solutionLines.length === 1)
            valueLines.push(`solution: ${solutionLines[0]}`);
          numericSolutionLines.forEach((line) => valueLines.push(`numeric: ${line}`));
          if (data.status)
            valueLines.push(`solve status: ${data.status}`);
          if (Number.isFinite(Number(data.solution_count)) && Number(data.solution_count) > 0)
            valueLines.push(`solutions found: ${data.solution_count}`);
          if (data.residual)
            valueLines.push(`residual expression: ${data.residual}`);
          if (data.error && residualValue !== 'unresolved')
            valueLines.push(`residual value: ${residualValue}`);
          value.textContent = valueLines.join('\n');
        }
        if (Array.isArray(data.binding_values))
          renderVariableValues(data.binding_values);
        else
          clearVariableValues();
        modeEditorText.equation = text;
        saveLastEquationState();
        currentVariables = [];
        currentDifferentiable = false;
        renderDerivativeButtons(currentVariables);
        commitModeState();
        setStatus('Ready');
      } catch (err) {
        setRenderedError(String(err));
        resetMoreDigitsButton(renderedMore, false);
        clearResultDetails({keepBindings: true});
        commitModeState();
        setStatus('Error');
      } finally {
        setBusy(false);
        if (!options.skipHistoryUpdate)
          updateHistoryButtons();
      }
    }

    async function evaluateIntegrator(options = {}) {
      commitVisibleBindingInputs();
      const text = currentExpressionText() || expr.value.trim();
      const nextState = historyStateForMode(currentMode(), text);
      const previousState = !options.skipHistoryUpdate
        ? previousModeStateForHistory(nextState)
        : null;
      if (!text)
        return;
      showResults();
      setBusy(true);
      setStatus('Integrating...');
      try {
        if (previousState)
          pushExpressionHistory(previousState);
        const {response, data} = await fetchIntegratorEvaluation();
        if (!response.ok || !data.ok) {
          const errorText = String(data.error || '').trim();
          const rawError = String(data.raw_error || '').trim();
          setRenderedError(
            errorText && errorText !== 'Integration failed'
              ? errorText
              : (rawError || errorText || 'Integration failed')
          );
          resetMoreDigitsButton(renderedMore, false);
          clearResultDetails({keepBindings: true});
          applyIntegratorBindingState(data, text);
          applyIntegratorResultBound(data);
          saveLastIntegratorState();
          commitModeState();
          setStatus('Error');
          return;
        }

        clearResultDetails({keepBindings: true});
        clearRenderedError();
        lastTex = data.tex || '';
        rendered.dataset.displayTex = data.tex || '';
        rendered.dataset.fullTex = data.tex || '';
        rendered.dataset.displaySvg = data.svg || '';
        rendered.dataset.fullSvg = '';
        rendered.dataset.renderError = data.render_error || '';
        setRenderedContent(
          data.svg || '',
          data.render_error || (data.tex || 'No rendered TeX available')
        );
        resetMoreDigitsButton(renderedMore, false);
        setExpandableText(parsed, parsedMore, data.expression || '', data.expression || '');
        setResultInputText(data.antiderivative || '');
        const workUnits = data.work_units || data.intervals || '';
        const workCap = data.work_cap || data.max_intervals || '';
        const statusText = String(data.status || '');
        const symbolicStatus = /symbolic|antiderivative|closed-form|fast path/i.test(statusText);
        const antiderivativeStatus = /antiderivative/i.test(statusText);
        const stoppedEarly = !symbolicStatus && workUnits && workCap && String(workUnits) !== String(workCap);
        const workText = workUnits && workCap
          ? `work used: ${workUnits} / ${workCap}${stoppedEarly ? ' (precision reached)' : ''}`
          : (workUnits ? `work used: ${workUnits}` : '');
        const detailLines = [];
        if (data.antiderivative)
          detailLines.push(`Antiderivative:\n${data.antiderivative}`);
        if (data.symbolic && !antiderivativeStatus)
          detailLines.push(`Definite result:\n${data.symbolic}`);
        const domainText = [data.bound, data.status ? `status: ${data.status}` : '', symbolicStatus ? '' : workText]
          .filter(Boolean)
          .join('\n');
        if (domainText)
          detailLines.push(domainText);
        const detailText = detailLines.join('\n\n');
        setExpandableText(functionStyle, functionMore, detailText, detailText);
        {
          const valueLines = [];
          if (data.value)
            valueLines.push(data.value);
          if (data.error)
            valueLines.push(`error ≈ ${data.error}`);
          if (!valueLines.length && data.status)
            valueLines.push(data.status);
          value.textContent = valueLines.join('\n');
        }
        applyIntegratorBindingState(data, text);
        applyIntegratorResultBound(data);
        saveLastIntegratorState();
        currentVariables = [];
        currentDifferentiable = false;
        renderDerivativeButtons(currentVariables);
        commitModeState();
        setStatus('Ready');
      } catch (err) {
        setRenderedError(String(err));
        resetMoreDigitsButton(renderedMore, false);
        clearResultDetails({keepBindings: true});
        commitModeState();
        setStatus('Error');
      } finally {
        setBusy(false);
        if (!options.skipHistoryUpdate)
          updateHistoryButtons();
      }
    }

    async function evaluateDatetime(options = {}) {
      const evaluationId = ++datetimeEvaluationSequence;
      const state = currentDatetimeState();
      const snapshotText = datetimeSummaryText(state);
      const nextState = historyStateForMode(currentMode(), snapshotText);
      const previousState = !options.skipHistoryUpdate
        ? previousModeStateForHistory(nextState)
        : null;
      showResults();
      setBusy(true);
      setStatus('Calculating dates...');
      try {
        if (previousState)
          pushExpressionHistory(previousState);
        const {response, data} = await fetchDatetimeEvaluation();
        if (evaluationId !== datetimeEvaluationSequence || currentMode() !== 'datetime')
          return;
        if (!response.ok || !data.ok) {
          setRenderedError(data.error || 'Datetime calculation failed');
          resetMoreDigitsButton(renderedMore, false);
          setDatetimeLocalText('');
          clearResultDetails({keepBindings: true});
          commitModeState();
          setStatus('Error');
          return;
        }

        clearResultDetails({keepBindings: true});
        clearRenderedError();
        renderDatetimeSections(rendered, null, data.overview_sections || [], data.overview || '');
        resetMoreDigitsButton(renderedMore, false);
        renderDatetimeSections(parsed, parsedMore, data.range_sections || [], data.range || '');
        renderDatetimeSections(functionStyle, functionMore, data.calendar_sections || [], data.calendar || '');
        renderDatetimeSections(value, null, data.solar_sections || [], data.solar || '');
        setDatetimeLocalText(data.local || '', data.local_sections || []);
        if (data.fields) {
          if (datetimeDate && data.fields.date)
            datetimeDate.value = validDateText(data.fields.date, datetimeDate.value || DEFAULT_DATETIME_DATE);
          if (datetimeJdn)
            datetimeJdn.value = String(data.fields.julian_day_number || '');
          if (datetimeYear && datetimeDate && datetimeDate.value)
            datetimeYear.value = datetimeDate.value.slice(0, 4);
          if (datetimeGmtOffset) {
            const currentOffset = String(datetimeGmtOffset.value || '').trim();
            const returnedOffset = String(data.fields.gmt_offset || '').trim();
            if (returnedOffset && (!datetimeGmtOffsetTouched || !currentOffset || currentOffset === datetimeAutoGmtOffset)) {
              datetimeGmtOffset.value = returnedOffset;
              datetimeAutoGmtOffset = returnedOffset;
              datetimeGmtOffsetTouched = false;
            }
          }
        }
        setResultInputText('');
        clearVariableValues();
        currentVariables = [];
        currentDifferentiable = false;
        renderDerivativeButtons(currentVariables);
        saveLastDatetimeState();
        commitModeState();
        setStatus('Ready');
      } catch (err) {
        if (evaluationId !== datetimeEvaluationSequence || currentMode() !== 'datetime')
          return;
        setRenderedError(String(err));
        resetMoreDigitsButton(renderedMore, false);
        setDatetimeLocalText('');
        clearResultDetails({keepBindings: true});
        commitModeState();
        setStatus('Error');
      } finally {
        if (evaluationId === datetimeEvaluationSequence)
          setBusy(false);
        if (!options.skipHistoryUpdate)
          updateHistoryButtons();
      }
    }

    async function evaluateAlmanac(options = {}) {
      const evaluationId = ++almanacEvaluationSequence;
      const state = currentAlmanacState();
      const snapshotText = almanacSummaryText(state);
      const nextState = historyStateForMode(currentMode(), snapshotText);
      const previousState = !options.skipHistoryUpdate
        ? previousModeStateForHistory(nextState)
        : null;
      showResults();
      setBusy(true);
      setStatus('Working the almanac...');
      try {
        if (previousState)
          pushExpressionHistory(previousState);
        const {response, data} = await fetchAlmanacEvaluation();
        if (evaluationId !== almanacEvaluationSequence || currentMode() !== 'almanac')
          return;
        if (!response.ok || !data.ok) {
          setRenderedError(data.error || 'Almanac calculation failed');
          resetMoreDigitsButton(renderedMore, false);
          clearResultDetails({keepBindings: true});
          commitModeState();
          setStatus('Error');
          return;
        }

        clearResultDetails({keepBindings: true});
        clearRenderedError();
        renderAlmanacWorksheet(rendered, data);
        rendered.dataset.copyText = data.worksheet || '';
        resetMoreDigitsButton(renderedMore, false);
        setExpandableText(parsed, parsedMore, data.selected_summary || '', data.selected_summary || '');
        setExpandableText(functionStyle, functionMore, data.body_listing || '', data.body_listing || '');
        value.textContent = data.notes || '';
        setResultInputText('');
        clearVariableValues();
        currentVariables = [];
        currentDifferentiable = false;
        renderDerivativeButtons(currentVariables);
        saveLastAlmanacState();
        commitModeState();
        setStatus('Ready');
      } catch (err) {
        if (evaluationId !== almanacEvaluationSequence || currentMode() !== 'almanac')
          return;
        setRenderedError(String(err));
        resetMoreDigitsButton(renderedMore, false);
        clearResultDetails({keepBindings: true});
        commitModeState();
        setStatus('Error');
      } finally {
        if (evaluationId === almanacEvaluationSequence)
          setBusy(false);
        if (!options.skipHistoryUpdate)
          updateHistoryButtons();
      }
    }

    async function evaluateActiveModeOnLoad() {
      if (currentMode() === 'matrix') {
        evaluateMatrix();
        return;
      }
      if (currentMode() === 'equation') {
        evaluateEquation();
        return;
      }
      if (currentMode() === 'integrator') {
        evaluateIntegrator();
        return;
      }
      if (currentMode() === 'datetime') {
        await refreshDatetimeJurisdictionLocation();
        evaluateDatetime();
        return;
      }
      if (currentMode() === 'almanac') {
        evaluateAlmanac();
        return;
      }
      evaluateExpression();
    }

    async function runGoalSeek(sourceText, target, start = {}, options = {}) {
      const request = goalSeekExpressionAndStarts(sourceText, start);
      const response = await fetch('/goal_seek', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify({
          expression: expressionForEvaluation(request.expression),
          target,
          start: request.start,
          precision: requestedValuePrecision()
        })
      });
      const data = await response.json();

      if (!response.ok || !data.ok) {
        setRenderedError(data.error || 'Goal seek failed');
        resetMoreDigitsButton(renderedMore, false);
        clearResultDetails({keepBindings: true});
        setStatus('Error');
        return false;
      }

      if (!options.skipHistoryUpdate)
        pushExpressionHistory(currentExpressionText());

      const solvedExpression = data.expression || sourceText;
      const solvedWithoutNan = expressionForEditor(solvedExpression).trim();
      const sourceWithoutNan = expressionForEditor(sourceText).trim();
      const unchanged = solvedWithoutNan === sourceWithoutNan;
      setRenderedResult(data);
      setExpressionEditor(
        solvedExpression,
        data.binding_values || null,
        data.editor_expression || null
      );
      setExpandableText(
        parsed,
        parsedMore,
        data.display_expression || compactExpressionForEditor(solvedExpression).display,
        data.full_display_expression || solvedExpression
      );
      setResultInputText(data.full_display_expression || solvedExpression);
      setExpandableText(
        functionStyle,
        functionMore,
        data.display_function || data.function || '',
        data.full_display_function || data.function || ''
      );
      value.textContent = data.value || '';
      lastEvaluationInputText = solvedExpression;
      lastDerivativeExpression = '';
      {
        const variableBindings = variableNamesFromBindings(data.binding_values || []);
        currentVariables = variableBindings.length
          ? variableBindings
          : variablesFromExpression(solvedExpression);
      }
      currentDifferentiable = String(data.differentiable || 'yes').trim().toLowerCase() !== 'no';
      renderDerivativeButtons(currentVariables);
      expr.dataset.goalSeekSource = expressionForEditor(request.expression).trim();
      expr.dataset.goalSeekTarget = target;
      hideTargetEntry();
      setStatus(unchanged ? 'Goal already reached' : 'Goal reached');
      return true;
    }

    run.addEventListener('click', () => {
      if (currentMode() === 'expression') {
        clearForwardHistory();
        clearGoalSeekRequest();
        hideTargetEntry();
        evaluateExpression();
      } else if (currentMode() === 'equation') {
        evaluateEquation();
      } else if (currentMode() === 'matrix') {
        evaluateMatrix();
      } else if (currentMode() === 'integrator') {
        evaluateIntegrator();
      } else {
        evaluateDatetime();
      }
    });

    back.addEventListener('click', () => {
      commitVisibleBindingInputs();
      const previous = modeHistoryStack(expressionHistory).pop();
      if (!previous) {
        updateHistoryButtons();
        return;
      }

      const current = historyStateForMode();
      if (current.text)
        modeHistoryStack(forwardHistory).push(current);
      restoreHistoryState(previous);
      evaluateCurrentMode({skipHistoryUpdate: true});
    });

    forward.addEventListener('click', () => {
      commitVisibleBindingInputs();
      const next = modeHistoryStack(forwardHistory).pop();
      if (!next) {
        updateHistoryButtons();
        return;
      }

      const current = historyStateForMode();
      if (current.text)
        modeHistoryStack(expressionHistory).push(current);
      restoreHistoryState(next);
      evaluateCurrentMode({skipHistoryUpdate: true});
    });

    async function takeDerivative(wrt) {
      commitVisibleBindingInputs();
      const text = currentExpressionText();
      if (!text || !wrt) return;

      showResults();
      rightPaneTitle.textContent = `${wrt} derivative RESULT`;
      setBusy(true);
      setStatus(`Differentiating d/d${wrt}...`);
      try {
        const {response, data} = await fetchEvaluation(text, wrt);
        const derivativeExpression = derivativeExpressionFromLine(data.derivative);
        const derivativeTex = data.derivative_tex || '';
        const derivativeSvg = data.derivative_svg || '';
        const derivativeFunction = data.display_derivative_function || data.derivative_function || derivativeExpression || '';
        const fullDerivativeFunction = data.full_display_derivative_function || data.derivative_function || derivativeExpression || '';

        if (!response.ok || !data.ok || !derivativeExpression) {
          setRenderedError(data.error || data.raw || `No derivative for ${wrt}`);
          resetMoreDigitsButton(renderedMore, false);
          setStatus('Error');
          return;
        }

        clearResultDetails({keepBindings: true});
        clearRenderedError();
        setExpandableText(
          parsed,
          parsedMore,
          derivativeExpression,
          derivativeExpression
        );
        setResultInputText(derivativeExpression);
        setExpandableText(
          functionStyle,
          functionMore,
          derivativeFunction,
          fullDerivativeFunction
        );
        value.textContent = data.derivative_value || '';
        lastDerivativeExpression = derivativeExpression;
        {
          const variableBindings = variableNamesFromBindings(data.binding_values || []);
          currentVariables = variableBindings.length
            ? variableBindings
            : variablesFromExpression(data.expression || text);
        }
        currentDifferentiable = String(data.differentiable || 'yes').trim().toLowerCase() !== 'no';
        renderDerivativeButtons(currentVariables);
        if (derivativeTex) {
          lastTex = derivativeTex;
          rendered.dataset.displayTex = derivativeTex;
          rendered.dataset.fullTex = derivativeTex;
          rendered.dataset.displaySvg = derivativeSvg;
          rendered.dataset.fullSvg = '';
          rendered.dataset.renderError = data.derivative_render_error || '';
          setRenderedContent(
            derivativeSvg,
            data.derivative_render_error || derivativeTex
          );
          resetMoreDigitsButton(renderedMore, false);
        } else {
          setRenderedContent('', derivativeExpression);
          resetMoreDigitsButton(renderedMore, false);
        }
        setStatus('Ready');
      } catch (err) {
        setRenderedError(String(err));
        resetMoreDigitsButton(renderedMore, false);
        setStatus('Error');
      } finally {
        setBusy(false);
      }
    }

    async function takeIntegral(wrt) {
      commitVisibleBindingInputs();
      const text = currentExpressionText();
      if (!text || !wrt) return;

      showResults();
      rightPaneTitle.textContent = `${wrt} integral RESULT`;
      setBusy(true);
      setStatus(`Integrating with respect to ${wrt}...`);
      try {
        const {response, data} = await fetchEvaluation(text, wrt, 'integral');
        const integralExpression = integralExpressionFromLine(data.integral);
        const integralTex = data.integral_tex || '';
        const integralSvg = data.integral_svg || '';
        const integralFunction = data.display_integral_function || data.integral_function || integralExpression || '';
        const fullIntegralFunction = data.full_display_integral_function || data.integral_function || integralExpression || '';

        if (!response.ok || !data.ok || !integralExpression) {
          setRenderedError(data.error || data.raw || `No integral for ${wrt}`);
          resetMoreDigitsButton(renderedMore, false);
          setStatus('Error');
          return;
        }

        clearResultDetails({keepBindings: true});
        clearRenderedError();
        setExpandableText(
          parsed,
          parsedMore,
          integralExpression,
          integralExpression
        );
        setResultInputText(integralExpression);
        setExpandableText(
          functionStyle,
          functionMore,
          integralFunction,
          fullIntegralFunction
        );
        value.textContent = data.integral_value || '';
        lastDerivativeExpression = '';
        {
          const variableBindings = variableNamesFromBindings(data.binding_values || []);
          currentVariables = variableBindings.length
            ? variableBindings
            : variablesFromExpression(data.expression || text);
        }
        currentDifferentiable = String(data.differentiable || 'yes').trim().toLowerCase() !== 'no';
        renderDerivativeButtons(currentVariables);
        if (integralTex) {
          lastTex = integralTex;
          rendered.dataset.displayTex = integralTex;
          rendered.dataset.fullTex = integralTex;
          rendered.dataset.displaySvg = integralSvg;
          rendered.dataset.fullSvg = '';
          rendered.dataset.renderError = data.integral_render_error || '';
          setRenderedContent(
            integralSvg,
            data.integral_render_error || integralTex
          );
          resetMoreDigitsButton(renderedMore, false);
        } else {
          setRenderedContent('', integralExpression);
          resetMoreDigitsButton(renderedMore, false);
        }
        setStatus('Ready');
      } catch (err) {
        setRenderedError(String(err));
        resetMoreDigitsButton(renderedMore, false);
        setStatus('Error');
      } finally {
        setBusy(false);
      }
    }

    function evaluateFromKeyboard() {
      clearForwardHistory();
      if (currentMode() === 'equation')
        evaluateEquation();
      else if (currentMode() === 'matrix')
        evaluateMatrix();
      else if (currentMode() === 'integrator')
        evaluateIntegrator();
      else if (currentMode() === 'datetime')
        evaluateDatetime();
      else
        evaluateExpression();
    }

    expr.addEventListener('keydown', (event) => {
      if ((event.ctrlKey || event.metaKey) && event.key === 'Enter') {
        event.preventDefault();
        if (currentMode() === 'expression')
          evaluateFromKeyboard();
        else if (currentMode() === 'equation')
          evaluateEquation();
        else if (currentMode() === 'matrix')
          evaluateMatrix();
        else if (currentMode() === 'integrator')
          evaluateIntegrator();
        else
          evaluateDatetime();
      }
    });

    expr.addEventListener('input', () => {
      if (currentMode() === 'datetime') {
        modeEditorText.datetime = expr.value.trim() || DEFAULT_DATETIME_TEXT;
        updateHistoryButtons();
        return;
      }
      if (currentMode() === 'equation') {
        if (!bindingParts(expr.value)) {
          fullExpressionText = expr.value.trim();
          displayedExpressionText = expr.value.trim();
          expr.dataset.fullExpression = fullExpressionText;
          expr.dataset.displayExpression = displayedExpressionText;
        }
        refreshVariableValuesFromEditor();
        updateHistoryButtons();
        return;
      }
      if (currentMode() === 'matrix' || currentMode() === 'integrator') {
        if (!bindingParts(expr.value)) {
          fullExpressionText = expr.value.trim();
          displayedExpressionText = expr.value.trim();
          expr.dataset.fullExpression = fullExpressionText;
          expr.dataset.displayExpression = displayedExpressionText;
        }
        refreshVariableValuesFromEditor();
        updateHistoryButtons();
        return;
      }
      if (currentMode() !== 'expression') {
        updateHistoryButtons();
        return;
      }
      if (expr.value.trim() === (expr.dataset.displayExpression || displayedExpressionText))
        return;
      /*
       * Keep the previous full { body | bindings } expression while the body is
       * edited.  The parser will drop bindings for symbols that disappear, but
       * carrying the old source forward lets newly edited bodies keep existing
       * values such as x = π/4 or a = e.
       */
      clearGoalSeekRequest();
      lastEvaluationInputText = '';
      refreshVariableValuesFromEditor();
      updateHistoryButtons();
    });

    clear.addEventListener('click', () => {
      const current = historyStateForMode();
      if (current.text)
        pushExpressionHistory(current);
      clearForwardHistory();
      expr.value = '';
      if (currentMode() === 'matrix') {
        matrixOperand.value = '';
        matrixOperation.value = 'inverse';
      }
      if (currentMode() === 'equation' && equationVariable)
        equationVariable.value = DEFAULT_EQUATION_VARIABLE_TEXT;
      if (currentMode() === 'integrator') {
        resetIntegratorBoundsToBlank();
        if (integratorIntervalCap)
          integratorIntervalCap.value = String(DEFAULT_INTEGRATOR_INTERVAL_CAP);
      }
      if (currentMode() === 'datetime') {
        if (datetimeDate)
          datetimeDate.value = DEFAULT_DATETIME_DATE;
        if (datetimeJdn)
          datetimeJdn.value = '';
        if (datetimeStart)
          datetimeStart.value = DEFAULT_DATETIME_DATE;
        if (datetimeEnd)
          datetimeEnd.value = DEFAULT_DATETIME_DATE;
        if (datetimeYear)
          datetimeYear.value = DEFAULT_DATETIME_DATE.slice(0, 4);
        if (datetimeJurisdiction)
          setSelectValue(datetimeJurisdiction, DEFAULT_DATETIME_JURISDICTION);
        if (datetimeLatitude)
          datetimeLatitude.value = DEFAULT_DATETIME_LATITUDE;
        if (datetimeLongitude)
          datetimeLongitude.value = DEFAULT_DATETIME_LONGITUDE;
        if (datetimeElevation)
          datetimeElevation.value = DEFAULT_DATETIME_ELEVATION;
        if (datetimeGmtOffset) {
          datetimeGmtOffset.value = DEFAULT_DATETIME_GMT_OFFSET;
          datetimeAutoGmtOffset = String(datetimeGmtOffset.value || '').trim();
          datetimeGmtOffsetTouched = false;
        }
        expr.value = DEFAULT_DATETIME_TEXT;
      }
      if (currentMode() === 'almanac') {
        if (almanacDate)
          almanacDate.value = DEFAULT_ALMANAC_DATE;
        if (almanacTime)
          almanacTime.value = DEFAULT_ALMANAC_TIME;
        if (almanacZone)
          almanacZone.value = DEFAULT_ALMANAC_ZONE;
        if (almanacLatitude)
          almanacLatitude.value = DEFAULT_ALMANAC_LATITUDE;
        if (almanacLongitude)
          almanacLongitude.value = DEFAULT_ALMANAC_LONGITUDE;
        if (almanacBody)
          setSelectValue(almanacBody, DEFAULT_ALMANAC_BODY);
        expr.value = DEFAULT_ALMANAC_TEXT;
      }
      captureCurrentModeEditor();
      clearExpressionSource();
      hideTargetEntry();
      clearResultPane();
      saveCurrentModeResultState();
      commitModeState();
      updateHistoryButtons();
      setStatus('Ready');
      expr.focus();
    });

    help.addEventListener('click', toggleHelp);

    goalSeek.addEventListener('click', async () => {
      if (currentMode() !== 'expression')
        return;
      commitVisibleBindingInputs();
      const text = currentExpressionText();
      if (!text) return;

      if (targetRow.classList.contains('hidden')) {
        showTargetEntry();
        return;
      }

      showResults();
      setBusy(true);
      setStatus('Goal seeking...');
      try {
        const target = goalTarget.value.trim() || '0';
        await runGoalSeek(text, target);
      } catch (err) {
        setRenderedError(String(err));
        resetMoreDigitsButton(renderedMore, false);
        clearResultDetails({keepBindings: true});
        setStatus('Error');
      } finally {
        setBusy(false);
      }
    });

    if (modeTabs.length) {
      modeTabs.forEach((tab) => tab.addEventListener('click', () => {
        captureCurrentModeEditor();
        saveCurrentModeResultState();
        if (!setMode(tab.dataset.mode))
          return;
        saveLastLabMode(currentMode());
        hideTargetEntry();
        restoreModeEditor(currentMode());
        syncModeUI();
        restoreModeResultState(currentMode());
        if (currentMode() === 'integrator' && currentIntegratorBoundRows().length === 0)
          resetIntegratorBoundsToDefault();
        if (currentMode() === 'integrator') {
          if (integratorIntervalCap)
            integratorIntervalCap.value = String(validIntegratorIntervalCap(integratorIntervalCap.value));
        }
        if (currentMode() === 'datetime') {
          restoreDatetimeDefaultsIfBlank();
          datetimeDate?.focus();
        } else if (currentMode() === 'almanac') {
          restoreAlmanacDefaultsIfBlank();
          almanacDate?.focus();
        } else {
          expr.focus();
        }
      }));
    }

    if (matrixOperation)
      matrixOperation.addEventListener('change', () => {
        matrixOperation.value = validMatrixOperation(matrixOperation.value);
        syncMatrixControls();
        if (currentMode() === 'matrix')
          saveLastMatrixState();
      });

    if (equationVariable)
      equationVariable.addEventListener('change', () => {
        equationVariable.value = String(equationVariable.value || DEFAULT_EQUATION_VARIABLE_TEXT).trim() ||
          DEFAULT_EQUATION_VARIABLE_TEXT;
        if (currentMode() === 'equation')
          saveLastEquationState();
      });

    if (integratorIntervalCap)
      integratorIntervalCap.addEventListener('change', () => {
        integratorIntervalCap.value = String(validIntegratorIntervalCap(integratorIntervalCap.value));
        if (currentMode() === 'integrator')
          saveLastIntegratorState();
      });

    [datetimeDate, datetimeJdn, datetimeStart, datetimeEnd, datetimeYear, datetimeJurisdiction, datetimeLatitude, datetimeLongitude, datetimeElevation, datetimeGmtOffset]
      .filter(Boolean)
      .forEach((control) => {
        if (control === datetimeDate || control === datetimeStart || control === datetimeEnd) {
          control.addEventListener('keydown', (event) => {
            const shell = control.closest('.mars-date-shell');
            const button = shell ? shell.querySelector('[data-date-target]') : null;
            if (!button)
              return;
            if (event.key === 'ArrowDown' || event.key === 'Enter') {
              event.preventDefault();
              openMarsDatePicker(control, button);
            } else if (event.key === 'Escape') {
              closeMarsDatePicker();
            }
          });
        }
        control.addEventListener('change', () => {
          if (control === datetimeDate) {
            if (datetimeYear && datetimeDate.value)
              datetimeYear.value = datetimeDate.value.slice(0, 4);
            if (datetimeJdn)
              datetimeJdn.value = '';
          }
          if (currentMode() === 'datetime') {
            triggerDatetimeAutoEvaluation({
              refreshJurisdiction: control === datetimeJurisdiction || control === datetimeDate
            });
          }
        });
      });

    [almanacDate, almanacTime, almanacZone, almanacLatitude, almanacLongitude, almanacBody]
      .filter(Boolean)
      .forEach((control) => {
        if (control === almanacDate) {
          control.addEventListener('keydown', (event) => {
            const shell = control.closest('.mars-date-shell');
            const button = shell ? shell.querySelector('[data-date-target]') : null;
            if (!button)
              return;
            if (event.key === 'ArrowDown' || event.key === 'Enter') {
              event.preventDefault();
              openMarsDatePicker(control, button);
            } else if (event.key === 'Escape') {
              closeMarsDatePicker();
            }
          });
        }
        control.addEventListener('change', () => {
          if (currentMode() === 'almanac') {
            saveLastAlmanacState();
            evaluateAlmanac({skipHistoryUpdate: true});
          }
        });
      });

    if (integratorAddBound) {
      integratorAddBound.addEventListener('click', () => {
        commitVisibleBindingInputs();
        const nextRows = currentIntegratorRows();
        nextRows.push({
          kind: 'bound',
          name: integratorDefaultVariableName(nextRows),
          lo: '',
          hi: '',
        });
        renderIntegratorRows(nextRows);
        refreshVariableValuesFromEditor();
        updateHistoryButtons();
        if (currentMode() === 'integrator')
          saveLastIntegratorState();
      });
    }

    goalTarget.addEventListener('keydown', (event) => {
      if (event.key === 'Enter') {
        event.preventDefault();
        goalSeek.click();
      } else if (event.key === 'Escape') {
        event.preventDefault();
        hideTargetEntry();
        expr.focus();
        setStatus('Ready');
      }
    });

    morePrecision.addEventListener('click', async () => {
      commitVisibleBindingInputs();
      setRequestedPrecisionBits(nextPrecisionStepBits(requestedPrecisionBits()));
      savePrecisionState();
      setStatus('Precision changed');
      try {
        if (currentMode() === 'expression') {
          const goalSeekSource = currentGoalSeekSource();
          const goalSeekTarget = expr.dataset.goalSeekTarget || '';

          if (goalSeekSource && goalSeekTarget) {
            const solvedExpression = fullExpressionText || currentExpressionText();
            const start = solvedStartValuesForGoalSeek(goalSeekSource, solvedExpression);
            await runGoalSeek(goalSeekSource, goalSeekTarget, start, {skipHistoryUpdate: true});
          } else {
            await evaluateExpression({skipHistoryUpdate: true, reuseLastInput: true});
          }
        }
        else if (currentMode() === 'equation')
          await evaluateEquation();
        else if (currentMode() === 'matrix')
          await evaluateMatrix();
        else
          await evaluateIntegrator();
      } finally {
        updateHistoryButtons();
      }
    });

    lessPrecision.addEventListener('click', async () => {
      commitVisibleBindingInputs();
      setRequestedPrecisionBits(previousPrecisionStepBits(requestedPrecisionBits()));
      savePrecisionState();
      setStatus('Precision changed');
      try {
        if (currentMode() === 'expression') {
          const goalSeekSource = currentGoalSeekSource();
          const goalSeekTarget = expr.dataset.goalSeekTarget || '';

          if (goalSeekSource && goalSeekTarget) {
            const solvedExpression = fullExpressionText || currentExpressionText();
            const start = solvedStartValuesForGoalSeek(goalSeekSource, solvedExpression);
            await runGoalSeek(goalSeekSource, goalSeekTarget, start, {skipHistoryUpdate: true});
          } else {
            await evaluateExpression({skipHistoryUpdate: true, reuseLastInput: true});
          }
        }
        else if (currentMode() === 'equation')
          await evaluateEquation();
        else if (currentMode() === 'matrix')
          await evaluateMatrix();
        else
          await evaluateIntegrator();
      } finally {
        updateHistoryButtons();
      }
    });

    renderedMore.addEventListener('click', () => {
      toggleRenderedDigits();
    });

    parsedMore.addEventListener('click', () => {
      toggleTextDigits(parsed, parsedMore);
    });

    functionMore.addEventListener('click', () => {
      toggleTextDigits(functionStyle, functionMore);
    });

    resultUseInput.addEventListener('click', () => {
      sendResultExpressionToInput();
    });

    inputCopy.addEventListener('click', async () => {
      commitVisibleBindingInputs();
      const text = currentMode() === 'datetime'
        ? datetimeSummaryText()
        : String(expr.value || '').trim();
      if (!text)
        return;
      try {
        await writeClipboardText(text);
        flashCopyButton(inputCopy, true);
        setStatus('Copied input');
        setTimeout(() => setStatus('Ready'), 1000);
      } catch (err) {
        flashCopyButton(inputCopy, false);
        setStatus(String(err));
      }
    });

    copyButtons.forEach((button) => {
      button.addEventListener('click', async () => {
        const text = copyTextForTarget(button.dataset.copyTarget);
        if (!text) return;
        try {
          await writeClipboardText(text);
          flashCopyButton(button, true);
          setStatus('Copied');
          setTimeout(() => setStatus('Ready'), 1000);
        } catch (err) {
          flashCopyButton(button, false);
          setStatus(String(err));
        }
      });
    });

    zoomButtons.forEach((button) => {
      button.addEventListener('click', (event) => {
        event.preventDefault();
        event.stopPropagation();
        const card = button.closest('.result-card');
        if (!card)
          return;
        if (button.hasAttribute('data-zoom-reset'))
          setResultZoom(card, RESULT_ZOOM_DEFAULT_INDEX);
        else
          stepResultZoom(card, Number(button.dataset.zoomStep || 1));
        setStatus(`Zoom ${Math.round(RESULT_ZOOM_LEVELS[resultZoomIndex(card)] * 100)}%`);
      });
    });

    resultCards.forEach((card) => {
      applyResultZoom(card);
      card.addEventListener('wheel', (event) => {
        if (!event.ctrlKey && !event.metaKey)
          return;
        event.preventDefault();
        stepResultZoom(card, event.deltaY < 0 ? 1 : -1);
      }, {passive: false});
    });

    window.addEventListener('resize', () => {
      resultCards.forEach((card) => applyResultZoom(card));
    });

    expandCardButtons.forEach((button) => {
      button.setAttribute('aria-expanded', 'false');
      button.addEventListener('click', () => toggleResultCardExpansion(button));
    });

    syncModeTabs();
    syncModeUI();
    restoreIntegratorBoundsText(DEFAULT_INTEGRATOR_BOUNDS_TEXT);
    setStatus('Ready');
    refreshMobileAccess();
    setInterval(refreshMobileAccess, 5000);
    loadLastState().finally(() => evaluateActiveModeOnLoad());
  </script>
</body>
</html>
""".replace("__LAB_NAME__", LAB_APP_NAME).replace(
    "__CONTROL_QUERY_PREFIX__", f"{CONTROL_QUERY_PARAM}="
).replace(
    "__THEME_COLOR__", LAB_THEME_COLOR
).replace(
    "__THEME_OVERRIDES__", LAB_THEME_OVERRIDES
).replace(
    "__BODY_CLASS__", LAB_BODY_CLASS
).replace(
    "__LAB_SUBTITLE__", LAB_SUBTITLE
)

WEB_MANIFEST = {
    "name": LAB_APP_NAME,
    "short_name": LAB_SHORT_NAME,
    "description": LAB_DESCRIPTION,
    "start_url": "/",
    "scope": "/",
    "display": "standalone",
    "background_color": LAB_MANIFEST_BACKGROUND,
    "theme_color": LAB_MANIFEST_THEME,
    "icons": [
        {"src": "/icon-192.png", "sizes": "192x192", "type": "image/png"},
        {"src": "/icon-512.png", "sizes": "512x512", "type": "image/png"}
    ]
}


def default_state() -> dict[str, object]:
    return {
        "expression": DEFAULT_EXPRESSION,
        "equation": DEFAULT_EQUATION,
        "equation_variable": DEFAULT_EQUATION_VARIABLE,
        "matrix": DEFAULT_MATRIX,
        "lab_mode": "expression",
        "matrix_operation": DEFAULT_MATRIX_OPERATION,
        "matrix_operand": "",
        "integrator_expression": DEFAULT_INTEGRATOR_EXPRESSION,
        "integrator_bounds": DEFAULT_INTEGRATOR_BOUNDS,
        "integrator_interval_cap": DEFAULT_INTEGRATOR_INTERVAL_CAP,
        "datetime_date": DEFAULT_DATETIME_DATE,
        "datetime_jdn": "",
        "datetime_start": DEFAULT_DATETIME_DATE,
        "datetime_end": DEFAULT_DATETIME_DATE,
        "datetime_year": DEFAULT_DATETIME_DATE[:4],
        "datetime_jurisdiction": DEFAULT_HOLIDAY_JURISDICTION,
        "datetime_latitude": DEFAULT_DATETIME_LATITUDE,
        "datetime_longitude": DEFAULT_DATETIME_LONGITUDE,
        "datetime_elevation": DEFAULT_DATETIME_ELEVATION,
        "datetime_gmt_offset": DEFAULT_DATETIME_GMT_OFFSET,
        "almanac_date": DEFAULT_ALMANAC_DATE,
        "almanac_time": DEFAULT_ALMANAC_TIME,
        "almanac_zone": DEFAULT_ALMANAC_ZONE,
        "almanac_latitude": DEFAULT_ALMANAC_LATITUDE,
        "almanac_longitude": DEFAULT_ALMANAC_LONGITUDE,
        "almanac_body": DEFAULT_ALMANAC_BODY,
        "precision_bits": {
            "expression": 256,
            "equation": 256,
            "matrix": 256,
            "integrator": 17,
            "datetime": 17,
            "almanac": 17,
        },
    }


def load_state_data() -> dict[str, object]:
    state = default_state()
    try:
        data = json.loads(STATE_FILE.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return state

    if not isinstance(data, dict):
        return state

    state.update(data)
    expression = str(state.get("expression", "")).strip()
    if "..." in expression:
        state["expression"] = DEFAULT_EXPRESSION
    else:
        state["expression"] = expression_with_sorted_constants(expression)

    matrix = str(state.get("matrix", "")).strip()
    if "..." in matrix:
        state["matrix"] = DEFAULT_MATRIX

    lab_mode = str(state.get("lab_mode", "")).strip()
    if lab_mode not in {"expression", "equation", "matrix", "integrator", "datetime", "almanac"}:
        state["lab_mode"] = "expression"

    equation = str(state.get("equation", "")).strip()
    if "..." in equation:
        state["equation"] = DEFAULT_EQUATION
    else:
        state["equation"] = expression_with_sorted_constants(equation)

    equation_variable = str(state.get("equation_variable", "")).strip()
    if not equation_variable:
        state["equation_variable"] = DEFAULT_EQUATION_VARIABLE

    matrix_operation = str(state.get("matrix_operation", "")).strip()
    if matrix_operation not in {
        "eval",
        "inverse",
        "eigenvalues",
        "eigendecompose",
        "charpoly",
        "det",
        "trace",
        "rank",
        "simplify",
        "solve",
    }:
        state["matrix_operation"] = DEFAULT_MATRIX_OPERATION

    integrator_expression = str(state.get("integrator_expression", "")).strip()
    if "..." in integrator_expression:
        state["integrator_expression"] = DEFAULT_INTEGRATOR_EXPRESSION
    else:
        state["integrator_expression"] = expression_with_sorted_constants(integrator_expression)

    try:
        cap = int(state.get("integrator_interval_cap", DEFAULT_INTEGRATOR_INTERVAL_CAP))
    except (TypeError, ValueError):
        cap = DEFAULT_INTEGRATOR_INTERVAL_CAP
    if cap not in INTEGRATOR_INTERVAL_CAP_CHOICES:
        cap = DEFAULT_INTEGRATOR_INTERVAL_CAP
    state["integrator_interval_cap"] = cap
    for key, default in (
        ("datetime_date", DEFAULT_DATETIME_DATE),
        ("datetime_start", DEFAULT_DATETIME_DATE),
        ("datetime_end", DEFAULT_DATETIME_DATE),
    ):
        value = str(state.get(key, "")).strip()
        try:
            py_datetime.date.fromisoformat(value)
        except ValueError:
            value = default
        state[key] = value
    try:
        year = int(str(state.get("datetime_year", DEFAULT_DATETIME_DATE[:4])).strip())
    except ValueError:
        year = int(DEFAULT_DATETIME_DATE[:4])
    state["datetime_year"] = str(max(1, min(9999, year)))
    state["datetime_jurisdiction"] = normalize_holiday_jurisdiction(
        str(state.get("datetime_jurisdiction", DEFAULT_HOLIDAY_JURISDICTION)).strip()
    )
    for key, default in (
        ("datetime_latitude", DEFAULT_DATETIME_LATITUDE),
        ("datetime_longitude", DEFAULT_DATETIME_LONGITUDE),
        ("datetime_elevation", DEFAULT_DATETIME_ELEVATION),
        ("datetime_gmt_offset", DEFAULT_DATETIME_GMT_OFFSET),
    ):
        state[key] = str(state.get(key, default)).strip()
    jdn = str(state.get("datetime_jdn", "")).strip()
    state["datetime_jdn"] = jdn if re.fullmatch(r"\d+", jdn) else ""
    try:
        py_datetime.date.fromisoformat(str(state.get("almanac_date", "")).strip())
    except ValueError:
        state["almanac_date"] = DEFAULT_ALMANAC_DATE
    else:
        state["almanac_date"] = str(state.get("almanac_date", DEFAULT_ALMANAC_DATE)).strip()
    almanac_time = str(state.get("almanac_time", DEFAULT_ALMANAC_TIME)).strip()
    if not re.fullmatch(r"\d{2}:\d{2}(:\d{2}(\.\d+)?)?", almanac_time):
        almanac_time = DEFAULT_ALMANAC_TIME
    state["almanac_time"] = almanac_time
    for key, default in (
        ("almanac_zone", DEFAULT_ALMANAC_ZONE),
        ("almanac_latitude", DEFAULT_ALMANAC_LATITUDE),
        ("almanac_longitude", DEFAULT_ALMANAC_LONGITUDE),
        ("almanac_body", DEFAULT_ALMANAC_BODY),
    ):
        state[key] = str(state.get(key, default)).strip() or str(default)
    return state


def load_state_expression() -> str:
    data = load_state_data()

    expression = str(data.get("expression", "")).strip()
    if "..." in expression:
        return DEFAULT_EXPRESSION
    return expression or DEFAULT_EXPRESSION


def save_state_data(updates: dict[str, object]) -> None:
    state = load_state_data()
    normalized = dict(updates)
    if "expression" in normalized:
        normalized["expression"] = expression_with_sorted_constants(
            str(normalized.get("expression") or "").strip()
        )
    if "equation" in normalized:
        normalized["equation"] = expression_with_sorted_constants(
            str(normalized.get("equation") or "").strip()
        )
    if "integrator_expression" in normalized:
        normalized["integrator_expression"] = expression_with_sorted_constants(
            str(normalized.get("integrator_expression") or "").strip()
        )
    state.update(normalized)
    STATE_FILE.write_text(
        json.dumps(state, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )


def save_state_expression(expression: str) -> None:
    if "..." in expression:
        return
    save_state_data({"expression": expression})


def expression_for_editor(expression: str) -> str:
    return re.sub(r"(=\s*)NAN\b", r"\1?", expression)


def editor_expression_from_fields(fields: dict[str, str]) -> str:
    unbound = str(fields.get("unbound", "")).strip()
    if unbound:
        return expression_for_editor(unbound)
    return expression_for_editor(fields.get("expression", ""))


def expression_for_display(expression: str) -> str:
    return expression_for_editor(expression)


def function_for_display(function: str) -> str:
    return re.sub(r"(=\s*)NAN\b", r"\1?", str(function or ""))


def tex_for_display(tex: str) -> str:
    return re.sub(r"(=\s*)NAN\b", r"\1?", str(tex or ""))


def _is_loopback_or_wildcard_host(host: str) -> bool:
    host = host.strip().lower().strip("[]")
    return (
        not host
        or host == "localhost"
        or host == "0.0.0.0"
        or host == "::"
        or host == "::0"
        or host == "::1"
        or host.startswith("127.")
    )


def _host_from_header(host_header: str) -> str:
    host_header = host_header.strip()
    if host_header.startswith("["):
        end = host_header.find("]")
        return host_header[1:end] if end >= 0 else host_header.strip("[]")
    return host_header.rsplit(":", 1)[0] if ":" in host_header else host_header


def _ip_address_from_text(text: str) -> ipaddress._BaseAddress | None:
    text = text.strip().strip('"')
    if not text:
        return None
    if text.startswith("[") and "]" in text:
        text = text[1:text.index("]")]
    elif ":" in text and text.count(":") == 1:
        text = text.split(":", 1)[0]
    try:
        return ipaddress.ip_address(text)
    except ValueError:
        return None


def request_allows_lab_access(client_host: str) -> bool:
    client_address = _ip_address_from_text(client_host)
    allowed_ipv4 = (
        ipaddress.ip_network("127.0.0.0/8"),
        ipaddress.ip_network("10.0.0.0/8"),
        ipaddress.ip_network("172.16.0.0/12"),
        ipaddress.ip_network("192.168.0.0/16"),
        ipaddress.ip_network("169.254.0.0/16"),
        ipaddress.ip_network("100.64.0.0/10"),
    )
    allowed_ipv6 = (
        ipaddress.ip_network("::1/128"),
        ipaddress.ip_network("fc00::/7"),
        ipaddress.ip_network("fe80::/10"),
    )

    if not client_address:
        return False
    if isinstance(client_address, ipaddress.IPv6Address) and client_address.ipv4_mapped:
        client_address = client_address.ipv4_mapped
    if isinstance(client_address, ipaddress.IPv4Address):
        return any(client_address in network for network in allowed_ipv4)
    return any(client_address in network for network in allowed_ipv6)


def request_uses_public_funnel_host(host_header: str) -> bool:
    request_host = _host_from_header(host_header).strip().lower()

    if not request_host.endswith(".ts.net"):
        return False
    tailscale_host = tailscale_https_host().strip().lower()
    return bool(request_host and tailscale_host and request_host == tailscale_host and
                tailscale_funnel_enabled())


def _control_token_from_query(path: str) -> str:
    query = urllib.parse.urlparse(path).query
    values = urllib.parse.parse_qs(query).get(CONTROL_QUERY_PARAM, [])
    return values[0] if values else ""


def _control_token_from_cookie(headers: http.client.HTTPMessage) -> str:
    for part in headers.get("Cookie", "").split(";"):
        name, sep, value = part.strip().partition("=")
        if sep and name == CONTROL_COOKIE:
            return urllib.parse.unquote(value)
    return ""


def _control_url(url: str) -> str:
    separator = "&" if "?" in url else "?"
    token = urllib.parse.urlencode({CONTROL_QUERY_PARAM: CONTROL_TOKEN})
    return f"{url}{separator}{token}"


def request_allows_funnel_control(headers: http.client.HTTPMessage,
                                  client_host: str,
                                  request_token: str = "") -> bool:
    if request_token == CONTROL_TOKEN:
        return True
    if headers.get("X-Dval-Lab-Control", "") == CONTROL_TOKEN:
        return True
    if _control_token_from_cookie(headers) == CONTROL_TOKEN:
        return True

    request_host = _host_from_header(headers.get("Host", "")).strip().lower()
    tailscale_hosts = {
        host for host in (
            tailscale_https_host().strip().lower(),
            tailscale_magicdns_host().strip().lower(),
            tailscale_ipv4().strip().lower(),
        ) if host
    }
    if request_host in tailscale_hosts:
        return False

    request_address = _ip_address_from_text(request_host)
    if request_address and request_address in ipaddress.ip_network("100.64.0.0/10"):
        return False

    client_address = _ip_address_from_text(client_host)
    return bool(client_address and client_address.is_loopback and
                _is_loopback_or_wildcard_host(request_host))


def tailscale_ipv4() -> str:
    if not shutil.which("tailscale"):
        return ""

    try:
        status = subprocess.run(
            ["tailscale", "status"],
            text=True,
            capture_output=True,
            timeout=2,
            check=False,
        )
        if status.returncode != 0:
            return ""

        completed = subprocess.run(
            ["tailscale", "ip", "-4"],
            text=True,
            capture_output=True,
            timeout=2,
            check=False,
        )
        for address in completed.stdout.split():
            if address.startswith("100."):
                return address
    except Exception:
        pass

    return ""


def tailscale_magicdns_host() -> str:
    if not tailscale_ipv4():
        return ""
    return os.environ.get("MARS_LAB_TAILSCALE_HOST", "mars").strip().strip(".")


def tailscale_https_host() -> str:
    if not tailscale_ipv4():
        return ""

    env_host = os.environ.get("MARS_LAB_TAILSCALE_HTTPS_HOST", "").strip().strip(".")
    if env_host:
        return env_host

    try:
        completed = subprocess.run(
            ["tailscale", "status", "--json"],
            text=True,
            capture_output=True,
            timeout=2,
            check=False,
        )
        if completed.returncode != 0:
            return ""
        status = json.loads(completed.stdout)
    except Exception:
        return ""

    for cert_host in status.get("CertDomains", []) or []:
        cert_host = str(cert_host).strip().strip(".")
        if cert_host:
            return cert_host

    self_info = status.get("Self", {}) or {}
    dns_name = str(self_info.get("DNSName", "")).strip().strip(".")
    return dns_name


def tailscale_funnel_enabled() -> bool:
    try:
        completed = subprocess.run(
            ["tailscale", "funnel", "status"],
            text=True,
            capture_output=True,
            timeout=2,
            check=False,
        )
    except Exception:
        return False

    return completed.returncode == 0 and "Funnel on" in completed.stdout


def set_tailscale_funnel_enabled(port: int, enabled: bool) -> bool:
    if not tailscale_https_host():
        return False

    try:
        if enabled:
            completed = subprocess.run(
                ["tailscale", "funnel", "--bg", "--https", "443", str(port)],
                text=True,
                capture_output=True,
                timeout=5,
                check=False,
            )
            return completed.returncode == 0

        completed = subprocess.run(
            ["tailscale", "funnel", "--https=443", "off"],
            text=True,
            capture_output=True,
            timeout=5,
            check=False,
        )
        # If there is no existing Serve/Funnel config, Tailscale may report that
        # the off command had nothing to change.  Private mode still needs Serve.

        completed = subprocess.run(
            ["tailscale", "serve", "--bg", "--https", "443", str(port)],
            text=True,
            capture_output=True,
            timeout=5,
            check=False,
        )
        return completed.returncode == 0
    except Exception:
        return False


def ensure_tailscale_serve(bind_host: str, port: int) -> None:
    if os.environ.get("MARS_LAB_TAILSCALE_SERVE", "1").strip() in ("0", "false", "False", "no", "NO"):
        return
    if bind_host.strip() not in ("0.0.0.0", "::", "::0") or not tailscale_ipv4():
        return
    if not tailscale_https_host():
        return

    # Privacy first: MARS Lab may be shared on local WiFi or the private
    # tailnet, but it should not publish itself to the public internet.
    set_tailscale_funnel_enabled(port, False)


def local_lan_ipv4() -> str:
    try:
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
            sock.connect(("8.8.8.8", 80))
            return str(sock.getsockname()[0])
    except OSError:
        pass

    try:
        for info in socket.getaddrinfo(socket.gethostname(), None, socket.AF_INET):
            address = str(info[4][0])
            if not _is_loopback_or_wildcard_host(address):
                return address
    except OSError:
        pass

    try:
        completed = subprocess.run(
            ["hostname", "-I"],
            text=True,
            capture_output=True,
            timeout=2,
            check=False,
        )
        for address in completed.stdout.split():
            if "." in address and not _is_loopback_or_wildcard_host(address):
                return address
    except Exception:
        pass

    try:
        completed = subprocess.run(
            ["ip", "-4", "route", "get", "1.1.1.1"],
            text=True,
            capture_output=True,
            timeout=2,
            check=False,
        )
        words = completed.stdout.split()
        if "src" in words:
            address = words[words.index("src") + 1]
            if not _is_loopback_or_wildcard_host(address):
                return address
    except Exception:
        pass

    return ""


def local_mdns_host() -> str:
    hostname = socket.gethostname().strip().strip(".")
    if not hostname:
        return ""

    short_name = hostname.split(".", 1)[0]
    if not short_name or _is_loopback_or_wildcard_host(short_name):
        return ""
    return f"{short_name.lower()}.local"


def host_port_reachable(host: str, port: int, timeout: float = 0.35) -> bool:
    host = host.strip().strip("[]")
    if not host or port <= 0:
        return False
    try:
        with socket.create_connection((host, port), timeout=timeout):
            return True
    except OSError:
        return False


def _host_is_ipv6(host: str) -> bool:
    address = _ip_address_from_text(host)
    return isinstance(address, ipaddress.IPv6Address)


class DualStackThreadingHTTPServer(http.server.ThreadingHTTPServer):
    address_family = socket.AF_INET6

    def server_bind(self) -> None:
        if hasattr(socket, "IPV6_V6ONLY"):
            try:
                self.socket.setsockopt(socket.IPPROTO_IPV6, socket.IPV6_V6ONLY, 0)
            except OSError:
                # Some platforms do not allow changing this.  IPv6 access still
                # works; IPv4 may need a separate bind on those systems.
                pass
        super().server_bind()


def create_threading_http_server(host: str, port: int,
                                 handler: type[http.server.BaseHTTPRequestHandler]
                                 ) -> http.server.ThreadingHTTPServer:
    host = host.strip() or "127.0.0.1"
    if host == "::0":
        host = "::"
    if _host_is_ipv6(host):
        return DualStackThreadingHTTPServer((host, port), handler)
    return http.server.ThreadingHTTPServer((host, port), handler)


def browser_access_host(bind_host: str, port: int = 0) -> str:
    bind_host = bind_host.strip()
    if bind_host in ("0.0.0.0", "::", "::0"):
        return "localhost"
    return bind_host


def browser_access_url(bind_host: str, port: int) -> str:
    bind_host = bind_host.strip()
    bind_address = _ip_address_from_text(bind_host)
    bind_is_tailscale = bool(bind_address and bind_address in ipaddress.ip_network("100.64.0.0/10"))
    if bind_is_tailscale:
        tailscale_host = tailscale_https_host()
        if tailscale_host:
            return f"https://{tailscale_host}/"

    browser_host = browser_access_host(bind_host, port)
    if ":" in browser_host and not browser_host.startswith("["):
        browser_host = f"[{browser_host}]"
    return f"http://{browser_host}:{port}/"


def tailscale_access_url(bind_host: str, port: int, path: str = "/") -> str:
    path = path if path.startswith("/") else f"/{path}"
    tailscale_ip = tailscale_ipv4()
    if not tailscale_ip:
        return ""

    tailscale_host = tailscale_https_host()
    if tailscale_host:
        return f"https://{tailscale_host}{path}"

    bind_host = bind_host.strip()
    bind_address = _ip_address_from_text(bind_host)
    bind_accepts_tailscale = (
        bind_host in ("0.0.0.0", "::", "::0") or
        bool(bind_address and bind_address in ipaddress.ip_network("100.64.0.0/10"))
    )
    if not bind_accepts_tailscale:
        return ""

    url_host = tailscale_magicdns_host() or tailscale_ip
    if ":" in url_host and not url_host.startswith("["):
        url_host = f"[{url_host}]"
    return f"http://{url_host}:{port}{path}"


def mobile_access_url(bind_host: str, port: int, host_header: str = "") -> str:
    return str(mobile_access_details(bind_host, port, host_header)["url"])


def mobile_access_details(bind_host: str, port: int, host_header: str = "",
                          control_allowed: bool = False) -> dict[str, object]:
    funnel = tailscale_funnel_enabled()
    request_host = _host_from_header(host_header)
    if request_host and not _is_loopback_or_wildcard_host(request_host):
        tailscale_host = tailscale_https_host()
        magicdns_host = tailscale_magicdns_host()
        request_is_tailscale = (
            request_host.startswith("100.") or
            (tailscale_host and request_host.lower() == tailscale_host.lower()) or
            (magicdns_host and request_host.lower() == magicdns_host.lower())
        )
        if request_is_tailscale:
            url_host = tailscale_host or request_host
            scheme = "https" if tailscale_host else "http"
            url_port = "" if tailscale_host else f":{port}"
            return {
                "url": f"{scheme}://{url_host}{url_port}/",
                "title": "Tailscale access",
                "hint": "Scan from a device connected to Tailscale.",
                "funnel": funnel,
                "tailscale": True,
                "control": control_allowed,
            }
        return {
            "url": f"http://{request_host}:{port}/",
            "title": "WiFi access",
            "hint": "Scan from a phone on the same WiFi.",
            "funnel": False,
            "tailscale": False,
            "control": False,
        }

    bind_host = bind_host.strip()
    if bind_host in ("0.0.0.0", "::", "::0"):
        lan_host = local_mdns_host() or local_lan_ipv4()
        if lan_host:
            return {
                "url": f"http://{lan_host}:{port}/",
                "title": "WiFi access",
                "hint": "Scan from a phone on the same WiFi.",
                "funnel": False,
                "tailscale": False,
                "control": False,
            }
        bind_host = tailscale_ipv4()
        if bind_host:
            tailscale_host = tailscale_https_host()
            scheme = "https" if tailscale_host else "http"
            tailscale_host = tailscale_host or tailscale_magicdns_host() or bind_host
            url_port = "" if scheme == "https" else f":{port}"
            return {
                "url": f"{scheme}://{tailscale_host}{url_port}/",
                "title": "Tailscale access",
                "hint": "Scan from a device connected to Tailscale.",
                "funnel": funnel,
                "tailscale": True,
                "control": control_allowed,
            }

    if _is_loopback_or_wildcard_host(bind_host):
        return {
            "url": "",
            "title": "Mobile access",
            "hint": "No mobile URL is available right now.",
            "funnel": False,
            "tailscale": False,
            "control": False,
        }

    if ":" in bind_host and not bind_host.startswith("["):
        bind_host = f"[{bind_host}]"
    title = "Tailscale access" if bind_host.startswith("100.") else "WiFi access"
    hint = "Scan from a phone connected to Tailscale." if bind_host.startswith("100.") else "Scan from a phone on the same WiFi."
    return {
        "url": f"http://{bind_host}:{port}/",
        "title": title,
        "hint": hint,
        "funnel": False,
        "tailscale": bind_host.startswith("100."),
        "control": control_allowed and bind_host.startswith("100."),
    }


def _qr_gf_tables() -> tuple[list[int], list[int]]:
    exp = [0] * 512
    log = [0] * 256
    x = 1
    for i in range(255):
        exp[i] = x
        log[x] = i
        x <<= 1
        if x & 0x100:
            x ^= 0x11D
    for i in range(255, 512):
        exp[i] = exp[i - 255]
    return exp, log


_QR_GF_EXP, _QR_GF_LOG = _qr_gf_tables()


def _qr_gf_mul(a: int, b: int) -> int:
    if a == 0 or b == 0:
        return 0
    return _QR_GF_EXP[_QR_GF_LOG[a] + _QR_GF_LOG[b]]


def _qr_rs_generator(degree: int) -> list[int]:
    poly = [1]
    for i in range(degree):
        next_poly = [0] * (len(poly) + 1)
        root = _QR_GF_EXP[i]
        for j, coef in enumerate(poly):
            next_poly[j] ^= coef
            next_poly[j + 1] ^= _qr_gf_mul(coef, root)
        poly = next_poly
    return poly


_QR_RS_GENERATOR = _qr_rs_generator(QR_EC_CODEWORDS)


def _qr_rs_remainder(data: list[int]) -> list[int]:
    result = [0] * QR_EC_CODEWORDS
    for value in data:
        factor = value ^ result[0]
        result = result[1:] + [0]
        for i in range(QR_EC_CODEWORDS):
            result[i] ^= _qr_gf_mul(_QR_RS_GENERATOR[i + 1], factor)
    return result


def _qr_data_codewords(text: str) -> list[int]:
    payload = text.encode("utf-8")
    bits: list[int] = []

    def append(value: int, width: int) -> None:
        for shift in range(width - 1, -1, -1):
            bits.append((value >> shift) & 1)

    append(0b0100, 4)  # byte mode
    append(len(payload), 8)
    for byte in payload:
        append(byte, 8)

    capacity = QR_DATA_CODEWORDS * 8
    if len(bits) > capacity:
        raise ValueError("mobile URL is too long for the built-in QR code")

    bits.extend([0] * min(4, capacity - len(bits)))
    while len(bits) % 8:
        bits.append(0)

    codewords = [
        sum(bits[i + bit] << (7 - bit) for bit in range(8))
        for i in range(0, len(bits), 8)
    ]
    pad = 0
    while len(codewords) < QR_DATA_CODEWORDS:
        codewords.append(0xEC if pad % 2 == 0 else 0x11)
        pad += 1
    return codewords


def _qr_format_bits() -> int:
    data = (QR_EC_LEVEL_L << 3) | QR_MASK_PATTERN
    rem = data
    for _ in range(10):
        rem = (rem << 1) ^ (0x537 if (rem >> 9) & 1 else 0)
    return ((data << 10) | (rem & 0x3FF)) ^ 0x5412


def _qr_make_matrix(text: str) -> list[list[int]]:
    data = _qr_data_codewords(text)
    codewords = data + _qr_rs_remainder(data)
    data_bits = [
        (codeword >> shift) & 1
        for codeword in codewords
        for shift in range(7, -1, -1)
    ]
    size = QR_SIZE
    modules: list[list[int | None]] = [[None for _ in range(size)] for _ in range(size)]
    reserved = [[False for _ in range(size)] for _ in range(size)]

    def set_module(x: int, y: int, dark: bool, reserve: bool = True) -> None:
        if 0 <= x < size and 0 <= y < size:
            modules[y][x] = 1 if dark else 0
            if reserve:
                reserved[y][x] = True

    def add_finder(x0: int, y0: int) -> None:
        for y in range(y0 - 1, y0 + 8):
            for x in range(x0 - 1, x0 + 8):
                set_module(x, y, False)
        for y in range(7):
            for x in range(7):
                dark = x in (0, 6) or y in (0, 6) or (2 <= x <= 4 and 2 <= y <= 4)
                set_module(x0 + x, y0 + y, dark)

    def add_alignment(cx: int, cy: int) -> None:
        if reserved[cy][cx]:
            return
        for y in range(-2, 3):
            for x in range(-2, 3):
                dark = max(abs(x), abs(y)) != 1
                set_module(cx + x, cy + y, dark)

    add_finder(0, 0)
    add_finder(size - 7, 0)
    add_finder(0, size - 7)
    add_alignment(30, 30)

    for i in range(size):
        if not reserved[6][i]:
            set_module(i, 6, i % 2 == 0)
        if not reserved[i][6]:
            set_module(6, i, i % 2 == 0)

    # Reserve format cells before data placement, then fill them below.
    for x, y in (
        [(8, i) for i in range(6)]
        + [(8, 7), (8, 8), (7, 8)]
        + [(i, 8) for i in range(6)]
        + [(size - 1 - i, 8) for i in range(8)]
        + [(8, size - 15 + i) for i in range(8, 15)]
    ):
        set_module(x, y, False)
    set_module(8, size - 8, True)

    bit_index = 0
    upward = True
    x = size - 1
    while x > 0:
        if x == 6:
            x -= 1
        for row in range(size):
            y = size - 1 - row if upward else row
            for dx in (0, 1):
                xx = x - dx
                if reserved[y][xx]:
                    continue
                bit = data_bits[bit_index] if bit_index < len(data_bits) else 0
                if (xx + y) % 2 == 0:
                    bit ^= 1
                set_module(xx, y, bool(bit), reserve=False)
                bit_index += 1
        upward = not upward
        x -= 2

    fmt = _qr_format_bits()

    def fmt_bit(i: int) -> bool:
        return ((fmt >> i) & 1) != 0

    for i in range(6):
        set_module(8, i, fmt_bit(i))
    set_module(8, 7, fmt_bit(6))
    set_module(8, 8, fmt_bit(7))
    set_module(7, 8, fmt_bit(8))
    for i in range(9, 15):
        set_module(14 - i, 8, fmt_bit(i))

    for i in range(8):
        set_module(size - 1 - i, 8, fmt_bit(i))
    for i in range(8, 15):
        set_module(8, size - 15 + i, fmt_bit(i))

    return [[1 if value else 0 for value in row] for row in modules]


def qr_svg(text: str) -> str:
    if not text:
        return ""

    try:
        matrix = _qr_make_matrix(text)
    except ValueError:
        return ""

    quiet = 4
    size = len(matrix) + quiet * 2
    path = []
    for y, row in enumerate(matrix):
        for x, dark in enumerate(row):
            if dark:
                path.append(f"M{x + quiet},{y + quiet}h1v1h-1z")

    return (
        f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {size} {size}" '
        'role="img" aria-label="Mobile access QR code">'
        f'<rect width="{size}" height="{size}" fill="#fff"/>'
        f'<path fill="#0f2f5f" d="{"".join(path)}"/>'
        "</svg>"
    )


def mobile_qr_svg(url: str, include_control_token: bool) -> str:
    url = str(url or "").strip()
    if not url:
        return ""

    if include_control_token:
        svg = qr_svg(_control_url(url))
        if svg:
            return svg

    return qr_svg(url)


def _compact_long_text_value(
    value: str,
    limit: int = COMPACT_BINDING_VALUE_LIMIT,
    keep: int = COMPACT_BINDING_VALUE_KEEP,
) -> str:
    value = str(value or "").strip()
    if "..." in value:
        return value
    if len(value) <= limit:
        return value
    return compact_long_numeric_tokens(value)


def compact_long_numeric_tokens(text: str) -> str:
    if not text:
        return text

    def compact_match(match: re.Match[str]) -> str:
        number_text = match.group(2)
        if len(number_text) <= COMPACT_BINDING_VALUE_LIMIT or "..." in number_text:
            return match.group(0)
        scientific = re.match(r"^(.*?)([Ee][+-]?\d+)$", number_text)
        if scientific:
            mantissa = scientific.group(1)
            exponent = scientific.group(2)
            return (
                match.group(1)
                + mantissa[:COMPACT_BINDING_VALUE_KEEP]
                + "..."
                + exponent
            )
        return match.group(1) + number_text[:COMPACT_BINDING_VALUE_KEEP] + "..."

    return re.sub(
        r"(^|[^A-Za-z0-9_.])([+-]?(?:\d+\.\d+|\d{21,})(?:[Ee][+-]?\d+)?)",
        compact_match,
        text,
    )


def precision_numeric_tokens(text: str, precision: int) -> str:
    if not text:
        return text

    text = decimalize_long_terminating_rational_tokens(text)

    def precision_match(match: re.Match[str]) -> str:
        return match.group(1) + format_number_text_for_precision(match.group(2), precision)

    return re.sub(
        r"(^|[^A-Za-z0-9_.])([+-]?(?:\d+\.\d+|\d{21,})(?:[Ee][+-]?\d+)?)",
        precision_match,
        text,
    )


def _decimalize_long_terminating_rational_token(token: str) -> str:
    token = str(token or "").strip()
    if "/" not in token:
        return token

    sign = ""
    if token.startswith(("+", "-")):
        sign = token[0]
        token = token[1:]

    try:
        numer_text, denom_text = token.split("/", 1)
        numer = int(numer_text, 10)
        denom = int(denom_text, 10)
    except (TypeError, ValueError):
        return (sign + token) if sign else token

    if denom == 0:
        return (sign + token) if sign else token

    if denom < 0:
        numer = -numer
        denom = -denom

    twos = 0
    fives = 0
    den_work = denom
    while den_work % 2 == 0:
        den_work //= 2
        twos += 1
    while den_work % 5 == 0:
        den_work //= 5
        fives += 1

    scale = max(twos, fives)
    if den_work != 1 or scale < 12:
        return (sign + token) if sign else token

    if twos < scale:
        numer *= 5 ** (scale - twos)
    if fives < scale:
        numer *= 2 ** (scale - fives)

    neg = numer < 0
    digits = str(abs(numer))
    if scale >= len(digits):
        digits = "0" * (scale - len(digits) + 1) + digits

    point = len(digits) - scale
    decimal_text = digits[:point] + "." + digits[point:]
    decimal_text = decimal_text.rstrip("0").rstrip(".")
    if not decimal_text:
        decimal_text = "0"
    if neg:
        decimal_text = "-" + decimal_text
    if sign == "+" and not decimal_text.startswith(("+", "-")):
        decimal_text = "+" + decimal_text
    return decimal_text


def decimalize_long_terminating_rational_tokens(text: str) -> str:
    if not text:
        return text

    return re.sub(
        r"(?<![A-Za-z0-9_.])([+-]?\d+/\d+)(?![A-Za-z0-9_.])",
        lambda match: _decimalize_long_terminating_rational_token(match.group(1)),
        text,
    )


def precision_limit_result_fields(fields: dict[str, str], precision: int) -> None:
    for key in (
        "expression",
        "unbound",
        "tex",
        "function",
        "derivative_function",
        "integral_function",
    ):
        value = fields.get(key, "")
        if not value:
            continue
        fields[f"raw_{key}"] = value
        fields[key] = precision_numeric_tokens(value, precision)


def compact_display_text(text: str) -> str:
    return compact_long_numeric_tokens(compact_binding_values_text(text))


def normalize_multiline_display_text(text: str) -> str:
    lines = str(text or "").splitlines()
    return "\n".join(line.strip() for line in lines if line.strip())


def compact_binding_values_text(text: str) -> str:
    if not text:
        return text

    def compact_after_pipe(match: re.Match[str]) -> str:
        value = match.group(2).strip()
        if not value:
            return match.group(0)
        return match.group(1) + _compact_long_text_value(value)

    return re.sub(r"(=\s*)(.*?)(?=(?:\\right|[,;}\n]|$))", compact_after_pipe, text)


def compact_function_text(text: str) -> str:
    if not text:
        return text

    compacted: list[str] = []
    for line in text.splitlines():
        match = re.match(r"^(\s*[^=\s][^=]*=\s*)(.+)$", line)
        if match and "(" not in match.group(1):
            compacted.append(match.group(1) + _compact_long_text_value(match.group(2)))
        else:
            compacted.append(compact_long_numeric_tokens(line))
    return compact_long_numeric_tokens("\n".join(compacted))


def find_free_port(host: str) -> int:
    family = socket.AF_INET6 if _host_is_ipv6(host) else socket.AF_INET
    bind_host = "::" if host.strip() == "::0" else host
    with socket.socket(family, socket.SOCK_STREAM) as sock:
        if family == socket.AF_INET6 and hasattr(socket, "IPV6_V6ONLY"):
            try:
                sock.setsockopt(socket.IPPROTO_IPV6, socket.IPV6_V6ONLY, 0)
            except OSError:
                pass
        sock.bind((bind_host, 0))
        return int(sock.getsockname()[1])


def ensure_mars_lab(binary: Path) -> None:
    if binary.exists() and os.access(binary, os.X_OK):
        return

    subprocess.run(
        ["make", DEFAULT_SCRATCH_TARGET],
        cwd=ROOT,
        check=True,
        text=True,
    )

    if not binary.exists():
        raise RuntimeError(f"scratch binary was not created at {binary}")


def ensure_scratch_binary(binary: Path, target: str) -> None:
    subprocess.run(
        ["make", target],
        cwd=ROOT,
        check=True,
        text=True,
    )

    if not binary.exists():
        raise RuntimeError(f"{target} binary was not created at {binary}")


def parse_keyed_output(
    output: str,
    patterns: dict[str, str],
    multiline_fields: set[str] | None = None,
) -> dict[str, str]:
    fields: dict[str, str] = {}
    current_multiline_key: str | None = None
    multiline_fields = multiline_fields or set()

    for line in output.splitlines():
        matched = False
        for key, pattern in patterns.items():
            match = re.match(pattern, line)
            if match:
                value = match.group(1).rstrip()
                if key in fields:
                    fields[key] = fields[key] + "\n" + value
                else:
                    fields[key] = value
                current_multiline_key = key if key in multiline_fields else None
                matched = True
                break
        if not matched and current_multiline_key:
            fields[current_multiline_key] += "\n" + line.rstrip()
    return fields


def parse_mars_lab_output(output: str) -> dict[str, str]:
    patterns = {
        "input": r"^input\s+(.*)$",
        "expression": r"^expression\s{2,}(.*)$",
        "unbound": r"^unbound\s+(.*)$",
        "function": r"^function\s+(.*)$",
        "tex": r"^tex\s+(.*)$",
        "differentiable": r"^differentiable\s+(.*)$",
        "value": r"^value\s+(.*)$",
        "value_note": r"^value_note\s+(.*)$",
        "residual": r"^residual\s+(.*)$",
        "iterations": r"^iterations\s+(.*)$",
        "complex": r"^complex\s+(.*)$",
        "derivative": r"^derivative\s+(.*)$",
        "derivative_function": r"^derivative_function\s{2,}(.*)$",
        "derivative_tex": r"^derivative_tex\s*(.*)$",
        "derivative_value": r"^d value\s+(.*)$",
        "integral": r"^integral\s+(.*)$",
        "integral_function": r"^integral_function\s{2,}(.*)$",
        "integral_tex": r"^integral_tex\s*(.*)$",
        "integral_value": r"^i value\s+(.*)$",
    }
    return parse_keyed_output(
        output,
        patterns,
        {
            "function",
            "tex",
            "derivative_function",
            "derivative_tex",
            "integral_function",
            "integral_tex",
        },
    )


def parse_matrix_lab_output(output: str) -> dict[str, str]:
    return parse_keyed_output(
        output,
        {
            "input": r"^input\s+(.*)$",
            "operation": r"^operation\s+(.*)$",
            "operand": r"^operand\s+(.*)$",
            "kind": r"^kind\s+(.*)$",
            "rows": r"^rows\s+(.*)$",
            "cols": r"^cols\s+(.*)$",
            "result": r"^result\s+(.*)$",
            "pretty": r"^pretty\s+(.*)$",
            "tex": r"^tex\s+(.*)$",
            "error": r"^error\s+(.*)$",
        },
        {"pretty", "tex"},
    )


def parse_integrator_lab_output(output: str) -> dict[str, str]:
    return parse_keyed_output(
        output,
        {
            "input": r"^input\s+(.*)$",
            "expression": r"^expression\s+(.*)$",
            "binding_expression": r"^binding_expression\s*(.*)$",
            "dimensions": r"^dimensions\s+(.*)$",
            "bound": r"^bound\s+(.*)$",
            "bound_var": r"^bound_var\s+(.*)$",
            "bound_lower": r"^bound_lower\s*(.*)$",
            "bound_upper": r"^bound_upper\s*(.*)$",
            "tex": r"^tex\s+(.*)$",
            "symbolic_tex": r"^symbolic_tex\s*(.*)$",
            "symbolic_value": r"^symbolic_value\s*(.*)$",
            "antiderivative_tex": r"^antiderivative_tex\s*(.*)$",
            "antiderivative": r"^antiderivative\s+(.*)$",
            "symbolic": r"^symbolic\s+(.*)$",
            "value": r"^value\s*(.*)$",
            "error": r"^error\s+(.*)$",
            "error": r"^error\s+(.*)$",
            "work_units": r"^work_units\s+(.*)$",
            "work_cap": r"^work_cap\s+(.*)$",
            "intervals": r"^intervals\s+(.*)$",
            "max_intervals": r"^max_intervals\s+(.*)$",
            "status": r"^status\s+(.*)$",
        },
        {"tex", "symbolic_tex", "antiderivative_tex"},
    )


def parse_equation_lab_output(output: str) -> dict[str, str]:
    return parse_keyed_output(
        output,
        {
            "input": r"^input\s+(.*)$",
            "equation": r"^equation\s+(.*)$",
            "unbound": r"^unbound\s+(.*)$",
            "tex": r"^tex\s+(.*)$",
            "residual": r"^residual\s+(.*)$",
            "value": r"^value\s+(.*)$",
            "status": r"^status\s+(.*)$",
            "solutions_tex": r"^solutions_tex\s*(.*)$",
            "solutions": r"^solutions\s+(.*)$",
            "numeric": r"^numeric\s+(.*)$",
        },
        {"tex", "solutions_tex", "solutions"},
    )


def parse_datetime_lab_output(output: str) -> dict[str, str]:
    return parse_keyed_output(
        output,
        {
            "date": r"^date\s+(.*)$",
            "weekday": r"^weekday\s+(.*)$",
            "julian_day_number": r"^julian_day_number\s+(.*)$",
            "christian_calendar_date": r"^christian_calendar_date\s+(.*)$",
            "chinese_calendar_date": r"^chinese_calendar_date\s+(.*)$",
            "hindu_calendar_date": r"^hindu_calendar_date\s+(.*)$",
            "buddhist_calendar_date": r"^buddhist_calendar_date\s+(.*)$",
            "muslim_calendar_date": r"^muslim_calendar_date\s+(.*)$",
            "jewish_calendar_date": r"^jewish_calendar_date\s+(.*)$",
            "cherokee_calendar_date": r"^cherokee_calendar_date\s+(.*)$",
            "mayan_calendar_date": r"^mayan_calendar_date\s+(.*)$",
            "aztec_calendar_date": r"^aztec_calendar_date\s+(.*)$",
            "ethiopian_calendar_date": r"^ethiopian_calendar_date\s+(.*)$",
            "moon_phase": r"^moon_phase\s+(.*)$",
            "solar_declination": r"^solar_declination\s+(.*)$",
            "solar_max_altitude": r"^solar_max_altitude\s+(.*)$",
            "solar_inclination": r"^solar_inclination\s+(.*)$",
            "latitude": r"^latitude\s+(.*)$",
            "longitude": r"^longitude\s+(.*)$",
            "elevation_metres": r"^elevation_metres\s+(.*)$",
            "gmt_offset": r"^gmt_offset\s+(.*)$",
            "sunrise": r"^sunrise\s+(.*)$",
            "sunrise_status": r"^sunrise_status\s+(.*)$",
            "sunset": r"^sunset\s+(.*)$",
            "sunset_status": r"^sunset_status\s+(.*)$",
            "actual_sunrise": r"^actual_sunrise\s+(.*)$",
            "actual_sunrise_status": r"^actual_sunrise_status\s+(.*)$",
            "actual_sunset": r"^actual_sunset\s+(.*)$",
            "actual_sunset_status": r"^actual_sunset_status\s+(.*)$",
            "dst_forward": r"^dst_forward\s+(.*)$",
            "dst_back": r"^dst_back\s+(.*)$",
            "dst_forward_from_offset": r"^dst_forward_from_offset\s+(.*)$",
            "dst_forward_to_offset": r"^dst_forward_to_offset\s+(.*)$",
            "dst_back_from_offset": r"^dst_back_from_offset\s+(.*)$",
            "dst_back_to_offset": r"^dst_back_to_offset\s+(.*)$",
            "dst_status": r"^dst_status\s+(.*)$",
            "start": r"^start\s+(.*)$",
            "end": r"^end\s+(.*)$",
            "days_between": r"^days_between\s+(.*)$",
            "days_between_abs": r"^days_between_abs\s+(.*)$",
            "duration_years": r"^duration_years\s+(.*)$",
            "duration_months": r"^duration_months\s+(.*)$",
            "duration_days": r"^duration_days\s+(.*)$",
            "easter": r"^easter\s+(.*)$",
            "orthodox_easter": r"^orthodox_easter\s+(.*)$",
            "christmas": r"^christmas\s+(.*)$",
            "orthodox_christmas": r"^orthodox_christmas\s+(.*)$",
            "chinese_new_year": r"^chinese_new_year\s+(.*)$",
            "diwali": r"^diwali\s+(.*)$",
            "holi": r"^holi\s+(.*)$",
            "hindu_new_year": r"^hindu_new_year\s+(.*)$",
            "buddhist_new_year": r"^buddhist_new_year\s+(.*)$",
            "vesak": r"^vesak\s+(.*)$",
            "asalha_puja": r"^asalha_puja\s+(.*)$",
            "ramadan": r"^ramadan\s+(.*)$",
            "ramadan_starts_local": r"^ramadan_starts_local\s+(.*)$",
            "eid_al_fitr": r"^eid_al_fitr\s+(.*)$",
            "eid_al_fitr_starts_local": r"^eid_al_fitr_starts_local\s+(.*)$",
            "muslim_new_year": r"^muslim_new_year\s+(.*)$",
            "muslim_new_year_starts_local": r"^muslim_new_year_starts_local\s+(.*)$",
            "passover": r"^passover\s+(.*)$",
            "passover_starts_local": r"^passover_starts_local\s+(.*)$",
            "jewish_new_year": r"^jewish_new_year\s+(.*)$",
            "jewish_new_year_starts_local": r"^jewish_new_year_starts_local\s+(.*)$",
            "ethiopian_new_year": r"^ethiopian_new_year\s+(.*)$",
            "genna": r"^genna\s+(.*)$",
            "timkat": r"^timkat\s+(.*)$",
            "meskel": r"^meskel\s+(.*)$",
            "fasika": r"^fasika\s+(.*)$",
            "cherokee_new_moon_festival": r"^cherokee_new_moon_festival\s+(.*)$",
            "cherokee_green_corn_ceremony": r"^cherokee_green_corn_ceremony\s+(.*)$",
            "cherokee_ripe_corn_ceremony": r"^cherokee_ripe_corn_ceremony\s+(.*)$",
            "cherokee_great_new_moon_festival": r"^cherokee_great_new_moon_festival\s+(.*)$",
            "mayan_haab_new_year": r"^mayan_haab_new_year\s+(.*)$",
            "mayan_wayeb_start": r"^mayan_wayeb_start\s+(.*)$",
            "aztec_xiuhpohualli_new_year": r"^aztec_xiuhpohualli_new_year\s+(.*)$",
            "aztec_nemontemi_start": r"^aztec_nemontemi_start\s+(.*)$",
        },
    )


def parse_almanac_lab_output(output: str) -> dict[str, str]:
    fields = parse_keyed_output(
        output,
        {
            "date": r"^date\s+(.*)$",
            "time": r"^time\s+(.*)$",
            "zone": r"^zone\s+(.*)$",
            "latitude": r"^latitude\s+(.*)$",
            "longitude": r"^longitude\s+(.*)$",
            "body": r"^body\s+(.*)$",
            "gha_aries": r"^gha_aries\s+(.*)$",
            "selected_name": r"^selected_name\s+(.*)$",
            "selected_kind": r"^selected_kind\s+(.*)$",
            "selected_declination": r"^selected_declination\s+(.*)$",
            "selected_right_ascension": r"^selected_right_ascension\s+(.*)$",
            "selected_gha": r"^selected_gha\s+(.*)$",
            "selected_sha": r"^selected_sha\s+(.*)$",
            "selected_lha": r"^selected_lha\s+(.*)$",
            "selected_geo_distance": r"^selected_geo_distance\s+(.*)$",
            "selected_helio_distance": r"^selected_helio_distance\s+(.*)$",
            "selected_phase": r"^selected_phase\s+(.*)$",
            "selected_visual_magnitude": r"^selected_visual_magnitude\s+(.*)$",
        },
    )
    snapshot_lines = []
    for line in output.splitlines():
        match = re.match(r"^snapshot\s+(.*)$", line)
        if match:
            snapshot_lines.append(match.group(1).rstrip())
    if snapshot_lines:
        fields["snapshot"] = "\n".join(snapshot_lines)
    return fields


def parse_holiday_lab_output(output: str) -> dict[str, str]:
    return parse_keyed_output(
        output,
        {
            "bank_holiday": r"^bank_holiday\s+(.*)$",
            "holiday_status": r"^holiday_status\s+(.*)$",
            "jurisdiction_latitude": r"^jurisdiction_latitude\s+(.*)$",
            "jurisdiction_longitude": r"^jurisdiction_longitude\s+(.*)$",
            "jurisdiction_gmt_offset": r"^jurisdiction_gmt_offset\s+(.*)$",
            "jurisdiction_status": r"^jurisdiction_status\s+(.*)$",
            "jurisdiction_gmt_offset_status": r"^jurisdiction_gmt_offset_status\s+(.*)$",
        },
        {"bank_holiday"},
    )


def holiday_install_hint() -> str:
    return "Jurisdiction database unavailable. Run `make install-jurisdiction-db` to enable local holiday lookups."


WEATHER_HISTORY_START = py_datetime.date(2010, 1, 1)
WEATHER_FORECAST_WINDOW_DAYS = 14
WEATHER_FUTURE_WINDOW_DAYS = 300
WEATHER_REQUEST_TIMEOUT_SECONDS = 4.0
WEATHER_TOTAL_BUDGET_SECONDS = 0.8
WEATHER_API_BASE_URL = "https://api.weatherapi.com/v1"
WEATHER_API_KEY_ENV = "MARS_WEATHER_API_KEY"
LEGACY_WEATHER_API_KEY_ENV = "WEATHERAPI_KEY"
WEATHER_CONFIG_FILE = "weather.env"


def weather_api_key() -> str:
    for env_name in (WEATHER_API_KEY_ENV, LEGACY_WEATHER_API_KEY_ENV):
        value = os.environ.get(env_name, "").strip()
        if value:
            return value
    for env_name in (WEATHER_API_KEY_ENV, LEGACY_WEATHER_API_KEY_ENV):
        value = read_env_like_value(config_env_path(WEATHER_CONFIG_FILE), env_name)
        if value:
            return value
    return ""


def weather_relative_day_span(date_text: str) -> int | None:
    try:
        selected_date = py_datetime.date.fromisoformat(str(date_text or "").strip())
    except ValueError:
        return None
    today = py_datetime.date.today()
    return (selected_date - today).days


def weather_date_is_supported(date_text: str) -> bool:
    day_span: int | None
    selected_date: py_datetime.date

    day_span = weather_relative_day_span(date_text)

    if day_span is None:
        return False
    selected_date = py_datetime.date.fromisoformat(str(date_text or "").strip())
    if selected_date < WEATHER_HISTORY_START:
        return False
    if day_span < 0:
        return True
    if day_span <= (WEATHER_FORECAST_WINDOW_DAYS - 1):
        return True
    return day_span <= WEATHER_FUTURE_WINDOW_DAYS


def format_celsius_text(value: object) -> str:
    try:
        numeric = float(value)
    except (TypeError, ValueError):
        return ""
    text = f"{numeric:.1f}".rstrip("0").rstrip(".")
    return f"{text}°C"


def fetch_daily_weather_for_datetime(date_text: str,
                                     latitude: float,
                                     longitude: float) -> dict[str, str] | None:
    day_payload: dict[str, object]
    forecast_payload: dict[str, object]
    forecast_days: list[object]
    query: str
    request: urllib.request.Request
    endpoint: str
    api_key: str

    day_span = weather_relative_day_span(date_text)

    if day_span is None or not weather_date_is_supported(date_text):
        return None
    api_key = weather_api_key()
    if not api_key:
        return None

    endpoint = "history.json" if day_span < 0 else ("forecast.json" if day_span <= (WEATHER_FORECAST_WINDOW_DAYS - 1) else "future.json")
    query_params = {
        "key": api_key,
        "q": f"{latitude:.6f},{longitude:.6f}",
        "dt": date_text,
    }
    if endpoint == "forecast.json":
        query_params["days"] = str(min(WEATHER_FORECAST_WINDOW_DAYS, day_span + 1))
    query = urllib.parse.urlencode(query_params)
    request = urllib.request.Request(
        f"{WEATHER_API_BASE_URL}/{endpoint}?{query}",
        headers={
            "Accept": "application/json",
            "User-Agent": "MARS-Lab/1.0",
        },
    )
    try:
        with urllib.request.urlopen(request, timeout=WEATHER_REQUEST_TIMEOUT_SECONDS) as response:
            payload = json.loads(response.read().decode("utf-8"))
    except (OSError, ValueError, urllib.error.URLError):
        return None

    forecast_payload = payload.get("forecast") if isinstance(payload, dict) else None
    if not isinstance(forecast_payload, dict):
        return None
    forecast_days = forecast_payload.get("forecastday")
    if not isinstance(forecast_days, list) or not forecast_days:
        return None
    if not isinstance(forecast_days[0], dict):
        return None
    if str(forecast_days[0].get("date") or "").strip() != date_text:
        return None
    day_payload = forecast_days[0].get("day")
    if not isinstance(day_payload, dict):
        return None

    min_text = format_celsius_text(day_payload.get("mintemp_c"))
    max_text = format_celsius_text(day_payload.get("maxtemp_c"))
    humidity_text = str(day_payload.get("avghumidity") or "").strip()
    rain_chance = str(day_payload.get("daily_chance_of_rain") or "").strip()
    max_wind_raw = day_payload.get("maxwind_kph")
    if not min_text or not max_text:
        return None
    if humidity_text and not humidity_text.endswith("%"):
        humidity_text = f"{humidity_text}%"
    if rain_chance and not rain_chance.endswith("%"):
        rain_chance = f"{rain_chance}%"
    try:
        max_wind_value = float(max_wind_raw)
        wind_text = f"{max_wind_value:.1f} km/h"
        if wind_text.endswith(".0 km/h"):
            wind_text = wind_text.replace(".0 km/h", " km/h")
    except (TypeError, ValueError):
        wind_text = ""

    return {
        "weather_min_c": min_text,
        "weather_max_c": max_text,
        "weather_humidity": humidity_text,
        "weather_wind": wind_text,
        "weather_rain_chance": rain_chance,
        "weather_summary": f"Min {min_text}, max {max_text}",
        "weather_source": "WeatherAPI.com",
    }


def fetch_daily_weather_with_budget(date_text: str,
                                    latitude: float,
                                    longitude: float) -> dict[str, str] | None:
    result: dict[str, str] | None = None
    finished = threading.Event()

    def worker() -> None:
        nonlocal result
        try:
            result = fetch_daily_weather_for_datetime(date_text, latitude, longitude)
        finally:
            finished.set()

    thread = threading.Thread(target=worker, daemon=True)
    thread.start()
    if not finished.wait(WEATHER_TOTAL_BUDGET_SECONDS):
        return None
    return result


def _trim_decimal_tail(text: str) -> str:
    mantissa, sep, exponent = text.partition("E")

    if "." in mantissa:
        mantissa = mantissa.rstrip("0").rstrip(".")
    if mantissa in ("-0", "+0"):
        mantissa = "0"
    return mantissa + (sep + exponent if sep else "")


def format_number_text_for_precision(
    text: str,
    precision: int,
    zero_subprecision: bool = False,
) -> str:
    text = str(text or "").strip()
    if not text:
        return text

    upper = text.upper()
    if upper in {"NAN", "+NAN", "-NAN", "INF", "+INF", "-INF", "INFINITY", "+INFINITY", "-INFINITY"}:
        return text

    match = re.match(r"^(.+?)\s+([+-])\s+(.+)i$", text)
    if match:
        real = format_number_text_for_precision(
            match.group(1), precision, zero_subprecision)
        imag = format_number_text_for_precision(
            match.group(3), precision, zero_subprecision)
        return f"{real} {match.group(2)} {imag}i"

    try:
        with localcontext() as ctx:
            ctx.prec = max(1, min(MAX_VALUE_PRECISION_DIGITS, int(precision)))
            rounded = +Decimal(text)
    except (InvalidOperation, ValueError):
        return text

    if zero_subprecision and rounded and rounded.copy_abs().adjusted() < -int(precision):
        return "0"
    return _trim_decimal_tail(format(rounded, "g").replace("e", "E"))


def render_tex_to_svg(tex: str) -> tuple[str | None, str | None]:
    if not tex:
        return None, None

    missing_tools = [
        command for command in ("latex", "dvisvgm")
        if shutil.which(command) is None
    ]
    if missing_tools:
        return (
            None,
            "Missing TeX rendering tool(s): "
            + ", ".join(missing_tools)
            + ". On Debian/Ubuntu, install: sudo apt install texlive-latex-base dvisvgm",
        )

    document = rf"""\documentclass{{article}}
\pagestyle{{empty}}
\usepackage{{amsmath}}
\begin{{document}}
\[
{tex}
\]
\end{{document}}
"""

    with tempfile.TemporaryDirectory(prefix="mars-expr-tex-") as tmp_name:
        tmp = Path(tmp_name)
        tex_file = tmp / "expr.tex"
        dvi_file = tmp / "expr.dvi"
        svg_file = tmp / "expr.svg"
        tex_file.write_text(document, encoding="utf-8")

        latex = subprocess.run(
            ["latex", "-interaction=nonstopmode", "-halt-on-error", tex_file.name],
            cwd=tmp,
            text=True,
            capture_output=True,
            timeout=10,
        )
        if latex.returncode != 0:
            return None, latex.stdout + latex.stderr

        dvisvgm = subprocess.run(
            ["dvisvgm", "--no-fonts", "--exact-bbox", str(dvi_file), "-o", str(svg_file)],
            cwd=tmp,
            text=True,
            capture_output=True,
            timeout=10,
        )
        if dvisvgm.returncode != 0:
            return None, dvisvgm.stdout + dvisvgm.stderr

        try:
            return svg_file.read_text(encoding="utf-8"), None
        except OSError as exc:
            return None, str(exc)


def split_top_level_text(text: str, separator: str) -> list[str]:
    parts: list[str] = []
    start = 0
    depth = 0

    for i, ch in enumerate(text):
        if ch in "([{":
            depth += 1
        elif ch in ")]}":
            depth = max(0, depth - 1)
        elif ch == separator and depth == 0:
            parts.append(text[start:i])
            start = i + 1

    parts.append(text[start:])
    return parts


def index_top_level_text(text: str, needle: str) -> int:
    depth = 0

    for i, ch in enumerate(text):
        if ch in "([{":
            depth += 1
        elif ch in ")]}":
            depth = max(0, depth - 1)
        elif ch == needle and depth == 0:
            return i

    return -1


def last_index_top_level_text(text: str, needle: str) -> int:
    depth = 0
    found = -1

    for i, ch in enumerate(text):
        if ch in "([{":
            depth += 1
        elif ch in ")]}":
            depth = max(0, depth - 1)
        elif ch == needle and depth == 0:
            found = i

    return found


def parse_expression_body(expression: str) -> tuple[str, str, str]:
    text = expression.strip()

    if text.startswith("{") and text.endswith("}"):
        text = text[1:-1].strip()

    pipe = last_index_top_level_text(text, "|")
    if pipe < 0:
        return text, "", ""

    body = text[:pipe].strip()
    bindings = text[pipe + 1:].strip()
    semi = index_top_level_text(bindings, ";")
    if semi < 0:
        return body, bindings.strip(), ""

    return body, bindings[:semi].strip(), bindings[semi + 1:].strip()


def parse_binding_assignments(bindings: str) -> list[tuple[str, str]]:
    out: list[tuple[str, str]] = []

    for part in split_top_level_text(bindings, ","):
        eq = index_top_level_text(part, "=")
        if eq < 0:
            continue
        name = part[:eq].strip()
        value = part[eq + 1:].strip()
        if name:
            out.append((name, value))

    return out


def binding_name_sort_key(name: str) -> tuple[str, str]:
    text = str(name or "").strip()
    return text.casefold(), text


def sorted_binding_assignments(assignments: list[tuple[str, str]]) -> list[tuple[str, str]]:
    return sorted(assignments, key=lambda item: binding_name_sort_key(item[0]))


def sorted_assignment_parts(parts: list[str]) -> list[str]:
    def part_key(part: str) -> tuple[str, str]:
        eq = index_top_level_text(part, "=")
        name = part[:eq].strip() if eq >= 0 else part.strip()
        return binding_name_sort_key(name)

    return sorted(parts, key=part_key)


def expression_with_sorted_constants(expression: str) -> str:
    text = str(expression or "").strip()
    if not text:
        return text

    body, var_text, const_text = parse_expression_body(text)
    if not const_text:
        return text

    var_parts = [
        part.strip()
        for part in split_top_level_text(var_text, ",")
        if part.strip()
    ]
    const_parts = sorted_assignment_parts([
        part.strip()
        for part in split_top_level_text(const_text, ",")
        if part.strip()
    ])

    binding_text = ", ".join(var_parts)
    if const_parts:
        const_binding_text = ", ".join(const_parts)
        binding_text = f"{binding_text}; {const_binding_text}" if binding_text else f"; {const_binding_text}"

    return f"{{ {body} | {binding_text} }}"


def restore_source_constant_spellings(expression: str, source_expression: str) -> str:
    text = str(expression or "").strip()
    source_text = str(source_expression or "").strip()
    if not text or not source_text:
        return text

    body, var_text, const_text = parse_expression_body(text)
    _, _, source_const_text = parse_expression_body(source_text)
    source_constants = {
        name: value
        for name, value in parse_binding_assignments(source_const_text)
        if name and value and value != "?" and value.upper() != "NAN"
    }
    if not source_constants or not const_text:
        return text

    changed = False
    const_parts: list[str] = []
    for part in split_top_level_text(const_text, ","):
        eq = index_top_level_text(part, "=")
        if eq < 0:
            stripped = part.strip()
            if stripped:
                const_parts.append(stripped)
            continue
        name = part[:eq].strip()
        value = part[eq + 1:].strip()
        source_value = source_constants.get(name)
        if source_value and value != source_value:
            const_parts.append(f"{name} = {source_value}")
            changed = True
        else:
            stripped = part.strip()
            if stripped:
                const_parts.append(stripped)

    if not changed:
        return text

    binding_text = var_text.strip()
    if const_parts:
        const_binding_text = ", ".join(sorted_assignment_parts(const_parts))
        binding_text = f"{binding_text}; {const_binding_text}" if binding_text else f"; {const_binding_text}"
    return f"{{ {body} | {binding_text} }}"


_SUPERSCRIPT_DIGIT_MAP = str.maketrans("⁰¹²³⁴⁵⁶⁷⁸⁹⁺⁻", "0123456789+-")
_SUBSCRIPT_DIGIT_MAP = str.maketrans("₀₁₂₃₄₅₆₇₈₉₊₋", "0123456789+-")


def _plain_numeric_to_tex_literal(text: str) -> str:
    source = str(text or "").strip()
    match = re.fullmatch(r"([⁰¹²³⁴⁵⁶⁷⁸⁹⁺⁻]+)⁄([₀₁₂₃₄₅₆₇₈₉₊₋]+)", source)
    if match:
        numerator = match.group(1).translate(_SUPERSCRIPT_DIGIT_MAP)
        denominator = match.group(2).translate(_SUBSCRIPT_DIGIT_MAP)
        return rf"\frac{{{numerator}}}{{{denominator}}}"
    return source


def source_constant_replacements(
    source_expression: str,
    solved_expression: str,
) -> list[tuple[str, str, str]]:
    _, _, source_const_text = parse_expression_body(source_expression)
    _, _, solved_const_text = parse_expression_body(solved_expression)
    source_constants = {
        name: value
        for name, value in parse_binding_assignments(source_const_text)
        if name and value and value != "?" and value.upper() != "NAN"
    }
    solved_constants = {
        name: value
        for name, value in parse_binding_assignments(solved_const_text)
        if name and value and value != "?" and value.upper() != "NAN"
    }
    replacements: list[tuple[str, str, str]] = []
    for name, source_value in source_constants.items():
        solved_value = solved_constants.get(name)
        if solved_value and solved_value != source_value:
            replacements.append((solved_value, source_value, _plain_numeric_to_tex_literal(solved_value)))
    return sorted(replacements, key=lambda item: len(item[0]), reverse=True)


def replace_source_constant_spellings_in_text(
    text: str,
    source_expression: str,
    solved_expression: str,
) -> str:
    rendered = str(text or "")
    for exact_value, source_value, _ in source_constant_replacements(source_expression, solved_expression):
        rendered = rendered.replace(exact_value, source_value)
    return rendered


def replace_source_constant_spellings_in_tex(
    tex: str,
    source_expression: str,
    solved_expression: str,
) -> str:
    rendered = str(tex or "")
    for exact_value, source_value, tex_literal in source_constant_replacements(source_expression, solved_expression):
        if tex_literal and tex_literal != exact_value:
            rendered = rendered.replace(tex_literal, source_value)
        rendered = rendered.replace(exact_value, source_value)
    return rendered


def numeric_equation_solution_lines(
    binary: Path,
    solutions_text: str,
    precision: int,
) -> list[str]:
    lines: list[str] = []
    for line in normalize_multiline_display_text(solutions_text).splitlines():
        lhs, sep, rhs = line.partition("=")
        if not sep:
            continue
        name = lhs.strip()
        expression = rhs.strip().replace("·", "*")
        if not name or not expression:
            continue
        try:
            fields, _, returncode = run_mars_lab_fields(binary, expression, precision)
        except Exception:
            continue
        if returncode != 0:
            continue
        value = str(fields.get("value") or "").strip()
        if not value:
            continue
        formatted = format_number_text_for_precision(value, precision, zero_subprecision=True)
        if not numeric_solution_line_is_finite(formatted):
            continue
        if formatted and formatted != expression:
            lines.append(f"{name} ≈ {formatted}")
    return lines


def numeric_solution_line_is_finite(line: str) -> bool:
    text = str(line or "")
    return not re.search(r"(?i)(?:^|[^A-Z])(?:NAN|[+-]?INF(?:INITY)?)(?:[^A-Z]|$)", text)


def equation_lab_numeric_solution_lines(fields: dict[str, str], precision: int) -> list[str]:
    lines = [
        line.strip()
        for line in normalize_multiline_display_text(fields.get("numeric") or "").splitlines()
        if line.strip()
    ]
    formatted_lines = [precision_numeric_tokens(line, precision) for line in lines]
    return [line for line in formatted_lines if numeric_solution_line_is_finite(line)]


def restore_compact_binding_values(expression: str, source_expression: str) -> str:
    if "..." not in expression or not source_expression or "..." in source_expression:
        return expression

    body, var_text, const_text = parse_expression_body(expression)
    _, source_var_text, source_const_text = parse_expression_body(source_expression)
    source_values = {
        name: value
        for name, value in (
            parse_binding_assignments(source_var_text)
            + parse_binding_assignments(source_const_text)
        )
    }
    if not source_values:
        return expression

    changed = False

    def restore_assignments(assignments: str) -> list[str]:
        nonlocal changed
        out: list[str] = []
        for part in split_top_level_text(assignments, ","):
            eq = index_top_level_text(part, "=")
            if eq < 0:
                stripped = part.strip()
                if stripped:
                    out.append(stripped)
                continue

            name = part[:eq].strip()
            value = part[eq + 1:].strip()
            cached = source_values.get(name)
            if cached and value.endswith("...") and cached.startswith(value[:-3]):
                changed = True
                value = cached
            out.append(f"{name} = {value}")
        return out

    var_parts = restore_assignments(var_text)
    const_parts = sorted_assignment_parts(restore_assignments(const_text))
    binding_text = ", ".join(var_parts)
    if const_parts:
        const_binding_text = ", ".join(const_parts)
        binding_text = f"{binding_text}; {const_binding_text}" if binding_text else f"; {const_binding_text}"

    return f"{{ {body} | {binding_text} }}" if changed else expression


def expression_with_binding_value(expression: str, target_name: str, value_text: str) -> str | None:
    body, var_text, const_text = parse_expression_body(expression)
    changed = False

    def replace_assignments(assignments: str) -> list[str]:
        nonlocal changed
        out: list[str] = []
        for part in split_top_level_text(assignments, ","):
            eq = index_top_level_text(part, "=")
            if eq < 0:
                stripped = part.strip()
                if stripped:
                    out.append(stripped)
                continue

            name = part[:eq].strip()
            value = part[eq + 1:].strip()
            if name == target_name:
                value = value_text
                changed = True
            out.append(f"{name} = {value}")
        return out

    var_parts = replace_assignments(var_text)
    const_parts = sorted_assignment_parts(replace_assignments(const_text))
    if not changed:
        return None

    binding_text = ", ".join(var_parts)
    if const_parts:
        const_binding_text = ", ".join(const_parts)
        binding_text = f"{binding_text}; {const_binding_text}" if binding_text else f"; {const_binding_text}"

    return f"{{ {body} | {binding_text} }}"


def expression_without_bindings_for_names(expression: str, names: set[str]) -> str:
    text = str(expression or "").strip()
    target_names = {str(name or "").strip() for name in names if str(name or "").strip()}

    if not text or not target_names:
        return text

    body, var_text, const_text = parse_expression_body(text)
    if not var_text and not const_text:
        return body or text

    def filtered_parts(assignments: str) -> list[str]:
        out: list[str] = []

        for part in split_top_level_text(assignments, ","):
            eq = index_top_level_text(part, "=")
            stripped = part.strip()

            if eq < 0:
                if stripped and stripped not in target_names:
                    out.append(stripped)
                continue

            name = part[:eq].strip()
            if name and name not in target_names:
                out.append(stripped)

        return out

    var_parts = filtered_parts(var_text)
    const_parts = sorted_assignment_parts(filtered_parts(const_text))
    binding_text = ", ".join(var_parts)
    if const_parts:
        const_binding_text = ", ".join(const_parts)
        binding_text = f"{binding_text}; {const_binding_text}" if binding_text else f"; {const_binding_text}"

    return f"{{ {body} | {binding_text} }}" if binding_text else body


def binding_syntax_error_details(raw: str) -> tuple[str, str] | None:
    first_line = str(raw or "").strip().splitlines()[0] if str(raw or "").strip() else ""
    match = re.match(r"^incorrect syntax for ([^:]+):\s*(.*)$", first_line)
    if not match:
        return None
    return match.group(1).strip(), match.group(2).strip()


def run_mars_lab_fields(
    binary: Path,
    expression: str,
    precision: int,
    wrt: str = "x",
    action: str = "",
) -> tuple[dict[str, str], str, int]:
    command = [str(binary), expression, wrt, str(max(17, precision))]
    if action:
        command.append(action)
    completed = subprocess.run(
        command,
        cwd=ROOT,
        text=True,
        capture_output=True,
        timeout=10,
    )
    raw = completed.stdout
    if completed.stderr:
        raw = raw + ("\n" if raw else "") + completed.stderr
    return parse_mars_lab_output(raw), raw, completed.returncode


def run_matrix_lab_fields(
    binary: Path,
    matrix_text: str,
    operation: str,
    precision: int,
    operand: str = "",
) -> tuple[dict[str, str], str, int]:
    command = [str(binary), matrix_text, operation, str(max(17, precision))]
    operand = str(operand or "").strip()
    if operand:
        command.append(operand)

    completed = subprocess.run(
        command,
        cwd=ROOT,
        text=True,
        capture_output=True,
        timeout=10,
    )
    raw = completed.stdout
    if completed.stderr:
        raw = raw + ("\n" if raw else "") + completed.stderr
    return parse_matrix_lab_output(raw), raw, completed.returncode


def run_integrator_lab_fields(
    binary: Path,
    expression: str,
    bounds: list[dict[str, str]],
    precision: int,
    max_intervals: int | None = None,
) -> tuple[dict[str, str], str, int]:
    effective_cap = max(MIN_INTEGRATOR_INTERVAL_CAP, min(MAX_INTEGRATOR_INTERVAL_CAP, int(max_intervals))) if max_intervals is not None else DEFAULT_INTEGRATOR_INTERVAL_CAP
    command = [str(binary)]
    if max_intervals is not None:
        command.extend(["--max-intervals", str(effective_cap)])
    command.extend([expression, str(max(17, precision))])
    for bound in bounds:
        command.extend([
            str(bound.get("name", "")).strip(),
            str(bound.get("lo", "")).strip(),
            str(bound.get("hi", "")).strip(),
        ])

    completed = subprocess.run(
        command,
        cwd=ROOT,
        text=True,
        capture_output=True,
        timeout=min(120, max(10, effective_cap // 500)),
    )
    raw = completed.stdout
    if completed.stderr:
        raw = raw + ("\n" if raw else "") + completed.stderr
    return parse_integrator_lab_output(raw), raw, completed.returncode


def run_equation_lab_fields(
    binary: Path,
    equation_text: str,
    precision: int,
) -> tuple[dict[str, str], str, int]:
    command = [
        str(binary),
        equation_text,
        str(max(17, precision)),
    ]
    completed = subprocess.run(
        command,
        cwd=ROOT,
        text=True,
        capture_output=True,
        timeout=10,
    )
    raw = completed.stdout
    if completed.stderr:
        raw = raw + ("\n" if raw else "") + completed.stderr
    return parse_equation_lab_output(raw), raw, completed.returncode


def run_datetime_lab_fields(
    binary: Path,
    options: dict[str, str],
) -> tuple[dict[str, str], str, int]:
    command = [str(binary)]
    for key in (
        "date",
        "jdn",
        "start",
        "end",
        "year",
        "lat",
        "lon",
        "elevation",
        "gmt_offset",
        "jurisdiction",
    ):
        if key in options:
            command.append(f"{key}={str(options.get(key, '')).strip()}")
    completed = subprocess.run(
        command,
        cwd=ROOT,
        text=True,
        capture_output=True,
        timeout=10,
    )
    raw = completed.stdout
    if completed.stderr:
        raw = raw + ("\n" if raw else "") + completed.stderr
    return parse_datetime_lab_output(raw), raw, completed.returncode


def run_almanac_lab_fields(
    binary: Path,
    options: dict[str, str],
) -> tuple[dict[str, str], str, int]:
    command = [str(binary)]
    for key in ("date", "time", "zone", "lat", "lon", "body"):
        if key in options:
            command.append(f"{key}={str(options.get(key, '')).strip()}")
    completed = subprocess.run(
        command,
        cwd=ROOT,
        text=True,
        capture_output=True,
        timeout=10,
    )
    raw = completed.stdout
    if completed.stderr:
        raw = raw + ("\n" if raw else "") + completed.stderr
    return parse_almanac_lab_output(raw), raw, completed.returncode


def run_holiday_lab_fields(
    binary: Path,
    options: dict[str, str],
) -> tuple[dict[str, str], str, int]:
    command = [str(binary)]
    for key in ("start", "end", "jurisdiction"):
        if key in options:
            command.append(f"{key}={str(options.get(key, '')).strip()}")

    child_env = os.environ.copy()
    child_env.update(jurisdiction_db_runtime_env())
    completed = subprocess.run(
        command,
        cwd=ROOT,
        env=child_env,
        text=True,
        capture_output=True,
        timeout=10,
    )
    raw = completed.stdout
    if completed.stderr:
        raw = raw + ("\n" if raw else "") + completed.stderr
    return parse_holiday_lab_output(raw), raw, completed.returncode


def matrix_failure_hint(
    binary: Path,
    matrix_text: str,
    operation: str,
    precision: int,
    operand: str = "",
) -> str:
    if operation != "inverse":
        return ""

    try:
        eval_fields, _, eval_rc = run_matrix_lab_fields(
            binary, matrix_text, "eval", precision, ""
        )
    except Exception:
        return ""

    if eval_rc != 0:
        return "This matrix still has unresolved symbolic entries. Bind variables first or use Evaluate."

    rows = str(eval_fields.get("rows", "")).strip()
    cols = str(eval_fields.get("cols", "")).strip()
    if rows and cols and rows != cols:
        return (
            f"This matrix is {rows}x{cols}, so it has no inverse. "
            "Only square n x n matrices can be inverted."
        )

    try:
        det_fields, _, det_rc = run_matrix_lab_fields(
            binary, matrix_text, "det", precision, ""
        )
    except Exception:
        return ""

    if det_rc == 0 and str(det_fields.get("value", "")).strip() == "0":
        return "This matrix is singular: det(A) = 0, so it has no inverse. Equivalently, 0 is an eigenvalue."

    return "The inverse operation failed for this matrix."


def expression_variable_binding_values(
    expression: str,
    precision: int | None = None,
) -> list[dict[str, str]]:
    _, var_text, const_text = parse_expression_body(expression)
    values: list[dict[str, str]] = []

    for name, value in parse_binding_assignments(var_text):
        if not value or value == "?" or value.upper() == "NAN":
            values.append({
                "name": name,
                "value": value or "?",
                "display": "",
                "kind": "variable",
            })
            continue

        display_value = precision_numeric_tokens(value, precision) if precision is not None else value
        values.append({
            "name": name,
            "value": display_value,
            "display": _compact_long_text_value(display_value),
            "kind": "variable",
        })

    for name, value in sorted_binding_assignments(parse_binding_assignments(const_text)):
        display_value = value or "?"
        display = ""
        if display_value != "?" and display_value.upper() != "NAN":
            display = _compact_long_text_value(display_value)
        values.append({
            "name": name,
            "value": display_value,
            "display": display,
            "kind": "constant",
        })

    return values


def goal_seek_expression(
    binary: Path,
    expression: str,
    target_text: str,
    precision: int,
    start_values: object = None,
) -> tuple[str, dict[str, str]]:
    command = [
        str(binary),
        "--goal-seek",
        expression,
        target_text,
        str(max(17, precision)),
    ]

    if start_values:
        if not isinstance(start_values, dict):
            raise ValueError("Start values must be supplied as binding assignments")
        for name, value in start_values.items():
            name_text = str(name).strip()
            value_text = str(value).strip()
            if not name_text or not value_text:
                continue
            command.append(f"{name_text}={value_text}")

    completed = subprocess.run(
        command,
        cwd=ROOT,
        text=True,
        capture_output=True,
        timeout=10,
    )
    raw = completed.stdout
    if completed.stderr:
        raw = raw + ("\n" if raw else "") + completed.stderr

    fields = parse_mars_lab_output(raw)
    if completed.returncode != 0:
        raise ValueError(raw or f"mars_lab exited with {completed.returncode}")

    expression_out = fields.get("expression", "").strip()
    if not expression_out:
        raise ValueError(raw or "Goal seek did not return an expression")
    return expression_out, fields


def prepare_evaluation_fields(
    binary: Path,
    fields: dict[str, str],
    expression: str,
    precision: int,
    save_expression: bool,
    wrt: str = "x",
) -> dict[str, object]:
    if fields.get("value"):
        fields["value"] = format_number_text_for_precision(
            fields["value"], precision, zero_subprecision=True)
    if fields.get("derivative_value"):
        fields["derivative_value"] = format_number_text_for_precision(
            fields["derivative_value"], precision, zero_subprecision=True
        )
    if fields.get("integral_value"):
        fields["integral_value"] = format_number_text_for_precision(
            fields["integral_value"], precision, zero_subprecision=True
        )

    precision_limit_result_fields(fields, precision)
    fields["editor_expression"] = editor_expression_from_fields(fields)
    if save_expression and fields.get("expression"):
        save_state_expression(expression_for_editor(expression))

    display_expression_source = fields.get("unbound", "") or fields.get("expression", "")
    fields["full_display_expression"] = expression_for_display(display_expression_source)
    fields["full_display_tex"] = tex_for_display(fields.get("tex", ""))
    fields["full_display_function"] = function_for_display(fields.get("function", ""))
    fields["display_expression"] = compact_display_text(str(fields["full_display_expression"]))
    fields["display_tex"] = compact_display_text(str(fields["full_display_tex"]))
    fields["display_function"] = compact_function_text(str(fields["full_display_function"]))
    fields["full_display_derivative_function"] = function_for_display(
        fields.get("derivative_function", "")
    )
    fields["display_derivative_function"] = compact_function_text(
        str(fields["full_display_derivative_function"])
    )
    derivative_tex = tex_for_display(str(fields.get("derivative_tex") or ""))
    fields["derivative_tex"] = derivative_tex
    if derivative_tex:
        derivative_svg, derivative_render_error = render_tex_to_svg(derivative_tex)
        if derivative_svg:
            fields["derivative_svg"] = derivative_svg
        elif derivative_render_error:
            fields["derivative_render_error"] = derivative_render_error
    fields["full_display_integral_function"] = function_for_display(
        fields.get("integral_function", "")
    )
    fields["display_integral_function"] = compact_function_text(
        str(fields["full_display_integral_function"])
    )
    integral_tex = tex_for_display(str(fields.get("integral_tex") or ""))
    fields["integral_tex"] = integral_tex
    if integral_tex:
        integral_svg, integral_render_error = render_tex_to_svg(integral_tex)
        if integral_svg:
            fields["integral_svg"] = integral_svg
        elif integral_render_error:
            fields["integral_render_error"] = integral_render_error
    fields["binding_values"] = expression_variable_binding_values(
        fields.get("expression", "") or expression,
        precision,
    )

    svg, render_error = render_tex_to_svg(str(fields.get("display_tex", "")))
    if svg:
        fields["svg"] = svg
    elif render_error:
        fields["render_error"] = render_error

    return fields


def prepare_matrix_fields(fields: dict[str, str], precision: int) -> dict[str, object]:
    result_text = str(fields.get("result") or fields.get("value") or "").strip()
    pretty_text = str(fields.get("pretty") or "").strip()
    tex = str(fields.get("tex") or "").strip()
    operation = str(fields.get("operation") or "eval").strip()
    kind = str(fields.get("kind") or "").strip()
    rows = str(fields.get("rows") or "").strip()
    cols = str(fields.get("cols") or "").strip()
    input_text = str(fields.get("input") or "").strip()

    svg = None
    render_error = None
    if tex and tex != "(null)":
        svg, render_error = render_tex_to_svg(tex)

    summary_parts = []
    if kind:
        summary_parts.append(kind)
    if rows and cols:
        summary_parts.append(f"{rows}x{cols}")
    if operation:
        summary_parts.append(operation)

    payload: dict[str, object] = {
        "ok": True,
        "mode": "matrix",
        "operation": operation,
        "result": result_text,
        "pretty": pretty_text,
        "tex": "" if tex == "(null)" else tex,
        "summary": " · ".join(summary_parts),
        "binding_values": expression_variable_binding_values(input_text, precision),
    }
    if svg:
        payload["svg"] = svg
    elif render_error:
        payload["render_error"] = render_error
    return payload


def integrator_bound_rows_from_fields(fields: dict[str, str]) -> list[dict[str, str]]:
    names = [line.strip() for line in str(fields.get("bound_var") or "").splitlines() if line.strip()]
    los = [line.strip() for line in str(fields.get("bound_lower") or "").splitlines()]
    his = [line.strip() for line in str(fields.get("bound_upper") or "").splitlines()]
    rows: list[dict[str, str]] = []

    for index, name in enumerate(names):
        rows.append({
            "kind": "bound",
            "name": name,
            "lo": los[index] if index < len(los) else "",
            "hi": his[index] if index < len(his) else "",
        })
    return rows


def prepare_integrator_fields(fields: dict[str, str], precision: int) -> dict[str, object]:
    if fields.get("value"):
        fields["value"] = format_number_text_for_precision(
            str(fields["value"]), precision, zero_subprecision=True)
    if fields.get("error"):
        fields["error"] = format_number_text_for_precision(
            str(fields["error"]), precision, zero_subprecision=True)
    if fields.get("symbolic_value"):
        fields["symbolic_value"] = format_number_text_for_precision(
            str(fields["symbolic_value"]), precision, zero_subprecision=True)
    if fields.get("error"):
        fields["error"] = format_number_text_for_precision(
            str(fields["error"]),
            min(int(precision), INTEGRATOR_ERROR_DISPLAY_DIGITS),
            zero_subprecision=False,
        )
    precision_limit_result_fields(fields, precision)

    tex = str(fields.get("tex") or "").strip()
    bounds = str(fields.get("bound") or "").strip()
    bound_rows = integrator_bound_rows_from_fields(fields)
    binding_expression = str(
        fields.get("binding_expression") or fields.get("input") or ""
    ).strip()
    tex = integrator_tex_for_display(tex)
    symbolic_text = str(fields.get("symbolic") or "").strip()
    symbolic_tex = str(fields.get("symbolic_tex") or "").strip()
    antiderivative_text = str(fields.get("antiderivative") or "").strip()
    antiderivative_tex = str(fields.get("antiderivative_tex") or "").strip()

    svg = None
    render_error = None
    if tex and tex != "(null)":
        svg, render_error = render_tex_to_svg(tex)

    payload: dict[str, object] = {
        "ok": True,
        "mode": "integrator",
        "expression": str(fields.get("expression") or "").strip(),
        "binding_expression": binding_expression,
        "tex": "" if tex == "(null)" else tex,
        "antiderivative": antiderivative_text,
        "antiderivative_tex": antiderivative_tex,
        "symbolic": symbolic_text,
        "symbolic_tex": symbolic_tex,
        "symbolic_value": str(fields.get("symbolic_value") or "").strip(),
        "value": str(fields.get("value") or "").strip(),
        "error": str(fields.get("error") or "").strip(),
        "error": str(fields.get("error") or "").strip(),
        "intervals": str(fields.get("intervals") or "").strip(),
        "max_intervals": str(fields.get("max_intervals") or "").strip(),
        "work_units": str(fields.get("work_units") or fields.get("intervals") or "").strip(),
        "work_cap": str(fields.get("work_cap") or fields.get("max_intervals") or "").strip(),
        "status": str(fields.get("status") or "").strip(),
        "dimensions": str(fields.get("dimensions") or "").strip(),
        "bound": bounds,
        "bounds": bound_rows,
        "bound_var": str(fields.get("bound_var") or "").strip().splitlines()[0] if str(fields.get("bound_var") or "").strip() else "",
        "bound_lower": str(fields.get("bound_lower") or "").strip().splitlines()[0] if str(fields.get("bound_lower") or "").strip() else "",
        "bound_upper": str(fields.get("bound_upper") or "").strip().splitlines()[0] if str(fields.get("bound_upper") or "").strip() else "",
        "binding_values": expression_variable_binding_values(binding_expression, precision),
    }
    if svg:
        payload["svg"] = svg
    elif render_error:
        payload["render_error"] = render_error
    return payload


def prepare_equation_fields(fields: dict[str, str], precision: int) -> dict[str, object]:
    if fields.get("value"):
        fields["value"] = format_number_text_for_precision(
            str(fields["value"]), precision, zero_subprecision=True)

    source_equation_text = str(fields.get("input") or "").strip()
    solved_equation_text = str(fields.get("equation") or "").strip()
    for key in ("equation", "unbound", "tex", "residual", "solutions", "solutions_tex"):
        value = str(fields.get(key) or "")
        if not value:
            continue
        fields[f"raw_{key}"] = value
        fields[key] = precision_numeric_tokens(value, precision)

    equation_text = restore_source_constant_spellings(
        expression_with_sorted_constants(str(fields.get("equation") or "").strip()),
        source_equation_text,
    )
    unbound_text = str(fields.get("unbound") or "").strip()
    solutions_text = replace_source_constant_spellings_in_text(
        normalize_multiline_display_text(fields.get("solutions") or ""),
        source_equation_text,
        solved_equation_text,
    )
    equation_tex = replace_source_constant_spellings_in_tex(
        str(fields.get("tex") or "").strip(),
        source_equation_text,
        solved_equation_text,
    )
    solutions_tex = replace_source_constant_spellings_in_tex(
        str(fields.get("solutions_tex") or "").strip(),
        source_equation_text,
        solved_equation_text,
    )
    render_tex = solutions_tex or equation_tex
    display_tex = compact_display_text(render_tex)
    solution_lines = [line.strip() for line in solutions_text.splitlines() if line.strip()]
    numeric_solution_lines = equation_lab_numeric_solution_lines(fields, precision)
    if not numeric_solution_lines:
        numeric_solution_lines = numeric_equation_solution_lines(
            DEFAULT_BIN,
            solutions_text,
            precision,
        )

    svg = None
    render_error = None
    if display_tex and display_tex != "(null)":
        svg, render_error = render_tex_to_svg(display_tex)

    payload: dict[str, object] = {
        "ok": True,
        "mode": "equation",
        "equation": equation_text,
        "unbound": unbound_text,
        "tex": "" if render_tex == "(null)" else render_tex,
        "equation_tex": "" if equation_tex == "(null)" else equation_tex,
        "solutions_tex": "" if solutions_tex == "(null)" else solutions_tex,
        "residual": str(fields.get("residual") or "").strip(),
        "value": str(fields.get("value") or "").strip(),
        "status": str(fields.get("status") or "").strip(),
        "solutions": solutions_text,
        "solution_count": len(solution_lines),
        "numeric_solutions": numeric_solution_lines,
        "full_display_equation": expression_for_display(unbound_text or equation_text),
        "display_equation": compact_display_text(expression_for_display(unbound_text or equation_text)),
        "full_display_tex": render_tex,
        "display_tex": display_tex,
        "binding_values": expression_variable_binding_values(source_equation_text or equation_text, precision),
    }
    if svg:
        payload["svg"] = svg
    elif render_error:
        payload["render_error"] = render_error
    return payload


def prepare_datetime_fields(fields: dict[str, str]) -> dict[str, object]:
    date = str(fields.get("date") or "").strip()
    weekday = str(fields.get("weekday") or "").strip()
    moon_phase = str(fields.get("moon_phase") or "").strip()
    sunrise = str(fields.get("sunrise") or "").strip()
    sunset = str(fields.get("sunset") or "").strip()
    sunrise_status = str(fields.get("sunrise_status") or "").strip()
    sunset_status = str(fields.get("sunset_status") or "").strip()
    actual_sunrise = str(fields.get("actual_sunrise") or "").strip()
    actual_sunset = str(fields.get("actual_sunset") or "").strip()
    actual_sunrise_status = str(fields.get("actual_sunrise_status") or "").strip()
    actual_sunset_status = str(fields.get("actual_sunset_status") or "").strip()
    dst_forward = str(fields.get("dst_forward") or "").strip()
    dst_back = str(fields.get("dst_back") or "").strip()
    dst_forward_from_offset = str(fields.get("dst_forward_from_offset") or "").strip()
    dst_forward_to_offset = str(fields.get("dst_forward_to_offset") or "").strip()
    dst_back_from_offset = str(fields.get("dst_back_from_offset") or "").strip()
    dst_back_to_offset = str(fields.get("dst_back_to_offset") or "").strip()
    dst_status = str(fields.get("dst_status") or "").strip()
    gmt_offset = str(fields.get("gmt_offset") or "").strip()
    weather_min_c = str(fields.get("weather_min_c") or "").strip()
    weather_max_c = str(fields.get("weather_max_c") or "").strip()
    weather_humidity = str(fields.get("weather_humidity") or "").strip()
    weather_wind = str(fields.get("weather_wind") or "").strip()
    weather_rain_chance = str(fields.get("weather_rain_chance") or "").strip()
    weather_summary = str(fields.get("weather_summary") or "").strip()
    weather_source = str(fields.get("weather_source") or "").strip()
    offset_text = "local machine GMT offset" if gmt_offset == "local" else f"GMT offset {gmt_offset}"

    def format_offset_text(text: str) -> str:
        if not text:
            return ""
        try:
            value = float(text)
        except ValueError:
            return text
        sign = "+" if value >= 0 else "-"
        abs_value = abs(value)
        hours = int(abs_value)
        minutes = int(round((abs_value - hours) * 60.0))
        if minutes == 60:
            hours += 1
            minutes = 0
        if minutes:
            return f"GMT{sign}{hours:01d}:{minutes:02d}"
        return f"GMT{sign}{hours}"

    def format_transition_text(text: str) -> str:
        try:
            parsed = py_datetime.datetime.strptime(text, "%Y-%m-%d %H:%M")
        except ValueError:
            return text
        day = parsed.day
        if 10 <= day % 100 <= 20:
            suffix = "th"
        else:
            suffix = {1: "st", 2: "nd", 3: "rd"}.get(day % 10, "th")
        return parsed.strftime("%A ") + f"{day}{suffix} " + parsed.strftime("%B %Y at %H:%M")

    dst_forward_time = format_transition_text(dst_forward) if dst_forward and dst_forward != "unavailable" else ""
    dst_back_time = format_transition_text(dst_back) if dst_back and dst_back != "unavailable" else ""
    dst_forward_text = (
        f"{dst_forward_time}, from {format_offset_text(dst_forward_from_offset)} to {format_offset_text(dst_forward_to_offset)}"
        if dst_forward_time and dst_forward_from_offset and dst_forward_to_offset else dst_forward_time
    )
    dst_back_text = (
        f"{dst_back_time}, from {format_offset_text(dst_back_from_offset)} to {format_offset_text(dst_back_to_offset)}"
        if dst_back_time and dst_back_from_offset and dst_back_to_offset else dst_back_time
    )
    dst_summary = "No daylight saving changes this year" if dst_status == "none" else ""

    def calendar_sort_key(text: str) -> tuple[int, str]:
        stripped = str(text or "").strip()
        if re.fullmatch(r"\d{4}-\d{2}-\d{2}", stripped):
            return (0, stripped)
        if re.fullmatch(r"\d{4}-\d{2}-\d{2} \d{2}:\d{2}", stripped):
            return (0, stripped)
        return (1, stripped)

    def build_calendar_rows(current_value: str,
                            observances: list[tuple[str, str, str]]) -> list[dict[str, str]]:
        rows = [{"label": "Current date", "value": current_value}]
        grouped: dict[str, list[tuple[str, str]]] = {}
        for label, value, sort_value in observances:
            value_text = str(value or "").strip()
            if not value_text:
                continue
            grouped.setdefault(str(sort_value or "").strip(), []).append((label, value_text))
        for _, items in sorted(grouped.items(), key=lambda item: calendar_sort_key(item[0])):
            for label, value_text in items:
                rows.append({"label": label, "value": value_text})
        return rows

    overview_lines = [
        f"{weekday} {date}".strip(),
        f"Moon phase: {moon_phase}" if moon_phase else "",
        f"Sunrise: {sunrise}" if sunrise and sunrise != "unavailable" else f"Sunrise: {sunrise_status or 'unavailable'}",
        f"Actual sunrise: {actual_sunrise}" if actual_sunrise and actual_sunrise != "unavailable" else f"Actual sunrise: {actual_sunrise_status or 'unavailable'}",
        f"Sunset: {sunset}" if sunset and sunset != "unavailable" else f"Sunset: {sunset_status or 'unavailable'}",
        f"Actual sunset: {actual_sunset}" if actual_sunset and actual_sunset != "unavailable" else f"Actual sunset: {actual_sunset_status or 'unavailable'}",
        f"Temperature: {weather_summary}" if weather_summary else "",
        f"Humidity: {weather_humidity}" if weather_humidity else "",
        f"Wind: {weather_wind}" if weather_wind else "",
        f"Clocks forward: {dst_forward_text}" if dst_forward_text else "",
        f"Clocks back: {dst_back_text}" if dst_back_text else "",
        dst_summary,
        offset_text if gmt_offset else "",
    ]
    overview_sections = [
        {
            "title": "Date",
            "open": True,
            "rows": [
                {"label": "Date", "value": date},
                {"label": "Weekday", "value": weekday},
                {"label": "Moon phase", "value": moon_phase},
                {"label": "Time basis", "value": offset_text},
            ],
        },
        {
            "title": "Sunlight",
            "open": True,
            "rows": [
                {
                    "label": "Sunrise estimate",
                    "value": sunrise if sunrise and sunrise != "unavailable" else (sunrise_status or "unavailable"),
                },
                {
                    "label": "Sunrise actual (almanac)",
                    "value": actual_sunrise if actual_sunrise and actual_sunrise != "unavailable" else (actual_sunrise_status or "unavailable"),
                },
                {
                    "label": "Sunset estimate",
                    "value": sunset if sunset and sunset != "unavailable" else (sunset_status or "unavailable"),
                },
                {
                    "label": "Sunset actual (almanac)",
                    "value": actual_sunset if actual_sunset and actual_sunset != "unavailable" else (actual_sunset_status or "unavailable"),
                },
                *([
                    {"label": "Clocks forward", "value": dst_forward_text},
                ] if dst_forward_text else []),
                *([
                    {"label": "Clocks back", "value": dst_back_text},
                ] if dst_back_text else []),
                *([
                    {"label": "Daylight saving", "value": dst_summary},
                ] if dst_summary else []),
            ],
        },
        *([
            {
                "title": "Weather",
                "open": True,
                "rows": [
                    {"label": "Minimum", "value": weather_min_c},
                    {"label": "Maximum", "value": weather_max_c},
                    *([
                        {"label": "Humidity", "value": weather_humidity},
                    ] if weather_humidity else []),
                    *([
                        {"label": "Wind", "value": weather_wind},
                    ] if weather_wind else []),
                    *([
                        {"label": "Chance of rain", "value": weather_rain_chance},
                    ] if weather_rain_chance else []),
                    *([
                        {"label": "Source", "value": weather_source},
                    ] if weather_source else []),
                ],
            },
        ] if weather_min_c and weather_max_c else []),
    ]
    range_lines = [
        f"Start date: {str(fields.get('start') or '').strip()}",
        f"End date: {str(fields.get('end') or '').strip()}",
        f"Days between: {str(fields.get('days_between') or '').strip()}",
        f"Absolute days: {str(fields.get('days_between_abs') or '').strip()}",
        "Calendar span: "
        f"{str(fields.get('duration_years') or '0').strip()} years, "
        f"{str(fields.get('duration_months') or '0').strip()} months, "
        f"{str(fields.get('duration_days') or '0').strip()} days",
    ]
    range_sections = [
        {
            "title": "Range",
            "open": True,
            "rows": [
                {"label": "Start date", "value": str(fields.get("start") or "").strip()},
                {"label": "End date", "value": str(fields.get("end") or "").strip()},
                {"label": "Days between", "value": str(fields.get("days_between") or "").strip()},
                {"label": "Absolute days", "value": str(fields.get("days_between_abs") or "").strip()},
                {
                    "label": "Calendar span",
                    "value": (
                        f"{str(fields.get('duration_years') or '0').strip()} years, "
                        f"{str(fields.get('duration_months') or '0').strip()} months, "
                        f"{str(fields.get('duration_days') or '0').strip()} days"
                    ),
                },
            ],
        },
    ]
    calendar_lines = [
        f"Christian calendar date: {str(fields.get('christian_calendar_date') or '').strip()}",
        f"Easter Sunday: {str(fields.get('easter') or '').strip()}",
        f"Orthodox Easter Sunday: {str(fields.get('orthodox_easter') or '').strip()}",
        f"Christmas Day: {str(fields.get('christmas') or '').strip()}",
        f"Orthodox Christmas Day: {str(fields.get('orthodox_christmas') or '').strip()}",
        f"Chinese calendar date: {str(fields.get('chinese_calendar_date') or '').strip()}",
        f"Chinese New Year: {str(fields.get('chinese_new_year') or '').strip()}",
        f"Hindu calendar date: {str(fields.get('hindu_calendar_date') or '').strip()}",
        f"Diwali (estimated): {str(fields.get('diwali') or '').strip()}",
        f"Holi (estimated): {str(fields.get('holi') or '').strip()}",
        f"Hindu New Year (estimated): {str(fields.get('hindu_new_year') or '').strip()}",
        f"Buddhist calendar date: {str(fields.get('buddhist_calendar_date') or '').strip()}",
        f"Buddhist New Year (estimated): {str(fields.get('buddhist_new_year') or '').strip()}",
        f"Vesak / Buddha Day (estimated): {str(fields.get('vesak') or '').strip()}",
        f"Asalha Puja / Dharma Day (estimated): {str(fields.get('asalha_puja') or '').strip()}",
        f"Muslim calendar date: {str(fields.get('muslim_calendar_date') or '').strip()}",
        f"Ramadan begins (civil Islamic): {str(fields.get('ramadan') or '').strip()}",
        f"Ramadan begins at sunset (local time): {str(fields.get('ramadan_starts_local') or '').strip()}",
        f"Eid al-Fitr (civil Islamic): {str(fields.get('eid_al_fitr') or '').strip()}",
        f"Eid al-Fitr begins at sunset (local time): {str(fields.get('eid_al_fitr_starts_local') or '').strip()}",
        f"Muslim New Year (civil Islamic): {str(fields.get('muslim_new_year') or '').strip()}",
        f"Muslim New Year begins at sunset (local time): {str(fields.get('muslim_new_year_starts_local') or '').strip()}",
        f"Jewish calendar date: {str(fields.get('jewish_calendar_date') or '').strip()}",
        f"Passover: {str(fields.get('passover') or '').strip()}",
        f"Passover begins at sunset (local time): {str(fields.get('passover_starts_local') or '').strip()}",
        f"Jewish New Year: {str(fields.get('jewish_new_year') or '').strip()}",
        f"Jewish New Year begins at sunset (local time): {str(fields.get('jewish_new_year_starts_local') or '').strip()}",
        f"Cherokee calendar date: {str(fields.get('cherokee_calendar_date') or '').strip()}",
        f"Cherokee New Moon Festival (estimated): {str(fields.get('cherokee_new_moon_festival') or '').strip()}",
        f"Cherokee Green Corn Ceremony (estimated): {str(fields.get('cherokee_green_corn_ceremony') or '').strip()}",
        f"Cherokee Ripe Corn Ceremony (estimated): {str(fields.get('cherokee_ripe_corn_ceremony') or '').strip()}",
        f"Cherokee Great New Moon Festival (estimated): {str(fields.get('cherokee_great_new_moon_festival') or '').strip()}",
        f"Mayan calendar date: {str(fields.get('mayan_calendar_date') or '').strip()}",
        f"Mayan Haab New Year: {str(fields.get('mayan_haab_new_year') or '').strip()}",
        f"Wayeb begins: {str(fields.get('mayan_wayeb_start') or '').strip()}",
        f"Aztec calendar date: {str(fields.get('aztec_calendar_date') or '').strip()}",
        f"Aztec Xiuhpohualli New Year: {str(fields.get('aztec_xiuhpohualli_new_year') or '').strip()}",
        f"Nemontemi begins: {str(fields.get('aztec_nemontemi_start') or '').strip()}",
        f"Ethiopian calendar date: {str(fields.get('ethiopian_calendar_date') or '').strip()}",
        f"Enkutatash: {str(fields.get('ethiopian_new_year') or '').strip()}",
        f"Genna: {str(fields.get('genna') or '').strip()}",
        f"Timkat: {str(fields.get('timkat') or '').strip()}",
        f"Meskel: {str(fields.get('meskel') or '').strip()}",
        f"Fasika: {str(fields.get('fasika') or '').strip()}",
    ]
    calendar_sections = [
        {
            "title": "Christian",
            "open": False,
            "rows": build_calendar_rows(
                str(fields.get("christian_calendar_date") or "").strip(),
                [
                    ("Easter Sunday", str(fields.get("easter") or "").strip(), str(fields.get("easter") or "").strip()),
                    ("Orthodox Easter Sunday", str(fields.get("orthodox_easter") or "").strip(), str(fields.get("orthodox_easter") or "").strip()),
                    ("Christmas Day", str(fields.get("christmas") or "").strip(), str(fields.get("christmas") or "").strip()),
                    ("Orthodox Christmas Day", str(fields.get("orthodox_christmas") or "").strip(), str(fields.get("orthodox_christmas") or "").strip()),
                ],
            ),
        },
        {
            "title": "Chinese",
            "open": False,
            "rows": build_calendar_rows(
                str(fields.get("chinese_calendar_date") or "").strip(),
                [
                    ("New Year", str(fields.get("chinese_new_year") or "").strip(), str(fields.get("chinese_new_year") or "").strip()),
                ],
            ),
        },
        {
            "title": "Hindu",
            "open": False,
            "rows": build_calendar_rows(
                str(fields.get("hindu_calendar_date") or "").strip(),
                [
                    ("Diwali (estimated)", str(fields.get("diwali") or "").strip(), str(fields.get("diwali") or "").strip()),
                    ("Holi (estimated)", str(fields.get("holi") or "").strip(), str(fields.get("holi") or "").strip()),
                    ("Hindu New Year (estimated)", str(fields.get("hindu_new_year") or "").strip(), str(fields.get("hindu_new_year") or "").strip()),
                ],
            ),
        },
        {
            "title": "Buddhist",
            "open": False,
            "rows": build_calendar_rows(
                str(fields.get("buddhist_calendar_date") or "").strip(),
                [
                    ("Buddhist New Year (estimated)", str(fields.get("buddhist_new_year") or "").strip(), str(fields.get("buddhist_new_year") or "").strip()),
                    ("Vesak / Buddha Day (estimated)", str(fields.get("vesak") or "").strip(), str(fields.get("vesak") or "").strip()),
                    ("Asalha Puja / Dharma Day (estimated)", str(fields.get("asalha_puja") or "").strip(), str(fields.get("asalha_puja") or "").strip()),
                ],
            ),
        },
        {
            "title": "Muslim",
            "open": False,
            "rows": build_calendar_rows(
                str(fields.get("muslim_calendar_date") or "").strip(),
                [
                    ("Ramadan begins (civil Islamic)", str(fields.get("ramadan") or "").strip(), str(fields.get("ramadan") or "").strip()),
                    ("Ramadan begins at sunset (local time)", str(fields.get("ramadan_starts_local") or "").strip(), str(fields.get("ramadan") or "").strip()),
                    ("Eid al-Fitr (civil Islamic)", str(fields.get("eid_al_fitr") or "").strip(), str(fields.get("eid_al_fitr") or "").strip()),
                    ("Eid al-Fitr begins at sunset (local time)", str(fields.get("eid_al_fitr_starts_local") or "").strip(), str(fields.get("eid_al_fitr") or "").strip()),
                    ("Muslim New Year (civil Islamic)", str(fields.get("muslim_new_year") or "").strip(), str(fields.get("muslim_new_year") or "").strip()),
                    ("Muslim New Year begins at sunset (local time)", str(fields.get("muslim_new_year_starts_local") or "").strip(), str(fields.get("muslim_new_year") or "").strip()),
                ],
            ),
        },
        {
            "title": "Jewish",
            "open": False,
            "rows": build_calendar_rows(
                str(fields.get("jewish_calendar_date") or "").strip(),
                [
                    ("Passover", str(fields.get("passover") or "").strip(), str(fields.get("passover") or "").strip()),
                    ("Passover begins at sunset (local time)", str(fields.get("passover_starts_local") or "").strip(), str(fields.get("passover") or "").strip()),
                    ("Jewish New Year", str(fields.get("jewish_new_year") or "").strip(), str(fields.get("jewish_new_year") or "").strip()),
                    ("Jewish New Year begins at sunset (local time)", str(fields.get("jewish_new_year_starts_local") or "").strip(), str(fields.get("jewish_new_year") or "").strip()),
                ],
            ),
        },
        {
            "title": "Cherokee",
            "open": False,
            "rows": build_calendar_rows(
                str(fields.get("cherokee_calendar_date") or "").strip(),
                [
                    ("New Moon Festival (estimated)", str(fields.get("cherokee_new_moon_festival") or "").strip(), str(fields.get("cherokee_new_moon_festival") or "").strip()),
                    ("Green Corn Ceremony (estimated)", str(fields.get("cherokee_green_corn_ceremony") or "").strip(), str(fields.get("cherokee_green_corn_ceremony") or "").strip()),
                    ("Ripe Corn Ceremony (estimated)", str(fields.get("cherokee_ripe_corn_ceremony") or "").strip(), str(fields.get("cherokee_ripe_corn_ceremony") or "").strip()),
                    ("Great New Moon Festival (estimated)", str(fields.get("cherokee_great_new_moon_festival") or "").strip(), str(fields.get("cherokee_great_new_moon_festival") or "").strip()),
                ],
            ),
        },
        {
            "title": "Mayan",
            "open": False,
            "rows": build_calendar_rows(
                str(fields.get("mayan_calendar_date") or "").strip(),
                [
                    ("Haab New Year", str(fields.get("mayan_haab_new_year") or "").strip(), str(fields.get("mayan_haab_new_year") or "").strip()),
                    ("Wayeb begins", str(fields.get("mayan_wayeb_start") or "").strip(), str(fields.get("mayan_wayeb_start") or "").strip()),
                ],
            ),
        },
        {
            "title": "Aztec",
            "open": False,
            "rows": build_calendar_rows(
                str(fields.get("aztec_calendar_date") or "").strip(),
                [
                    ("Xiuhpohualli New Year", str(fields.get("aztec_xiuhpohualli_new_year") or "").strip(), str(fields.get("aztec_xiuhpohualli_new_year") or "").strip()),
                    ("Nemontemi begins", str(fields.get("aztec_nemontemi_start") or "").strip(), str(fields.get("aztec_nemontemi_start") or "").strip()),
                ],
            ),
        },
        {
            "title": "Ethiopian",
            "open": False,
            "rows": build_calendar_rows(
                str(fields.get("ethiopian_calendar_date") or "").strip(),
                [
                    ("Enkutatash", str(fields.get("ethiopian_new_year") or "").strip(), str(fields.get("ethiopian_new_year") or "").strip()),
                    ("Genna", str(fields.get("genna") or "").strip(), str(fields.get("genna") or "").strip()),
                    ("Timkat", str(fields.get("timkat") or "").strip(), str(fields.get("timkat") or "").strip()),
                    ("Meskel", str(fields.get("meskel") or "").strip(), str(fields.get("meskel") or "").strip()),
                    ("Fasika", str(fields.get("fasika") or "").strip(), str(fields.get("fasika") or "").strip()),
                ],
            ),
        },
    ]
    bank_holiday_text = str(fields.get("bank_holiday") or "").strip()
    holiday_notice_text = str(fields.get("holiday_notice") or "").strip()
    local_lines = []
    local_rows = []
    if bank_holiday_text:
        local_lines.append(
            "Local holidays between "
            f"{str(fields.get('start') or '').strip()} and {str(fields.get('end') or '').strip()}:"
        )
        for line in bank_holiday_text.splitlines():
            line = line.strip()
            if not line:
                continue
            local_lines.append(line)
            match = re.match(r"^(.*?):\s*(.+)$", line)
            if match:
                local_rows.append({
                    "label": match.group(1).strip(),
                    "value": match.group(2).strip(),
                })
            else:
                local_rows.append({"label": line, "value": ""})
    elif holiday_notice_text:
        local_lines.append(holiday_notice_text)
        local_rows.append({"label": "Holiday database", "value": holiday_notice_text})
    local_sections = []
    if local_rows:
        local_sections.append({
            "title": (
                f"Local holidays ({str(fields.get('start') or '').strip()} to {str(fields.get('end') or '').strip()})"
                if bank_holiday_text
                else "Local holidays"
            ),
            "open": True,
            "rows": local_rows,
        })
    solar_lines = [
        f"Latitude: {str(fields.get('latitude') or '').strip()}",
        f"Longitude: {str(fields.get('longitude') or '').strip()}",
        f"Elevation: {str(fields.get('elevation_metres') or '').strip()} m",
        f"Solar declination: {str(fields.get('solar_declination') or '').strip()}°",
        f"Sun's noon inclination: {str(fields.get('solar_inclination') or '').strip()}°",
        f"Sun's maximum altitude: {str(fields.get('solar_max_altitude') or '').strip()}°",
    ]
    solar_sections = [
        {
            "title": "Location",
            "open": True,
            "rows": [
                {"label": "Latitude", "value": str(fields.get("latitude") or "").strip()},
                {"label": "Longitude", "value": str(fields.get("longitude") or "").strip()},
                {"label": "Elevation", "value": f"{str(fields.get('elevation_metres') or '').strip()} m"},
            ],
        },
        {
            "title": "Solar",
            "open": True,
            "rows": [
                {"label": "Solar declination", "value": f"{str(fields.get('solar_declination') or '').strip()}°"},
                {"label": "Sun's noon inclination", "value": f"{str(fields.get('solar_inclination') or '').strip()}°"},
                {"label": "Sun's maximum altitude", "value": f"{str(fields.get('solar_max_altitude') or '').strip()}°"},
            ],
        },
    ]

    return {
        "ok": True,
        "mode": "datetime",
        "overview": "\n".join(line for line in overview_lines if line),
        "overview_sections": overview_sections,
        "range": "\n".join(line for line in range_lines if line.strip(": ")),
        "range_sections": range_sections,
        "calendar": "\n".join(line for line in calendar_lines if line.strip(": ")),
        "calendar_sections": calendar_sections,
        "local": "\n".join(line for line in local_lines if line.strip(": ")),
        "local_sections": local_sections,
        "solar": "\n".join(line for line in solar_lines if line.strip(": ")),
        "solar_sections": solar_sections,
        "fields": fields,
    }


def parse_almanac_snapshot_rows(text: str) -> list[dict[str, str]]:
    rows: list[dict[str, str]] = []
    for line in str(text or "").splitlines():
      parts = line.split("|")
      if len(parts) != 11:
          continue
      rows.append({
          "code": parts[0].strip(),
          "name": parts[1].strip(),
          "kind": parts[2].strip(),
          "declination": parts[3].strip(),
          "right_ascension": parts[4].strip(),
          "gha": parts[5].strip(),
          "sha": parts[6].strip(),
          "lha": parts[7].strip(),
          "distance_au": parts[8].strip(),
          "phase": parts[9].strip(),
          "magnitude": parts[10].strip(),
      })
    return rows


def prepare_almanac_fields(fields: dict[str, str]) -> dict[str, object]:
    rows = parse_almanac_snapshot_rows(fields.get("snapshot", ""))
    selected_code = str(fields.get("body") or "").strip().upper()
    selected_name = str(fields.get("selected_name") or selected_code).strip()
    date_text = str(fields.get("date") or "").strip()
    time_text = str(fields.get("time") or "").strip()
    zone_text = str(fields.get("zone") or "").strip()
    latitude_text = str(fields.get("latitude") or "").strip()
    longitude_text = str(fields.get("longitude") or "").strip()
    gha_aries = str(fields.get("gha_aries") or "").strip()

    worksheet_lines = [
        "AstroNav 2000-2040 Navigation Almanac",
        f"Date (GMT): {date_text}",
        f"Time (GMT): {time_text}",
        f"Zone: {zone_text}",
        f"Latitude: {latitude_text}",
        f"Longitude: {longitude_text}",
        f"Selected body: {selected_name} ({selected_code})",
        "",
        "Body | Kind | Declination | GHA | SHA | LHA | RA",
    ]
    for row in rows:
        worksheet_lines.append(
            " | ".join(
                [
                    str(row.get("name") or row.get("code") or "").strip(),
                    str(row.get("kind") or "").strip(),
                    str(row.get("declination") or "").strip(),
                    str(row.get("gha") or "").strip(),
                    str(row.get("sha") or "").strip(),
                    str(row.get("lha") or "").strip(),
                    str(row.get("right_ascension") or "").strip(),
                ]
            )
        )

    selected_summary = "\n".join(
        [
            f"Selected body: {selected_name} ({selected_code})",
            f"Kind: {str(fields.get('selected_kind') or '').strip()}",
            f"Declination: {str(fields.get('selected_declination') or '').strip()} deg",
            f"GHA: {str(fields.get('selected_gha') or '').strip()} deg",
            f"SHA: {str(fields.get('selected_sha') or '').strip()} deg",
            f"LHA: {str(fields.get('selected_lha') or '').strip()} deg",
            f"Right ascension: {str(fields.get('selected_right_ascension') or '').strip()} h",
            f"Geocentric distance: {str(fields.get('selected_geo_distance') or '').strip()} AU",
            f"Heliocentric distance: {str(fields.get('selected_helio_distance') or '').strip()} AU",
            f"Phase angle: {str(fields.get('selected_phase') or '').strip()} deg",
            f"Visual magnitude: {str(fields.get('selected_visual_magnitude') or '').strip()}",
        ]
    )
    body_listing = "\n".join(
        [
            f"{str(row.get('name') or row.get('code') or '').strip()}: "
            f"SHA {str(row.get('sha') or '').strip()} deg, "
            f"Declination {str(row.get('declination') or '').strip()} deg"
            for row in rows
        ]
    )
    notes = "\n".join(
        [
            f"GHA Aries: {gha_aries} deg",
            "Enter date and time in GMT, then zone, latitude, and longitude.",
            "For 2000-2040 GMT, the workbook intention is navigation-grade output within 6 arc-seconds after rounding to the nearest arc-second for the listed navigation bodies.",
        ]
    )
    return {
        "ok": True,
        "mode": "almanac",
        "worksheet_title": "AstroNav 2000-2040 Navigation Almanac",
        "moment_text": f"GMT moment: {date_text} {time_text}".strip(),
        "observer_text": f"Observer: zone {zone_text}, latitude {latitude_text}, longitude {longitude_text}",
        "body_text": f"Location of Navigational Bodies for {selected_name} ({selected_code})",
        "range_text": "Beyond 2040, use at your own risk.",
        "selected_code": selected_code,
        "rows": rows,
        "worksheet": "\n".join(worksheet_lines),
        "selected_summary": selected_summary,
        "body_listing": body_listing,
        "notes": notes,
        "fields": fields,
    }


def integrator_tex_for_display(tex: str) -> str:
    tex = str(tex or "").strip()
    if not tex:
        return ""

    binding_wrapper = re.compile(
        r"\\left\\\{\s*(.*?)\s*\\;\\middle\|\\;.*?\\right\\\}"
    )

    def replace_wrapper(match: re.Match[str]) -> str:
        body = match.group(1).strip()
        return body or match.group(0)

    previous = None
    while previous != tex:
        previous = tex
        tex = binding_wrapper.sub(replace_wrapper, tex)
    return tex


class MarsLabHandler(http.server.BaseHTTPRequestHandler):
    binary: Path = DEFAULT_BIN
    equation_binary: Path = DEFAULT_EQUATION_BIN
    matrix_binary: Path = DEFAULT_MATRIX_BIN
    integrator_binary: Path = DEFAULT_INTEGRATOR_BIN
    datetime_binary: Path = DEFAULT_DATETIME_BIN
    almanac_binary: Path = DEFAULT_ALMANAC_BIN
    holiday_binary: Path = DEFAULT_HOLIDAY_BIN
    server_host: str = "127.0.0.1"
    server_port: int = 0
    mobile_url: str = ""

    def log_message(self, fmt: str, *args: object) -> None:
        try:
            print(f"mars_lab: {fmt % args}", file=sys.stderr)
        except OSError:
            pass

    def send_json(self, status: int, payload: dict[str, object]) -> None:
        data = json.dumps(payload).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Cache-Control", "no-store")
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def send_file(self, path: Path, content_type: str) -> None:
        try:
            data = path.read_bytes()
        except OSError:
            self.send_error(404)
            return
        self.send_response(200)
        self.send_header("Content-Type", content_type)
        self.send_header("Cache-Control", "no-store")
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def request_allowed(self) -> bool:
        if request_uses_public_funnel_host(self.headers.get("Host", "")):
            self.send_error(403, f"{LAB_APP_NAME} is private. Use WiFi or Tailscale.")
            return False
        if request_allows_lab_access(str(self.client_address[0])):
            return True
        self.send_error(403, f"{LAB_APP_NAME} is only available on this machine, local WiFi, or Tailscale.")
        return False

    def do_GET(self) -> None:
        if not self.request_allowed():
            return
        parsed_path = urllib.parse.urlparse(self.path)
        path = parsed_path.path
        if path == "/state":
            self.send_json(200, load_state_data())
            return

        if path == "/mobile-access":
            control_allowed = request_allows_funnel_control(
                self.headers,
                str(self.client_address[0]),
                _control_token_from_query(self.path),
            )
            details = mobile_access_details(
                self.server_host,
                self.server_port,
                self.headers.get("Host", ""),
                control_allowed,
            )
            details["qr"] = mobile_qr_svg(
                str(details["url"]),
                bool(details.get("control")),
            ) if details.get("url") else ""
            self.send_json(200, details)
            return

        if path == "/favicon.svg":
            self.send_file(LAB_FAVICON_FILE, "image/svg+xml")
            return

        if path == "/apple-touch-icon.png":
            self.send_file(LAB_TOUCH_ICON_FILE, "image/png")
            return

        if path == "/icon-192.png":
            self.send_file(LAB_ICON_192_FILE, "image/png")
            return

        if path == "/icon-512.png":
            self.send_file(LAB_ICON_512_FILE, "image/png")
            return

        if path == "/manifest.webmanifest":
            self.send_json(200, WEB_MANIFEST)
            return

        if path not in ("/", "/index.html"):
            self.send_error(404)
            return

        control_allowed = request_allows_funnel_control(
            self.headers,
            str(self.client_address[0]),
            _control_token_from_query(self.path),
        )
        mobile_details = mobile_access_details(
            self.server_host,
            self.server_port,
            self.headers.get("Host", ""),
            control_allowed,
        )
        mobile_url = str(mobile_details["url"])
        mobile_qr = mobile_qr_svg(
            mobile_url,
            bool(mobile_details.get("control")),
        ) if mobile_url else ""
        page = (
            INDEX_HTML.replace(
                "__INITIAL_EXPRESSION__",
                html.escape(load_state_expression(), quote=False),
            )
            .replace("__MOBILE_TITLE__", html.escape(mobile_details["title"], quote=False))
            .replace("__MOBILE_HINT__", html.escape(mobile_details["hint"], quote=False))
            .replace("__MOBILE_URL__", html.escape(mobile_url, quote=False))
            .replace("__MOBILE_QR_SVG__", mobile_qr)
            .replace("__MOBILE_CARD_CLASS__", "")
            .replace("__MOBILE_TAILSCALE_CLASS__", "" if mobile_details.get("tailscale") else "hidden")
            .replace("__DEFAULT_EXPRESSION__", json.dumps(DEFAULT_EXPRESSION))
            .replace("__DEFAULT_EQUATION__", json.dumps(DEFAULT_EQUATION))
            .replace("__DEFAULT_EQUATION_VARIABLE__", json.dumps(DEFAULT_EQUATION_VARIABLE))
            .replace("__DEFAULT_MATRIX__", json.dumps(DEFAULT_MATRIX))
            .replace("__DEFAULT_INTEGRATOR__", json.dumps(DEFAULT_INTEGRATOR_EXPRESSION))
            .replace("__DEFAULT_INTEGRATOR_BOUNDS__", json.dumps(DEFAULT_INTEGRATOR_BOUNDS))
            .replace("__DEFAULT_INTEGRATOR_INTERVAL_CAP__", json.dumps(DEFAULT_INTEGRATOR_INTERVAL_CAP))
            .replace("__DEFAULT_DATETIME_TEXT__", json.dumps(DEFAULT_DATETIME_TEXT))
            .replace("__DEFAULT_DATETIME_DATE__", json.dumps(DEFAULT_DATETIME_DATE))
            .replace("__DEFAULT_DATETIME_JURISDICTION__", json.dumps(DEFAULT_HOLIDAY_JURISDICTION))
            .replace("__DEFAULT_DATETIME_LATITUDE__", json.dumps(DEFAULT_DATETIME_LATITUDE))
            .replace("__DEFAULT_DATETIME_LONGITUDE__", json.dumps(DEFAULT_DATETIME_LONGITUDE))
            .replace("__DEFAULT_DATETIME_ELEVATION__", json.dumps(DEFAULT_DATETIME_ELEVATION))
            .replace("__DEFAULT_DATETIME_GMT_OFFSET__", json.dumps(DEFAULT_DATETIME_GMT_OFFSET))
            .replace("__DEFAULT_ALMANAC_TEXT__", json.dumps(DEFAULT_ALMANAC_TEXT))
            .replace("__DEFAULT_ALMANAC_DATE__", json.dumps(DEFAULT_ALMANAC_DATE))
            .replace("__DEFAULT_ALMANAC_TIME__", json.dumps(DEFAULT_ALMANAC_TIME))
            .replace("__DEFAULT_ALMANAC_ZONE__", json.dumps(DEFAULT_ALMANAC_ZONE))
            .replace("__DEFAULT_ALMANAC_LATITUDE__", json.dumps(DEFAULT_ALMANAC_LATITUDE))
            .replace("__DEFAULT_ALMANAC_LONGITUDE__", json.dumps(DEFAULT_ALMANAC_LONGITUDE))
            .replace("__DEFAULT_ALMANAC_BODY__", json.dumps(DEFAULT_ALMANAC_BODY))
            .replace("__ALMANAC_BODY_OPTIONS__", ALMANAC_BODY_OPTIONS_HTML)
            .replace("__HOLIDAY_JURISDICTION_CODES__", json.dumps(sorted(VALID_HOLIDAY_JURISDICTIONS)))
            .replace("__HOLIDAY_JURISDICTION_OPTIONS__", HOLIDAY_JURISDICTION_OPTIONS_HTML)
            .replace("__CONTROL_TOKEN__", json.dumps(CONTROL_TOKEN if control_allowed else ""))
        )
        data = page.encode("utf-8")
        self.send_response(200)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Cache-Control", "no-store")
        if _control_token_from_query(self.path) == CONTROL_TOKEN:
            self.send_header(
                "Set-Cookie",
                f"{CONTROL_COOKIE}={urllib.parse.quote(CONTROL_TOKEN)}; Path=/; SameSite=Lax",
            )
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def do_POST(self) -> None:
        if not self.request_allowed():
            return
        path = urllib.parse.urlparse(self.path).path
        if path == "/funnel-toggle":
            self.send_json(410, {"ok": False, "error": "Public access switching is disabled in MARS Lab."})
            return

        if path == "/state":
            try:
                length = int(self.headers.get("Content-Length", "0"))
                body = self.rfile.read(length)
                payload = json.loads(body.decode("utf-8"))
                updates: dict[str, object] = {}

                expression = str(payload.get("expression", "")).strip()
                if expression and "..." not in expression:
                    updates["expression"] = expression

                matrix = str(payload.get("matrix", "")).strip()
                if matrix and "..." not in matrix:
                    updates["matrix"] = matrix

                lab_mode = str(payload.get("lab_mode", "")).strip()
                if lab_mode in {"expression", "equation", "matrix", "integrator", "datetime", "almanac"}:
                    updates["lab_mode"] = lab_mode

                equation = str(payload.get("equation", "")).strip()
                if equation and "..." not in equation:
                    updates["equation"] = equation

                equation_variable = str(payload.get("equation_variable", "")).strip()
                if equation_variable:
                    updates["equation_variable"] = equation_variable

                matrix_operation = str(payload.get("matrix_operation", "")).strip()
                if matrix_operation in {
                    "eval",
                    "inverse",
                    "eigenvalues",
                    "eigendecompose",
                    "charpoly",
                    "det",
                    "trace",
                    "rank",
                    "simplify",
                    "solve",
                }:
                    updates["matrix_operation"] = matrix_operation

                if "matrix_operand" in payload:
                    updates["matrix_operand"] = str(payload.get("matrix_operand", "")).strip()

                integrator_expression = str(payload.get("integrator_expression", "")).strip()
                if integrator_expression and "..." not in integrator_expression:
                    updates["integrator_expression"] = integrator_expression

                integrator_bounds = str(payload.get("integrator_bounds", "")).strip()
                if integrator_bounds:
                    updates["integrator_bounds"] = integrator_bounds

                if "integrator_interval_cap" in payload:
                    cap = int(payload.get("integrator_interval_cap", DEFAULT_INTEGRATOR_INTERVAL_CAP))
                    cap = max(MIN_INTEGRATOR_INTERVAL_CAP, min(MAX_INTEGRATOR_INTERVAL_CAP, cap))
                    if cap in INTEGRATOR_INTERVAL_CAP_CHOICES:
                        updates["integrator_interval_cap"] = cap

                for key in (
                    "datetime_date",
                    "datetime_jdn",
                    "datetime_start",
                    "datetime_end",
                    "datetime_year",
                    "datetime_jurisdiction",
                    "datetime_latitude",
                    "datetime_longitude",
                    "datetime_elevation",
                    "datetime_gmt_offset",
                    "almanac_date",
                    "almanac_time",
                    "almanac_zone",
                    "almanac_latitude",
                    "almanac_longitude",
                    "almanac_body",
                ):
                    if key in payload:
                        updates[key] = str(payload.get(key, "")).strip()

                if isinstance(payload.get("precision_bits"), dict):
                    saved_precision = load_state_data().get("precision_bits", {})
                    precision_bits = dict(saved_precision) if isinstance(saved_precision, dict) else {}
                    for mode in ("expression", "equation", "matrix", "integrator", "datetime", "almanac"):
                        if mode in payload["precision_bits"]:
                            bits = int(payload["precision_bits"][mode])
                            precision_bits[mode] = max(17, min(MAX_VALUE_PRECISION_BITS, bits))
                    updates["precision_bits"] = precision_bits
                elif "precision_bits" in payload:
                    bits = int(payload["precision_bits"])
                    updates["precision_bits"] = {
                        "expression": max(17, min(MAX_VALUE_PRECISION_BITS, bits)),
                        "equation": 256,
                        "matrix": 256,
                        "integrator": 17,
                        "datetime": 17,
                        "almanac": 17,
                    }

                if updates:
                    save_state_data(updates)
                self.send_json(200, {"ok": True})
            except Exception as exc:
                self.send_json(400, {"ok": False, "error": f"Bad request: {exc}"})
            return

        if path == "/render_tex":
            try:
                length = int(self.headers.get("Content-Length", "0"))
                body = self.rfile.read(length)
                payload = json.loads(body.decode("utf-8"))
                tex = str(payload.get("tex", "")).strip()
            except Exception as exc:
                self.send_json(400, {"ok": False, "error": f"Bad request: {exc}"})
                return

            svg, render_error = render_tex_to_svg(tex)
            if svg:
                self.send_json(200, {"ok": True, "svg": svg})
            else:
                self.send_json(422, {
                    "ok": False,
                    "error": render_error or "Could not render TeX",
                })
            return

        if path == "/equation-eval":
            try:
                length = int(self.headers.get("Content-Length", "0"))
                body = self.rfile.read(length)
                payload = json.loads(body.decode("utf-8"))
                equation_text = str(payload.get("equation", "")).strip()
                precision = int(payload.get("precision", 96))
            except Exception as exc:
                self.send_json(400, {"ok": False, "error": f"Bad request: {exc}"})
                return

            if not equation_text:
                self.send_json(400, {"ok": False, "error": "Equation input is empty"})
                return

            try:
                precision = max(17, min(MAX_VALUE_PRECISION_DIGITS, precision))
                ensure_scratch_binary(self.equation_binary, "scratch/equation_lab")
                fields, raw, returncode = run_equation_lab_fields(
                    self.equation_binary,
                    equation_text,
                    precision,
                )
            except Exception as exc:
                self.send_json(422, {"ok": False, "error": str(exc)})
                return

            if returncode != 0:
                self.send_json(422, {"ok": False, "error": raw or "Equation solving failed"})
                return

            save_state_data({
                "equation": equation_text,
            })
            self.send_json(200, prepare_equation_fields(fields, precision))
            return

        if path == "/matrix-eval":
            try:
                length = int(self.headers.get("Content-Length", "0"))
                body = self.rfile.read(length)
                payload = json.loads(body.decode("utf-8"))
                matrix_text = str(payload.get("matrix", "")).strip()
                operation = str(payload.get("operation", "eval")).strip() or "eval"
                operand = str(payload.get("operand", "")).strip()
                precision = int(payload.get("precision", 96))
            except Exception as exc:
                self.send_json(400, {"ok": False, "error": f"Bad request: {exc}"})
                return

            if not matrix_text:
                self.send_json(400, {"ok": False, "error": "Matrix input is empty"})
                return

            try:
                precision = max(17, min(MAX_VALUE_PRECISION_DIGITS, precision))
                ensure_scratch_binary(self.matrix_binary, "scratch/matrix_lab")
                fields, raw, returncode = run_matrix_lab_fields(
                    self.matrix_binary,
                    matrix_text,
                    operation,
                    precision,
                    operand,
                )
            except Exception as exc:
                self.send_json(422, {"ok": False, "error": str(exc)})
                return

            if returncode != 0:
                hint = matrix_failure_hint(
                    self.matrix_binary,
                    matrix_text,
                    operation,
                    precision,
                    operand,
                )
                message = hint or raw or "Matrix evaluation failed"
                self.send_json(422, {"ok": False, "error": message})
                return

            save_state_data({
                "matrix": matrix_text,
                "matrix_operation": operation,
                "matrix_operand": operand,
            })
            self.send_json(200, prepare_matrix_fields(fields, precision))
            return

        if path == "/integrator-eval":
            try:
                length = int(self.headers.get("Content-Length", "0"))
                body = self.rfile.read(length)
                payload = json.loads(body.decode("utf-8"))
                expression = str(payload.get("expression", "")).strip()
                bounds = payload.get("bounds", [])
                precision = int(payload.get("precision", 96))
                max_intervals = int(payload.get("max_intervals", DEFAULT_INTEGRATOR_INTERVAL_CAP))
            except Exception as exc:
                self.send_json(400, {"ok": False, "error": f"Bad request: {exc}"})
                return

            if not expression:
                self.send_json(400, {"ok": False, "error": "Integrand expression is empty"})
                return
            if not isinstance(bounds, list):
                self.send_json(400, {"ok": False, "error": "Bounds must be a list"})
                return
            if not bounds:
                bounds = [{"name": "x", "lo": "", "hi": ""}]

            cleaned_bounds: list[dict[str, str]] = []
            for item in bounds:
                if not isinstance(item, dict):
                    self.send_json(400, {"ok": False, "error": "Bounds must be name/lo/hi objects"})
                    return
                name = str(item.get("name", "")).strip()
                lo_text = str(item.get("lo", "")).strip()
                hi_text = str(item.get("hi", "")).strip()
                if not name:
                    self.send_json(400, {"ok": False, "error": "Every bound needs a variable name"})
                    return
                if lo_text and not hi_text:
                    self.send_json(400, {"ok": False, "error": "A one-sided bound should be entered as an upper value"})
                    return
                cleaned_bounds.append({"name": name, "lo": lo_text, "hi": hi_text})

            expression = expression_without_bindings_for_names(
                expression,
                {item["name"] for item in cleaned_bounds},
            )

            try:
                precision = max(17, min(MAX_VALUE_PRECISION_DIGITS, precision))
                max_intervals = max(MIN_INTEGRATOR_INTERVAL_CAP, min(MAX_INTEGRATOR_INTERVAL_CAP, max_intervals))
                ensure_scratch_binary(self.integrator_binary, "scratch/integrator_lab")
                fields, raw, returncode = run_integrator_lab_fields(
                    self.integrator_binary,
                    expression,
                    cleaned_bounds,
                    precision,
                    max_intervals,
                )
            except Exception as exc:
                self.log_message(
                    'integrator exception expression=%r bounds=%r precision=%r max_intervals=%r error=%s',
                    expression,
                    cleaned_bounds,
                    precision,
                    max_intervals,
                    exc,
                )
                self.send_json(422, {"ok": False, "error": str(exc)})
                return

            bounds_text = "\n".join(
                f"{item['name']} = {item['lo']} .. {item['hi']}"
                if item["lo"] and item["hi"]
                else (f"{item['name']} = {item['hi']}" if item["hi"] else item["name"])
                for item in cleaned_bounds
            )
            if returncode != 0:
                response_payload = prepare_integrator_fields(fields, precision)
                error_lines = [line.strip() for line in str(raw or "").splitlines() if line.strip()]
                response_payload["ok"] = False
                response_payload["error"] = error_lines[-1] if error_lines else "Integration failed"
                response_payload["raw_error"] = str(raw or "").strip()
                response_payload["returncode"] = returncode
                self.log_message(
                    'integrator failure expression=%r bounds=%r precision=%r max_intervals=%r returncode=%r raw=%r',
                    expression,
                    cleaned_bounds,
                    precision,
                    max_intervals,
                    returncode,
                    str(raw or "").strip(),
                )
                normalized_expression = str(
                    response_payload.get("binding_expression") or expression
                ).strip()
                if normalized_expression:
                    save_state_data({
                        "integrator_expression": normalized_expression,
                        "integrator_bounds": bounds_text,
                        "integrator_interval_cap": max_intervals,
                    })
                self.send_json(422, response_payload)
                return

            response_payload = prepare_integrator_fields(fields, precision)
            normalized_expression = str(
                response_payload.get("binding_expression") or expression
            ).strip()
            save_state_data({
                "integrator_expression": normalized_expression,
                "integrator_bounds": bounds_text,
                "integrator_interval_cap": max_intervals,
            })
            self.send_json(200, response_payload)
            return

        if path == "/datetime-eval":
            try:
                length = int(self.headers.get("Content-Length", "0"))
                body = self.rfile.read(length)
                payload = json.loads(body.decode("utf-8"))
                date_text = str(payload.get("date", DEFAULT_DATETIME_DATE)).strip()
                jdn_text = str(payload.get("jdn", "")).strip()
                start_text = str(payload.get("start", date_text)).strip()
                end_text = str(payload.get("end", date_text)).strip()
                year_text = str(payload.get("year", date_text[:4] or DEFAULT_DATETIME_DATE[:4])).strip()
                jurisdiction_text = str(payload.get("jurisdiction", DEFAULT_HOLIDAY_JURISDICTION)).strip()
                latitude_text = str(payload.get("latitude", DEFAULT_DATETIME_LATITUDE)).strip()
                longitude_text = str(payload.get("longitude", DEFAULT_DATETIME_LONGITUDE)).strip()
                elevation_text = str(payload.get("elevation", DEFAULT_DATETIME_ELEVATION)).strip()
                gmt_offset_text = str(payload.get("gmt_offset", "")).strip()
                for date_value in (date_text, start_text, end_text):
                    py_datetime.date.fromisoformat(date_value)
                year = max(1, min(9999, int(year_text)))
                latitude = float(latitude_text)
                longitude = float(longitude_text)
                elevation = float(elevation_text)
                if latitude < -90.0 or latitude > 90.0:
                    raise ValueError("Latitude must be between -90 and 90")
                if longitude < -180.0 or longitude > 180.0:
                    raise ValueError("Longitude must be between -180 and 180")
                if not math.isfinite(elevation):
                    raise ValueError("Elevation must be finite")
                jurisdiction = normalize_holiday_jurisdiction(jurisdiction_text)
                if gmt_offset_text:
                    gmt_offset = float(gmt_offset_text)
                    if gmt_offset < -14.0 or gmt_offset > 14.0:
                        raise ValueError("GMT offset must be between -14 and 14")
                if jdn_text and not re.fullmatch(r"\d+", jdn_text):
                    raise ValueError("Julian Day Number must be a positive integer")
            except Exception as exc:
                self.send_json(400, {"ok": False, "error": f"Bad request: {exc}"})
                return

            try:
                ensure_scratch_binary(self.datetime_binary, "scratch/datetime_lab")
                datetime_options = {
                    "date": date_text,
                    "start": start_text,
                    "end": end_text,
                    "year": str(year),
                    "lat": str(latitude),
                    "lon": str(longitude),
                    "elevation": str(elevation),
                    "gmt_offset": gmt_offset_text,
                    "jurisdiction": jurisdiction,
                }
                if jdn_text:
                    datetime_options["jdn"] = jdn_text
                fields, raw, returncode = run_datetime_lab_fields(
                    self.datetime_binary,
                    datetime_options,
                )
            except Exception as exc:
                self.send_json(422, {"ok": False, "error": str(exc)})
                return

            try:
                if returncode != 0:
                    self.send_json(422, {"ok": False, "error": raw or "Datetime calculation failed"})
                    return

                effective_weather_latitude = latitude
                effective_weather_longitude = longitude
                try:
                    ensure_scratch_binary(self.holiday_binary, "scratch/holiday_lab")
                    holiday_fields, holiday_raw, holiday_returncode = run_holiday_lab_fields(
                        self.holiday_binary,
                        {
                            "start": start_text,
                            "end": end_text,
                            "jurisdiction": jurisdiction,
                        },
                    )
                    if holiday_returncode == 0 and str(holiday_fields.get("holiday_status") or "").strip() == "ok":
                        fields.update(holiday_fields)
                    else:
                        holiday_status = str(holiday_fields.get("holiday_status") or "").strip()
                        if holiday_returncode == 0 and holiday_status == "unavailable":
                            fields["holiday_notice"] = (
                                "No holiday rules are available yet for the selected jurisdiction and date range."
                            )
                        else:
                            fields["holiday_notice"] = holiday_install_hint()
                        if holiday_returncode == 0:
                            fields.update(holiday_fields)
                        self.log_message(
                            "holiday helper failure jurisdiction=%r returncode=%r raw=%r",
                            jurisdiction,
                            holiday_returncode,
                            str(holiday_raw or "").strip(),
                        )
                    try:
                        jurisdiction_latitude = float(str(holiday_fields.get("jurisdiction_latitude") or "").strip())
                        jurisdiction_longitude = float(str(holiday_fields.get("jurisdiction_longitude") or "").strip())
                        effective_weather_latitude = jurisdiction_latitude
                        effective_weather_longitude = jurisdiction_longitude
                    except (TypeError, ValueError):
                        pass
                except Exception as exc:
                    fields["holiday_notice"] = holiday_install_hint()
                    self.log_message("holiday helper unavailable jurisdiction=%r error=%r", jurisdiction, exc)

                selected_date_text = str(fields.get("date") or date_text).strip()
                selected_jdn_text = str(fields.get("julian_day_number") or jdn_text).strip()
                weather_fields = fetch_daily_weather_with_budget(
                    selected_date_text,
                    effective_weather_latitude,
                    effective_weather_longitude,
                )
                if weather_fields:
                    fields.update(weather_fields)
                save_state_data({
                    "datetime_date": selected_date_text,
                    "datetime_jdn": selected_jdn_text,
                    "datetime_start": start_text,
                    "datetime_end": end_text,
                    "datetime_year": str(year),
                    "datetime_jurisdiction": jurisdiction,
                    "datetime_latitude": str(latitude),
                    "datetime_longitude": str(longitude),
                    "datetime_elevation": str(elevation),
                    "datetime_gmt_offset": gmt_offset_text,
                })
                self.send_json(200, prepare_datetime_fields(fields))
                return
            except Exception as exc:
                self.log_message(
                    "datetime response assembly failure jurisdiction=%r date=%r start=%r end=%r error=%r",
                    jurisdiction,
                    date_text,
                    start_text,
                    end_text,
                    exc,
                )
                self.send_json(500, {"ok": False, "error": f"Datetime response failed: {exc}"})
                return

        if path == "/datetime-jurisdiction-location":
            try:
                length = int(self.headers.get("Content-Length", "0"))
                body = self.rfile.read(length)
                payload = json.loads(body.decode("utf-8"))
                jurisdiction = normalize_holiday_jurisdiction(
                    str(payload.get("jurisdiction", DEFAULT_HOLIDAY_JURISDICTION)).strip()
                )
                date_text = str(payload.get("date", DEFAULT_DATETIME_DATE)).strip()
                py_datetime.date.fromisoformat(date_text)
            except Exception as exc:
                self.send_json(400, {"ok": False, "error": f"Bad request: {exc}"})
                return

            try:
                ensure_scratch_binary(self.holiday_binary, "scratch/holiday_lab")
                holiday_fields, holiday_raw, holiday_returncode = run_holiday_lab_fields(
                    self.holiday_binary,
                    {
                        "date": date_text,
                        "jurisdiction": jurisdiction,
                    },
                )
            except Exception as exc:
                self.send_json(422, {"ok": False, "error": str(exc)})
                return

            if holiday_returncode != 0 or str(holiday_fields.get("jurisdiction_status") or "").strip() != "ok":
                self.log_message(
                    "holiday location unavailable jurisdiction=%r returncode=%r raw=%r",
                    jurisdiction,
                    holiday_returncode,
                    str(holiday_raw or "").strip(),
                )
                self.send_json(200, {"ok": False, "error": "Jurisdiction location unavailable"})
                return

            self.send_json(200, {
                "ok": True,
                "jurisdiction": jurisdiction,
                "latitude": str(holiday_fields.get("jurisdiction_latitude") or "").strip(),
                "longitude": str(holiday_fields.get("jurisdiction_longitude") or "").strip(),
                "gmt_offset": str(holiday_fields.get("jurisdiction_gmt_offset") or "").strip(),
            })
            return

        if path == "/almanac-eval":
            try:
                length = int(self.headers.get("Content-Length", "0"))
                body = self.rfile.read(length)
                payload = json.loads(body.decode("utf-8"))
                date_text = str(payload.get("date", DEFAULT_ALMANAC_DATE)).strip()
                time_text = str(payload.get("time", DEFAULT_ALMANAC_TIME)).strip()
                zone_text = str(payload.get("zone", DEFAULT_ALMANAC_ZONE)).strip()
                latitude_text = str(payload.get("latitude", DEFAULT_ALMANAC_LATITUDE)).strip()
                longitude_text = str(payload.get("longitude", DEFAULT_ALMANAC_LONGITUDE)).strip()
                body_text = str(payload.get("body", DEFAULT_ALMANAC_BODY)).strip().upper()
                py_datetime.date.fromisoformat(date_text)
                if not re.fullmatch(r"\d{2}:\d{2}(?::\d{2}(?:\.\d+)?)?", time_text):
                    raise ValueError("Time must look like HH:MM or HH:MM:SS")
                zone = float(zone_text)
                latitude = float(latitude_text)
                longitude = float(longitude_text)
                if zone < -14.0 or zone > 14.0:
                    raise ValueError("Zone must be between -14 and 14")
                if latitude < -90.0 or latitude > 90.0:
                    raise ValueError("Latitude must be between -90 and 90")
                if longitude < -180.0 or longitude > 180.0:
                    raise ValueError("Longitude must be between -180 and 180")
                if body_text not in {code for code, _ in ALMANAC_BODY_OPTIONS}:
                    raise ValueError("Unknown almanac body")
            except Exception as exc:
                self.send_json(400, {"ok": False, "error": f"Bad request: {exc}"})
                return

            try:
                ensure_scratch_binary(self.almanac_binary, "scratch/almanac_lab")
                fields, raw, returncode = run_almanac_lab_fields(
                    self.almanac_binary,
                    {
                        "date": date_text,
                        "time": time_text,
                        "zone": zone_text,
                        "lat": latitude_text,
                        "lon": longitude_text,
                        "body": body_text,
                    },
                )
            except Exception as exc:
                self.send_json(422, {"ok": False, "error": str(exc)})
                return

            if returncode != 0:
                self.send_json(422, {"ok": False, "error": raw or "Almanac calculation failed"})
                return

            save_state_data({
                "almanac_date": date_text,
                "almanac_time": time_text,
                "almanac_zone": zone_text,
                "almanac_latitude": latitude_text,
                "almanac_longitude": longitude_text,
                "almanac_body": body_text,
            })
            self.send_json(200, prepare_almanac_fields(fields))
            return

        if path == "/goal_seek":
            try:
                length = int(self.headers.get("Content-Length", "0"))
                body = self.rfile.read(length)
                payload = json.loads(body.decode("utf-8"))
                expression = str(payload.get("expression", "")).strip()
                target = str(payload.get("target", "0")).strip() or "0"
                start = payload.get("start", {})
                precision = int(payload.get("precision", 96))
            except Exception as exc:
                self.send_json(400, {"ok": False, "error": f"Bad request: {exc}"})
                return

            if not expression:
                self.send_json(400, {"ok": False, "error": "Expression is empty"})
                return

            try:
                precision = max(17, min(MAX_VALUE_PRECISION_DIGITS, precision))
                expression = restore_compact_binding_values(expression, load_state_expression())
                solved, fields = goal_seek_expression(self.binary, expression, target, precision, start)
            except Exception as exc:
                self.send_json(422, {"ok": False, "error": str(exc)})
                return

            fields["ok"] = True
            fields["expression"] = solved
            fields["precision"] = precision
            precision_limit_result_fields(fields, precision)
            fields["editor_expression"] = editor_expression_from_fields(fields)
            save_state_expression(fields["editor_expression"])
            if fields.get("value"):
                fields["value"] = format_number_text_for_precision(
                    fields["value"], precision, zero_subprecision=True)
            if fields.get("residual"):
                fields["residual"] = format_number_text_for_precision(
                    fields["residual"], precision, zero_subprecision=True)
            fields["full_display_expression"] = expression_for_display(fields.get("expression", ""))
            fields["full_display_tex"] = tex_for_display(fields.get("tex", ""))
            fields["full_display_function"] = function_for_display(fields.get("function", ""))
            fields["display_expression"] = compact_display_text(fields["full_display_expression"])
            fields["display_tex"] = compact_display_text(fields["full_display_tex"])
            fields["display_function"] = compact_function_text(fields["full_display_function"])
            fields["binding_values"] = expression_variable_binding_values(
                fields.get("expression", "") or solved,
                precision,
            )
            svg, render_error = render_tex_to_svg(fields.get("display_tex", ""))
            if svg:
                fields["svg"] = svg
            elif render_error:
                fields["render_error"] = render_error
            self.send_json(200, fields)
            return

        if path != "/eval":
            self.send_error(404)
            return

        try:
            length = int(self.headers.get("Content-Length", "0"))
            body = self.rfile.read(length)
            payload = json.loads(body.decode("utf-8"))
            expression = str(payload.get("expression", "")).strip()
            requested_wrt = str(payload.get("wrt", "")).strip()
            action = str(payload.get("action", "")).strip().lower()
            if action not in {"", "derivative", "integral"}:
                raise ValueError("Action must be derivative or integral")
            operation_request = bool(requested_wrt) or action in {"derivative", "integral"}
            derivative_request = bool(requested_wrt) and action != "integral"
            integral_request = action == "integral"
            wrt = requested_wrt or "x"
            precision = int(payload.get("precision", 96))
        except Exception as exc:
            self.send_json(400, {"ok": False, "error": f"Bad request: {exc}"})
            return

        if not expression:
            self.send_json(400, {"ok": False, "error": "Expression is empty"})
            return
        precision = max(17, min(MAX_VALUE_PRECISION_DIGITS, precision))
        expression = restore_compact_binding_values(expression, load_state_expression())

        try:
            command = [str(self.binary), expression]
            command.extend([wrt, str(precision)])
            if integral_request:
                command.append("integral")
            completed = subprocess.run(
                command,
                cwd=ROOT,
                text=True,
                capture_output=True,
                timeout=10,
            )
        except Exception as exc:
            self.send_json(500, {"ok": False, "error": str(exc)})
            return

        raw = completed.stdout
        if completed.stderr:
            raw = raw + ("\n" if raw else "") + completed.stderr

        fields = parse_mars_lab_output(raw)
        fields["ok"] = completed.returncode == 0
        if completed.returncode != 0:
            binding_error = binding_syntax_error_details(raw)
            if binding_error:
                binding_name, _ = binding_error
                fallback_expression = expression_with_binding_value(expression, binding_name, "NAN")
                if fallback_expression:
                    fallback_fields, fallback_raw, fallback_rc = run_mars_lab_fields(
                            self.binary,
                            fallback_expression,
                            precision,
                            wrt,
                            "integral" if integral_request else "",
                        )
                    if fallback_rc == 0:
                        fallback_fields["ok"] = True
                        fallback_fields["partial_error"] = True
                        fallback_fields["error"] = raw.strip()
                        fallback_fields["raw"] = raw
                        fallback_fields["recovery_expression"] = fallback_expression
                        prepare_evaluation_fields(
                            self.binary,
                            fallback_fields,
                            fallback_expression,
                            precision,
                            save_expression=False,
                            wrt=wrt,
                        )
                        fallback_fields["binding_values"] = expression_variable_binding_values(
                            expression,
                            precision,
                        )
                        self.send_json(200, fallback_fields)
                        return
                    fields["recovery_raw"] = fallback_raw
            fields["raw"] = raw
            fields["error"] = raw or f"mars_lab exited with {completed.returncode}"
            self.send_json(422, fields)
            return

        prepare_evaluation_fields(
            self.binary,
            fields,
            expression,
            precision,
            save_expression=not operation_request,
            wrt=wrt,
        )

        self.send_json(200, fields)


def open_lab_url(url: str, browser: str = "") -> None:
    if browser:
        try:
            process = subprocess.Popen(
                [browser, url],
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
                close_fds=True,
            )
            try:
                if process.wait(timeout=0.5) == 0:
                    return
            except subprocess.TimeoutExpired:
                return
        except OSError:
            pass

    # Prefer desktop URI openers so the user's configured default browser is
    # used.  Drop BROWSER for these commands because some environments set it
    # to a stale Firefox path, which makes xdg-open look non-default and fail.
    opener_env = os.environ.copy()
    opener_env.pop("BROWSER", None)
    for command in (
        ("gio", "open", url),
        ("kde-open6", url),
        ("kde-open5", url),
        ("xdg-open", url),
    ):
        if shutil.which(command[0]):
            subprocess.Popen(
                list(command),
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
                close_fds=True,
                env=opener_env,
            )
            return

    webbrowser.open(url)


def main() -> int:
    parser = argparse.ArgumentParser(description=f"Launch the local {LAB_APP_NAME}.")
    parser.add_argument("--host", default="::", help="host to bind")
    parser.add_argument("--port", type=int, default=0, help="port to bind, or 0 for auto")
    parser.add_argument("--no-browser", action="store_true", help="do not open the browser automatically")
    parser.add_argument("--browser", default="", help="browser executable to open the lab URL")
    parser.add_argument("--binary", type=Path, default=DEFAULT_BIN, help="path to the scratch lab binary")
    parser.add_argument("--equation-binary", type=Path, default=DEFAULT_EQUATION_BIN, help="path to the equation scratch binary")
    parser.add_argument("--datetime-binary", type=Path, default=DEFAULT_DATETIME_BIN, help="path to the datetime scratch binary")
    parser.add_argument("--almanac-binary", type=Path, default=DEFAULT_ALMANAC_BIN, help="path to the almanac scratch binary")
    parser.add_argument("--holiday-binary", type=Path, default=DEFAULT_HOLIDAY_BIN, help="path to the holiday scratch binary")
    args = parser.parse_args()

    binary = args.binary if args.binary.is_absolute() else ROOT / args.binary
    equation_binary = args.equation_binary if args.equation_binary.is_absolute() else ROOT / args.equation_binary
    datetime_binary = args.datetime_binary if args.datetime_binary.is_absolute() else ROOT / args.datetime_binary
    almanac_binary = args.almanac_binary if args.almanac_binary.is_absolute() else ROOT / args.almanac_binary
    holiday_binary = args.holiday_binary if args.holiday_binary.is_absolute() else ROOT / args.holiday_binary
    ensure_mars_lab(binary)

    MarsLabHandler.binary = binary
    MarsLabHandler.equation_binary = equation_binary
    MarsLabHandler.datetime_binary = datetime_binary
    MarsLabHandler.almanac_binary = almanac_binary
    MarsLabHandler.holiday_binary = holiday_binary

    port = args.port or find_free_port(args.host)
    MarsLabHandler.server_host = args.host
    MarsLabHandler.server_port = port
    try:
        server = create_threading_http_server(args.host, port, MarsLabHandler)
    except OSError as exc:
        if exc.errno == errno.EADDRINUSE:
            ensure_tailscale_serve(args.host, port)
            MarsLabHandler.mobile_url = mobile_access_url(args.host, port)
            url = browser_access_url(args.host, port)
            print(f"{LAB_APP_NAME} already running at {url}")
            if not args.no_browser:
                open_lab_url(_control_url(url), args.browser)
            return 0
        raise

    ensure_tailscale_serve(args.host, port)
    MarsLabHandler.mobile_url = mobile_access_url(args.host, port)
    url = browser_access_url(args.host, port)
    print(f"{LAB_APP_NAME} running at {url}")
    if MarsLabHandler.mobile_url:
        print(f"Mobile access: {MarsLabHandler.mobile_url}")
    print("Press Ctrl+C to stop.")

    if not args.no_browser:
        threading.Timer(0.25, open_lab_url, args=(_control_url(url), args.browser)).start()

    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print(f"\nStopping {LAB_APP_NAME}.")
    finally:
        server.server_close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
