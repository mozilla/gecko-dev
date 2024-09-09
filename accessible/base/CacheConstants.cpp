/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set expandtab shiftwidth=2 tabstop=2: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CacheConstants.h"
#include "nsAccessibilityService.h"

namespace mozilla::a11y {

// Get the set of cache domains required by the given cache domains, which will
// always be equal to or a superset of the given set of cache domains.
static uint64_t GetCacheDomainSuperset(uint64_t aCacheDomains) {
  uint64_t allNecessaryDomains = aCacheDomains;
  if (aCacheDomains & CacheDomain::TextOffsetAttributes) {
    allNecessaryDomains |= CacheDomain::Text;
  }
  if (aCacheDomains & CacheDomain::TextBounds) {
    allNecessaryDomains |= CacheDomain::Text;
    allNecessaryDomains |= CacheDomain::Bounds;
  }
  MOZ_ASSERT((allNecessaryDomains & aCacheDomains) == aCacheDomains,
             "Return value is not a superset of the input.");
  return allNecessaryDomains;
}

bool DomainsAreActive(uint64_t aRequiredCacheDomains) {
  const uint64_t activeCacheDomains =
      nsAccessibilityService::GetActiveCacheDomains();
  const bool allRequiredDomainsAreActive =
      (aRequiredCacheDomains & ~activeCacheDomains) == 0;
  return allRequiredDomainsAreActive;
}

bool RequestDomainsIfInactive(uint64_t aRequiredCacheDomains) {
  nsAccessibilityService* accService = GetAccService();
  if (!accService) {
    return true;
  }
  const uint64_t activeCacheDomains =
      nsAccessibilityService::GetActiveCacheDomains();
  const bool isMissingRequiredCacheDomain =
      (aRequiredCacheDomains & ~activeCacheDomains) != 0;
  if (isMissingRequiredCacheDomain) {
    aRequiredCacheDomains = GetCacheDomainSuperset(aRequiredCacheDomains);

    const uint64_t cacheDomains = aRequiredCacheDomains | activeCacheDomains;
#if defined(ANDROID)
    // We might not be on the main Android thread, but we must be in order to
    // send IPDL messages. Dispatch to the main thread to set cache domains.
    NS_DispatchToMainThread(
        NS_NewRunnableFunction("a11y::SetCacheDomains", [cacheDomains]() {
          if (nsAccessibilityService* accService = GetAccService()) {
            accService->SetCacheDomains(cacheDomains);
          }
        }));
    return true;
#endif

    accService->SetCacheDomains(cacheDomains);
    return true;
  }
  return false;
}

}  // namespace mozilla::a11y
