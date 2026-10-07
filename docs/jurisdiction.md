# `jurisdiction_t`

`jurisdiction_t` provides jurisdiction-aware public holiday lookups on top of the
configured jurisdiction database installed with MARS Lab.

It answers questions such as:

- which holidays fall between two dates
- whether a date is a weekend in a given jurisdiction
- whether a date is a national holiday
- how many working days fall in a date range

## Configuration files

Database path and key resolution retains its existing precedence: modern
`MARS_JURISDICTION_DB_PATH` / `MARS_JURISDICTION_DB_KEY` environment values,
legacy `MARS_HOLIDAY_DB_PATH` / `MARS_HOLIDAY_DB_KEY` values, then configuration
files under `MARS_HOME/config` (or `~/.mars/config`).
The modern file is `jurisdiction-db.env`; the fallback is `holiday-db.env`.

These readers use `file_t` with explicit symbolic-link following, and parse
complete lines entirely as `string_t`. UTF-8 BOMs, LF, CRLF and CR endings
are supported; the old 4 KiB line limit is removed. A key must immediately
precede `=`, optionally after an `export ` prefix and leading whitespace.
Surrounding value whitespace and matching outer single or double quotes are
removed. The first non-empty matching value wins. Shell expansion is not
performed. Malformed UTF-8 or an I/O error before a match leaves the lookup
unresolved, allowing the existing fallback rules to apply.

## Scope

`jurisdiction_t` is the jurisdiction-policy layer that sits above `datetime_t`.

`datetime_t` remains responsible for reusable calendar calculations such as
Easter, Orthodox Easter, Chinese New Year, and other observance dates.
`jurisdiction_t` applies jurisdiction-specific rules such as:

- substitute-day behaviour
- weekend definitions that vary by country
- one-off exceptions such as funerals, coronations, or special observances
- historical rule changes over time

## Source Organisation

The implementation is split by responsibility within `src/jurisdiction/`:

- `jurisdiction.c` — engine configuration and lifetime, shared row storage,
  jurisdiction ancestry, default locations and serialisation.
- `jurisdiction_calendar.c` — civil-date conversion, inherited weekend policy
  and working-day queries.
- `jurisdiction_holiday_rules.c` — loading holiday, observance and exception
  rules, and evaluating calendar-based or SQL-based holiday dates.
- `jurisdiction_holidays.c` — assembling holiday occurrences, applying
  exceptions and substitute-day rules, and exposing visitor and array queries.
- `jurisdiction_timezone.c` — timezone eras, named rules and GMT offsets.
- `jurisdiction_dst.c` — daylight-saving transition collection and local
  transition times, including the offsets before and after each change.

One module-private header, `jurisdiction_internal.h`, groups the shared engine
state, calendar helpers, holiday rows and timezone declarations by responsibility.
Callers continue to use the unchanged public API in `include/jurisdiction.h`.

## Basic Usage

Open a jurisdiction engine for a jurisdiction:

```c
#include "jurisdiction.h"

jurisdiction_t *jurisdiction = jurisdict_open("GB-ENG");
```

When you are finished, release it with:

```c
jurisdict_close(jurisdiction);
```

## Installation

To provision the private jurisdiction database without installing the full desktop
app, run:

```sh
make install-jurisdiction-db
```

To remove that private jurisdiction database again, run:

```sh
make uninstall-jurisdiction-db
```

If you want the desktop app as well, use `make install-mars-lab`.

MARS supplies no shared WeatherAPI account or key. When the installer chooses
to create their own WeatherAPI account and configure weather lookups, that
account's key is stored separately in `~/.mars/config/weather.env` and is
preserved when the jurisdiction database is rebuilt. A lookup sends the key,
selected date and observer coordinates from the local MARS Lab server to
WeatherAPI.com over HTTPS; the key is not sent to the browser. MARS does not
cache or persist the returned weather response, although the date and
coordinates remain in private local Lab state so that its inputs can be
restored. Weather information is general and probabilistic; consult official
meteorological services and authorities for safety-critical decisions. See the
[MARS privacy notice](privacy.md), [WeatherAPI privacy policy](https://www.weatherapi.com/privacy.aspx)
and [WeatherAPI terms](https://www.weatherapi.com/terms.aspx).

## Local Calendar View

The jurisdiction database installer creates the read-only SQL view
`calendar_local`, defined in
`packaging/jurisdiction-db/mars_calendar_local.sql`. Its name stays the same
regardless of the selected town.

To select Limmen during installation:

```sh
make install-jurisdiction-db limmen
```

The final output line is:

```text
calendar_local location: Limmen, NL
```

The optional town is looked up in the packaged catalogue, without network
geolocation. Matching ignores case, accents and repeated spaces. An interactive
installation presents a numbered town menu: enter a number to select, type a
town name to filter, use `n`/`p` to change pages, `/` to browse all towns, or `q`
to cancel. Ambiguous names show their jurisdiction codes and time zones.
Misspellings offer suggestions, never an automatic replacement.

The supplied or saved location is offered as the menu default. Otherwise the
default comes from `MARS_HOLIDAY_JURISDICTION` or the system locale. Press Enter
to accept a displayed default. A non-interactive installation resolves the
supplied name, reuses the saved town, or uses the jurisdiction default in that
order; unknown names fail with suggestions and instructions. A locale-derived
town is a representative default, **not** detection of the machine's physical
position. Cancelling or failing selection leaves the existing database intact.

For names containing spaces or ambiguous names, use the Make variable
`LOCATION`, with a value of `town` or `town, jurisdiction`. Do not combine this
installation with other Make targets. The selected location is saved as
`MARS_CALENDAR_LOCATION` alongside the database configuration; an explicit town
takes precedence over the environment and saved choice.

`Quebec`, `Québec` and `Quebec City` select Québec City in `CA-QC`, with its
packaged jurisdiction rules, America/Toronto time and French calendar names by
default, with English also available in the language menu. Holiday names follow
the selected language; `[Date Regional]` remains Canadian French. Holiday
coverage is separate from town/language coverage: `CA-QC`
currently inherits Canada's packaged holidays, without Quebec-specific additions.
For example:

```sh
make install-jurisdiction-db Quebec
```

The location menu includes this choice (select `1`, or accept the default):

```text
  1. Québec, CA-QC (America/Toronto)
```

Accept French in the language menu. The final output is:

```text
calendar_local language: fr_CA
calendar_local location: Québec, CA-QC
```

With French selected:

```sql
select [Date Lingua], [Date Regional], [Public Holiday]
from calendar_local where FullDateAlternateKey = '2026-07-01';
```

```text
1ᵉʳ juillet 2026|1ᵉʳ juillet 2026|Fête du Canada
```

French dates use the superscript ordinal suffix only for the first day of the
month in both `[Date Lingua]` and `[Date Regional]`; other day numbers remain
cardinal. This supplements the plain numeric day in CLDR's date patterns,
following the [French date-writing guidance](https://nos-langues.canada.ca/fr/cles-de-la-redaction/date-regles-decriture).

After town selection, jurisdictions with several language choices have a
numbered language menu showing English and native names. Enter a number, a
language name or a locale code. Regional official languages are labelled as
regional; their availability is not a claim that they are used in every town.
Where CLDR lacks calendar names, the option is marked unavailable rather than
silently replaced with English. Quebec retains its French subdivision default
and offers English as an additional choice. Vatican City offers Italian and Latin; Latin is labelled **Holy See**
to distinguish that usage from Vatican City's Italian administration.
See the [OSCE Holy See profile](https://www.osce.org/files/f/documents/d/7/13602.pdf)
and [Unicode Latin calendar data](https://www.unicode.org/cldr/charts/47/summary/la.html).

Breton (`br`, `br_FR`, brezhoneg) and Cornish (`kw`, `kw_GB`, kernewek) are
additional choices labelled **regional language**. Breton is offered for French
jurisdictions; Cornish is offered through the British country fallback, including
`GB-ENG`. Select them in the language menu or with `CALENDAR_LANGUAGE=br` or
`CALENDAR_LANGUAGE=kw`. They do not replace the French or English defaults.
These choices are jurisdiction-wide: the installer does not yet restrict them
to towns in Brittany or Cornwall or infer a regional language from coordinates.

After selecting either language, this query shows the full and abbreviated names:

```sql
select [Date Lingua], [Month Name], [Month Name Abbrev], [Day Name], [Day Name Abbrev]
from calendar_local
where FullDateAlternateKey = '2024-06-21';
```

Breton output:

```text
21 Mezheven 2024|Mezheven|Mezh.|Gwener|Gwe.
```

Cornish output:

```text
21 mis Metheven 2024|mis Metheven|Met|dy Gwener|Gwe
```

The chosen locale is saved as `MARS_CALENDAR_LANGUAGE` and in
`calendar_local_settings.locale`. Reinstallation offers it again when it is
valid for the selected jurisdiction. `CALENDAR_LANGUAGE` supplies an explicit
Make choice, also available as the installer's `--language` argument. For example:

```sh
make install-jurisdiction-db LOCATION='Vatican City' CALENDAR_LANGUAGE=la
```

After accepting the displayed choices, the final output is:

```text
calendar_local language: la_VA
calendar_local location: Vatican City, VA
```

In that installed database:

```sql
select [Date Lingua], "Day Name", "Day Name Abbrev", "Month Name", "Month Name Abbrev", [Date UK]
from calendar_local
where FullDateAlternateKey = '2024-06-21';
```

Output:

```text
Date Lingua|Day Name|Day Name Abbrev|Month Name|Month Name Abbrev|Date UK
21 Iunii 2024|Veneris|Ven|Iunius|Iun|21ˢᵗ June 2024
```

Non-interactive installs use the explicit language, then a compatible saved
choice, then the jurisdiction default. An explicit unavailable language is an
error. Language selection changes month and weekday labels and uses available
holiday-name translations, without changing the town's holiday dates or working-day
rules. Holiday names use an exact locale match before a language-only match;
untranslated names and unmatched event-specific names retain their source text.
Translations are stored in `holiday_name` and applied when the calendar is installed;
reinstall after changing the language or updating the translation catalogue.
Known observed-day suffixes are translated through `holiday_name_qualifier`,
but only when the base holiday name also has a translation. Unknown qualifiers
retain the complete source name rather than losing their meaning. The packaged
catalogue includes Spanish names for US national holidays, inherited by their
subdivisions. Yiddish and Ladino options and holiday translations are also
available. These are additional language
choices, not assertions of official status or automatic detection of a town's
community languages. Territory defaults remain unchanged.
Yiddish uses CLDR calendar names in Hebrew script. Ladino uses a hand-maintained
Latin-script vocabulary and date pattern, with explicit short forms; it is not
the unrelated Ladin locale. Both use Gregorian dates in `Date Lingua`.
`Date UK` remains British English by design.

US language menus also offer Cajun French (`frc`) and Pennsylvania Dutch
(`pdc`), with Gregorian calendar names and translated national holiday labels.
The latter is displayed as **Pennsylvania Dutch (Amish)**; the installer accepts
`Amish`, `Pennsylvania Dutch`, `Pennsylvania German` and `Deitsch` as aliases.
`Cajun` and `Louisiana French` are aliases for Cajun French. These options are
inherited by US subdivisions, including Louisiana and Pennsylvania, without
changing the regional default or holiday policy. Choosing a language does not
select a religious observance calendar or imply that every Amish community
speaks the same language.

Neither locale has date data in CLDR 46. Their hand-maintained calendar labels
use [LSU's Cajun French vocabulary](https://www.lsu.edu/hss/french/undergraduate_program/cajun_french/cajun_french_by_themes.php)
and [PA Dutch language resources](https://padutch101.com/services-and-resources/pa-dutch-learning-resources/),
with explicit application abbreviations and date patterns. Holiday translations
are supplementary descriptive labels, not official names certified by the
language communities. Pennsylvania Dutch spelling varies by community; the
packaged data uses one consistent spelling set. Identical French calendar-name
sets remain shared rather than being duplicated.

Alaska (`US-AK`) additionally offers **Iñupiaq (North Slope)**, stored as `ik`.
The installer accepts `Iñupiaq`, `Inupiaq`, `Iñupiatun`, `ipk` and `Alaskan Inuit`;
it does not substitute Canadian Inuktitut or a Yup'ik language. The regional
menu retains the inherited US language choices and the English default.
The supplement uses the [Iñupiatun Tusaalaŋa glossary](https://inupiaq.tusaalanga.ca/glossary/english?showall=1)
and [Edna Ahgeak MacLean's month-name appendix](https://www.inupiaqonline.com/appendix/29).
Its short forms and month–day–year display pattern are application conventions.
`Date Lingua` remains a Gregorian date with Iñupiaq names, not a reconstruction
of the traditional seasonal calendar. Holiday dates, working-day rules and
`Date Regional` remain those of the selected Alaska jurisdiction. Attested
Iñupiaq names are supplied for Christmas and Thanksgiving; other holiday names
and unmatched observed-day labels retain their source text pending verified
translations. Language selection does not invent community festivals or their dates.

Jamaican Patois (`jam`, native label **Patwa**) is available for all Jamaican
towns. It is also an additional choice for the existing British catalogue
entries Birmingham, Bristol, Leeds, Leicester, London, Manchester and Nottingham.
The British list is curated, not a population threshold or an inference about
an individual user's language. The indexed `calendar_town_language` table
adds choices by country and normalised town name, so both country-level and
subdivision entries work without enabling the language for every British town.
English remains the default. Accepted names include `Patwa`, `Patwah`,
`Patois`, `Jamaican Creole` and `Jamaican English Patois`.

The supplement uses the English-based spellings in the
[Jamaican Patwah calendar vocabulary](https://jamaicanpatwah.com/b/talk-like-a-jamaican-how-to-say-the-weekdays-and-months);
it does not claim to implement the separate Cassidy–JLU spelling system.
Short forms and the day–month–year pattern are application conventions.
The British selection is informed by published community information, including
the [Greater London Authority's Caribbean population profile](https://data.london.gov.uk/download/f423a8de-798b-49da-8911-0f228d1eb22e/61fa18fc-9a09-4b71-bf88-effe0ebf11ad/DMAG%20briefing%202008-15%202001%20Census%20Profiles%20Black%20Caribbeans%20in%20London%20-%20low%20res.pdf),
[Leeds' Jamaica Society partnership](https://news.leeds.gov.uk/news/inspiring-photos-capture-communitys-park-life)
and [Leicester's community profile](https://cabinet.leicester.gov.uk/documents/s3263/Community%2520Cohesion%2520Document.pdf).
Patwa is not a substitute for other Caribbean languages.
Hand-maintained holiday labels cover Jamaica's ten packaged holiday definitions
and seven recurring English bank holidays. Untranslated special-event names
and observed-day qualifiers retain their source text. These labels are not
official holiday renamings or a claim of community certification.
Holiday dates, working-day counts, `Date UK` and `Date Regional` are unchanged.

For `Kingston, JM` with Patwa selected:

```sql
select FullDateAlternateKey, [Date Lingua], [Day Name], [Public Holiday]
from calendar_local
where FullDateAlternateKey = '2026-08-06';
```

Output:

```text
FullDateAlternateKey|Date Lingua|Day Name|Public Holiday
2026-08-06|6 Aagus 2026|Turzdeh|Independence Deh
```

`Date Regional` uses the jurisdiction's packaged default language and long
Gregorian date pattern, independently of the selected display language. A
subdivision override takes precedence over its territory default; for example,
Quebec uses Canadian French. It represents the regional default, rather than a
live registry of legal language status. Arabic regional dates use Arabic-Indic
digits and Gregorian month names; only Arabic `Date Lingua` uses Hijri dates.
Likewise, Hebrew regional dates remain Gregorian, whereas Hebrew `Date Lingua`
uses the Jewish calendar. Reinstall to add this column to an existing database.

For a New York City calendar installed with Spanish selected:

```sql
select FullDateAlternateKey, [Date Lingua], [Public Holiday]
from calendar_local
where FullDateAlternateKey in ('2026-01-01', '2026-07-04', '2026-12-25')
order by FullDateAlternateKey;
```

Output:

```text
FullDateAlternateKey|Date Lingua|Public Holiday
2026-01-01|1 de enero de 2026|Día de Año Nuevo
2026-07-04|4 de julio de 2026|Día de la Independencia
2026-12-25|25 de diciembre de 2026|Día de Navidad
```

The installer uses the town's coordinates and IANA time zone, and expands
MARS's native holiday and weekend rules for its jurisdiction. Limmen therefore
uses Dutch holidays and Europe/Amsterdam, whereas Shrewsbury uses
England-and-Wales holidays and Europe/London. A fully prepared temporary
database replaces the old database only after location resolution and calendar
generation succeed. Validation checks the full date range and all three
formatted date columns; an empty or incomplete view cannot replace the existing
database. Solar times may legitimately be absent during polar day or night.

Welsh towns use the shared England-and-Wales bank-holiday rules, including
substitute days and the seeded one-off exceptions. Selecting Welsh also
translates those holiday labels, including named substitute days, using the
[Welsh government calendar](https://www.gov.uk/gwyliau-banc). Political
jurisdiction ancestry remains unchanged.

After installing for Rhyl and choosing Welsh, the Christmas bank holidays are:

```sql
select FullDateAlternateKey, [Date Lingua], [Public Holiday]
from calendar_local
where FullDateAlternateKey between '2026-12-25' and '2026-12-28'
  and [Public Holiday] is not null
order by FullDateAlternateKey;
```

Output:

```text
FullDateAlternateKey|Date Lingua|Public Holiday
2026-12-25|25 Rhagfyr 2026|Dydd Nadolig
2026-12-28|28 Rhagfyr 2026|Dydd San Steffan (diwrnod amgen)
```

Selecting Irish translates the packaged UK and Irish holiday names, including
substitute days and named exceptions. Shared labels serve both Irish locales
(`ga_GB` and `ga_IE`); selecting a language never changes the jurisdiction's
holiday dates or working-day policy. Irish holiday vocabulary follows the
[Workplace Relations Commission's Irish guide](https://workplacerelations.ie/ga/eolas-rithabhachtach/saoiri-poibli/);
UK-only event labels are application translations. Unrecognised event-specific
names retain their source text rather than losing their qualification.

For Rhyl with Irish selected, the same Christmas query above produces:

```text
FullDateAlternateKey|Date Lingua|Public Holiday
2026-12-25|25 Nollaig 2026|Lá Nollag
2026-12-28|28 Nollaig 2026|Lá Fhéile Stiofáin (lá ionaid)
```

Western Frisian (`fy_NL`) also translates the Dutch holiday catalogue, including
historical Queen's Day names and moved royal holidays. Names follow
[Leeuwarden's Frisian terminology](https://www.leeuwarden.nl/fy-nl/fergunningen/winkeltijdenontheffing-aanvragen/).
`Date Lingua`, month/day names and `Public Holiday` use Frisian; `Date Regional`
remains Dutch, and holiday dates and working-day policy do not change.

For Limmen with Western Frisian selected:

```sql
select [Date Lingua], [Date Regional], [Public Holiday]
from calendar_local where FullDateAlternateKey = '2026-12-25';
```

Output:

```text
Date Lingua|Date Regional|Public Holiday
25 Desimber 2026|25 december 2026|Earste Krystdei
```

The view provides one row per date from 1 January 2015 through 31 December of
the installation year, in the selected time zone, plus seven. Its 36 columns
include civil dates, Monday-based weeks, month boundaries, April-to-March fiscal
years, jurisdiction holidays, working-day flags, sunrise, sunset, an approximate
Moon-phase percentage, `Date UK`, `Date Lingua` and `Date Regional`. The original `UK Holiday` column is now
called `Public Holiday`; it is `NULL` on dates without a jurisdiction holiday.
These are the jurisdiction engine's holidays, not a promise of a statutory day
off for every worker. `Day of Week` is Monday = 0 through Sunday = 6, and
`Workday Type` is 1 for a date which is neither a jurisdiction weekend nor a
holiday.

`Working Day No` is the inclusive running total of working days from the first
date in the installed calendar. It uses the same jurisdiction holiday and
weekend policy as `Workday Type`. The first working day is 1; any preceding
non-working days are 0. Weekends and holidays retain the previous count, and
the count never restarts at a calendar-year or fiscal-year boundary. Filtering
the view to a later date range does not restart it either.

For a Shrewsbury calendar beginning on 1 January 2015:

```sql
select FullDateAlternateKey, [Workday Type], [Working Day No]
from calendar_local
where FullDateAlternateKey between '2015-01-01' and '2015-01-05'
order by FullDateAlternateKey;
```

Output:

```text
FullDateAlternateKey|Workday Type|Working Day No
2015-01-01|0|0
2015-01-02|1|1
2015-01-03|0|1
2015-01-04|0|1
2015-01-05|1|2
```

`Fiscal Quarter Name` uses `yyyy Qn`, with the same fiscal year-end year as
`Fiscal Year End` and the same quarter as `Fiscal Quarter No`. The fiscal year
runs from April to March, so April–June 2024 belongs to fiscal year 2025,
quarter 1. For example:

```sql
select FullDateAlternateKey, [Fiscal Quarter Name]
from calendar_local
where FullDateAlternateKey in ('2024-03-31', '2024-04-01', '2024-07-01', '2025-01-01')
order by FullDateAlternateKey;
```

Output:

```text
FullDateAlternateKey|Fiscal Quarter Name
2024-03-31|2024 Q4
2024-04-01|2025 Q1
2024-07-01|2025 Q2
2025-01-01|2025 Q4
```

Month and weekday names are stored in the jurisdiction database, not hard-coded
in the view. `calendar_territory_locale` stores territory defaults and explicit
subdivision overrides. `calendar_jurisdiction_language` stores language options
and their status, with shared display names in `calendar_language_name`;
`calendar_month_names` stores shared sets of full and abbreviated month names,
and `calendar_weekday_names` stores shared Monday-indexed weekday sets.
`calendar_locale_name_set` maps each locale to these sets, so identical English,
French and other language data is stored once. Month and weekday sets are shared
independently; genuine regional differences, including abbreviations, are kept.
The views `calendar_locale_month`, `calendar_locale_month_standalone` and
`calendar_locale_weekday` resolve the mappings for callers. Formatting and
standalone month sets share the same stored rows wherever their names agree.
`calendar_local` exposes `Month Name` and `Day Name` as full names;
`Month Name Abbrev` and `Day Name Abbrev` are the abbreviated names. The
standalone full month name can differ grammatically from the name inside a date.
`ME Dates Text` and `WE Dates Text` retain their abbreviated month names.

Except for Arabic, Hebrew and the hand-maintained language supplements, `Date Lingua` uses the selected locale's CLDR **long** Gregorian date pattern,
including its field order, punctuation, connecting words and era label where
required. French adds a superscript first-day ordinal in both date columns;
Latin omits the era suffix and the introductory `die`. Latin weekday
names also omit `dies`; their abbreviations remain unchanged. Month names use
the date-context grammatical form. Patterns are
stored as shared ordered parts in `calendar_date_pattern_part`, selected through
`calendar_locale_date_pattern`; the view formats them using only built-in SQL.
The optional `first_day_suffix` part contributes its literal only when the
displayed day is the first of the month; identical patterns remain shared.
See [Unicode's date-pattern guidance](https://cldr.unicode.org/translation/date-time/date-time-patterns).
Arabic `Date Lingua` uses the civil Islamic (Hijri) calendar: Hijri day, Arabic
Hijri month name and AH year, all numbers written with Arabic-Indic digits
(`٠١٢٣٤٥٦٧٨٩`). It uses the tabular 30-year leap cycle and Friday epoch, matching
MARS's native civil Islamic conversion. It is not an Umm al-Qura or local
moon-sighting calendar. Because each row represents a civil date rather than
an instant, it does not change dates at local sunset.

Hebrew `Date Lingua` uses the fixed Jewish (Hebrew) calendar: Hebrew day, Hebrew
month name and Jewish year, with decimal day/year numbers. Leap years distinguish
Adar I (`אדר א׳`) and Adar II (`אדר ב׳`); common years use Adar (`אדר`).
The conversion includes the new-year postponement rules and variable Cheshvan
and Kislev lengths. As with Arabic output, a row identifies the calendar date
during the civil day; it does not switch at sunset. Reinstall the jurisdiction
database with Hebrew selected to update an existing installation's view.

`Date UK`, `Date Regional`, ISO date keys, numeric year/month/day fields, month-name columns,
fiscal fields and holiday policies remain Gregorian-based. Other languages
retain Western digits. The British superscript suffixes are not copied into
other languages. For UK towns with English selected, `[Date Lingua]` adds the
same superscript day suffix as `[Date UK]`, including `ᵗʰ` for days 11–13.
English dates outside the UK and `[Date Regional]` retain their existing formats.

With Arabic selected:

```sql
select [Date Lingua], [Date UK]
from calendar_local
where FullDateAlternateKey = '2026-04-02';
```

Output:

```text
١٤ شوال ١٤٤٧|2ⁿᵈ April 2026
```

With Hebrew selected, the same query gives a Jewish calendar date:

```sql
select [Date Lingua], [Date UK]
from calendar_local
where FullDateAlternateKey = '2026-04-02';
```

Output:

```text
15 בניסן 5786|2ⁿᵈ April 2026
```

Limmen defaults to `nl_NL`, irrespective of the installation machine's language.
Options and defaults use the packaged CLDR 46 language-status data, with an
explicit Latin option for Vatican City. They are not a live legal registry.
Subdivisions inherit country options unless an explicit override exists. The
saved selection takes precedence over these defaults. If a default locale has
no date-name data, the packaged mapping prefers an available official language;
territories without language data fall back to British English.

For an installed Limmen calendar:

```sql
select [Date Lingua], "Day Name", "Day Name Abbrev", "Month Name", "Month Name Abbrev",
       "ME Dates Text", "WE Dates Text"
from calendar_local
where FullDateAlternateKey = '2024-06-21';
```

Output (pipe-separated, with a header):

```text
Date Lingua|Day Name|Day Name Abbrev|Month Name|Month Name Abbrev|ME Dates Text|WE Dates Text
21 juni 2024|vrijdag|vr|juni|jun|jun 2024|23 jun 2024
```

Underlying dates use ISO `YYYY-MM-DD` text, timestamps use `YYYY-MM-DD HH:MM:SS`, and
times use `HH:MM:SS`. The `Date UK` column uses an unpadded day number, a Unicode
superscript ordinal suffix, the full English month name and the year. Suffixes
are `ˢᵗ`, `ⁿᵈ`, `ʳᵈ` and `ᵗʰ`; days 11, 12 and 13 all use `ᵗʰ`. This UK
display format and the April fiscal year remain unchanged for every location.
In particular, `Date UK` intentionally retains English month names and suffixes;
it is separate from `Date Lingua` and the localised month and weekday columns.
Use the ISO date column for chronological sorting.
Re-run the installer to rebuild an existing database with these columns and
locale tables; merely updating the source files does not change an installed database.

For a calendar installed for Shrewsbury, run this query on a SQLCipher
connection with the database key already set:

```sql
select FullDateAlternateKey, [Date UK], [Date Lingua], "Day Name", Sunrise, Sunset
from calendar_local
where FullDateAlternateKey = '2024-06-21';
```

Output (pipe-separated, with a header):

```text
FullDateAlternateKey|Date UK|Date Lingua|Day Name|Sunrise|Sunset
2024-06-21|21ˢᵗ June 2024|21ˢᵗ June 2024|Friday|04:47:00|21:39:00
```

Sunrise and sunset retain the original approximate solar model, rounded to the
nearest minute using the location's UTC offset at local noon. Polar dates with
no sunrise or sunset yield `NULL`. `Moon Phase %` retains the triangular
approximation using a 29.53-day cycle from 6 January 2000; it is **not** a
calculated illuminated fraction. Use the [almanac module](almanac.md) for
astronomical Moon-phase calculations.

`calendar_local_settings` records the selected location, jurisdiction, time
zone, coordinates and year range. `calendar_local_days` stores dated holiday,
weekend and UTC-offset values generated during installation. Reinstall to
refresh these snapshots, extend their date range, or change location; travelling
with the machine does not change the installed calendar. Future policy changes
require updated MARS rules or system time-zone data and reinstallation.

Installation requires the MARS shared library (built by the Make target),
Python's `zoneinfo` and system IANA time-zone data. Queries require only
SQLCipher with SQLite 3.35 or later and `SQLITE_ENABLE_MATH_FUNCTIONS`; no MARS
library, custom SQL functions or `generate_series` extension is needed when
querying. Loading the SQL script alone creates an empty calendar until the
installer populates its settings and policy tables. Ordering is not guaranteed
unless the caller supplies `ORDER BY`. Editing the source files does not update
an already installed database.

The locale seed is
`packaging/jurisdiction-db/mars_calendar_locale_names.sql`, generated by
`tools/generate_calendar_locales.py` using Babel 2.17.0 and Unicode CLDR 46.
Babel is a regeneration-only dependency: normal installation uses the packaged
SQL and Python's standard library, and SQL queries need no language packages or
OS-generated locales. Regeneration is version-pinned and the generator's
`--check` option verifies the checked-in seed. See the
[Unicode data notice](../THIRD_PARTY_NOTICES.md#unicode-cldr-week-data-and-calendar-names).

## Range Queries

`jurisdict_holidays_between()` returns an `array_t *` of `holiday_event_t` values:

```c
array_t *events = jurisdict_holidays_between(jurisdiction, start, end);
```

Destroy the returned array with `array_destroy(events)`. The array performs a
deep destroy of the holiday events it owns.

## API Reference

### Types

`jurisdiction_t`

- Opaque jurisdiction engine for one jurisdiction.
- Open it with `jurisdict_open()` and release it with `jurisdict_close()`.

`holiday_event_t`

- One holiday occurrence.
- Fields:
- `holiday_id` stable holiday identifier from the configured rule source.
- `rule_id` stable rule identifier for the rule that produced the occurrence.
- `event_year` civil year used to evaluate the holiday rule.
- `holiday_date` holiday date as a `datetime_t`.
- `holiday_name` display name for the holiday.
- `holiday_class` holiday class, typically `public`.
- `derived_from_observance` `true` when the event came from an observance or substitute-day rule rather than the base rule date.

`jurisdict_visit_fn`

- Visitor callback used by `jurisdict_each_holiday_between()`.
- Return `true` to continue enumeration, or `false` to stop early.

### Functions

`jurisdiction_t *jurisdict_open(const char *jurisdiction_code);`

- Opens a jurisdiction engine for a jurisdiction such as `GB-ENG`, `ZA`, `NL`, or `UA`.
- Pass `NULL` or an empty string to use the machine default jurisdiction.
- Returns `NULL` if the configured jurisdiction rule source cannot be opened.

`void jurisdict_close(jurisdiction_t *jurisdiction);`

- Releases an open jurisdiction engine.
- Safe to call with `NULL`.

`const char *jurisdict_last_error(const jurisdiction_t *jurisdiction);`

- Returns the last error message recorded on the engine.
- The returned pointer is borrowed and becomes invalid after the next jurisdiction API call on that engine or after `jurisdict_close()`.

`array_t *jurisdict_holidays_between(jurisdiction_t *jurisdiction, const datetime_t *start, const datetime_t *end);`

- Returns all holidays in the inclusive range `[start, end]`.
- The returned `array_t` owns deep copies of its `holiday_event_t` elements.
- Destroy it with `array_destroy()`.

`bool jurisdict_is_weekend(jurisdiction_t *jurisdiction, const datetime_t *date);`

- Returns whether the date is a weekend day in the selected jurisdiction.
- This uses jurisdiction-specific weekend rules, not a hardcoded Saturday/Sunday assumption.

`bool jurisdict_is_national_holiday(jurisdiction_t *jurisdiction, const datetime_t *date);`

- Returns whether the date is a holiday in the selected jurisdiction.
- Weekend status is separate; a date may be a weekend, a holiday, both, or neither.

`bool jurisdict_default_location(jurisdiction_t *jurisdiction, double *latitude, double *longitude);`

- Returns a representative default location for the selected jurisdiction.
- This is intended for UI defaults such as capital-city latitude and longitude.
- Returns `false` when no default location is configured.

`long jurisdict_working_days_between(jurisdiction_t *jurisdiction, const datetime_t *start, const datetime_t *end);`

- Counts working days in the inclusive range `[start, end]`.
- A working day is any day that is neither a jurisdictional weekend nor a holiday.
- Returns `-1` on failure.

`bool jurisdict_each_holiday_between(jurisdiction_t *jurisdiction, const datetime_t *start, const datetime_t *end, jurisdict_visit_fn visitor, void *ctx);`

- Enumerates holidays in ascending date order without building your own result array first.
- Useful when you want to stream results into your own container or stop early.
- Returns `false` on rule-loading or rule-evaluation failure.

### `jurisdict_default_gmt_offset()`

Reports whether the condition described by default gmt offset holds.

```c
bool jurisdict_default_gmt_offset(jurisdiction_t *jurisdiction, const datetime_t *date, double *offset_hours);
```

### `jurisdict_deserialise()`

Creates or reconstructs the public value described by deserialise.

```c
jurisdiction_t *jurisdict_deserialise(const void *data, size_t len, const string_t *type, const string_t *encoding);
```

### `jurisdict_dst_transition_datetimes()`

Reports whether the condition described by dst transition datetimes holds.

```c
bool jurisdict_dst_transition_datetimes(jurisdiction_t *jurisdiction, int year, datetime_t **clocks_forward, datetime_t **clocks_back);
```

### `jurisdict_dst_transition_details()`

Reports whether the condition described by dst transition details holds.

```c
bool jurisdict_dst_transition_details(jurisdiction_t *jurisdiction, int year, datetime_t **clocks_forward, double *forward_from_offset_hours, double *forward_to_offset_hours, datetime_t **clocks_back, double *back_from_offset_hours, double *back_to_offset_hours);
```

### `jurisdict_serialize()`

Reports whether the condition described by serialize holds.

```c
bool jurisdict_serialize(const jurisdiction_t *jurisdiction, string_t **out_type, string_t **out_encoding, void **out_data, size_t *out_len);
```

## Example: Range, Holiday, and Working-Day Queries

```c
#include <stdio.h>
#include <stdlib.h>
#include "array.h"
#include "datetime.h"
#include "jurisdiction.h"

int main(void) {
    jurisdiction_t *jurisdiction = jurisdict_open("GB-ENG");
    array_t *events = NULL;
    datetime_t *bank_holiday = datetime_init_ymd(datetime_alloc(), 2021, DT_December, 25);
    datetime_t *range_start = datetime_init_ymd(datetime_alloc(), 2021, DT_December, 24);
    datetime_t *range_end = datetime_init_ymd(datetime_alloc(), 2021, DT_December, 31);
    long working_days;
    size_t i;

    if (!jurisdiction || !bank_holiday || !range_start || !range_end) {
        fprintf(stderr, "Jurisdiction data is unavailable.\n");
        return 1;
    }

    events = jurisdict_holidays_between(jurisdiction, range_start, range_end);
    if (!events) {
        fprintf(stderr, "Jurisdiction query failed.\n");
        return 1;
    }

    printf("Holidays between 2021-12-24 and 2021-12-31:\n");
    for (i = 0; i < array_size(events); ++i) {
        holiday_event_t *event = array_get(events, i);
        char *date_text = datetime_format(event->holiday_date, "%yyyy-%MM-%dd");

        printf("- %s: %s\n",
               event->holiday_name,
               date_text ? date_text : "(unavailable)");
        free(date_text);
    }

    printf("2021-12-25 weekend: %s\n",
           jurisdict_is_weekend(jurisdiction, bank_holiday) ? "yes" : "no");
    printf("2021-12-25 national holiday: %s\n",
           jurisdict_is_national_holiday(jurisdiction, bank_holiday) ? "yes" : "no");

    working_days = jurisdict_working_days_between(jurisdiction, range_start, range_end);
    printf("Working days between 2021-12-24 and 2021-12-31: %ld\n", working_days);

    array_destroy(events);
    datetime_dealloc(range_end);
    datetime_dealloc(range_start);
    datetime_dealloc(bank_holiday);
    jurisdict_close(jurisdiction);
    return 0;
}
```

Expected output:

```text
Holidays between 2021-12-24 and 2021-12-31:
- Bank Holiday in Lieu of Christmas Day: 2021-12-27
- Bank Holiday in Lieu of Boxing Day: 2021-12-28
2021-12-25 weekend: yes
2021-12-25 national holiday: no
Working days between 2021-12-24 and 2021-12-31: 4
```

## Notes

- Jurisdiction codes follow the jurisdiction rule source, for example `GB-ENG`,
  `ZA`, `NL`, or `UA`.
- If you pass `NULL` or an empty string to `jurisdict_open()`, the engine uses
  the machine's configured default jurisdiction.
- `jurisdict_is_weekend()` and `jurisdict_is_national_holiday()` answer different
  questions. A date can be one, the other, both, or neither.
