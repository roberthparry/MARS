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
#include <string.h>

#include "lab_mode.h"
#include "lab_profile.h"

enum { lab_profile_trim = 1, lab_profile_fill = 2, lab_profile_reset = 4, lab_profile_time = 8 };

typedef struct {
    const char *key, *element, *initial;
    lab_profile_kind_t kind;
    lab_profile_fallback_t fallback;
    unsigned flags;
} lab_profile_field_t;

static const lab_profile_field_t lab_profile_datetime[] = {
    {"date", "datetimeDate", "DEFAULT_DATETIME_DATE", LAB_PROFILE_DATE, LAB_PROFILE_DEFAULT,
     lab_profile_fill | lab_profile_reset},
    {"jdn", "datetimeJdn", "", LAB_PROFILE_TEXT, LAB_PROFILE_DEFAULT, lab_profile_reset},
    {"start", "datetimeStart", "", LAB_PROFILE_DATE, LAB_PROFILE_RESOLVED_DATE, lab_profile_fill | lab_profile_reset},
    {"end", "datetimeEnd", "", LAB_PROFILE_DATE, LAB_PROFILE_RESOLVED_DATE, lab_profile_fill | lab_profile_reset},
    {"year", "datetimeYear", "", LAB_PROFILE_TEXT, LAB_PROFILE_RESOLVED_YEAR, lab_profile_fill | lab_profile_reset},
    {"jurisdiction", "datetimeJurisdiction", "DEFAULT_DATETIME_JURISDICTION", LAB_PROFILE_JURISDICTION,
     LAB_PROFILE_DEFAULT, lab_profile_fill | lab_profile_reset},
    {"town", "datetimeTown", "", LAB_PROFILE_TEXT, LAB_PROFILE_DEFAULT, 0},
    {"latitude", "datetimeLatitude", "DEFAULT_DATETIME_LATITUDE", LAB_PROFILE_TEXT, LAB_PROFILE_DEFAULT,
     lab_profile_fill | lab_profile_reset},
    {"longitude", "datetimeLongitude", "DEFAULT_DATETIME_LONGITUDE", LAB_PROFILE_TEXT, LAB_PROFILE_DEFAULT,
     lab_profile_fill | lab_profile_reset},
    {"elevation", "datetimeElevation", "DEFAULT_DATETIME_ELEVATION", LAB_PROFILE_TEXT, LAB_PROFILE_DEFAULT,
     lab_profile_fill | lab_profile_reset},
    {"gmt_offset", "datetimeGmtOffset", "DEFAULT_DATETIME_GMT_OFFSET", LAB_PROFILE_TEXT, LAB_PROFILE_DEFAULT,
     lab_profile_reset},
};
static const lab_profile_field_t lab_profile_almanac[] = {
    {"date", "almanacDate", "DEFAULT_ALMANAC_DATE", LAB_PROFILE_DATE, LAB_PROFILE_DEFAULT,
     lab_profile_fill | lab_profile_reset},
    {"time", "almanacTime", "DEFAULT_ALMANAC_TIME", LAB_PROFILE_TEXT, LAB_PROFILE_DEFAULT,
     lab_profile_trim | lab_profile_fill | lab_profile_reset | lab_profile_time},
    {"zone", "almanacZone", "DEFAULT_ALMANAC_ZONE", LAB_PROFILE_TEXT, LAB_PROFILE_DEFAULT,
     lab_profile_trim | lab_profile_fill | lab_profile_reset},
    {"jurisdiction", "almanacJurisdiction", "DEFAULT_DATETIME_JURISDICTION", LAB_PROFILE_JURISDICTION,
     LAB_PROFILE_DEFAULT, lab_profile_fill},
    {"town", "almanacTown", "", LAB_PROFILE_TEXT, LAB_PROFILE_DEFAULT, 0},
    {"latitude", "almanacLatitude", "DEFAULT_ALMANAC_LATITUDE", LAB_PROFILE_TEXT, LAB_PROFILE_DEFAULT,
     lab_profile_trim | lab_profile_fill | lab_profile_reset},
    {"longitude", "almanacLongitude", "DEFAULT_ALMANAC_LONGITUDE", LAB_PROFILE_TEXT, LAB_PROFILE_DEFAULT,
     lab_profile_trim | lab_profile_fill | lab_profile_reset},
    {"elevation", "almanacElevation", "DEFAULT_ALMANAC_ELEVATION", LAB_PROFILE_TEXT, LAB_PROFILE_DEFAULT,
     lab_profile_trim | lab_profile_fill | lab_profile_reset},
    {"visibility", "", "DEFAULT_ALMANAC_VISIBILITY", LAB_PROFILE_VISIBILITY, LAB_PROFILE_DEFAULT,
     lab_profile_fill | lab_profile_reset},
};

/* Expose only the two calendar schemas. */
unsigned lab_profile_count(lab_mode_t mode)
{
    return mode == LAB_MODE_DATETIME   ? sizeof lab_profile_datetime / sizeof *lab_profile_datetime
           : mode == LAB_MODE_ALMANAC ? sizeof lab_profile_almanac / sizeof *lab_profile_almanac
                                      : 0;
}

static const lab_profile_field_t *lab_profile_field(lab_mode_t mode, unsigned field)
{
    if (field >= lab_profile_count(mode))
        return 0;
    return mode == LAB_MODE_DATETIME ? &lab_profile_datetime[field] : &lab_profile_almanac[field];
}

/* Strings have module lifetime and cannot be changed by the host. */
const char *lab_profile_text(lab_mode_t mode, unsigned field, lab_profile_part_t part)
{
    const lab_profile_field_t *entry = lab_profile_field(mode, field);
    if (!entry)
        return "";
    return part == LAB_PROFILE_KEY     ? entry->key
           : part == LAB_PROFILE_ELEMENT ? entry->element
           : part == LAB_PROFILE_INITIAL ? entry->initial
                                         : "";
}

/* Measure a fixed literal, not user text or a schema lookup. */
unsigned lab_profile_text_length(lab_mode_t mode, unsigned field, lab_profile_part_t part)
{
    return (unsigned)strlen(lab_profile_text(mode, field, part));
}

/* Classification is applied by the host using existing native validators. */
lab_profile_kind_t lab_profile_kind(lab_mode_t mode, unsigned field)
{
    const lab_profile_field_t *entry = lab_profile_field(mode, field);
    return entry ? entry->kind : LAB_PROFILE_TEXT;
}

/* Date and year dependencies refer only to a previously resolved field. */
lab_profile_fallback_t lab_profile_fallback(lab_mode_t mode, unsigned field)
{
    const lab_profile_field_t *entry = lab_profile_field(mode, field);
    return entry ? entry->fallback : LAB_PROFILE_DEFAULT;
}

/* Choose values without retaining host data or reinterpreting authored text. */
lab_profile_choice_t lab_profile_choose(lab_mode_t mode, unsigned field, lab_calendar_operation_t operation,
                                        bool present, bool nonblank, bool valid)
{
    const lab_profile_field_t *entry = lab_profile_field(mode, field);
    if (!entry || (unsigned)operation > LAB_CALENDAR_RESET)
        return LAB_PROFILE_CHOICE_INVALID;
    if (operation == LAB_CALENDAR_RESET)
        return LAB_PROFILE_FALLBACK;
    if (operation == LAB_CALENDAR_FILL && entry->kind != LAB_PROFILE_VISIBILITY)
        return present ? LAB_PROFILE_RAW : LAB_PROFILE_FALLBACK;
    if (entry->kind != LAB_PROFILE_TEXT)
        return valid ? LAB_PROFILE_NORMALISED : LAB_PROFILE_FALLBACK;
    if (operation == LAB_CALENDAR_RESTORE && (entry->flags & lab_profile_time) && !nonblank)
        return LAB_PROFILE_FALLBACK;
    if (!present)
        return LAB_PROFILE_FALLBACK;
    return operation == LAB_CALENDAR_CAPTURE || (entry->flags & lab_profile_trim) ? LAB_PROFILE_NORMALISED : LAB_PROFILE_RAW;
}

/* Towns require asynchronous guarded catalogue restoration, never a direct write. */
bool lab_profile_write(lab_mode_t mode, unsigned field, lab_calendar_operation_t operation)
{
    const lab_profile_field_t *entry = lab_profile_field(mode, field);
    if (!entry || (unsigned)operation > LAB_CALENDAR_RESET)
        return false;
    if (operation == LAB_CALENDAR_FILL)
        return (entry->flags & lab_profile_fill) != 0;
    if (operation == LAB_CALENDAR_RESET)
        return (entry->flags & lab_profile_reset) != 0;
    return operation == LAB_CALENDAR_RESTORE && field != (mode == LAB_MODE_DATETIME ? 6u : 4u);
}

/* Preserve edited DateTime offsets and offsets supplied by named towns. */
bool lab_profile_accept_offset(lab_mode_t mode, bool town_applied, bool touched, bool present, bool automatic, bool suggested)
{
    return !town_applied &&
           (mode == LAB_MODE_DATETIME ? (!touched || !present || automatic) : mode == LAB_MODE_ALMANAC && suggested);
}

/* Event actions are directly indexed, matching the field schema order. */
unsigned lab_profile_event(lab_mode_t mode, unsigned field)
{
    static const unsigned char datetime[] = {19, 0, 1, 1, 0, 48, 4, 8, 8, 8, 0};
    static const unsigned char almanac[] = {17, 0, 0, 48, 4, 8, 8, 8, 0};
    if (field >= lab_profile_count(mode))
        return 0;
    return mode == LAB_MODE_DATETIME ? datetime[field] : almanac[field];
}
