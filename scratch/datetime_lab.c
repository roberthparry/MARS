#include <ctype.h>
#include <float.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "datetime.h"
#include "ustring.h"

typedef struct datetime_lab_options_t {
    short date_year;
    month_t date_month;
    uint8_t date_day;
    short start_year;
    month_t start_month;
    uint8_t start_day;
    short end_year;
    month_t end_month;
    uint8_t end_day;
    int year;
    double latitude;
    double longitude;
    double gmt_offset;
} datetime_lab_options_t;

static bool parse_date_text(const char *text, short *year, month_t *month, uint8_t *day)
{
    int y;
    int m;
    int d;
    char tail;

    if (!text || !year || !month || !day)
        return false;
    if (sscanf(text, "%d-%d-%d%c", &y, &m, &d, &tail) != 3)
        return false;
    if (y < 1 || y > 9999 || m < 1 || m > 12 || d < 1 || d > 31)
        return false;
    if (!datetime_valid_ymd((short)y, (month_t)m, (uint8_t)d))
        return false;

    *year = (short)y;
    *month = (month_t)m;
    *day = (uint8_t)d;
    return true;
}

static bool parse_double_text(const char *text, double *out)
{
    char *end = NULL;
    double value;

    if (!text || !out)
        return false;
    while (isspace((unsigned char)*text))
        text++;
    if (*text == '\0')
        return false;

    value = strtod(text, &end);
    if (end == text || !isfinite(value))
        return false;
    while (isspace((unsigned char)*end))
        end++;
    if (*end != '\0')
        return false;

    *out = value;
    return true;
}

static bool parse_int_text(const char *text, int *out)
{
    char *end = NULL;
    long value;

    if (!text || !out)
        return false;
    while (isspace((unsigned char)*text))
        text++;
    if (*text == '\0')
        return false;

    value = strtol(text, &end, 10);
    if (end == text || value < 1 || value > 9999)
        return false;
    while (isspace((unsigned char)*end))
        end++;
    if (*end != '\0')
        return false;

    *out = (int)value;
    return true;
}

static bool key_equals(const char *got, size_t got_len, const char *want)
{
    return strlen(want) == got_len && strncmp(got, want, got_len) == 0;
}

static void set_today(datetime_lab_options_t *options)
{
    time_t now;
    struct tm local_tm;

    time(&now);
    local_tm = *localtime(&now);
    options->date_year = (short)(local_tm.tm_year + 1900);
    options->date_month = (month_t)(local_tm.tm_mon + 1);
    options->date_day = (uint8_t)local_tm.tm_mday;
}

static void init_defaults(datetime_lab_options_t *options)
{
    set_today(options);
    options->start_year = options->date_year;
    options->start_month = options->date_month;
    options->start_day = options->date_day;
    options->end_year = options->date_year;
    options->end_month = options->date_month;
    options->end_day = options->date_day;
    options->year = options->date_year;
    options->latitude = 51.5074;
    options->longitude = -0.1278;
    options->gmt_offset = DBL_MAX;
}

static bool apply_arg(datetime_lab_options_t *options, const char *arg)
{
    const char *equals = strchr(arg, '=');
    const char *value;
    size_t key_len;
    short y;
    month_t m;
    uint8_t d;
    double number;
    int integer;

    if (!equals)
        return false;

    key_len = (size_t)(equals - arg);
    value = equals + 1;

    if (key_equals(arg, key_len, "date")) {
        if (!parse_date_text(value, &y, &m, &d))
            return false;
        options->date_year = y;
        options->date_month = m;
        options->date_day = d;
        return true;
    }
    if (key_equals(arg, key_len, "start")) {
        if (!parse_date_text(value, &y, &m, &d))
            return false;
        options->start_year = y;
        options->start_month = m;
        options->start_day = d;
        return true;
    }
    if (key_equals(arg, key_len, "end")) {
        if (!parse_date_text(value, &y, &m, &d))
            return false;
        options->end_year = y;
        options->end_month = m;
        options->end_day = d;
        return true;
    }
    if (key_equals(arg, key_len, "year")) {
        if (!parse_int_text(value, &integer))
            return false;
        options->year = integer;
        return true;
    }
    if (key_equals(arg, key_len, "lat") || key_equals(arg, key_len, "latitude")) {
        if (!parse_double_text(value, &number) || number < -90.0 || number > 90.0)
            return false;
        options->latitude = number;
        return true;
    }
    if (key_equals(arg, key_len, "lon") || key_equals(arg, key_len, "longitude")) {
        if (!parse_double_text(value, &number) || number < -180.0 || number > 180.0)
            return false;
        options->longitude = number;
        return true;
    }
    if (key_equals(arg, key_len, "gmt_offset")) {
        if (*value == '\0') {
            options->gmt_offset = DBL_MAX;
            return true;
        }
        if (!parse_double_text(value, &number) || number < -14.0 || number > 14.0)
            return false;
        options->gmt_offset = number;
        return true;
    }

    return false;
}

static char *format_date(const datetime_t *dttm)
{
    string_t *format = string_new_with("%yyyy-%mm-%dd");
    string_t *text = format ? datetime_format_text(dttm, format) : NULL;
    const char *c_text = text ? string_c_str(text) : NULL;
    char *out = NULL;

    if (c_text) {
        size_t len = strlen(c_text);
        out = malloc(len + 1u);
        if (out)
            memcpy(out, c_text, len + 1u);
    }

    string_free(text);
    string_free(format);
    return out;
}

static char *format_datetime_minutes(const datetime_t *dttm)
{
    string_t *format = string_new_with("%yyyy-%mm-%dd @Hh:@mm");
    string_t *text = format ? datetime_format_text(dttm, format) : NULL;
    const char *c_text = text ? string_c_str(text) : NULL;
    char *out = NULL;

    if (c_text) {
        size_t len = strlen(c_text);
        out = malloc(len + 1u);
        if (out)
            memcpy(out, c_text, len + 1u);
    }

    string_free(text);
    string_free(format);
    return out;
}

static void print_date_field(const char *name, const datetime_t *dttm)
{
    char *text = format_date(dttm);
    printf("%s %s\n", name, text ? text : "unavailable");
    free(text);
}

static void print_time_field(const char *name, const datetime_t *dttm)
{
    char *text = format_datetime_minutes(dttm);
    printf("%s %s\n", name, text ? text : "unavailable");
    free(text);
}

static void print_optional_date_field(const char *name, const datetime_t *dttm)
{
    if (!dttm) {
        printf("%s unavailable\n", name);
        return;
    }
    print_date_field(name, dttm);
}

static void print_optional_start_gmt_field(const char *name,
                                           const datetime_t *observance,
                                           double latitude,
                                           double longitude)
{
    datetime_t *storage;
    datetime_t *start;

    if (!observance) {
        printf("%s unavailable\n", name);
        return;
    }

    storage = datetime_alloc();
    start = datetime_init_sunset_observance_start(storage,
                                                  observance,
                                                  latitude,
                                                  longitude,
                                                  0.0);
    if (!start) {
        datetime_dealloc(storage);
        printf("%s unavailable\n", name);
        return;
    }

    print_time_field(name, start);
    datetime_dealloc(start);
}

static void print_bank_holidays_between(const datetime_t *start, const datetime_t *end)
{
    datetime_holiday_list_t holidays;

    if (!datetime_english_bank_holidays_between(start, end, &holidays)) {
        printf("bank_holidays unavailable\n");
        return;
    }

    for (unsigned short i = 0; i < holidays.count; i++) {
        const datetime_holiday_t *holiday = &holidays.items[i];
        datetime_t *date = datetime_init_ymd(datetime_alloc(),
                                             holiday->year,
                                             holiday->month,
                                             holiday->day);
        char *text = format_date(date);
        printf("bank_holiday %s: %s\n", holiday->name, text ? text : "unavailable");
        free(text);
        datetime_dealloc(date);
    }
}

static void print_sun_time_field(const char *name,
                                 long jdn,
                                 double latitude,
                                 double longitude,
                                 double gmt_offset,
                                 bool sunrise)
{
    double raw = datetime_sun_time(jdn, latitude, longitude, sunrise);
    datetime_t *dttm;

    if (raw == -1.0) {
        printf("%s unavailable\n", name);
        printf("%s_status sun never rises\n", name);
        return;
    }
    if (raw == -2.0) {
        printf("%s unavailable\n", name);
        printf("%s_status sun never sets\n", name);
        return;
    }
    if (raw < 0.0) {
        printf("%s unavailable\n", name);
        printf("%s_status unavailable\n", name);
        return;
    }

    dttm = sunrise
        ? datetime_init_sunrise(datetime_alloc(), jdn, latitude, longitude, gmt_offset)
        : datetime_init_sunset(datetime_alloc(), jdn, latitude, longitude, gmt_offset);
    if (!dttm) {
        printf("%s unavailable\n", name);
        printf("%s_status unavailable\n", name);
        return;
    }

    print_time_field(name, dttm);
    printf("%s_status ok\n", name);
    datetime_dealloc(dttm);
}

int main(int argc, char **argv)
{
    datetime_lab_options_t options;
    datetime_t *date;
    datetime_t *start;
    datetime_t *end;
    datetime_t *easter;
    datetime_t *orthodox_easter;
    datetime_t *christmas;
    datetime_t *orthodox_christmas;
    datetime_t *chinese_new_year;
    datetime_t *diwali;
    datetime_t *holi;
    datetime_t *hindu_new_year;
    datetime_t *buddhist_new_year;
    datetime_t *vesak;
    datetime_t *asalha_puja;
    datetime_t *ramadan;
    datetime_t *eid_al_fitr;
    datetime_t *muslim_new_year;
    datetime_t *passover;
    datetime_t *jewish_new_year;
    datetime_span_t span;
    double days_between;
    double solar_declination;
    double solar_max_altitude;
    double solar_inclination;
    long jdn;

    init_defaults(&options);

    for (int i = 1; i < argc; i++) {
        if (!apply_arg(&options, argv[i])) {
            fprintf(stderr, "Bad datetime argument: %s\n", argv[i]);
            return 2;
        }
    }

    date = datetime_alloc();
    start = datetime_alloc();
    end = datetime_alloc();
    easter = datetime_alloc();
    orthodox_easter = datetime_alloc();
    christmas = datetime_alloc();
    orthodox_christmas = datetime_alloc();
    chinese_new_year = datetime_alloc();
    diwali = datetime_alloc();
    holi = datetime_alloc();
    hindu_new_year = datetime_alloc();
    buddhist_new_year = datetime_alloc();
    vesak = datetime_alloc();
    asalha_puja = datetime_alloc();
    ramadan = datetime_alloc();
    eid_al_fitr = datetime_alloc();
    muslim_new_year = datetime_alloc();
    passover = datetime_alloc();
    jewish_new_year = datetime_alloc();
    if (!date || !start || !end || !easter || !orthodox_easter ||
        !christmas || !orthodox_christmas ||
        !chinese_new_year || !diwali || !holi || !hindu_new_year ||
        !buddhist_new_year || !vesak || !asalha_puja ||
        !ramadan || !eid_al_fitr ||
        !muslim_new_year || !passover || !jewish_new_year) {
        fprintf(stderr, "Datetime allocation failed\n");
        datetime_dealloc(date);
        datetime_dealloc(start);
        datetime_dealloc(end);
        datetime_dealloc(easter);
        datetime_dealloc(orthodox_easter);
        datetime_dealloc(christmas);
        datetime_dealloc(orthodox_christmas);
        datetime_dealloc(chinese_new_year);
        datetime_dealloc(diwali);
        datetime_dealloc(holi);
        datetime_dealloc(hindu_new_year);
        datetime_dealloc(buddhist_new_year);
        datetime_dealloc(vesak);
        datetime_dealloc(asalha_puja);
        datetime_dealloc(ramadan);
        datetime_dealloc(eid_al_fitr);
        datetime_dealloc(muslim_new_year);
        datetime_dealloc(passover);
        datetime_dealloc(jewish_new_year);
        return 1;
    }
    datetime_init_ymdt(date, options.date_year, options.date_month, options.date_day, 12, 0, 0.0);
    datetime_init_ymd(start, options.start_year, options.start_month, options.start_day);
    datetime_init_ymd(end, options.end_year, options.end_month, options.end_day);
    {
        datetime_t *easter_storage = easter;
        datetime_t *orthodox_storage = orthodox_easter;
        datetime_t *christmas_storage = christmas;
        datetime_t *orthodox_christmas_storage = orthodox_christmas;
        datetime_t *cny_storage = chinese_new_year;
        datetime_t *diwali_storage = diwali;
        datetime_t *holi_storage = holi;
        datetime_t *hindu_new_year_storage = hindu_new_year;
        datetime_t *buddhist_new_year_storage = buddhist_new_year;
        datetime_t *vesak_storage = vesak;
        datetime_t *asalha_puja_storage = asalha_puja;
        datetime_t *ramadan_storage = ramadan;
        datetime_t *eid_storage = eid_al_fitr;
        datetime_t *muslim_storage = muslim_new_year;
        datetime_t *passover_storage = passover;
        datetime_t *jewish_storage = jewish_new_year;
        easter = datetime_init_easter(easter_storage, options.year);
        orthodox_easter = datetime_init_orthodox_easter(orthodox_storage, options.year);
        christmas = datetime_init_christmas(christmas_storage, options.year);
        orthodox_christmas = datetime_init_orthodox_christmas(orthodox_christmas_storage, options.year);
        chinese_new_year = datetime_init_chinese_new_year(cny_storage, options.year);
        diwali = datetime_init_diwali(diwali_storage, options.year);
        holi = datetime_init_holi(holi_storage, options.year);
        hindu_new_year = datetime_init_hindu_new_year(hindu_new_year_storage, options.year);
        buddhist_new_year = datetime_init_buddhist_new_year(buddhist_new_year_storage, options.year);
        vesak = datetime_init_vesak(vesak_storage, options.year);
        asalha_puja = datetime_init_asalha_puja(asalha_puja_storage, options.year);
        ramadan = datetime_init_ramadan(ramadan_storage, options.year);
        eid_al_fitr = datetime_init_eid_al_fitr(eid_storage, options.year);
        muslim_new_year = datetime_init_muslim_new_year(muslim_storage, options.year);
        passover = datetime_init_passover(passover_storage, options.year);
        jewish_new_year = datetime_init_jewish_new_year(jewish_storage, options.year);
        if (!easter)
            datetime_dealloc(easter_storage);
        if (!orthodox_easter)
            datetime_dealloc(orthodox_storage);
        if (!christmas)
            datetime_dealloc(christmas_storage);
        if (!orthodox_christmas)
            datetime_dealloc(orthodox_christmas_storage);
        if (!chinese_new_year)
            datetime_dealloc(cny_storage);
        if (!diwali)
            datetime_dealloc(diwali_storage);
        if (!holi)
            datetime_dealloc(holi_storage);
        if (!hindu_new_year)
            datetime_dealloc(hindu_new_year_storage);
        if (!buddhist_new_year)
            datetime_dealloc(buddhist_new_year_storage);
        if (!vesak)
            datetime_dealloc(vesak_storage);
        if (!asalha_puja)
            datetime_dealloc(asalha_puja_storage);
        if (!ramadan)
            datetime_dealloc(ramadan_storage);
        if (!eid_al_fitr)
            datetime_dealloc(eid_storage);
        if (!muslim_new_year)
            datetime_dealloc(muslim_storage);
        if (!passover)
            datetime_dealloc(passover_storage);
        if (!jewish_new_year)
            datetime_dealloc(jewish_storage);
    }
    if (!easter || !orthodox_easter) {
        fprintf(stderr, "Datetime calculation failed\n");
        datetime_dealloc(date);
        datetime_dealloc(start);
        datetime_dealloc(end);
        return 1;
    }

    jdn = datetime_jdn(date);
    days_between = datetime_duration(end, start, &span);
    solar_declination = datetime_solar_declination(date);
    solar_max_altitude = datetime_solar_max_altitude(date, options.latitude);
    solar_inclination = datetime_solar_inclination(date, options.latitude);

    print_date_field("date", date);
    printf("weekday %s\n", datetime_weekday_name(datetime_weekday(date)));
    printf("julian_day_number %ld\n", jdn);
    printf("moon_phase %s\n", datetime_moon_phase_name(datetime_moon_phase(date)));
    printf("solar_declination %.10g\n", solar_declination);
    printf("solar_max_altitude %.10g\n", solar_max_altitude);
    printf("solar_inclination %.10g\n", solar_inclination);
    printf("latitude %.10g\n", options.latitude);
    printf("longitude %.10g\n", options.longitude);
    if (options.gmt_offset == DBL_MAX)
        printf("gmt_offset local\n");
    else
        printf("gmt_offset %.10g\n", options.gmt_offset);

    print_sun_time_field("sunrise", jdn, options.latitude, options.longitude, options.gmt_offset, true);
    print_sun_time_field("sunset", jdn, options.latitude, options.longitude, options.gmt_offset, false);

    print_date_field("start", start);
    print_date_field("end", end);
    printf("days_between %.17g\n", days_between);
    printf("days_between_abs %.17g\n", fabs(days_between));
    printf("duration_years %u\n", span.years);
    printf("duration_months %u\n", span.months);
    printf("duration_days %u\n", span.days);

    print_date_field("easter", easter);
    print_date_field("orthodox_easter", orthodox_easter);
    print_optional_date_field("christmas", christmas);
    print_optional_date_field("orthodox_christmas", orthodox_christmas);
    print_optional_date_field("chinese_new_year", chinese_new_year);
    print_optional_date_field("diwali", diwali);
    print_optional_date_field("holi", holi);
    print_optional_date_field("hindu_new_year", hindu_new_year);
    print_optional_date_field("buddhist_new_year", buddhist_new_year);
    print_optional_date_field("vesak", vesak);
    print_optional_date_field("asalha_puja", asalha_puja);
    print_optional_date_field("ramadan", ramadan);
    print_optional_start_gmt_field("ramadan_starts_gmt", ramadan, options.latitude, options.longitude);
    print_optional_date_field("eid_al_fitr", eid_al_fitr);
    print_optional_start_gmt_field("eid_al_fitr_starts_gmt", eid_al_fitr, options.latitude, options.longitude);
    print_optional_date_field("muslim_new_year", muslim_new_year);
    print_optional_start_gmt_field("muslim_new_year_starts_gmt", muslim_new_year, options.latitude, options.longitude);
    print_optional_date_field("passover", passover);
    print_optional_start_gmt_field("passover_starts_gmt", passover, options.latitude, options.longitude);
    print_optional_date_field("jewish_new_year", jewish_new_year);
    print_optional_start_gmt_field("jewish_new_year_starts_gmt", jewish_new_year, options.latitude, options.longitude);
    print_bank_holidays_between(start, end);

    datetime_dealloc(date);
    datetime_dealloc(start);
    datetime_dealloc(end);
    datetime_dealloc(easter);
    datetime_dealloc(orthodox_easter);
    datetime_dealloc(christmas);
    datetime_dealloc(orthodox_christmas);
    datetime_dealloc(chinese_new_year);
    datetime_dealloc(diwali);
    datetime_dealloc(holi);
    datetime_dealloc(hindu_new_year);
    datetime_dealloc(buddhist_new_year);
    datetime_dealloc(vesak);
    datetime_dealloc(asalha_puja);
    datetime_dealloc(ramadan);
    datetime_dealloc(eid_al_fitr);
    datetime_dealloc(muslim_new_year);
    datetime_dealloc(passover);
    datetime_dealloc(jewish_new_year);
    return 0;
}
