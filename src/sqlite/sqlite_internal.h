#ifndef MARS_SQLITE_INTERNAL_H
#define MARS_SQLITE_INTERNAL_H

#if !defined(MARS_SQLITE_INTERNAL_ACCESS) &&                                                                           \
    (!defined(__INTELLISENSE__) || (defined(__INCLUDE_LEVEL__) && __INCLUDE_LEVEL__ > 0))
#error "sqlite_internal.h is private to the sqlite module; include sqlite.h instead."
#endif

#ifndef SQLITE_HAS_CODEC
#define SQLITE_HAS_CODEC 1
#endif
#include <sqlcipher/sqlite3.h>

#include "sqlite.h"

sqlite3 *sqlite_native_handle(sqlite_t *db);

#endif /* MARS_SQLITE_INTERNAL_H */
