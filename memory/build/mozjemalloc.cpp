/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Portions of this file were originally under the following license:
//
// Copyright (C) 2006-2008 Jason Evans <jasone@FreeBSD.org>.
// All rights reserved.
// Copyright (C) 2007-2017 Mozilla Foundation.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1. Redistributions of source code must retain the above copyright
//    notice(s), this list of conditions and the following disclaimer as
//    the first lines of this file unmodified other than the possible
//    addition of one or more copyright notices.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice(s), this list of conditions and the following disclaimer in
//    the documentation and/or other materials provided with the
//    distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER(S) ``AS IS'' AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER(S) BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
// BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
// OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
// EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// *****************************************************************************
//
// This allocator implementation is designed to provide scalable performance
// for multi-threaded programs on multi-processor systems.  The following
// features are included for this purpose:
//
//   + Multiple arenas are used if there are multiple CPUs, which reduces lock
//     contention and cache sloshing.
//
//   + Cache line sharing between arenas is avoided for internal data
//     structures.
//
//   + Memory is managed in chunks and runs (chunks can be split into runs),
//     rather than as individual pages.  This provides a constant-time
//     mechanism for associating allocations with particular arenas.
//
// Allocation requests are rounded up to the nearest size class, and no record
// of the original request size is maintained.  Allocations are broken into
// categories according to size class.  Assuming runtime defaults, the size
// classes in each category are as follows (for x86, x86_64 and Apple Silicon):
//
//   |=========================================================|
//   | Category | Subcategory    |     x86 |  x86_64 | Mac ARM |
//   |---------------------------+---------+---------+---------|
//   | Word size                 |  32 bit |  64 bit |  64 bit |
//   | Page size                 |    4 Kb |    4 Kb |   16 Kb |
//   |=========================================================|
//   | Small    | Tiny           |    4/-w |      -w |       - |
//   |          |                |       8 |    8/-w |       8 |
//   |          |----------------+---------|---------|---------|
//   |          | Quantum-spaced |      16 |      16 |      16 |
//   |          |                |      32 |      32 |      32 |
//   |          |                |      48 |      48 |      48 |
//   |          |                |     ... |     ... |     ... |
//   |          |                |     480 |     480 |     480 |
//   |          |                |     496 |     496 |     496 |
//   |          |----------------+---------|---------|---------|
//   |          | Quantum-wide-  |     512 |     512 |     512 |
//   |          | spaced         |     768 |     768 |     768 |
//   |          |                |     ... |     ... |     ... |
//   |          |                |    3584 |    3584 |    3584 |
//   |          |                |    3840 |    3840 |    3840 |
//   |          |----------------+---------|---------|---------|
//   |          | Sub-page       |       - |       - |    4096 |
//   |          |                |       - |       - |    8 kB |
//   |=========================================================|
//   | Large                     |    4 kB |    4 kB |       - |
//   |                           |    8 kB |    8 kB |       - |
//   |                           |   12 kB |   12 kB |       - |
//   |                           |   16 kB |   16 kB |   16 kB |
//   |                           |     ... |     ... |       - |
//   |                           |   32 kB |   32 kB |   32 kB |
//   |                           |     ... |     ... |     ... |
//   |                           | 1008 kB | 1008 kB | 1008 kB |
//   |                           | 1012 kB | 1012 kB |       - |
//   |                           | 1016 kB | 1016 kB |       - |
//   |                           | 1020 kB | 1020 kB |       - |
//   |=========================================================|
//   | Huge                      |    1 MB |    1 MB |    1 MB |
//   |                           |    2 MB |    2 MB |    2 MB |
//   |                           |    3 MB |    3 MB |    3 MB |
//   |                           |     ... |     ... |     ... |
//   |=========================================================|
//
// Legend:
//   n:    Size class exists for this platform.
//   n/-w: This size class doesn't exist on Windows (see kMinTinyClass).
//   -:    This size class doesn't exist for this platform.
//   ...:  Size classes follow a pattern here.
//
// NOTE: Due to Mozilla bug 691003, we cannot reserve less than one word for an
// allocation on Linux or Mac.  So on 32-bit *nix, the smallest bucket size is
// 4 bytes, and on 64-bit, the smallest bucket size is 8 bytes.
//
// A different mechanism is used for each category:
//
//   Small : Each size class is segregated into its own set of runs.  Each run
//           maintains a bitmap of which regions are free/allocated.
//
//   Large : Each allocation is backed by a dedicated run.  Metadata are stored
//           in the associated arena chunk header maps.
//
//   Huge : Each allocation is backed by a dedicated contiguous set of chunks.
//          Metadata are stored in a separate red-black tree.
//
// *****************************************************************************

#include "mozmemory_wrap.h"
#include "mozjemalloc.h"
#include "mozjemalloc_types.h"

#include <cstring>
#include <cerrno>
#include <optional>
#include <type_traits>
#ifdef XP_WIN
#  include <io.h>
#  include <windows.h>
#else
#  include <sys/mman.h>
#  include <unistd.h>
#endif
#ifdef XP_DARWIN
#  include <libkern/OSAtomic.h>
#  include <mach/mach_init.h>
#  include <mach/vm_map.h>
#endif

#include "mozilla/Atomics.h"
#include "mozilla/Alignment.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/Assertions.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/DoublyLinkedList.h"
#include "mozilla/HelperMacros.h"
#include "mozilla/Likely.h"
#include "mozilla/Literals.h"
#include "mozilla/MathAlgorithms.h"
#include "mozilla/RandomNum.h"
// Note: MozTaggedAnonymousMmap() could call an LD_PRELOADed mmap
// instead of the one defined here; use only MozTagAnonymousMemory().
#include "mozilla/TaggedAnonymousMemory.h"
#include "mozilla/ThreadLocal.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Unused.h"
#include "mozilla/XorShift128PlusRNG.h"
#include "mozilla/fallible.h"
#include "rb.h"
#include "Mutex.h"
#include "PHC.h"
#include "Utils.h"

#if defined(XP_WIN)
#  include "mozmemory_utils.h"
#endif

// For GetGeckoProcessType(), when it's used.
#if defined(XP_WIN) && !defined(JS_STANDALONE)
#  include "mozilla/ProcessType.h"
#endif

using namespace mozilla;

// On Linux, we use madvise(MADV_DONTNEED) to release memory back to the
// operating system.  If we release 1MB of live pages with MADV_DONTNEED, our
// RSS will decrease by 1MB (almost) immediately.
//
// On Mac, we use madvise(MADV_FREE).  Unlike MADV_DONTNEED on Linux, MADV_FREE
// on Mac doesn't cause the OS to release the specified pages immediately; the
// OS keeps them in our process until the machine comes under memory pressure.
//
// It's therefore difficult to measure the process's RSS on Mac, since, in the
// absence of memory pressure, the contribution from the heap to RSS will not
// decrease due to our madvise calls.
//
// We therefore define MALLOC_DOUBLE_PURGE on Mac.  This causes jemalloc to
// track which pages have been MADV_FREE'd.  You can then call
// jemalloc_purge_freed_pages(), which will force the OS to release those
// MADV_FREE'd pages, making the process's RSS reflect its true memory usage.

#ifdef XP_DARWIN
#  define MALLOC_DOUBLE_PURGE
#endif

#ifdef XP_WIN
#  define MALLOC_DECOMMIT
#endif

// Define MALLOC_RUNTIME_CONFIG depending on MOZ_DEBUG. Overriding this as
// a build option allows us to build mozjemalloc/firefox without runtime asserts
// but with runtime configuration. Making some testing easier.

#ifdef MOZ_DEBUG
#  define MALLOC_RUNTIME_CONFIG
#endif

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

#ifdef XP_WIN
#  define STDERR_FILENO 2

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

#ifndef XP_WIN
// Newer Linux systems support MADV_FREE, but we're not supporting
// that properly. bug #1406304.
#  if defined(XP_LINUX) && defined(MADV_FREE)
#    undef MADV_FREE
#  endif
#  ifndef MADV_FREE
#    define MADV_FREE MADV_DONTNEED
#  endif
#endif

// Some tools, such as /dev/dsp wrappers, LD_PRELOAD libraries that
// happen to override mmap() and call dlsym() from their overridden
// mmap(). The problem is that dlsym() calls malloc(), and this ends
// up in a dead lock in jemalloc.
// On these systems, we prefer to directly use the system call.
// We do that for Linux systems and kfreebsd with GNU userland.
// Note sanity checks are not done (alignment of offset, ...) because
// the uses of mmap are pretty limited, in jemalloc.
//
// On Alpha, glibc has a bug that prevents syscall() to work for system
// calls with 6 arguments.
#if (defined(XP_LINUX) && !defined(__alpha__)) || \
    (defined(__FreeBSD_kernel__) && defined(__GLIBC__))
#  include <sys/syscall.h>
#  if defined(SYS_mmap) || defined(SYS_mmap2)
static inline void* _mmap(void* addr, size_t length, int prot, int flags,
                          int fd, off_t offset) {
// S390 only passes one argument to the mmap system call, which is a
// pointer to a structure containing the arguments.
#    ifdef __s390__
  struct {
    void* addr;
    size_t length;
    long prot;
    long flags;
    long fd;
    off_t offset;
  } args = {addr, length, prot, flags, fd, offset};
  return (void*)syscall(SYS_mmap, &args);
#    else
#      if defined(ANDROID) && defined(__aarch64__) && defined(SYS_mmap2)
  // Android NDK defines SYS_mmap2 for AArch64 despite it not supporting mmap2.
#        undef SYS_mmap2
#      endif
#      ifdef SYS_mmap2
  return (void*)syscall(SYS_mmap2, addr, length, prot, flags, fd, offset >> 12);
#      else
  return (void*)syscall(SYS_mmap, addr, length, prot, flags, fd, offset);
#      endif
#    endif
}
#    define mmap _mmap
#    define munmap(a, l) syscall(SYS_munmap, a, l)
#  endif
#endif

// ***************************************************************************
// Structures for chunk headers for chunks used for non-huge allocations.

struct arena_t;

// Each element of the chunk map corresponds to one page within the chunk.
struct arena_chunk_map_t {
  // Linkage for run trees.  There are two disjoint uses:
  //
  // 1) arena_t's tree or available runs.
  // 2) arena_run_t conceptually uses this linkage for in-use non-full
  //    runs, rather than directly embedding linkage.
  RedBlackTreeNode<arena_chunk_map_t> link;

  // Run address (or size) and various flags are stored together.  The bit
  // layout looks like (assuming 32-bit system):
  //
  //   ???????? ???????? ????---- fmckdzla
  //
  // ? : Unallocated: Run address for first/last pages, unset for internal
  //                  pages.
  //     Small: Run address.
  //     Large: Run size for first page, unset for trailing pages.
  // - : Unused.
  // f : Fresh memory?
  // m : MADV_FREE/MADV_DONTNEED'ed?
  // c : decommitted?
  // k : key?
  // d : dirty?
  // z : zeroed?
  // l : large?
  // a : allocated?
  //
  // Following are example bit patterns for the three types of runs.
  //
  // r : run address
  // s : run size
  // x : don't care
  // - : 0
  // [cdzla] : bit set
  //
  //   Unallocated:
  //     ssssssss ssssssss ssss---- --c-----
  //     xxxxxxxx xxxxxxxx xxxx---- ----d---
  //     ssssssss ssssssss ssss---- -----z--
  //
  //   Small:
  //     rrrrrrrr rrrrrrrr rrrr---- -------a
  //     rrrrrrrr rrrrrrrr rrrr---- -------a
  //     rrrrrrrr rrrrrrrr rrrr---- -------a
  //
  //   Large:
  //     ssssssss ssssssss ssss---- ------la
  //     -------- -------- -------- ------la
  //     -------- -------- -------- ------la
  size_t bits;

// A page can be in one of several states.
//
// CHUNK_MAP_ALLOCATED marks allocated pages, the only other bit that can be
// combined is CHUNK_MAP_LARGE.
//
// CHUNK_MAP_LARGE may be combined with CHUNK_MAP_ALLOCATED to show that the
// allocation is a "large" allocation (see SizeClass), rather than a run of
// small allocations.  The interpretation of the gPageSizeMask bits depends onj
// this bit, see the description above.
//
// CHUNK_MAP_DIRTY is used to mark pages that were allocated and are now freed.
// They may contain their previous contents (or poison).  CHUNK_MAP_DIRTY, when
// set, must be the only set bit.
//
// CHUNK_MAP_MADVISED marks pages which are madvised (with either MADV_DONTNEED
// or MADV_FREE).  This is only valid if MALLOC_DECOMMIT is not defined.  When
// set, it must be the only bit set.
//
// CHUNK_MAP_DECOMMITTED is used if CHUNK_MAP_DECOMMITTED is defined.  Unused
// dirty pages may be decommitted and marked as CHUNK_MAP_DECOMMITTED.  They
// must be re-committed with pages_commit() before they can be touched.
//
// CHUNK_MAP_FRESH is set on pages that have never been used before (the chunk
// is newly allocated or they were decommitted and have now been recommitted.
// CHUNK_MAP_FRESH is also used for "double purged" pages meaning that they were
// madvised and later were unmapped and remapped to force them out of the
// program's resident set.  This is enabled when MALLOC_DOUBLE_PURGE is defined
// (eg on MacOS).
//
// CHUNK_MAP_ZEROED is set on pages that are known to contain zeros.
//
// CHUNK_MAP_DIRTY, _DECOMMITED _MADVISED and _FRESH are always mutually
// exclusive.
//
// CHUNK_MAP_KEY is never used on real pages, only on lookup keys.
//
#define CHUNK_MAP_FRESH ((size_t)0x80U)
#define CHUNK_MAP_MADVISED ((size_t)0x40U)
#define CHUNK_MAP_DECOMMITTED ((size_t)0x20U)
#define CHUNK_MAP_MADVISED_OR_DECOMMITTED \
  (CHUNK_MAP_MADVISED | CHUNK_MAP_DECOMMITTED)
#define CHUNK_MAP_FRESH_MADVISED_OR_DECOMMITTED \
  (CHUNK_MAP_FRESH | CHUNK_MAP_MADVISED | CHUNK_MAP_DECOMMITTED)
#define CHUNK_MAP_KEY ((size_t)0x10U)
#define CHUNK_MAP_DIRTY ((size_t)0x08U)
#define CHUNK_MAP_ZEROED ((size_t)0x04U)
#define CHUNK_MAP_LARGE ((size_t)0x02U)
#define CHUNK_MAP_ALLOCATED ((size_t)0x01U)
};

// Arena chunk header.
struct arena_chunk_t {
  // Arena that owns the chunk.
  arena_t* arena;

  // Linkage for the arena's tree of dirty chunks.
  RedBlackTreeNode<arena_chunk_t> link_dirty;

#ifdef MALLOC_DOUBLE_PURGE
  // If we're double-purging, we maintain a linked list of chunks which
  // have pages which have been madvise(MADV_FREE)'d but not explicitly
  // purged.
  //
  // We're currently lazy and don't remove a chunk from this list when
  // all its madvised pages are recommitted.
  DoublyLinkedListElement<arena_chunk_t> chunks_madvised_elem;
#endif

  // Number of dirty pages.
  size_t ndirty;

  // Map of pages within chunk that keeps track of free/large/small.
  arena_chunk_map_t map[];  // Dynamically sized.
};

// ***************************************************************************
// Constants defining allocator size classes and behavior.

// Our size classes are inclusive ranges of memory sizes.  By describing the
// minimums and how memory is allocated in each range the maximums can be
// calculated.

// Smallest size class to support.  On Windows the smallest allocation size
// must be 8 bytes on 32-bit, 16 bytes on 64-bit.  On Linux and Mac, even
// malloc(1) must reserve a word's worth of memory (see Mozilla bug 691003).
#ifdef XP_WIN
static const size_t kMinTinyClass = sizeof(void*) * 2;
#else
static const size_t kMinTinyClass = sizeof(void*);
#endif

// Maximum tiny size class.
static const size_t kMaxTinyClass = 8;

// Smallest quantum-spaced size classes. It could actually also be labelled a
// tiny allocation, and is spaced as such from the largest tiny size class.
// Tiny classes being powers of 2, this is twice as large as the largest of
// them.
static const size_t kMinQuantumClass = kMaxTinyClass * 2;
static const size_t kMinQuantumWideClass = 512;
static const size_t kMinSubPageClass = 4_KiB;

// Amount (quantum) separating quantum-spaced size classes.
static const size_t kQuantum = 16;
static const size_t kQuantumMask = kQuantum - 1;
static const size_t kQuantumWide = 256;
static const size_t kQuantumWideMask = kQuantumWide - 1;

static const size_t kMaxQuantumClass = kMinQuantumWideClass - kQuantum;
static const size_t kMaxQuantumWideClass = kMinSubPageClass - kQuantumWide;

// We can optimise some divisions to shifts if these are powers of two.
static_assert(mozilla::IsPowerOfTwo(kQuantum),
              "kQuantum is not a power of two");
static_assert(mozilla::IsPowerOfTwo(kQuantumWide),
              "kQuantumWide is not a power of two");

static_assert(kMaxQuantumClass % kQuantum == 0,
              "kMaxQuantumClass is not a multiple of kQuantum");
static_assert(kMaxQuantumWideClass % kQuantumWide == 0,
              "kMaxQuantumWideClass is not a multiple of kQuantumWide");
static_assert(kQuantum < kQuantumWide,
              "kQuantum must be smaller than kQuantumWide");
static_assert(mozilla::IsPowerOfTwo(kMinSubPageClass),
              "kMinSubPageClass is not a power of two");

// Number of (2^n)-spaced tiny classes.
static const size_t kNumTinyClasses =
    LOG2(kMaxTinyClass) - LOG2(kMinTinyClass) + 1;

// Number of quantum-spaced classes.  We add kQuantum(Max) before subtracting to
// avoid underflow when a class is empty (Max<Min).
static const size_t kNumQuantumClasses =
    (kMaxQuantumClass + kQuantum - kMinQuantumClass) / kQuantum;
static const size_t kNumQuantumWideClasses =
    (kMaxQuantumWideClass + kQuantumWide - kMinQuantumWideClass) / kQuantumWide;

// Size and alignment of memory chunks that are allocated by the OS's virtual
// memory system.
static const size_t kChunkSize = 1_MiB;
static const size_t kChunkSizeMask = kChunkSize - 1;

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
static size_t gRealPageSize;
static size_t gPageSize;
#endif

#ifdef MALLOC_STATIC_PAGESIZE
#  define DECLARE_GLOBAL(type, name)
#  define DEFINE_GLOBALS
#  define END_GLOBALS
#  define DEFINE_GLOBAL(type) static const type
#  define GLOBAL_LOG2 LOG2
#  define GLOBAL_ASSERT_HELPER1(x) static_assert(x, #x)
#  define GLOBAL_ASSERT_HELPER2(x, y) static_assert(x, y)
#  define GLOBAL_ASSERT(...)                                               \
    MACRO_CALL(                                                            \
        MOZ_PASTE_PREFIX_AND_ARG_COUNT(GLOBAL_ASSERT_HELPER, __VA_ARGS__), \
        (__VA_ARGS__))
#  define GLOBAL_CONSTEXPR constexpr
#else
#  define DECLARE_GLOBAL(type, name) static type name;
#  define DEFINE_GLOBALS static void DefineGlobals() {
#  define END_GLOBALS }
#  define DEFINE_GLOBAL(type)
#  define GLOBAL_LOG2 FloorLog2
#  define GLOBAL_ASSERT MOZ_RELEASE_ASSERT
#  define GLOBAL_CONSTEXPR
#endif

DECLARE_GLOBAL(size_t, gMaxSubPageClass)
DECLARE_GLOBAL(uint8_t, gNumSubPageClasses)
DECLARE_GLOBAL(uint8_t, gPageSize2Pow)
DECLARE_GLOBAL(size_t, gPageSizeMask)
DECLARE_GLOBAL(size_t, gChunkNumPages)
DECLARE_GLOBAL(size_t, gChunkHeaderNumPages)
DECLARE_GLOBAL(size_t, gMaxLargeClass)

DEFINE_GLOBALS

// Largest sub-page size class, or zero if there are none
DEFINE_GLOBAL(size_t)
gMaxSubPageClass = gPageSize / 2 >= kMinSubPageClass ? gPageSize / 2 : 0;

// Max size class for bins.
#define gMaxBinClass \
  (gMaxSubPageClass ? gMaxSubPageClass : kMaxQuantumWideClass)

// Number of sub-page bins.
DEFINE_GLOBAL(uint8_t)
gNumSubPageClasses = []() GLOBAL_CONSTEXPR -> uint8_t {
  if GLOBAL_CONSTEXPR (gMaxSubPageClass != 0) {
    return FloorLog2(gMaxSubPageClass) - LOG2(kMinSubPageClass) + 1;
  }
  return 0;
}();

DEFINE_GLOBAL(uint8_t) gPageSize2Pow = GLOBAL_LOG2(gPageSize);
DEFINE_GLOBAL(size_t) gPageSizeMask = gPageSize - 1;

// Number of pages in a chunk.
DEFINE_GLOBAL(size_t) gChunkNumPages = kChunkSize >> gPageSize2Pow;

// Number of pages necessary for a chunk header plus a guard page.
DEFINE_GLOBAL(size_t)
gChunkHeaderNumPages =
    1 + (((sizeof(arena_chunk_t) + sizeof(arena_chunk_map_t) * gChunkNumPages +
           gPageSizeMask) &
          ~gPageSizeMask) >>
         gPageSize2Pow);

// One chunk, minus the header, minus a guard page
DEFINE_GLOBAL(size_t)
gMaxLargeClass =
    kChunkSize - gPageSize - (gChunkHeaderNumPages << gPageSize2Pow);

// Various sanity checks that regard configuration.
GLOBAL_ASSERT(1ULL << gPageSize2Pow == gPageSize,
              "Page size is not a power of two");
GLOBAL_ASSERT(kQuantum >= sizeof(void*));
GLOBAL_ASSERT(kQuantum <= kQuantumWide);
GLOBAL_ASSERT(!kNumQuantumWideClasses ||
              kQuantumWide <= (kMinSubPageClass - kMaxQuantumClass));

GLOBAL_ASSERT(kQuantumWide <= kMaxQuantumClass);

GLOBAL_ASSERT(gMaxSubPageClass >= kMinSubPageClass || gMaxSubPageClass == 0);
GLOBAL_ASSERT(gMaxLargeClass >= gMaxSubPageClass);
GLOBAL_ASSERT(kChunkSize >= gPageSize);
GLOBAL_ASSERT(kQuantum * 4 <= kChunkSize);

END_GLOBALS

// Recycle at most 128 MiB of chunks. This means we retain at most
// 6.25% of the process address space on a 32-bit OS for later use.
static const size_t gRecycleLimit = 128_MiB;

// The current amount of recycled bytes, updated atomically.
static Atomic<size_t, ReleaseAcquire> gRecycledSize;

// Maximum number of dirty pages per arena.
#define DIRTY_MAX_DEFAULT (1U << 8)

static size_t opt_dirty_max = DIRTY_MAX_DEFAULT;

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

// ***************************************************************************
// MALLOC_DECOMMIT and MALLOC_DOUBLE_PURGE are mutually exclusive.
#if defined(MALLOC_DECOMMIT) && defined(MALLOC_DOUBLE_PURGE)
#  error MALLOC_DECOMMIT and MALLOC_DOUBLE_PURGE are mutually exclusive.
#endif

static void* base_alloc(size_t aSize);

// Set to true once the allocator has been initialized.
#if defined(_MSC_VER) && !defined(__clang__)
// MSVC may create a static initializer for an Atomic<bool>, which may actually
// run after `malloc_init` has been called once, which triggers multiple
// initializations.
// We work around the problem by not using an Atomic<bool> at all. There is a
// theoretical problem with using `malloc_initialized` non-atomically, but
// practically, this is only true if `malloc_init` is never called before
// threads are created.
static bool malloc_initialized;
#else
static Atomic<bool, MemoryOrdering::ReleaseAcquire> malloc_initialized;
#endif

static StaticMutex gInitLock MOZ_UNANNOTATED = {STATIC_MUTEX_INIT};

// ***************************************************************************
// Statistics data structures.

struct arena_stats_t {
  // Number of bytes currently mapped.
  size_t mapped;

  // Current number of committed pages (non madvised/decommitted)
  size_t committed;

  // Per-size-category statistics.
  size_t allocated_small;

  size_t allocated_large;
};

// ***************************************************************************
// Extent data structures.

enum ChunkType {
  UNKNOWN_CHUNK,
  ZEROED_CHUNK,    // chunk only contains zeroes.
  ARENA_CHUNK,     // used to back arena runs created by arena_t::AllocRun.
  HUGE_CHUNK,      // used to back huge allocations (e.g. arena_t::MallocHuge).
  RECYCLED_CHUNK,  // chunk has been stored for future use by chunk_recycle.
};

// Tree of extents.
struct extent_node_t {
  union {
    // Linkage for the size/address-ordered tree for chunk recycling.
    RedBlackTreeNode<extent_node_t> mLinkBySize;
    // Arena id for huge allocations. It's meant to match mArena->mId,
    // which only holds true when the arena hasn't been disposed of.
    arena_id_t mArenaId;
  };

  // Linkage for the address-ordered tree.
  RedBlackTreeNode<extent_node_t> mLinkByAddr;

  // Pointer to the extent that this tree node is responsible for.
  void* mAddr;

  // Total region size.
  size_t mSize;

  union {
    // What type of chunk is there; used for chunk recycling.
    ChunkType mChunkType;

    // A pointer to the associated arena, for huge allocations.
    arena_t* mArena;
  };
};

struct ExtentTreeSzTrait {
  static RedBlackTreeNode<extent_node_t>& GetTreeNode(extent_node_t* aThis) {
    return aThis->mLinkBySize;
  }

  static inline Order Compare(extent_node_t* aNode, extent_node_t* aOther) {
    Order ret = CompareInt(aNode->mSize, aOther->mSize);
    return (ret != Order::eEqual) ? ret
                                  : CompareAddr(aNode->mAddr, aOther->mAddr);
  }
};

struct ExtentTreeTrait {
  static RedBlackTreeNode<extent_node_t>& GetTreeNode(extent_node_t* aThis) {
    return aThis->mLinkByAddr;
  }

  static inline Order Compare(extent_node_t* aNode, extent_node_t* aOther) {
    return CompareAddr(aNode->mAddr, aOther->mAddr);
  }
};

struct ExtentTreeBoundsTrait : public ExtentTreeTrait {
  static inline Order Compare(extent_node_t* aKey, extent_node_t* aNode) {
    uintptr_t key_addr = reinterpret_cast<uintptr_t>(aKey->mAddr);
    uintptr_t node_addr = reinterpret_cast<uintptr_t>(aNode->mAddr);
    size_t node_size = aNode->mSize;

    // Is aKey within aNode?
    if (node_addr <= key_addr && key_addr < node_addr + node_size) {
      return Order::eEqual;
    }

    return CompareAddr(aKey->mAddr, aNode->mAddr);
  }
};

// Describe size classes to which allocations are rounded up to.
// TODO: add large and huge types when the arena allocation code
// changes in a way that allows it to be beneficial.
class SizeClass {
 public:
  enum ClassType {
    Tiny,
    Quantum,
    QuantumWide,
    SubPage,
    Large,
  };

  explicit inline SizeClass(size_t aSize) {
    if (aSize <= kMaxTinyClass) {
      mType = Tiny;
      mSize = std::max(RoundUpPow2(aSize), kMinTinyClass);
    } else if (aSize <= kMaxQuantumClass) {
      mType = Quantum;
      mSize = QUANTUM_CEILING(aSize);
    } else if (aSize <= kMaxQuantumWideClass) {
      mType = QuantumWide;
      mSize = QUANTUM_WIDE_CEILING(aSize);
    } else if (aSize <= gMaxSubPageClass) {
      mType = SubPage;
      mSize = SUBPAGE_CEILING(aSize);
    } else if (aSize <= gMaxLargeClass) {
      mType = Large;
      mSize = PAGE_CEILING(aSize);
    } else {
      MOZ_MAKE_COMPILER_ASSUME_IS_UNREACHABLE("Invalid size");
    }
  }

  SizeClass& operator=(const SizeClass& aOther) = default;

  bool operator==(const SizeClass& aOther) { return aOther.mSize == mSize; }

  size_t Size() { return mSize; }

  ClassType Type() { return mType; }

  SizeClass Next() { return SizeClass(mSize + 1); }

 private:
  ClassType mType;
  size_t mSize;
};

// Fast division
//
// During deallocation we want to divide by the size class.  This class
// provides a routine and sets up a constant as follows.
//
// To divide by a number D that is not a power of two we multiply by (2^17 /
// D) and then right shift by 17 positions.
//
//   X / D
//
// becomes
//
//   (X * m) >> p
//
// Where m is calculated during the FastDivisor constructor similarly to:
//
//   m = 2^p / D
//
template <typename T>
class FastDivisor {
 private:
  // The shift amount (p) is chosen to minimise the size of m while
  // working for divisors up to 65536 in steps of 16.  I arrived at 17
  // experimentally.  I wanted a low number to minimise the range of m
  // so it can fit in a uint16_t, 16 didn't work but 17 worked perfectly.
  //
  // We'd need to increase this if we allocated memory on smaller boundaries
  // than 16.
  static const unsigned p = 17;

  // We can fit the inverted divisor in 16 bits, but we template it here for
  // convenience.
  T m;

 public:
  // Needed so mBins can be constructed.
  FastDivisor() : m(0) {}

  FastDivisor(unsigned div, unsigned max) {
    MOZ_ASSERT(div <= max);

    // divide_inv_shift is large enough.
    MOZ_ASSERT((1U << p) >= div);

    // The calculation here for m is formula 26 from Section
    // 10-9 "Unsigned Division by Divisors >= 1" in
    // Henry S. Warren, Jr.'s Hacker's Delight, 2nd Ed.
    unsigned m_ = ((1U << p) + div - 1 - (((1U << p) - 1) % div)) / div;

    // Make sure that max * m does not overflow.
    MOZ_DIAGNOSTIC_ASSERT(max < UINT_MAX / m_);

    MOZ_ASSERT(m_ <= std::numeric_limits<T>::max());
    m = static_cast<T>(m_);

    // Initialisation made m non-zero.
    MOZ_ASSERT(m);

    // Test that all the divisions in the range we expected would work.
#ifdef MOZ_DEBUG
    for (unsigned num = 0; num < max; num += div) {
      MOZ_ASSERT(num / div == divide(num));
    }
#endif
  }

  // Note that this always occurs in uint32_t regardless of m's type.  If m is
  // a uint16_t it will be zero-extended before the multiplication.  We also use
  // uint32_t rather than something that could possibly be larger because it is
  // most-likely the cheapest multiplication.
  inline uint32_t divide(uint32_t num) const {
    // Check that m was initialised.
    MOZ_ASSERT(m);
    return (num * m) >> p;
  }
};

template <typename T>
unsigned inline operator/(unsigned num, FastDivisor<T> divisor) {
  return divisor.divide(num);
}

// ***************************************************************************
// Radix tree data structures.
//
// The number of bits passed to the template is the number of significant bits
// in an address to do a radix lookup with.
//
// An address is looked up by splitting it in kBitsPerLevel bit chunks, except
// the most significant bits, where the bit chunk is kBitsAtLevel1 which can be
// different if Bits is not a multiple of kBitsPerLevel.
//
// With e.g. sizeof(void*)=4, Bits=16 and kBitsPerLevel=8, an address is split
// like the following:
// 0x12345678 -> mRoot[0x12][0x34]
template <size_t Bits>
class AddressRadixTree {
// Size of each radix tree node (as a power of 2).
// This impacts tree depth.
#ifdef HAVE_64BIT_BUILD
  static const size_t kNodeSize = kCacheLineSize;
#else
  static const size_t kNodeSize = 16_KiB;
#endif
  static const size_t kBitsPerLevel = LOG2(kNodeSize) - LOG2(sizeof(void*));
  static const size_t kBitsAtLevel1 =
      (Bits % kBitsPerLevel) ? Bits % kBitsPerLevel : kBitsPerLevel;
  static const size_t kHeight = (Bits + kBitsPerLevel - 1) / kBitsPerLevel;
  static_assert(kBitsAtLevel1 + (kHeight - 1) * kBitsPerLevel == Bits,
                "AddressRadixTree parameters don't work out");

  Mutex mLock MOZ_UNANNOTATED;
  void** mRoot;

 public:
  bool Init();

  inline void* Get(void* aAddr);

  // Returns whether the value was properly set.
  inline bool Set(void* aAddr, void* aValue);

  inline bool Unset(void* aAddr) { return Set(aAddr, nullptr); }

 private:
  inline void** GetSlot(void* aAddr, bool aCreate = false);
};

// ***************************************************************************
// Arena data structures.

struct arena_bin_t;

struct ArenaChunkMapLink {
  static RedBlackTreeNode<arena_chunk_map_t>& GetTreeNode(
      arena_chunk_map_t* aThis) {
    return aThis->link;
  }
};

struct ArenaRunTreeTrait : public ArenaChunkMapLink {
  static inline Order Compare(arena_chunk_map_t* aNode,
                              arena_chunk_map_t* aOther) {
    MOZ_ASSERT(aNode);
    MOZ_ASSERT(aOther);
    return CompareAddr(aNode, aOther);
  }
};

struct ArenaAvailTreeTrait : public ArenaChunkMapLink {
  static inline Order Compare(arena_chunk_map_t* aNode,
                              arena_chunk_map_t* aOther) {
    size_t size1 = aNode->bits & ~gPageSizeMask;
    size_t size2 = aOther->bits & ~gPageSizeMask;
    Order ret = CompareInt(size1, size2);
    return (ret != Order::eEqual)
               ? ret
               : CompareAddr((aNode->bits & CHUNK_MAP_KEY) ? nullptr : aNode,
                             aOther);
  }
};

struct ArenaDirtyChunkTrait {
  static RedBlackTreeNode<arena_chunk_t>& GetTreeNode(arena_chunk_t* aThis) {
    return aThis->link_dirty;
  }

  static inline Order Compare(arena_chunk_t* aNode, arena_chunk_t* aOther) {
    MOZ_ASSERT(aNode);
    MOZ_ASSERT(aOther);
    return CompareAddr(aNode, aOther);
  }
};

#ifdef MALLOC_DOUBLE_PURGE
namespace mozilla {

template <>
struct GetDoublyLinkedListElement<arena_chunk_t> {
  static DoublyLinkedListElement<arena_chunk_t>& Get(arena_chunk_t* aThis) {
    return aThis->chunks_madvised_elem;
  }
};
}  // namespace mozilla
#endif

struct arena_run_t {
#if defined(MOZ_DIAGNOSTIC_ASSERT_ENABLED)
  uint32_t mMagic;
#  define ARENA_RUN_MAGIC 0x384adf93

  // On 64-bit platforms, having the arena_bin_t pointer following
  // the mMagic field means there's padding between both fields, making
  // the run header larger than necessary.
  // But when MOZ_DIAGNOSTIC_ASSERT_ENABLED is not set, starting the
  // header with this field followed by the arena_bin_t pointer yields
  // the same padding. We do want the mMagic field to appear first, so
  // depending whether MOZ_DIAGNOSTIC_ASSERT_ENABLED is set or not, we
  // move some field to avoid padding.

  // Number of free regions in run.
  unsigned mNumFree;
#endif

  // Bin this run is associated with.
  arena_bin_t* mBin;

  // Index of first element that might have a free region.
  unsigned mRegionsMinElement;

#if !defined(MOZ_DIAGNOSTIC_ASSERT_ENABLED)
  // Number of free regions in run.
  unsigned mNumFree;
#endif

  // Bitmask of in-use regions (0: in use, 1: free).
  unsigned mRegionsMask[];  // Dynamically sized.
};

struct arena_bin_t {
  // Current run being used to service allocations of this bin's size
  // class.
  arena_run_t* mCurrentRun;

  // Tree of non-full runs.  This tree is used when looking for an
  // existing run when mCurrentRun is no longer usable.  We choose the
  // non-full run that is lowest in memory; this policy tends to keep
  // objects packed well, and it can also help reduce the number of
  // almost-empty chunks.
  RedBlackTree<arena_chunk_map_t, ArenaRunTreeTrait> mNonFullRuns;

  // Bin's size class.
  size_t mSizeClass;

  // Total number of regions in a run for this bin's size class.
  uint32_t mRunNumRegions;

  // Number of elements in a run's mRegionsMask for this bin's size class.
  uint32_t mRunNumRegionsMask;

  // Offset of first region in a run for this bin's size class.
  uint32_t mRunFirstRegionOffset;

  // Current number of runs in this bin, full or otherwise.
  uint32_t mNumRuns;

  // A constant for fast division by size class.  This value is 16 bits wide so
  // it is placed last.
  FastDivisor<uint16_t> mSizeDivisor;

  // Total number of pages in a run for this bin's size class.
  uint8_t mRunSizePages;

  // Amount of overhead runs are allowed to have.
  static constexpr double kRunOverhead = 1.6_percent;
  static constexpr double kRunRelaxedOverhead = 2.4_percent;

  // Initialize a bin for the given size class.
  // The generated run sizes, for a page size of 4 KiB, are:
  //   size|run       size|run       size|run       size|run
  //  class|size     class|size     class|size     class|size
  //     4   4 KiB      8   4 KiB     16   4 KiB     32   4 KiB
  //    48   4 KiB     64   4 KiB     80   4 KiB     96   4 KiB
  //   112   4 KiB    128   8 KiB    144   4 KiB    160   8 KiB
  //   176   4 KiB    192   4 KiB    208   8 KiB    224   4 KiB
  //   240   8 KiB    256  16 KiB    272   8 KiB    288   4 KiB
  //   304  12 KiB    320  12 KiB    336   4 KiB    352   8 KiB
  //   368   4 KiB    384   8 KiB    400  20 KiB    416  16 KiB
  //   432  12 KiB    448   4 KiB    464  16 KiB    480   8 KiB
  //   496  20 KiB    512  32 KiB    768  16 KiB   1024  64 KiB
  //  1280  24 KiB   1536  32 KiB   1792  16 KiB   2048 128 KiB
  //  2304  16 KiB   2560  48 KiB   2816  36 KiB   3072  64 KiB
  //  3328  36 KiB   3584  32 KiB   3840  64 KiB
  inline void Init(SizeClass aSizeClass);
};

// We try to keep the above structure aligned with common cache lines sizes,
// often that's 64 bytes on x86 and ARM, we don't make assumptions for other
// architectures.
#if defined(__x86_64__) || defined(__aarch64__)
// On 64bit platforms this structure is often 48 bytes
// long, which means every other array element will be properly aligned.
static_assert(sizeof(arena_bin_t) == 48);
#elif defined(__x86__) || defined(__arm__)
static_assert(sizeof(arena_bin_t) == 32);
#endif

struct arena_t {
#if defined(MOZ_DIAGNOSTIC_ASSERT_ENABLED)
  uint32_t mMagic;
#  define ARENA_MAGIC 0x947d3d24
#endif

  // Linkage for the tree of arenas by id.
  RedBlackTreeNode<arena_t> mLink;

  // Arena id, that we keep away from the beginning of the struct so that
  // free list pointers in TypedBaseAlloc<arena_t> don't overflow in it,
  // and it keeps the value it had after the destructor.
  arena_id_t mId;

  // All operations on this arena require that lock be locked.  The MaybeMutex
  // class well elude locking if the arena is accessed from a single thread
  // only.
  MaybeMutex mLock MOZ_UNANNOTATED;

  arena_stats_t mStats;

 private:
  // Tree of dirty-page-containing chunks this arena manages.
  RedBlackTree<arena_chunk_t, ArenaDirtyChunkTrait> mChunksDirty;

#ifdef MALLOC_DOUBLE_PURGE
  // Head of a linked list of MADV_FREE'd-page-containing chunks this
  // arena manages.
  DoublyLinkedList<arena_chunk_t> mChunksMAdvised;
#endif

  // In order to avoid rapid chunk allocation/deallocation when an arena
  // oscillates right on the cusp of needing a new chunk, cache the most
  // recently freed chunk.  The spare is left in the arena's chunk trees
  // until it is deleted.
  //
  // There is one spare chunk per arena, rather than one spare total, in
  // order to avoid interactions between multiple threads that could make
  // a single spare inadequate.
  arena_chunk_t* mSpare;

  // A per-arena opt-in to randomize the offset of small allocations
  bool mRandomizeSmallAllocations;

  // Whether this is a private arena. Multiple public arenas are just a
  // performance optimization and not a safety feature.
  //
  // Since, for example, we don't want thread-local arenas to grow too much, we
  // use the default arena for bigger allocations. We use this member to allow
  // realloc() to switch out of our arena if needed (which is not allowed for
  // private arenas for security).
  bool mIsPrivate;

  // A pseudorandom number generator. Initially null, it gets initialized
  // on first use to avoid recursive malloc initialization (e.g. on OSX
  // arc4random allocates memory).
  mozilla::non_crypto::XorShift128PlusRNG* mPRNG;
  bool mIsPRNGInitializing;

 public:
  // Current count of pages within unused runs that are potentially
  // dirty, and for which madvise(... MADV_FREE) has not been called.  By
  // tracking this, we can institute a limit on how much dirty unused
  // memory is mapped for each arena.
  size_t mNumDirty;

  // The current number of pages that are available without a system call (but
  // probably a page fault).
  size_t mNumMAdvised;
  size_t mNumFresh;

  // Maximum value allowed for mNumDirty.
  size_t mMaxDirty;

  int32_t mMaxDirtyIncreaseOverride;
  int32_t mMaxDirtyDecreaseOverride;

 private:
  // Size/address-ordered tree of this arena's available runs.  This tree
  // is used for first-best-fit run allocation.
  RedBlackTree<arena_chunk_map_t, ArenaAvailTreeTrait> mRunsAvail;

 public:
  // mBins is used to store rings of free regions of the following sizes,
  // assuming a 16-byte quantum, 4kB pagesize, and default MALLOC_OPTIONS.
  //
  //  | mBins[i] | size |
  //  +----------+------+
  //  |       0  |    2 |
  //  |       1  |    4 |
  //  |       2  |    8 |
  //  +----------+------+
  //  |       3  |   16 |
  //  |       4  |   32 |
  //  |       5  |   48 |
  //  |       6  |   64 |
  //  |          :      :
  //  |          :      :
  //  |      33  |  496 |
  //  |      34  |  512 |
  //  +----------+------+
  //  |      35  |  768 |
  //  |      36  | 1024 |
  //  |          :      :
  //  |          :      :
  //  |      46  | 3584 |
  //  |      47  | 3840 |
  //  +----------+------+
  arena_bin_t mBins[];  // Dynamically sized.

  explicit arena_t(arena_params_t* aParams, bool aIsPrivate);
  ~arena_t();

 private:
  void InitChunk(arena_chunk_t* aChunk, size_t aMinCommittedPages);

  // This may return a chunk that should be destroyed with chunk_dealloc outside
  // of the arena lock.  It is not the same chunk as was passed in (since that
  // chunk now becomes mSpare).
  [[nodiscard]] arena_chunk_t* DeallocChunk(arena_chunk_t* aChunk);

  arena_run_t* AllocRun(size_t aSize, bool aLarge, bool aZero);

  arena_chunk_t* DallocRun(arena_run_t* aRun, bool aDirty);

  [[nodiscard]] bool SplitRun(arena_run_t* aRun, size_t aSize, bool aLarge,
                              bool aZero);

  void TrimRunHead(arena_chunk_t* aChunk, arena_run_t* aRun, size_t aOldSize,
                   size_t aNewSize);

  void TrimRunTail(arena_chunk_t* aChunk, arena_run_t* aRun, size_t aOldSize,
                   size_t aNewSize, bool dirty);

  arena_run_t* GetNonFullBinRun(arena_bin_t* aBin);

  inline uint8_t FindFreeBitInMask(uint32_t aMask, uint32_t& aRng);

  inline void* ArenaRunRegAlloc(arena_run_t* aRun, arena_bin_t* aBin);

  inline void* MallocSmall(size_t aSize, bool aZero);

  void* MallocLarge(size_t aSize, bool aZero);

  void* MallocHuge(size_t aSize, bool aZero);

  void* PallocLarge(size_t aAlignment, size_t aSize, size_t aAllocSize);

  void* PallocHuge(size_t aSize, size_t aAlignment, bool aZero);

  void RallocShrinkLarge(arena_chunk_t* aChunk, void* aPtr, size_t aSize,
                         size_t aOldSize);

  bool RallocGrowLarge(arena_chunk_t* aChunk, void* aPtr, size_t aSize,
                       size_t aOldSize);

  void* RallocSmallOrLarge(void* aPtr, size_t aSize, size_t aOldSize);

  void* RallocHuge(void* aPtr, size_t aSize, size_t aOldSize);

 public:
  inline void* Malloc(size_t aSize, bool aZero);

  void* Palloc(size_t aAlignment, size_t aSize);

  // This may return a chunk that should be destroyed with chunk_dealloc outside
  // of the arena lock.  It is not the same chunk as was passed in (since that
  // chunk now becomes mSpare).
  [[nodiscard]] inline arena_chunk_t* DallocSmall(arena_chunk_t* aChunk,
                                                  void* aPtr,
                                                  arena_chunk_map_t* aMapElm);

  [[nodiscard]] arena_chunk_t* DallocLarge(arena_chunk_t* aChunk, void* aPtr);

  void* Ralloc(void* aPtr, size_t aSize, size_t aOldSize);

  size_t EffectiveMaxDirty();

#ifdef MALLOC_DECOMMIT
  // During a commit operation (for aReqPages) we have the opportunity of
  // commiting at most aRemPages additional pages.  How many should we commit to
  // amortise system calls?
  size_t ExtraCommitPages(size_t aReqPages, size_t aRemainingPages);
#endif

  // Passing one means purging all.
  void Purge(size_t aMaxDirty);

  void HardPurge();

  bool IsMainThreadOnly() const { return !mLock.LockIsEnabled(); }

  void* operator new(size_t aCount) = delete;

  void* operator new(size_t aCount, const fallible_t&) noexcept;

  void operator delete(void*);
};

struct ArenaTreeTrait {
  static RedBlackTreeNode<arena_t>& GetTreeNode(arena_t* aThis) {
    return aThis->mLink;
  }

  static inline Order Compare(arena_t* aNode, arena_t* aOther) {
    MOZ_ASSERT(aNode);
    MOZ_ASSERT(aOther);
    return CompareInt(aNode->mId, aOther->mId);
  }
};

// Bookkeeping for all the arenas used by the allocator.
// Arenas are separated in two categories:
// - "private" arenas, used through the moz_arena_* API
// - all the other arenas: the default arena, and thread-local arenas,
//   used by the standard API.
class ArenaCollection {
 public:
  bool Init() {
    mArenas.Init();
    mPrivateArenas.Init();
    mMainThreadArenas.Init();
    arena_params_t params;
    // The main arena allows more dirty pages than the default for other arenas.
    params.mMaxDirty = opt_dirty_max;
    mDefaultArena =
        mLock.Init() ? CreateArena(/* aIsPrivate = */ false, &params) : nullptr;
    return bool(mDefaultArena);
  }

  inline arena_t* GetById(arena_id_t aArenaId, bool aIsPrivate);

  arena_t* CreateArena(bool aIsPrivate, arena_params_t* aParams);

  void DisposeArena(arena_t* aArena) {
    MutexAutoLock lock(mLock);
    Tree& tree =
        aArena->IsMainThreadOnly() ? mMainThreadArenas : mPrivateArenas;

    MOZ_RELEASE_ASSERT(tree.Search(aArena), "Arena not in tree");
    tree.Remove(aArena);
    delete aArena;
  }

  void SetDefaultMaxDirtyPageModifier(int32_t aModifier) {
    mDefaultMaxDirtyPageModifier = aModifier;
  }
  int32_t DefaultMaxDirtyPageModifier() { return mDefaultMaxDirtyPageModifier; }

  using Tree = RedBlackTree<arena_t, ArenaTreeTrait>;

  struct Iterator : Tree::Iterator {
    explicit Iterator(Tree* aTree, Tree* aSecondTree,
                      Tree* aThirdTree = nullptr)
        : Tree::Iterator(aTree),
          mSecondTree(aSecondTree),
          mThirdTree(aThirdTree) {}

    Item<Iterator> begin() {
      return Item<Iterator>(this, *Tree::Iterator::begin());
    }

    Item<Iterator> end() { return Item<Iterator>(this, nullptr); }

    arena_t* Next() {
      arena_t* result = Tree::Iterator::Next();
      if (!result && mSecondTree) {
        new (this) Iterator(mSecondTree, mThirdTree);
        result = *Tree::Iterator::begin();
      }
      return result;
    }

   private:
    Tree* mSecondTree;
    Tree* mThirdTree;
  };

  Iterator iter() {
    if (IsOnMainThreadWeak()) {
      return Iterator(&mArenas, &mPrivateArenas, &mMainThreadArenas);
    }
    return Iterator(&mArenas, &mPrivateArenas);
  }

  inline arena_t* GetDefault() { return mDefaultArena; }

  Mutex mLock MOZ_UNANNOTATED;

  // We're running on the main thread which is set by a call to SetMainThread().
  bool IsOnMainThread() const {
    return mMainThreadId.isSome() &&
           ThreadIdEqual(mMainThreadId.value(), GetThreadId());
  }

  // We're running on the main thread or SetMainThread() has never been called.
  bool IsOnMainThreadWeak() const {
    return mMainThreadId.isNothing() || IsOnMainThread();
  }

  // After a fork set the new thread ID in the child.
  void ResetMainThread() {
    // The post fork handler in the child can run from a MacOS worker thread,
    // so we can't set our main thread to it here.  Instead we have to clear it.
    mMainThreadId = Nothing();
  }

  void SetMainThread() {
    MutexAutoLock lock(mLock);
    MOZ_ASSERT(mMainThreadId.isNothing());
    mMainThreadId = Some(GetThreadId());
  }

 private:
  const static arena_id_t MAIN_THREAD_ARENA_BIT = 0x1;

  inline arena_t* GetByIdInternal(Tree& aTree, arena_id_t aArenaId);

  arena_id_t MakeRandArenaId(bool aIsMainThreadOnly) const;
  static bool ArenaIdIsMainThreadOnly(arena_id_t aArenaId) {
    return aArenaId & MAIN_THREAD_ARENA_BIT;
  }

  arena_t* mDefaultArena;
  arena_id_t mLastPublicArenaId;

  // Accessing mArenas and mPrivateArenas can only be done while holding mLock.
  // Since mMainThreadArenas can only be used from the main thread, it can be
  // accessed without a lock which is why it is a seperate tree.
  Tree mArenas;
  Tree mPrivateArenas;
  Tree mMainThreadArenas;
  Atomic<int32_t, MemoryOrdering::Relaxed> mDefaultMaxDirtyPageModifier;
  Maybe<ThreadId> mMainThreadId;
};

static ArenaCollection gArenas;

// ******
// Chunks.
static AddressRadixTree<(sizeof(void*) << 3) - LOG2(kChunkSize)> gChunkRTree;

// Protects chunk-related data structures.
static Mutex chunks_mtx;

// Trees of chunks that were previously allocated (trees differ only in node
// ordering).  These are used when allocating chunks, in an attempt to re-use
// address space.  Depending on function, different tree orderings are needed,
// which is why there are two trees with the same contents.
static RedBlackTree<extent_node_t, ExtentTreeSzTrait> gChunksBySize
    MOZ_GUARDED_BY(chunks_mtx);
static RedBlackTree<extent_node_t, ExtentTreeTrait> gChunksByAddress
    MOZ_GUARDED_BY(chunks_mtx);

// Protects huge allocation-related data structures.
static Mutex huge_mtx;

// Tree of chunks that are stand-alone huge allocations.
static RedBlackTree<extent_node_t, ExtentTreeTrait> huge
    MOZ_GUARDED_BY(huge_mtx);

// Huge allocation statistics.
static size_t huge_allocated MOZ_GUARDED_BY(huge_mtx);
static size_t huge_mapped MOZ_GUARDED_BY(huge_mtx);

// **************************
// base (internal allocation).

static Mutex base_mtx;

// Current pages that are being used for internal memory allocations.  These
// pages are carved up in cacheline-size quanta, so that there is no chance of
// false cache line sharing.
static void* base_pages MOZ_GUARDED_BY(base_mtx);
static void* base_next_addr MOZ_GUARDED_BY(base_mtx);
static void* base_next_decommitted MOZ_GUARDED_BY(base_mtx);
// Address immediately past base_pages.
static void* base_past_addr MOZ_GUARDED_BY(base_mtx);
static size_t base_mapped MOZ_GUARDED_BY(base_mtx);
static size_t base_committed MOZ_GUARDED_BY(base_mtx);

// ******
// Arenas.

// The arena associated with the current thread (per
// jemalloc_thread_local_arena) On OSX, __thread/thread_local circles back
// calling malloc to allocate storage on first access on each thread, which
// leads to an infinite loop, but pthread-based TLS somehow doesn't have this
// problem.
#if !defined(XP_DARWIN)
static MOZ_THREAD_LOCAL(arena_t*) thread_arena;
#else
static detail::ThreadLocal<arena_t*, detail::ThreadLocalKeyStorage>
    thread_arena;
#endif

// *****************************
// Runtime configuration options.

#ifdef MALLOC_RUNTIME_CONFIG
#  define MALLOC_RUNTIME_VAR static
#else
#  define MALLOC_RUNTIME_VAR static const
#endif

enum PoisonType {
  NONE,
  SOME,
  ALL,
};

MALLOC_RUNTIME_VAR bool opt_junk = false;
MALLOC_RUNTIME_VAR bool opt_zero = false;

#ifdef EARLY_BETA_OR_EARLIER
MALLOC_RUNTIME_VAR PoisonType opt_poison = ALL;
#else
MALLOC_RUNTIME_VAR PoisonType opt_poison = SOME;
#endif

// Keep this larger than and ideally a multiple of kCacheLineSize;
MALLOC_RUNTIME_VAR size_t opt_poison_size = 256;
#ifndef MALLOC_RUNTIME_CONFIG
static_assert(opt_poison_size >= kCacheLineSize);
static_assert((opt_poison_size % kCacheLineSize) == 0);
#endif

static bool opt_randomize_small = true;

// ***************************************************************************
// Begin forward declarations.

static void* chunk_alloc(size_t aSize, size_t aAlignment, bool aBase);
static void chunk_dealloc(void* aChunk, size_t aSize, ChunkType aType);
#ifdef MOZ_DEBUG
static void chunk_assert_zero(void* aPtr, size_t aSize);
#endif
static void huge_dalloc(void* aPtr, arena_t* aArena);
static bool malloc_init_hard();

#ifndef XP_WIN
#  ifdef XP_DARWIN
#    define FORK_HOOK extern "C"
#  else
#    define FORK_HOOK static
#  endif
FORK_HOOK void _malloc_prefork(void);
FORK_HOOK void _malloc_postfork_parent(void);
FORK_HOOK void _malloc_postfork_child(void);
#  ifdef XP_DARWIN
FORK_HOOK void _malloc_postfork(void);
#  endif
#endif

// End forward declarations.
// ***************************************************************************

// FreeBSD's pthreads implementation calls malloc(3), so the malloc
// implementation has to take pains to avoid infinite recursion during
// initialization.
// Returns whether the allocator was successfully initialized.
static inline bool malloc_init() {
  if (!malloc_initialized) {
    return malloc_init_hard();
  }
  return true;
}

static void _malloc_message(const char* p) {
#if !defined(XP_WIN)
#  define _write write
#endif
  // Pretend to check _write() errors to suppress gcc warnings about
  // warn_unused_result annotations in some versions of glibc headers.
  if (_write(STDERR_FILENO, p, (unsigned int)strlen(p)) < 0) {
    return;
  }
}

template <typename... Args>
static void _malloc_message(const char* p, Args... args) {
  _malloc_message(p);
  _malloc_message(args...);
}

#ifdef ANDROID
// Android's pthread.h does not declare pthread_atfork() until SDK 21.
extern "C" MOZ_EXPORT int pthread_atfork(void (*)(void), void (*)(void),
                                         void (*)(void));
#endif

// ***************************************************************************
// Begin Utility functions/macros.

// Return the chunk address for allocation address a.
static inline arena_chunk_t* GetChunkForPtr(const void* aPtr) {
  return (arena_chunk_t*)(uintptr_t(aPtr) & ~kChunkSizeMask);
}

// Return the chunk offset of address a.
static inline size_t GetChunkOffsetForPtr(const void* aPtr) {
  return (size_t)(uintptr_t(aPtr) & kChunkSizeMask);
}

static inline const char* _getprogname(void) { return "<jemalloc>"; }

static inline void MaybePoison(void* aPtr, size_t aSize) {
  size_t size;
  switch (opt_poison) {
    case NONE:
      return;
    case SOME:
      size = std::min(aSize, opt_poison_size);
      break;
    case ALL:
      size = aSize;
      break;
  }
  MOZ_ASSERT(size != 0 && size <= aSize);
  memset(aPtr, kAllocPoison, size);
}

// Fill the given range of memory with zeroes or junk depending on opt_junk and
// opt_zero.
static inline void ApplyZeroOrJunk(void* aPtr, size_t aSize) {
  if (opt_junk) {
    memset(aPtr, kAllocJunk, aSize);
  } else if (opt_zero) {
    memset(aPtr, 0, aSize);
  }
}

// On Windows, delay crashing on OOM.
#ifdef XP_WIN

// Implementation of VirtualAlloc wrapper (bug 1716727).
namespace MozAllocRetries {

// Maximum retry count on OOM.
constexpr size_t kMaxAttempts = 10;
// Minimum delay time between retries. (The actual delay time may be larger. See
// Microsoft's documentation for ::Sleep() for details.)
constexpr size_t kDelayMs = 50;

using StallSpecs = ::mozilla::StallSpecs;

static constexpr StallSpecs maxStall = {.maxAttempts = kMaxAttempts,
                                        .delayMs = kDelayMs};

static inline StallSpecs GetStallSpecs() {
#  if defined(JS_STANDALONE)
  // GetGeckoProcessType() isn't available in this configuration. (SpiderMonkey
  // on Windows mostly skips this in favor of directly calling ::VirtualAlloc(),
  // though, so it's probably not going to matter whether we stall here or not.)
  return maxStall;
#  else
  switch (GetGeckoProcessType()) {
    // For the main process, stall for the maximum permissible time period. (The
    // main process is the most important one to keep alive.)
    case GeckoProcessType::GeckoProcessType_Default:
      return maxStall;

    // For all other process types, stall for at most half as long.
    default:
      return {.maxAttempts = maxStall.maxAttempts / 2,
              .delayMs = maxStall.delayMs};
  }
#  endif
}

// Drop-in wrapper around VirtualAlloc. When out of memory, may attempt to stall
// and retry rather than returning immediately, in hopes that the page file is
// about to be expanded by Windows.
//
// Ref:
// https://docs.microsoft.com/en-us/troubleshoot/windows-client/performance/slow-page-file-growth-memory-allocation-errors
[[nodiscard]] void* MozVirtualAlloc(LPVOID lpAddress, SIZE_T dwSize,
                                    DWORD flAllocationType, DWORD flProtect) {
  DWORD const lastError = ::GetLastError();

  constexpr auto IsOOMError = [] {
    switch (::GetLastError()) {
      // This is the usual error result from VirtualAlloc for OOM.
      case ERROR_COMMITMENT_LIMIT:
      // Although rare, this has also been observed in low-memory situations.
      // (Presumably this means Windows can't allocate enough kernel-side space
      // for its own internal representation of the process's virtual address
      // space.)
      case ERROR_NOT_ENOUGH_MEMORY:
        return true;
    }
    return false;
  };

  {
    void* ptr = ::VirtualAlloc(lpAddress, dwSize, flAllocationType, flProtect);
    if (MOZ_LIKELY(ptr)) return ptr;

    // We can't do anything for errors other than OOM...
    if (!IsOOMError()) return nullptr;
    // ... or if this wasn't a request to commit memory in the first place.
    // (This function has no strategy for resolving MEM_RESERVE failures.)
    if (!(flAllocationType & MEM_COMMIT)) return nullptr;
  }

  // Retry as many times as desired (possibly zero).
  const StallSpecs stallSpecs = GetStallSpecs();

  const auto ret =
      stallSpecs.StallAndRetry(&::Sleep, [&]() -> std::optional<void*> {
        void* ptr =
            ::VirtualAlloc(lpAddress, dwSize, flAllocationType, flProtect);

        if (ptr) {
          // The OOM status has been handled, and should not be reported to
          // telemetry.
          if (IsOOMError()) {
            ::SetLastError(lastError);
          }
          return ptr;
        }

        // Failure for some reason other than OOM.
        if (!IsOOMError()) {
          return nullptr;
        }

        return std::nullopt;
      });

  return ret.value_or(nullptr);
}
}  // namespace MozAllocRetries

using MozAllocRetries::MozVirtualAlloc;

namespace mozilla {
MOZ_JEMALLOC_API StallSpecs GetAllocatorStallSpecs() {
  return ::MozAllocRetries::GetStallSpecs();
}
}  // namespace mozilla

#endif  // XP_WIN

// ***************************************************************************

static inline void pages_decommit(void* aAddr, size_t aSize) {
#ifdef XP_WIN
  // The region starting at addr may have been allocated in multiple calls
  // to VirtualAlloc and recycled, so decommitting the entire region in one
  // go may not be valid. However, since we allocate at least a chunk at a
  // time, we may touch any region in chunksized increments.
  size_t pages_size = std::min(aSize, kChunkSize - GetChunkOffsetForPtr(aAddr));
  while (aSize > 0) {
    // This will cause Access Violation on read and write and thus act as a
    // guard page or region as well.
    if (!VirtualFree(aAddr, pages_size, MEM_DECOMMIT)) {
      MOZ_CRASH();
    }
    aAddr = (void*)((uintptr_t)aAddr + pages_size);
    aSize -= pages_size;
    pages_size = std::min(aSize, kChunkSize);
  }
#else
  if (mmap(aAddr, aSize, PROT_NONE, MAP_FIXED | MAP_PRIVATE | MAP_ANON, -1,
           0) == MAP_FAILED) {
    // We'd like to report the OOM for our tooling, but we can't allocate
    // memory at this point, so avoid the use of printf.
    const char out_of_mappings[] =
        "[unhandlable oom] Failed to mmap, likely no more mappings "
        "available " __FILE__ " : " MOZ_STRINGIFY(__LINE__);
    if (errno == ENOMEM) {
#  ifndef ANDROID
      fputs(out_of_mappings, stderr);
      fflush(stderr);
#  endif
      MOZ_CRASH_ANNOTATE(out_of_mappings);
    }
    MOZ_REALLY_CRASH(__LINE__);
  }
  MozTagAnonymousMemory(aAddr, aSize, "jemalloc-decommitted");
#endif
}

// Commit pages. Returns whether pages were committed.
[[nodiscard]] static inline bool pages_commit(void* aAddr, size_t aSize) {
#ifdef XP_WIN
  // The region starting at addr may have been allocated in multiple calls
  // to VirtualAlloc and recycled, so committing the entire region in one
  // go may not be valid. However, since we allocate at least a chunk at a
  // time, we may touch any region in chunksized increments.
  size_t pages_size = std::min(aSize, kChunkSize - GetChunkOffsetForPtr(aAddr));
  while (aSize > 0) {
    if (!MozVirtualAlloc(aAddr, pages_size, MEM_COMMIT, PAGE_READWRITE)) {
      return false;
    }
    aAddr = (void*)((uintptr_t)aAddr + pages_size);
    aSize -= pages_size;
    pages_size = std::min(aSize, kChunkSize);
  }
#else
  if (mmap(aAddr, aSize, PROT_READ | PROT_WRITE,
           MAP_FIXED | MAP_PRIVATE | MAP_ANON, -1, 0) == MAP_FAILED) {
    return false;
  }
  MozTagAnonymousMemory(aAddr, aSize, "jemalloc");
#endif
  return true;
}

static bool base_pages_alloc(size_t minsize) MOZ_REQUIRES(base_mtx) {
  size_t csize;
  size_t pminsize;

  MOZ_ASSERT(minsize != 0);
  csize = CHUNK_CEILING(minsize);
  base_pages = chunk_alloc(csize, kChunkSize, true);
  if (!base_pages) {
    return true;
  }
  base_next_addr = base_pages;
  base_past_addr = (void*)((uintptr_t)base_pages + csize);
  // Leave enough pages for minsize committed, since otherwise they would
  // have to be immediately recommitted.
  pminsize = PAGE_CEILING(minsize);
  base_next_decommitted = (void*)((uintptr_t)base_pages + pminsize);
  if (pminsize < csize) {
    pages_decommit(base_next_decommitted, csize - pminsize);
  }
  base_mapped += csize;
  base_committed += pminsize;

  return false;
}

static void* base_alloc(size_t aSize) {
  void* ret;
  size_t csize;

  // Round size up to nearest multiple of the cacheline size.
  csize = CACHELINE_CEILING(aSize);

  MutexAutoLock lock(base_mtx);
  // Make sure there's enough space for the allocation.
  if ((uintptr_t)base_next_addr + csize > (uintptr_t)base_past_addr) {
    if (base_pages_alloc(csize)) {
      return nullptr;
    }
  }
  // Allocate.
  ret = base_next_addr;
  base_next_addr = (void*)((uintptr_t)base_next_addr + csize);
  // Make sure enough pages are committed for the new allocation.
  if ((uintptr_t)base_next_addr > (uintptr_t)base_next_decommitted) {
    void* pbase_next_addr = (void*)(PAGE_CEILING((uintptr_t)base_next_addr));

    if (!pages_commit(
            base_next_decommitted,
            (uintptr_t)pbase_next_addr - (uintptr_t)base_next_decommitted)) {
      return nullptr;
    }

    base_committed +=
        (uintptr_t)pbase_next_addr - (uintptr_t)base_next_decommitted;
    base_next_decommitted = pbase_next_addr;
  }

  return ret;
}

static void* base_calloc(size_t aNumber, size_t aSize) {
  void* ret = base_alloc(aNumber * aSize);
  if (ret) {
    memset(ret, 0, aNumber * aSize);
  }
  return ret;
}

// A specialization of the base allocator with a free list.
template <typename T>
struct TypedBaseAlloc {
  static T* sFirstFree;

  static size_t size_of() { return sizeof(T); }

  static T* alloc() {
    T* ret;

    base_mtx.Lock();
    if (sFirstFree) {
      ret = sFirstFree;
      sFirstFree = *(T**)ret;
      base_mtx.Unlock();
    } else {
      base_mtx.Unlock();
      ret = (T*)base_alloc(size_of());
    }

    return ret;
  }

  static void dealloc(T* aNode) {
    MutexAutoLock lock(base_mtx);
    *(T**)aNode = sFirstFree;
    sFirstFree = aNode;
  }
};

using ExtentAlloc = TypedBaseAlloc<extent_node_t>;

template <>
extent_node_t* ExtentAlloc::sFirstFree = nullptr;

template <>
arena_t* TypedBaseAlloc<arena_t>::sFirstFree = nullptr;

template <>
size_t TypedBaseAlloc<arena_t>::size_of() {
  // Allocate enough space for trailing bins.
  return sizeof(arena_t) + (sizeof(arena_bin_t) * NUM_SMALL_CLASSES);
}

template <typename T>
struct BaseAllocFreePolicy {
  void operator()(T* aPtr) { TypedBaseAlloc<T>::dealloc(aPtr); }
};

using UniqueBaseNode =
    UniquePtr<extent_node_t, BaseAllocFreePolicy<extent_node_t>>;

// End Utility functions/macros.
// ***************************************************************************
// Begin chunk management functions.

#ifdef XP_WIN

static void* pages_map(void* aAddr, size_t aSize) {
  void* ret = nullptr;
  ret = MozVirtualAlloc(aAddr, aSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
  return ret;
}

static void pages_unmap(void* aAddr, size_t aSize) {
  if (VirtualFree(aAddr, 0, MEM_RELEASE) == 0) {
    _malloc_message(_getprogname(), ": (malloc) Error in VirtualFree()\n");
  }
}
#else

static void pages_unmap(void* aAddr, size_t aSize) {
  if (munmap(aAddr, aSize) == -1) {
    char buf[64];

    if (strerror_r(errno, buf, sizeof(buf)) == 0) {
      _malloc_message(_getprogname(), ": (malloc) Error in munmap(): ", buf,
                      "\n");
    }
  }
}

static void* pages_map(void* aAddr, size_t aSize) {
  void* ret;
#  if defined(__ia64__) || \
      (defined(__sparc__) && defined(__arch64__) && defined(__linux__))
  // The JS engine assumes that all allocated pointers have their high 17 bits
  // clear, which ia64's mmap doesn't support directly. However, we can emulate
  // it by passing mmap an "addr" parameter with those bits clear. The mmap will
  // return that address, or the nearest available memory above that address,
  // providing a near-guarantee that those bits are clear. If they are not, we
  // return nullptr below to indicate out-of-memory.
  //
  // The addr is chosen as 0x0000070000000000, which still allows about 120TB of
  // virtual address space.
  //
  // See Bug 589735 for more information.
  bool check_placement = true;
  if (!aAddr) {
    aAddr = (void*)0x0000070000000000;
    check_placement = false;
  }
#  endif

#  if defined(__sparc__) && defined(__arch64__) && defined(__linux__)
  const uintptr_t start = 0x0000070000000000ULL;
  const uintptr_t end = 0x0000800000000000ULL;

  // Copied from js/src/gc/Memory.cpp and adapted for this source
  uintptr_t hint;
  void* region = MAP_FAILED;
  for (hint = start; region == MAP_FAILED && hint + aSize <= end;
       hint += kChunkSize) {
    region = mmap((void*)hint, aSize, PROT_READ | PROT_WRITE,
                  MAP_PRIVATE | MAP_ANON, -1, 0);
    if (region != MAP_FAILED) {
      if (((size_t)region + (aSize - 1)) & 0xffff800000000000) {
        if (munmap(region, aSize)) {
          MOZ_ASSERT(errno == ENOMEM);
        }
        region = MAP_FAILED;
      }
    }
  }
  ret = region;
#  else
  // We don't use MAP_FIXED here, because it can cause the *replacement*
  // of existing mappings, and we only want to create new mappings.
  ret =
      mmap(aAddr, aSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
  MOZ_ASSERT(ret);
#  endif
  if (ret == MAP_FAILED) {
    ret = nullptr;
  }
#  if defined(__ia64__) || \
      (defined(__sparc__) && defined(__arch64__) && defined(__linux__))
  // If the allocated memory doesn't have its upper 17 bits clear, consider it
  // as out of memory.
  else if ((long long)ret & 0xffff800000000000) {
    munmap(ret, aSize);
    ret = nullptr;
  }
  // If the caller requested a specific memory location, verify that's what mmap
  // returned.
  else if (check_placement && ret != aAddr) {
#  else
  else if (aAddr && ret != aAddr) {
#  endif
    // We succeeded in mapping memory, but not in the right place.
    pages_unmap(ret, aSize);
    ret = nullptr;
  }
  if (ret) {
    MozTagAnonymousMemory(ret, aSize, "jemalloc");
  }

#  if defined(__ia64__) || \
      (defined(__sparc__) && defined(__arch64__) && defined(__linux__))
  MOZ_ASSERT(!ret || (!check_placement && ret) ||
             (check_placement && ret == aAddr));
#  else
  MOZ_ASSERT(!ret || (!aAddr && ret != aAddr) || (aAddr && ret == aAddr));
#  endif
  return ret;
}
#endif

#ifdef XP_DARWIN
#  define VM_COPY_MIN kChunkSize
static inline void pages_copy(void* dest, const void* src, size_t n) {
  MOZ_ASSERT((void*)((uintptr_t)dest & ~gPageSizeMask) == dest);
  MOZ_ASSERT(n >= VM_COPY_MIN);
  MOZ_ASSERT((void*)((uintptr_t)src & ~gPageSizeMask) == src);

  kern_return_t r = vm_copy(mach_task_self(), (vm_address_t)src, (vm_size_t)n,
                            (vm_address_t)dest);
  if (r != KERN_SUCCESS) {
    MOZ_CRASH("vm_copy() failed");
  }
}

#endif

template <size_t Bits>
bool AddressRadixTree<Bits>::Init() {
  mLock.Init();
  mRoot = (void**)base_calloc(1 << kBitsAtLevel1, sizeof(void*));
  return mRoot;
}

template <size_t Bits>
void** AddressRadixTree<Bits>::GetSlot(void* aKey, bool aCreate) {
  uintptr_t key = reinterpret_cast<uintptr_t>(aKey);
  uintptr_t subkey;
  unsigned i, lshift, height, bits;
  void** node;
  void** child;

  for (i = lshift = 0, height = kHeight, node = mRoot; i < height - 1;
       i++, lshift += bits, node = child) {
    bits = i ? kBitsPerLevel : kBitsAtLevel1;
    subkey = (key << lshift) >> ((sizeof(void*) << 3) - bits);
    child = (void**)node[subkey];
    if (!child && aCreate) {
      child = (void**)base_calloc(1 << kBitsPerLevel, sizeof(void*));
      if (child) {
        node[subkey] = child;
      }
    }
    if (!child) {
      return nullptr;
    }
  }

  // node is a leaf, so it contains values rather than node
  // pointers.
  bits = i ? kBitsPerLevel : kBitsAtLevel1;
  subkey = (key << lshift) >> ((sizeof(void*) << 3) - bits);
  return &node[subkey];
}

template <size_t Bits>
void* AddressRadixTree<Bits>::Get(void* aKey) {
  void* ret = nullptr;

  void** slot = GetSlot(aKey);

  if (slot) {
    ret = *slot;
  }
#ifdef MOZ_DEBUG
  MutexAutoLock lock(mLock);

  // Suppose that it were possible for a jemalloc-allocated chunk to be
  // munmap()ped, followed by a different allocator in another thread re-using
  // overlapping virtual memory, all without invalidating the cached rtree
  // value.  The result would be a false positive (the rtree would claim that
  // jemalloc owns memory that it had actually discarded).  I don't think this
  // scenario is possible, but the following assertion is a prudent sanity
  // check.
  if (!slot) {
    // In case a slot has been created in the meantime.
    slot = GetSlot(aKey);
  }
  if (slot) {
    // The MutexAutoLock above should act as a memory barrier, forcing
    // the compiler to emit a new read instruction for *slot.
    MOZ_ASSERT(ret == *slot);
  } else {
    MOZ_ASSERT(ret == nullptr);
  }
#endif
  return ret;
}

template <size_t Bits>
bool AddressRadixTree<Bits>::Set(void* aKey, void* aValue) {
  MutexAutoLock lock(mLock);
  void** slot = GetSlot(aKey, /* aCreate = */ true);
  if (slot) {
    *slot = aValue;
  }
  return slot;
}

// pages_trim, chunk_alloc_mmap_slow and chunk_alloc_mmap were cherry-picked
// from upstream jemalloc 3.4.1 to fix Mozilla bug 956501.

// Return the offset between a and the nearest aligned address at or below a.
#define ALIGNMENT_ADDR2OFFSET(a, alignment) \
  ((size_t)((uintptr_t)(a) & ((alignment) - 1)))

// Return the smallest alignment multiple that is >= s.
#define ALIGNMENT_CEILING(s, alignment) \
  (((s) + ((alignment) - 1)) & (~((alignment) - 1)))

static void* pages_trim(void* addr, size_t alloc_size, size_t leadsize,
                        size_t size) {
  void* ret = (void*)((uintptr_t)addr + leadsize);

  MOZ_ASSERT(alloc_size >= leadsize + size);
#ifdef XP_WIN
  {
    void* new_addr;

    pages_unmap(addr, alloc_size);
    new_addr = pages_map(ret, size);
    if (new_addr == ret) {
      return ret;
    }
    if (new_addr) {
      pages_unmap(new_addr, size);
    }
    return nullptr;
  }
#else
  {
    size_t trailsize = alloc_size - leadsize - size;

    if (leadsize != 0) {
      pages_unmap(addr, leadsize);
    }
    if (trailsize != 0) {
      pages_unmap((void*)((uintptr_t)ret + size), trailsize);
    }
    return ret;
  }
#endif
}

static void* chunk_alloc_mmap_slow(size_t size, size_t alignment) {
  void *ret, *pages;
  size_t alloc_size, leadsize;

  alloc_size = size + alignment - gRealPageSize;
  // Beware size_t wrap-around.
  if (alloc_size < size) {
    return nullptr;
  }
  do {
    pages = pages_map(nullptr, alloc_size);
    if (!pages) {
      return nullptr;
    }
    leadsize =
        ALIGNMENT_CEILING((uintptr_t)pages, alignment) - (uintptr_t)pages;
    ret = pages_trim(pages, alloc_size, leadsize, size);
  } while (!ret);

  MOZ_ASSERT(ret);
  return ret;
}

static void* chunk_alloc_mmap(size_t size, size_t alignment) {
  void* ret;
  size_t offset;

  // Ideally, there would be a way to specify alignment to mmap() (like
  // NetBSD has), but in the absence of such a feature, we have to work
  // hard to efficiently create aligned mappings. The reliable, but
  // slow method is to create a mapping that is over-sized, then trim the
  // excess. However, that always results in one or two calls to
  // pages_unmap().
  //
  // Optimistically try mapping precisely the right amount before falling
  // back to the slow method, with the expectation that the optimistic
  // approach works most of the time.
  ret = pages_map(nullptr, size);
  if (!ret) {
    return nullptr;
  }
  offset = ALIGNMENT_ADDR2OFFSET(ret, alignment);
  if (offset != 0) {
    pages_unmap(ret, size);
    return chunk_alloc_mmap_slow(size, alignment);
  }

  MOZ_ASSERT(ret);
  return ret;
}

// Purge and release the pages in the chunk of length `length` at `addr` to
// the OS.
// Returns whether the pages are guaranteed to be full of zeroes when the
// function returns.
// The force_zero argument explicitly requests that the memory is guaranteed
// to be full of zeroes when the function returns.
static bool pages_purge(void* addr, size_t length, bool force_zero) {
  pages_decommit(addr, length);
  return true;
}

static void* chunk_recycle(size_t aSize, size_t aAlignment) {
  extent_node_t key;

  size_t alloc_size = aSize + aAlignment - kChunkSize;
  // Beware size_t wrap-around.
  if (alloc_size < aSize) {
    return nullptr;
  }
  key.mAddr = nullptr;
  key.mSize = alloc_size;
  chunks_mtx.Lock();
  extent_node_t* node = gChunksBySize.SearchOrNext(&key);
  if (!node) {
    chunks_mtx.Unlock();
    return nullptr;
  }
  size_t leadsize = ALIGNMENT_CEILING((uintptr_t)node->mAddr, aAlignment) -
                    (uintptr_t)node->mAddr;
  MOZ_ASSERT(node->mSize >= leadsize + aSize);
  size_t trailsize = node->mSize - leadsize - aSize;
  void* ret = (void*)((uintptr_t)node->mAddr + leadsize);

  // All recycled chunks are zeroed (because they're purged) before being
  // recycled.
  MOZ_ASSERT(node->mChunkType == ZEROED_CHUNK);

  // Remove node from the tree.
  gChunksBySize.Remove(node);
  gChunksByAddress.Remove(node);
  if (leadsize != 0) {
    // Insert the leading space as a smaller chunk.
    node->mSize = leadsize;
    gChunksBySize.Insert(node);
    gChunksByAddress.Insert(node);
    node = nullptr;
  }
  if (trailsize != 0) {
    // Insert the trailing space as a smaller chunk.
    if (!node) {
      // An additional node is required, but
      // TypedBaseAlloc::alloc() can cause a new base chunk to be
      // allocated.  Drop chunks_mtx in order to avoid
      // deadlock, and if node allocation fails, deallocate
      // the result before returning an error.
      chunks_mtx.Unlock();
      node = ExtentAlloc::alloc();
      if (!node) {
        chunk_dealloc(ret, aSize, ZEROED_CHUNK);
        return nullptr;
      }
      chunks_mtx.Lock();
    }
    node->mAddr = (void*)((uintptr_t)(ret) + aSize);
    node->mSize = trailsize;
    node->mChunkType = ZEROED_CHUNK;
    gChunksBySize.Insert(node);
    gChunksByAddress.Insert(node);
    node = nullptr;
  }

  gRecycledSize -= aSize;

  chunks_mtx.Unlock();

  if (node) {
    ExtentAlloc::dealloc(node);
  }
  if (!pages_commit(ret, aSize)) {
    return nullptr;
  }

  return ret;
}

#ifdef XP_WIN
// On Windows, calls to VirtualAlloc and VirtualFree must be matched, making it
// awkward to recycle allocations of varying sizes. Therefore we only allow
// recycling when the size equals the chunksize, unless deallocation is entirely
// disabled.
#  define CAN_RECYCLE(size) ((size) == kChunkSize)
#else
#  define CAN_RECYCLE(size) true
#endif

// Allocates `size` bytes of system memory aligned for `alignment`.
// `base` indicates whether the memory will be used for the base allocator
// (e.g. base_alloc).
// `zeroed` is an outvalue that returns whether the allocated memory is
// guaranteed to be full of zeroes. It can be omitted when the caller doesn't
// care about the result.
static void* chunk_alloc(size_t aSize, size_t aAlignment, bool aBase) {
  void* ret = nullptr;

  MOZ_ASSERT(aSize != 0);
  MOZ_ASSERT((aSize & kChunkSizeMask) == 0);
  MOZ_ASSERT(aAlignment != 0);
  MOZ_ASSERT((aAlignment & kChunkSizeMask) == 0);

  // Base allocations can't be fulfilled by recycling because of
  // possible deadlock or infinite recursion.
  if (CAN_RECYCLE(aSize) && !aBase) {
    ret = chunk_recycle(aSize, aAlignment);
  }
  if (!ret) {
    ret = chunk_alloc_mmap(aSize, aAlignment);
  }
  if (ret && !aBase) {
    if (!gChunkRTree.Set(ret, ret)) {
      chunk_dealloc(ret, aSize, UNKNOWN_CHUNK);
      return nullptr;
    }
  }

  MOZ_ASSERT(GetChunkOffsetForPtr(ret) == 0);
  return ret;
}

#ifdef MOZ_DEBUG
static void chunk_assert_zero(void* aPtr, size_t aSize) {
  size_t i;
  size_t* p = (size_t*)(uintptr_t)aPtr;

  for (i = 0; i < aSize / sizeof(size_t); i++) {
    MOZ_ASSERT(p[i] == 0);
  }
}
#endif

static void chunk_record(void* aChunk, size_t aSize, ChunkType aType) {
  extent_node_t key;

  if (aType != ZEROED_CHUNK) {
    if (pages_purge(aChunk, aSize, aType == HUGE_CHUNK)) {
      aType = ZEROED_CHUNK;
    }
  }

  // Allocate a node before acquiring chunks_mtx even though it might not
  // be needed, because TypedBaseAlloc::alloc() may cause a new base chunk to
  // be allocated, which could cause deadlock if chunks_mtx were already
  // held.
  UniqueBaseNode xnode(ExtentAlloc::alloc());
  // Use xprev to implement conditional deferred deallocation of prev.
  UniqueBaseNode xprev;

  // RAII deallocates xnode and xprev defined above after unlocking
  // in order to avoid potential dead-locks
  MutexAutoLock lock(chunks_mtx);
  key.mAddr = (void*)((uintptr_t)aChunk + aSize);
  extent_node_t* node = gChunksByAddress.SearchOrNext(&key);
  // Try to coalesce forward.
  if (node && node->mAddr == key.mAddr) {
    // Coalesce chunk with the following address range.  This does
    // not change the position within gChunksByAddress, so only
    // remove/insert from/into gChunksBySize.
    gChunksBySize.Remove(node);
    node->mAddr = aChunk;
    node->mSize += aSize;
    if (node->mChunkType != aType) {
      node->mChunkType = RECYCLED_CHUNK;
    }
    gChunksBySize.Insert(node);
  } else {
    // Coalescing forward failed, so insert a new node.
    if (!xnode) {
      // TypedBaseAlloc::alloc() failed, which is an exceedingly
      // unlikely failure.  Leak chunk; its pages have
      // already been purged, so this is only a virtual
      // memory leak.
      return;
    }
    node = xnode.release();
    node->mAddr = aChunk;
    node->mSize = aSize;
    node->mChunkType = aType;
    gChunksByAddress.Insert(node);
    gChunksBySize.Insert(node);
  }

  // Try to coalesce backward.
  extent_node_t* prev = gChunksByAddress.Prev(node);
  if (prev && (void*)((uintptr_t)prev->mAddr + prev->mSize) == aChunk) {
    // Coalesce chunk with the previous address range.  This does
    // not change the position within gChunksByAddress, so only
    // remove/insert node from/into gChunksBySize.
    gChunksBySize.Remove(prev);
    gChunksByAddress.Remove(prev);

    gChunksBySize.Remove(node);
    node->mAddr = prev->mAddr;
    node->mSize += prev->mSize;
    if (node->mChunkType != prev->mChunkType) {
      node->mChunkType = RECYCLED_CHUNK;
    }
    gChunksBySize.Insert(node);

    xprev.reset(prev);
  }

  gRecycledSize += aSize;
}

static void chunk_dealloc(void* aChunk, size_t aSize, ChunkType aType) {
  MOZ_ASSERT(aChunk);
  MOZ_ASSERT(GetChunkOffsetForPtr(aChunk) == 0);
  MOZ_ASSERT(aSize != 0);
  MOZ_ASSERT((aSize & kChunkSizeMask) == 0);

  gChunkRTree.Unset(aChunk);

  if (CAN_RECYCLE(aSize)) {
    size_t recycled_so_far = gRecycledSize;
    // In case some race condition put us above the limit.
    if (recycled_so_far < gRecycleLimit) {
      size_t recycle_remaining = gRecycleLimit - recycled_so_far;
      size_t to_recycle;
      if (aSize > recycle_remaining) {
        to_recycle = recycle_remaining;
        // Drop pages that would overflow the recycle limit
        pages_trim(aChunk, aSize, 0, to_recycle);
      } else {
        to_recycle = aSize;
      }
      chunk_record(aChunk, to_recycle, aType);
      return;
    }
  }

  pages_unmap(aChunk, aSize);
}

#undef CAN_RECYCLE

// End chunk management functions.
// ***************************************************************************
// Begin arena.

static inline arena_t* thread_local_arena(bool enabled) {
  arena_t* arena;

  if (enabled) {
    // The arena will essentially be leaked if this function is
    // called with `false`, but it doesn't matter at the moment.
    // because in practice nothing actually calls this function
    // with `false`, except maybe at shutdown.
    arena =
        gArenas.CreateArena(/* aIsPrivate = */ false, /* aParams = */ nullptr);
  } else {
    arena = gArenas.GetDefault();
  }
  thread_arena.set(arena);
  return arena;
}

inline void MozJemalloc::jemalloc_thread_local_arena(bool aEnabled) {
  if (malloc_init()) {
    thread_local_arena(aEnabled);
  }
}

// Choose an arena based on a per-thread value.
static inline arena_t* choose_arena(size_t size) {
  arena_t* ret = nullptr;

  // We can only use TLS if this is a PIC library, since for the static
  // library version, libc's malloc is used by TLS allocation, which
  // introduces a bootstrapping issue.

  if (size > kMaxQuantumClass) {
    // Force the default arena for larger allocations.
    ret = gArenas.GetDefault();
  } else {
    // Check TLS to see if our thread has requested a pinned arena.
    ret = thread_arena.get();
    // If ret is non-null, it must not be in the first page.
    MOZ_DIAGNOSTIC_ASSERT_IF(ret, (size_t)ret >= gPageSize);
    if (!ret) {
      // Nothing in TLS. Pin this thread to the default arena.
      ret = thread_local_arena(false);
    }
  }

  MOZ_DIAGNOSTIC_ASSERT(ret);
  return ret;
}

inline uint8_t arena_t::FindFreeBitInMask(uint32_t aMask, uint32_t& aRng) {
  if (mPRNG != nullptr) {
    if (aRng == UINT_MAX) {
      aRng = mPRNG->next() % 32;
    }
    uint8_t bitIndex;
    // RotateRight asserts when provided bad input.
    aMask = aRng ? RotateRight(aMask, aRng)
                 : aMask;  // Rotate the mask a random number of slots
    bitIndex = CountTrailingZeroes32(aMask);
    return (bitIndex + aRng) % 32;
  }
  return CountTrailingZeroes32(aMask);
}

inline void* arena_t::ArenaRunRegAlloc(arena_run_t* aRun, arena_bin_t* aBin) {
  void* ret;
  unsigned i, mask, bit, regind;
  uint32_t rndPos = UINT_MAX;

  MOZ_DIAGNOSTIC_ASSERT(aRun->mMagic == ARENA_RUN_MAGIC);
  MOZ_ASSERT(aRun->mRegionsMinElement < aBin->mRunNumRegionsMask);

  // Move the first check outside the loop, so that aRun->mRegionsMinElement can
  // be updated unconditionally, without the possibility of updating it
  // multiple times.
  i = aRun->mRegionsMinElement;
  mask = aRun->mRegionsMask[i];
  if (mask != 0) {
    bit = FindFreeBitInMask(mask, rndPos);

    regind = ((i << (LOG2(sizeof(int)) + 3)) + bit);
    MOZ_ASSERT(regind < aBin->mRunNumRegions);
    ret = (void*)(((uintptr_t)aRun) + aBin->mRunFirstRegionOffset +
                  (aBin->mSizeClass * regind));

    // Clear bit.
    mask ^= (1U << bit);
    aRun->mRegionsMask[i] = mask;

    return ret;
  }

  for (i++; i < aBin->mRunNumRegionsMask; i++) {
    mask = aRun->mRegionsMask[i];
    if (mask != 0) {
      bit = FindFreeBitInMask(mask, rndPos);

      regind = ((i << (LOG2(sizeof(int)) + 3)) + bit);
      MOZ_ASSERT(regind < aBin->mRunNumRegions);
      ret = (void*)(((uintptr_t)aRun) + aBin->mRunFirstRegionOffset +
                    (aBin->mSizeClass * regind));

      // Clear bit.
      mask ^= (1U << bit);
      aRun->mRegionsMask[i] = mask;

      // Make a note that nothing before this element
      // contains a free region.
      aRun->mRegionsMinElement = i;  // Low payoff: + (mask == 0);

      return ret;
    }
  }
  // Not reached.
  MOZ_DIAGNOSTIC_ASSERT(0);
  return nullptr;
}

static inline void arena_run_reg_dalloc(arena_run_t* run, arena_bin_t* bin,
                                        void* ptr, size_t size) {
  uint32_t diff, regind;
  unsigned elm, bit;

  MOZ_DIAGNOSTIC_ASSERT(run->mMagic == ARENA_RUN_MAGIC);

  // Avoid doing division with a variable divisor if possible.  Using
  // actual division here can reduce allocator throughput by over 20%!
  diff =
      (uint32_t)((uintptr_t)ptr - (uintptr_t)run - bin->mRunFirstRegionOffset);

  MOZ_ASSERT(diff <=
             (static_cast<unsigned>(bin->mRunSizePages) << gPageSize2Pow));
  regind = diff / bin->mSizeDivisor;

  MOZ_DIAGNOSTIC_ASSERT(diff == regind * size);
  MOZ_DIAGNOSTIC_ASSERT(regind < bin->mRunNumRegions);

  elm = regind >> (LOG2(sizeof(int)) + 3);
  if (elm < run->mRegionsMinElement) {
    run->mRegionsMinElement = elm;
  }
  bit = regind - (elm << (LOG2(sizeof(int)) + 3));
  MOZ_RELEASE_ASSERT((run->mRegionsMask[elm] & (1U << bit)) == 0,
                     "Double-free?");
  run->mRegionsMask[elm] |= (1U << bit);
}

bool arena_t::SplitRun(arena_run_t* aRun, size_t aSize, bool aLarge,
                       bool aZero) {
  arena_chunk_t* chunk = GetChunkForPtr(aRun);
  size_t old_ndirty = chunk->ndirty;
  size_t run_ind =
      (unsigned)((uintptr_t(aRun) - uintptr_t(chunk)) >> gPageSize2Pow);
  size_t total_pages =
      (chunk->map[run_ind].bits & ~gPageSizeMask) >> gPageSize2Pow;
  size_t need_pages = (aSize >> gPageSize2Pow);
  MOZ_ASSERT(need_pages > 0);
  MOZ_ASSERT(need_pages <= total_pages);
  size_t rem_pages = total_pages - need_pages;

#ifdef MALLOC_DECOMMIT
  size_t i = 0;
  while (i < need_pages) {
    // Commit decommitted pages if necessary.  If a decommitted
    // page is encountered, commit all needed adjacent decommitted
    // pages in one operation, in order to reduce system call
    // overhead.
    if (chunk->map[run_ind + i].bits & CHUNK_MAP_DECOMMITTED) {
      // Advance i+j to just past the index of the last page
      // to commit.  Clear CHUNK_MAP_DECOMMITTED along the way.
      size_t j;
      for (j = 0; i + j < need_pages &&
                  (chunk->map[run_ind + i + j].bits & CHUNK_MAP_DECOMMITTED);
           j++) {
        // DECOMMITTED, MADVISED and FRESH are mutually exclusive.
        MOZ_ASSERT((chunk->map[run_ind + i + j].bits &
                    (CHUNK_MAP_FRESH | CHUNK_MAP_MADVISED)) == 0);
      }

      // Consider committing more pages to amortise calls to VirtualAlloc.
      // This only makes sense at the edge of our run hence the if condition
      // here.
      if (i + j == need_pages) {
        size_t extra_commit = ExtraCommitPages(j, rem_pages);
        for (; i + j < need_pages + extra_commit &&
               (chunk->map[run_ind + i + j].bits &
                CHUNK_MAP_MADVISED_OR_DECOMMITTED);
             j++) {
          MOZ_ASSERT((chunk->map[run_ind + i + j].bits &
                      (CHUNK_MAP_FRESH | CHUNK_MAP_MADVISED)) == 0);
        }
      }

      if (!pages_commit(
              (void*)(uintptr_t(chunk) + ((run_ind + i) << gPageSize2Pow)),
              j << gPageSize2Pow)) {
        return false;
      }

      // pages_commit zeroes pages, so mark them as such if it succeeded.
      // That's checked further below to avoid manually zeroing the pages.
      for (size_t k = 0; k < j; k++) {
        chunk->map[run_ind + i + k].bits =
            (chunk->map[run_ind + i + k].bits & ~CHUNK_MAP_DECOMMITTED) |
            CHUNK_MAP_ZEROED | CHUNK_MAP_FRESH;
      }

      mNumFresh += j;
      i += j;
    } else {
      i++;
    }
  }
#endif

  mRunsAvail.Remove(&chunk->map[run_ind]);

  // Keep track of trailing unused pages for later use.
  if (rem_pages > 0) {
    chunk->map[run_ind + need_pages].bits =
        (rem_pages << gPageSize2Pow) |
        (chunk->map[run_ind + need_pages].bits & gPageSizeMask);
    chunk->map[run_ind + total_pages - 1].bits =
        (rem_pages << gPageSize2Pow) |
        (chunk->map[run_ind + total_pages - 1].bits & gPageSizeMask);
    mRunsAvail.Insert(&chunk->map[run_ind + need_pages]);
  }

  for (size_t i = 0; i < need_pages; i++) {
    // Zero if necessary.
    if (aZero) {
      if ((chunk->map[run_ind + i].bits & CHUNK_MAP_ZEROED) == 0) {
        memset((void*)(uintptr_t(chunk) + ((run_ind + i) << gPageSize2Pow)), 0,
               gPageSize);
        // CHUNK_MAP_ZEROED is cleared below.
      }
    }

    // Update dirty page accounting.
    if (chunk->map[run_ind + i].bits & CHUNK_MAP_DIRTY) {
      chunk->ndirty--;
      mNumDirty--;
      // CHUNK_MAP_DIRTY is cleared below.
    } else if (chunk->map[run_ind + i].bits & CHUNK_MAP_MADVISED) {
      mStats.committed++;
      mNumMAdvised--;
    }

    if (chunk->map[run_ind + i].bits & CHUNK_MAP_FRESH) {
      mStats.committed++;
      mNumFresh--;
    }

    // This bit has already been cleared
    MOZ_ASSERT(!(chunk->map[run_ind + i].bits & CHUNK_MAP_DECOMMITTED));

    // Initialize the chunk map.  This clears the dirty, zeroed and madvised
    // bits, decommitted is cleared above.
    if (aLarge) {
      chunk->map[run_ind + i].bits = CHUNK_MAP_LARGE | CHUNK_MAP_ALLOCATED;
    } else {
      chunk->map[run_ind + i].bits = size_t(aRun) | CHUNK_MAP_ALLOCATED;
    }
  }

  // Set the run size only in the first element for large runs.  This is
  // primarily a debugging aid, since the lack of size info for trailing
  // pages only matters if the application tries to operate on an
  // interior pointer.
  if (aLarge) {
    chunk->map[run_ind].bits |= aSize;
  }

  if (chunk->ndirty == 0 && old_ndirty > 0) {
    mChunksDirty.Remove(chunk);
  }
  return true;
}

void arena_t::InitChunk(arena_chunk_t* aChunk, size_t aMinCommittedPages) {
  mStats.mapped += kChunkSize;

  aChunk->arena = this;

  // Claim that no pages are in use, since the header is merely overhead.
  aChunk->ndirty = 0;

  // Setup the chunk's pages in two phases.  First we mark which pages are
  // committed & decommitted and perform the decommit.  Then we update the map
  // to create the runs.

  // Clear the bits for the real header pages.
  size_t i;
  for (i = 0; i < gChunkHeaderNumPages - 1; i++) {
    aChunk->map[i].bits = 0;
  }
  mStats.committed += gChunkHeaderNumPages - 1;

  // Decommit the last header page (=leading page) as a guard.
  pages_decommit((void*)(uintptr_t(aChunk) + (i << gPageSize2Pow)), gPageSize);
  aChunk->map[i++].bits = CHUNK_MAP_DECOMMITTED;

  // If MALLOC_DECOMMIT is enabled then commit only the pages we're about to
  // use.  Otherwise commit all of them.
#ifdef MALLOC_DECOMMIT
  size_t n_fresh_pages =
      aMinCommittedPages +
      ExtraCommitPages(
          aMinCommittedPages,
          gChunkNumPages - gChunkHeaderNumPages - aMinCommittedPages - 1);
#else
  size_t n_fresh_pages = gChunkNumPages - 1 - gChunkHeaderNumPages;
#endif

  // The committed pages are marked as Fresh.  Our caller, SplitRun will update
  // this when it uses them.
  for (size_t j = 0; j < n_fresh_pages; j++) {
    aChunk->map[i + j].bits = CHUNK_MAP_ZEROED | CHUNK_MAP_FRESH;
  }
  i += n_fresh_pages;
  mNumFresh += n_fresh_pages;

#ifndef MALLOC_DECOMMIT
  // If MALLOC_DECOMMIT isn't defined then all the pages are fresh and setup in
  // the loop above.
  MOZ_ASSERT(i == gChunkNumPages - 1);
#endif

  // If MALLOC_DECOMMIT is defined, then this will decommit the remainder of the
  // chunk plus the last page which is a guard page, if it is not defined it
  // will only decommit the guard page.
  pages_decommit((void*)(uintptr_t(aChunk) + (i << gPageSize2Pow)),
                 (gChunkNumPages - i) << gPageSize2Pow);
  for (; i < gChunkNumPages; i++) {
    aChunk->map[i].bits = CHUNK_MAP_DECOMMITTED;
  }

  // aMinCommittedPages will create a valid run.
  MOZ_ASSERT(aMinCommittedPages > 0);
  MOZ_ASSERT(aMinCommittedPages <= gChunkNumPages - gChunkHeaderNumPages - 1);

  // Create the run.
  aChunk->map[gChunkHeaderNumPages].bits |= gMaxLargeClass;
  aChunk->map[gChunkNumPages - 2].bits |= gMaxLargeClass;
  mRunsAvail.Insert(&aChunk->map[gChunkHeaderNumPages]);

#ifdef MALLOC_DOUBLE_PURGE
  new (&aChunk->chunks_madvised_elem) DoublyLinkedListElement<arena_chunk_t>();
#endif
}

arena_chunk_t* arena_t::DeallocChunk(arena_chunk_t* aChunk) {
  if (mSpare) {
    if (mSpare->ndirty > 0) {
      aChunk->arena->mChunksDirty.Remove(mSpare);
      mNumDirty -= mSpare->ndirty;
      mStats.committed -= mSpare->ndirty;
    }

    // Count the number of madvised/fresh pages and update the stats.
    size_t madvised = 0;
    size_t fresh = 0;
    for (size_t i = gChunkHeaderNumPages; i < gChunkNumPages - 1; i++) {
      // There must not be any pages that are not fresh, madvised, decommitted
      // or dirty.
      MOZ_ASSERT(mSpare->map[i].bits &
                 (CHUNK_MAP_FRESH_MADVISED_OR_DECOMMITTED | CHUNK_MAP_DIRTY));

      if (mSpare->map[i].bits & CHUNK_MAP_MADVISED) {
        madvised++;
      } else if (mSpare->map[i].bits & CHUNK_MAP_FRESH) {
        fresh++;
      }
    }

    mNumMAdvised -= madvised;
    mNumFresh -= fresh;

#ifdef MALLOC_DOUBLE_PURGE
    if (mChunksMAdvised.ElementProbablyInList(mSpare)) {
      mChunksMAdvised.remove(mSpare);
    }
#endif

    mStats.mapped -= kChunkSize;
    mStats.committed -= gChunkHeaderNumPages - 1;
  }

  // Remove run from the tree of available runs, so that the arena does not use
  // it. Dirty page flushing only uses the tree of dirty chunks, so leaving this
  // chunk in the chunks_* trees is sufficient for that purpose.
  mRunsAvail.Remove(&aChunk->map[gChunkHeaderNumPages]);

  arena_chunk_t* chunk_dealloc = mSpare;
  mSpare = aChunk;
  return chunk_dealloc;
}

arena_run_t* arena_t::AllocRun(size_t aSize, bool aLarge, bool aZero) {
  arena_run_t* run;
  arena_chunk_map_t* mapelm;
  arena_chunk_map_t key;

  MOZ_ASSERT(aSize <= gMaxLargeClass);
  MOZ_ASSERT((aSize & gPageSizeMask) == 0);

  // Search the arena's chunks for the lowest best fit.
  key.bits = aSize | CHUNK_MAP_KEY;
  mapelm = mRunsAvail.SearchOrNext(&key);
  if (mapelm) {
    arena_chunk_t* chunk = GetChunkForPtr(mapelm);
    size_t pageind =
        (uintptr_t(mapelm) - uintptr_t(chunk->map)) / sizeof(arena_chunk_map_t);

    run = (arena_run_t*)(uintptr_t(chunk) + (pageind << gPageSize2Pow));
  } else if (mSpare) {
    // Use the spare.
    arena_chunk_t* chunk = mSpare;
    mSpare = nullptr;
    run = (arena_run_t*)(uintptr_t(chunk) +
                         (gChunkHeaderNumPages << gPageSize2Pow));
    // Insert the run into the tree of available runs.
    mRunsAvail.Insert(&chunk->map[gChunkHeaderNumPages]);
  } else {
    // No usable runs.  Create a new chunk from which to allocate
    // the run.
    arena_chunk_t* chunk =
        (arena_chunk_t*)chunk_alloc(kChunkSize, kChunkSize, false);
    if (!chunk) {
      return nullptr;
    }

    InitChunk(chunk, aSize >> gPageSize2Pow);
    run = (arena_run_t*)(uintptr_t(chunk) +
                         (gChunkHeaderNumPages << gPageSize2Pow));
  }
  // Update page map.
  return SplitRun(run, aSize, aLarge, aZero) ? run : nullptr;
}

size_t arena_t::EffectiveMaxDirty() {
  int32_t modifier = gArenas.DefaultMaxDirtyPageModifier();
  if (modifier) {
    int32_t arenaOverride =
        modifier > 0 ? mMaxDirtyIncreaseOverride : mMaxDirtyDecreaseOverride;
    if (arenaOverride) {
      modifier = arenaOverride;
    }
  }

  return modifier >= 0 ? mMaxDirty << modifier : mMaxDirty >> -modifier;
}

#ifdef MALLOC_DECOMMIT

size_t arena_t::ExtraCommitPages(size_t aReqPages, size_t aRemainingPages) {
  const int32_t modifier = gArenas.DefaultMaxDirtyPageModifier();
  if (modifier < 0) {
    return 0;
  }

  // The maximum size of the page cache
  const size_t max_page_cache = EffectiveMaxDirty();

  // The current size of the page cache, note that we use mNumFresh +
  // mNumMAdvised here but Purge() does not.
  const size_t page_cache = mNumDirty + mNumFresh + mNumMAdvised;

  if (page_cache > max_page_cache) {
    // We're already exceeding our dirty page count even though we're trying
    // to allocate.  This can happen due to fragmentation.  Don't commit
    // excess memory since we're probably here due to a larger allocation and
    // small amounts of memory are certainly available in the page cache.
    return 0;
  }
  if (modifier > 0) {
    // If modifier is > 0 then we want to keep all the pages we can, but don't
    // exceed the size of the page cache.  The subtraction cannot underflow
    // because of the condition above.
    return std::min(aRemainingPages, max_page_cache - page_cache);
  }

  // The rest is arbitrary and involves a some assumptions.  I've broken it down
  // into simple expressions to document them more clearly.

  // Assumption 1: a quarter of EffectiveMaxDirty() is a sensible "minimum
  // target" for the dirty page cache.  Likewise 3 quarters is a sensible
  // "maximum target".  Note that for the maximum we avoid using the whole page
  // cache now so that a free that follows this allocation doesn't immeidatly
  // call Purge (churning memory).
  const size_t min = max_page_cache / 4;
  const size_t max = 3 * max_page_cache / 4;

  // Assumption 2: Committing 32 pages at a time is sufficient to amortise
  // VirtualAlloc costs.
  size_t amortisation_threshold = 32;

  // extra_pages is the number of additional pages needed to meet
  // amortisation_threshold.
  size_t extra_pages = aReqPages < amortisation_threshold
                           ? amortisation_threshold - aReqPages
                           : 0;

  // If committing extra_pages isn't enough to hit the minimum target then
  // increase it.
  if (page_cache + extra_pages < min) {
    extra_pages = min - page_cache;
  } else if (page_cache + extra_pages > max) {
    // If committing extra_pages would exceed our maximum target then it may
    // still be useful to allocate extra pages.  One of the reasons this can
    // happen could be fragmentation of the cache,

    // Therefore reduce the amortisation threshold so that we might allocate
    // some extra pages but avoid exceeding the dirty page cache.
    amortisation_threshold /= 2;
    extra_pages = std::min(aReqPages < amortisation_threshold
                               ? amortisation_threshold - aReqPages
                               : 0,
                           max_page_cache - page_cache);
  }

  // Cap extra_pages to aRemainingPages and adjust aRemainingPages.  We will
  // commit at least this many extra pages.
  extra_pages = std::min(extra_pages, aRemainingPages);

  // Finally if commiting a small number of additional pages now can prevent
  // a small commit later then try to commit a little more now, provided we
  // don't exceed max_page_cache.
  if ((aRemainingPages - extra_pages) < amortisation_threshold / 2 &&
      (page_cache + aRemainingPages) < max_page_cache) {
    return aRemainingPages;
  }

  return extra_pages;
}
#endif

void arena_t::Purge(size_t aMaxDirty) {
  arena_chunk_t* chunk;
  size_t i, npages;

#ifdef MOZ_DEBUG
  size_t ndirty = 0;
  for (auto chunk : mChunksDirty.iter()) {
    ndirty += chunk->ndirty;
  }
  MOZ_ASSERT(ndirty == mNumDirty);
#endif
  MOZ_DIAGNOSTIC_ASSERT(aMaxDirty == 1 || (mNumDirty > aMaxDirty));

  // Iterate downward through chunks until enough dirty memory has been
  // purged.  Terminate as soon as possible in order to minimize the
  // number of system calls, even if a chunk has only been partially
  // purged.
  while (mNumDirty > (aMaxDirty >> 1)) {
#ifdef MALLOC_DOUBLE_PURGE
    bool madvised = false;
#endif
    chunk = mChunksDirty.Last();
    MOZ_DIAGNOSTIC_ASSERT(chunk);
    // Last page is DECOMMITTED as a guard page.
    MOZ_ASSERT((chunk->map[gChunkNumPages - 1].bits & CHUNK_MAP_DECOMMITTED) !=
               0);
    for (i = gChunkNumPages - 2; chunk->ndirty > 0; i--) {
      MOZ_DIAGNOSTIC_ASSERT(i >= gChunkHeaderNumPages);

      if (chunk->map[i].bits & CHUNK_MAP_DIRTY) {
#ifdef MALLOC_DECOMMIT
        const size_t free_operation = CHUNK_MAP_DECOMMITTED;
#else
        const size_t free_operation = CHUNK_MAP_MADVISED;
#endif
        MOZ_ASSERT((chunk->map[i].bits &
                    CHUNK_MAP_FRESH_MADVISED_OR_DECOMMITTED) == 0);
        chunk->map[i].bits ^= free_operation | CHUNK_MAP_DIRTY;
        // Find adjacent dirty run(s).
        for (npages = 1; i > gChunkHeaderNumPages &&
                         (chunk->map[i - 1].bits & CHUNK_MAP_DIRTY);
             npages++) {
          i--;
          MOZ_ASSERT((chunk->map[i].bits &
                      CHUNK_MAP_FRESH_MADVISED_OR_DECOMMITTED) == 0);
          chunk->map[i].bits ^= free_operation | CHUNK_MAP_DIRTY;
        }
        chunk->ndirty -= npages;
        mNumDirty -= npages;

#ifdef MALLOC_DECOMMIT
        pages_decommit((void*)(uintptr_t(chunk) + (i << gPageSize2Pow)),
                       (npages << gPageSize2Pow));
#else
#  ifdef XP_SOLARIS
        posix_madvise((void*)(uintptr_t(chunk) + (i << gPageSize2Pow)),
                      (npages << gPageSize2Pow), MADV_FREE);
#  else
        madvise((void*)(uintptr_t(chunk) + (i << gPageSize2Pow)),
                (npages << gPageSize2Pow), MADV_FREE);
#  endif
        mNumMAdvised += npages;
#  ifdef MALLOC_DOUBLE_PURGE
        madvised = true;
#  endif
#endif
        mStats.committed -= npages;

        if (mNumDirty <= (aMaxDirty >> 1)) {
          break;
        }
      }
    }

    if (chunk->ndirty == 0) {
      mChunksDirty.Remove(chunk);
    }
#ifdef MALLOC_DOUBLE_PURGE
    if (madvised) {
      // The chunk might already be in the list, but this
      // makes sure it's at the front.
      if (mChunksMAdvised.ElementProbablyInList(chunk)) {
        mChunksMAdvised.remove(chunk);
      }
      mChunksMAdvised.pushFront(chunk);
    }
#endif
  }
}

arena_chunk_t* arena_t::DallocRun(arena_run_t* aRun, bool aDirty) {
  arena_chunk_t* chunk;
  size_t size, run_ind, run_pages;

  chunk = GetChunkForPtr(aRun);
  run_ind = (size_t)((uintptr_t(aRun) - uintptr_t(chunk)) >> gPageSize2Pow);
  MOZ_DIAGNOSTIC_ASSERT(run_ind >= gChunkHeaderNumPages);
  MOZ_RELEASE_ASSERT(run_ind < gChunkNumPages - 1);
  if ((chunk->map[run_ind].bits & CHUNK_MAP_LARGE) != 0) {
    size = chunk->map[run_ind].bits & ~gPageSizeMask;
    run_pages = (size >> gPageSize2Pow);
  } else {
    run_pages = aRun->mBin->mRunSizePages;
    size = run_pages << gPageSize2Pow;
  }

  // Mark pages as unallocated in the chunk map.
  if (aDirty) {
    size_t i;

    for (i = 0; i < run_pages; i++) {
      MOZ_DIAGNOSTIC_ASSERT((chunk->map[run_ind + i].bits & CHUNK_MAP_DIRTY) ==
                            0);
      chunk->map[run_ind + i].bits = CHUNK_MAP_DIRTY;
    }

    if (chunk->ndirty == 0) {
      mChunksDirty.Insert(chunk);
    }
    chunk->ndirty += run_pages;
    mNumDirty += run_pages;
  } else {
    size_t i;

    for (i = 0; i < run_pages; i++) {
      chunk->map[run_ind + i].bits &= ~(CHUNK_MAP_LARGE | CHUNK_MAP_ALLOCATED);
    }
  }
  chunk->map[run_ind].bits = size | (chunk->map[run_ind].bits & gPageSizeMask);
  chunk->map[run_ind + run_pages - 1].bits =
      size | (chunk->map[run_ind + run_pages - 1].bits & gPageSizeMask);

  // Try to coalesce forward.
  if (run_ind + run_pages < gChunkNumPages - 1 &&
      (chunk->map[run_ind + run_pages].bits & CHUNK_MAP_ALLOCATED) == 0) {
    size_t nrun_size = chunk->map[run_ind + run_pages].bits & ~gPageSizeMask;

    // Remove successor from tree of available runs; the coalesced run is
    // inserted later.
    mRunsAvail.Remove(&chunk->map[run_ind + run_pages]);

    size += nrun_size;
    run_pages = size >> gPageSize2Pow;

    MOZ_DIAGNOSTIC_ASSERT((chunk->map[run_ind + run_pages - 1].bits &
                           ~gPageSizeMask) == nrun_size);
    chunk->map[run_ind].bits =
        size | (chunk->map[run_ind].bits & gPageSizeMask);
    chunk->map[run_ind + run_pages - 1].bits =
        size | (chunk->map[run_ind + run_pages - 1].bits & gPageSizeMask);
  }

  // Try to coalesce backward.
  if (run_ind > gChunkHeaderNumPages &&
      (chunk->map[run_ind - 1].bits & CHUNK_MAP_ALLOCATED) == 0) {
    size_t prun_size = chunk->map[run_ind - 1].bits & ~gPageSizeMask;

    run_ind -= prun_size >> gPageSize2Pow;

    // Remove predecessor from tree of available runs; the coalesced run is
    // inserted later.
    mRunsAvail.Remove(&chunk->map[run_ind]);

    size += prun_size;
    run_pages = size >> gPageSize2Pow;

    MOZ_DIAGNOSTIC_ASSERT((chunk->map[run_ind].bits & ~gPageSizeMask) ==
                          prun_size);
    chunk->map[run_ind].bits =
        size | (chunk->map[run_ind].bits & gPageSizeMask);
    chunk->map[run_ind + run_pages - 1].bits =
        size | (chunk->map[run_ind + run_pages - 1].bits & gPageSizeMask);
  }

  // Insert into tree of available runs, now that coalescing is complete.
  mRunsAvail.Insert(&chunk->map[run_ind]);

  // Deallocate chunk if it is now completely unused.
  arena_chunk_t* chunk_dealloc = nullptr;
  if ((chunk->map[gChunkHeaderNumPages].bits &
       (~gPageSizeMask | CHUNK_MAP_ALLOCATED)) == gMaxLargeClass) {
    chunk_dealloc = DeallocChunk(chunk);
  }

  size_t maxDirty = EffectiveMaxDirty();
  if (mNumDirty > maxDirty) {
    Purge(maxDirty);
  }

  return chunk_dealloc;
}

void arena_t::TrimRunHead(arena_chunk_t* aChunk, arena_run_t* aRun,
                          size_t aOldSize, size_t aNewSize) {
  size_t pageind = (uintptr_t(aRun) - uintptr_t(aChunk)) >> gPageSize2Pow;
  size_t head_npages = (aOldSize - aNewSize) >> gPageSize2Pow;

  MOZ_ASSERT(aOldSize > aNewSize);

  // Update the chunk map so that arena_t::RunDalloc() can treat the
  // leading run as separately allocated.
  aChunk->map[pageind].bits =
      (aOldSize - aNewSize) | CHUNK_MAP_LARGE | CHUNK_MAP_ALLOCATED;
  aChunk->map[pageind + head_npages].bits =
      aNewSize | CHUNK_MAP_LARGE | CHUNK_MAP_ALLOCATED;

#ifdef MOZ_DEBUG
  arena_chunk_t* no_chunk =
#endif
      DallocRun(aRun, false);
  // This will never release a chunk as there's still at least one allocated
  // run.
  MOZ_ASSERT(!no_chunk);
}

void arena_t::TrimRunTail(arena_chunk_t* aChunk, arena_run_t* aRun,
                          size_t aOldSize, size_t aNewSize, bool aDirty) {
  size_t pageind = (uintptr_t(aRun) - uintptr_t(aChunk)) >> gPageSize2Pow;
  size_t npages = aNewSize >> gPageSize2Pow;

  MOZ_ASSERT(aOldSize > aNewSize);

  // Update the chunk map so that arena_t::RunDalloc() can treat the
  // trailing run as separately allocated.
  aChunk->map[pageind].bits = aNewSize | CHUNK_MAP_LARGE | CHUNK_MAP_ALLOCATED;
  aChunk->map[pageind + npages].bits =
      (aOldSize - aNewSize) | CHUNK_MAP_LARGE | CHUNK_MAP_ALLOCATED;

#ifdef MOZ_DEBUG
  arena_chunk_t* no_chunk =
#endif
      DallocRun((arena_run_t*)(uintptr_t(aRun) + aNewSize), aDirty);

  // This will never release a chunk as there's still at least one allocated
  // run.
  MOZ_ASSERT(!no_chunk);
}

arena_run_t* arena_t::GetNonFullBinRun(arena_bin_t* aBin) {
  arena_chunk_map_t* mapelm;
  arena_run_t* run;
  unsigned i, remainder;

  // Look for a usable run.
  mapelm = aBin->mNonFullRuns.First();
  if (mapelm) {
    // run is guaranteed to have available space.
    aBin->mNonFullRuns.Remove(mapelm);
    run = (arena_run_t*)(mapelm->bits & ~gPageSizeMask);
    return run;
  }
  // No existing runs have any space available.

  // Allocate a new run.
  run = AllocRun(static_cast<size_t>(aBin->mRunSizePages) << gPageSize2Pow,
                 false, false);
  if (!run) {
    return nullptr;
  }
  // Don't initialize if a race in arena_t::RunAlloc() allowed an existing
  // run to become usable.
  if (run == aBin->mCurrentRun) {
    return run;
  }

  // Initialize run internals.
  run->mBin = aBin;

  for (i = 0; i < aBin->mRunNumRegionsMask - 1; i++) {
    run->mRegionsMask[i] = UINT_MAX;
  }
  remainder = aBin->mRunNumRegions & ((1U << (LOG2(sizeof(int)) + 3)) - 1);
  if (remainder == 0) {
    run->mRegionsMask[i] = UINT_MAX;
  } else {
    // The last element has spare bits that need to be unset.
    run->mRegionsMask[i] =
        (UINT_MAX >> ((1U << (LOG2(sizeof(int)) + 3)) - remainder));
  }

  run->mRegionsMinElement = 0;

  run->mNumFree = aBin->mRunNumRegions;
#if defined(MOZ_DIAGNOSTIC_ASSERT_ENABLED)
  run->mMagic = ARENA_RUN_MAGIC;
#endif

  aBin->mNumRuns++;
  return run;
}

void arena_bin_t::Init(SizeClass aSizeClass) {
  size_t try_run_size;
  unsigned try_nregs, try_mask_nelms, try_reg0_offset;
  // Size of the run header, excluding mRegionsMask.
  static const size_t kFixedHeaderSize = offsetof(arena_run_t, mRegionsMask);

  MOZ_ASSERT(aSizeClass.Size() <= gMaxBinClass);

  try_run_size = gPageSize;

  mCurrentRun = nullptr;
  mNonFullRuns.Init();
  mSizeClass = aSizeClass.Size();
  mNumRuns = 0;

  // Run size expansion loop.
  while (true) {
    try_nregs = ((try_run_size - kFixedHeaderSize) / mSizeClass) +
                1;  // Counter-act try_nregs-- in loop.

    // The do..while loop iteratively reduces the number of regions until
    // the run header and the regions no longer overlap.  A closed formula
    // would be quite messy, since there is an interdependency between the
    // header's mask length and the number of regions.
    do {
      try_nregs--;
      try_mask_nelms =
          (try_nregs >> (LOG2(sizeof(int)) + 3)) +
          ((try_nregs & ((1U << (LOG2(sizeof(int)) + 3)) - 1)) ? 1 : 0);
      try_reg0_offset = try_run_size - (try_nregs * mSizeClass);
    } while (kFixedHeaderSize + (sizeof(unsigned) * try_mask_nelms) >
             try_reg0_offset);

    // Try to keep the run overhead below kRunOverhead.
    if (Fraction(try_reg0_offset, try_run_size) <= kRunOverhead) {
      break;
    }

    // If the overhead is larger than the size class, it means the size class
    // is small and doesn't align very well with the header. It's desirable to
    // have smaller run sizes for them, so relax the overhead requirement.
    if (try_reg0_offset > mSizeClass) {
      if (Fraction(try_reg0_offset, try_run_size) <= kRunRelaxedOverhead) {
        break;
      }
    }

    // The run header includes one bit per region of the given size. For sizes
    // small enough, the number of regions is large enough that growing the run
    // size barely moves the needle for the overhead because of all those bits.
    // For example, for a size of 8 bytes, adding 4KiB to the run size adds
    // close to 512 bits to the header, which is 64 bytes.
    // With such overhead, there is no way to get to the wanted overhead above,
    // so we give up if the required size for mRegionsMask more than doubles the
    // size of the run header.
    if (try_mask_nelms * sizeof(unsigned) >= kFixedHeaderSize) {
      break;
    }

    // If next iteration is going to be larger than the largest possible large
    // size class, then we didn't find a setup where the overhead is small
    // enough, and we can't do better than the current settings, so just use
    // that.
    if (try_run_size + gPageSize > gMaxLargeClass) {
      break;
    }

    // Try more aggressive settings.
    try_run_size += gPageSize;
  }

  MOZ_ASSERT(kFixedHeaderSize + (sizeof(unsigned) * try_mask_nelms) <=
             try_reg0_offset);
  MOZ_ASSERT((try_mask_nelms << (LOG2(sizeof(int)) + 3)) >= try_nregs);

  // Copy final settings.
  MOZ_ASSERT((try_run_size >> gPageSize2Pow) <= UINT8_MAX);
  mRunSizePages = static_cast<uint8_t>(try_run_size >> gPageSize2Pow);
  mRunNumRegions = try_nregs;
  mRunNumRegionsMask = try_mask_nelms;
  mRunFirstRegionOffset = try_reg0_offset;
  mSizeDivisor = FastDivisor<uint16_t>(aSizeClass.Size(), try_run_size);
}

void* arena_t::MallocSmall(size_t aSize, bool aZero) {
  void* ret;
  arena_bin_t* bin;
  arena_run_t* run;
  SizeClass sizeClass(aSize);
  aSize = sizeClass.Size();

  switch (sizeClass.Type()) {
    case SizeClass::Tiny:
      bin = &mBins[FloorLog2(aSize / kMinTinyClass)];
      break;
    case SizeClass::Quantum:
      // Although we divide 2 things by kQuantum, the compiler will
      // reduce `kMinQuantumClass / kQuantum` and `kNumTinyClasses` to a
      // single constant.
      bin = &mBins[kNumTinyClasses + (aSize / kQuantum) -
                   (kMinQuantumClass / kQuantum)];
      break;
    case SizeClass::QuantumWide:
      bin =
          &mBins[kNumTinyClasses + kNumQuantumClasses + (aSize / kQuantumWide) -
                 (kMinQuantumWideClass / kQuantumWide)];
      break;
    case SizeClass::SubPage:
      bin =
          &mBins[kNumTinyClasses + kNumQuantumClasses + kNumQuantumWideClasses +
                 (FloorLog2(aSize) - LOG2(kMinSubPageClass))];
      break;
    default:
      MOZ_MAKE_COMPILER_ASSUME_IS_UNREACHABLE("Unexpected size class type");
  }
  MOZ_DIAGNOSTIC_ASSERT(aSize == bin->mSizeClass);

  {
    MaybeMutexAutoLock lock(mLock);

    if (MOZ_UNLIKELY(mRandomizeSmallAllocations && mPRNG == nullptr &&
                     !mIsPRNGInitializing)) {
      // Both another thread could race and the code backing RandomUint64
      // (arc4random for example) may allocate memory while here, so we must
      // ensure to start the mPRNG initialization only once and to not hold
      // the lock while initializing.
      mIsPRNGInitializing = true;
      mozilla::non_crypto::XorShift128PlusRNG* prng;
      {
        // TODO: I think no MaybeMutexAutoUnlock or similar exists, should it?
        mLock.Unlock();
        mozilla::Maybe<uint64_t> prngState1 = mozilla::RandomUint64();
        mozilla::Maybe<uint64_t> prngState2 = mozilla::RandomUint64();
        void* backing =
            base_alloc(sizeof(mozilla::non_crypto::XorShift128PlusRNG));
        prng = new (backing) mozilla::non_crypto::XorShift128PlusRNG(
            prngState1.valueOr(0), prngState2.valueOr(0));
        mLock.Lock();
      }
      mPRNG = prng;
      mIsPRNGInitializing = false;
    }
    MOZ_ASSERT(!mRandomizeSmallAllocations || mPRNG);

    run = bin->mCurrentRun;
    if (MOZ_UNLIKELY(!run || run->mNumFree == 0)) {
      run = bin->mCurrentRun = GetNonFullBinRun(bin);
    }
    if (MOZ_UNLIKELY(!run)) {
      return nullptr;
    }
    MOZ_DIAGNOSTIC_ASSERT(run->mMagic == ARENA_RUN_MAGIC);
    MOZ_DIAGNOSTIC_ASSERT(run->mNumFree > 0);
    ret = ArenaRunRegAlloc(run, bin);
    MOZ_DIAGNOSTIC_ASSERT(ret);
    run->mNumFree--;
    if (!ret) {
      return nullptr;
    }

    mStats.allocated_small += aSize;
  }

  if (!aZero) {
    ApplyZeroOrJunk(ret, aSize);
  } else {
    memset(ret, 0, aSize);
  }

  return ret;
}

void* arena_t::MallocLarge(size_t aSize, bool aZero) {
  void* ret;

  // Large allocation.
  aSize = PAGE_CEILING(aSize);

  {
    MaybeMutexAutoLock lock(mLock);
    ret = AllocRun(aSize, true, aZero);
    if (!ret) {
      return nullptr;
    }
    mStats.allocated_large += aSize;
  }

  if (!aZero) {
    ApplyZeroOrJunk(ret, aSize);
  }

  return ret;
}

void* arena_t::Malloc(size_t aSize, bool aZero) {
  MOZ_DIAGNOSTIC_ASSERT(mMagic == ARENA_MAGIC);
  MOZ_ASSERT(aSize != 0);

  if (aSize <= gMaxBinClass) {
    return MallocSmall(aSize, aZero);
  }
  if (aSize <= gMaxLargeClass) {
    return MallocLarge(aSize, aZero);
  }
  return MallocHuge(aSize, aZero);
}

// Only handles large allocations that require more than page alignment.
void* arena_t::PallocLarge(size_t aAlignment, size_t aSize, size_t aAllocSize) {
  void* ret;
  size_t offset;
  arena_chunk_t* chunk;

  MOZ_ASSERT((aSize & gPageSizeMask) == 0);
  MOZ_ASSERT((aAlignment & gPageSizeMask) == 0);

  {
    MaybeMutexAutoLock lock(mLock);
    ret = AllocRun(aAllocSize, true, false);
    if (!ret) {
      return nullptr;
    }

    chunk = GetChunkForPtr(ret);

    offset = uintptr_t(ret) & (aAlignment - 1);
    MOZ_ASSERT((offset & gPageSizeMask) == 0);
    MOZ_ASSERT(offset < aAllocSize);
    if (offset == 0) {
      TrimRunTail(chunk, (arena_run_t*)ret, aAllocSize, aSize, false);
    } else {
      size_t leadsize, trailsize;

      leadsize = aAlignment - offset;
      if (leadsize > 0) {
        TrimRunHead(chunk, (arena_run_t*)ret, aAllocSize,
                    aAllocSize - leadsize);
        ret = (void*)(uintptr_t(ret) + leadsize);
      }

      trailsize = aAllocSize - leadsize - aSize;
      if (trailsize != 0) {
        // Trim trailing space.
        MOZ_ASSERT(trailsize < aAllocSize);
        TrimRunTail(chunk, (arena_run_t*)ret, aSize + trailsize, aSize, false);
      }
    }

    mStats.allocated_large += aSize;
  }

  ApplyZeroOrJunk(ret, aSize);
  return ret;
}

void* arena_t::Palloc(size_t aAlignment, size_t aSize) {
  void* ret;
  size_t ceil_size;

  // Round size up to the nearest multiple of alignment.
  //
  // This done, we can take advantage of the fact that for each small
  // size class, every object is aligned at the smallest power of two
  // that is non-zero in the base two representation of the size.  For
  // example:
  //
  //   Size |   Base 2 | Minimum alignment
  //   -----+----------+------------------
  //     96 |  1100000 |  32
  //    144 | 10100000 |  32
  //    192 | 11000000 |  64
  //
  // Depending on runtime settings, it is possible that arena_malloc()
  // will further round up to a power of two, but that never causes
  // correctness issues.
  ceil_size = ALIGNMENT_CEILING(aSize, aAlignment);

  // (ceil_size < aSize) protects against the combination of maximal
  // alignment and size greater than maximal alignment.
  if (ceil_size < aSize) {
    // size_t overflow.
    return nullptr;
  }

  if (ceil_size <= gPageSize ||
      (aAlignment <= gPageSize && ceil_size <= gMaxLargeClass)) {
    ret = Malloc(ceil_size, false);
  } else {
    size_t run_size;

    // We can't achieve sub-page alignment, so round up alignment
    // permanently; it makes later calculations simpler.
    aAlignment = PAGE_CEILING(aAlignment);
    ceil_size = PAGE_CEILING(aSize);

    // (ceil_size < aSize) protects against very large sizes within
    // pagesize of SIZE_T_MAX.
    //
    // (ceil_size + aAlignment < ceil_size) protects against the
    // combination of maximal alignment and ceil_size large enough
    // to cause overflow.  This is similar to the first overflow
    // check above, but it needs to be repeated due to the new
    // ceil_size value, which may now be *equal* to maximal
    // alignment, whereas before we only detected overflow if the
    // original size was *greater* than maximal alignment.
    if (ceil_size < aSize || ceil_size + aAlignment < ceil_size) {
      // size_t overflow.
      return nullptr;
    }

    // Calculate the size of the over-size run that arena_palloc()
    // would need to allocate in order to guarantee the alignment.
    if (ceil_size >= aAlignment) {
      run_size = ceil_size + aAlignment - gPageSize;
    } else {
      // It is possible that (aAlignment << 1) will cause
      // overflow, but it doesn't matter because we also
      // subtract pagesize, which in the case of overflow
      // leaves us with a very large run_size.  That causes
      // the first conditional below to fail, which means
      // that the bogus run_size value never gets used for
      // anything important.
      run_size = (aAlignment << 1) - gPageSize;
    }

    if (run_size <= gMaxLargeClass) {
      ret = PallocLarge(aAlignment, ceil_size, run_size);
    } else if (aAlignment <= kChunkSize) {
      ret = MallocHuge(ceil_size, false);
    } else {
      ret = PallocHuge(ceil_size, aAlignment, false);
    }
  }

  MOZ_ASSERT((uintptr_t(ret) & (aAlignment - 1)) == 0);
  return ret;
}

class AllocInfo {
 public:
  template <bool Validate = false>
  static inline AllocInfo Get(const void* aPtr) {
    // If the allocator is not initialized, the pointer can't belong to it.
    if (Validate && !malloc_initialized) {
      return AllocInfo();
    }

    auto chunk = GetChunkForPtr(aPtr);
    if (Validate) {
      if (!chunk || !gChunkRTree.Get(chunk)) {
        return AllocInfo();
      }
    }

    if (chunk != aPtr) {
      MOZ_DIAGNOSTIC_ASSERT(chunk->arena->mMagic == ARENA_MAGIC);
      size_t pageind = (((uintptr_t)aPtr - (uintptr_t)chunk) >> gPageSize2Pow);
      return GetInChunk(aPtr, chunk, pageind);
    }

    extent_node_t key;

    // Huge allocation
    key.mAddr = chunk;
    MutexAutoLock lock(huge_mtx);
    extent_node_t* node = huge.Search(&key);
    if (Validate && !node) {
      return AllocInfo();
    }
    return AllocInfo(node->mSize, node);
  }

  // Get the allocation information for a pointer we know is within a chunk
  // (Small or large, not huge).
  static inline AllocInfo GetInChunk(const void* aPtr, arena_chunk_t* aChunk,
                                     size_t pageind) {
    size_t mapbits = aChunk->map[pageind].bits;
    MOZ_DIAGNOSTIC_ASSERT((mapbits & CHUNK_MAP_ALLOCATED) != 0);

    size_t size;
    if ((mapbits & CHUNK_MAP_LARGE) == 0) {
      arena_run_t* run = (arena_run_t*)(mapbits & ~gPageSizeMask);
      MOZ_DIAGNOSTIC_ASSERT(run->mMagic == ARENA_RUN_MAGIC);
      size = run->mBin->mSizeClass;
    } else {
      size = mapbits & ~gPageSizeMask;
      MOZ_DIAGNOSTIC_ASSERT(size != 0);
    }

    return AllocInfo(size, aChunk);
  }

  // Validate ptr before assuming that it points to an allocation.  Currently,
  // the following validation is performed:
  //
  // + Check that ptr is not nullptr.
  //
  // + Check that ptr lies within a mapped chunk.
  static inline AllocInfo GetValidated(const void* aPtr) {
    return Get<true>(aPtr);
  }

  AllocInfo() : mSize(0), mChunk(nullptr) {}

  explicit AllocInfo(size_t aSize, arena_chunk_t* aChunk)
      : mSize(aSize), mChunk(aChunk) {
    MOZ_ASSERT(mSize <= gMaxLargeClass);
  }

  explicit AllocInfo(size_t aSize, extent_node_t* aNode)
      : mSize(aSize), mNode(aNode) {
    MOZ_ASSERT(mSize > gMaxLargeClass);
  }

  size_t Size() { return mSize; }

  arena_t* Arena() {
    if (mSize <= gMaxLargeClass) {
      return mChunk->arena;
    }
    // Best effort detection that we're not trying to access an already
    // disposed arena. In the case of a disposed arena, the memory location
    // pointed by mNode->mArena is either free (but still a valid memory
    // region, per TypedBaseAlloc<arena_t>), in which case its id was reset,
    // or has been reallocated for a new region, and its id is very likely
    // different (per randomness). In both cases, the id is unlikely to
    // match what it was for the disposed arena.
    MOZ_RELEASE_ASSERT(mNode->mArenaId == mNode->mArena->mId);
    return mNode->mArena;
  }

  bool IsValid() const { return !!mSize; }

 private:
  size_t mSize;
  union {
    // Pointer to the chunk associated with the allocation for small
    // and large allocations.
    arena_chunk_t* mChunk;

    // Pointer to the extent node for huge allocations.
    extent_node_t* mNode;
  };
};

inline void MozJemalloc::jemalloc_ptr_info(const void* aPtr,
                                           jemalloc_ptr_info_t* aInfo) {
  arena_chunk_t* chunk = GetChunkForPtr(aPtr);

  // Is the pointer null, or within one chunk's size of null?
  // Alternatively, if the allocator is not initialized yet, the pointer
  // can't be known.
  if (!chunk || !malloc_initialized) {
    *aInfo = {TagUnknown, nullptr, 0, 0};
    return;
  }

  // Look for huge allocations before looking for |chunk| in gChunkRTree.
  // This is necessary because |chunk| won't be in gChunkRTree if it's
  // the second or subsequent chunk in a huge allocation.
  extent_node_t* node;
  extent_node_t key;
  {
    MutexAutoLock lock(huge_mtx);
    key.mAddr = const_cast<void*>(aPtr);
    node =
        reinterpret_cast<RedBlackTree<extent_node_t, ExtentTreeBoundsTrait>*>(
            &huge)
            ->Search(&key);
    if (node) {
      *aInfo = {TagLiveAlloc, node->mAddr, node->mSize, node->mArena->mId};
      return;
    }
  }

  // It's not a huge allocation. Check if we have a known chunk.
  if (!gChunkRTree.Get(chunk)) {
    *aInfo = {TagUnknown, nullptr, 0, 0};
    return;
  }

  MOZ_DIAGNOSTIC_ASSERT(chunk->arena->mMagic == ARENA_MAGIC);

  // Get the page number within the chunk.
  size_t pageind = (((uintptr_t)aPtr - (uintptr_t)chunk) >> gPageSize2Pow);
  if (pageind < gChunkHeaderNumPages) {
    // Within the chunk header.
    *aInfo = {TagUnknown, nullptr, 0, 0};
    return;
  }

  size_t mapbits = chunk->map[pageind].bits;

  if (!(mapbits & CHUNK_MAP_ALLOCATED)) {
    void* pageaddr = (void*)(uintptr_t(aPtr) & ~gPageSizeMask);
    *aInfo = {TagFreedPage, pageaddr, gPageSize, chunk->arena->mId};
    return;
  }

  if (mapbits & CHUNK_MAP_LARGE) {
    // It's a large allocation. Only the first page of a large
    // allocation contains its size, so if the address is not in
    // the first page, scan back to find the allocation size.
    size_t size;
    while (true) {
      size = mapbits & ~gPageSizeMask;
      if (size != 0) {
        break;
      }

      // The following two return paths shouldn't occur in
      // practice unless there is heap corruption.
      pageind--;
      MOZ_DIAGNOSTIC_ASSERT(pageind >= gChunkHeaderNumPages);
      if (pageind < gChunkHeaderNumPages) {
        *aInfo = {TagUnknown, nullptr, 0, 0};
        return;
      }

      mapbits = chunk->map[pageind].bits;
      MOZ_DIAGNOSTIC_ASSERT(mapbits & CHUNK_MAP_LARGE);
      if (!(mapbits & CHUNK_MAP_LARGE)) {
        *aInfo = {TagUnknown, nullptr, 0, 0};
        return;
      }
    }

    void* addr = ((char*)chunk) + (pageind << gPageSize2Pow);
    *aInfo = {TagLiveAlloc, addr, size, chunk->arena->mId};
    return;
  }

  // It must be a small allocation.
  auto run = (arena_run_t*)(mapbits & ~gPageSizeMask);
  MOZ_DIAGNOSTIC_ASSERT(run->mMagic == ARENA_RUN_MAGIC);

  // The allocation size is stored in the run metadata.
  size_t size = run->mBin->mSizeClass;

  // Address of the first possible pointer in the run after its headers.
  uintptr_t reg0_addr = (uintptr_t)run + run->mBin->mRunFirstRegionOffset;
  if (aPtr < (void*)reg0_addr) {
    // In the run header.
    *aInfo = {TagUnknown, nullptr, 0, 0};
    return;
  }

  // Position in the run.
  unsigned regind = ((uintptr_t)aPtr - reg0_addr) / size;

  // Pointer to the allocation's base address.
  void* addr = (void*)(reg0_addr + regind * size);

  // Check if the allocation has been freed.
  unsigned elm = regind >> (LOG2(sizeof(int)) + 3);
  unsigned bit = regind - (elm << (LOG2(sizeof(int)) + 3));
  PtrInfoTag tag =
      ((run->mRegionsMask[elm] & (1U << bit))) ? TagFreedAlloc : TagLiveAlloc;

  *aInfo = {tag, addr, size, chunk->arena->mId};
}

namespace Debug {
// Helper for debuggers. We don't want it to be inlined and optimized out.
MOZ_NEVER_INLINE jemalloc_ptr_info_t* jemalloc_ptr_info(const void* aPtr) {
  static jemalloc_ptr_info_t info;
  MozJemalloc::jemalloc_ptr_info(aPtr, &info);
  return &info;
}
}  // namespace Debug

arena_chunk_t* arena_t::DallocSmall(arena_chunk_t* aChunk, void* aPtr,
                                    arena_chunk_map_t* aMapElm) {
  arena_run_t* run;
  arena_bin_t* bin;
  size_t size;

  run = (arena_run_t*)(aMapElm->bits & ~gPageSizeMask);
  MOZ_DIAGNOSTIC_ASSERT(run->mMagic == ARENA_RUN_MAGIC);
  bin = run->mBin;
  size = bin->mSizeClass;
  MOZ_DIAGNOSTIC_ASSERT(uintptr_t(aPtr) >=
                        uintptr_t(run) + bin->mRunFirstRegionOffset);

  arena_run_reg_dalloc(run, bin, aPtr, size);
  run->mNumFree++;
  arena_chunk_t* dealloc_chunk = nullptr;

  if (run->mNumFree == bin->mRunNumRegions) {
    // Deallocate run.
    if (run == bin->mCurrentRun) {
      bin->mCurrentRun = nullptr;
    } else if (bin->mRunNumRegions != 1) {
      size_t run_pageind =
          (uintptr_t(run) - uintptr_t(aChunk)) >> gPageSize2Pow;
      arena_chunk_map_t* run_mapelm = &aChunk->map[run_pageind];

      // This block's conditional is necessary because if the
      // run only contains one region, then it never gets
      // inserted into the non-full runs tree.
      MOZ_DIAGNOSTIC_ASSERT(bin->mNonFullRuns.Search(run_mapelm) == run_mapelm);
      bin->mNonFullRuns.Remove(run_mapelm);
    }
#if defined(MOZ_DIAGNOSTIC_ASSERT_ENABLED)
    run->mMagic = 0;
#endif
    dealloc_chunk = DallocRun(run, true);
    bin->mNumRuns--;
  } else if (run->mNumFree == 1 && run != bin->mCurrentRun) {
    // Make sure that bin->mCurrentRun always refers to the lowest
    // non-full run, if one exists.
    if (!bin->mCurrentRun) {
      bin->mCurrentRun = run;
    } else if (uintptr_t(run) < uintptr_t(bin->mCurrentRun)) {
      // Switch mCurrentRun.
      if (bin->mCurrentRun->mNumFree > 0) {
        arena_chunk_t* runcur_chunk = GetChunkForPtr(bin->mCurrentRun);
        size_t runcur_pageind =
            (uintptr_t(bin->mCurrentRun) - uintptr_t(runcur_chunk)) >>
            gPageSize2Pow;
        arena_chunk_map_t* runcur_mapelm = &runcur_chunk->map[runcur_pageind];

        // Insert runcur.
        MOZ_DIAGNOSTIC_ASSERT(!bin->mNonFullRuns.Search(runcur_mapelm));
        bin->mNonFullRuns.Insert(runcur_mapelm);
      }
      bin->mCurrentRun = run;
    } else {
      size_t run_pageind =
          (uintptr_t(run) - uintptr_t(aChunk)) >> gPageSize2Pow;
      arena_chunk_map_t* run_mapelm = &aChunk->map[run_pageind];

      MOZ_DIAGNOSTIC_ASSERT(bin->mNonFullRuns.Search(run_mapelm) == nullptr);
      bin->mNonFullRuns.Insert(run_mapelm);
    }
  }
  mStats.allocated_small -= size;

  return dealloc_chunk;
}

arena_chunk_t* arena_t::DallocLarge(arena_chunk_t* aChunk, void* aPtr) {
  MOZ_DIAGNOSTIC_ASSERT((uintptr_t(aPtr) & gPageSizeMask) == 0);
  size_t pageind = (uintptr_t(aPtr) - uintptr_t(aChunk)) >> gPageSize2Pow;
  size_t size = aChunk->map[pageind].bits & ~gPageSizeMask;

  mStats.allocated_large -= size;

  return DallocRun((arena_run_t*)aPtr, true);
}

static inline void arena_dalloc(void* aPtr, size_t aOffset, arena_t* aArena) {
  MOZ_ASSERT(aPtr);
  MOZ_ASSERT(aOffset != 0);
  MOZ_ASSERT(GetChunkOffsetForPtr(aPtr) == aOffset);

  auto chunk = (arena_chunk_t*)((uintptr_t)aPtr - aOffset);
  auto arena = chunk->arena;
  MOZ_ASSERT(arena);
  MOZ_DIAGNOSTIC_ASSERT(arena->mMagic == ARENA_MAGIC);
  MOZ_RELEASE_ASSERT(!aArena || arena == aArena);

  size_t pageind = aOffset >> gPageSize2Pow;
  if (opt_poison) {
    AllocInfo info = AllocInfo::GetInChunk(aPtr, chunk, pageind);
    MOZ_ASSERT(info.IsValid());
    MaybePoison(aPtr, info.Size());
  }

  arena_chunk_t* chunk_dealloc_delay = nullptr;

  {
    MaybeMutexAutoLock lock(arena->mLock);
    arena_chunk_map_t* mapelm = &chunk->map[pageind];
    MOZ_RELEASE_ASSERT(
        (mapelm->bits &
         (CHUNK_MAP_FRESH_MADVISED_OR_DECOMMITTED | CHUNK_MAP_ZEROED)) == 0,
        "Freeing in a page with bad bits.");
    MOZ_RELEASE_ASSERT((mapelm->bits & CHUNK_MAP_ALLOCATED) != 0,
                       "Double-free?");
    if ((mapelm->bits & CHUNK_MAP_LARGE) == 0) {
      // Small allocation.
      chunk_dealloc_delay = arena->DallocSmall(chunk, aPtr, mapelm);
    } else {
      // Large allocation.
      chunk_dealloc_delay = arena->DallocLarge(chunk, aPtr);
    }
  }

  if (chunk_dealloc_delay) {
    chunk_dealloc((void*)chunk_dealloc_delay, kChunkSize, ARENA_CHUNK);
  }
}

static inline void idalloc(void* ptr, arena_t* aArena) {
  size_t offset;

  MOZ_ASSERT(ptr);

  offset = GetChunkOffsetForPtr(ptr);
  if (offset != 0) {
    arena_dalloc(ptr, offset, aArena);
  } else {
    huge_dalloc(ptr, aArena);
  }
}

void arena_t::RallocShrinkLarge(arena_chunk_t* aChunk, void* aPtr, size_t aSize,
                                size_t aOldSize) {
  MOZ_ASSERT(aSize < aOldSize);

  // Shrink the run, and make trailing pages available for other
  // allocations.
  MaybeMutexAutoLock lock(mLock);
  TrimRunTail(aChunk, (arena_run_t*)aPtr, aOldSize, aSize, true);
  mStats.allocated_large -= aOldSize - aSize;
}

// Returns whether reallocation was successful.
bool arena_t::RallocGrowLarge(arena_chunk_t* aChunk, void* aPtr, size_t aSize,
                              size_t aOldSize) {
  size_t pageind = (uintptr_t(aPtr) - uintptr_t(aChunk)) >> gPageSize2Pow;
  size_t npages = aOldSize >> gPageSize2Pow;

  MaybeMutexAutoLock lock(mLock);
  MOZ_DIAGNOSTIC_ASSERT(aOldSize ==
                        (aChunk->map[pageind].bits & ~gPageSizeMask));

  // Try to extend the run.
  MOZ_ASSERT(aSize > aOldSize);
  if (pageind + npages < gChunkNumPages - 1 &&
      (aChunk->map[pageind + npages].bits & CHUNK_MAP_ALLOCATED) == 0 &&
      (aChunk->map[pageind + npages].bits & ~gPageSizeMask) >=
          aSize - aOldSize) {
    // The next run is available and sufficiently large.  Split the
    // following run, then merge the first part with the existing
    // allocation.
    if (!SplitRun((arena_run_t*)(uintptr_t(aChunk) +
                                 ((pageind + npages) << gPageSize2Pow)),
                  aSize - aOldSize, true, false)) {
      return false;
    }

    aChunk->map[pageind].bits = aSize | CHUNK_MAP_LARGE | CHUNK_MAP_ALLOCATED;
    aChunk->map[pageind + npages].bits = CHUNK_MAP_LARGE | CHUNK_MAP_ALLOCATED;

    mStats.allocated_large += aSize - aOldSize;
    return true;
  }

  return false;
}

void* arena_t::RallocSmallOrLarge(void* aPtr, size_t aSize, size_t aOldSize) {
  void* ret;
  size_t copysize;
  SizeClass sizeClass(aSize);

  // Try to avoid moving the allocation.
  if (aOldSize <= gMaxLargeClass && sizeClass.Size() == aOldSize) {
    if (aSize < aOldSize) {
      MaybePoison((void*)(uintptr_t(aPtr) + aSize), aOldSize - aSize);
    }
    return aPtr;
  }
  if (sizeClass.Type() == SizeClass::Large && aOldSize > gMaxBinClass &&
      aOldSize <= gMaxLargeClass) {
    arena_chunk_t* chunk = GetChunkForPtr(aPtr);
    if (sizeClass.Size() < aOldSize) {
      // Fill before shrinking in order to avoid a race.
      MaybePoison((void*)((uintptr_t)aPtr + aSize), aOldSize - aSize);
      RallocShrinkLarge(chunk, aPtr, sizeClass.Size(), aOldSize);
      return aPtr;
    }
    if (RallocGrowLarge(chunk, aPtr, sizeClass.Size(), aOldSize)) {
      ApplyZeroOrJunk((void*)((uintptr_t)aPtr + aOldSize), aSize - aOldSize);
      return aPtr;
    }
  }

  // If we get here, then aSize and aOldSize are different enough that we
  // need to move the object.  In that case, fall back to allocating new
  // space and copying. Allow non-private arenas to switch arenas.
  ret = (mIsPrivate ? this : choose_arena(aSize))->Malloc(aSize, false);
  if (!ret) {
    return nullptr;
  }

  // Junk/zero-filling were already done by arena_t::Malloc().
  copysize = (aSize < aOldSize) ? aSize : aOldSize;
#ifdef VM_COPY_MIN
  if (copysize >= VM_COPY_MIN) {
    pages_copy(ret, aPtr, copysize);
  } else
#endif
  {
    memcpy(ret, aPtr, copysize);
  }
  idalloc(aPtr, this);
  return ret;
}

void* arena_t::Ralloc(void* aPtr, size_t aSize, size_t aOldSize) {
  MOZ_DIAGNOSTIC_ASSERT(mMagic == ARENA_MAGIC);
  MOZ_ASSERT(aPtr);
  MOZ_ASSERT(aSize != 0);

  return (aSize <= gMaxLargeClass) ? RallocSmallOrLarge(aPtr, aSize, aOldSize)
                                   : RallocHuge(aPtr, aSize, aOldSize);
}

void* arena_t::operator new(size_t aCount, const fallible_t&) noexcept {
  MOZ_ASSERT(aCount == sizeof(arena_t));
  return TypedBaseAlloc<arena_t>::alloc();
}

void arena_t::operator delete(void* aPtr) {
  TypedBaseAlloc<arena_t>::dealloc((arena_t*)aPtr);
}

arena_t::arena_t(arena_params_t* aParams, bool aIsPrivate) {
  unsigned i;

  memset(&mLink, 0, sizeof(mLink));
  memset(&mStats, 0, sizeof(arena_stats_t));
  mId = 0;

  // Initialize chunks.
  mChunksDirty.Init();
#ifdef MALLOC_DOUBLE_PURGE
  new (&mChunksMAdvised) DoublyLinkedList<arena_chunk_t>();
#endif
  mSpare = nullptr;

  mRandomizeSmallAllocations = opt_randomize_small;
  MaybeMutex::DoLock doLock = MaybeMutex::MUST_LOCK;
  if (aParams) {
    uint32_t randFlags = aParams->mFlags & ARENA_FLAG_RANDOMIZE_SMALL_MASK;
    switch (randFlags) {
      case ARENA_FLAG_RANDOMIZE_SMALL_ENABLED:
        mRandomizeSmallAllocations = true;
        break;
      case ARENA_FLAG_RANDOMIZE_SMALL_DISABLED:
        mRandomizeSmallAllocations = false;
        break;
      case ARENA_FLAG_RANDOMIZE_SMALL_DEFAULT:
      default:
        break;
    }

    uint32_t threadFlags = aParams->mFlags & ARENA_FLAG_THREAD_MASK;
    if (threadFlags == ARENA_FLAG_THREAD_MAIN_THREAD_ONLY) {
      // At the moment we require that any ARENA_FLAG_THREAD_MAIN_THREAD_ONLY
      // arenas are created and therefore always accessed by the main thread.
      // This is for two reasons:
      //  * it allows jemalloc_stats to read their statistics (we also require
      //    that jemalloc_stats is only used on the main thread).
      //  * Only main-thread or threadsafe arenas can be guanteed to be in a
      //    consistent state after a fork() from the main thread.  If fork()
      //    occurs off-thread then the new child process cannot use these arenas
      //    (new children should usually exec() or exit() since other data may
      //    also be inconsistent).
      MOZ_ASSERT(gArenas.IsOnMainThread());
      MOZ_ASSERT(aIsPrivate);
      doLock = MaybeMutex::AVOID_LOCK_UNSAFE;
    }

    mMaxDirtyIncreaseOverride = aParams->mMaxDirtyIncreaseOverride;
    mMaxDirtyDecreaseOverride = aParams->mMaxDirtyDecreaseOverride;
  } else {
    mMaxDirtyIncreaseOverride = 0;
    mMaxDirtyDecreaseOverride = 0;
  }

  MOZ_RELEASE_ASSERT(mLock.Init(doLock));

  mPRNG = nullptr;
  mIsPRNGInitializing = false;

  mIsPrivate = aIsPrivate;

  mNumDirty = 0;
  mNumFresh = 0;
  mNumMAdvised = 0;
  // The default maximum amount of dirty pages allowed on arenas is a fraction
  // of opt_dirty_max.
  mMaxDirty = (aParams && aParams->mMaxDirty) ? aParams->mMaxDirty
                                              : (opt_dirty_max / 8);

  mRunsAvail.Init();

  // Initialize bins.
  SizeClass sizeClass(1);

  for (i = 0;; i++) {
    arena_bin_t& bin = mBins[i];
    bin.Init(sizeClass);

    // SizeClass doesn't want sizes larger than gMaxBinClass for now.
    if (sizeClass.Size() == gMaxBinClass) {
      break;
    }
    sizeClass = sizeClass.Next();
  }
  MOZ_ASSERT(i == NUM_SMALL_CLASSES - 1);

#if defined(MOZ_DIAGNOSTIC_ASSERT_ENABLED)
  mMagic = ARENA_MAGIC;
#endif
}

arena_t::~arena_t() {
  size_t i;
  MaybeMutexAutoLock lock(mLock);
  MOZ_RELEASE_ASSERT(!mLink.Left() && !mLink.Right(),
                     "Arena is still registered");
  MOZ_RELEASE_ASSERT(!mStats.allocated_small && !mStats.allocated_large,
                     "Arena is not empty");
  if (mSpare) {
    chunk_dealloc(mSpare, kChunkSize, ARENA_CHUNK);
  }
  for (i = 0; i < NUM_SMALL_CLASSES; i++) {
    MOZ_RELEASE_ASSERT(!mBins[i].mNonFullRuns.First(), "Bin is not empty");
  }
#ifdef MOZ_DEBUG
  {
    MutexAutoLock lock(huge_mtx);
    // This is an expensive check, so we only do it on debug builds.
    for (auto node : huge.iter()) {
      MOZ_RELEASE_ASSERT(node->mArenaId != mId, "Arena has huge allocations");
    }
  }
#endif
  mId = 0;
}

arena_t* ArenaCollection::CreateArena(bool aIsPrivate,
                                      arena_params_t* aParams) {
  arena_t* ret = new (fallible) arena_t(aParams, aIsPrivate);
  if (!ret) {
    // Only reached if there is an OOM error.

    // OOM here is quite inconvenient to propagate, since dealing with it
    // would require a check for failure in the fast path.  Instead, punt
    // by using the first arena.
    // In practice, this is an extremely unlikely failure.
    _malloc_message(_getprogname(), ": (malloc) Error initializing arena\n");

    return mDefaultArena;
  }

  MutexAutoLock lock(mLock);

  // For public arenas, it's fine to just use incrementing arena id
  if (!aIsPrivate) {
    ret->mId = mLastPublicArenaId++;
    mArenas.Insert(ret);
    return ret;
  }

  // For private arenas, generate a cryptographically-secure random id for the
  // new arena. If an attacker manages to get control of the process, this
  // should make it more difficult for them to "guess" the ID of a memory
  // arena, stopping them from getting data they may want
  Tree& tree = (ret->IsMainThreadOnly()) ? mMainThreadArenas : mPrivateArenas;
  arena_id_t arena_id;
  do {
    arena_id = MakeRandArenaId(ret->IsMainThreadOnly());
    // Keep looping until we ensure that the random number we just generated
    // isn't already in use by another active arena
  } while (GetByIdInternal(tree, arena_id));

  ret->mId = arena_id;
  tree.Insert(ret);
  return ret;
}

arena_id_t ArenaCollection::MakeRandArenaId(bool aIsMainThreadOnly) const {
  uint64_t rand;
  do {
    mozilla::Maybe<uint64_t> maybeRandomId = mozilla::RandomUint64();
    MOZ_RELEASE_ASSERT(maybeRandomId.isSome());

    rand = maybeRandomId.value();

    // Set or clear the least significant bit depending on if this is a
    // main-thread-only arena.  We use this in GetById.
    if (aIsMainThreadOnly) {
      rand = rand | MAIN_THREAD_ARENA_BIT;
    } else {
      rand = rand & ~MAIN_THREAD_ARENA_BIT;
    }

    // Avoid 0 as an arena Id. We use 0 for disposed arenas.
  } while (rand == 0);

  return arena_id_t(rand);
}

// End arena.
// ***************************************************************************
// Begin general internal functions.

void* arena_t::MallocHuge(size_t aSize, bool aZero) {
  return PallocHuge(aSize, kChunkSize, aZero);
}

void* arena_t::PallocHuge(size_t aSize, size_t aAlignment, bool aZero) {
  void* ret;
  size_t csize;
  size_t psize;
  extent_node_t* node;

  // We're going to configure guard pages in the region between the
  // page-aligned size and the chunk-aligned size, so if those are the same
  // then we need to force that region into existence.
  csize = CHUNK_CEILING(aSize + gPageSize);
  if (csize < aSize) {
    // size is large enough to cause size_t wrap-around.
    return nullptr;
  }

  // Allocate an extent node with which to track the chunk.
  node = ExtentAlloc::alloc();
  if (!node) {
    return nullptr;
  }

  // Allocate one or more contiguous chunks for this request.
  ret = chunk_alloc(csize, aAlignment, false);
  if (!ret) {
    ExtentAlloc::dealloc(node);
    return nullptr;
  }
  psize = PAGE_CEILING(aSize);
#ifdef MOZ_DEBUG
  if (aZero) {
    chunk_assert_zero(ret, psize);
  }
#endif

  // Insert node into huge.
  node->mAddr = ret;
  node->mSize = psize;
  node->mArena = this;
  node->mArenaId = mId;

  {
    MutexAutoLock lock(huge_mtx);
    huge.Insert(node);

    // Although we allocated space for csize bytes, we indicate that we've
    // allocated only psize bytes.
    //
    // If DECOMMIT is defined, this is a reasonable thing to do, since
    // we'll explicitly decommit the bytes in excess of psize.
    //
    // If DECOMMIT is not defined, then we're relying on the OS to be lazy
    // about how it allocates physical pages to mappings.  If we never
    // touch the pages in excess of psize, the OS won't allocate a physical
    // page, and we won't use more than psize bytes of physical memory.
    //
    // A correct program will only touch memory in excess of how much it
    // requested if it first calls malloc_usable_size and finds out how
    // much space it has to play with.  But because we set node->mSize =
    // psize above, malloc_usable_size will return psize, not csize, and
    // the program will (hopefully) never touch bytes in excess of psize.
    // Thus those bytes won't take up space in physical memory, and we can
    // reasonably claim we never "allocated" them in the first place.
    huge_allocated += psize;
    huge_mapped += csize;
  }

  pages_decommit((void*)((uintptr_t)ret + psize), csize - psize);

  if (!aZero) {
    ApplyZeroOrJunk(ret, psize);
  }

  return ret;
}

void* arena_t::RallocHuge(void* aPtr, size_t aSize, size_t aOldSize) {
  void* ret;
  size_t copysize;

  // Avoid moving the allocation if the size class would not change.
  if (aOldSize > gMaxLargeClass &&
      CHUNK_CEILING(aSize + gPageSize) == CHUNK_CEILING(aOldSize + gPageSize)) {
    size_t psize = PAGE_CEILING(aSize);
    if (aSize < aOldSize) {
      MaybePoison((void*)((uintptr_t)aPtr + aSize), aOldSize - aSize);
    }
    if (psize < aOldSize) {
      extent_node_t key;

      pages_decommit((void*)((uintptr_t)aPtr + psize), aOldSize - psize);

      // Update recorded size.
      MutexAutoLock lock(huge_mtx);
      key.mAddr = const_cast<void*>(aPtr);
      extent_node_t* node = huge.Search(&key);
      MOZ_ASSERT(node);
      MOZ_ASSERT(node->mSize == aOldSize);
      MOZ_RELEASE_ASSERT(node->mArena == this);
      huge_allocated -= aOldSize - psize;
      // No need to change huge_mapped, because we didn't (un)map anything.
      node->mSize = psize;
    } else if (psize > aOldSize) {
      if (!pages_commit((void*)((uintptr_t)aPtr + aOldSize),
                        psize - aOldSize)) {
        return nullptr;
      }

      // We need to update the recorded size if the size increased,
      // so malloc_usable_size doesn't return a value smaller than
      // what was requested via realloc().
      extent_node_t key;
      MutexAutoLock lock(huge_mtx);
      key.mAddr = const_cast<void*>(aPtr);
      extent_node_t* node = huge.Search(&key);
      MOZ_ASSERT(node);
      MOZ_ASSERT(node->mSize == aOldSize);
      MOZ_RELEASE_ASSERT(node->mArena == this);
      huge_allocated += psize - aOldSize;
      // No need to change huge_mapped, because we didn't
      // (un)map anything.
      node->mSize = psize;
    }

    if (aSize > aOldSize) {
      ApplyZeroOrJunk((void*)((uintptr_t)aPtr + aOldSize), aSize - aOldSize);
    }
    return aPtr;
  }

  // If we get here, then aSize and aOldSize are different enough that we
  // need to use a different size class.  In that case, fall back to allocating
  // new space and copying. Allow non-private arenas to switch arenas.
  ret = (mIsPrivate ? this : choose_arena(aSize))->MallocHuge(aSize, false);
  if (!ret) {
    return nullptr;
  }

  copysize = (aSize < aOldSize) ? aSize : aOldSize;
#ifdef VM_COPY_MIN
  if (copysize >= VM_COPY_MIN) {
    pages_copy(ret, aPtr, copysize);
  } else
#endif
  {
    memcpy(ret, aPtr, copysize);
  }
  idalloc(aPtr, this);
  return ret;
}

static void huge_dalloc(void* aPtr, arena_t* aArena) {
  extent_node_t* node;
  size_t mapped = 0;
  {
    extent_node_t key;
    MutexAutoLock lock(huge_mtx);

    // Extract from tree of huge allocations.
    key.mAddr = aPtr;
    node = huge.Search(&key);
    MOZ_RELEASE_ASSERT(node, "Double-free?");
    MOZ_ASSERT(node->mAddr == aPtr);
    MOZ_RELEASE_ASSERT(!aArena || node->mArena == aArena);
    // See AllocInfo::Arena.
    MOZ_RELEASE_ASSERT(node->mArenaId == node->mArena->mId);
    huge.Remove(node);

    mapped = CHUNK_CEILING(node->mSize + gPageSize);
    huge_allocated -= node->mSize;
    huge_mapped -= mapped;
  }

  // Unmap chunk.
  chunk_dealloc(node->mAddr, mapped, HUGE_CHUNK);

  ExtentAlloc::dealloc(node);
}

size_t GetKernelPageSize() {
  static size_t kernel_page_size = ([]() {
#ifdef XP_WIN
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    return info.dwPageSize;
#else
    long result = sysconf(_SC_PAGESIZE);
    MOZ_ASSERT(result != -1);
    return result;
#endif
  })();
  return kernel_page_size;
}

// Returns whether the allocator was successfully initialized.
static bool malloc_init_hard() {
  unsigned i;
  const char* opts;

  AutoLock<StaticMutex> lock(gInitLock);

  if (malloc_initialized) {
    // Another thread initialized the allocator before this one
    // acquired gInitLock.
    return true;
  }

  if (!thread_arena.init()) {
    return true;
  }

  // Get page size and number of CPUs
  const size_t page_size = GetKernelPageSize();
  // We assume that the page size is a power of 2.
  MOZ_ASSERT(IsPowerOfTwo(page_size));
#ifdef MALLOC_STATIC_PAGESIZE
  if (gPageSize % page_size) {
    _malloc_message(
        _getprogname(),
        "Compile-time page size does not divide the runtime one.\n");
    MOZ_CRASH();
  }
#else
  gRealPageSize = gPageSize = page_size;
#endif

  // Get runtime configuration.
  if ((opts = getenv("MALLOC_OPTIONS"))) {
    for (i = 0; opts[i] != '\0'; i++) {
      // All options are single letters, some take a *prefix* numeric argument.

      // Parse the argument.
      unsigned prefix_arg = 0;
      while (opts[i] >= '0' && opts[i] <= '9') {
        prefix_arg *= 10;
        prefix_arg += opts[i] - '0';
        i++;
      }

      switch (opts[i]) {
        case 'f':
          opt_dirty_max >>= prefix_arg ? prefix_arg : 1;
          break;
        case 'F':
          prefix_arg = prefix_arg ? prefix_arg : 1;
          if (opt_dirty_max == 0) {
            opt_dirty_max = 1;
            prefix_arg--;
          }
          opt_dirty_max <<= prefix_arg;
          if (opt_dirty_max == 0) {
            // If the shift above overflowed all the bits then clamp the result
            // instead.  If we started with DIRTY_MAX_DEFAULT then this will
            // always be a power of two so choose the maximum power of two that
            // fits in a size_t.
            opt_dirty_max = size_t(1) << (sizeof(size_t) * CHAR_BIT - 1);
          }
          break;
#ifdef MALLOC_RUNTIME_CONFIG
        case 'j':
          opt_junk = false;
          break;
        case 'J':
          opt_junk = true;
          break;
        case 'q':
          // The argument selects how much poisoning to do.
          opt_poison = NONE;
          break;
        case 'Q':
          if (opts[i + 1] == 'Q') {
            // Maximum poisoning.
            i++;
            opt_poison = ALL;
          } else {
            opt_poison = SOME;
            opt_poison_size = kCacheLineSize * prefix_arg;
          }
          break;
        case 'z':
          opt_zero = false;
          break;
        case 'Z':
          opt_zero = true;
          break;
#  ifndef MALLOC_STATIC_PAGESIZE
        case 'P':
          MOZ_ASSERT(gPageSize >= 4_KiB);
          MOZ_ASSERT(gPageSize <= 64_KiB);
          prefix_arg = prefix_arg ? prefix_arg : 1;
          gPageSize <<= prefix_arg;
          // We know that if the shift causes gPageSize to be zero then it's
          // because it shifted all the bits off.  We didn't start with zero.
          // Therefore if gPageSize is out of bounds we set it to 64KiB.
          if (gPageSize < 4_KiB || gPageSize > 64_KiB) {
            gPageSize = 64_KiB;
          }
          break;
#  endif
#endif
        case 'r':
          opt_randomize_small = false;
          break;
        case 'R':
          opt_randomize_small = true;
          break;
        default: {
          char cbuf[2];

          cbuf[0] = opts[i];
          cbuf[1] = '\0';
          _malloc_message(_getprogname(),
                          ": (malloc) Unsupported character "
                          "in malloc options: '",
                          cbuf, "'\n");
        }
      }
    }
  }

#ifndef MALLOC_STATIC_PAGESIZE
  DefineGlobals();
#endif
  gRecycledSize = 0;

  // Initialize chunks data.
  chunks_mtx.Init();
  MOZ_PUSH_IGNORE_THREAD_SAFETY
  gChunksBySize.Init();
  gChunksByAddress.Init();
  MOZ_POP_THREAD_SAFETY

  // Initialize huge allocation data.
  huge_mtx.Init();
  MOZ_PUSH_IGNORE_THREAD_SAFETY
  huge.Init();
  huge_allocated = 0;
  huge_mapped = 0;
  MOZ_POP_THREAD_SAFETY

  // Initialize base allocation data structures.
  base_mtx.Init();
  MOZ_PUSH_IGNORE_THREAD_SAFETY
  base_mapped = 0;
  base_committed = 0;
  MOZ_POP_THREAD_SAFETY

  // Initialize arenas collection here.
  if (!gArenas.Init()) {
    return false;
  }

  // Assign the default arena to the initial thread.
  thread_arena.set(gArenas.GetDefault());

  if (!gChunkRTree.Init()) {
    return false;
  }

  malloc_initialized = true;

  // Dummy call so that the function is not removed by dead-code elimination
  Debug::jemalloc_ptr_info(nullptr);

#if !defined(XP_WIN) && !defined(XP_DARWIN)
  // Prevent potential deadlock on malloc locks after fork.
  pthread_atfork(_malloc_prefork, _malloc_postfork_parent,
                 _malloc_postfork_child);
#endif

  return true;
}

// End general internal functions.
// ***************************************************************************
// Begin malloc(3)-compatible functions.

// The BaseAllocator class is a helper class that implements the base allocator
// functions (malloc, calloc, realloc, free, memalign) for a given arena,
// or an appropriately chosen arena (per choose_arena()) when none is given.
struct BaseAllocator {
#define MALLOC_DECL(name, return_type, ...) \
  inline return_type name(__VA_ARGS__);

#define MALLOC_FUNCS MALLOC_FUNCS_MALLOC_BASE
#include "malloc_decls.h"

  explicit BaseAllocator(arena_t* aArena) : mArena(aArena) {}

 private:
  arena_t* mArena;
};

#define MALLOC_DECL(name, return_type, ...)                  \
  inline return_type MozJemalloc::name(                      \
      ARGS_HELPER(TYPED_ARGS, ##__VA_ARGS__)) {              \
    BaseAllocator allocator(nullptr);                        \
    return allocator.name(ARGS_HELPER(ARGS, ##__VA_ARGS__)); \
  }
#define MALLOC_FUNCS MALLOC_FUNCS_MALLOC_BASE
#include "malloc_decls.h"

inline void* BaseAllocator::malloc(size_t aSize) {
  void* ret;
  arena_t* arena;

  if (!malloc_init()) {
    ret = nullptr;
    goto RETURN;
  }

  if (aSize == 0) {
    aSize = 1;
  }
  // If mArena is non-null, it must not be in the first page.
  MOZ_DIAGNOSTIC_ASSERT_IF(mArena, (size_t)mArena >= gPageSize);
  arena = mArena ? mArena : choose_arena(aSize);
  ret = arena->Malloc(aSize, /* aZero = */ false);

RETURN:
  if (!ret) {
    errno = ENOMEM;
  }

  return ret;
}

inline void* BaseAllocator::memalign(size_t aAlignment, size_t aSize) {
  MOZ_ASSERT(((aAlignment - 1) & aAlignment) == 0);

  if (!malloc_init()) {
    return nullptr;
  }

  if (aSize == 0) {
    aSize = 1;
  }

  aAlignment = aAlignment < sizeof(void*) ? sizeof(void*) : aAlignment;
  arena_t* arena = mArena ? mArena : choose_arena(aSize);
  return arena->Palloc(aAlignment, aSize);
}

inline void* BaseAllocator::calloc(size_t aNum, size_t aSize) {
  void* ret;

  if (malloc_init()) {
    CheckedInt<size_t> checkedSize = CheckedInt<size_t>(aNum) * aSize;
    if (checkedSize.isValid()) {
      size_t allocSize = checkedSize.value();
      if (allocSize == 0) {
        allocSize = 1;
      }
      arena_t* arena = mArena ? mArena : choose_arena(allocSize);
      ret = arena->Malloc(allocSize, /* aZero = */ true);
    } else {
      ret = nullptr;
    }
  } else {
    ret = nullptr;
  }

  if (!ret) {
    errno = ENOMEM;
  }

  return ret;
}

inline void* BaseAllocator::realloc(void* aPtr, size_t aSize) {
  void* ret;

  if (aSize == 0) {
    aSize = 1;
  }

  if (aPtr) {
    MOZ_RELEASE_ASSERT(malloc_initialized);

    auto info = AllocInfo::Get(aPtr);
    auto arena = info.Arena();
    MOZ_RELEASE_ASSERT(!mArena || arena == mArena);
    ret = arena->Ralloc(aPtr, aSize, info.Size());
  } else {
    if (!malloc_init()) {
      ret = nullptr;
    } else {
      arena_t* arena = mArena ? mArena : choose_arena(aSize);
      ret = arena->Malloc(aSize, /* aZero = */ false);
    }
  }

  if (!ret) {
    errno = ENOMEM;
  }
  return ret;
}

inline void BaseAllocator::free(void* aPtr) {
  size_t offset;

  // A version of idalloc that checks for nullptr pointer.
  offset = GetChunkOffsetForPtr(aPtr);
  if (offset != 0) {
    MOZ_RELEASE_ASSERT(malloc_initialized);
    arena_dalloc(aPtr, offset, mArena);
  } else if (aPtr) {
    MOZ_RELEASE_ASSERT(malloc_initialized);
    huge_dalloc(aPtr, mArena);
  }
}

inline int MozJemalloc::posix_memalign(void** aMemPtr, size_t aAlignment,
                                       size_t aSize) {
  return AlignedAllocator<memalign>::posix_memalign(aMemPtr, aAlignment, aSize);
}

inline void* MozJemalloc::aligned_alloc(size_t aAlignment, size_t aSize) {
  return AlignedAllocator<memalign>::aligned_alloc(aAlignment, aSize);
}

inline void* MozJemalloc::valloc(size_t aSize) {
  return AlignedAllocator<memalign>::valloc(aSize);
}

// End malloc(3)-compatible functions.
// ***************************************************************************
// Begin non-standard functions.

// This was added by Mozilla for use by SQLite.
inline size_t MozJemalloc::malloc_good_size(size_t aSize) {
  if (aSize <= gMaxLargeClass) {
    // Small or large
    aSize = SizeClass(aSize).Size();
  } else {
    // Huge.  We use PAGE_CEILING to get psize, instead of using
    // CHUNK_CEILING to get csize.  This ensures that this
    // malloc_usable_size(malloc(n)) always matches
    // malloc_good_size(n).
    aSize = PAGE_CEILING(aSize);
  }
  return aSize;
}

inline size_t MozJemalloc::malloc_usable_size(usable_ptr_t aPtr) {
  return AllocInfo::GetValidated(aPtr).Size();
}

inline void MozJemalloc::jemalloc_stats_internal(
    jemalloc_stats_t* aStats, jemalloc_bin_stats_t* aBinStats) {
  size_t non_arena_mapped, chunk_header_size;

  if (!aStats) {
    return;
  }
  if (!malloc_init()) {
    memset(aStats, 0, sizeof(*aStats));
    return;
  }
  if (aBinStats) {
    memset(aBinStats, 0, sizeof(jemalloc_bin_stats_t) * NUM_SMALL_CLASSES);
  }

  // Gather runtime settings.
  aStats->opt_junk = opt_junk;
  aStats->opt_zero = opt_zero;
  aStats->quantum = kQuantum;
  aStats->quantum_max = kMaxQuantumClass;
  aStats->quantum_wide = kQuantumWide;
  aStats->quantum_wide_max = kMaxQuantumWideClass;
  aStats->subpage_max = gMaxSubPageClass;
  aStats->large_max = gMaxLargeClass;
  aStats->chunksize = kChunkSize;
  aStats->page_size = gPageSize;
  aStats->dirty_max = opt_dirty_max;

  // Gather current memory usage statistics.
  aStats->narenas = 0;
  aStats->mapped = 0;
  aStats->allocated = 0;
  aStats->waste = 0;
  aStats->pages_dirty = 0;
  aStats->pages_fresh = 0;
  aStats->pages_madvised = 0;
  aStats->bookkeeping = 0;
  aStats->bin_unused = 0;

  non_arena_mapped = 0;

  // Get huge mapped/allocated.
  {
    MutexAutoLock lock(huge_mtx);
    non_arena_mapped += huge_mapped;
    aStats->allocated += huge_allocated;
    MOZ_ASSERT(huge_mapped >= huge_allocated);
  }

  // Get base mapped/allocated.
  {
    MutexAutoLock lock(base_mtx);
    non_arena_mapped += base_mapped;
    aStats->bookkeeping += base_committed;
    MOZ_ASSERT(base_mapped >= base_committed);
  }

  gArenas.mLock.Lock();

  // Stats can only read complete information if its run on the main thread.
  MOZ_ASSERT(gArenas.IsOnMainThreadWeak());

  // Iterate over arenas.
  for (auto arena : gArenas.iter()) {
    // Cannot safely read stats for this arena and therefore stats would be
    // incomplete.
    MOZ_ASSERT(arena->mLock.SafeOnThisThread());

    size_t arena_mapped, arena_allocated, arena_committed, arena_dirty,
        arena_fresh, arena_madvised, j, arena_unused, arena_headers;

    arena_headers = 0;
    arena_unused = 0;

    {
      MaybeMutexAutoLock lock(arena->mLock);

      arena_mapped = arena->mStats.mapped;

      // "committed" counts dirty and allocated memory.
      arena_committed = arena->mStats.committed << gPageSize2Pow;

      arena_allocated =
          arena->mStats.allocated_small + arena->mStats.allocated_large;

      arena_dirty = arena->mNumDirty << gPageSize2Pow;
      arena_fresh = arena->mNumFresh << gPageSize2Pow;
      arena_madvised = arena->mNumMAdvised << gPageSize2Pow;

      for (j = 0; j < NUM_SMALL_CLASSES; j++) {
        arena_bin_t* bin = &arena->mBins[j];
        size_t bin_unused = 0;
        size_t num_non_full_runs = 0;

        for (auto mapelm : bin->mNonFullRuns.iter()) {
          arena_run_t* run = (arena_run_t*)(mapelm->bits & ~gPageSizeMask);
          bin_unused += run->mNumFree * bin->mSizeClass;
          num_non_full_runs++;
        }

        if (bin->mCurrentRun) {
          bin_unused += bin->mCurrentRun->mNumFree * bin->mSizeClass;
          num_non_full_runs++;
        }

        arena_unused += bin_unused;
        arena_headers += bin->mNumRuns * bin->mRunFirstRegionOffset;
        if (aBinStats) {
          aBinStats[j].size = bin->mSizeClass;
          aBinStats[j].num_non_full_runs += num_non_full_runs;
          aBinStats[j].num_runs += bin->mNumRuns;
          aBinStats[j].bytes_unused += bin_unused;
          size_t bytes_per_run = static_cast<size_t>(bin->mRunSizePages)
                                 << gPageSize2Pow;
          aBinStats[j].bytes_total +=
              bin->mNumRuns * (bytes_per_run - bin->mRunFirstRegionOffset);
          aBinStats[j].bytes_per_run = bytes_per_run;
        }
      }
    }

    MOZ_ASSERT(arena_mapped >= arena_committed);
    MOZ_ASSERT(arena_committed >= arena_allocated + arena_dirty);

    aStats->mapped += arena_mapped;
    aStats->allocated += arena_allocated;
    aStats->pages_dirty += arena_dirty;
    aStats->pages_fresh += arena_fresh;
    aStats->pages_madvised += arena_madvised;
    // "waste" is committed memory that is neither dirty nor
    // allocated.  If you change this definition please update
    // memory/replace/logalloc/replay/Replay.cpp's jemalloc_stats calculation of
    // committed.
    MOZ_ASSERT(arena_committed >=
               (arena_allocated + arena_dirty + arena_unused + arena_headers));
    aStats->waste += arena_committed - arena_allocated - arena_dirty -
                     arena_unused - arena_headers;
    aStats->bin_unused += arena_unused;
    aStats->bookkeeping += arena_headers;
    aStats->narenas++;
  }
  gArenas.mLock.Unlock();

  // Account for arena chunk headers in bookkeeping rather than waste.
  chunk_header_size =
      ((aStats->mapped / aStats->chunksize) * (gChunkHeaderNumPages - 1))
      << gPageSize2Pow;

  aStats->mapped += non_arena_mapped;
  aStats->bookkeeping += chunk_header_size;
  aStats->waste -= chunk_header_size;

  MOZ_ASSERT(aStats->mapped >= aStats->allocated + aStats->waste +
                                   aStats->pages_dirty + aStats->bookkeeping);
}

inline size_t MozJemalloc::jemalloc_stats_num_bins() {
  return NUM_SMALL_CLASSES;
}

inline void MozJemalloc::jemalloc_set_main_thread() {
  MOZ_ASSERT(malloc_initialized);
  gArenas.SetMainThread();
}

#ifdef MALLOC_DOUBLE_PURGE

// Explicitly remove all of this chunk's MADV_FREE'd pages from memory.
static size_t hard_purge_chunk(arena_chunk_t* aChunk) {
  size_t total_npages = 0;
  // See similar logic in arena_t::Purge().
  for (size_t i = gChunkHeaderNumPages; i < gChunkNumPages; i++) {
    // Find all adjacent pages with CHUNK_MAP_MADVISED set.
    size_t npages;
    for (npages = 0; aChunk->map[i + npages].bits & CHUNK_MAP_MADVISED &&
                     i + npages < gChunkNumPages;
         npages++) {
      // Turn off the page's CHUNK_MAP_MADVISED bit and turn on its
      // CHUNK_MAP_FRESH bit.
      MOZ_DIAGNOSTIC_ASSERT(!(aChunk->map[i + npages].bits &
                              (CHUNK_MAP_FRESH | CHUNK_MAP_DECOMMITTED)));
      aChunk->map[i + npages].bits ^= (CHUNK_MAP_MADVISED | CHUNK_MAP_FRESH);
    }

    // We could use mincore to find out which pages are actually
    // present, but it's not clear that's better.
    if (npages > 0) {
      pages_decommit(((char*)aChunk) + (i << gPageSize2Pow),
                     npages << gPageSize2Pow);
      Unused << pages_commit(((char*)aChunk) + (i << gPageSize2Pow),
                             npages << gPageSize2Pow);
    }
    total_npages += npages;
    i += npages;
  }

  return total_npages;
}

// Explicitly remove all of this arena's MADV_FREE'd pages from memory.
void arena_t::HardPurge() {
  MaybeMutexAutoLock lock(mLock);

  while (!mChunksMAdvised.isEmpty()) {
    arena_chunk_t* chunk = mChunksMAdvised.popFront();
    size_t npages = hard_purge_chunk(chunk);
    mNumMAdvised -= npages;
    mNumFresh += npages;
  }
}

inline void MozJemalloc::jemalloc_purge_freed_pages() {
  if (malloc_initialized) {
    MutexAutoLock lock(gArenas.mLock);
    MOZ_ASSERT(gArenas.IsOnMainThreadWeak());
    for (auto arena : gArenas.iter()) {
      arena->HardPurge();
    }
  }
}

#else  // !defined MALLOC_DOUBLE_PURGE

inline void MozJemalloc::jemalloc_purge_freed_pages() {
  // Do nothing.
}

#endif  // defined MALLOC_DOUBLE_PURGE

inline void MozJemalloc::jemalloc_free_dirty_pages(void) {
  if (malloc_initialized) {
    MutexAutoLock lock(gArenas.mLock);
    MOZ_ASSERT(gArenas.IsOnMainThreadWeak());
    for (auto arena : gArenas.iter()) {
      MaybeMutexAutoLock arena_lock(arena->mLock);
      arena->Purge(1);
    }
  }
}

inline arena_t* ArenaCollection::GetByIdInternal(Tree& aTree,
                                                 arena_id_t aArenaId) {
  // Use AlignedStorage2 to avoid running the arena_t constructor, while
  // we only need it as a placeholder for mId.
  mozilla::AlignedStorage2<arena_t> key;
  key.addr()->mId = aArenaId;
  return aTree.Search(key.addr());
}

inline arena_t* ArenaCollection::GetById(arena_id_t aArenaId, bool aIsPrivate) {
  if (!malloc_initialized) {
    return nullptr;
  }

  Tree* tree = nullptr;
  if (aIsPrivate) {
    if (ArenaIdIsMainThreadOnly(aArenaId)) {
      // Main thread only arena.  Do the lookup here without taking the lock.
      arena_t* result = GetByIdInternal(mMainThreadArenas, aArenaId);
      MOZ_RELEASE_ASSERT(result);
      return result;
    }
    tree = &mPrivateArenas;
  } else {
    tree = &mArenas;
  }

  MutexAutoLock lock(mLock);
  arena_t* result = GetByIdInternal(*tree, aArenaId);
  MOZ_RELEASE_ASSERT(result);
  return result;
}

inline arena_id_t MozJemalloc::moz_create_arena_with_params(
    arena_params_t* aParams) {
  if (malloc_init()) {
    arena_t* arena = gArenas.CreateArena(/* IsPrivate = */ true, aParams);
    return arena->mId;
  }
  return 0;
}

inline void MozJemalloc::moz_dispose_arena(arena_id_t aArenaId) {
  arena_t* arena = gArenas.GetById(aArenaId, /* IsPrivate = */ true);
  MOZ_RELEASE_ASSERT(arena);
  gArenas.DisposeArena(arena);
}

inline void MozJemalloc::moz_set_max_dirty_page_modifier(int32_t aModifier) {
  gArenas.SetDefaultMaxDirtyPageModifier(aModifier);
}

#define MALLOC_DECL(name, return_type, ...)                          \
  inline return_type MozJemalloc::moz_arena_##name(                  \
      arena_id_t aArenaId, ARGS_HELPER(TYPED_ARGS, ##__VA_ARGS__)) { \
    BaseAllocator allocator(                                         \
        gArenas.GetById(aArenaId, /* IsPrivate = */ true));          \
    return allocator.name(ARGS_HELPER(ARGS, ##__VA_ARGS__));         \
  }
#define MALLOC_FUNCS MALLOC_FUNCS_MALLOC_BASE
#include "malloc_decls.h"

// End non-standard functions.
// ***************************************************************************
#ifndef XP_WIN
// Begin library-private functions, used by threading libraries for protection
// of malloc during fork().  These functions are only called if the program is
// running in threaded mode, so there is no need to check whether the program
// is threaded here.
//
// Note that the only way to keep the main-thread-only arenas in a consistent
// state for the child is if fork is called from the main thread only.  Or the
// child must not use them, eg it should call exec().  We attempt to prevent the
// child for accessing these arenas by refusing to re-initialise them.
//
// This is only accessed in the fork handlers while gArenas.mLock is held.
static pthread_t gForkingThread;

#  ifdef XP_DARWIN
// This is only accessed in the fork handlers while gArenas.mLock is held.
static pid_t gForkingProcess;
#  endif

FORK_HOOK
void _malloc_prefork(void) MOZ_NO_THREAD_SAFETY_ANALYSIS {
  // Acquire all mutexes in a safe order.
  gArenas.mLock.Lock();
  gForkingThread = pthread_self();
#  ifdef XP_DARWIN
  gForkingProcess = getpid();
#  endif

  for (auto arena : gArenas.iter()) {
    if (arena->mLock.LockIsEnabled()) {
      arena->mLock.Lock();
    }
  }

  base_mtx.Lock();

  huge_mtx.Lock();
}

FORK_HOOK
void _malloc_postfork_parent(void) MOZ_NO_THREAD_SAFETY_ANALYSIS {
  // Release all mutexes, now that fork() has completed.
  huge_mtx.Unlock();

  base_mtx.Unlock();

  for (auto arena : gArenas.iter()) {
    if (arena->mLock.LockIsEnabled()) {
      arena->mLock.Unlock();
    }
  }

  gArenas.mLock.Unlock();
}

FORK_HOOK
void _malloc_postfork_child(void) {
  // Do this before iterating over the arenas.
  gArenas.ResetMainThread();

  // Reinitialize all mutexes, now that fork() has completed.
  huge_mtx.Init();

  base_mtx.Init();

  for (auto arena : gArenas.iter()) {
    arena->mLock.Reinit(gForkingThread);
  }

  gArenas.mLock.Init();
}

#  ifdef XP_DARWIN
FORK_HOOK
void _malloc_postfork(void) {
  // On MacOS we need to check if this is running in the parent or child
  // process.
  bool is_in_parent = getpid() == gForkingProcess;
  gForkingProcess = 0;
  if (is_in_parent) {
    _malloc_postfork_parent();
  } else {
    _malloc_postfork_child();
  }
}
#  endif  // XP_DARWIN
#endif    // ! XP_WIN

// End library-private functions.
// ***************************************************************************
#ifdef MOZ_REPLACE_MALLOC
// Windows doesn't come with weak imports as they are possible with
// LD_PRELOAD or DYLD_INSERT_LIBRARIES on Linux/OSX. On this platform,
// the replacement functions are defined as variable pointers to the
// function resolved with GetProcAddress() instead of weak definitions
// of functions. On Android, the same needs to happen as well, because
// the Android linker doesn't handle weak linking with non LD_PRELOADed
// libraries, but LD_PRELOADing is not very convenient on Android, with
// the zygote.
#  ifdef XP_DARWIN
#    define MOZ_REPLACE_WEAK __attribute__((weak_import))
#  elif defined(XP_WIN) || defined(ANDROID)
#    define MOZ_DYNAMIC_REPLACE_INIT
#    define replace_init replace_init_decl
#  elif defined(__GNUC__)
#    define MOZ_REPLACE_WEAK __attribute__((weak))
#  endif

#  include "replace_malloc.h"

#  define MALLOC_DECL(name, return_type, ...) CanonicalMalloc::name,

// The default malloc table, i.e. plain allocations. It never changes. It's
// used by init(), and not used after that.
static const malloc_table_t gDefaultMallocTable = {
#  include "malloc_decls.h"
};

// The malloc table installed by init(). It never changes from that point
// onward. It will be the same as gDefaultMallocTable if no replace-malloc tool
// is enabled at startup.
static malloc_table_t gOriginalMallocTable = {
#  include "malloc_decls.h"
};

// The malloc table installed by jemalloc_replace_dynamic(). (Read the
// comments above that function for more details.)
static malloc_table_t gDynamicMallocTable = {
#  include "malloc_decls.h"
};

// This briefly points to gDefaultMallocTable at startup. After that, it points
// to either gOriginalMallocTable or gDynamicMallocTable. It's atomic to avoid
// races when switching between tables.
static Atomic<malloc_table_t const*, mozilla::MemoryOrdering::Relaxed>
    gMallocTablePtr;

#  ifdef MOZ_DYNAMIC_REPLACE_INIT
#    undef replace_init
typedef decltype(replace_init_decl) replace_init_impl_t;
static replace_init_impl_t* replace_init = nullptr;
#  endif

#  ifdef XP_WIN
typedef HMODULE replace_malloc_handle_t;

static replace_malloc_handle_t replace_malloc_handle() {
  wchar_t replace_malloc_lib[1024];
  if (GetEnvironmentVariableW(L"MOZ_REPLACE_MALLOC_LIB", replace_malloc_lib,
                              ArrayLength(replace_malloc_lib)) > 0) {
    return LoadLibraryW(replace_malloc_lib);
  }
  return nullptr;
}

#    define REPLACE_MALLOC_GET_INIT_FUNC(handle) \
      (replace_init_impl_t*)GetProcAddress(handle, "replace_init")

#  elif defined(ANDROID)
#    include <dlfcn.h>

typedef void* replace_malloc_handle_t;

static replace_malloc_handle_t replace_malloc_handle() {
  const char* replace_malloc_lib = getenv("MOZ_REPLACE_MALLOC_LIB");
  if (replace_malloc_lib && *replace_malloc_lib) {
    return dlopen(replace_malloc_lib, RTLD_LAZY);
  }
  return nullptr;
}

#    define REPLACE_MALLOC_GET_INIT_FUNC(handle) \
      (replace_init_impl_t*)dlsym(handle, "replace_init")

#  endif

static void replace_malloc_init_funcs(malloc_table_t*);

#  ifdef MOZ_REPLACE_MALLOC_STATIC
extern "C" void logalloc_init(malloc_table_t*, ReplaceMallocBridge**);

extern "C" void dmd_init(malloc_table_t*, ReplaceMallocBridge**);
#  endif

void phc_init(malloc_table_t*, ReplaceMallocBridge**);

bool Equals(const malloc_table_t& aTable1, const malloc_table_t& aTable2) {
  return memcmp(&aTable1, &aTable2, sizeof(malloc_table_t)) == 0;
}

// Below is the malloc implementation overriding jemalloc and calling the
// replacement functions if they exist.
static ReplaceMallocBridge* gReplaceMallocBridge = nullptr;
static void init() {
  malloc_table_t tempTable = gDefaultMallocTable;

#  ifdef MOZ_DYNAMIC_REPLACE_INIT
  replace_malloc_handle_t handle = replace_malloc_handle();
  if (handle) {
    replace_init = REPLACE_MALLOC_GET_INIT_FUNC(handle);
  }
#  endif

  // Set this *before* calling replace_init, otherwise if replace_init calls
  // malloc() we'll get an infinite loop.
  gMallocTablePtr = &gDefaultMallocTable;

  // Pass in the default allocator table so replace functions can copy and use
  // it for their allocations. The replace_init() function should modify the
  // table if it wants to be active, otherwise leave it unmodified.
  if (replace_init) {
    replace_init(&tempTable, &gReplaceMallocBridge);
  }
#  ifdef MOZ_REPLACE_MALLOC_STATIC
  if (Equals(tempTable, gDefaultMallocTable)) {
    logalloc_init(&tempTable, &gReplaceMallocBridge);
  }
#    ifdef MOZ_DMD
  if (Equals(tempTable, gDefaultMallocTable)) {
    dmd_init(&tempTable, &gReplaceMallocBridge);
  }
#    endif
#  endif
  if (!Equals(tempTable, gDefaultMallocTable)) {
    replace_malloc_init_funcs(&tempTable);
  }
  gOriginalMallocTable = tempTable;
  gMallocTablePtr = &gOriginalMallocTable;
}

// WARNING WARNING WARNING: this function should be used with extreme care. It
// is not as general-purpose as it looks. It is currently used by
// tools/profiler/core/memory_hooks.cpp for counting allocations and probably
// should not be used for any other purpose.
//
// This function allows the original malloc table to be temporarily replaced by
// a different malloc table. Or, if the argument is nullptr, it switches back to
// the original malloc table.
//
// Limitations:
//
// - It is not threadsafe. If multiple threads pass it the same
//   `replace_init_func` at the same time, there will be data races writing to
//   the malloc_table_t within that function.
//
// - Only one replacement can be installed. No nesting is allowed.
//
// - The new malloc table must be able to free allocations made by the original
//   malloc table, and upon removal the original malloc table must be able to
//   free allocations made by the new malloc table. This means the new malloc
//   table can only do simple things like recording extra information, while
//   delegating actual allocation/free operations to the original malloc table.
//
MOZ_JEMALLOC_API void jemalloc_replace_dynamic(
    jemalloc_init_func replace_init_func) {
  if (replace_init_func) {
    malloc_table_t tempTable = gOriginalMallocTable;
    (*replace_init_func)(&tempTable, &gReplaceMallocBridge);
    if (!Equals(tempTable, gOriginalMallocTable)) {
      replace_malloc_init_funcs(&tempTable);

      // Temporarily switch back to the original malloc table. In the
      // (supported) non-nested case, this is a no-op. But just in case this is
      // a (unsupported) nested call, it makes the overwriting of
      // gDynamicMallocTable less racy, because ongoing calls to malloc() and
      // friends won't go through gDynamicMallocTable.
      gMallocTablePtr = &gOriginalMallocTable;

      gDynamicMallocTable = tempTable;
      gMallocTablePtr = &gDynamicMallocTable;
      // We assume that dynamic replaces don't occur close enough for a
      // thread to still have old copies of the table pointer when the 2nd
      // replace occurs.
    }
  } else {
    // Switch back to the original malloc table.
    gMallocTablePtr = &gOriginalMallocTable;
  }
}

#  define MALLOC_DECL(name, return_type, ...)                           \
    inline return_type ReplaceMalloc::name(                             \
        ARGS_HELPER(TYPED_ARGS, ##__VA_ARGS__)) {                       \
      if (MOZ_UNLIKELY(!gMallocTablePtr)) {                             \
        init();                                                         \
      }                                                                 \
      return (*gMallocTablePtr).name(ARGS_HELPER(ARGS, ##__VA_ARGS__)); \
    }
#  include "malloc_decls.h"

MOZ_JEMALLOC_API struct ReplaceMallocBridge* get_bridge(void) {
  if (MOZ_UNLIKELY(!gMallocTablePtr)) {
    init();
  }
  return gReplaceMallocBridge;
}

// posix_memalign, aligned_alloc, memalign and valloc all implement some kind
// of aligned memory allocation. For convenience, a replace-malloc library can
// skip defining replace_posix_memalign, replace_aligned_alloc and
// replace_valloc, and default implementations will be automatically derived
// from replace_memalign.
static void replace_malloc_init_funcs(malloc_table_t* table) {
  if (table->posix_memalign == CanonicalMalloc::posix_memalign &&
      table->memalign != CanonicalMalloc::memalign) {
    table->posix_memalign =
        AlignedAllocator<ReplaceMalloc::memalign>::posix_memalign;
  }
  if (table->aligned_alloc == CanonicalMalloc::aligned_alloc &&
      table->memalign != CanonicalMalloc::memalign) {
    table->aligned_alloc =
        AlignedAllocator<ReplaceMalloc::memalign>::aligned_alloc;
  }
  if (table->valloc == CanonicalMalloc::valloc &&
      table->memalign != CanonicalMalloc::memalign) {
    table->valloc = AlignedAllocator<ReplaceMalloc::memalign>::valloc;
  }
  if (table->moz_create_arena_with_params ==
          CanonicalMalloc::moz_create_arena_with_params &&
      table->malloc != CanonicalMalloc::malloc) {
#  define MALLOC_DECL(name, ...) \
    table->name = DummyArenaAllocator<ReplaceMalloc>::name;
#  define MALLOC_FUNCS MALLOC_FUNCS_ARENA_BASE
#  include "malloc_decls.h"
  }
  if (table->moz_arena_malloc == CanonicalMalloc::moz_arena_malloc &&
      table->malloc != CanonicalMalloc::malloc) {
#  define MALLOC_DECL(name, ...) \
    table->name = DummyArenaAllocator<ReplaceMalloc>::name;
#  define MALLOC_FUNCS MALLOC_FUNCS_ARENA_ALLOC
#  include "malloc_decls.h"
  }
}

#endif  // MOZ_REPLACE_MALLOC
// ***************************************************************************
// Definition of all the _impl functions
// GENERIC_MALLOC_DECL2_MINGW is only used for the MinGW build, and aliases
// the malloc funcs (e.g. malloc) to the je_ versions. It does not generate
// aliases for the other functions (jemalloc and arena functions).
//
// We do need aliases for the other mozglue.def-redirected functions though,
// these are done at the bottom of mozmemory_wrap.cpp
#define GENERIC_MALLOC_DECL2_MINGW(name, name_impl, return_type, ...) \
  return_type name(ARGS_HELPER(TYPED_ARGS, ##__VA_ARGS__))            \
      __attribute__((alias(MOZ_STRINGIFY(name_impl))));

#define GENERIC_MALLOC_DECL2(attributes, name, name_impl, return_type, ...)  \
  return_type name_impl(ARGS_HELPER(TYPED_ARGS, ##__VA_ARGS__)) attributes { \
    return DefaultMalloc::name(ARGS_HELPER(ARGS, ##__VA_ARGS__));            \
  }

#ifndef __MINGW32__
#  define GENERIC_MALLOC_DECL(attributes, name, return_type, ...)    \
    GENERIC_MALLOC_DECL2(attributes, name, name##_impl, return_type, \
                         ##__VA_ARGS__)
#else
#  define GENERIC_MALLOC_DECL(attributes, name, return_type, ...)    \
    GENERIC_MALLOC_DECL2(attributes, name, name##_impl, return_type, \
                         ##__VA_ARGS__)                              \
    GENERIC_MALLOC_DECL2_MINGW(name, name##_impl, return_type, ##__VA_ARGS__)
#endif

#define NOTHROW_MALLOC_DECL(...) \
  MOZ_MEMORY_API MACRO_CALL(GENERIC_MALLOC_DECL, (noexcept(true), __VA_ARGS__))
#define MALLOC_DECL(...) \
  MOZ_MEMORY_API MACRO_CALL(GENERIC_MALLOC_DECL, (, __VA_ARGS__))
#define MALLOC_FUNCS MALLOC_FUNCS_MALLOC
#include "malloc_decls.h"

#undef GENERIC_MALLOC_DECL
#define GENERIC_MALLOC_DECL(attributes, name, return_type, ...) \
  GENERIC_MALLOC_DECL2(attributes, name, name, return_type, ##__VA_ARGS__)

#define MALLOC_DECL(...) \
  MOZ_JEMALLOC_API MACRO_CALL(GENERIC_MALLOC_DECL, (, __VA_ARGS__))
#define MALLOC_FUNCS (MALLOC_FUNCS_JEMALLOC | MALLOC_FUNCS_ARENA)
#include "malloc_decls.h"
// ***************************************************************************

#ifdef HAVE_DLFCN_H
#  include <dlfcn.h>
#endif

#if defined(__GLIBC__) && !defined(__UCLIBC__)
// glibc provides the RTLD_DEEPBIND flag for dlopen which can make it possible
// to inconsistently reference libc's malloc(3)-compatible functions
// (bug 493541).
//
// These definitions interpose hooks in glibc.  The functions are actually
// passed an extra argument for the caller return address, which will be
// ignored.

extern "C" {
MOZ_EXPORT void (*__free_hook)(void*) = free_impl;
MOZ_EXPORT void* (*__malloc_hook)(size_t) = malloc_impl;
MOZ_EXPORT void* (*__realloc_hook)(void*, size_t) = realloc_impl;
MOZ_EXPORT void* (*__memalign_hook)(size_t, size_t) = memalign_impl;
}

#elif defined(RTLD_DEEPBIND)
// XXX On systems that support RTLD_GROUP or DF_1_GROUP, do their
// implementations permit similar inconsistencies?  Should STV_SINGLETON
// visibility be used for interposition where available?
#  error \
      "Interposing malloc is unsafe on this system without libc malloc hooks."
#endif

#ifdef XP_WIN
MOZ_EXPORT void* _recalloc(void* aPtr, size_t aCount, size_t aSize) {
  size_t oldsize = aPtr ? AllocInfo::Get(aPtr).Size() : 0;
  CheckedInt<size_t> checkedSize = CheckedInt<size_t>(aCount) * aSize;

  if (!checkedSize.isValid()) {
    return nullptr;
  }

  size_t newsize = checkedSize.value();

  // In order for all trailing bytes to be zeroed, the caller needs to
  // use calloc(), followed by recalloc().  However, the current calloc()
  // implementation only zeros the bytes requested, so if recalloc() is
  // to work 100% correctly, calloc() will need to change to zero
  // trailing bytes.
  aPtr = DefaultMalloc::realloc(aPtr, newsize);
  if (aPtr && oldsize < newsize) {
    memset((void*)((uintptr_t)aPtr + oldsize), 0, newsize - oldsize);
  }

  return aPtr;
}

// This impl of _expand doesn't ever actually expand or shrink blocks: it
// simply replies that you may continue using a shrunk block.
MOZ_EXPORT void* _expand(void* aPtr, size_t newsize) {
  if (AllocInfo::Get(aPtr).Size() >= newsize) {
    return aPtr;
  }

  return nullptr;
}

MOZ_EXPORT size_t _msize(void* aPtr) {
  return DefaultMalloc::malloc_usable_size(aPtr);
}
#endif

#ifdef MOZ_PHC
// Compile PHC and mozjemalloc together so that PHC can inline mozjemalloc.
#  include "PHC.cpp"
#endif
