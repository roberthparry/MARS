# `datetime_t`

`datetime_t` provides civil calendar utilities together with higher-level
astronomical helpers.

## Capabilities

- Gregorian calendar support
- Julian Day conversion
- leap-year and weekday helpers
- date/time arithmetic
- holiday helpers
- sunrise and sunset utilities
- moon-phase helpers
- formatting support

## Example

```c
#include <stdio.h>
#include "datetime.h"

int main(void) {
    struct {
        int year;
        month_t month;
        unsigned char day;
    } cases[] = {
        {2020, DT_January, 25},
        {2021, DT_February, 12},
        {2022, DT_February, 1},
        {2023, DT_January, 22},
        {2024, DT_February, 10},
        {2025, DT_January, 29},
    };

    for (int i = 0; i < 6; i++) {
        datetime_t *dt = datetime_init_chinese_new_year(
            datetime_alloc(),
            cases[i].year
        );

        if (!dt) {
            printf("Year %d is outside the supported range.\n", cases[i].year);
            continue;
        }

        short y = datetime_year(dt);
        month_t m = datetime_month(dt);
        unsigned char d = datetime_day(dt);

        printf("Chinese New Year %d: %d-%02d-%02d\n",
               cases[i].year, y, (int)m, d);

        datetime_dealloc(dt);
    }

    return 0;
}
```

Expected output:

```text
Chinese New Year 2020: 2020-01-25
Chinese New Year 2021: 2021-02-12
Chinese New Year 2022: 2022-02-01
Chinese New Year 2023: 2023-01-22
Chinese New Year 2024: 2024-02-10
Chinese New Year 2025: 2025-01-29
```

## Design Notes

### Civil Calendar Layer

The civil layer handles:

- year, month, and day arithmetic
- leap years
- month boundaries
- weekday calculations
- Julian Day conversion

This part is mostly deterministic and relies primarily on integer arithmetic.

### Astronomical Layer

The module also includes higher-level helpers for tasks such as:

- sunrise and sunset estimation
- solar position helpers
- moon-phase helpers
- calendar-related astronomical events

These routines are intended for practical use, not as a substitute for a full
ephemeris engine.

### Ownership

The public API shown here uses explicit allocation helpers such as
`datetime_alloc()` and `datetime_dealloc()`, which keeps ownership visible.

## Scope

`datetime_t` combines practical civil date handling with lightweight
astronomical support in a self-contained C API.

## API Reference

The full public API is declared in `include/datetime.h`. The functions below are
the main entry points shown in this document.

### Construction and Lifetime

- `datetime_alloc(void)`  
  Allocate a `datetime_t`.

- `datetime_dealloc(datetime_t *dt)`  
  Release a `datetime_t`.

### Calendar Construction

- `datetime_init_chinese_new_year(datetime_t *dt, int year)`  
  Initialize a date to the Chinese New Year for the given year.

### Field Access

- `datetime_year(const datetime_t *dt)`  
  Return the year component.

- `datetime_month(const datetime_t *dt)`  
  Return the month component.

- `datetime_day(const datetime_t *dt)`  
  Return the day-of-month component.