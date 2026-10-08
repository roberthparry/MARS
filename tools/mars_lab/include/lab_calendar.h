/**
 * @file lab_calendar.h
 * @brief Native MARS Lab calendar and navigation route adapter.
 *
 * Accepts the browser's DateTime, weather, jurisdiction and almanac payloads.
 * Results are owned JSON trees. Scratch executables run through lab_process;
 * database configuration remains the responsibility of the native backends.
 * Call from the repository root, or set MARS_LAB_ROOT to that directory.
 * Named-town conversions temporarily change TZ and restore it before returning;
 * this adapter requires the server's single-threaded prefork worker model.
 */
#ifndef MARS_LAB_CALENDAR_H
#define MARS_LAB_CALENDAR_H

#include "json.h"
#include "ustring.h"

/**
 * @brief Adapt a supported calendar route, returning a caller-owned JSON response.
 * @param route Borrowed exact route, including its leading slash.
 * @param payload Borrowed JSON object containing browser request fields.
 * @param status Optional HTTP status output, unchanged for unsupported routes.
 * @return Owned response released with json_free; NULL for an unsupported route or allocation failure.
 */
json_t *lab_cal_request(const string_t *route, const json_t *payload, unsigned *status);

#endif
