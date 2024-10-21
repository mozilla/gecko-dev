/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SharedSubResourceCache.h"

#include "mozilla/RefPtr.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/dom/CacheablePerformanceTimingData.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/Performance.h"
#include "mozilla/dom/PerformanceResourceTimingBinding.h"
#include "mozilla/dom/PerformanceStorage.h"
#include "mozilla/dom/PerformanceTiming.h"
#include "mozilla/net/HttpBaseChannel.h"
#include "nsCOMPtr.h"
#include "nsDOMNavigationTiming.h"
#include "nsIHttpChannel.h"
#include "nsIRequest.h"
#include "nsITimedChannel.h"
#include "nsPIDOMWindow.h"
#include "nsQueryObject.h"

namespace mozilla {

namespace SharedSubResourceCacheUtils {

void AddPerformanceEntryForCache(
    const nsString& aEntryName, const nsString& aInitiatorType,
    const SubResourceNetworkMetadataHolder* aNetworkMetadata,
    TimeStamp aStartTime, TimeStamp aEndTime, dom::Document* aDocument) {
  MOZ_ASSERT(aDocument);

  if (!aNetworkMetadata || !aNetworkMetadata->GetPerfData()) {
    return;
  }

  nsPIDOMWindowInner* win = aDocument->GetInnerWindow();
  if (!win) {
    return;
  }
  RefPtr<dom::Performance> performance = win->GetPerformance();
  if (!performance) {
    return;
  }

  // TODO: Bug 1751383.
  auto renderBlocking = dom::RenderBlockingStatusType::Non_blocking;

  UniquePtr<dom::PerformanceTimingData> data(
      dom::PerformanceTimingData::Create(*aNetworkMetadata->GetPerfData(), 0,
                                         aStartTime, aEndTime, renderBlocking));
  if (!data) {
    return;
  }

  dom::PerformanceStorage* storage = performance->AsPerformanceStorage();
  MOZ_ASSERT(storage);
  storage->AddEntry(aEntryName, aInitiatorType, std::move(data));
}

}  // namespace SharedSubResourceCacheUtils

SubResourceNetworkMetadataHolder::SubResourceNetworkMetadataHolder(
    nsIRequest* aRequest) {
  nsCOMPtr<nsITimedChannel> timedChannel = do_QueryInterface(aRequest);
  if (timedChannel) {
    nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(aRequest);
    mPerfData.emplace(timedChannel, httpChannel);
  }

  RefPtr<net::HttpBaseChannel> httpBaseChannel = do_QueryObject(aRequest);
  if (httpBaseChannel) {
    mResponseHead = httpBaseChannel->MaybeCloneResponseHeadForCachedResource();
  }
}

}  // namespace mozilla
