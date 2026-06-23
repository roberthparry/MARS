#ifndef MARS_SQLITE_INTERNAL_H
#define MARS_SQLITE_INTERNAL_H

#ifndef SQLITE_HAS_CODEC
#define SQLITE_HAS_CODEC 1
#endif
#include <sqlcipher/sqlite3.h>

#include "sqlite.h"

sqlite3 *sqlite_native_handle(sqlite_t *db);

#endif /* MARS_SQLITE_INTERNAL_H */
