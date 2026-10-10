/**
 * @file lab_profile.c
 * @brief Calendar field ordering, defaults and persistence policy in C/WebAssembly.
 *
 * A directly indexed schema replaces repeated browser field lists. Ordered
 * date/range/year defaults preserve dependent fallbacks. Restore retains the
 * authored DateTime whitespace; capture normalises it. Fill only touches blank
 * defaultable controls; reset deliberately preserves town selection and the
 * almanac jurisdiction. Browser adapters still own DOM access and text transport.
 */
#include "lab_profile.h"

enum { lab_profile_trim = 1, lab_profile_fill = 2, lab_profile_reset = 4, lab_profile_time = 8 };

typedef struct {
    const char *key, *element, *initial;
    unsigned kind, fallback, flags;
} lab_profile_field_t;

static const lab_profile_field_t lab_profile_datetime[] = {
    {"date", "datetimeDate", "DEFAULT_DATETIME_DATE", 1, 0, lab_profile_fill | lab_profile_reset},
    {"jdn", "datetimeJdn", "", 0, 0, lab_profile_reset},
    {"start", "datetimeStart", "", 1, 1, lab_profile_fill | lab_profile_reset},
    {"end", "datetimeEnd", "", 1, 1, lab_profile_fill | lab_profile_reset},
    {"year", "datetimeYear", "", 0, 2, lab_profile_fill | lab_profile_reset},
    {"jurisdiction", "datetimeJurisdiction", "DEFAULT_DATETIME_JURISDICTION", 2, 0,
     lab_profile_fill | lab_profile_reset},
    {"town", "datetimeTown", "", 0, 0, 0},
    {"latitude", "datetimeLatitude", "DEFAULT_DATETIME_LATITUDE", 0, 0, lab_profile_fill | lab_profile_reset},
    {"longitude", "datetimeLongitude", "DEFAULT_DATETIME_LONGITUDE", 0, 0, lab_profile_fill | lab_profile_reset},
    {"elevation", "datetimeElevation", "DEFAULT_DATETIME_ELEVATION", 0, 0, lab_profile_fill | lab_profile_reset},
    {"gmt_offset", "datetimeGmtOffset", "DEFAULT_DATETIME_GMT_OFFSET", 0, 0, lab_profile_reset}};
static const lab_profile_field_t lab_profile_almanac[] = {
    {"date", "almanacDate", "DEFAULT_ALMANAC_DATE", 1, 0, lab_profile_fill | lab_profile_reset},
    {"time", "almanacTime", "DEFAULT_ALMANAC_TIME", 0, 0,
     lab_profile_trim | lab_profile_fill | lab_profile_reset | lab_profile_time},
    {"zone", "almanacZone", "DEFAULT_ALMANAC_ZONE", 0, 0, lab_profile_trim | lab_profile_fill | lab_profile_reset},
    {"jurisdiction", "almanacJurisdiction", "DEFAULT_DATETIME_JURISDICTION", 2, 0, lab_profile_fill},
    {"town", "almanacTown", "", 0, 0, 0},
    {"latitude", "almanacLatitude", "DEFAULT_ALMANAC_LATITUDE", 0, 0,
     lab_profile_trim | lab_profile_fill | lab_profile_reset},
    {"longitude", "almanacLongitude", "DEFAULT_ALMANAC_LONGITUDE", 0, 0,
     lab_profile_trim | lab_profile_fill | lab_profile_reset},
    {"elevation", "almanacElevation", "DEFAULT_ALMANAC_ELEVATION", 0, 0,
     lab_profile_trim | lab_profile_fill | lab_profile_reset},
    {"visibility", "", "DEFAULT_ALMANAC_VISIBILITY", 3, 0, lab_profile_fill | lab_profile_reset}};

/* Expose only the two calendar schemas. */
unsigned lab_profile_count(unsigned mode)
{
    return mode == 5   ? sizeof lab_profile_datetime / sizeof *lab_profile_datetime
           : mode == 6 ? sizeof lab_profile_almanac / sizeof *lab_profile_almanac
                       : 0;
}

static const lab_profile_field_t *lab_profile_field(unsigned mode, unsigned field)
{
    if (field >= lab_profile_count(mode))
        return 0;
    return mode == 5 ? &lab_profile_datetime[field] : &lab_profile_almanac[field];
}

/* Strings have module lifetime and cannot be changed by the host. */
const char *lab_profile_text(unsigned mode, unsigned field, unsigned part)
{
    const lab_profile_field_t *entry = lab_profile_field(mode, field);
    if (!entry)
        return "";
    return part == 0 ? entry->key : part == 1 ? entry->element : part == 2 ? entry->initial : "";
}

/* This bounded literal-length loop is not a schema lookup or text parser. */
unsigned lab_profile_text_length(unsigned mode, unsigned field, unsigned part)
{
    const char *text = lab_profile_text(mode, field, part);
    unsigned length = 0;
    while (text[length])
        ++length;
    return length;
}

/* Classification is applied by the host using existing native validators. */
unsigned lab_profile_kind(unsigned mode, unsigned field)
{
    const lab_profile_field_t *entry = lab_profile_field(mode, field);
    return entry ? entry->kind : 0;
}

/* Date and year dependencies refer only to a previously resolved field. */
unsigned lab_profile_fallback(unsigned mode, unsigned field)
{
    const lab_profile_field_t *entry = lab_profile_field(mode, field);
    return entry ? entry->fallback : 0;
}

/* Choose values without retaining host data or reinterpreting authored text. */
int lab_profile_choose(unsigned mode, unsigned field, unsigned operation, int present, int nonblank, int valid)
{
    const lab_profile_field_t *entry = lab_profile_field(mode, field);
    if (!entry || operation > 3)
        return -1;
    if (operation == 3)
        return 2;
    if (operation == 2 && entry->kind != 3)
        return present ? 0 : 2;
    if (entry->kind)
        return valid ? 1 : 2;
    if (operation == 0 && (entry->flags & lab_profile_time) && !nonblank)
        return 2;
    if (!present)
        return 2;
    return operation == 1 || (entry->flags & lab_profile_trim) ? 1 : 0;
}

/* Towns require asynchronous guarded catalogue restoration, never a direct write. */
int lab_profile_write(unsigned mode, unsigned field, unsigned operation)
{
    const lab_profile_field_t *entry = lab_profile_field(mode, field);
    if (!entry || operation > 3)
        return 0;
    if (operation == 2)
        return !!(entry->flags & lab_profile_fill);
    if (operation == 3)
        return !!(entry->flags & lab_profile_reset);
    return operation == 0 && field != (mode == 5 ? 6u : 4u);
}

/* Preserve edited DateTime offsets and offsets supplied by named towns. */
int lab_profile_accept_offset(unsigned mode, int town_applied, int touched, int present, int automatic, int suggested)
{
    return !town_applied && (mode == 5 ? (!touched || !present || automatic) : mode == 6 && suggested);
}

/* Event actions are directly indexed, matching the field schema order. */
unsigned lab_profile_event(unsigned mode, unsigned field)
{
    static const unsigned char datetime[] = {19, 0, 1, 1, 0, 48, 4, 8, 8, 8, 0};
    static const unsigned char almanac[] = {17, 0, 0, 48, 4, 8, 8, 8, 0};
    if (field >= lab_profile_count(mode))
        return 0;
    return mode == 5 ? datetime[field] : almanac[field];
}
