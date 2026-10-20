/**
 * @file jurisdiction_rivers.c
 * @brief Connect jurisdiction towns and independent configured globe geometry.
 *
 * Supplies a borrowed town connection without exposing credentials or private jurisdiction implementation.
 * Geometry is owned separately by the globe controller; missing geometry does not disable town queries.
 */
#include "jurisdiction-t.h"
#define MARS_JURISDICTION_INTERNAL_ACCESS
#include "jurisdiction_internal.h"

/* Borrow towns and open independently configured geometry, without legacy geometry fallback. */
globe_t *jurisdict_globe_open(jurisdiction_t *jurisdiction)
{
    if (!jurisdiction) return NULL;
    globe_t *globe = globe_open_configured(jurisdiction->db);
    return globe ? globe : globe_open_databases(NULL, NULL, jurisdiction->db);
}
