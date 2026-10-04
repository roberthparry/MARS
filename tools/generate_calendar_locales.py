#!/usr/bin/env python3
"""Generate calendar labels from Babel 2.17.0 (CLDR 46) and explicit supplements.

Babel is required only for regeneration, never installation or calendar queries.
The extracted Unicode data is covered by THIRD_PARTY_NOTICES.md.
"""

import argparse
from copy import deepcopy
from functools import lru_cache
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
OUTPUT = Path("packaging/jurisdiction-db/mars_calendar_locale_names.sql")
# Explicit user-selectable supplements, not claims of national official status.
SUPPLEMENTAL_LANGUAGE_OPTIONS = {
    "CA-QC": {"en": "additional"},
    "FR": {"br": "regional"},
    "GB": {"kw": "regional"},
    "IL": {"ar": "additional", "en": "additional", "he": "additional", "lad": "additional", "yi": "additional"},
    "JM": {"jam": "additional"},
    "PS": {"ar": "additional", "en": "additional", "he": "additional", "lad": "additional", "yi": "additional"},
    "US": {"frc": "additional", "lad": "additional", "pdc": "additional", "yi": "additional"},
    "US-AK": {"ik": "official_regional"},
}
SUPPLEMENTAL_TOWN_LANGUAGES = {
    "GB": {"jam": ("Birmingham", "Bristol", "Leeds", "Leicester", "London", "Manchester", "Nottingham")},
}
# These languages lack CLDR 46 date data. Keep supplements explicit rather than
# silently substituting another language. Abbreviations and patterns are curated.
SUPPLEMENTAL_LOCALES = {
    # Common calendar vocabulary: LSU's Cajun French by Themes (L'almanaque).
    # https://www.lsu.edu/hss/french/undergraduate_program/cajun_french/cajun_french_by_themes.php
    "frc": {
        "english_name": "Cajun French", "native_name": "français cadien",
        "date_pattern": "d 'de' MMMM y",
        "months": (("janv.", "janvier"), ("févr.", "février"), ("mars", "mars"), ("avr.", "avril"),
                   ("mai", "mai"), ("juin", "juin"), ("juil.", "juillet"), ("août", "août"),
                   ("sept.", "septembre"), ("oct.", "octobre"), ("nov.", "novembre"), ("déc.", "décembre")),
        "weekdays": (("lundi", "lun."), ("mardi", "mar."), ("mercredi", "mer."), ("jeudi", "jeu."),
                     ("vendredi", "ven."), ("samedi", "sam."), ("dimanche", "dim.")),
    },
    # North Slope Iñupiaq vocabulary, not Canadian Inuktitut (iu) or Yup'ik.
    # https://inupiaq.tusaalanga.ca/glossary/english?showall=1
    # https://www.inupiaqonline.com/appendix/29
    # https://beringstraits.com/wp-content/uploads/2024/01/Abridged-lnupiaq-and-English-Dictionary-by-Edna-Maclean.pdf
    # Short forms and the Gregorian display pattern are application conventions.
    "ik": {
        "english_name": "Iñupiaq (North Slope)", "native_name": "Iñupiatun",
        "date_pattern": "MMMM d, y",
        "months": (("Siqiññaat.", "Siqiññaatchiaq"), ("Siqiññaas.", "Siqiññaasugruk"), ("Pani.", "Paniqsiqsiivik"),
                   ("Umia.", "Umiaqqavik"), ("Supp.", "Suppivik"), ("Iġñ.", "Iġñivik"), ("Iñuk.", "Iñukkuksaivik"),
                   ("Amiġ.", "Amiġaiqsivik"), ("Siku.", "Sikuaqtuġvik"), ("Sikk.", "Sikkuvik"),
                   ("Nipp.", "Nippivik"), ("Siqiñġi.", "Siqiñġiḷaq")),
        "weekdays": (("Atautchiiġñiq", "Ata."), ("Aippiġñiq", "Aip."), ("Piŋatchiġñiq", "Piŋ."),
                     ("Sisammiġñiq", "Sis."), ("Tallimmiġñiq", "Tal."), ("Itchaksriġñiq", "Itc."), ("Savaiññiq", "Sav.")),
    },
    # English-based Patwa spellings; short forms and date pattern are application conventions.
    # https://jamaicanpatwah.com/b/talk-like-a-jamaican-how-to-say-the-weekdays-and-months
    "jam": {
        "english_name": "Jamaican Patois", "native_name": "Patwa",
        "date_pattern": "d MMMM y",
        "months": (("Jan.", "Januari"), ("Feb.", "Febiweri"), ("Maa.", "Maach"), ("Apr.", "April"),
                   ("May", "May"), ("Juun", "Juun"), ("July", "July"), ("Aag.", "Aagus"),
                   ("Sep.", "Septemba"), ("Akt.", "Aktoba"), ("Nov.", "Novemba"), ("Dis.", "Disemba")),
        "weekdays": (("Mondeh", "Mon."), ("Tuesdeh", "Tue."), ("Wenzdeh", "Wen."), ("Turzdeh", "Tur."),
                     ("Frideh", "Fri."), ("Satdeh", "Sat."), ("Sundeh", "Sun.")),
    },
    # Ladino, not the unrelated Ladin (lld) locale. Latin-script vocabulary:
    # https://kantoniko.com/listas/dias-de-la-semana
    # https://sefaradies.cl/wp-content/uploads/2021/01/Diccionario_Online_Ladino_Espa%C3%B1ol.pdf
    "lad": {
        "english_name": "Ladino", "native_name": "Djudeo-espanyol",
        "date_pattern": "d 'de' MMMM 'de' y",
        "months": (("jen.", "jenero"), ("fev.", "fevrero"), ("mar.", "marso"), ("avr.", "avril"),
                   ("may.", "mayo"), ("djun.", "djunyo"), ("djul.", "djulyo"), ("ago.", "agosto"),
                   ("sep.", "septembre"), ("okt.", "oktovre"), ("nov.", "novembre"), ("des.", "desembre")),
        "weekdays": (("lunes", "lun."), ("martes", "mar."), ("mierkoles", "mie."), ("djueves", "dju."),
                     ("viernes", "vie."), ("shabat", "sha."), ("alhad", "alh.")),
    },
    # Pennsylvania Dutch, not standard German or Dutch. Spellings vary by community;
    # use one consistent set and explicit application abbreviations.
    # https://padutch101.com/services-and-resources/pa-dutch-learning-resources/
    "pdc": {
        "english_name": "Pennsylvania Dutch (Amish)", "native_name": "Pennsilfaanisch Deitsch",
        "date_pattern": "d'.' MMMM y",
        "months": (("Yen.", "Yenner"), ("Han.", "Hanning"), ("Matz", "Matz"), ("Abr.", "Abrill"),
                   ("Moi", "Moi"), ("Tschun.", "Tschunn"), ("Tschul.", "Tschulei"), ("Aag.", "Aaguscht"),
                   ("Sep.", "September"), ("Okt.", "Oktower"), ("Nof.", "Nofember"), ("Die.", "Diesember")),
        "weekdays": (("Mundaag", "Mu."), ("Dinschdaag", "Di."), ("Mittwoch", "Mi."), ("Dunnerschdaag", "Du."),
                     ("Freidaag", "Fr."), ("Samschdaag", "Sa."), ("Sunndaag", "Su.")),
    },
}
# Omit redundant day wording and the era suffix from Latin calendar labels.
CALENDAR_DATE_PATTERNS = {"la": "d MMMM y"}
CALENDAR_WEEKDAY_PREFIXES = {"la": "dies "}
# Long French dates use an ordinal only on the first day of the month.
# https://nos-langues.canada.ca/fr/cles-de-la-redaction/date-regles-decriture
CALENDAR_FIRST_DAY_SUFFIXES = {"fr": "ᵉʳ"}


def generate() -> str:
    import babel
    from babel import Locale, UnknownLocaleError, localedata
    from babel.core import get_global
    from babel.dates import tokenize_pattern

    if babel.__version__ != "2.17.0":
        raise RuntimeError("Regeneration requires Babel 2.17.0 with CLDR 46.")
    countries = sorted(set(re.findall(
        r"\('([A-Z]{2})', 'country'",
        (ROOT / "packaging/jurisdiction-db/mars_country_jurisdictions.sql").read_text(),
    )))
    languages = get_global("territory_languages")
    locales = {"en_GB": "en"}
    selected = {}
    for country in countries:
        try:
            locale = Locale.parse("und_" + country)
        except UnknownLocaleError:
            # A territory's likely language may lack CLDR date data. Prefer an
            # available official language, then a population-ranked alternative.
            candidates = sorted(languages.get(country, {}).items(), key=lambda item: (
                item[1].get("official_status") != "official",
                -item[1].get("population_percent", 0), item[0],
            ))
            locale = None
            for language, _ in candidates:
                for identifier in (language + "_" + country, language):
                    try:
                        locale = Locale.parse(identifier)
                        break
                    except (UnknownLocaleError, ValueError):
                        pass
                if locale is not None:
                    break
            if locale is None:
                locale = Locale.parse("en_GB")  # Uninhabited territories without language data.
        selected[country] = str(locale)
        locales[str(locale)] = locale.language

    # Explicit subdivision overrides precede territory defaults in calendar_local.
    selected["CA-QC"] = "fr_CA"
    locales["fr_CA"] = "fr"

    # Keep language availability separate from shared calendar-name sets. CLDR
    # distinguishes national, de facto and regional official status; preserve it.
    language_names, choices = {}, []
    english = Locale.parse("en")
    for territory in dict.fromkeys((*selected, *SUPPLEMENTAL_LANGUAGE_OPTIONS)):
        # A regional addition inherits its country's options unless an explicit
        # subdivision catalogue already exists, as for Quebec.
        source = territory if territory in selected else territory.split("-", 1)[0]
        official = {language: entry["official_status"]
                    for language, entry in languages.get(source, {}).items()
                    if entry.get("official_status")}
        if territory == "CA-QC":
            official = {"fr": "official"}
        if territory == "VA":
            official["la"] = "holy_see"
        if not official:
            official = {locales[selected[source]]: "default"}
        supplements = {**SUPPLEMENTAL_LANGUAGE_OPTIONS.get(source, {}),
                       **SUPPLEMENTAL_LANGUAGE_OPTIONS.get(territory, {})}
        for language, status in supplements.items():
            official.setdefault(language, status)
        for language, status in sorted(official.items()):
            if language in SUPPLEMENTAL_LOCALES:
                data = SUPPLEMENTAL_LOCALES[language]
                locales[language] = language
                language_names[language] = (data["english_name"], data["native_name"])
                choices.append((territory, language, language, status))
                continue
            locale = None
            for identifier in (language + "_" + territory[:2], language):
                try:
                    locale = Locale.parse(identifier)
                    break
                except (UnknownLocaleError, ValueError):
                    pass
            if locale is not None:
                locales[str(locale)] = locale.language
                label = locale.get_language_name("en") or english.languages.get(language, language)
                native = locale.get_language_name() or label
            else:
                label = english.languages.get(language, language)
                native = label
            language_names[language] = (label, native)
            choices.append((territory, language, str(locale) if locale else None, status))

    def quoted(value):
        return "'" + value.replace("'", "''") + "'"

    @lru_cache(maxsize=None)
    def date_names(identifier):
        # Babel resolves inherited aliases lazily, mutating shared dictionaries.
        # Resolve an isolated copy so one locale cannot supply another's labels.
        return localedata.LocaleDataDict(deepcopy(localedata.load(identifier)))

    @lru_cache(maxsize=None)
    def month_labels(identifier, context):
        if identifier in SUPPLEMENTAL_LOCALES:
            return SUPPLEMENTAL_LOCALES[identifier]["months"]
        names = date_names(identifier)["months"][context]
        short, full = names["abbreviated"], names["wide"]
        return tuple((short[month], full[month]) for month in range(1, 13))

    @lru_cache(maxsize=None)
    def weekday_labels(identifier):
        if identifier in SUPPLEMENTAL_LOCALES:
            return SUPPLEMENTAL_LOCALES[identifier]["weekdays"]
        names = date_names(identifier)["days"]["format"]
        prefix = CALENDAR_WEEKDAY_PREFIXES.get(identifier.split("_", 1)[0], "")
        return tuple((names["wide"][day].removeprefix(prefix), names["abbreviated"][day]) for day in range(7))

    def shared_sets(labels, contexts=("",)):
        owners, sets, aliases = {}, {}, {}
        for identifier, language in sorted(locales.items()):
            for context in contexts:
                values = labels(identifier, context) if context else labels(identifier)
                base = labels(language, context) if context else labels(language)
                # Share identical formatting and standalone sets too, while keeping
                # grammatical differences (such as Latin Iunii/Iunius) intact.
                candidate = language if values == base else identifier
                if context == "stand-alone":
                    candidate += "_standalone"
                owner = owners.setdefault(values, candidate)
                sets.setdefault(owner, values)
                aliases[identifier, context] = owner
        return sets, aliases

    month_sets, month_aliases = shared_sets(month_labels, ("format", "stand-alone"))
    weekday_sets, weekday_aliases = shared_sets(weekday_labels)
    patterns, pattern_aliases = {}, {}
    fields = {("y", 1): "year", ("M", 1): "month", ("M", 3): "month_short", ("M", 4): "month_full",
              ("d", 1): "day", ("d", 2): "day_padded"}
    for identifier in sorted(locales):
        data = None if identifier in SUPPLEMENTAL_LOCALES else date_names(identifier)
        pattern = SUPPLEMENTAL_LOCALES[identifier]["date_pattern"] if data is None else str(data["date_formats"]["long"])
        parts = []
        display_pattern = CALENDAR_DATE_PATTERNS.get(locales[identifier], pattern)
        for kind, value in tokenize_pattern(display_pattern):
            if kind == "chars":
                parts.append(("literal", value))
            elif value == ("G", 1):
                # The installed calendar contains modern Gregorian CE dates.
                parts.append(("literal", data["eras"]["abbreviated"][1]))
            elif value in fields:
                parts.append((fields[value], ""))
                suffix = CALENDAR_FIRST_DAY_SUFFIXES.get(locales[identifier])
                if suffix and fields[value] in ("day", "day_padded"):
                    parts.append(("first_day_suffix", suffix))
            else:
                raise ValueError(f"Unsupported long-date field {value!r} for {identifier}: {pattern}")
        pattern_id = patterns.setdefault(tuple(parts), len(patterns) + 1)
        pattern_aliases[identifier] = (pattern_id, pattern)
    lines = [
        "-- Generated by tools/generate_calendar_locales.py; do not edit by hand.",
        "-- Babel 2.17.0; Unicode CLDR 46. https://github.com/unicode-org/cldr/tree/release-46",
        "-- Copyright © 1991-2024 Unicode, Inc. Unicode Licence v3; see THIRD_PARTY_NOTICES.md.",
        "-- Territory defaults with explicit subdivision overrides; not town-level language detection.",
        "-- Breton and Cornish are explicit regional options supplementing CLDR official-language choices.",
        "-- Cajun French, Iñupiaq, Jamaican Patois, Ladino and Pennsylvania Dutch are supplements; Yiddish uses CLDR.",
        "-- Identical month and weekday sets are stored once; locale mappings preserve regional differences.",
        "create table if not exists calendar_territory_locale (",
        "    territory text primary key, locale text not null",
        ");",
        "create table if not exists calendar_language_name (",
        "    language text primary key, english_name text not null, native_name text not null",
        ");",
        "create table if not exists calendar_jurisdiction_language (",
        "    jurisdiction text not null, language text not null, locale text, status text not null,",
        "    primary key (jurisdiction, language)",
        ");",
        "create table if not exists calendar_town_language (",
        "    country text not null, town_key text not null, language text not null, locale text not null,",
        "    status text not null, primary key (country, town_key, language)",
        ");",
        "create table if not exists calendar_month_names (",
        "    name_set text not null, month_no integer not null check (month_no between 1 and 12),",
        "    short_name text not null, full_name text not null, primary key (name_set, month_no)",
        ");",
        "create table if not exists calendar_weekday_names (",
        "    name_set text not null, weekday_no integer not null check (weekday_no between 0 and 6),",
        "    day_name text not null, short_name text not null, primary key (name_set, weekday_no)",
        ");",
        "create table if not exists calendar_locale_name_set (",
        "    locale text primary key, month_set text not null, weekday_set text not null,",
        "    standalone_month_set text not null",
        ");",
        "create table if not exists calendar_locale_date_pattern (",
        "    locale text primary key, pattern_id integer not null, cldr_pattern text not null",
        ");",
        "create table if not exists calendar_date_pattern_part (",
        "    pattern_id integer not null, position integer not null, field text not null, literal text not null,",
        "    primary key (pattern_id, position)",
        ");",
        "create view if not exists calendar_locale_month as",
        "select l.locale, n.month_no, n.short_name, n.full_name",
        "from calendar_locale_name_set as l join calendar_month_names as n on n.name_set = l.month_set;",
        "create view if not exists calendar_locale_month_standalone as",
        "select l.locale, n.month_no, n.short_name, n.full_name",
        "from calendar_locale_name_set as l join calendar_month_names as n on n.name_set = l.standalone_month_set;",
        "create view if not exists calendar_locale_weekday as",
        "select l.locale, n.weekday_no, n.day_name, n.short_name",
        "from calendar_locale_name_set as l join calendar_weekday_names as n on n.name_set = l.weekday_set;",
    ]

    def insert(table, rows):
        lines.append("insert or replace into " + table + " values")
        lines.extend("    (" + ", ".join("null" if value is None else quoted(value) if isinstance(value, str) else str(value)
                                        for value in row) + ")" + ("," if index + 1 < len(rows) else ";")
                     for index, row in enumerate(rows))

    insert("calendar_territory_locale", list(selected.items()))
    insert("calendar_language_name", [(language, *labels) for language, labels in sorted(language_names.items())])
    insert("calendar_jurisdiction_language", choices)
    from jurisdiction_calendar import location_key
    insert("calendar_town_language", [(country, location_key(town), language, language, "additional")
           for country, languages in sorted(SUPPLEMENTAL_TOWN_LANGUAGES.items())
           for language, towns in sorted(languages.items()) for town in towns])
    insert("calendar_locale_name_set",
           [(identifier, month_aliases[identifier, "format"], weekday_aliases[identifier, ""],
             month_aliases[identifier, "stand-alone"]) for identifier in sorted(locales)])
    insert("calendar_locale_date_pattern", [(identifier, *value) for identifier, value in pattern_aliases.items()])
    insert("calendar_date_pattern_part", [(pattern_id, position, *part)
           for parts, pattern_id in patterns.items() for position, part in enumerate(parts)])
    insert("calendar_month_names",
           [(owner, month, *names) for owner, values in sorted(month_sets.items())
            for month, names in enumerate(values, 1)])
    insert("calendar_weekday_names",
           [(owner, day, *names) for owner, values in sorted(weekday_sets.items())
            for day, names in enumerate(values)])
    return "\n".join(lines) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true", help="Verify the checked-in seed.")
    parser.add_argument("--patch", action="store_true", help="Emit an apply_patch addition for the generated seed.")
    args = parser.parse_args()
    text = generate()
    if args.check:
        if (ROOT / OUTPUT).read_text() != text:
            raise SystemExit("Calendar locale seed differs from the generator output.")
        print("Calendar locale seed matches Babel 2.17.0 / CLDR 46 and explicit supplements.")
    elif args.patch:
        print("*** Begin Patch\n*** Add File: " + str(OUTPUT))
        print("".join("+" + line + "\n" for line in text.splitlines()), end="")
        print("*** End Patch")
    else:
        print(text, end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
