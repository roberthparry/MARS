/**
 * @file cfg_calendar_internal.h
 * @brief Private selection, translation and policy boundaries for calendar installation.
 *
 * Calendar implementation files and isolated calendar tests share these helpers.
 * Other installer modules use cfg_calendar.h; no private jurisdiction or SQLite
 * facilities are exposed here. Owned location fields are released together.
 */
#ifndef MARS_CFG_CALENDAR_INTERNAL_H
#define MARS_CFG_CALENDAR_INTERNAL_H

#include <time.h>

#include "array.h"
#include "cfg_calendar.h"

/** @brief Owned, fully resolved calendar location. */
typedef struct {
    string_t *name;
    string_t *jurisdiction;
    string_t *timezone;
    double latitude;
    double longitude;
} cfg_calendar_place_t;

/** @brief Saved environment value, including the distinction between unset and empty. */
typedef struct {
    const char *name;
    string_t *previous;
    bool present;
    bool changed;
} cfg_calendar_environment_t;

/** @brief Copy optional text and trim outer whitespace. */
string_t *cfg_calendar_value(const string_t *text);

/** @brief Test string equality against a literal. */
bool cfg_calendar_equal(const string_t *text, const char *literal);

/** @brief Reject embedded NUL characters at environment and SQL text boundaries. */
bool cfg_calendar_text_valid(const string_t *text);

/** @brief Parse an ASCII menu integer containing at most three digits. */
bool cfg_calendar_number(const string_t *text, unsigned *number);

/** @brief Save and replace an environment setting; restore using cfg_calendar_restore. */
bool cfg_calendar_environment(cfg_calendar_environment_t *state, const char *name, const string_t *value);

/** @brief Restore a changed environment setting, including its original unset state. */
bool cfg_calendar_restore(cfg_calendar_environment_t *state);

/** @brief Validate installed zoneinfo and enter its TZ scope. */
bool cfg_calendar_zone_enter(const string_t *name, cfg_calendar_environment_t *state);

/** @brief Return a selected-zone civil date's noon offset, without changing TZ. */
bool cfg_calendar_noon(int year, int month, int day, double *offset);

/** @brief Destroy the string fields of one owned location. */
void cfg_calendar_place_clear(void *place);

/** @brief Build the indexed temporary location catalogue. */
bool cfg_calendar_catalogue(sqlite_t *db);

/** @brief Resolve exact town aliases and optional jurisdiction to owned location records. */
array_t *cfg_calendar_matches(sqlite_t *db, const string_t *text);

/** @brief Search the finite catalogue on explicit discovery requests. */
array_t *cfg_calendar_search(sqlite_t *db, const string_t *text);

/** @brief Select an owned location, returning false on cancellation, ambiguity or invalid tzdata. */
bool cfg_calendar_choose_place(sqlite_t *db, const string_t *requested, const string_t *saved, bool interactive,
                               cfg_calendar_place_t *place);

/** @brief Select an owned supported locale from indexed language spellings. */
string_t *cfg_calendar_choose_language(sqlite_t *db, const cfg_calendar_place_t *place, const string_t *requested,
                                       const string_t *saved, bool interactive);

/** @brief Build indexed, precedence-aware native-event label translations. */
bool cfg_calendar_labels(sqlite_t *db, const string_t *jurisdiction, const string_t *locale);

/** @brief Expand native policies and atomically replace the validated daily calendar snapshot. */
bool cfg_calendar_snapshot(sqlite_t *db, const string_t *path, const string_t *key, const cfg_calendar_place_t *place,
                           const string_t *locale);

/** @brief Fill temporary events and caller-owned (last-first+1)*7 Monday-first weekend flags using prepared labels. */
bool cfg_calendar_policy(sqlite_t *db, const string_t *path, const string_t *key, const cfg_calendar_place_t *place,
                         int first_year, int last_year, bool *weekends);

#endif
