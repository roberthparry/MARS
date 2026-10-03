#ifndef MARS_JURISDICTION_INTERNAL_H
#define MARS_JURISDICTION_INTERNAL_H

#include "jurisdiction.h"
#include "sqlite.h"
#include "ustring.h"

/* Private state and helpers shared only by jurisdiction implementation files. */
struct _jurisdiction_t {
    sqlite_t *db;
    string_t *error;
    char jurisdiction[32];
};

typedef struct lineage_row_t {
    char jurisdiction_id[32];
    int depth;
} lineage_row_t;

typedef struct jurisdiction_vec_t {
    void *items;
    size_t count;
    size_t capacity;
    size_t item_size;
} jurisdiction_vec_t;

typedef struct weekend_rule_t {
    char jurisdiction_id[32];
    int depth;
    char *weekend_mask;
    int valid_from_year;
    int valid_to_year;
} weekend_rule_t;

/* Engine-owned diagnostics, row storage and inherited jurisdiction lookup. */
void jurisdiction_set_error(jurisdiction_t *jurisdiction, const char *message);
char *jurisdiction_dup_c_string(const char *text);
void jurisdiction_vec_free(jurisdiction_vec_t *vec);
bool jurisdiction_vec_push(jurisdiction_vec_t *vec, const void *item);
bool jurisdiction_year_in_range(int year, int valid_from_year, int valid_to_year);
bool jurisdiction_load_lineage(sqlite_t *db, const char *jurisdiction, jurisdiction_vec_t *lineage_rows);

/* Calendar text is owned by the caller; row vectors must be released after use. */
bool jurisdiction_parse_date_text(const char *text, short *year, month_t *month, uint8_t *day);
char *jurisdiction_format_date(const datetime_t *dttm);
int jurisdiction_iso_weekday_from_datetime_weekday(int weekday);
int jurisdiction_iso_weekday_from_date_text(const char *text);
char *jurisdiction_date_text_add_days(const char *text, long days);
char *jurisdiction_normalise_weekend_mask(const char *mask);
bool jurisdiction_weekend_mask_contains(const char *mask, int weekday);
const char *jurisdiction_effective_weekend_mask_for_year(const weekend_rule_t *rules, size_t count, int year);
bool jurisdiction_load_weekend_rules(sqlite_t *db, const jurisdiction_vec_t *lineage_rows, jurisdiction_vec_t *weekend_rules);
void jurisdiction_free_weekend_rules(jurisdiction_vec_t *rows);


/* Owned database rows shared by holiday loading and occurrence assembly. */
typedef struct holiday_rule_row_t {
    int holiday_id;
    int rule_id;
    char jurisdiction_id[32];
    char *holiday_name;
    char *holiday_class;
    char *rule_kind;
    int month;
    int day;
    int weekday;
    int ordinal;
    int offset_days;
    char *holiday_date;
    char *expression_language;
    char *expression_text;
    int valid_from_year;
    int valid_to_year;
} holiday_rule_row_t;

typedef struct observance_rule_row_t {
    int holiday_id;
    int applies_to_rule_id;
    char *observed_rule_kind;
    char *observed_name;
    char *weekend_mask;
    int suppress_original;
    int valid_from_year;
    int valid_to_year;
} observance_rule_row_t;

typedef struct holiday_exception_row_t {
    int holiday_id;
    int target_rule_id;
    char *holiday_date;
    char *action;
    char *name;
    int valid_from_year;
    int valid_to_year;
} holiday_exception_row_t;

void jurisdiction_free_holiday_rule_rows(jurisdiction_vec_t *rows);
void jurisdiction_free_observance_rule_rows(jurisdiction_vec_t *rows);
void jurisdiction_free_exception_rows(jurisdiction_vec_t *rows);
bool jurisdiction_load_holiday_rules(sqlite_t *db, const jurisdiction_vec_t *lineage_rows, int start_year, int end_year,
                               jurisdiction_vec_t *rules);
bool jurisdiction_load_observance_rules(sqlite_t *db, const jurisdiction_vec_t *lineage_rows, jurisdiction_vec_t *observances);
bool jurisdiction_load_exceptions(sqlite_t *db, const jurisdiction_vec_t *lineage_rows, jurisdiction_vec_t *exceptions);
char *jurisdiction_evaluate_rule_date(sqlite_t *db, const holiday_rule_row_t *rule, int year, const char *jurisdiction);

/* Owned timezone rows shared by offset and transition queries. */
typedef struct timezone_era_row_t {
    int sequence_no;
    int gmtoff_minutes;
    char *rules_kind;
    int fixed_save_minutes;
    char *rule_name;
    char *until_day_kind;
    int until_year;
    int until_month;
    int until_day_value;
    int until_weekday;
    int until_seconds;
    char until_suffix;
} timezone_era_row_t;

typedef struct timezone_transition_rule_row_t {
    char *rule_name;
    int from_year;
    int to_year;
    int in_month;
    char *on_kind;
    int on_day;
    int on_weekday;
    int at_seconds;
    char at_suffix;
    int save_minutes;
} timezone_transition_rule_row_t;

void jurisdiction_free_timezone_era_rows(jurisdiction_vec_t *rows);
void jurisdiction_free_timezone_transition_rule_rows(jurisdiction_vec_t *rows);
bool jurisdiction_load_default_timezone_name(sqlite_t *db, const jurisdiction_vec_t *lineage_rows, char *timezone_name,
                                       size_t timezone_name_size);
bool jurisdiction_load_timezone_eras(sqlite_t *db, const char *timezone_name, jurisdiction_vec_t *rows);
bool jurisdiction_load_timezone_transition_rules(sqlite_t *db, const char *rule_name, jurisdiction_vec_t *rows);
bool jurisdiction_normalise_day_time(short *year, month_t *month, uint8_t *day, int *seconds);
const timezone_era_row_t *jurisdiction_select_timezone_era_for_date(const jurisdiction_vec_t *eras, const datetime_t *date);
bool jurisdiction_compute_transition_occurrence(const timezone_transition_rule_row_t *rule, int year, short *out_year,
                                          month_t *out_month, uint8_t *out_day, int *out_seconds);
int jurisdiction_compare_boundary_to_boundary(short left_year, month_t left_month, uint8_t left_day, int left_seconds,
                                        short right_year, month_t right_month, uint8_t right_day, int right_seconds);
bool jurisdiction_timezone_offset_for_name_on_date(sqlite_t *db, const char *timezone_name, const datetime_t *date,
                                             double *offset_hours);

#endif /* MARS_JURISDICTION_INTERNAL_H */
