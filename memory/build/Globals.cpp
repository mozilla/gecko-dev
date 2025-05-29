/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "Globals.h"

#ifdef XP_WIN
#  include <processenv.h>
#endif

// ***************************************************************************
// Constants defining allocator size classes and behavior.

namespace mozilla {

#ifndef MALLOC_STATIC_PAGESIZE
#  define GLOBAL(type, name, value) type name;
#  define GLOBAL_ASSERT(...)
#  include "Globals_inc.h"
#  undef GLOBAL_ASSERT
#  undef GLOBAL

size_t gRealPageSize = 0;
size_t gPageSize = 0;

void DefineGlobals() {
#  define GLOBAL(type, name, value) name = value;
#  define GLOBAL_LOG2 mozilla::FloorLog2
#  define GLOBAL_ASSERT MOZ_RELEASE_ASSERT
#  define GLOBAL_CONSTEXPR
#  include "Globals_inc.h"
#  undef GLOBAL_CONSTEXPR
#  undef GLOBAL_ASSERT
#  undef GLOBAL_LOG2
#  undef GLOBAL
}
#endif

#ifdef XP_WIN

// Implement getenv without using malloc.
static char mozillaMallocOptionsBuf[64];

#  define getenv xgetenv
static char* getenv(const char* name) {
  if (GetEnvironmentVariableA(name, mozillaMallocOptionsBuf,
                              sizeof(mozillaMallocOptionsBuf)) > 0) {
    return mozillaMallocOptionsBuf;
  }

  return nullptr;
}
#endif

// *****************************
// Runtime configuration options.

size_t opt_dirty_max = DIRTY_MAX_DEFAULT;

#ifdef MALLOC_RUNTIME_CONFIG
bool opt_junk = OPT_JUNK_DEFAULT;
bool opt_zero = OPT_ZERO_DEFAULT;
PoisonType opt_poison = OPT_POISON_DEFAULT;
size_t opt_poison_size = OPT_POISON_SIZE_DEFAULT;
#endif

bool opt_randomize_small = true;

}  // namespace mozilla
