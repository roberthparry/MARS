# `jurisdiction_t`

`jurisdiction_t` provides jurisdiction-aware public holiday lookups on top of the
configured jurisdiction database installed with MARS Lab.

It answers questions such as:

- which holidays fall between two dates
- whether a date is a weekend in a given jurisdiction
- whether a date is a national holiday
- how many working days fall in a date range

## Scope

`jurisdiction_t` is the jurisdiction-policy layer that sits above `datetime_t`.

`datetime_t` remains responsible for reusable calendar calculations such as
Easter, Orthodox Easter, Chinese New Year, and other observance dates.
`jurisdiction_t` applies jurisdiction-specific rules such as:

- substitute-day behaviour
- weekend definitions that vary by country
- one-off exceptions such as funerals, coronations, or special observances
- historical rule changes over time

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
