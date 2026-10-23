/**
 * @file lab_menu_index.h
 * @brief Native lookup indexes for town and other searchable option lists.
 *
 * Owns copied keys and normalised search text, with dictionary key lookup,
 * sorted coordinate indexes and a suffix index for substring queries. Browser
 * mutation versions invalidate snapshots without retaining DOM handles in C.
 * Each entry point dispatches synchronously through the service's private singleton. Browser handles
 * require the owning non-shared WASM instance; native dispatch serialises complete operations with a mutex.
 */
#ifndef LAB_MENU_INDEX_H
#define LAB_MENU_INDEX_H

#include <stdbool.h>

#include "lab_handle.h"

/** @brief Parse coordinates with the same strict numeric-input rules as location controls. */
double lab_menu_index_number(jsref value);

/** @brief Build normalised option search text, including jurisdiction aliases, without retaining handles. */
jsref lab_menu_index_search_text(node_jsref option, bool details);

/** @brief Return an option index by exact value, -1 if absent, or -2 if indexing is unavailable. */
int lab_menu_index_key(node_jsref select, jsref value);

/** @brief Match coordinates within 0.000001 degrees; return first option index, -1 absent, or -2 unavailable. */
int lab_menu_index_coordinates(node_jsref select, double latitude, double longitude);

/** @brief Return visible/hidden option-index arrays for a normalised substring, or null when unavailable. */
jsref lab_menu_index_filter(node_jsref select, jsref query);

/** @brief Discard a menu's index when its rendered rows are rebuilt. */
void lab_menu_index_invalidate(node_jsref select);

/** @brief Return index size and candidates examined by the latest coordinate and text queries. */
void lab_menu_index_stats(node_jsref select);

/** @brief Release all native menu indexes at page shutdown. */
void lab_menu_index_clear(void);

#endif
