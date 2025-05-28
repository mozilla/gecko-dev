/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CONSTANTS_H
#define CONSTANTS_H

#include "mozilla/Literals.h"
#include "mozilla/MathAlgorithms.h"

#include "Utils.h"

// This file contains compile-time connstants that don't depend on sizes of
// structures or the page size.  It can be included before defining
// structures and classes.  Other runtime or structure-dependant options are
// in options.h

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

// Our size classes are inclusive ranges of memory sizes.  By describing the
// minimums and how memory is allocated in each range the maximums can be
// calculated.

// Smallest size class to support.  On Windows the smallest allocation size
// must be 8 bytes on 32-bit, 16 bytes on 64-bit.  On Linux and Mac, even
// malloc(1) must reserve a word's worth of memory (see Mozilla bug 691003).
#ifdef XP_WIN
static constexpr size_t kMinTinyClass = sizeof(void*) * 2;
#else
static constexpr size_t kMinTinyClass = sizeof(void*);
#endif

// Maximum tiny size class.
static constexpr size_t kMaxTinyClass = 8;

// Smallest quantum-spaced size classes. It could actually also be labelled a
// tiny allocation, and is spaced as such from the largest tiny size class.
// Tiny classes being powers of 2, this is twice as large as the largest of
// them.
static constexpr size_t kMinQuantumClass = kMaxTinyClass * 2;
static constexpr size_t kMinQuantumWideClass = 512;
static constexpr size_t kMinSubPageClass = 4_KiB;

// Amount (quantum) separating quantum-spaced size classes.
static constexpr size_t kQuantum = 16;
static constexpr size_t kQuantumMask = kQuantum - 1;
static constexpr size_t kQuantumWide = 256;
static constexpr size_t kQuantumWideMask = kQuantumWide - 1;

static constexpr size_t kMaxQuantumClass = kMinQuantumWideClass - kQuantum;
static constexpr size_t kMaxQuantumWideClass = kMinSubPageClass - kQuantumWide;

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
static constexpr size_t kNumTinyClasses =
    LOG2(kMaxTinyClass) - LOG2(kMinTinyClass) + 1;

// Number of quantum-spaced classes.  We add kQuantum(Max) before subtracting to
// avoid underflow when a class is empty (Max<Min).
static constexpr size_t kNumQuantumClasses =
    (kMaxQuantumClass + kQuantum - kMinQuantumClass) / kQuantum;
static constexpr size_t kNumQuantumWideClasses =
    (kMaxQuantumWideClass + kQuantumWide - kMinQuantumWideClass) / kQuantumWide;

// Size and alignment of memory chunks that are allocated by the OS's virtual
// memory system.
static constexpr size_t kChunkSize = 1_MiB;
static constexpr size_t kChunkSizeMask = kChunkSize - 1;

// Maximum size of L1 cache line.  This is used to avoid cache line aliasing,
// so over-estimates are okay (up to a point), but under-estimates will
// negatively affect performance.
constexpr size_t kCacheLineSize =
#if defined(XP_DARWIN) && defined(__aarch64__)
    128
#else
    64
#endif
    ;

// Recycle at most 128 MiB of chunks. This means we retain at most
// 6.25% of the process address space on a 32-bit OS for later use.
static constexpr size_t gRecycleLimit = 128_MiB;

#endif /* ! CONSTANTS_H */
