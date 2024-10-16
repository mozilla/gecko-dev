/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "LineBreakCache.h"

#include "nsIObserverService.h"
#include "mozilla/Services.h"

using namespace mozilla;
using namespace mozilla::intl;

StaticAutoPtr<LineBreakCache> LineBreakCache::sBreakCache;

void LineBreakCache::Initialize() {
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  if (obs) {
    obs->AddObserver(new LineBreakCache::Observer(), "memory-pressure", false);
  }
}

void LineBreakCache::Shutdown() {
  MOZ_ASSERT(NS_IsMainThread());
  sBreakCache = nullptr;
}

NS_IMPL_ISUPPORTS(LineBreakCache::Observer, nsIObserver)

NS_IMETHODIMP LineBreakCache::Observer::Observe(nsISupports* aSubject,
                                                const char* aTopic,
                                                const char16_t* aData) {
  MOZ_ASSERT(NS_IsMainThread());

  if (strcmp(aTopic, "memory-pressure") == 0) {
    // We don't delete the cache itself, as it would almost certainly just
    // get immediately re-created; just clear its contents.
    if (LineBreakCache::sBreakCache) {
      LineBreakCache::sBreakCache->Clear();
    }
  }

  return NS_OK;
}
