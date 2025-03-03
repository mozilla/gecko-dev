/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CrossProcessSemaphore.h"
#include "mozilla/Unused.h"
#include "nsDebug.h"
#include "nsISupportsImpl.h"
#include <errno.h>

static const uint64_t kNsPerMs = 1000000;
static const uint64_t kNsPerSec = 1000000000;

namespace {

struct SemaphoreData {
  sem_t mSemaphore;
  mozilla::Atomic<int32_t> mRefCount;
  uint32_t mInitialValue;
};

}  // namespace

namespace mozilla {

/* static */
CrossProcessSemaphore* CrossProcessSemaphore::Create(const char*,
                                                     uint32_t aInitialValue) {
  auto handle = ipc::shared_memory::Create(sizeof(SemaphoreData));
  if (!handle) {
    return nullptr;
  }

  auto mapping = handle.Map();
  if (!mapping) {
    return nullptr;
  }

  SemaphoreData* data = mapping.DataAs<SemaphoreData>();

  if (!data) {
    return nullptr;
  }

  if (sem_init(&data->mSemaphore, 1, aInitialValue)) {
    return nullptr;
  }

  CrossProcessSemaphore* sem = new CrossProcessSemaphore;
  sem->mHandle = std::move(handle);
  sem->mSharedBuffer = std::move(mapping);
  sem->mSemaphore = &data->mSemaphore;
  sem->mRefCount = &data->mRefCount;
  *sem->mRefCount = 1;

  data->mInitialValue = aInitialValue;

  return sem;
}

/* static */
CrossProcessSemaphore* CrossProcessSemaphore::Create(
    CrossProcessSemaphoreHandle aHandle) {
  auto mapping = aHandle.Map();
  if (!mapping) {
    return nullptr;
  }

  aHandle = nullptr;

  SemaphoreData* data = mapping.DataAs<SemaphoreData>();

  if (!data) {
    return nullptr;
  }

  int32_t oldCount = data->mRefCount++;
  if (oldCount == 0) {
    // The other side has already let go of their CrossProcessSemaphore, so now
    // mSemaphore is garbage. We need to re-initialize it.
    if (sem_init(&data->mSemaphore, 1, data->mInitialValue)) {
      data->mRefCount--;
      return nullptr;
    }
  }

  CrossProcessSemaphore* sem = new CrossProcessSemaphore;
  sem->mSharedBuffer = std::move(mapping);
  sem->mSemaphore = &data->mSemaphore;
  sem->mRefCount = &data->mRefCount;
  return sem;
}

CrossProcessSemaphore::CrossProcessSemaphore()
    : mSemaphore(nullptr), mRefCount(nullptr) {
  MOZ_COUNT_CTOR(CrossProcessSemaphore);
}

CrossProcessSemaphore::~CrossProcessSemaphore() {
  int32_t oldCount = --(*mRefCount);

  if (oldCount == 0) {
    // Nothing can be done if the destroy fails so ignore return code.
    Unused << sem_destroy(mSemaphore);
  }

  MOZ_COUNT_DTOR(CrossProcessSemaphore);
}

bool CrossProcessSemaphore::Wait(const Maybe<TimeDuration>& aWaitTime) {
  MOZ_ASSERT(*mRefCount > 0,
             "Attempting to wait on a semaphore with zero ref count");
  int ret;
  if (aWaitTime.isSome()) {
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) == -1) {
      return false;
    }

    uint64_t ns = uint64_t(kNsPerMs * aWaitTime->ToMilliseconds()) + ts.tv_nsec;
    ts.tv_sec += ns / kNsPerSec;
    ts.tv_nsec = ns % kNsPerSec;

    while ((ret = sem_timedwait(mSemaphore, &ts)) == -1 && errno == EINTR) {
    }
  } else {
    while ((ret = sem_wait(mSemaphore)) == -1 && errno == EINTR) {
    }
  }
  return ret == 0;
}

void CrossProcessSemaphore::Signal() {
  MOZ_ASSERT(*mRefCount > 0,
             "Attempting to signal a semaphore with zero ref count");
  sem_post(mSemaphore);
}

CrossProcessSemaphoreHandle CrossProcessSemaphore::CloneHandle() {
  if (mSharedBuffer) {
    auto handle = mHandle.Clone();
    if (!handle) {
      MOZ_CRASH();
    }
    return handle;
  }
  return nullptr;
}

void CrossProcessSemaphore::CloseHandle() { mHandle = nullptr; }

}  // namespace mozilla
