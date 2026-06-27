#!/usr/bin/env python3
"""Generate jurisdiction default locations for the jurisdiction database.

Country-level rows are derived from the local tzdata ``zone.tab`` entries so
each ISO country code keeps its own principal coordinate and timezone instead of
inheriting a shared multi-country representative row. Hand-curated overrides
remain in place for jurisdictions where we want a known capital or subdivision
centre instead.
"""

from __future__ import annotations

from pathlib import Path
import re

OUT_PATH = Path("packaging/holiday-db/mars_jurisdiction_location_defaults.sql")
COUNTRY_SQL_PATH = Path("packaging/holiday-db/mars_country_jurisdictions.sql")
ZONE_TAB_PATH = Path("/usr/share/zoneinfo/zone.tab")


def sql_quote(value: str) -> str:
    return "'" + value.replace("'", "''") + "'"


def parse_country_ids() -> list[str]:
    pattern = re.compile(r"^\s*\('([A-Z]{2})',\s+NULL,\s+'country'")
    ids: list[str] = []
    for line in COUNTRY_SQL_PATH.read_text(encoding="utf-8").splitlines():
        match = pattern.match(line)
        if match:
            ids.append(match.group(1))
    return ids


def parse_iso6709_compact(text: str) -> tuple[float, float]:
    text = text.strip()
    if len(text) not in (11, 15):
        raise ValueError(f"unsupported ISO 6709 coordinate {text!r}")

    lat_sign = -1.0 if text[0] == "-" else 1.0
    lon_sign_index = 5 if len(text) == 11 else 7
    lon_sign = -1.0 if text[lon_sign_index] == "-" else 1.0

    if len(text) == 11:
        lat_deg = int(text[1:3])
        lat_min = int(text[3:5])
        lon_deg = int(text[6:9])
        lon_min = int(text[9:11])
        latitude = lat_sign * (lat_deg + lat_min / 60.0)
        longitude = lon_sign * (lon_deg + lon_min / 60.0)
    else:
        lat_deg = int(text[1:3])
        lat_min = int(text[3:5])
        lat_sec = int(text[5:7])
        lon_deg = int(text[8:11])
        lon_min = int(text[11:13])
        lon_sec = int(text[13:15])
        latitude = lat_sign * (lat_deg + lat_min / 60.0 + lat_sec / 3600.0)
        longitude = lon_sign * (lon_deg + lon_min / 60.0 + lon_sec / 3600.0)

    return latitude, longitude


def fallback_locality_name(tz_name: str, comment: str) -> str:
    if comment:
        return comment
    return tz_name.rsplit("/", 1)[-1].replace("_", " ")


def parse_zone_tab_rows() -> dict[str, tuple[str, str, str]]:
    rows: dict[str, tuple[str, str, str]] = {}

    for raw_line in ZONE_TAB_PATH.read_text(encoding="utf-8").splitlines():
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue
        parts = raw_line.split("\t")
        if len(parts) < 3:
            continue
        country_code, coords, tz_name = parts[:3]
        comment = parts[3].strip() if len(parts) > 3 else ""
        code = country_code.strip().upper()

        if len(code) != 2 or code in rows:
            continue

        rows[code] = (
            coords.strip(),
            tz_name.strip(),
            fallback_locality_name(tz_name, comment),
        )

    return rows


EXPLICIT_ROWS: dict[str, tuple[str, str, str, str, str]] = {
    "AU": ("-35.2802", "149.1310", "Australia/Sydney", "Canberra", "Country capital default."),
    "AU-ACT": ("-35.2802", "149.1310", "Australia/Sydney", "Canberra", "Territory capital default."),
    "AU-NSW": ("-33.8688", "151.2093", "Australia/Sydney", "Sydney", "State capital default."),
    "AU-NT": ("-12.4634", "130.8456", "Australia/Darwin", "Darwin", "Territory capital default."),
    "AU-QLD": ("-27.4698", "153.0251", "Australia/Brisbane", "Brisbane", "State capital default."),
    "AU-SA": ("-34.9285", "138.6007", "Australia/Adelaide", "Adelaide", "State capital default."),
    "AU-TAS": ("-42.8821", "147.3272", "Australia/Hobart", "Hobart", "State capital default."),
    "AU-VIC": ("-37.8136", "144.9631", "Australia/Melbourne", "Melbourne", "State capital default."),
    "AU-WA": ("-31.9523", "115.8613", "Australia/Perth", "Perth", "State capital default."),
    "CA": ("45.4215", "-75.6972", "America/Toronto", "Ottawa", "Country capital default."),
    "CA-AB": ("53.5461", "-113.4938", "America/Edmonton", "Edmonton", "Province capital default."),
    "CA-BC": ("48.4284", "-123.3656", "America/Vancouver", "Victoria", "Province capital default."),
    "CA-MB": ("49.8951", "-97.1384", "America/Winnipeg", "Winnipeg", "Province capital default."),
    "CA-NB": ("45.9636", "-66.6431", "America/Moncton", "Fredericton", "Province capital default."),
    "CA-NL": ("47.5615", "-52.7126", "America/St_Johns", "St. John's", "Province capital default."),
    "CA-NS": ("44.6488", "-63.5752", "America/Halifax", "Halifax", "Province capital default."),
    "CA-NT": ("62.4540", "-114.3718", "America/Yellowknife", "Yellowknife", "Territory capital default."),
    "CA-NU": ("63.7467", "-68.5170", "America/Iqaluit", "Iqaluit", "Territory capital default."),
    "CA-ON": ("43.6532", "-79.3832", "America/Toronto", "Toronto", "Province capital default."),
    "CA-PE": ("46.2382", "-63.1311", "America/Halifax", "Charlottetown", "Province capital default."),
    "CA-QC": ("46.8139", "-71.2080", "America/Toronto", "Quebec City", "Province capital default."),
    "CA-SK": ("50.4452", "-104.6189", "America/Regina", "Regina", "Province capital default."),
    "CA-YT": ("60.7212", "-135.0568", "America/Whitehorse", "Whitehorse", "Territory capital default."),
    "DE": ("52.5200", "13.4050", "Europe/Berlin", "Berlin", "Country capital default."),
    "DK": ("55.6761", "12.5683", "Europe/Copenhagen", "Copenhagen", "Country capital default."),
    "FR": ("48.8566", "2.3522", "Europe/Paris", "Paris", "Country capital default."),
    "GB": ("51.5074", "-0.1278", "Europe/London", "London", "Country capital default."),
    "GB-ENG": ("52.7077", "-2.7541", "Europe/London", "Shrewsbury", "England default."),
    "GB-NIR": ("54.5973", "-5.9301", "Europe/London", "Belfast", "Northern Ireland default."),
    "GB-SCT": ("55.9533", "-3.1883", "Europe/London", "Edinburgh", "Scotland default."),
    "GB-WLS": ("53.3210", "-3.4800", "Europe/London", "Rhyl", "Wales default."),
    "GR": ("37.9838", "23.7275", "Europe/Athens", "Athens", "Country capital default."),
    "IS": ("64.1466", "-21.9426", "Atlantic/Reykjavik", "Reykjavik", "Country capital default."),
    "IE": ("53.3498", "-6.2603", "Europe/Dublin", "Dublin", "Country capital default."),
    "IT": ("41.9028", "12.4964", "Europe/Rome", "Rome", "Country capital default."),
    "NL": ("52.5697", "4.6948", "Europe/Amsterdam", "Limmen", "Country default."),
    "NZ": ("-41.2866", "174.7756", "Pacific/Auckland", "Wellington", "Country capital default."),
    "NZ-AUK": ("-36.8509", "174.7645", "Pacific/Auckland", "Auckland", "Subdivision centre default."),
    "NZ-WGN": ("-41.2866", "174.7756", "Pacific/Auckland", "Wellington", "Subdivision centre default."),
    "PT": ("38.7223", "-9.1393", "Europe/Lisbon", "Lisbon", "Country capital default."),
    "PT-11": ("38.7223", "-9.1393", "Europe/Lisbon", "Lisbon", "District centre default."),
    "PT-13": ("41.1579", "-8.6291", "Europe/Lisbon", "Porto", "District centre default."),
    "SJ": ("78.2232", "15.6469", "Arctic/Longyearbyen", "Longyearbyen", "Country principal settlement default."),
    "UA": ("50.4501", "30.5234", "Europe/Kyiv", "Kyiv", "Country capital default."),
    "US": ("38.9072", "-77.0369", "America/New_York", "Washington, D.C.", "Country capital default."),
    "US-AK": ("58.3019", "-134.4197", "America/Juneau", "Juneau", "State capital default."),
    "US-DC": ("38.9072", "-77.0369", "America/New_York", "Washington, D.C.", "District default."),
    "ZA": ("-33.9249", "18.4241", "Africa/Johannesburg", "Cape Town", "Country default."),
}


def build_rows() -> list[tuple[str, str, str, str, str, str]]:
    country_ids = parse_country_ids()
    zone_rows = parse_zone_tab_rows()
    rows: dict[str, tuple[str, str, str, str, str]] = {}

    for country_id in country_ids:
        if country_id in EXPLICIT_ROWS:
            rows[country_id] = EXPLICIT_ROWS[country_id]
            continue
        zone_row = zone_rows.get(country_id)
        if not zone_row:
            continue
        coords, tz_name, locality = zone_row
        latitude, longitude = parse_iso6709_compact(coords)
        rows[country_id] = (
            f"{latitude:.4f}",
            f"{longitude:.4f}",
            tz_name,
            locality,
            "Representative principal location default generated from tzdata zone.tab.",
        )

    for jurisdiction_id, data in EXPLICIT_ROWS.items():
        rows[jurisdiction_id] = data

    ordered_ids = sorted(rows)
    return [(jid, *rows[jid]) for jid in ordered_ids]


def main() -> None:
    rows = build_rows()
    lines = [
        "-- Generated by packaging/holiday-db/generate_jurisdiction_location_defaults.py",
        "-- Country rows fall back to tzdata zone.tab principal locations; curated overrides pin known capitals and subdivision centres.",
        "",
        "INSERT INTO jurisdiction_location_default(",
        "    jurisdiction_id,",
        "    latitude,",
        "    longitude,",
        "    timezone_name,",
        "    locality_name,",
        "    notes",
        ") VALUES",
    ]

    value_lines = []
    for jurisdiction_id, latitude, longitude, timezone_name, locality_name, notes in rows:
        value_lines.append(
            "    ("
            f"{sql_quote(jurisdiction_id)}, "
            f"{sql_quote(latitude)}, "
            f"{sql_quote(longitude)}, "
            f"{sql_quote(timezone_name)}, "
            f"{sql_quote(locality_name)}, "
            f"{sql_quote(notes)})"
        )

    if value_lines:
        lines.append(",\n".join(value_lines) + ";")
    else:
        lines.append("    ('', '', '', '', '');")

    OUT_PATH.write_text("\n".join(lines) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
