/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

/**
 * This is an exported header containing declarations of methods for statically
 * linked SQLite3 extensions.
 */

#ifndef SQLITE3_STATIC_EXT_H
#define SQLITE3_STATIC_EXT_H
#include "sqlite3.h"
#include "misc/carray.h"

#ifdef __cplusplus
extern "C" {
#endif

SQLITE_API int sqlite3_carray_init(sqlite3*, char**,
                                   const sqlite3_api_routines*);

#ifdef __cplusplus
} /* end of the 'extern "C"' block */
#endif

#endif /* SQLITE3_STATIC_EXT_H */
