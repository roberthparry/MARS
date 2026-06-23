#!/usr/bin/env python3
"""Generate materialized holiday instances for worldwide national holidays.

This uses the Python ``holidays`` package as a seed data source for visible
country coverage. The normalized rule schema remains the long-term model for
native MARS evaluation; this file generates a companion SQL import containing
actual holiday dates so the fixture can expose broad worldwide data now.
"""

from __future__ import annotations

from collections import defaultdict
from datetime import date, timedelta
import re
import unicodedata

import holidays
from dateutil.easter import easter
from workalendar.registry import registry as workalendar_registry

YEARS = tuple(range(1926, 2028))
OUT_PATH = "packaging/holiday-db/mars_generated_holiday_instances.sql"
SUBDIVISION_OUT_PATH = "packaging/holiday-db/mars_target_subdivisions.sql"
RULES_OUT_PATH = "packaging/holiday-db/mars_generated_first_class_rules.sql"
PRIMARY_SOURCE_DOCUMENT_ID = 6
SECONDARY_SOURCE_DOCUMENT_ID = 8
INFERRED_SOURCE_DOCUMENT_ID = 9
COUNTRY_ALIASES = {
    "UK": "GB",
}
TARGET_SUBDIVISION_COUNTRIES = ("AU", "CA", "US", "GB", "NZ", "PT", "IT")
SEEDED_RULE_JURISDICTIONS = {"GB-ENG", "AU", "NZ", "IE", "FR", "DE", "ZA", "DK", "NL"}


def sql_quote(value: str) -> str:
    return "'" + value.replace("'", "''") + "'"


def normalize_country_code(code: str) -> str | None:
    code = (code or "").strip().upper()
    code = COUNTRY_ALIASES.get(code, code)
    if len(code) == 2:
        return code
    return None


def subdivision_jurisdiction_id(country_code: str, subdiv_code: str) -> str:
    return f"{country_code}-{subdiv_code}"


def invert_aliases(aliases: dict[str, str]) -> dict[str, list[str]]:
    by_code: dict[str, list[str]] = defaultdict(list)
    for name, code in aliases.items():
        by_code[str(code)].append(str(name))
    return by_code


def preferred_subdivision_name(country_code: str, subdiv_code: str, alias_names: list[str]) -> str:
    preferred = sorted(alias_names, key=lambda name: (name.isupper(), len(name), name))
    if preferred:
        return preferred[0]
    return f"{country_code} subdivision {subdiv_code}"


def slugify(text: str) -> str:
    normalized = unicodedata.normalize("NFKD", text)
    ascii_text = normalized.encode("ascii", "ignore").decode("ascii")
    slug = re.sub(r"[^a-z0-9]+", "_", ascii_text.lower()).strip("_")
    return slug or "holiday"


def weekday_iso(dttm: date) -> int:
    return dttm.isoweekday()


def nth_weekday_ordinal(dttm: date) -> int:
    return 1 + (dttm.day - 1) // 7


def is_last_weekday_of_month(dttm: date) -> bool:
    return dttm.day + 7 > month_last_day(dttm.year, dttm.month)


def month_last_day(year: int, month: int) -> int:
    if month == 12:
        next_month = date(year + 1, 1, 1)
    else:
        next_month = date(year, month + 1, 1)
    return (next_month - timedelta(days=1)).day


def easter_offset_days(dttm: date) -> int:
    return (dttm - easter(dttm.year)).days


def contiguous_years(years: list[int]) -> list[tuple[int, int]]:
    if not years:
        return []
    spans: list[tuple[int, int]] = []
    start = prev = years[0]
    for year in years[1:]:
        if year == prev + 1:
            prev = year
            continue
        spans.append((start, prev))
        start = prev = year
    spans.append((start, prev))
    return spans


def nth_weekday_of_month(year: int, month: int, weekday_iso_num: int, ordinal: int) -> date:
    first = date(year, month, 1)
    offset = (weekday_iso_num - first.isoweekday()) % 7
    return first + timedelta(days=offset + 7 * (ordinal - 1))


def last_weekday_of_month(year: int, month: int, weekday_iso_num: int) -> date:
    last = date(year, month, month_last_day(year, month))
    offset = (last.isoweekday() - weekday_iso_num) % 7
    return last - timedelta(days=offset)


def england_bank_holidays_for_year(year: int) -> list[tuple[date, str]]:
    rows: list[tuple[date, str]] = []

    new_year = date(year, 1, 1)
    if new_year.isoweekday() == 6:
        rows.append((new_year + timedelta(days=2), "Bank Holiday in Lieu of New Years Day"))
    elif new_year.isoweekday() == 7:
        rows.append((new_year + timedelta(days=1), "Bank Holiday in Lieu of New Years Day"))
    else:
        rows.append((new_year, "New Years Day"))

    easter_sunday = easter(year)
    rows.append((easter_sunday - timedelta(days=2), "Good Friday"))
    rows.append((easter_sunday + timedelta(days=1), "Easter Monday"))

    if year == 2020:
        rows.append((date(2020, 5, 8), "75th anniversary of Victory in Europe (VE Day)"))
    else:
        rows.append((nth_weekday_of_month(year, 5, 1, 1), "May Day Bank Holiday"))

    if year == 2022:
        rows.append((date(2022, 6, 2), "Spring Bank Holiday"))
        rows.append((date(2022, 6, 3), "Platinum Jubilee Bank Holiday"))
        rows.append((date(2022, 9, 19), "State Funeral of Queen Elizabeth II"))
    else:
        rows.append((last_weekday_of_month(year, 5, 1), "Spring Bank Holiday"))

    if year == 2023:
        rows.append((date(2023, 5, 8), "Coronation of King Charles III"))

    rows.append((last_weekday_of_month(year, 8, 1), "August Bank Holiday"))

    christmas = date(year, 12, 25)
    boxing = date(year, 12, 26)
    if christmas.isoweekday() in (6, 7):
        rows.append((christmas + timedelta(days=2), "Bank Holiday in Lieu of Christmas Day"))
    else:
        rows.append((christmas, "Christmas Day"))

    if christmas.isoweekday() in (5, 6):
        rows.append((boxing + timedelta(days=2), "Bank Holiday in Lieu of Boxing Day"))
    elif boxing.isoweekday() == 7:
        rows.append((boxing + timedelta(days=1), "Bank Holiday in Lieu of Boxing Day"))
    else:
        rows.append((boxing, "Boxing Day"))

    return sorted(rows)


def infer_pattern(dates: list[date]) -> tuple[str, dict[str, int | str]] | None:
    if not dates:
        return None

    if all((d.month, d.day) == (dates[0].month, dates[0].day) for d in dates):
        return ("fixed_date", {"month": dates[0].month, "day": dates[0].day})

    easter_offsets = {easter_offset_days(d) for d in dates}
    if len(easter_offsets) == 1:
        return ("easter_offset", {"offset_days": next(iter(easter_offsets))})

    months = {d.month for d in dates}
    weekdays = {weekday_iso(d) for d in dates}
    ordinals = {nth_weekday_ordinal(d) for d in dates}
    if len(months) == 1 and len(weekdays) == 1 and len(ordinals) == 1:
        return (
            "nth_weekday",
            {"month": next(iter(months)), "weekday": next(iter(weekdays)), "ordinal": next(iter(ordinals))},
        )

    if len(months) == 1 and len(weekdays) == 1 and all(is_last_weekday_of_month(d) for d in dates):
        return (
            "last_weekday",
            {"month": next(iter(months)), "weekday": next(iter(weekdays)), "ordinal": -1},
        )

    return None


def infer_segments(date_rows: list[tuple[int, date]]) -> list[tuple[str, dict[str, int | str], int, int, list[date]]]:
    by_pattern: list[tuple[str, dict[str, int | str], int, int, list[date]]] = []
    years = sorted(year for year, _ in date_rows)
    for start, end in contiguous_years(years):
        segment_dates = [d for year, d in date_rows if start <= year <= end]
        pattern = infer_pattern(segment_dates)
        if pattern:
            by_pattern.append((pattern[0], pattern[1], start, end, segment_dates))
        else:
            for year, dttm in ((year, d) for year, d in date_rows if start <= year <= end):
                by_pattern.append(("one_off", {"holiday_date": dttm.isoformat()}, year, year, [dttm]))
    return by_pattern


def generate_first_class_rules() -> None:
    holiday_id = 1_000_000
    holiday_name_id = 2_000_000
    rule_id = 3_000_000
    rule_source_id = 4_000_000

    definition_lines: list[str] = []
    name_lines: list[str] = []
    rule_lines: list[str] = []
    rule_source_lines: list[str] = []

    supported_codes = sorted(
        code
        for code in {normalize_country_code(raw_code) for raw_code in holidays.list_supported_countries()}
        if code
    )
    for country_code in supported_codes:
        if not country_code or country_code in SEEDED_RULE_JURISDICTIONS:
            continue

        try:
            holiday_map = holidays.country_holidays(country_code, years=YEARS, expand=False, observed=False)
        except Exception:
            continue

        by_name: dict[str, list[tuple[int, date]]] = defaultdict(list)
        for holiday_date, holiday_name in sorted(holiday_map.items()):
            by_name[str(holiday_name)].append((holiday_date.year, holiday_date))

        langs = getattr(holiday_map, "supported_languages", None)
        locale = langs[0] if langs else "und"
        used_keys: set[str] = set()

        for holiday_name, date_rows in sorted(by_name.items()):
            years = [year for year, _ in date_rows]
            base_key = slugify(holiday_name)
            holiday_key = base_key
            suffix = 2
            while holiday_key in used_keys:
                holiday_key = f"{base_key}_{suffix}"
                suffix += 1
            used_keys.add(holiday_key)

            definition_lines.append(
                "("
                + ", ".join(
                    [
                        str(holiday_id),
                        sql_quote(country_code),
                        sql_quote(holiday_key),
                        sql_quote(holiday_name),
                        sql_quote("public"),
                        sql_quote("full_day"),
                        sql_quote("gregory"),
                        str(min(years)),
                        str(max(years)),
                        sql_quote("Inferred first-class holiday definition from materialized country-level holiday history."),
                    ]
                )
                + ")"
            )
            name_lines.append(
                "("
                + ", ".join(
                    [
                        str(holiday_name_id),
                        str(holiday_id),
                        sql_quote(locale),
                        sql_quote(holiday_name),
                        "1",
                    ]
                )
                + ")"
            )
            holiday_name_id += 1

            for pattern_kind, attrs, start_year, end_year, _segment_dates in infer_segments(date_rows):
                month = attrs.get("month")
                day = attrs.get("day")
                weekday = attrs.get("weekday")
                ordinal = attrs.get("ordinal")
                offset_days = attrs.get("offset_days")
                holiday_date = attrs.get("holiday_date")

                rule_lines.append(
                    "("
                    + ", ".join(
                        [
                            str(rule_id),
                            str(holiday_id),
                            "1",
                            sql_quote(pattern_kind),
                            "NULL" if month is None else str(month),
                            "NULL" if day is None else str(day),
                            "NULL" if weekday is None else str(weekday),
                            "NULL" if ordinal is None else str(ordinal),
                            "NULL" if offset_days is None else str(offset_days),
                            "NULL",
                            "NULL",
                            "NULL",
                            "NULL",
                            "NULL" if holiday_date is None else sql_quote(str(holiday_date)),
                            str(start_year),
                            str(end_year),
                            "500",
                            sql_quote("Inferred recurring rule from materialized holiday instances."),
                        ]
                    )
                    + ")"
                )
                rule_source_lines.append(
                    "("
                    + ", ".join(
                        [
                            str(rule_source_id),
                            str(holiday_id),
                            str(rule_id),
                            "NULL",
                            "NULL",
                            str(INFERRED_SOURCE_DOCUMENT_ID),
                            sql_quote("definition"),
                            sql_quote("Automatically inferred first-class rule from non-observed country-level holiday history."),
                        ]
                    )
                    + ")"
                )
                rule_id += 1
                rule_source_id += 1

            holiday_id += 1

    with open(RULES_OUT_PATH, "w", encoding="utf-8") as handle:
        handle.write("-- Generated by packaging/holiday-db/generate_holiday_instances.py\n")
        handle.write("-- Automatically inferred first-class holiday definitions and rules.\n\n")

        if definition_lines:
            handle.write(
                "INSERT INTO holiday_definition(\n"
                "    holiday_id,\n"
                "    jurisdiction_id,\n"
                "    holiday_key,\n"
                "    default_name,\n"
                "    holiday_class,\n"
                "    scope,\n"
                "    calendar_system_id,\n"
                "    valid_from_year,\n"
                "    valid_to_year,\n"
                "    notes\n"
                ") VALUES\n    "
                + ",\n    ".join(definition_lines)
                + ";\n\n"
            )

        if name_lines:
            handle.write(
                "INSERT INTO holiday_name(\n"
                "    holiday_name_id,\n"
                "    holiday_id,\n"
                "    locale,\n"
                "    localized_name,\n"
                "    is_primary\n"
                ") VALUES\n    "
                + ",\n    ".join(name_lines)
                + ";\n\n"
            )

        if rule_lines:
            handle.write(
                "INSERT INTO holiday_rule(\n"
                "    rule_id,\n"
                "    holiday_id,\n"
                "    sequence_no,\n"
                "    rule_kind,\n"
                "    month,\n"
                "    day,\n"
                "    weekday,\n"
                "    ordinal,\n"
                "    offset_days,\n"
                "    anchor_holiday_key,\n"
                "    rrule_text,\n"
                "    expression_language,\n"
                "    expression_text,\n"
                "    holiday_date,\n"
                "    valid_from_year,\n"
                "    valid_to_year,\n"
                "    priority,\n"
                "    notes\n"
                ") VALUES\n    "
                + ",\n    ".join(rule_lines)
                + ";\n\n"
            )

        if rule_source_lines:
            handle.write(
                "INSERT INTO holiday_rule_source(\n"
                "    holiday_rule_source_id,\n"
                "    holiday_id,\n"
                "    rule_id,\n"
                "    observance_rule_id,\n"
                "    exception_id,\n"
                "    source_document_id,\n"
                "    role,\n"
                "    notes\n"
                ") VALUES\n    "
                + ",\n    ".join(rule_source_lines)
                + ";\n"
            )


def generate_subdivision_sql() -> None:
    rows: list[tuple[str, str, str, str]] = []
    for country_code in TARGET_SUBDIVISION_COUNTRIES:
        base = holidays.country_holidays(country_code)
        subdivs = list(getattr(base, "subdivisions", []) or [])
        aliases = invert_aliases(getattr(base, "subdivisions_aliases", {}) or {})
        for subdiv_code in subdivs:
            if country_code == "GB" and subdiv_code == "ENG":
                continue
            jurisdiction_id = subdivision_jurisdiction_id(country_code, subdiv_code)
            subdivision_name = preferred_subdivision_name(country_code, subdiv_code, aliases.get(subdiv_code, []))
            rows.append((jurisdiction_id, country_code, subdiv_code, subdivision_name))

    with open(SUBDIVISION_OUT_PATH, "w", encoding="utf-8") as handle:
        handle.write("-- Generated by packaging/holiday-db/generate_holiday_instances.py\n")
        handle.write("-- Target subdivision jurisdictions for countries requested to carry deeper holiday coverage.\n\n")
        handle.write("INSERT INTO jurisdiction(\n")
        handle.write("    jurisdiction_id,\n")
        handle.write("    parent_jurisdiction_id,\n")
        handle.write("    jurisdiction_type,\n")
        handle.write("    iso_country_code,\n")
        handle.write("    iso_subdivision_code,\n")
        handle.write("    cldr_region_code,\n")
        handle.write("    name,\n")
        handle.write("    notes\n")
        handle.write(") VALUES\n")

        first = True
        for jurisdiction_id, country_code, subdiv_code, subdivision_name in rows:
            values = (
                sql_quote(jurisdiction_id),
                sql_quote(country_code),
                sql_quote("subdivision"),
                sql_quote(country_code),
                sql_quote(jurisdiction_id),
                sql_quote(country_code),
                sql_quote(subdivision_name),
                sql_quote("Generated targeted subdivision jurisdiction for deeper holiday coverage."),
            )
            prefix = "    " if first else ",\n    "
            handle.write(prefix + "(" + ", ".join(values) + ")")
            first = False

        handle.write(";\n")


def collect_primary_rows() -> dict[str, list[dict[str, str]]]:
    supported = holidays.list_supported_countries()
    rows_by_country: dict[str, list[dict[str, str]]] = defaultdict(list)
    seen: set[tuple[str, date, str]] = set()

    for raw_code in sorted(supported):
        country_code = normalize_country_code(raw_code)
        if not country_code:
            continue

        try:
            holiday_map = holidays.country_holidays(country_code, years=YEARS, expand=False, observed=True)
        except Exception:
            continue

        language = None
        langs = getattr(holiday_map, "supported_languages", None)
        if langs:
            language = langs[0]

        for holiday_date, holiday_name in sorted(holiday_map.items()):
            key = (country_code, holiday_date, str(holiday_name))
            if key in seen:
                continue
            seen.add(key)
            rows_by_country[country_code].append({
                "jurisdiction_id": country_code,
                "holiday_date": holiday_date.isoformat(),
                "holiday_name": str(holiday_name),
                "holiday_class": "public",
                "language": language or "und",
                "source_document_id": PRIMARY_SOURCE_DOCUMENT_ID,
                "notes": (
                    f"Imported from python-holidays {holidays.__version__} for materialized coverage "
                    f"over requested years {YEARS[0]}-{YEARS[-1]}; source availability varies by jurisdiction."
                ),
            })

    return rows_by_country


def collect_target_subdivision_rows(rows_by_country: dict[str, list[dict[str, str]]]) -> dict[str, list[dict[str, str]]]:
    seen = {
        (row["jurisdiction_id"], row["holiday_date"], row["holiday_name"])
        for rows in rows_by_country.values()
        for row in rows
    }

    for country_code in TARGET_SUBDIVISION_COUNTRIES:
        base = holidays.country_holidays(country_code)
        subdivs = list(getattr(base, "subdivisions", []) or [])
        langs = getattr(base, "supported_languages", None)
        language = langs[0] if langs else "und"

        for subdiv_code in subdivs:
            if country_code == "GB" and subdiv_code == "ENG":
                continue

            jurisdiction_id = subdivision_jurisdiction_id(country_code, subdiv_code)
            try:
                holiday_map = holidays.country_holidays(
                    country_code,
                    years=YEARS,
                    subdiv=subdiv_code,
                    expand=False,
                    observed=True,
                )
            except Exception:
                continue

            langs = getattr(holiday_map, "supported_languages", None)
            if langs:
                language = langs[0]

            for holiday_date, holiday_name in sorted(holiday_map.items()):
                key = (jurisdiction_id, holiday_date.isoformat(), str(holiday_name))
                if key in seen:
                    continue
                seen.add(key)
                rows_by_country[jurisdiction_id].append({
                    "jurisdiction_id": jurisdiction_id,
                    "holiday_date": holiday_date.isoformat(),
                    "holiday_name": str(holiday_name),
                    "holiday_class": "public",
                    "language": language or "und",
                    "source_document_id": PRIMARY_SOURCE_DOCUMENT_ID,
                    "notes": (
                        f"Imported subdivision coverage from python-holidays {holidays.__version__} "
                        f"for requested years {YEARS[0]}-{YEARS[-1]}."
                    ),
                })

    return rows_by_country


def collect_seeded_jurisdiction_rows(rows_by_country: dict[str, list[dict[str, str]]]) -> dict[str, list[dict[str, str]]]:
    for year in YEARS:
        for holiday_date, holiday_name in england_bank_holidays_for_year(year):
            rows_by_country["GB-ENG"].append({
                "jurisdiction_id": "GB-ENG",
                "holiday_date": holiday_date.isoformat(),
                "holiday_name": holiday_name,
                "holiday_class": "bank",
                "language": "en-GB",
                "source_document_id": 7,
                "notes": (
                    f"Materialized from first-class England bank holiday seed rules for year {year}; "
                    "substitute-day naming follows the configured observance labels."
                ),
            })

    rows_by_country["GB-ENG"].sort(key=lambda row: (row["holiday_date"], row["holiday_name"]))
    return rows_by_country


def iter_workalendar_rows(country_code: str, year_stop: int):
    calendar_cls = workalendar_registry.region_registry.get(country_code)
    if calendar_cls is None or year_stop < YEARS[0]:
        return

    try:
        calendar = calendar_cls()
    except Exception:
        return

    for year in range(YEARS[0], year_stop + 1):
        try:
            holidays_for_year = calendar.holidays(year)
        except Exception:
            continue
        for holiday_date, holiday_name in sorted(holidays_for_year):
            yield {
                "jurisdiction_id": country_code,
                "holiday_date": holiday_date.isoformat(),
                "holiday_name": str(holiday_name),
                "holiday_class": "public",
                "language": "und",
                "source_document_id": SECONDARY_SOURCE_DOCUMENT_ID,
                "notes": (
                    f"Backfilled from workalendar 17.0.0 for years before primary-source coverage "
                    f"over requested years {YEARS[0]}-{YEARS[-1]}."
                ),
            }


def merge_secondary_rows(rows_by_country: dict[str, list[dict[str, str]]]) -> dict[str, list[dict[str, str]]]:
    existing_keys = {
        (row["jurisdiction_id"], row["holiday_date"], row["holiday_name"])
        for rows in rows_by_country.values()
        for row in rows
    }

    secondary_codes = sorted(
        code
        for code in {normalize_country_code(code) for code in workalendar_registry.region_registry}
        if code
    )

    for country_code in secondary_codes:

        primary_rows = rows_by_country.get(country_code, [])
        if primary_rows:
            first_primary_year = min(int(row["holiday_date"][:4]) for row in primary_rows)
            year_stop = first_primary_year - 1
        else:
            year_stop = YEARS[-1]

        for row in iter_workalendar_rows(country_code, year_stop):
            key = (row["jurisdiction_id"], row["holiday_date"], row["holiday_name"])
            if key in existing_keys:
                continue
            existing_keys.add(key)
            rows_by_country[country_code].append(row)

    return rows_by_country


def main() -> None:
    generate_subdivision_sql()
    generate_first_class_rules()
    rows_by_country = collect_primary_rows()
    rows_by_country = collect_target_subdivision_rows(rows_by_country)
    rows_by_country = collect_seeded_jurisdiction_rows(rows_by_country)
    rows_by_country = merge_secondary_rows(rows_by_country)

    with open(OUT_PATH, "w", encoding="utf-8") as handle:
        handle.write("-- Generated by packaging/holiday-db/generate_holiday_instances.py\n")
        handle.write(f"-- Source packages: python-holidays {holidays.__version__}; workalendar 17.0.0\n")
        handle.write(f"-- Years: {', '.join(str(y) for y in YEARS)}\n\n")
        handle.write("INSERT INTO holiday_instance(\n")
        handle.write("    jurisdiction_id,\n")
        handle.write("    holiday_date,\n")
        handle.write("    holiday_name,\n")
        handle.write("    holiday_class,\n")
        handle.write("    language,\n")
        handle.write("    source_document_id,\n")
        handle.write("    notes\n")
        handle.write(") VALUES\n")

        first = True
        for country_code in sorted(rows_by_country):
            for row in rows_by_country[country_code]:
                values = (
                    sql_quote(row["jurisdiction_id"]),
                    sql_quote(row["holiday_date"]),
                    sql_quote(row["holiday_name"]),
                    sql_quote(row["holiday_class"]),
                    sql_quote(row["language"]),
                    str(row["source_document_id"]),
                    sql_quote(row["notes"]),
                )
                prefix = "    " if first else ",\n    "
                handle.write(prefix + "(" + ", ".join(values) + ")")
                first = False

        handle.write(";\n")


if __name__ == "__main__":
    main()
