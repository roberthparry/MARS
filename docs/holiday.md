# `holiday_t`

`holiday_t` provides jurisdiction-aware public holiday lookups on top of the
configured holiday rule source installed with MARS Lab.

It answers questions such as:

- which holidays fall between two dates
- whether a date is a weekend in a given jurisdiction
- whether a date is a national holiday
- how many working days fall in a date range

## Scope

`holiday_t` is the jurisdiction-policy layer that sits above `datetime_t`.

`datetime_t` remains responsible for reusable calendar calculations such as
Easter, Orthodox Easter, Chinese New Year, and other observance dates.
`holiday_t` applies jurisdiction-specific rules such as:

- substitute-day behaviour
- weekend definitions that vary by country
- one-off exceptions such as funerals, coronations, or special observances
- historical rule changes over time

## Basic Usage

Open a holiday engine for a jurisdiction:

```c
#include "holiday.h"

holiday_t *holiday = holiday_open("GB-ENG");
```

When you are finished, release it with:

```c
holiday_close(holiday);
```

## Installation

To provision the private holiday database without installing the full desktop
app, run:

```sh
make install-holiday-db
```

To remove that private holiday database again, run:

```sh
make uninstall-holiday-db
```

If you want the desktop app as well, use `make install-mars-lab`.

## Range Queries

`holiday_between()` returns an `array_t *` of `holiday_event_t` values:

```c
array_t *events = holiday_between(holiday, start, end);
```

Destroy the returned array with `array_destroy(events)`. The array performs a
deep destroy of the holiday events it owns.

## API Reference

### Types

`holiday_t`

- Opaque holiday engine for one jurisdiction.
- Open it with `holiday_open()` and release it with `holiday_close()`.

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

`holiday_visit_fn`

- Visitor callback used by `holiday_each_between()`.
- Return `true` to continue enumeration, or `false` to stop early.

### Functions

`holiday_t *holiday_open(const char *jurisdiction);`

- Opens a holiday engine for a jurisdiction such as `GB-ENG`, `ZA`, `NL`, or `UA`.
- Pass `NULL` or an empty string to use the machine default jurisdiction.
- Returns `NULL` if the configured holiday rule source cannot be opened.

`void holiday_close(holiday_t *holiday);`

- Releases an open holiday engine.
- Safe to call with `NULL`.

`const char *holiday_last_error(const holiday_t *holiday);`

- Returns the last error message recorded on the engine.
- The returned pointer is borrowed and becomes invalid after the next holiday API call on that engine or after `holiday_close()`.

`array_t *holiday_between(holiday_t *holiday, const datetime_t *start, const datetime_t *end);`

- Returns all holidays in the inclusive range `[start, end]`.
- The returned `array_t` owns deep copies of its `holiday_event_t` elements.
- Destroy it with `array_destroy()`.

`bool holiday_is_weekend(holiday_t *holiday, const datetime_t *date);`

- Returns whether the date is a weekend day in the selected jurisdiction.
- This uses jurisdiction-specific weekend rules, not a hardcoded Saturday/Sunday assumption.

`bool holiday_is_national_holiday(holiday_t *holiday, const datetime_t *date);`

- Returns whether the date is a holiday in the selected jurisdiction.
- Weekend status is separate; a date may be a weekend, a holiday, both, or neither.

`long holiday_working_days_between(holiday_t *holiday, const datetime_t *start, const datetime_t *end);`

- Counts working days in the inclusive range `[start, end]`.
- A working day is any day that is neither a jurisdictional weekend nor a holiday.
- Returns `-1` on failure.

`bool holiday_each_between(holiday_t *holiday, const datetime_t *start, const datetime_t *end, holiday_visit_fn visitor, void *ctx);`

- Enumerates holidays in ascending date order without building your own result array first.
- Useful when you want to stream results into your own container or stop early.
- Returns `false` on rule-loading or rule-evaluation failure.

## Example: Range, Holiday, and Working-Day Queries

```c
#include <stdio.h>
#include <stdlib.h>
#include "array.h"
#include "datetime.h"
#include "holiday.h"

int main(void) {
    holiday_t *holiday = holiday_open("GB-ENG");
    array_t *events = NULL;
    datetime_t *bank_holiday = datetime_init_ymd(datetime_alloc(), 2021, DT_December, 25);
    datetime_t *range_start = datetime_init_ymd(datetime_alloc(), 2021, DT_December, 24);
    datetime_t *range_end = datetime_init_ymd(datetime_alloc(), 2021, DT_December, 31);
    long working_days;
    size_t i;

    if (!holiday || !bank_holiday || !range_start || !range_end) {
        fprintf(stderr, "Holiday data is unavailable.\n");
        return 1;
    }

    events = holiday_between(holiday, range_start, range_end);
    if (!events) {
        fprintf(stderr, "Holiday query failed.\n");
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
           holiday_is_weekend(holiday, bank_holiday) ? "yes" : "no");
    printf("2021-12-25 national holiday: %s\n",
           holiday_is_national_holiday(holiday, bank_holiday) ? "yes" : "no");

    working_days = holiday_working_days_between(holiday, range_start, range_end);
    printf("Working days between 2021-12-24 and 2021-12-31: %ld\n", working_days);

    array_destroy(events);
    datetime_dealloc(range_end);
    datetime_dealloc(range_start);
    datetime_dealloc(bank_holiday);
    holiday_close(holiday);
    return 0;
}
```

Expected output:

```text
Holidays between 2021-12-24 and 2021-12-31:
- Christmas Day: 2021-12-25
- Boxing Day: 2021-12-26
- Bank Holiday in Lieu of Christmas Day: 2021-12-27
- Bank Holiday in Lieu of Boxing Day: 2021-12-28
2021-12-25 weekend: yes
2021-12-25 national holiday: yes
Working days between 2021-12-24 and 2021-12-31: 4
```

## Notes

- Jurisdiction codes follow the holiday rule source, for example `GB-ENG`,
  `ZA`, `NL`, or `UA`.
- If you pass `NULL` or an empty string to `holiday_open()`, the engine uses
  the machine's configured default jurisdiction.
- `holiday_is_weekend()` and `holiday_is_national_holiday()` answer different
  questions. A date can be one, the other, both, or neither.
