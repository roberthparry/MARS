"""Location selection and native policy snapshots for the SQL calendar view."""

from __future__ import annotations

from collections import defaultdict
from contextlib import contextmanager
import ctypes
from dataclasses import dataclass
import datetime as dt
import difflib
import json
import os
from pathlib import Path
import re
import subprocess
import unicodedata
from zoneinfo import ZoneInfo


ROOT = Path(__file__).resolve().parents[1]
LOCATION_ENV = "MARS_CALENDAR_LOCATION"
LOCATION_ALIASES = {"quebec city": "quebec"}


def location_key(text: str) -> str:
    """Ignore case, accents and repeated spaces without changing stored names."""
    decomposed = unicodedata.normalize("NFKD", text.casefold())
    return " ".join("".join(char for char in decomposed if not unicodedata.combining(char)).split())


def sql_quote(value: str) -> str:
    return "'" + value.replace("'", "''") + "'"


def run_sql(database: Path, key: str, sql: str, executable: str = "sqlcipher") -> list[dict]:
    """Execute SQL without putting the encryption key on the command line."""
    result = subprocess.run(
        [executable, str(database)], cwd=ROOT,
        input=(".bail on\n.output /dev/null\n"
               f"pragma key = {sql_quote(key)};\n.output stdout\n.mode json\n{sql}\n"),
        text=True, capture_output=True, timeout=120,
    )
    if result.returncode:
        # A SQLCipher diagnostic can echo a failed statement containing the key.
        raise RuntimeError((result.stderr or "SQLCipher failed").replace(key, "<redacted>"))
    return json.loads(result.stdout) if result.stdout.strip() else []


@dataclass(frozen=True)
class Location:
    name: str
    jurisdiction: str
    latitude: float
    longitude: float
    timezone: str

    @property
    def label(self) -> str:
        return f"{self.name}, {self.jurisdiction}"


class LocationCatalogue:
    def __init__(self, database: Path, key: str, executable: str = "sqlcipher"):
        rows = run_sql(database, key, """
select t.town_name, t.jurisdiction_id, lat.latitude, lon.longitude, zone.timezone_name,
       default_town.jurisdiction_id as default_for
from jurisdiction_town as t
join jurisdiction_town_latitude as lat using (jurisdiction_town_id)
join jurisdiction_town_longitude as lon using (jurisdiction_town_id)
join jurisdiction_town_timezone as tz using (jurisdiction_town_id)
join timezone_code as zone using (timezone_code)
left join jurisdiction_default_town as default_town using (jurisdiction_town_id);
""", executable)
        # Index the finite installed catalogue once for case/accent-insensitive lookup.
        self.by_name: dict[str, list[Location]] = defaultdict(list)
        self.defaults: dict[str, Location] = {}
        for row in rows:
            location = Location(row["town_name"], row["jurisdiction_id"], float(row["latitude"]),
                                float(row["longitude"]), row["timezone_name"])
            self.by_name[location_key(location.name)].append(location)
            if row["default_for"]:
                self.defaults[row["default_for"]] = location

    def matches(self, text: str) -> list[Location]:
        name, separator, jurisdiction = text.strip().rpartition(",")
        if not separator or location_key(text) in self.by_name:
            name, jurisdiction = text.strip(), ""
        name = location_key(name)
        candidates = self.by_name.get(LOCATION_ALIASES.get(name, name), [])
        if jurisdiction:
            candidates = [place for place in candidates if place.jurisdiction == jurisdiction.strip().upper()]
        else:
            # Country and subdivision entries can describe the same physical town.
            candidates = [place for place in candidates if not any(
                other.jurisdiction.startswith(place.jurisdiction + "-")
                and (place.latitude, place.longitude, place.timezone)
                == (other.latitude, other.longitude, other.timezone)
                for other in candidates
            )]
        return sorted(set(candidates), key=lambda place: (location_key(place.name), place.jurisdiction))

    def search(self, text: str) -> list[Location]:
        """Search the finite town index only on explicit menu searches or failed lookup."""
        exact = self.matches(text)
        if exact:
            return exact
        name, separator, jurisdiction = text.strip().rpartition(",")
        if not separator:
            name, jurisdiction = text, ""
        name = location_key(name)
        # A bounded catalogue scan is appropriate for substring/fuzzy discovery;
        # normal town resolution above remains an indexed lookup.
        keys = [key for key in self.by_name if name in key]
        if not keys and name:
            keys = difflib.get_close_matches(name, self.by_name, n=6, cutoff=0.6)
        suffix = ", " + jurisdiction if jurisdiction else ""
        places = {place for key in keys for place in self.matches(key + suffix)}
        return sorted(places, key=lambda place: (location_key(place.name), place.jurisdiction))

    def resolve(self, text: str) -> Location:
        candidates = self.matches(text)
        if not candidates:
            suggestions = self.search(text)[:6]
            hint = " Did you mean: " + "; ".join(place.label for place in suggestions) + "?" if suggestions else ""
            raise ValueError(f"Unknown calendar location: {text!r}.{hint} "
                             "Run make install-jurisdiction-db in a terminal to choose from the town menu, "
                             "or supply LOCATION='town, jurisdiction'.")
        if len(candidates) != 1:
            choices = "; ".join(place.label for place in candidates)
            raise ValueError(f"Ambiguous calendar location: {text!r}. Specify a jurisdiction: {choices}")
        location = candidates[0]
        ZoneInfo(location.timezone)  # Fail before replacing the existing database if tzdata is missing.
        return location


def locale_jurisdiction(environ: dict | None = None) -> str:
    environ = os.environ if environ is None else environ
    explicit = environ.get("MARS_HOLIDAY_JURISDICTION", "").strip().upper()
    if explicit:
        return "GB-ENG" if explicit == "GB" else explicit
    for variable in ("LC_ALL", "LC_MESSAGES", "LANG"):
        match = re.search(r"_([A-Za-z]{2})(?:[.@]|$)", environ.get(variable, ""))
        if match:
            country = match[1].upper()
            return "GB-ENG" if country == "GB" else country
    return ""


def choose_location(catalogue: LocationCatalogue, requested: str, saved: str, interactive: bool) -> Location:
    chosen = requested.strip() or saved.strip()
    if interactive:
        return location_menu(catalogue, chosen)
    if chosen:
        return catalogue.resolve(chosen)
    suggested = catalogue.defaults.get(locale_jurisdiction())
    if suggested:
        return suggested
    raise ValueError("No calendar location is configured. Supply a town after install-jurisdiction-db.")


def location_menu(catalogue: LocationCatalogue, chosen: str) -> Location:
    """Require an explicit selection; never silently accept a fuzzy match."""
    suggested = catalogue.defaults.get(locale_jurisdiction()) if not chosen else None
    if chosen:
        try:
            suggested = catalogue.resolve(chosen)
        except ValueError as error:
            print(error)
    choices = catalogue.search(chosen)
    page, page_size = 0, 12
    print("\nCalendar location — select a town, not your computer's language.")
    while True:
        pages = max(1, (len(choices) + page_size - 1) // page_size)
        visible = choices[page * page_size:(page + 1) * page_size]
        print(f"\nTowns — page {page + 1}/{pages} ({len(choices)} matches)")
        if suggested:
            print(f"  0. Use {suggested.label} [Enter]")
        for number, place in enumerate(visible, 1):
            print(f"  {number}. {place.label} ({place.timezone})")
        if not visible:
            print("  No matching towns. Try a shorter name, or / to browse all towns.")
        print("Number = select; type a town to filter; / = all towns; n/p = next/previous; q = cancel.")
        try:
            answer = input("Choice: ").strip()
        except (EOFError, KeyboardInterrupt):
            raise ValueError("Calendar location selection cancelled; existing database unchanged.") from None
        if answer.lower() == "q":
            raise ValueError("Calendar location selection cancelled; existing database unchanged.")
        if suggested and answer in ("", "0"):
            ZoneInfo(suggested.timezone)
            return suggested
        if answer.isascii() and answer.isdecimal() and len(answer) <= 3:
            number = int(answer)
            if 1 <= number <= len(visible):
                location = visible[number - 1]
                ZoneInfo(location.timezone)
                return location
            print("Choose one of the numbers shown.")
        elif answer.lower() in ("n", "p"):
            page = min(pages - 1, page + 1) if answer.lower() == "n" else max(0, page - 1)
        elif answer:
            choices = catalogue.search("" if answer == "/" else answer)
            page = 0
        else:
            print("Choose a town number or type a name to filter.")


@contextmanager
def database_environment(database: Path, key: str):
    values = {"MARS_JURISDICTION_DB_PATH": str(database), "MARS_JURISDICTION_DB_KEY": key}
    previous = {name: os.environ.get(name) for name in values}
    os.environ.update(values)
    try:
        yield
    finally:
        for name, value in previous.items():
            if value is None:
                os.environ.pop(name, None)
            else:
                os.environ[name] = value


class HolidayEvent(ctypes.Structure):
    _fields_ = [
        ("holiday_id", ctypes.c_int), ("rule_id", ctypes.c_int), ("event_year", ctypes.c_int),
        ("holiday_date", ctypes.c_void_p), ("holiday_name", ctypes.c_char_p),
        ("holiday_class", ctypes.c_char_p), ("derived_from_observance", ctypes.c_bool),
    ]


VISITOR = ctypes.CFUNCTYPE(ctypes.c_bool, ctypes.POINTER(HolidayEvent), ctypes.c_void_p)


def holiday_labels(database: Path, key: str, jurisdiction: str, locale: str,
                   executable: str = "sqlcipher") -> dict[tuple[int, str], str]:
    """Index translated definition names, retaining unmatched event-specific names."""
    if not locale:
        return {}
    locale = locale.replace("-", "_").lower()
    language = locale.split("_", 1)[0]
    rows = run_sql(database, key, f"""
with recursive lineage(jurisdiction_id) as (
    select {sql_quote(jurisdiction)}
    union
    select parent.parent_jurisdiction_id
    from jurisdiction_parent_jurisdiction_id as parent
    join lineage on lineage.jurisdiction_id = parent.jurisdiction_id
)
select holiday.holiday_id, holiday.default_name, original.localized_name as source_name, names.localized_name
from lineage
join holiday_definition as holiday using (jurisdiction_id)
join holiday_name as names using (holiday_id)
left join holiday_name as original on original.holiday_id = holiday.holiday_id and original.is_primary = 'Y'
where lower(replace(names.locale, '-', '_')) in ({sql_quote(locale)}, {sql_quote(language)})
order by case when lower(replace(names.locale, '-', '_')) = {sql_quote(locale)} then 0 else 1 end,
         names.is_primary desc, names.holiday_name_id;
""", executable)
    labels = {}
    for row in rows:
        for original in (row["default_name"], row["source_name"]):
            if original and row["localized_name"]:
                labels.setdefault((row["holiday_id"], original), row["localized_name"])
    qualifiers = run_sql(database, key, f"""
select source_suffix, localized_suffix from holiday_name_qualifier
where lower(replace(locale, '-', '_')) in ({sql_quote(locale)}, {sql_quote(language)})
order by case when lower(replace(locale, '-', '_')) = {sql_quote(locale)} then 0 else 1 end, source_suffix;
""", executable)
    suffixes = {}
    for row in qualifiers:
        suffixes.setdefault(row["source_suffix"], row["localized_suffix"])
    # Expand the small qualifier catalogue once; event lookup remains indexed.
    # Never discard or guess an unknown event-specific qualification.
    for (holiday_id, original), translated in tuple(labels.items()):
        for source_suffix, localized_suffix in suffixes.items():
            labels.setdefault((holiday_id, original + source_suffix), translated + localized_suffix)
    # Named substitute days and proclamations need exact translations, not a
    # guessed replacement of their base holiday name. Exact locale wins.
    events = run_sql(database, key, f"""
with recursive lineage(jurisdiction_id) as (
    select {sql_quote(jurisdiction)}
    union
    select parent.parent_jurisdiction_id from jurisdiction_parent_jurisdiction_id as parent
    join lineage on lineage.jurisdiction_id = parent.jurisdiction_id
)
select names.holiday_id, names.source_name, names.localized_name
from lineage join holiday_definition using (jurisdiction_id)
join holiday_event_localized_name as names using (holiday_id)
where lower(replace(names.locale, '-', '_')) in ({sql_quote(locale)}, {sql_quote(language)})
order by case when lower(replace(names.locale, '-', '_')) = {sql_quote(locale)} then 1 else 0 end;
""", executable)
    for row in events:
        labels[row["holiday_id"], row["source_name"]] = row["localized_name"]
    return labels


def native_policy(database: Path, key: str, jurisdiction: str, first_year: int, last_year: int,
                  locale: str = "", executable: str = "sqlcipher"):
    """Expand public MARS holiday and weekend APIs once during installation."""
    library = Path(os.environ.get("MARS_CALENDAR_LIBRARY", ROOT / "build/release/libmars.so")).resolve()
    if not library.is_file():
        raise RuntimeError("Build libmars before installing the local calendar (make install-jurisdiction-db does this).")
    api = ctypes.CDLL(str(library))
    pointer = ctypes.c_void_p
    signatures = {
        "jurisdict_open": ([ctypes.c_char_p], pointer),
        "jurisdict_close": ([pointer], None),
        "jurisdict_last_error": ([pointer], ctypes.c_char_p),
        "jurisdict_each_holiday_between": ([pointer, pointer, pointer, VISITOR, pointer], ctypes.c_bool),
        "jurisdict_is_weekend": ([pointer, pointer], ctypes.c_bool),
        "datetime_from_string": ([ctypes.c_char_p], pointer),
        "datetime_dealloc": ([pointer], None),
        "datetime_year": ([pointer], ctypes.c_short),
        "datetime_month": ([pointer], ctypes.c_int),
        "datetime_day": ([pointer], ctypes.c_uint8),
    }
    for name, (arguments, result) in signatures.items():
        function = getattr(api, name)
        function.argtypes, function.restype = arguments, result

    def date_pointer(date: dt.date):
        result = api.datetime_from_string(date.isoformat().encode())
        if not result:
            raise RuntimeError(f"Cannot construct calendar date {date}")
        return result

    holidays: dict[str, set[str]] = defaultdict(set)
    weekends: dict[tuple[int, int], bool] = {}
    errors = []
    labels = holiday_labels(database, key, jurisdiction, locale, executable)

    @VISITOR
    def collect(event_pointer, context):
        try:
            event = event_pointer.contents
            date = event.holiday_date
            text = dt.date(api.datetime_year(date), api.datetime_month(date), api.datetime_day(date)).isoformat()
            name = event.holiday_name.decode("utf-8") if event.holiday_name else "Holiday"
            holidays[text].add(labels.get((event.holiday_id, name), name))
            return True
        except Exception as error:
            errors.append(error)
            return False

    with database_environment(database, key):
        engine = api.jurisdict_open(jurisdiction.encode())
        if not engine:
            raise RuntimeError(f"Cannot open calendar jurisdiction {jurisdiction}")
        start = end = None
        try:
            start = date_pointer(dt.date(first_year, 1, 1))
            end = date_pointer(dt.date(last_year, 12, 31))
            if not api.jurisdict_each_holiday_between(engine, start, end, collect, None) or errors:
                message = api.jurisdict_last_error(engine)
                raise RuntimeError(message.decode() if message else f"Failed to expand {jurisdiction} holidays")
            # Native weekend rules are year-based. Seven calls cover every weekday.
            for year in range(first_year, last_year + 1):
                for day in range(1, 8):
                    date = dt.date(year, 1, day)
                    value = date_pointer(date)
                    try:
                        weekends[year, date.weekday()] = api.jurisdict_is_weekend(engine, value)
                        error = api.jurisdict_last_error(engine)
                        if error:
                            raise RuntimeError(error.decode())
                    finally:
                        api.datetime_dealloc(value)
        finally:
            api.datetime_dealloc(start)
            api.datetime_dealloc(end)
            api.jurisdict_close(engine)
    return holidays, weekends


def populate_calendar(database: Path, key: str, location: Location, executable: str = "sqlcipher",
                      locale: str = "") -> None:
    zone = ZoneInfo(location.timezone)
    first_year, last_year = 2015, dt.datetime.now(zone).year + 7
    holidays, weekends = native_policy(database, key, location.jurisdiction, first_year, last_year, locale, executable)
    rows = []
    date = dt.date(first_year, 1, 1)
    last = dt.date(last_year, 12, 31)
    while date <= last:
        date_text = date.isoformat()
        names = holidays.get(date_text)
        holiday = sql_quote("; ".join(sorted(names))) if names else "null"
        offset = dt.datetime.combine(date, dt.time(12), zone).utcoffset().total_seconds() / 3600
        rows.append(f"({sql_quote(date_text)}, {holiday}, {int(weekends[date.year, date.weekday()])}, {offset})")
        date += dt.timedelta(days=1)
    settings = (
        f"(1, {sql_quote(location.name)}, {sql_quote(location.jurisdiction)}, {sql_quote(location.timezone)}, "
        f"{location.latitude}, {location.longitude}, {first_year}, {last_year}, "
        f"{sql_quote(locale) if locale else 'null'})"
    )
    validation = run_sql(database, key,
            "begin; delete from calendar_local_days; delete from calendar_local_settings;\n"
            "insert into calendar_local_settings values " + settings + ";\n"
            "insert into calendar_local_days values\n" + ",\n".join(rows) + ";\ncommit;\n"
            "select count(*) as row_count, count(distinct FullDateAlternateKey) as date_count, "
            "count([Date UK]) as uk_count, count([Date Lingua]) as lingua_count, "
            "count([Date Regional]) as regional_count from calendar_local;", executable)
    if not validation or any(count != len(rows) for count in validation[0].values()):
        raise RuntimeError(f"Incomplete calendar for {location.label}: expected {len(rows)} dated rows. "
                           "Check the calendar locale mappings; the existing database has not been replaced.")
