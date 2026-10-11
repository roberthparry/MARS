/**
 * @file tba_period.c
 * @brief Timezone-independent monthly, quarterly and annual horizon calculations.
 *
 * Shared with native regression tests. Numeric civil dates avoid browser timezone
 * and daylight-saving shifts. Daily advances skip whole months, at most 329 for
 * the bounded 10000-day horizon; other frequencies use direct arithmetic.
 */
#include "tba_period.h"

static int tba_period_choices[1200];
static unsigned tba_period_choice_count;

static int tba_period_days(int year, unsigned month)
{
    static const int days[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    return month == 2 ? 28 + (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)) : days[month];
}

static int tba_period_date(int year, unsigned month, int day)
{
    return year >= 1 && year <= 9999 && month >= 1 && month <= 12 && day >= 1 &&
                   day <= tba_period_days(year, month) ? year * 10000 + (int)month * 100 + day : 0;
}

/* Fiscal and calendar quarters share March, June, September and December boundaries. */
int tba_period_end(int date, unsigned frequency, int fiscal)
{
    int year = date / 10000, day = date % 100;
    unsigned month = (unsigned)(date / 100 % 100);
    if (!tba_period_date(year, month, day) || frequency > 4)
        return 0;
    if (frequency < 2)
        return date;
    if (frequency == 3)
        month = ((month - 1) / 3 + 1) * 3;
    if (frequency == 4) {
        year += fiscal && month > 3;
        month = fiscal ? 3 : 12;
    }
    return tba_period_date(year, month, tba_period_days(year, month));
}

/* Keep integer date transitions bounded and independent of browser Date normalisation. */
int tba_period_add(int date, unsigned frequency, unsigned count, int fiscal)
{
    int year = date / 10000, day = date % 100;
    unsigned month = (unsigned)(date / 100 % 100);
    if (!tba_period_date(year, month, day) || frequency > 4 || count > 10000)
        return 0;
    if (frequency == 0)
        return date;
    if (frequency == 1) {
        day += (int)count;
        while (day > tba_period_days(year, month)) {
            day -= tba_period_days(year, month);
            if (++month > 12) {
                month = 1;
                ++year;
            }
        }
        return tba_period_date(year, month, day);
    }
    if (frequency == 4) {
        year += (int)count + (fiscal && month > 3);
        month = fiscal ? 3 : 12;
    } else {
        unsigned months = month - 1 + count * (frequency == 3 ? 3 : 1);
        year += (int)(months / 12);
        month = months % 12 + 1;
    }
    return tba_period_end(tba_period_date(year, month, 1), frequency, fiscal);
}

/* Generate bounded horizon options in chronological order, with no browser date arithmetic. */
unsigned tba_period_options(int target, int driver, unsigned frequency, int fiscal)
{
    tba_period_choice_count = 0;
    if (!frequency || frequency > 4 || !tba_period_end(target, 1, 0))
        return 0;
    int limit = driver > target && tba_period_end(driver, 1, 0) ? driver :
                tba_period_add(target, frequency, 24, fiscal);
    int cursor = tba_period_add(target, frequency, 1, fiscal);
    while (cursor && cursor <= limit && tba_period_choice_count < 1200) {
        tba_period_choices[tba_period_choice_count++] = cursor;
        int next = tba_period_add(cursor, frequency, 1, fiscal);
        if (next <= cursor)
            break;
        cursor = next;
    }
    return tba_period_choice_count;
}

/* Copy one civil-date option across the browser ABI. */
int tba_period_option(unsigned index)
{
    return index < tba_period_choice_count ? tba_period_choices[index] : 0;
}
