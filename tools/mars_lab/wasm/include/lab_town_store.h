/**
 * @file lab_town_store.h
 * @brief Native town catalogue and spatial indexes for browser location controls.
 *
 * Owns copied town records in array_t and dictionary_t. Browser consumers receive
 * transient snapshots; no scoped browser handles survive a call. Each page owns
 * one catalogue, shared by menus and the globe.
 * Globe place snapshots expose displayName and fullName without numeric GeoNames suffixes;
 * the nested town record keeps its original name and selection key for identity and restoration.
 * Each entry point dispatches synchronously through the service's private singleton. Browser handles
 * require the owning non-shared WASM instance; native dispatch serialises complete operations with a mutex.
 */
#ifndef LAB_TOWN_STORE_H
#define LAB_TOWN_STORE_H

#include "lab_handle.h"

/** @brief Merge a jurisdiction's records; return the inserted count, or -1 after rolling back a failed batch. */
int lab_town_store_add(jsref code, jsref rows);

/**
 * @brief Publish a town-menu snapshot, combining regional memberships for a country code.
 * @param code Scoped string handle containing a country or jurisdiction code; not retained.
 * @post Publishes an independently owned browser array through lab_dom_return, or null when absent or allocation fails.
 * A two-letter country code combines its directly indexed regional lists and removes repeated selection keys.
 * Each town retains its actual jurisdiction and time zone; the national default remains preferred. Longer codes
 * select the original regional list. No native ownership transfers and no browser handle survives the call.
 */
void lab_town_store_list(jsref code);

/**
 * @brief Build the same scoped town-menu snapshot for another native location consumer.
 * @param code Scoped country or jurisdiction string handle, borrowed only during the call.
 * @return Scoped browser array handle, or zero when absent or allocation fails; valid only in the caller's scope.
 * @note Uses the country merging and default-selection policy documented by lab_town_store_list().
 * The caller owns the browser result through its handle scope; no native records are transferred or retained.
 */
jsref lab_town_store_snapshot(jsref code);

/** @brief Return one town by jurisdiction and selection key, or null. */
void lab_town_store_get(jsref code, jsref key);

/** @brief Return towns in visible five-degree buckets, including wrapped longitudes. */
void lab_town_store_region(double west, double south, double east, double north);

/** @brief Release all records and indexes at catalogue replacement or page shutdown. */
void lab_town_store_clear(void);

#endif
