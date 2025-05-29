/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GLOBALS_H
#define GLOBALS_H

// This file contains compile-time constants that depend on sizes of structures
// or the page size.  Page size isn't always known at compile time so some
// values defined here may be determined at runtime.

#include "mozilla/Assertions.h"
#include "mozilla/HelperMacros.h"
#include "mozilla/Literals.h"
#include "mozilla/MathAlgorithms.h"

#include "Constants.h"
// Chunk.h is required for sizeof(arena_chunk_t), but it's inconvenient that
// Chunk.h can't access any constants.
#include "Chunk.h"
#include "Utils.h"

// Define MALLOC_RUNTIME_CONFIG depending on MOZ_DEBUG. Overriding this as
// a build option allows us to build mozjemalloc/firefox without runtime asserts
// but with runtime configuration. Making some testing easier.

#ifdef MOZ_DEBUG
#  define MALLOC_RUNTIME_CONFIG
#endif

// Uncomment this to enable extra-vigilant assertions.  These assertions may run
// more expensive checks that are sometimes too slow for regular debug mode.
// #define MALLOC_DEBUG_VIGILANT

// When MALLOC_STATIC_PAGESIZE is defined, the page size is fixed at
// compile-time for better performance, as opposed to determined at
// runtime. Some platforms can have different page sizes at runtime
// depending on kernel configuration, so they are opted out by default.
// Debug builds are opted out too, for test coverage.
#ifndef MALLOC_RUNTIME_CONFIG
#  if !defined(__ia64__) && !defined(__sparc__) && !defined(__mips__) &&       \
      !defined(__aarch64__) && !defined(__powerpc__) && !defined(XP_MACOSX) && \
      !defined(__loongarch__)
#    define MALLOC_STATIC_PAGESIZE 1
#  endif
#endif

namespace mozilla {

#ifdef MALLOC_STATIC_PAGESIZE
// VM page size. It must divide the runtime CPU page size or the code
// will abort.
// Platform specific page size conditions copied from js/public/HeapAPI.h
#  if defined(__powerpc64__)
static const size_t gPageSize = 64_KiB;
#  elif defined(__loongarch64)
static const size_t gPageSize = 16_KiB;
#  else
static const size_t gPageSize = 4_KiB;
#  endif
static const size_t gRealPageSize = gPageSize;
#else

// When MALLOC_OPTIONS contains one or several `P`s, the page size used
// across the allocator is multiplied by 2 for each `P`, but we also keep
// the real page size for code paths that need it. gPageSize is thus a
// power of two greater or equal to gRealPageSize.
extern size_t gRealPageSize;
extern size_t gPageSize;

#endif

#ifdef MALLOC_STATIC_PAGESIZE
#  define GLOBAL(type, name, value) static const type name = value;
#  define GLOBAL_LOG2 LOG2
#  define GLOBAL_ASSERT_HELPER1(x) static_assert(x, #x)
#  define GLOBAL_ASSERT_HELPER2(x, y) static_assert(x, y)
#  define GLOBAL_ASSERT(...)                                          \
                                                                      \
    MOZ_PASTE_PREFIX_AND_ARG_COUNT(GLOBAL_ASSERT_HELPER, __VA_ARGS__) \
    (__VA_ARGS__)
#  define GLOBAL_CONSTEXPR constexpr
#  include "Globals_inc.h"
#  undef GLOBAL_CONSTEXPR
#  undef GLOBAL_ASSERT
#  undef GLOBAL_ASSERT_HELPER1
#  undef GLOBAL_ASSERT_HELPER2
#  undef GLOBAL_LOG2
#  undef GLOBAL
#else
// We declare the globals here and initialise them in DefineGlobals()
#  define GLOBAL(type, name, value) extern type name;
#  define GLOBAL_ASSERT(...)
#  include "Globals_inc.h"
#  undef GLOBAL_ASSERT
#  undef GLOBAL

void DefineGlobals();
#endif

// Max size class for bins.
#define gMaxBinClass \
  (gMaxSubPageClass ? gMaxSubPageClass : kMaxQuantumWideClass)

// Return the smallest chunk multiple that is >= s.
#define CHUNK_CEILING(s) (((s) + kChunkSizeMask) & ~kChunkSizeMask)

// Return the smallest cacheline multiple that is >= s.
#define CACHELINE_CEILING(s) \
  (((s) + (kCacheLineSize - 1)) & ~(kCacheLineSize - 1))

// Return the smallest quantum multiple that is >= a.
#define QUANTUM_CEILING(a) (((a) + (kQuantumMask)) & ~(kQuantumMask))
#define QUANTUM_WIDE_CEILING(a) \
  (((a) + (kQuantumWideMask)) & ~(kQuantumWideMask))

// Return the smallest sub page-size  that is >= a.
#define SUBPAGE_CEILING(a) (RoundUpPow2(a))

// Return the smallest pagesize multiple that is >= s.
#define PAGE_CEILING(s) (((s) + gPageSizeMask) & ~gPageSizeMask)

// Number of all the small-allocated classes
#define NUM_SMALL_CLASSES                                          \
  (kNumTinyClasses + kNumQuantumClasses + kNumQuantumWideClasses + \
   gNumSubPageClasses)

// Return the chunk address for allocation address a.
static inline arena_chunk_t* GetChunkForPtr(const void* aPtr) {
  return (arena_chunk_t*)(uintptr_t(aPtr) & ~kChunkSizeMask);
}

// Return the chunk offset of address a.
static inline size_t GetChunkOffsetForPtr(const void* aPtr) {
  return (size_t)(uintptr_t(aPtr) & kChunkSizeMask);
}

// Maximum number of dirty pages per arena.
#define DIRTY_MAX_DEFAULT (1U << 8)

enum PoisonType {
  NONE,
  SOME,
  ALL,
};

extern size_t opt_dirty_max;

#define OPT_JUNK_DEFAULT false
#define OPT_ZERO_DEFAULT false
#ifdef EARLY_BETA_OR_EARLIER
#  define OPT_POISON_DEFAULT ALL
#else
#  define OPT_POISON_DEFAULT SOME
#endif
// Keep this larger than and ideally a multiple of kCacheLineSize;
#define OPT_POISON_SIZE_DEFAULT 256

#ifdef MALLOC_RUNTIME_CONFIG

extern bool opt_junk;
extern bool opt_zero;
extern PoisonType opt_poison;
extern size_t opt_poison_size;

#else

constexpr bool opt_junk = OPT_JUNK_DEFAULT;
constexpr bool opt_zero = OPT_ZERO_DEFAULT;
constexpr PoisonType opt_poison = OPT_POISON_DEFAULT;
constexpr size_t opt_poison_size = OPT_POISON_SIZE_DEFAULT;

static_assert(opt_poison_size >= kCacheLineSize);
static_assert((opt_poison_size % kCacheLineSize) == 0);

#endif

extern bool opt_randomize_small;

}  // namespace mozilla

#endif  // ! GLOBALS_H
