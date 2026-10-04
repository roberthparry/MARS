"""Database-backed calendar language choices and installer menu."""

from pathlib import Path

from jurisdiction_calendar import location_key, run_sql, sql_quote


LANGUAGE_ENV = "MARS_CALENDAR_LANGUAGE"
LANGUAGE_ALIASES = {
    "frc": ("Cajun", "Louisiana French"),
    "ik": ("Iñupiaq", "Inupiaq", "ipk", "Alaskan Inuit"),
    "jam": ("Jamaican Patwa", "Jamaican Patwah", "Jamaican Creole", "Jamaican English Patois", "Patwah", "Patois"),
    "pdc": ("Amish", "Pennsylvania Dutch", "Pennsylvania German", "Deitsch"),
}
LOCATION_LANGUAGE_EXCLUSIONS = {
    ("PS", "Asia/Hebron"): frozenset(("he", "yi", "lad")),
    ("PS", "Asia/Jerusalem"): frozenset(("he", "yi", "lad")),
}
STATUS_LABELS = {
    "official": "official",
    "de_facto_official": "de facto official",
    "official_regional": "regional official",
    "regional": "regional language",
    "additional": "additional language",
    "holy_see": "Holy See",
    "default": "calendar default",
}


def choose_language(database: Path, key: str, jurisdiction: str, requested: str = "",
                    saved: str = "", interactive: bool = False, executable: str = "sqlcipher",
                    *, timezone: str = "", town: str = "") -> str:
    """Select a supported language, never silently substitute for an explicit choice."""
    jurisdiction_sql = sql_quote(jurisdiction)
    rows = run_sql(database, key, f"""
with town_choices as (
    select language, locale, status from calendar_town_language
    where country = substr({jurisdiction_sql}, 1, 2) and town_key = {sql_quote(location_key(town))}
), choices as (
    select language, locale, status from calendar_jurisdiction_language
    where jurisdiction = case
        when exists (select 1 from calendar_jurisdiction_language where jurisdiction = {jurisdiction_sql})
        then {jurisdiction_sql} else substr({jurisdiction_sql}, 1, 2) end
      and language not in (select language from town_choices)
    union all
    select language, locale, status from town_choices
)
select choices.*, names.english_name, names.native_name,
       coalesce(local.locale, country.locale) as default_locale
from choices
join calendar_language_name as names using (language)
join calendar_territory_locale as country on country.territory = substr({jurisdiction_sql}, 1, 2)
left join calendar_territory_locale as local on local.territory = {jurisdiction_sql}
order by names.english_name, choices.language;
""", executable)
    excluded = LOCATION_LANGUAGE_EXCLUSIONS.get((jurisdiction, timezone), ())
    rows = [row for row in rows if row["language"] not in excluded]
    if not rows:
        raise ValueError(f"No calendar language data for {jurisdiction}.")
    available = [row for row in rows if row["locale"]]
    if not available:
        raise ValueError(f"No supported calendar languages for {jurisdiction}.")
    # Each jurisdiction has only a small, bounded set of languages. Index all
    # accepted spellings once, and preserve collisions instead of guessing.
    aliases = {}
    for row in rows:
        for spelling in (row["language"], row["locale"], row["english_name"], row["native_name"],
                         *LANGUAGE_ALIASES.get(row["language"], ())):
            if spelling:
                bucket = aliases.setdefault(location_key(spelling.replace("-", "_")), [])
                if row not in bucket:
                    bucket.append(row)

    def match(value):
        matches = aliases.get(location_key(value.replace("-", "_")), [])
        return matches[0] if len(matches) == 1 else None

    default = next((row for row in available if row["locale"] == rows[0]["default_locale"]), available[0])
    previous = match(saved)
    if previous and previous["locale"]:
        default = previous
    invalid_request = False
    if requested:
        explicit = match(requested)
        if explicit and explicit["locale"]:
            default = explicit
        else:
            invalid_request = True
            labels = "; ".join(f"{row['english_name']} ({row['locale']})" for row in available)
            message = f"Calendar language {requested!r} is unavailable for {jurisdiction}. Choose: {labels}"
            if not interactive:
                raise ValueError(message)
            print(message)
    if not interactive:
        return default["locale"]
    if len(rows) == 1 and not invalid_request:
        print(f"Calendar language: {default['english_name']} ({default['native_name']}) [{default['locale']}]")
        return default["locale"]
    while True:
        print(f"\nCalendar language for {jurisdiction}")
        for number, row in enumerate(rows, 1):
            status = STATUS_LABELS.get(row["status"], row["status"])
            availability = row["locale"] or "calendar names unavailable"
            marker = " [Enter]" if row is default else ""
            print(f"  {number}. {row['english_name']} — {row['native_name']} ({status}; {availability}){marker}")
        try:
            answer = input("Language number or name [Enter = default; q = cancel]: ").strip()
        except (EOFError, KeyboardInterrupt):
            raise ValueError("Calendar language selection cancelled; existing database unchanged.") from None
        if answer.lower() == "q":
            raise ValueError("Calendar language selection cancelled; existing database unchanged.")
        selected = default if not answer else match(answer)
        if answer.isascii() and answer.isdecimal() and len(answer) <= 3:
            number = int(answer)
            selected = rows[number - 1] if 1 <= number <= len(rows) else None
        if selected and selected["locale"]:
            return selected["locale"]
        print("Choose a listed language with available calendar names.")
