/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Maybe.h"
#include "mozilla/PageloadEvent.h"
#include "mozilla/RandomNum.h"
#include "mozilla/glean/DomMetrics.h"

namespace mozilla::performance::pageload_event {

// We don't want to record an event for every page load, so instead we
// randomly sample the events based on the channel.
#ifdef NIGHTLY_BUILD
static constexpr uint64_t kNormalSamplingInterval = 1;  // Every pageload.
#else
static constexpr uint64_t kNormalSamplingInterval = 10;  // Every 10 pageloads.
#endif

PageloadEventType GetPageloadEventType() {
  Maybe<uint64_t> rand = mozilla::RandomUint64();
  if (rand.isSome()) {
    uint64_t result =
        static_cast<uint64_t>(rand.value() % kNormalSamplingInterval);
    if (result == 0) {
      return PageloadEventType::kNormal;
    }
  }
  return PageloadEventType::kNone;
}

void PageloadEventData::SetDocumentFeature(DocumentFeature aFeature) {
  uint32_t value = 0;
  if (documentFeatures.isSome()) {
    value = documentFeatures.value();
  }
  value |= aFeature;
  documentFeatures = mozilla::Some(value);
}

void PageloadEventData::SetUserFeature(UserFeature aFeature) {
  uint32_t value = 0;
  if (userFeatures.isSome()) {
    value = userFeatures.value();
  }
  value |= aFeature;
  userFeatures = mozilla::Some(value);
}

mozilla::glean::perf::PageLoadExtra PageloadEventData::ToPageLoadExtra() const {
  mozilla::glean::perf::PageLoadExtra out;

#define COPY_METRIC(name, type) out.name = this->name;
  FOR_EACH_PAGELOAD_METRIC(COPY_METRIC)
#undef COPY_METRIC
  return out;
}

}  // namespace mozilla::performance::pageload_event
