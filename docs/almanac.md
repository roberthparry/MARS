# Almanac

The almanac layer opens the configured ephemeris database, computes GHA Aries,
and resolves SHA/declination snapshots for catalogued bodies like `SUN`,
`MOON`, `MARS`, `JUPITER`, or named stars such as `SIRIUS`.

## Quick Example

```c
#include <stdio.h>

#include "almanac.h"
#include "datetime.h"

int main(void)
{
    datetime_t *moment = datetime_alloc();
    almanac_t *almanac = almanac_open();
    almanac_entry_t sun;
    almanac_entry_t moon;

    if (!moment || !almanac)
        return 1;
    if (!datetime_init_ymdt(moment, 2026, DT_August, 12, 17, 47, 5.8))
        return 1;
    if (!almanac_lookup(almanac, "SUN", moment, &sun))
        return 1;
    if (!almanac_lookup(almanac, "MOON", moment, &moon))
        return 1;

    printf("Sun  SHA %.9f  Dec %.9f\n",
           sun.sha_degrees,
           sun.declination_degrees);
    printf("Moon SHA %.9f  Dec %.9f\n",
           moon.sha_degrees,
           moon.declination_degrees);

    almanac_close(almanac);
    datetime_dealloc(moment);
    return 0;
}
```

For a Shrewsbury eclipse-watching example, the test suite uses the greatest
eclipse instant of the 2026-08-12 solar eclipse. That eclipse is a strong
partial from Shrewsbury rather than total, but it still gives a memorable
Sun/Moon alignment example that stays inside the current packaged almanac date
range.

Example output from `example_almanac_shrewsbury_eclipse_watch`:

```text
Shrewsbury eclipse watch
2026-08-12 17:47:05.8 GMT
Greatest eclipse of the 2026-08-12 solar eclipse.
Shrewsbury sees a deep partial eclipse, not totality.
Sun:  SHA 217.552361258 deg  Dec 14.801020249 deg  Dist 1.013291318778 AU
Moon: SHA 217.165828033 deg  Dec 15.611220445 deg  Dist 0.002453090521 AU
Separation: SHA 1391.52 arcsec  Dec 2916.72 arcsec
```

## Oracle Output

The oracle regression prints `expected`, `got`, rounded arcsecond error, and a
navigation-grade decision for each axis. A case counts as navigation grade
`PASS` when the rounded SHA and declination errors are both `<= 6` arcseconds;
otherwise it is a `FAIL`.
