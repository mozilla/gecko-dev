/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "BaseAlloc.h"

#include <cstring>

#include "Globals.h"

using namespace mozilla;

Mutex base_mtx;

// Current pages that are being used for internal memory allocations.  These
// pages are carved up in cacheline-size quanta, so that there is no chance of
// false cache line sharing.
static void* base_pages MOZ_GUARDED_BY(base_mtx);
static void* base_next_addr MOZ_GUARDED_BY(base_mtx);
static void* base_next_decommitted MOZ_GUARDED_BY(base_mtx);
// Address immediately past base_pages.
static void* base_past_addr MOZ_GUARDED_BY(base_mtx);
size_t base_mapped MOZ_GUARDED_BY(base_mtx);
size_t base_committed MOZ_GUARDED_BY(base_mtx);

// Initialize base allocation data structures.
void base_init() MOZ_REQUIRES(gInitLock) {
  base_mtx.Init();
  MOZ_PUSH_IGNORE_THREAD_SAFETY
  base_mapped = 0;
  base_committed = 0;
  MOZ_POP_THREAD_SAFETY
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

void* base_alloc(size_t aSize) {
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

void* base_calloc(size_t aNumber, size_t aSize) {
  void* ret = base_alloc(aNumber * aSize);
  if (ret) {
    memset(ret, 0, aNumber * aSize);
  }
  return ret;
}
