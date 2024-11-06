/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Attributes.h"

#include "gc/GCLock.h"
#include "jsapi-tests/tests.h"

#include "gc/ArenaList-inl.h"
#include "gc/Heap-inl.h"

using namespace js;
using namespace js::gc;

// Automatically allocate and free an Arena for testing purposes.
class MOZ_RAII AutoTestArena {
  Arena* arena = nullptr;

 public:
  explicit AutoTestArena(JSContext* cx, AllocKind kind, size_t nfree) {
    // For testing purposes only. Don't do this in real code!
    arena = js_pod_calloc<Arena>(1);
    MOZ_RELEASE_ASSERT(arena);

    {
      AutoLockGC lock(cx->runtime());
      arena->init(&cx->runtime()->gc, cx->zone(), kind, lock);
    }

    size_t nallocs = Arena::thingsPerArena(kind) - nfree;
    size_t thingSize = Arena::thingSize(kind);
    for (size_t i = 0; i < nallocs; i++) {
      MOZ_RELEASE_ASSERT(arena->getFirstFreeSpan()->allocate(thingSize));
    }
    MOZ_RELEASE_ASSERT(arena->countFreeCells() == nfree);
  }

  ~AutoTestArena() { js_free(arena); }

  Arena* get() { return arena; }
  operator Arena*() { return arena; }
  Arena* operator->() { return arena; }
};

BEGIN_TEST(testSortedArenaList) {
  const AllocKind kind = AllocKind::OBJECT0;

  // Test empty list.

  SortedArenaList sortedList(kind);
  CHECK(sortedList.thingsPerArena() == Arena::thingsPerArena(kind));

  CHECK(ConvertToArenaList(kind, sortedList, 0));

  // Test with single non-empty arena.

  size_t thingsPerArena = Arena::thingsPerArena(kind);
  for (size_t i = 0; i != thingsPerArena; i++) {
    AutoTestArena arena(cx, kind, i);
    sortedList.insertAt(arena.get(), i);

    CHECK(ConvertToArenaList(kind, sortedList, 1, nullptr, arena.get()));
  }

  // Test with single empty arena.

  AutoTestArena arena(cx, kind, thingsPerArena);
  sortedList.insertAt(arena.get(), thingsPerArena);

  CHECK(ConvertToArenaList(kind, sortedList, 0, arena.get()));

  // Test with full and non-full arenas.

  AutoTestArena fullArena(cx, kind, 0);
  AutoTestArena nonFullArena(cx, kind, 1);
  sortedList.insertAt(fullArena.get(), 0);
  sortedList.insertAt(nonFullArena.get(), 1);

  CHECK(ConvertToArenaList(kind, sortedList, 2, nullptr, nonFullArena.get(),
                           fullArena.get()));

  return true;
}

bool ConvertToArenaList(AllocKind kind, SortedArenaList& sortedList,
                        size_t expectedBucketCount,
                        Arena* expectedEmpty = nullptr,
                        Arena* expectedFirst = nullptr,
                        Arena* expectedLast = nullptr) {
  CHECK(ConvertToArenaListOnce(sortedList, expectedBucketCount, expectedEmpty,
                               expectedFirst, expectedLast));

  // Check again to test restoreFromArenaList restored the original state
  // (except for the empty arenas which are not restored).
  CHECK(ConvertToArenaListOnce(sortedList, expectedBucketCount, nullptr,
                               expectedFirst, expectedLast));

  // Clear the list on exit.
  new (&sortedList) SortedArenaList(kind);

  return true;
}

bool ConvertToArenaListOnce(SortedArenaList& sortedList,
                            size_t expectedBucketCount, Arena* expectedEmpty,
                            Arena* expectedFirst, Arena* expectedLast) {
  Arena* emptyArenas = nullptr;
  sortedList.extractEmptyTo(&emptyArenas);
  CHECK(emptyArenas == expectedEmpty);

  Arena* bucketLast[SortedArenaList::BucketCount];
  ArenaList list = sortedList.convertToArenaList(bucketLast);
  CHECK(list.isEmpty() == (expectedBucketCount == 0));
  if (expectedFirst) {
    CHECK(list.first() == expectedFirst);
  }
  if (expectedLast) {
    CHECK(list.last() == expectedLast);
  }

  size_t count = 0;
  for (size_t j = 0; j < SortedArenaList::BucketCount; j++) {
    if (bucketLast[j]) {
      count++;
    }
  }
  CHECK(count == expectedBucketCount);

  sortedList.restoreFromArenaList(list, bucketLast);
  CHECK(list.isEmpty());

  return true;
}
END_TEST(testSortedArenaList)
