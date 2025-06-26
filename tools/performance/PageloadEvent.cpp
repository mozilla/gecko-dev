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
//
// For nightly, 10% of page loads will be sent as page_load_etld pings, and
// all other page loads will be sent using the default page_load ping.
//
// For release and beta, only 0.1% of page loads will be sent as page_load_etld
// pings, and 10% of the other page loads will be sent using the default ping.
#ifdef NIGHTLY_BUILD
static constexpr uint64_t kNormalSamplingInterval = 1;   // Every pageload.
static constexpr uint64_t kDomainSamplingInterval = 10;  // Every 10 pageloads.
#else
static constexpr uint64_t kNormalSamplingInterval = 10;  // Every 10 pageloads.
static constexpr uint64_t kDomainSamplingInterval =
    1000;  // Every 1000 pageloads.
#endif

PageloadEventType GetPageloadEventType() {
  static_assert(kDomainSamplingInterval >= kNormalSamplingInterval,
                "kDomainSamplingInterval should always be higher than "
                "kNormalSamplingInterval");

  Maybe<uint64_t> rand = mozilla::RandomUint64();
  if (rand.isSome()) {
    uint64_t result =
        static_cast<uint64_t>(rand.value() % kDomainSamplingInterval);
    if (result == 0) {
      return PageloadEventType::kDomain;
    }
    result = static_cast<uint64_t>(rand.value() % kNormalSamplingInterval);
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

bool PageloadEventData::MaybeSetDomain(nsCString& aDomain) {
  if (aDomain.IsEmpty()) {
    return false;
  }

  mDomain = mozilla::Some(aDomain);
  return true;
}

mozilla::glean::perf::PageLoadExtra PageloadEventData::ToPageLoadExtra() const {
  mozilla::glean::perf::PageLoadExtra out;

#define COPY_METRIC(name, type) out.name = this->name;
  FOR_EACH_PAGELOAD_METRIC(COPY_METRIC)
#undef COPY_METRIC
  return out;
}

mozilla::glean::perf::PageLoadDomainExtra
PageloadEventData::ToPageLoadDomainExtra() const {
  mozilla::glean::perf::PageLoadDomainExtra out;
  out.domain = this->mDomain;
  out.httpVer = this->httpVer;
  out.sameOriginNav = this->sameOriginNav;
  out.documentFeatures = this->documentFeatures;
  out.loadType = this->loadType;
  out.lcpTime = this->lcpTime;
  return out;
}

}  // namespace mozilla::performance::pageload_event
