/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef BASEALLOC_H
#define BASEALLOC_H

#include "Mutex.h"

extern Mutex base_mtx;

extern size_t base_mapped MOZ_GUARDED_BY(base_mtx);
extern size_t base_committed MOZ_GUARDED_BY(base_mtx);

void base_init();

void* base_alloc(size_t aSize);

void* base_calloc(size_t aNumber, size_t aSize);

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

template <typename T>
T* TypedBaseAlloc<T>::sFirstFree = nullptr;

template <typename T>
struct BaseAllocFreePolicy {
  void operator()(T* aPtr) { TypedBaseAlloc<T>::dealloc(aPtr); }
};

#endif /* ! BASEALLOC_H */
