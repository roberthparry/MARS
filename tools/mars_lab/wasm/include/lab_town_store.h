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

/**
 * @brief Merge one jurisdiction's records within the bounded, country-aware native catalogue.
 * @param code Scoped country or subdivision string, borrowed only during this call.
 * @param rows Scoped array of town records; copied strings and indexes remain owned by the store.
 * @return Number of new records, zero for duplicates, or -1 after rolling back this batch on invalid input or failure.
 * @note The 16 MiB text and index admission allowance retains the incoming country and the active-country pins.
 * Older unpinned countries may be discarded to make room, including before a later batch failure. Use
 * lab_town_store_policy() to acknowledge discarded country codes and invalidate completed-loading markers.
 * Calls are serialised; no scoped browser handle is retained. lab_town_store_clear() releases resident records.
 */
int lab_town_store_add(jsref code, jsref rows);

/**
 * @brief Update selected-country protection and report bounded cache accounting and discarded catalogues.
 * @param request Scoped object, optionally containing pins: an array of at most eight country or subdivision codes.
 * Subdivision codes protect their whole country. Omit pins to preserve the existing protection set.
 * @return No C value; the scoped host result contains bytes, limit, count and evicted country codes. Invalid pins
 * return null without changing protection or consuming pending eviction notices.
 * @note Reading the result acknowledges its eviction notices. Strings and arrays are independent browser copies;
 * no ownership transfers or host handles survive this serialised call. The caller must invalidate its completed
 * catalogue markers for discarded countries so later selections reload them from the database.
 */
void lab_town_store_policy(jsref request);

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
