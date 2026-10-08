/**
 * @file lab_calendar_internal.h
 * @brief Shared private facilities for the native Lab calendar adapters.
 *
 * Calendar-owned translation units share JSON ownership helpers, validated
 * request options, bounded scratch execution and UI presentation here.
 * The server should include lab_calendar.h instead.
 */
#ifndef MARS_LAB_CALENDAR_INTERNAL_H
#define MARS_LAB_CALENDAR_INTERNAL_H

#include <stdbool.h>
#include <time.h>

#include "lab_calendar.h"

/** @brief Resolve submitted town metadata or the packaged catalogue to an owned IANA zone name. */
string_t *lab_cal_town_timezone(const json_t *options);

/**
 * @brief Convert a UTC Julian date using a validated zoneinfo file, restoring TZ afterwards.
 * Invalid zones return false without modifying outputs. Only single-threaded workers may call this function.
 */
bool lab_cal_timezone_at(const string_t *zone, double jd, struct tm *local, double *offset);

/** @brief Resolve a civil date's noon offset in a validated IANA zone; preserve outputs on failure. */
bool lab_cal_timezone_noon(const string_t *zone, const string_t *date, double *offset);

/** @brief Borrow an object member by its ASCII key. */
const json_t *lab_cal_get(const json_t *object, const char *key);

/** @brief Borrow string or number text; absent and other values yield an empty string. */
const char *lab_cal_text(const json_t *object, const char *key);

/** @brief Parse a finite decimal using a string cursor, rejecting trailing syntax. */
bool lab_cal_number(const char *text, double *value);

/** @brief Split on a literal delimiter, preserving empty columns through string views. */
string_t **lab_cal_split(const string_t *text, const char *delimiter, size_t *count);

/** @brief Parse an unsigned decimal integer with a cursor and an explicit bound. */
bool lab_cal_integer(const char *text, unsigned maximum, unsigned *value);

/** @brief Copy a string into an object. */
void lab_cal_set(json_t *object, const char *key, const char *value);

/** @brief Copy a value into an object and release the supplied owned value. */
void lab_cal_take(json_t *object, const char *key, json_t *value);

/** @brief Append a copy of an owned array item and release it. */
void lab_cal_append(json_t *array, json_t *value);

/** @brief Build an error response and set its HTTP status. */
json_t *lab_cal_error(unsigned *status, unsigned code, const char *message);

/** @brief Run one native scratch helper with bounded output; return parsed fields on success. */
json_t *lab_cal_run(const char *name, const json_t *options, unsigned timeout, string_t **diagnostic);

/** @brief Read an environment variable or a named MARS configuration file value. */
string_t *lab_cal_config(const char *filename, const char *key);

/** @brief Add a browser label/value row. */
void lab_cal_row(json_t *rows, const char *label, const char *value);

/** @brief Add a browser section, copying the supplied rows. */
void lab_cal_section(json_t *sections, const char *title, bool open, const json_t *rows);

/** @brief Convert sections to their plain-text companion field. */
void lab_cal_sections(json_t *response, const char *name, json_t *sections);

/** @brief Prepare the DateTime cards from native fields. */
json_t *lab_cal_datetime(json_t *fields);

/** @brief Prepare a jurisdiction-location response from successful native fields and named-town metadata. */
json_t *lab_cal_location_fields(const json_t *fields, const json_t *options);

/** @brief Prepare weather cards using a bounded native HTTPS request. */
json_t *lab_cal_weather(const json_t *options);

/** @brief Prepare navigation body rows and upcoming events. */
json_t *lab_cal_almanac(json_t *fields, const json_t *options, unsigned *status);

/** @brief Search named land totality locations for requested events. */
json_t *lab_cal_totality(const json_t *payload, const json_t *options, unsigned *status);

#endif
