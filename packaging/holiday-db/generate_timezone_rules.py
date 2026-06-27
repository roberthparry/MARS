#!/usr/bin/env python3
"""Generate representative timezone rule tables for the jurisdiction database.

The jurisdiction database stores one representative timezone per jurisdiction. This
script reads those timezone names, extracts the required zone eras and daylight
saving transition rules from the host tzdata source file, and emits SQL inserts
so `holiday_t` can resolve GMT offsets without consulting host timezone APIs.
"""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
import re

TZDATA_PATH = Path("/usr/share/zoneinfo/tzdata.zi")
LOCATION_SQL_PATH = Path("packaging/holiday-db/mars_jurisdiction_location_defaults.sql")
OUT_PATH = Path("packaging/holiday-db/mars_timezone_rules.sql")

MONTH_INDEX = {
    "Ja": 1,
    "F": 2,
    "Mar": 3,
    "Ap": 4,
    "May": 5,
    "Jun": 6,
    "Jul": 7,
    "Au": 8,
    "S": 9,
    "O": 10,
    "N": 11,
    "D": 12,
}

WEEKDAY_INDEX = {
    "Su": 1,
    "M": 2,
    "Mo": 2,
    "T": 3,
    "Tu": 3,
    "W": 4,
    "We": 4,
    "Th": 5,
    "F": 6,
    "Fr": 6,
    "Sa": 7,
}


@dataclass(frozen=True)
class TransitionRule:
    rule_name: str
    from_year: int | None
    to_year: int | None
    in_month: int
    on_kind: str
    on_day: int
    on_weekday: int | None
    at_seconds: int
    at_suffix: str
    save_minutes: int
    letters: str | None


@dataclass(frozen=True)
class ZoneEra:
    timezone_name: str
    sequence_no: int
    gmtoff_minutes: int
    rules_kind: str
    fixed_save_minutes: int | None
    rule_name: str | None
    format_text: str
    until_year: int | None
    until_month: int | None
    until_day_kind: str | None
    until_day_value: int | None
    until_weekday: int | None
    until_seconds: int | None
    until_suffix: str | None


def sql_quote(value: str) -> str:
    return "'" + value.replace("'", "''") + "'"


def parse_year_token(token: str) -> int | None:
    if token == "ma":
        return None
    if token == "mi":
        return -999999
    if token == "o":
        raise ValueError("'o' should have been normalised before parsing")
    return int(token)


def parse_offset_seconds(token: str) -> int:
    sign = -1 if token.startswith("-") else 1
    text = token[1:] if token[:1] in "+-" else token
    parts = text.split(":")
    if len(parts) > 3:
        raise ValueError(f"unsupported offset token {token!r}")
    hours = int(parts[0]) if parts[0] else 0
    minutes = int(parts[1]) if len(parts) >= 2 else 0
    seconds = int(parts[2]) if len(parts) >= 3 else 0
    return sign * (hours * 3600 + minutes * 60 + seconds)


def parse_time_token(token: str) -> tuple[int, str]:
    suffix = "w"
    text = token
    if token[-1:].isalpha():
        suffix = token[-1]
        text = token[:-1]
    return parse_offset_seconds(text), suffix


def parse_day_expression(token: str) -> tuple[str, int, int | None]:
    last_match = re.fullmatch(r"last([A-Z][a-z]?|[A-Z])", token)
    if last_match:
        weekday = WEEKDAY_INDEX[last_match.group(1)]
        return ("last_weekday", 0, weekday)

    ge_match = re.fullmatch(r"([A-Z][a-z]?|[A-Z])>=(\d+)", token)
    if ge_match:
        weekday = WEEKDAY_INDEX[ge_match.group(1)]
        return ("weekday_on_or_after", int(ge_match.group(2)), weekday)

    le_match = re.fullmatch(r"([A-Z][a-z]?|[A-Z])<=(\d+)", token)
    if le_match:
        weekday = WEEKDAY_INDEX[le_match.group(1)]
        return ("weekday_on_or_before", int(le_match.group(2)), weekday)

    return ("day_of_month", int(token), None)


def parse_rule_line(line: str) -> TransitionRule:
    parts = line.split()
    if len(parts) != 10 or parts[0] != "R":
        raise ValueError(f"unexpected rule line {line!r}")

    from_token = parts[2]
    to_token = parts[3]
    if to_token == "o":
        to_token = from_token

    on_kind, on_day, on_weekday = parse_day_expression(parts[6])
    at_seconds, at_suffix = parse_time_token(parts[7])
    save_minutes = int(parse_offset_seconds(parts[8]) / 60)
    letters = None if parts[9] == "-" else parts[9]

    return TransitionRule(
        rule_name=parts[1],
        from_year=parse_year_token(from_token),
        to_year=parse_year_token(to_token),
        in_month=MONTH_INDEX[parts[5]],
        on_kind=on_kind,
        on_day=on_day,
        on_weekday=on_weekday,
        at_seconds=at_seconds,
        at_suffix=at_suffix,
        save_minutes=save_minutes,
        letters=letters,
    )


def parse_zone_era(timezone_name: str, sequence_no: int, tokens: list[str]) -> ZoneEra:
    if len(tokens) < 3:
        raise ValueError(f"unexpected zone era tokens {tokens!r}")

    gmtoff_minutes = int(parse_offset_seconds(tokens[0]) / 60)
    rule_token = tokens[1]
    rules_kind = "none"
    fixed_save_minutes = None
    rule_name = None

    if rule_token == "-":
        rules_kind = "none"
    else:
        try:
            fixed_save_minutes = int(parse_offset_seconds(rule_token) / 60)
            rules_kind = "fixed"
        except ValueError:
            rules_kind = "named"
            rule_name = rule_token

    until_year = None
    until_month = None
    until_day_kind = None
    until_day_value = None
    until_weekday = None
    until_seconds = None
    until_suffix = None

    if len(tokens) >= 4:
        until_year = int(tokens[3])
        until_month = 1
        until_day_kind = "day_of_month"
        until_day_value = 1
        until_weekday = None
        until_seconds = 0
        until_suffix = "w"

        if len(tokens) >= 5:
            until_month = MONTH_INDEX[tokens[4]]
        if len(tokens) >= 6:
            until_day_kind, until_day_value, until_weekday = parse_day_expression(tokens[5])
        if len(tokens) >= 7:
            until_seconds, until_suffix = parse_time_token(tokens[6])

    return ZoneEra(
        timezone_name=timezone_name,
        sequence_no=sequence_no,
        gmtoff_minutes=gmtoff_minutes,
        rules_kind=rules_kind,
        fixed_save_minutes=fixed_save_minutes,
        rule_name=rule_name,
        format_text=tokens[2],
        until_year=until_year,
        until_month=until_month,
        until_day_kind=until_day_kind,
        until_day_value=until_day_value,
        until_weekday=until_weekday,
        until_seconds=until_seconds,
        until_suffix=until_suffix,
    )


def parse_target_timezones() -> list[str]:
    pattern = re.compile(
        r"^\s*\('(?:[^']|'')*',\s*'(?:[^']|'')*',\s*'(?:[^']|'')*',\s*'((?:[^']|'')*)'",
    )
    zones: set[str] = set()
    for line in LOCATION_SQL_PATH.read_text(encoding="utf-8").splitlines():
        match = pattern.match(line)
        if match:
            zones.add(match.group(1).replace("''", "'"))
    return sorted(zones)


def parse_tzdata() -> tuple[dict[str, list[TransitionRule]], dict[str, list[ZoneEra]], dict[str, str]]:
    rules: dict[str, list[TransitionRule]] = {}
    zones: dict[str, list[ZoneEra]] = {}
    links: dict[str, str] = {}
    current_zone: str | None = None
    current_sequence = 0

    for raw_line in TZDATA_PATH.read_text(encoding="utf-8").splitlines():
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue

        if line.startswith("R "):
            current_zone = None
            rule = parse_rule_line(line)
            rules.setdefault(rule.rule_name, []).append(rule)
            continue

        if line.startswith("L "):
            current_zone = None
            parts = line.split()
            if len(parts) == 3:
                links[parts[2]] = parts[1]
            continue

        if line.startswith("Z "):
            parts = line.split()
            current_zone = parts[1]
            current_sequence = 1
            zones.setdefault(current_zone, []).append(parse_zone_era(current_zone, current_sequence, parts[2:]))
            continue

        if current_zone and (raw_line[:1] in {" ", "\t"} or raw_line[:1] in "+-0123456789"):
            current_sequence += 1
            zones[current_zone].append(parse_zone_era(current_zone, current_sequence, raw_line.split()))
            continue

        current_zone = None

    return rules, zones, links


def resolve_zone_name(zone_name: str, links: dict[str, str]) -> str:
    seen: set[str] = set()
    current = zone_name
    while current in links and current not in seen:
        seen.add(current)
        current = links[current]
    return current


def build_sql() -> str:
    target_timezones = parse_target_timezones()
    rules, zones, links = parse_tzdata()
    target_to_canonical = {
        zone_name: resolve_zone_name(zone_name, links)
        for zone_name in target_timezones
    }
    canonical_timezones = sorted(set(target_to_canonical.values()))

    missing = [zone_name for zone_name in canonical_timezones if zone_name not in zones]
    if missing:
        raise SystemExit(f"missing zones in tzdata.zi: {', '.join(missing)}")

    used_rule_names: set[str] = set()
    used_eras: list[ZoneEra] = []
    for zone_name in canonical_timezones:
        for era in zones[zone_name]:
            used_eras.append(era)
            if era.rule_name:
                used_rule_names.add(era.rule_name)

    used_rules: list[TransitionRule] = []
    for rule_name in sorted(used_rule_names):
        used_rules.extend(rules.get(rule_name, []))

    lines = [
        "-- Generated by packaging/holiday-db/generate_timezone_rules.py",
        "-- Representative timezone eras and daylight-saving rules for holiday jurisdictions.",
        "",
        "INSERT INTO timezone_definition(",
        "    timezone_name,",
        "    canonical_timezone_name,",
        "    notes",
        ") VALUES",
    ]
    lines.append(",\n".join(
        f"    ({sql_quote(zone_name)}, {sql_quote(canonical_zone_name)}, "
        f"{sql_quote('Representative timezone imported from host tzdata.zi.')})"
        for zone_name, canonical_zone_name in sorted(target_to_canonical.items())
    ) + ";")
    lines.append("")
    lines.extend([
        "INSERT INTO timezone_era(",
        "    timezone_name,",
        "    sequence_no,",
        "    gmtoff_minutes,",
        "    rules_kind,",
        "    fixed_save_minutes,",
        "    rule_name,",
        "    format_text,",
        "    until_year,",
        "    until_month,",
        "    until_day_kind,",
        "    until_day_value,",
        "    until_weekday,",
        "    until_seconds,",
        "    until_suffix",
        ") VALUES",
    ])
    lines.append(",\n".join(
        "    ("
        f"{sql_quote(era.timezone_name)}, "
        f"{era.sequence_no}, "
        f"{era.gmtoff_minutes}, "
        f"{sql_quote(era.rules_kind)}, "
        f"{'NULL' if era.fixed_save_minutes is None else era.fixed_save_minutes}, "
        f"{'NULL' if era.rule_name is None else sql_quote(era.rule_name)}, "
        f"{sql_quote(era.format_text)}, "
        f"{'NULL' if era.until_year is None else era.until_year}, "
        f"{'NULL' if era.until_month is None else era.until_month}, "
        f"{'NULL' if era.until_day_kind is None else sql_quote(era.until_day_kind)}, "
        f"{'NULL' if era.until_day_value is None else era.until_day_value}, "
        f"{'NULL' if era.until_weekday is None else era.until_weekday}, "
        f"{'NULL' if era.until_seconds is None else era.until_seconds}, "
        f"{'NULL' if era.until_suffix is None else sql_quote(era.until_suffix)})"
        for era in used_eras
    ) + ";")
    lines.append("")
    lines.extend([
        "INSERT INTO timezone_transition_rule(",
        "    rule_name,",
        "    from_year,",
        "    to_year,",
        "    in_month,",
        "    on_kind,",
        "    on_day,",
        "    on_weekday,",
        "    at_seconds,",
        "    at_suffix,",
        "    save_minutes,",
        "    letters",
        ") VALUES",
    ])
    lines.append(",\n".join(
        "    ("
        f"{sql_quote(rule.rule_name)}, "
        f"{'NULL' if rule.from_year is None else rule.from_year}, "
        f"{'NULL' if rule.to_year is None else rule.to_year}, "
        f"{rule.in_month}, "
        f"{sql_quote(rule.on_kind)}, "
        f"{rule.on_day}, "
        f"{'NULL' if rule.on_weekday is None else rule.on_weekday}, "
        f"{rule.at_seconds}, "
        f"{sql_quote(rule.at_suffix)}, "
        f"{rule.save_minutes}, "
        f"{'NULL' if rule.letters is None else sql_quote(rule.letters)})"
        for rule in used_rules
    ) + ";")

    return "\n".join(lines) + "\n"


def main() -> None:
    OUT_PATH.write_text(build_sql(), encoding="utf-8")


if __name__ == "__main__":
    main()
