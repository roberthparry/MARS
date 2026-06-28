#ifndef MARS_ALMANAC_H
#define MARS_ALMANAC_H

#include <stdbool.h>

#include "array.h"
#include "datetime.h"

/**
 * @brief Opaque astronomical almanac engine backed by configured ephemeris data.
 *
 * An @c almanac_t owns the private resources needed to resolve catalogued stars,
 * planets, and Greenwich Hour Angle of Aries from a configured encrypted
 * almanac database.
 */
typedef struct _almanac_t almanac_t;

/**
 * @brief Catalogued body kind supported by the almanac.
 */
typedef enum _almanac_body_kind_t {
    ALMANAC_BODY_STAR = 0,
    ALMANAC_BODY_PLANET,
    ALMANAC_BODY_SUN,
    ALMANAC_BODY_MOON
} almanac_body_kind_t;

/**
 * @brief One computed almanac position.
 *
 * Values of this type are plain value objects. When returned inside an
 * @c array_t from @c almanac_snapshot(), the array owns the storage for the
 * array itself, while each element stores its own fixed-size text fields.
 */
typedef struct _almanac_entry_t {
    char body_code[32];
    char display_name[96];
    almanac_body_kind_t body_kind;
    double gha_aries_degrees;
    double sha_degrees;
    double declination_degrees;
    double right_ascension_hours;
    double geocentric_distance_au;
    double heliocentric_distance_au;
    double phase_angle_degrees;
    double visual_magnitude;
} almanac_entry_t;

/**
 * @brief Open the configured almanac engine.
 *
 * The engine resolves its own configured backing store, using
 * @c MARS_ALMANAC_DB_PATH and @c MARS_ALMANAC_DB_KEY when available, otherwise
 * falling back to @c ~/.mars/almanac/almanac.db and any configured key file.
 *
 * @return newly allocated almanac engine, or @c NULL on failure.
 */
almanac_t *almanac_open(void);

/**
 * @brief Destroy an almanac engine.
 *
 * @param almanac almanac engine to destroy, or @c NULL.
 */
void almanac_close(almanac_t *almanac);

/**
 * @brief Return the last error message recorded by the almanac engine.
 *
 * The returned pointer is borrowed from the engine and remains valid until the
 * next almanac API call on the same engine or until @c almanac_close().
 *
 * @param almanac almanac engine to query.
 * @return borrowed error string, or @c NULL when unavailable.
 */
const char *almanac_last_error(const almanac_t *almanac);

/**
 * @brief Compute Greenwich Hour Angle of Aries for a moment.
 *
 * @param almanac open almanac engine.
 * @param moment civil moment to evaluate.
 * @param gha_aries_degrees output pointer for GHA Aries in degrees [0, 360).
 * @return @c true on success, otherwise @c false.
 */
bool almanac_gha_aries(almanac_t *almanac,
                       const datetime_t *moment,
                       double *gha_aries_degrees);

/**
 * @brief Compute SHA and declination for one catalogued body.
 *
 * The returned entry includes the matching GHA Aries for the same moment.
 *
 * @param almanac open almanac engine.
 * @param body_code body code such as @c SUN, @c MARS, or @c SIRIUS.
 * @param moment civil moment to evaluate.
 * @param out output entry to fill.
 * @return @c true on success, otherwise @c false.
 */
bool almanac_lookup(almanac_t *almanac,
                    const char *body_code,
                    const datetime_t *moment,
                    almanac_entry_t *out);

/**
 * @brief Compute SHA and declination for every enabled catalogued body.
 *
 * The returned array contains @c almanac_entry_t values sorted by configured
 * catalog order. Destroy the returned array with @c array_destroy() when
 * finished.
 *
 * @param almanac open almanac engine.
 * @param moment civil moment to evaluate.
 * @return newly allocated array of @c almanac_entry_t values, or @c NULL on
 *         failure.
 */
array_t *almanac_snapshot(almanac_t *almanac, const datetime_t *moment);

/**
 * @brief Serialise an almanac engine into a SQLite-ready payload.
 *
 * The payload records that the configured almanac engine should be reopened;
 * it does not attempt to serialise private database handles or cached model
 * state. On success, the caller owns @p out_type, @p out_encoding, and
 * @p out_data and must release them with @c string_free() and @c free().
 *
 * @param almanac Almanac engine to serialise.
 * @param out_type Receives a newly allocated type label.
 * @param out_encoding Receives a newly allocated encoding label.
 * @param out_data Receives a newly allocated payload buffer.
 * @param out_len Receives the payload length in bytes.
 * @return @c true on success, otherwise @c false.
 */
bool almanac_serialize(const almanac_t *almanac,
                       string_t **out_type,
                       string_t **out_encoding,
                       void **out_data,
                       size_t *out_len);

/**
 * @brief Reconstruct an almanac engine from a serialised payload.
 *
 * @param data Serialised payload bytes.
 * @param len Payload length in bytes.
 * @param type Stored type label.
 * @param encoding Stored encoding label.
 * @return Newly allocated almanac engine on success, otherwise @c NULL.
 */
almanac_t *almanac_deserialise(const void *data,
                               size_t len,
                               const string_t *type,
                               const string_t *encoding);

#endif /* MARS_ALMANAC_H */
