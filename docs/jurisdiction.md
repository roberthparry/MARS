# `jurisdiction_t`

`jurisdiction_t` provides jurisdiction-aware public holiday lookups on top of the
configured jurisdiction database installed with MARS Lab.

It answers questions such as:

- which holidays fall between two dates
- whether a date is a weekend in a given jurisdiction
- whether a date is a national holiday
- how many working days fall in a date range

## Country names and flags

`jurisdict_setup_origin` visits the single location saved by calendar installation,
independently of the engine's selected jurisdiction. It supplies borrowed place strings
for the callback's duration, including coordinates and the IANA timezone. It returns false
when setup is missing or unreadable, without substituting a jurisdiction default or
inferring coordinates from the machine's language. MARS Lab uses this for its home-sky card.

`jurisdict_country(jurisdiction, code, &name, &flag_svg)` looks up an exact uppercase
ISO alpha-2 country code or subdivision code through the catalogue's indexed key.
Subdivisions retain their own name and use their own artwork when available,
otherwise inheriting the parent country's flag. Kaliningrad Oblast (`RU-KGD`)
uses the Russian flag, its existing town catalogue and the `Europe/Kaliningrad`
timezone, with Kaliningrad as its representative town. Wales (`GB-WLS`), Scotland (`GB-SCT`) and the
Isle of Man (`IM`) therefore retain their distinct flags. The caller releases
both owned strings with `string_free`. An unavailable flag leaves its output NULL
while the country name remains usable. Invalid or unknown codes return false and
clear both outputs; the selected jurisdiction is never used as a fallback.

`jurisdiction_country_flag` owns offline SVG artwork, keyed by country code.
`jurisdiction_country_flag_source` retains version, upstream address and licence
records. The bundled flag-icons 7.3.2 artwork imports for all 249 ISO countries
and the four UK constituent-country entries, alongside the separately attributed
regional additions. Display-only emblems use the same SVG table and the separate
`jurisdiction_flag_emblem` name catalogue. `jurisdict_country` resolves `ZZ-SEA`
to the static Offshore emblem without adding a jurisdiction, town or calendar.
Its source SVG and generation record are packaged under `flags/zz-sea.svg` and
`flags/zz-sea.txt`; `OFFSHORE-NOTICES.txt` is imported into the provenance table.
Managed-data revision 4 imports this artwork into existing installations without
rebuilding their towns or calendars. Missing required emblem artwork rolls back
the import instead of marking an incomplete migration successful.
Running `make update-mars-db` applies pending flag metadata migrations to an
existing database without rebuilding its calendar or town tables. The separate
`make install-jurisdiction-db` command rebuilds the packaged database. Custom older
packages without artwork remain usable. Country geometry and ISO identifiers
remain in the separate globe database, updated with `make update-globe-db`.

The Almanac uses the independently identified land country's name and flag in
the selected-position tooltip and its Navigation Almanac card. Sea positions,
uncertain coasts and missing/ambiguous boundaries do not inherit a country from
the form selection. Flags are decorative beside accessible country-name text;
they are not evidence of legal jurisdiction. No network request is required.

Artwork: [flag-icons 7.3.2](https://github.com/lipis/flag-icons/tree/v7.3.2),
[bundled MIT licence](../packaging/jurisdiction-db/flags/LICENSE).
Source archive SHA-256: `6a5497e1029e66415c3941a5e5ee001944b0a7e46bf365c6d5a91a033acee87e`.

### Regional display flags

`jurisdict_country` also accepts the local display codes in
`jurisdiction_flag_region`, whose indexed rows define a display name, parent
flag and regional artwork. They are not additional sovereign jurisdictions.
Returned self-contained SVGs cross-fade regional and parent artwork over a
fifteen-second cycle: hold each flag for five seconds, then fade to the other over
2.5 seconds, easing smoothly at either end. CSS reduced-motion preference retains the
regional image. No JavaScript interval, external image request or persistent
animation state is needed. Missing parent artwork leaves the regional flag static.

Ordinary England, Wales, Scotland, Isle of Man, Northern Ireland and Jersey
flags remain static. Cornwall alternates St Piran's flag with England's cross,
not the Union flag. Catalan artwork is also used for Valencia and the Balearics;
their display names remain distinct. West, East and North Frisia have distinct
artwork. North Frisian coverage includes Helgoland. Saterland (`DE-SAT`) uses
its own municipal flag, and Sorbian Lusatia (`DE-SOR`) uses the blue–red–white
Sorbian tricolour; both alternate with Germany's flag without changing jurisdiction.
Kashubia (`PL-KAS`) alternates the black-over-yellow Kashubian bicolour
with Poland's flag; the country's jurisdiction and civil time remain Polish.
Finland's Sámi homeland (`FI-SAP`) alternates the Sámi flag with Finland's flag;
the jurisdiction and civil time remain Finnish.
Mongolia's Kazakh cultural areas (`MN-KAZ`) alternate Kazakhstan's existing
blue-and-gold flag artwork with Mongolia's flag. Mongolia remains the jurisdiction
and supplies civil time; this is a cultural display convention, not a territorial claim.
Cultural coverage, including historical Brittany and both sides of the
Basque border, is defined by the independent [globe region layer](globe.md#regional-flag-coverage).

Flag-cache identifiers remain separate from geographic country identifiers.
A regional reply therefore cannot change a country's cached national flag or
the observer's jurisdiction/time-zone selection. The same artwork is used in
town tooltips, the location summary card and the selected jurisdiction dropdown;
the dropdown retains its actual jurisdiction value. Additional artwork and its
licences are recorded in [regional notices](../packaging/jurisdiction-db/flags/REGIONAL-NOTICES.txt).

## River network

River geometry belongs in the independent globe database, not the jurisdiction
catalogue. The inaccurate WDBII network has been withdrawn; no replacement is
packaged yet. Town and calendar data remain available.

`jurisdict_globe_open()` returns an owned `globe_t` controller borrowing the engine's
town connection and owning its separately configured geometry connection. Release
it with `globe_close()` before closing the jurisdiction engine. Missing globe
storage leaves towns available; legacy river tables in jurisdiction storage are
not used.
See the [globe module guide](globe.md) for the query API, storage, dataset
provenance, memory behaviour and limitations.

## Catalogue visitors

`jurisdict_each_choice()` enumerates installed country and subdivision codes with
database-derived labels, sorted case-insensitively by label. Subdivision labels
include the parent name. `jurisdict_each_location()` enumerates representative
locations in jurisdiction-code order. `jurisdict_each_town()` enumerates towns by
jurisdiction, default first, then case-insensitive name. All three use the whole
configured database, independently of the engine's selected jurisdiction.
Town enumeration visits indexed jurisdictions in order, limiting each name sort
to one jurisdiction rather than sorting the entire worldwide catalogue in memory.
The offline town generator resolves British GeoNames administrative codes to
England (`GB-ENG`), Northern Ireland (`GB-NIR`), Scotland (`GB-SCT`) and Wales
(`GB-WLS`). These nation identifiers must be available in its staging catalogue,
even though their installed definitions come from the holiday seed rather than
the target-subdivision supplement. Colchester and West Mersea belong to England,
not the generic Britain list. Existing installations need a jurisdiction database
reinstallation to receive corrected packaged assignments.

`jurisdict_place_t` supplies the jurisdiction code, locality name, latitude,
longitude, elevation, IANA timezone and default marker. Coordinate fields remain
text to preserve their stored precision. Locations and towns need coordinates
and a timezone; incomplete records are omitted. Missing town elevation becomes
`"0"`. Records and strings are borrowed only during the callback: copy anything
you retain, and do not close or re-enter the engine from a visitor.

The callbacks are `jurisdict_choice_visit_fn` and `jurisdict_place_visit_fn`.
Return `false` from a visitor to stop successfully. The enumeration itself returns
`false` for invalid arguments or database errors; `jurisdict_last_error()` reports
the engine diagnostic. A successful empty enumeration is distinct from an error.
These calls query the database, never the packaging SQL or a duplicated JSON list.

The following complete example requires the installed jurisdiction database:

```c
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "jurisdiction-t.h"

static bool readme_location(const jurisdict_place_t *place, void *context)
{
    bool *found = context;
    if (strcmp(place->jurisdiction_code, "AD"))
        return true;
    printf("%s: %s (%s, %s), %s\n", place->jurisdiction_code, place->name,
           place->latitude, place->longitude, place->timezone);
    *found = true;
    return false;
}

int main(void)
{
    jurisdiction_t *engine = jurisdict_open(NULL);
    bool found = false;
    bool ok = engine && jurisdict_each_location(engine, readme_location, &found);
    jurisdict_close(engine);
    return ok && found ? EXIT_SUCCESS : EXIT_FAILURE;
}
```

```text
AD: Andorra (42.5000, 1.5167), Europe/Andorra
```

## Configuration files

An empty or absent database key selects plaintext SQLite; a non-empty key retains encrypted SQLCipher.
An explicitly empty modern environment key overrides saved and legacy keys. Opening never retries a failed keyed
connection as plaintext. Existing files require explicit [conversion](globe.md#plaintext-conversion), not a key deletion.

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
Callers continue to use the unchanged public API in `include/jurisdiction-t.h`.

## Basic Usage

Open a jurisdiction engine for a jurisdiction:

```c
#include <stdio.h>
#include "jurisdiction-t.h"

int main(void)
{
    jurisdiction_t *jurisdiction = jurisdict_open("GB-ENG");
    if (!jurisdiction) return 1;
    printf("opened GB-ENG\n");
    jurisdict_close(jurisdiction);
    return 0;
}
```

Output:

```text
opened GB-ENG
```

The example releases the engine with `jurisdict_close(jurisdiction)` before returning.

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

This removes only `~/.mars/jurisdiction/mars_jurisdiction_rules.db`, its SQLite
journal/WAL sidecars and `~/.mars/config/jurisdiction-db.env`. Almanac data,
weather configuration, Lab settings and other files are preserved. Databases
installed at custom paths are not removed by this target.

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

Before generating the calendar, the installer releases unused SQLCipher import-cache
memory. This avoids retaining locked cache pages while opening the calendar reader
and building the snapshot; encrypted mode retains encryption and SQLCipher memory protection.
The installer's main and temporary page caches are each limited to a suggested
512 KiB working size, keeping larger town imports within the same memory-lock budget.
Large town-import and normalised-selection tables live in the selected-mode staging
database rather than memory-only temporary storage. They are removed before the
database is published. Plaintext mode skips key derivation for the separate calendar reader too.
The bounded SQL importer accepts blank lines and standalone `--` comments between
literal tuples, including after a batch has been flushed. Import failures identify
the packaged source and line number without printing SQL or credentials.

To select Limmen during installation:

```sh
make install-jurisdiction-db limmen
```

The final output line is:

```text
calendar_local location: Limmen, NL
```

The Italian catalogue includes 54 Sardinian towns, covering coastal and inland
locations such as Cagliari, Sassari, Olbia, Alghero, Oristano, Nuoro, Bosa and
Tortolì. These use GeoNames coordinates and approximate DEM elevations with the
`Europe/Rome` timezone; the [data attribution](../THIRD_PARTY_NOTICES.md#geonames-sardinian-town-catalogue)
describes their source. Reinstall the jurisdiction database and reload MARS Lab
to make added towns available in both the observer selector and globe.

The French catalogue also includes Ajaccio, Bastia, Biguglia, Borgo, Calvi,
Corte and Porto-Vecchio in Corsica, from the same GeoNames extract, with
approximate DEM elevations and the `Europe/Paris` timezone.

The worldwide supplement established a baseline of **50,000 locations**,
adding 46,455 GeoNames populated places across more than 200 countries and
territories. Existing coordinates and default choices are retained. Duplicate
names within a jurisdiction are distinguished by a GeoNames-ID suffix. New US
and British places use supported subdivisions; other new places use country
jurisdictions rather than guessing a regional holiday policy. Coordinates and
DEM elevations describe reference points, not an observer's exact position.

The source DEM value `-9999` denotes unavailable elevation, not a location below
sea level. Imports omit that elevation record while retaining the town. Catalogue
readers also recognise the sentinel in older installed databases and return the
existing `0` metre fallback. This is an assumed starting value, not a surveyed
height; set the actual observer altitude when known. Genuine negative elevations
are preserved, and manually entered observer heights remain subject to validation.

Targeted Sunshine Coast additions brought the packaged total to **50,004
records**: Mooloolaba and Maroochydore each appear under Australia (`AU`) and
Queensland (`AU-QLD`), with the `Australia/Brisbane` timezone. GeoNames classifies
these as urban sections (`PPLX`), normally excluded by the worldwide generator;
explicit seeds retain these useful coastal locations without importing every
suburb. The existing Sunshine Coast entry and all other towns are retained.
The Azores (`PT-20`) and Madeira (`PT-30`) also have explicit regional lists,
selected from Portugal's towns by their respective `Atlantic/Azores` and
`Atlantic/Madeira` timezones. Their defaults are Ponta Delgada and Funchal, not
Lisbon. These 29 regional entries bring the packaged total to **50,033 records**;
the original Portugal-wide entries remain available for existing selections.
Reinstall the jurisdiction database and restart MARS Lab to load catalogue changes;
updating the globe database alone does not update towns.

`jurisdict_each_town_jurisdiction_batch()` restricts indexed town pagination to
one non-empty jurisdiction code, without implicit parent or child expansion.
It shares the page limits, cursor semantics and borrowed callback records below.

`jurisdict_each_town_batch()` provides indexed, ascending-ID pagination for
bounded transports. Start with ID zero, request 1–1024 records, and continue
with the returned last ID until an empty batch returns zero. Records are borrowed
for the callback duration, as with the other catalogue visitors. A callback may
stop early; the returned ID then identifies its last visited record. These cursors
must not be reused after the installed database is replaced. Display ordering is
separate from ID pagination; `jurisdict_each_town()` retains its ordered full visitor.

`jurisdict_each_town_near_batch()` prioritises towns within five degrees of the
supplied latitude and longitude, wrapping longitude across the date line. It uses
the same bounded ID pagination; keep the centre fixed until the local sequence
ends. This rectangular window is not a nearest-distance ranking. New installations
index numeric latitude for these queries; older databases remain compatible but
may scan the latitude records until reinstalled.

The globe indexes towns geographically and suppresses overlapping labels in the
current view, while retaining the selected label. Loading uses bounded background
pages rather than one large response. The generator caps the combined seed and
supplement at 50,000 records. National capitals, first-level regional seats and
towns with at least 100,000 inhabitants take precedence, followed by the selected
cruising regions; remaining candidates are ranked by population, with GeoNames
ID breaking ties. These are country-level priorities, not a coastline-distance
filter or a verified harbour directory. Existing smaller seed towns remain available.

Coverage includes 109 Welsh and 137 Irish records, Fishguard, Wexford, all three Nykøbing
towns, Castricum, Alkmaar and Madalena on Pico. Small villages absent from the
cities5000 source still need targeted additions.
Fishguard is a targeted seed from the fuller British extract, using its town
reference point rather than a harbour entrance position.
The seed also includes 16 targeted Cape Peninsula places from the fuller South
African extract, including Fish Hoek, Noordhoek, Camps Bay, Hout Bay, Clovelly
and Kalk Bay. Selected coastal suburbs are included even where the worldwide
source omits them; regenerating the supplement keeps the overall 50,000-record cap.
This is not a complete gazetteer. See the
[worldwide data attribution](../THIRD_PARTY_NOTICES.md#geonames-worldwide-town-supplement).

Maintainers can regenerate the supplement with the native `catalogue_towns`
target in `tools/mars_config/Makefile`. The executable takes the extracted
GeoNames cities5000 TSV pathname and output SQL pathname, and runs from the
repository root. It uses indexed staging queries, preserves the original seed,
and emits bounded batches; installation does not download geographical data.

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

Database installation code lives together in `tools/mars_config/src/database/`.
`cfg_database_almanac.c`, `cfg_database_jurisdiction.c` and `cfg_database_globe.c`
own their respective database settings and installation policies. The shared
`cfg_database.c` manages the shared installation lifecycle, single-key coordinator,
bounded SQL imports and safe publication. Globe-specific imports and staging live
in `cfg_database_globe.c`. These four C files share one private declarations header;
installer code does not belong to the runtime library. This source organisation
does not change installation commands, configuration paths or database keys.

Installation uses the native `mars_config` application (built by the Make target),
linked to MARS, and system IANA time-zone data. The installer displays a live
progress bar for each packaged SQL source, with percentages based on processed
bytes rather than estimated installation time. Database preparation, calendar
generation and publication show an activity indicator and elapsed time, including
while a database operation is blocking. Reporting stops before interactive prompts.
Redirected output uses plain stage messages and five-second activity updates,
without terminal control codes. A failed stage is never reported as complete.
Queries require only
SQLCipher with SQLite 3.35 or later and `SQLITE_ENABLE_MATH_FUNCTIONS`; no MARS
library, custom SQL functions or `generate_series` extension is needed when
querying. Loading the SQL script alone creates an empty calendar until the
installer populates its settings and policy tables. Ordering is not guaranteed
unless the caller supplies `ORDER BY`. Editing the source files does not update
an already installed database.

The locale seed is
`packaging/jurisdiction-db/mars_calendar_locale_names.sql`, generated by
the native `mars_config locales` command. Its pinned input is resolved Unicode
CLDR 46 data originally extracted through Babel 2.17.0, together with separately
maintained language supplements. Regeneration, installation and calendar queries
require neither Python nor Babel nor OS-generated locales. The generator selects
territory defaults and language options, shares identical name sets, tokenises
long-date patterns and quotes SQL values; it does not copy the packaged SQL.

Build the generator with `make -j1 native-config`. Verify the checked-in seed:

```sh
tools/mars_config/build/release/mars_config locales --check
```

Output:

```text
Calendar locale seed matches Babel 2.17.0 / CLDR 46 and explicit supplements.
```

To regenerate it, use the same command with
`--output packaging/jurisdiction-db/mars_calendar_locale_names.sql` instead of
`--check`. With neither option, SQL is written to standard output. Generation
does not update an already installed database; reinstall the jurisdiction database
to use changed data. The source data is version-pinned rather than downloaded
during generation. Inputs live under `tools/mars_config/data/locales/`:
`cldr46.json` records resolution and territory-language data, the
`cldr46-locales-*.json` fragments contain resolved calendar labels and patterns,
and `supplements.json` keeps the hand-maintained vocabulary, source references and
application-specific options separate from CLDR. Use `--data-dir DIR` to validate
an alternative input directory before replacing the packaged seed. Updating CLDR
requires a new reviewed snapshot and regression comparison; this command is not a
raw CLDR XML importer. Source JSON is ASCII-only: non-ASCII text uses single-key
`utf8_hex` objects containing its UTF-8 bytes in hexadecimal. The native loader
validates and decodes these without Unicode normalisation, preserving even
deliberately decomposed CLDR spellings. Input files are limited to 4 MiB each and
32 MiB in total; output is limited to 16 MiB. Date patterns are limited to 1,024
bytes and 128 parts, with era/suffix text limited to 256 bytes. Invalid input
produces no SQL and leaves an existing output destination untouched. Supplemental
town names use the installer's catalogue-key normalisation; non-ASCII town names
require the existing libunistring-enabled build. See the
[Unicode data notice](../THIRD_PARTY_NOTICES.md#unicode-cldr-week-data-and-calendar-names).

## Range Queries

`jurisdict_holidays_between()` returns an `array_t *` of `holiday_event_t` values:

```c
#include <stdio.h>
#include "jurisdiction-t.h"

int main(void)
{
    jurisdiction_t *jurisdiction = jurisdict_open("GB-ENG");
    datetime_t *start = datetime_init_ymd(datetime_alloc(), 2021, DT_December, 24);
    datetime_t *end = datetime_init_ymd(datetime_alloc(), 2021, DT_December, 31);
    if (!jurisdiction || !start || !end) return 1;
    array_t *events = jurisdict_holidays_between(jurisdiction, start, end);
    if (!events) return 1;
    printf("holiday count: %zu\n", array_size(events));
    array_destroy(events);
    datetime_dealloc(end);
    datetime_dealloc(start);
    jurisdict_close(jurisdiction);
    return 0;
}
```

Output:

```text
holiday count: 2
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
#include "array-t.h"
#include "datetime-t.h"
#include "jurisdiction-t.h"

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
