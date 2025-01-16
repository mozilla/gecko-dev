/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_Queue_h
#define mozilla_Queue_h

#include <utility>
#include <stdint.h>
#include "mozilla/MemoryReporting.h"
#include "mozilla/Assertions.h"
#include "mozalloc.h"

namespace mozilla {

// A queue implements a singly linked list of pages, each of which contains some
// number of elements. Since the queue needs to store a "next" pointer, the
// actual number of elements per page won't be quite as many as were requested.
//
// Each page consists of N entries.  We use the head buffer as a circular buffer
// if it's the only buffer; if we have more than one buffer when the head is
// empty we release it.  This avoids occasional freeing and reallocating buffers
// every N entries.  We'll still allocate and free every N if the normal queue
// depth is greated than N.  A fancier solution would be to move an empty Head
// buffer to be an empty tail buffer, freeing if we have multiple empty tails,
// but that probably isn't worth it.
//
// Cases:
//   a) single buffer, circular
//      Push: if not full:
//              Add to tail, increase count
//            full:
//              Add new page, insert there and increase count.
//      Pop:
//            take entry, bump head and decrease count
//   b) multiple buffers:
//      Push: if not full:
//              Add to tail, increase count
//            full:
//              Add new page, insert there and increase count.
//      Pop:
//            take entry, bump head and decrease count
//            if buffer is empty, free head buffer and promote next to head
//
template <class T, size_t RequestedItemsPerPage = 256>
class Queue {
 public:
  Queue() = default;

  Queue(Queue&& aOther) noexcept
      : mHead(std::exchange(aOther.mHead, nullptr)),
        mTail(std::exchange(aOther.mTail, nullptr)),
        mCount(std::exchange(aOther.mCount, 0)),
        mOffsetHead(std::exchange(aOther.mOffsetHead, 0)),
        mHeadLength(std::exchange(aOther.mHeadLength, 0)) {}

  Queue& operator=(Queue&& aOther) noexcept {
    Clear();

    mHead = std::exchange(aOther.mHead, nullptr);
    mTail = std::exchange(aOther.mTail, nullptr);
    mCount = std::exchange(aOther.mCount, 0);
    mOffsetHead = std::exchange(aOther.mOffsetHead, 0);
    mHeadLength = std::exchange(aOther.mHeadLength, 0);
    return *this;
  }

  ~Queue() { Clear(); }

  // Discard all elements form the queue, clearing it to be empty.
  void Clear() {
    while (!IsEmpty()) {
      Pop();
    }
    if (mHead) {
      free(mHead);
      mHead = nullptr;
    }
  }

  T& Push(T&& aElement) {
    MOZ_ASSERT(mCount < std::numeric_limits<uint32_t>::max());

    if (!mHead) {
      // First page
      mHead = NewPage();
      MOZ_ASSERT(mHead);

      mTail = mHead;
      T* eltPtr = &mTail->mEvents[0];
      new (eltPtr) T(std::move(aElement));
      mOffsetHead = 0;
      mCount = 1;
      mHeadLength = 1;
      return *eltPtr;
    }
    if (mHead == mTail && mCount < ItemsPerPage) {
      // Single buffer, circular
      uint16_t offsetTail = (mOffsetHead + mCount) % ItemsPerPage;
      T* eltPtr = &mHead->mEvents[offsetTail];
      new (eltPtr) T(std::move(aElement));
      ++mCount;
      ++mHeadLength;
      MOZ_ASSERT(mCount == mHeadLength);
      return *eltPtr;
    }

    // Multiple buffers
    uint16_t offsetTail = (mCount - mHeadLength) % ItemsPerPage;
    if (offsetTail == 0) {
      // Tail buffer is full
      Page* page = NewPage();
      MOZ_ASSERT(page);

      mTail->mNext = page;
      mTail = page;
      T* eltPtr = &page->mEvents[0];
      new (eltPtr) T(std::move(aElement));
      ++mCount;
      return *eltPtr;
    }

    MOZ_ASSERT(mHead != mTail, "can't have a non-circular single buffer");
    T* eltPtr = &mTail->mEvents[offsetTail];
    new (eltPtr) T(std::move(aElement));
    ++mCount;
    return *eltPtr;
  }

  bool IsEmpty() const { return !mCount; }

  T Pop() {
    MOZ_ASSERT(!IsEmpty());

    T result = std::move(mHead->mEvents[mOffsetHead]);
    mHead->mEvents[mOffsetHead].~T();
    // Could be circular buffer, or not.
    mOffsetHead = (mOffsetHead + 1) % ItemsPerPage;
    mCount -= 1;
    mHeadLength -= 1;

    // Check if the head page is empty and we have more pages.
    if (mHead != mTail && mHeadLength == 0) {
      Page* dead = mHead;
      mHead = mHead->mNext;
      free(dead);
      // Non-circular buffer
      mOffsetHead = 0;
      mHeadLength =
          static_cast<uint16_t>(std::min<uint32_t>(mCount, ItemsPerPage));
      // if there are still >1 pages, the new head is full.
    }

    return result;
  }

  T& FirstElement() {
    MOZ_ASSERT(!IsEmpty());
    return mHead->mEvents[mOffsetHead];
  }

  const T& FirstElement() const {
    MOZ_ASSERT(!IsEmpty());
    return mHead->mEvents[mOffsetHead];
  }

  size_t Count() const { return mCount; }

  size_t ShallowSizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const {
    size_t n = 0;
    if (mHead) {
      for (Page* page = mHead; page != mTail; page = page->mNext) {
        n += aMallocSizeOf(page);
      }
    }
    return n;
  }

  size_t ShallowSizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const {
    return aMallocSizeOf(this) + ShallowSizeOfExcludingThis(aMallocSizeOf);
  }

 private:
  static_assert(
      (RequestedItemsPerPage & (RequestedItemsPerPage - 1)) == 0,
      "RequestedItemsPerPage should be a power of two to avoid heap slop.");

  // Since a Page must also contain a "next" pointer, we use one of the items to
  // store this pointer. If sizeof(T) > sizeof(Page*), then some space will be
  // wasted. So be it.
  static const size_t ItemsPerPage = RequestedItemsPerPage - 1;

  // Page objects are linked together to form a simple deque.
  struct Page {
    struct Page* mNext;
    T mEvents[ItemsPerPage];
  };

  static Page* NewPage() {
    return static_cast<Page*>(moz_xcalloc(1, sizeof(Page)));
  }

  Page* mHead = nullptr;
  Page* mTail = nullptr;

  uint32_t mCount = 0;       // Number of items in the queue
  uint16_t mOffsetHead = 0;  // Read position in head page
  uint16_t mHeadLength = 0;  // Number of items in the circular head page
};

}  // namespace mozilla

#endif  // mozilla_Queue_h
