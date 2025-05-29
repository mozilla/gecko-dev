/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "Globals.h"

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

}  // namespace mozilla
