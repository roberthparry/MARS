/**
 * @file mars_keyring.h
 * @brief Desktop Secret Service access for the MARS configuration encryption key.
 *
 * Loads the system libsecret implementation at runtime. The key never belongs in the JSON locator,
 * command line or resource database. A stable application identity permits moving the database locally.
 */
#ifndef MARS_APP_KEYRING_H
#define MARS_APP_KEYRING_H

#include "string-t.h"

/** @brief Return an owned configuration key, optionally creating its desktop-keyring entry; NULL means unavailable. */
string_t *mars_keyring_password(bool create);

#endif
