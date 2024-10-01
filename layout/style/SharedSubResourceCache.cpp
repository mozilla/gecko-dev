/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SharedSubResourceCache.h"

#include "mozilla/dom/CacheablePerformanceTimingData.h"
#include "nsCOMPtr.h"
#include "nsIHttpChannel.h"
#include "nsIRequest.h"
#include "nsITimedChannel.h"

namespace mozilla {

SubResourceNetworkMetadataHolder::SubResourceNetworkMetadataHolder(
    nsIRequest* aRequest) {
  nsCOMPtr<nsITimedChannel> timedChannel = do_QueryInterface(aRequest);
  if (timedChannel) {
    nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(aRequest);
    mPerfData.emplace(timedChannel, httpChannel);
  }
}

}  // namespace mozilla
