/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "UrlClassifierExceptionList.h"
#include "nsIEffectiveTLDService.h"
#include "nsIUrlClassifierExceptionListEntry.h"
#include "nsIURI.h"
#include "mozilla/net/UrlClassifierCommon.h"
#include "mozilla/ProfilerMarkers.h"
#include "nsNetCID.h"
#include "nsServiceManagerUtils.h"
#include "mozilla/RustRegex.h"

namespace mozilla::net {

NS_IMPL_ISUPPORTS(UrlClassifierExceptionList, nsIUrlClassifierExceptionList)

NS_IMETHODIMP
UrlClassifierExceptionList::Init(const nsACString& aFeature) {
  mFeature = aFeature;
  return NS_OK;
}

NS_IMETHODIMP
UrlClassifierExceptionList::AddEntry(
    nsIUrlClassifierExceptionListEntry* aEntry) {
  NS_ENSURE_ARG_POINTER(aEntry);

  // From the url patterns in the entry, extract the site and top level site.
  // They are used as keys in the exception entry maps.

  nsAutoCString urlPattern;
  nsresult rv = aEntry->GetUrlPattern(urlPattern);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString site;
  rv = GetSchemelessSiteFromUrlPattern(urlPattern, site);
  NS_ENSURE_SUCCESS(rv, rv);

  // We must be able to parse a site from the url pattern.
  NS_ENSURE_TRUE(!site.IsEmpty(), NS_ERROR_INVALID_ARG);

  nsAutoCString topLevelUrlPattern;
  rv = aEntry->GetTopLevelUrlPattern(topLevelUrlPattern);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString topLevelSite;
  rv = GetSchemelessSiteFromUrlPattern(topLevelUrlPattern, topLevelSite);
  NS_ENSURE_SUCCESS(rv, rv);

  // topLevelUrlPattern is not mandatory, but if topLevelUrlPattern is set,
  // topLevelSite populated as well.
  NS_ENSURE_TRUE(topLevelUrlPattern.IsEmpty() == topLevelSite.IsEmpty(),
                 NS_ERROR_INVALID_ARG);

  if (MOZ_LOG_TEST(UrlClassifierCommon::sLog, LogLevel::Debug)) {
    nsAutoCString entryString;
    Unused << aEntry->Describe(entryString);
    UC_LOG_DEBUG(("UrlClassifierExceptionList::%s - Adding entry: %s",
                  __FUNCTION__, entryString.get()));
  }

  // If the top level site is empty, the exception applies across all top
  // level sites. Store it in the global exceptions map.
  if (topLevelSite.IsEmpty()) {
    mGlobalExceptions.LookupOrInsert(site).AppendElement(aEntry);
    return NS_OK;
  }

  // Otherwise, store it in the site specific exception map.
  mExceptions
      // Outer map keyed by top level site.
      // topLevelSite may be the empty string. We still use that a key. These
      // entries apply to all top-level sites.
      .LookupOrInsert(topLevelSite)
      // Inner map keyed by site of the load.
      .LookupOrInsert(site)
      // Append the entry.
      .AppendElement(aEntry);

  return NS_OK;
}

NS_IMETHODIMP
UrlClassifierExceptionList::Matches(nsIURI* aURI, nsIURI* aTopLevelURI,
                                    bool aIsPrivateBrowsing, bool* aResult) {
  NS_ENSURE_ARG_POINTER(aURI);
  NS_ENSURE_ARG_POINTER(aResult);

  // Record how long it takes to perform the exception list lookup.
  AUTO_PROFILER_MARKER_UNTYPED("UrlClassifierExceptionList::Matches", OTHER,
                               MarkerTiming::IntervalStart());

  *aResult = false;

  UC_LOG_DEBUG(
      ("UrlClassifierExceptionList::%s - aURI: %s, aTopLevelURI: %s, "
       "aIsPrivateBrowsing: %d",
       __FUNCTION__, aURI->GetSpecOrDefault().get(),
       aTopLevelURI ? aTopLevelURI->GetSpecOrDefault().get() : "null",
       aIsPrivateBrowsing));

  // Get the eTLD service so we can compute sites from URIs.
  nsresult rv;
  nsCOMPtr<nsIEffectiveTLDService> eTLDService(
      do_GetService(NS_EFFECTIVETLDSERVICE_CONTRACTID, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  // If given, compute the (schemeless) site from the top level URI.
  // If not we will leave it empty and only look for global exceptions.
  nsAutoCString aTopLevelSite;
  if (aTopLevelURI) {
    rv = eTLDService->GetSchemelessSite(aTopLevelURI, aTopLevelSite);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Compute the (schemeless) site from the URI of the load.
  nsAutoCString aSite;
  rv = eTLDService->GetSchemelessSite(aURI, aSite);
  NS_ENSURE_SUCCESS(rv, rv);

  // Get the list of exceptions that apply to the current load.
  // We need to check both global and site specific exceptions

  // 1. Check global exceptions, which apply to all top level sites and lookup
  //    entries matching the current load (aSite).
  ExceptionEntryArray* globalExceptions =
      mGlobalExceptions.Lookup(aSite).DataPtrOrNull();

  *aResult = ExceptionListMatchesLoad(globalExceptions, aURI, aTopLevelURI,
                                      aIsPrivateBrowsing);
  if (*aResult) {
    // We found a match, no need to check the site specific exceptions.
    return NS_OK;
  }

  // 2. Get exceptions which apply only to the current top level site.
  SiteToEntries* topLevelSiteToEntries =
      mExceptions.Lookup(aTopLevelSite).DataPtrOrNull();
  if (topLevelSiteToEntries) {
    ExceptionEntryArray* siteSpecificExceptions =
        topLevelSiteToEntries->Lookup(aSite).DataPtrOrNull();

    *aResult = ExceptionListMatchesLoad(siteSpecificExceptions, aURI,
                                        aTopLevelURI, aIsPrivateBrowsing);
    if (*aResult) {
      return NS_OK;
    }
  }

  if (!(*aResult)) {
    UC_LOG_DEBUG(("%s - No match found", __FUNCTION__));
  }

  return NS_OK;
}

bool UrlClassifierExceptionList::ExceptionListMatchesLoad(
    ExceptionEntryArray* aExceptions, nsIURI* aURI, nsIURI* aTopLevelURI,
    bool aIsPrivateBrowsing) {
  MOZ_ASSERT(aURI);

  if (!aExceptions) {
    return false;
  }
  for (const auto& entry : *aExceptions) {
    bool match = false;
    nsresult rv =
        entry->Matches(aURI, aTopLevelURI, aIsPrivateBrowsing, &match);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      continue;
    }
    if (match) {
      // Match found, return immediately.
      if (MOZ_LOG_TEST(UrlClassifierCommon::sLog, LogLevel::Debug)) {
        nsAutoCString entryString;
        Unused << entry->Describe(entryString);
        UC_LOG_DEBUG(
            ("UrlClassifierExceptionList::%s - Exception list match found. "
             "entry: %s",
             __FUNCTION__, entryString.get()));
      }
      return true;
    }
  }
  return false;
}

NS_IMETHODIMP
UrlClassifierExceptionList::GetSchemelessSiteFromUrlPattern(
    const nsACString& aUrlPattern, nsACString& aSite) {
  if (aUrlPattern.IsEmpty()) {
    aSite.Truncate();
    return NS_OK;
  }

  // Extract the host portion from the url pattern. This regex only supports url
  // patterns with a host.
  mozilla::RustRegex regex("://(?:\\*\\.)?([^/*]+)");
  mozilla::RustRegexCaptures captures = regex.FindCaptures(aUrlPattern);
  NS_ENSURE_TRUE(captures.IsValid(), NS_ERROR_INVALID_ARG);

  // Get the host from the first capture group
  auto maybeMatch = captures[1];
  NS_ENSURE_TRUE(maybeMatch, NS_ERROR_INVALID_ARG);

  nsAutoCString host;
  host.Assign(Substring(aUrlPattern, maybeMatch->start,
                        maybeMatch->end - maybeMatch->start));
  NS_ENSURE_TRUE(!host.IsEmpty(), NS_ERROR_INVALID_ARG);

  // Get the eTLD service to convert host to schemeless site
  nsresult rv;
  nsCOMPtr<nsIEffectiveTLDService> eTLDService(
      do_GetService(NS_EFFECTIVETLDSERVICE_CONTRACTID, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  return eTLDService->GetSchemelessSiteFromHost(host, aSite);
}

NS_IMETHODIMP
UrlClassifierExceptionList::TestGetEntries(
    nsTArray<RefPtr<nsIUrlClassifierExceptionListEntry>>& aEntries) {
  // Global entries (not top-level specific)
  for (const auto& entry : mGlobalExceptions) {
    const ExceptionEntryArray& entries = entry.GetData();
    aEntries.AppendElements(entries);
  }

  // Site specific entries.
  // Iterate through the outer map (top-level sites)
  for (const auto& outerEntry : mExceptions) {
    const SiteToEntries& innerMap = outerEntry.GetData();

    // Iterate through the inner map (sites to exception entries)
    for (const auto& innerEntry : innerMap) {
      const ExceptionEntryArray& entries = innerEntry.GetData();
      // Append all entries from this array to the result
      aEntries.AppendElements(entries);
    }
  }

  return NS_OK;
}
}  // namespace mozilla::net
