/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PerformanceInteractionMetrics_h__
#define mozilla_dom_PerformanceInteractionMetrics_h__

#include "nsHashtablesFwd.h"
#include "PerformanceEventTiming.h"

namespace mozilla::dom {

class PerformanceInteractionMetrics final {
 public:
  PerformanceInteractionMetrics();

  PerformanceInteractionMetrics(const PerformanceInteractionMetrics& aCopy) =
      delete;
  PerformanceInteractionMetrics& operator=(
      const PerformanceInteractionMetrics& aCopy) = delete;

  Maybe<uint64_t> ComputeInteractionId(PerformanceEventTiming* aEventTiming,
                                       const WidgetEvent* aEvent);

  nsTHashMap<uint32_t, RefPtr<PerformanceEventTiming>>& PendingKeyDowns() {
    return mPendingKeyDowns;
  }
  nsTHashMap<uint32_t, RefPtr<PerformanceEventTiming>>& PendingPointerDowns() {
    return mPendingPointerDowns;
  }

  uint64_t InteractionCount() { return mInteractionCount; }

  uint64_t IncreaseInteractionValueAndCount();

  virtual ~PerformanceInteractionMetrics() = default;

 private:
  // A map of integers to PerformanceEventTimings which is initially empty.
  // https://w3c.github.io/event-timing/#pending-key-downs
  nsTHashMap<uint32_t, RefPtr<PerformanceEventTiming>> mPendingKeyDowns;

  // A map of integers to PerformanceEventTimings which is initially empty.
  // https://w3c.github.io/event-timing/#pending-pointer-downs
  nsTHashMap<uint32_t, RefPtr<PerformanceEventTiming>> mPendingPointerDowns;

  // https://w3c.github.io/event-timing/#pointer-interaction-value-map
  nsTHashMap<uint32_t, uint32_t> mPointerInteractionValueMap;

  // An integer which counts the total number of distinct user interactions,
  // for which there was a unique interactionId computed via computing
  // interactionId.
  // https://w3c.github.io/event-timing/#window-interactioncount
  uint64_t mInteractionCount = 0;

  uint64_t mCurrentInteractionValue;

  Maybe<uint64_t> mLastKeydownInteractionValue;

  bool mContextMenuTriggered = false;
};

inline void ImplCycleCollectionTraverse(
    nsCycleCollectionTraversalCallback& aCallback,
    PerformanceInteractionMetrics& aMetrics, const char* aName,
    uint32_t aFlags = 0) {
  ImplCycleCollectionTraverse(aCallback, aMetrics.PendingKeyDowns(), aName,
                              aFlags);
  ImplCycleCollectionTraverse(aCallback, aMetrics.PendingPointerDowns(), aName,
                              aFlags);
}

inline void ImplCycleCollectionUnlink(PerformanceInteractionMetrics& aMetrics) {
  aMetrics.PendingKeyDowns().Clear();
  aMetrics.PendingPointerDowns().Clear();
}

}  // namespace mozilla::dom

#endif  // mozilla_dom_PerformanceInteractionMetrics_h__
