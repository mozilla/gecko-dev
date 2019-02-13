#define	JEMALLOC_TCACHE_C_
#include "jemalloc/internal/jemalloc_internal.h"

/******************************************************************************/
/* Data. */

bool	opt_tcache = true;
ssize_t	opt_lg_tcache_max = LG_TCACHE_MAXCLASS_DEFAULT;

tcache_bin_info_t	*tcache_bin_info;
static unsigned		stack_nelms; /* Total stack elms per tcache. */

size_t			nhbins;
size_t			tcache_maxclass;

/******************************************************************************/

size_t	tcache_salloc(const void *ptr)
{

	return (arena_salloc(ptr, false));
}

void
tcache_event_hard(tcache_t *tcache)
{
	index_t binind = tcache->next_gc_bin;
	tcache_bin_t *tbin = &tcache->tbins[binind];
	tcache_bin_info_t *tbin_info = &tcache_bin_info[binind];

	if (tbin->low_water > 0) {
		/*
		 * Flush (ceiling) 3/4 of the objects below the low water mark.
		 */
		if (binind < NBINS) {
			tcache_bin_flush_small(tbin, binind, tbin->ncached -
			    tbin->low_water + (tbin->low_water >> 2), tcache);
		} else {
			tcache_bin_flush_large(tbin, binind, tbin->ncached -
			    tbin->low_water + (tbin->low_water >> 2), tcache);
		}
		/*
		 * Reduce fill count by 2X.  Limit lg_fill_div such that the
		 * fill count is always at least 1.
		 */
		if ((tbin_info->ncached_max >> (tbin->lg_fill_div+1)) >= 1)
			tbin->lg_fill_div++;
	} else if (tbin->low_water < 0) {
		/*
		 * Increase fill count by 2X.  Make sure lg_fill_div stays
		 * greater than 0.
		 */
		if (tbin->lg_fill_div > 1)
			tbin->lg_fill_div--;
	}
	tbin->low_water = tbin->ncached;

	tcache->next_gc_bin++;
	if (tcache->next_gc_bin == nhbins)
		tcache->next_gc_bin = 0;
	tcache->ev_cnt = 0;
}

void *
tcache_alloc_small_hard(tcache_t *tcache, tcache_bin_t *tbin, index_t binind)
{
	void *ret;

	arena_tcache_fill_small(tcache->arena, tbin, binind,
	    config_prof ? tcache->prof_accumbytes : 0);
	if (config_prof)
		tcache->prof_accumbytes = 0;
	ret = tcache_alloc_easy(tbin);

	return (ret);
}

void
tcache_bin_flush_small(tcache_bin_t *tbin, index_t binind, unsigned rem,
    tcache_t *tcache)
{
	void *ptr;
	unsigned i, nflush, ndeferred;
	bool merged_stats = false;

	assert(binind < NBINS);
	assert(rem <= tbin->ncached);

	for (nflush = tbin->ncached - rem; nflush > 0; nflush = ndeferred) {
		/* Lock the arena bin associated with the first object. */
		arena_chunk_t *chunk = (arena_chunk_t *)CHUNK_ADDR2BASE(
		    tbin->avail[0]);
		arena_t *arena = chunk->arena;
		arena_bin_t *bin = &arena->bins[binind];

		if (config_prof && arena == tcache->arena) {
			if (arena_prof_accum(arena, tcache->prof_accumbytes))
				prof_idump();
			tcache->prof_accumbytes = 0;
		}

		malloc_mutex_lock(&bin->lock);
		if (config_stats && arena == tcache->arena) {
			assert(!merged_stats);
			merged_stats = true;
			bin->stats.nflushes++;
			bin->stats.nrequests += tbin->tstats.nrequests;
			tbin->tstats.nrequests = 0;
		}
		ndeferred = 0;
		for (i = 0; i < nflush; i++) {
			ptr = tbin->avail[i];
			assert(ptr != NULL);
			chunk = (arena_chunk_t *)CHUNK_ADDR2BASE(ptr);
			if (chunk->arena == arena) {
				size_t pageind = ((uintptr_t)ptr -
				    (uintptr_t)chunk) >> LG_PAGE;
				arena_chunk_map_bits_t *bitselm =
				    arena_bitselm_get(chunk, pageind);
				arena_dalloc_bin_junked_locked(arena, chunk,
				    ptr, bitselm);
			} else {
				/*
				 * This object was allocated via a different
				 * arena bin than the one that is currently
				 * locked.  Stash the object, so that it can be
				 * handled in a future pass.
				 */
				tbin->avail[ndeferred] = ptr;
				ndeferred++;
			}
		}
		malloc_mutex_unlock(&bin->lock);
	}
	if (config_stats && !merged_stats) {
		/*
		 * The flush loop didn't happen to flush to this thread's
		 * arena, so the stats didn't get merged.  Manually do so now.
		 */
		arena_bin_t *bin = &tcache->arena->bins[binind];
		malloc_mutex_lock(&bin->lock);
		bin->stats.nflushes++;
		bin->stats.nrequests += tbin->tstats.nrequests;
		tbin->tstats.nrequests = 0;
		malloc_mutex_unlock(&bin->lock);
	}

	memmove(tbin->avail, &tbin->avail[tbin->ncached - rem],
	    rem * sizeof(void *));
	tbin->ncached = rem;
	if ((int)tbin->ncached < tbin->low_water)
		tbin->low_water = tbin->ncached;
}

void
tcache_bin_flush_large(tcache_bin_t *tbin, index_t binind, unsigned rem,
    tcache_t *tcache)
{
	void *ptr;
	unsigned i, nflush, ndeferred;
	bool merged_stats = false;

	assert(binind < nhbins);
	assert(rem <= tbin->ncached);

	for (nflush = tbin->ncached - rem; nflush > 0; nflush = ndeferred) {
		/* Lock the arena associated with the first object. */
		arena_chunk_t *chunk = (arena_chunk_t *)CHUNK_ADDR2BASE(
		    tbin->avail[0]);
		arena_t *arena = chunk->arena;
		UNUSED bool idump;

		if (config_prof)
			idump = false;
		malloc_mutex_lock(&arena->lock);
		if ((config_prof || config_stats) && arena == tcache->arena) {
			if (config_prof) {
				idump = arena_prof_accum_locked(arena,
				    tcache->prof_accumbytes);
				tcache->prof_accumbytes = 0;
			}
			if (config_stats) {
				merged_stats = true;
				arena->stats.nrequests_large +=
				    tbin->tstats.nrequests;
				arena->stats.lstats[binind - NBINS].nrequests +=
				    tbin->tstats.nrequests;
				tbin->tstats.nrequests = 0;
			}
		}
		ndeferred = 0;
		for (i = 0; i < nflush; i++) {
			ptr = tbin->avail[i];
			assert(ptr != NULL);
			chunk = (arena_chunk_t *)CHUNK_ADDR2BASE(ptr);
			if (chunk->arena == arena) {
				arena_dalloc_large_junked_locked(arena, chunk,
				    ptr);
			} else {
				/*
				 * This object was allocated via a different
				 * arena than the one that is currently locked.
				 * Stash the object, so that it can be handled
				 * in a future pass.
				 */
				tbin->avail[ndeferred] = ptr;
				ndeferred++;
			}
		}
		malloc_mutex_unlock(&arena->lock);
		if (config_prof && idump)
			prof_idump();
	}
	if (config_stats && !merged_stats) {
		/*
		 * The flush loop didn't happen to flush to this thread's
		 * arena, so the stats didn't get merged.  Manually do so now.
		 */
		arena_t *arena = tcache->arena;
		malloc_mutex_lock(&arena->lock);
		arena->stats.nrequests_large += tbin->tstats.nrequests;
		arena->stats.lstats[binind - NBINS].nrequests +=
		    tbin->tstats.nrequests;
		tbin->tstats.nrequests = 0;
		malloc_mutex_unlock(&arena->lock);
	}

	memmove(tbin->avail, &tbin->avail[tbin->ncached - rem],
	    rem * sizeof(void *));
	tbin->ncached = rem;
	if ((int)tbin->ncached < tbin->low_water)
		tbin->low_water = tbin->ncached;
}

void
tcache_arena_associate(tcache_t *tcache, arena_t *arena)
{

	if (config_stats) {
		/* Link into list of extant tcaches. */
		malloc_mutex_lock(&arena->lock);
		ql_elm_new(tcache, link);
		ql_tail_insert(&arena->tcache_ql, tcache, link);
		malloc_mutex_unlock(&arena->lock);
	}
	tcache->arena = arena;
}

void
tcache_arena_reassociate(tcache_t *tcache, arena_t *arena)
{

	tcache_arena_dissociate(tcache);
	tcache_arena_associate(tcache, arena);
}

void
tcache_arena_dissociate(tcache_t *tcache)
{

	if (config_stats) {
		/* Unlink from list of extant tcaches. */
		malloc_mutex_lock(&tcache->arena->lock);
		ql_remove(&tcache->arena->tcache_ql, tcache, link);
		tcache_stats_merge(tcache, tcache->arena);
		malloc_mutex_unlock(&tcache->arena->lock);
	}
}

tcache_t *
tcache_get_hard(tsd_t *tsd)
{
	arena_t *arena;

	if (!tcache_enabled_get()) {
		if (tsd_nominal(tsd))
			tcache_enabled_set(false); /* Memoize. */
		return (NULL);
	}
	arena = arena_choose(tsd, NULL);
	if (unlikely(arena == NULL))
		return (NULL);
	return (tcache_create(tsd, arena));
}

tcache_t *
tcache_create(tsd_t *tsd, arena_t *arena)
{
	tcache_t *tcache;
	size_t size, stack_offset;
	unsigned i;

	size = offsetof(tcache_t, tbins) + (sizeof(tcache_bin_t) * nhbins);
	/* Naturally align the pointer stacks. */
	size = PTR_CEILING(size);
	stack_offset = size;
	size += stack_nelms * sizeof(void *);
	/* Avoid false cacheline sharing. */
	size = sa2u(size, CACHELINE);

	tcache = ipalloct(tsd, size, CACHELINE, true, false, arena);
	if (tcache == NULL)
		return (NULL);

	tcache_arena_associate(tcache, arena);

	assert((TCACHE_NSLOTS_SMALL_MAX & 1U) == 0);
	for (i = 0; i < nhbins; i++) {
		tcache->tbins[i].lg_fill_div = 1;
		tcache->tbins[i].avail = (void **)((uintptr_t)tcache +
		    (uintptr_t)stack_offset);
		stack_offset += tcache_bin_info[i].ncached_max * sizeof(void *);
	}

	return (tcache);
}

static void
tcache_destroy(tsd_t *tsd, tcache_t *tcache)
{
	unsigned i;

	tcache_arena_dissociate(tcache);

	for (i = 0; i < NBINS; i++) {
		tcache_bin_t *tbin = &tcache->tbins[i];
		tcache_bin_flush_small(tbin, i, 0, tcache);

		if (config_stats && tbin->tstats.nrequests != 0) {
			arena_t *arena = tcache->arena;
			arena_bin_t *bin = &arena->bins[i];
			malloc_mutex_lock(&bin->lock);
			bin->stats.nrequests += tbin->tstats.nrequests;
			malloc_mutex_unlock(&bin->lock);
		}
	}

	for (; i < nhbins; i++) {
		tcache_bin_t *tbin = &tcache->tbins[i];
		tcache_bin_flush_large(tbin, i, 0, tcache);

		if (config_stats && tbin->tstats.nrequests != 0) {
			arena_t *arena = tcache->arena;
			malloc_mutex_lock(&arena->lock);
			arena->stats.nrequests_large += tbin->tstats.nrequests;
			arena->stats.lstats[i - NBINS].nrequests +=
			    tbin->tstats.nrequests;
			malloc_mutex_unlock(&arena->lock);
		}
	}

	if (config_prof && tcache->prof_accumbytes > 0 &&
	    arena_prof_accum(tcache->arena, tcache->prof_accumbytes))
		prof_idump();

	idalloct(tsd, tcache, false);
}

void
tcache_cleanup(tsd_t *tsd)
{
	tcache_t *tcache;

	if (!config_tcache)
		return;

	if ((tcache = tsd_tcache_get(tsd)) != NULL) {
		tcache_destroy(tsd, tcache);
		tsd_tcache_set(tsd, NULL);
	}
}

void
tcache_enabled_cleanup(tsd_t *tsd)
{

	/* Do nothing. */
}

/* Caller must own arena->lock. */
void
tcache_stats_merge(tcache_t *tcache, arena_t *arena)
{
	unsigned i;

	cassert(config_stats);

	/* Merge and reset tcache stats. */
	for (i = 0; i < NBINS; i++) {
		arena_bin_t *bin = &arena->bins[i];
		tcache_bin_t *tbin = &tcache->tbins[i];
		malloc_mutex_lock(&bin->lock);
		bin->stats.nrequests += tbin->tstats.nrequests;
		malloc_mutex_unlock(&bin->lock);
		tbin->tstats.nrequests = 0;
	}

	for (; i < nhbins; i++) {
		malloc_large_stats_t *lstats = &arena->stats.lstats[i - NBINS];
		tcache_bin_t *tbin = &tcache->tbins[i];
		arena->stats.nrequests_large += tbin->tstats.nrequests;
		lstats->nrequests += tbin->tstats.nrequests;
		tbin->tstats.nrequests = 0;
	}
}

bool
tcache_boot(void)
{
	unsigned i;

	/*
	 * If necessary, clamp opt_lg_tcache_max, now that arena_maxclass is
	 * known.
	 */
	if (opt_lg_tcache_max < 0 || (1U << opt_lg_tcache_max) < SMALL_MAXCLASS)
		tcache_maxclass = SMALL_MAXCLASS;
	else if ((1U << opt_lg_tcache_max) > arena_maxclass)
		tcache_maxclass = arena_maxclass;
	else
		tcache_maxclass = (1U << opt_lg_tcache_max);

	nhbins = NBINS + (tcache_maxclass >> LG_PAGE);

	/* Initialize tcache_bin_info. */
	tcache_bin_info = (tcache_bin_info_t *)base_alloc(nhbins *
	    sizeof(tcache_bin_info_t));
	if (tcache_bin_info == NULL)
		return (true);
	stack_nelms = 0;
	for (i = 0; i < NBINS; i++) {
		if ((arena_bin_info[i].nregs << 1) <= TCACHE_NSLOTS_SMALL_MAX) {
			tcache_bin_info[i].ncached_max =
			    (arena_bin_info[i].nregs << 1);
		} else {
			tcache_bin_info[i].ncached_max =
			    TCACHE_NSLOTS_SMALL_MAX;
		}
		stack_nelms += tcache_bin_info[i].ncached_max;
	}
	for (; i < nhbins; i++) {
		tcache_bin_info[i].ncached_max = TCACHE_NSLOTS_LARGE;
		stack_nelms += tcache_bin_info[i].ncached_max;
	}

	return (false);
}
