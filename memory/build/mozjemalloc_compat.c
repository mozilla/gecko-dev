/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZ_JEMALLOC3
#  error Should only compile this file when building with jemalloc 3
#endif

#define MOZ_JEMALLOC_IMPL

#include "mozmemory_wrap.h"
#include "jemalloc_types.h"
#include "mozilla/Types.h"

#include <stdbool.h>

#if defined(MOZ_NATIVE_JEMALLOC)

MOZ_IMPORT_API int
je_(mallctl)(const char*, void*, size_t*, void*, size_t);
MOZ_IMPORT_API int
je_(mallctlnametomib)(const char *name, size_t *mibp, size_t *miblenp);
MOZ_IMPORT_API int
je_(mallctlbymib)(const size_t *mib, size_t miblen, void *oldp, size_t *oldlenp, void *newp, size_t newlen);
MOZ_IMPORT_API int
je_(nallocx)(size_t size, int flags);

#else
#  include "jemalloc/jemalloc.h"
#endif

/*
 *  CTL_* macros are from memory/jemalloc/src/src/stats.c with changes:
 *  - drop `t' argument to avoid redundancy in calculating type size
 *  - require `i' argument for arena number explicitly
 */

#define	CTL_GET(n, v) do {						\
	size_t sz = sizeof(v);						\
	je_(mallctl)(n, &v, &sz, NULL, 0);				\
} while (0)

#define	CTL_I_GET(n, v, i) do {						\
	size_t mib[6];							\
	size_t miblen = sizeof(mib) / sizeof(mib[0]);			\
	size_t sz = sizeof(v);						\
	je_(mallctlnametomib)(n, mib, &miblen);			\
	mib[2] = i;							\
	je_(mallctlbymib)(mib, miblen, &v, &sz, NULL, 0);		\
} while (0)

#define	CTL_IJ_GET(n, v, i, j) do {					\
	size_t mib[6];							\
	size_t miblen = sizeof(mib) / sizeof(mib[0]);			\
	size_t sz = sizeof(v);						\
	je_(mallctlnametomib)(n, mib, &miblen);				\
	mib[2] = i;							\
	mib[4] = j;							\
	je_(mallctlbymib)(mib, miblen, &v, &sz, NULL, 0);			\
} while (0)

/*
 * VARIABLE_ARRAY is copied from
 * memory/jemalloc/src/include/jemalloc/internal/jemalloc_internal.h.in
 */
#if __STDC_VERSION__ < 199901L
#  ifdef _MSC_VER
#    include <malloc.h>
#    define alloca _alloca
#  else
#    ifdef HAVE_ALLOCA_H
#      include <alloca.h>
#    else
#      include <stdlib.h>
#    endif
#  endif
#  define VARIABLE_ARRAY(type, name, count) \
	type *name = alloca(sizeof(type) * (count))
#else
#  define VARIABLE_ARRAY(type, name, count) type name[(count)]
#endif

MOZ_MEMORY_API size_t
malloc_good_size_impl(size_t size)
{
  /* je_nallocx crashes when given a size of 0. As
   * malloc_usable_size(malloc(0)) and malloc_usable_size(malloc(1))
   * return the same value, use a size of 1. */
  if (size == 0)
    size = 1;
  return je_(nallocx)(size, 0);
}

static size_t
compute_bin_unused(unsigned int narenas)
{
    size_t bin_unused = 0;

    uint32_t nregs; // number of regions per run in the j-th bin
    size_t reg_size; // size of regions served by the j-th bin
    size_t curruns; // number of runs belonging to a bin
    size_t curregs; // number of allocated regions in a bin

    unsigned int nbins; // number of bins per arena
    unsigned int i, j;

    // narenas also counts uninitialized arenas, and initialized arenas
    // are not guaranteed to be adjacent
    VARIABLE_ARRAY(bool, initialized, narenas);
    size_t isz = sizeof(initialized) / sizeof(initialized[0]);

    je_(mallctl)("arenas.initialized", initialized, &isz, NULL, 0);
    CTL_GET("arenas.nbins", nbins);

    for (j = 0; j < nbins; j++) {
        CTL_I_GET("arenas.bin.0.nregs", nregs, j);
        CTL_I_GET("arenas.bin.0.size", reg_size, j);

        for (i = 0; i < narenas; i++) {
            if (!initialized[i]) {
                continue;
            }

            CTL_IJ_GET("stats.arenas.0.bins.0.curruns", curruns, i, j);
            CTL_IJ_GET("stats.arenas.0.bins.0.curregs", curregs, i, j);

            bin_unused += (nregs * curruns - curregs) * reg_size;
        }
    }

    return bin_unused;
}

MOZ_JEMALLOC_API void
jemalloc_stats_impl(jemalloc_stats_t *stats)
{
  unsigned narenas;
  size_t active, allocated, mapped, page, pdirty;
  size_t lg_chunk;

  // Refresh jemalloc's stats by updating its epoch, see ctl_refresh in
  // src/ctl.c
  uint64_t epoch = 0;
  size_t esz = sizeof(epoch);
  int ret = je_(mallctl)("epoch", &epoch, &esz, &epoch, esz);

  CTL_GET("arenas.narenas", narenas);
  CTL_GET("arenas.page", page);
  CTL_GET("stats.active", active);
  CTL_GET("stats.allocated", allocated);
  CTL_GET("stats.mapped", mapped);
  CTL_GET("opt.lg_chunk", lg_chunk);
  CTL_GET("stats.bookkeeping", stats->bookkeeping);

  /* get the summation for all arenas, i == narenas */
  CTL_I_GET("stats.arenas.0.pdirty", pdirty, narenas);

  stats->chunksize = (size_t) 1 << lg_chunk;
  stats->mapped = mapped;
  stats->allocated = allocated;
  stats->waste = active - allocated;
  stats->page_cache = pdirty * page;
  stats->bin_unused = compute_bin_unused(narenas);
  stats->waste -= stats->bin_unused;
}

MOZ_JEMALLOC_API void
jemalloc_purge_freed_pages_impl()
{
}

MOZ_JEMALLOC_API void
jemalloc_free_dirty_pages_impl()
{
  unsigned narenas;
  size_t mib[3];
  size_t miblen = sizeof(mib) / sizeof(mib[0]);

  CTL_GET("arenas.narenas", narenas);
  je_(mallctlnametomib)("arena.0.purge", mib, &miblen);
  mib[1] = narenas;
  je_(mallctlbymib)(mib, miblen, NULL, NULL, NULL, 0);
}
