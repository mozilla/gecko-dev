/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef KeyedStackCapturer_h__
#define KeyedStackCapturer_h__

#include "CombinedStacks.h"
#include "mozilla/Mutex.h"
#include "mozilla/Telemetry.h"
#include "nsClassHashtable.h"
#include "nsString.h"

struct JSContext;

namespace mozilla {
namespace Telemetry {

/**
 * Allows taking a snapshot of a call stack on demand. Captured stacks are
 * indexed by a string key in a hash table. The stack is only captured Once
 * for each key. Consequent captures with the same key result in incrementing
 * capture counter without re-capturing the stack.
 */
class KeyedStackCapturer {
 public:
  KeyedStackCapturer() : mStackCapturerMutex("Telemetry::StackCapturerMutex") {}

  /**
   * Captures a stack for the given key.
   */
  void Capture(const nsACString& aKey);

  /**
   * Transforms captured stacks into a JS object.
   */
  NS_IMETHODIMP ReflectCapturedStacks(JSContext* cx,
                                      JS::MutableHandle<JS::Value> ret);

  /**
   * Resets captured stacks and the information related to them.
   */
  void Clear();

  size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

 private:
  /**
   * Describes how often a stack was captured.
   */
  struct StackFrequencyInfo {
    // A number of times the stack was captured.
    uint32_t mCount;
    // Index of the stack inside stacks array.
    uint32_t mIndex;

    StackFrequencyInfo(uint32_t aCount, uint32_t aIndex)
        : mCount(aCount), mIndex(aIndex) {}
  };

  typedef nsClassHashtable<nsCStringHashKey, StackFrequencyInfo>
      FrequencyInfoMapType;

  FrequencyInfoMapType mStackInfos;
  CombinedStacks mStacks;
  Mutex mStackCapturerMutex;
};

}  // namespace Telemetry
}  // namespace mozilla

#endif  // KeyedStackCapturer_h__
