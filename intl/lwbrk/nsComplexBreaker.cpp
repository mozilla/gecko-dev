/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsComplexBreaker.h"

#include <algorithm>

#include "LineBreakCache.h"
#include "MainThreadUtils.h"
#include "mozilla/Assertions.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/UniquePtr.h"
#include "nsTHashMap.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsString.h"
#include "nsTArray.h"
#include "nsThreadUtils.h"

using namespace mozilla;
using namespace mozilla::intl;

void ComplexBreaker::GetBreaks(const char16_t* aText, uint32_t aLength,
                               uint8_t* aBreakBefore) {
  // It is believed that this is only called on the main thread, so we don't
  // need to lock the caching structures. A diagnostic assert is used in case
  // our tests don't exercise all code paths.
  MOZ_DIAGNOSTIC_ASSERT(NS_IsMainThread());

  MOZ_ASSERT(aText, "aText shouldn't be null");
  MOZ_ASSERT(aLength, "aLength shouldn't be zero");
  MOZ_ASSERT(aBreakBefore, "aBreakBefore shouldn't be null");

  // Check the cache.
  LineBreakCache::KeyType key{aText, aLength};
  auto entry = LineBreakCache::Cache()->Lookup(key);
  if (entry) {
    auto& breakBefore = entry.Data().mBreaks;
    LineBreakCache::CopyAndFill(breakBefore, aBreakBefore,
                                aBreakBefore + aLength);
    return;
  }

  NS_GetComplexLineBreaks(aText, aLength, aBreakBefore);

  // As a very simple memory saving measure we trim off trailing elements that
  // are false before caching.
  auto* afterLastTrue = aBreakBefore + aLength;
  while (!*(afterLastTrue - 1)) {
    if (--afterLastTrue == aBreakBefore) {
      break;
    }
  }

  entry.Set(LineBreakCache::EntryType{
      nsString(aText, aLength),
      nsTArray<uint8_t>(aBreakBefore, afterLastTrue - aBreakBefore)});
}
