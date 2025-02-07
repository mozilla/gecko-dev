/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsSegmentedBuffer.h"
#include "nsNetCID.h"
#include "nsServiceManagerUtils.h"
#include "nsThreadUtils.h"
#include "mozilla/ScopeExit.h"

static constexpr uint32_t kSegmentedBufferFreeOMTThreshold = 128;

nsresult nsSegmentedBuffer::Init(uint32_t aSegmentSize) {
  if (mSegmentArrayCount != 0) {
    return NS_ERROR_FAILURE;  // initialized more than once
  }
  mSegmentSize = aSegmentSize;
  mSegmentArrayCount = NS_SEGMENTARRAY_INITIAL_COUNT;
  return NS_OK;
}

char* nsSegmentedBuffer::AppendNewSegment(
    mozilla::UniqueFreePtr<char> aSegment) {
  if (!mSegmentArray) {
    uint32_t bytes = mSegmentArrayCount * sizeof(char*);
    mSegmentArray = (char**)moz_xmalloc(bytes);
    memset(mSegmentArray, 0, bytes);
  }

  if (IsFull()) {
    mozilla::CheckedInt<uint32_t> newArraySize =
        mozilla::CheckedInt<uint32_t>(mSegmentArrayCount) * 2;
    mozilla::CheckedInt<uint32_t> bytes = newArraySize * sizeof(char*);
    if (!bytes.isValid()) {
      return nullptr;
    }

    mSegmentArray = (char**)moz_xrealloc(mSegmentArray, bytes.value());
    // copy wrapped content to new extension
    if (mFirstSegmentIndex > mLastSegmentIndex) {
      // deal with wrap around case
      memcpy(&mSegmentArray[mSegmentArrayCount], mSegmentArray,
             mLastSegmentIndex * sizeof(char*));
      memset(mSegmentArray, 0, mLastSegmentIndex * sizeof(char*));
      mLastSegmentIndex += mSegmentArrayCount;
      memset(&mSegmentArray[mLastSegmentIndex], 0,
             (newArraySize.value() - mLastSegmentIndex) * sizeof(char*));
    } else {
      memset(&mSegmentArray[mLastSegmentIndex], 0,
             (newArraySize.value() - mLastSegmentIndex) * sizeof(char*));
    }
    mSegmentArrayCount = newArraySize.value();
  }

  char* seg = aSegment ? aSegment.release() : (char*)malloc(mSegmentSize);
  if (!seg) {
    return nullptr;
  }
  mSegmentArray[mLastSegmentIndex] = seg;
  mLastSegmentIndex = ModSegArraySize(mLastSegmentIndex + 1);
  return seg;
}

mozilla::UniqueFreePtr<char> nsSegmentedBuffer::PopFirstSegment() {
  NS_ASSERTION(mSegmentArray[mFirstSegmentIndex] != nullptr,
               "deleting bad segment");
  mozilla::UniqueFreePtr<char> segment(mSegmentArray[mFirstSegmentIndex]);
  mSegmentArray[mFirstSegmentIndex] = nullptr;
  int32_t last = ModSegArraySize(mLastSegmentIndex - 1);
  if (mFirstSegmentIndex == last) {
    mLastSegmentIndex = last;
  } else {
    mFirstSegmentIndex = ModSegArraySize(mFirstSegmentIndex + 1);
  }
  return segment;
}

mozilla::UniqueFreePtr<char> nsSegmentedBuffer::PopLastSegment() {
  int32_t last = ModSegArraySize(mLastSegmentIndex - 1);
  NS_ASSERTION(mSegmentArray[last] != nullptr, "deleting bad segment");
  mozilla::UniqueFreePtr<char> segment(mSegmentArray[last]);
  mSegmentArray[last] = nullptr;
  mLastSegmentIndex = last;
  return segment;
}

bool nsSegmentedBuffer::ReallocLastSegment(size_t aNewSize) {
  int32_t last = ModSegArraySize(mLastSegmentIndex - 1);
  NS_ASSERTION(mSegmentArray[last] != nullptr, "realloc'ing bad segment");
  char* newSegment = (char*)realloc(mSegmentArray[last], aNewSize);
  if (newSegment) {
    mSegmentArray[last] = newSegment;
    return true;
  }
  return false;
}

void nsSegmentedBuffer::Clear() {
  // Clear out the buffer's members back to their initial state.
  uint32_t arrayCount =
      std::exchange(mSegmentArrayCount, NS_SEGMENTARRAY_INITIAL_COUNT);
  char** segmentArray = std::exchange(mSegmentArray, nullptr);
  mFirstSegmentIndex = mLastSegmentIndex = 0;

  auto freeSegmentArray = [arrayCount, segmentArray]() {
    for (uint32_t i = 0; i < arrayCount; ++i) {
      if (segmentArray[i]) {
        free(segmentArray[i]);
      }
    }
    free(segmentArray);
  };

  if (segmentArray) {
    // If we have a small number of entries, free them synchronously. In some
    // rare cases, `nsSegmentedBuffer` may be gigantic, in which case it should
    // be freed async in a background task to avoid janking this thread.
    if (arrayCount < kSegmentedBufferFreeOMTThreshold ||
        NS_FAILED(NS_DispatchBackgroundTask(NS_NewRunnableFunction(
            "nsSegmentedBuffer::Clear", freeSegmentArray)))) {
      freeSegmentArray();
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
