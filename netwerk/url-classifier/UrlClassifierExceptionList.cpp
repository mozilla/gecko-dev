/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "UrlClassifierExceptionList.h"
#include "nsIUrlClassifierExceptionListEntry.h"
#include "nsIURI.h"
#include "mozilla/net/UrlClassifierCommon.h"

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

  nsAutoCString entryString;
  Unused << aEntry->Describe(entryString);
  UC_LOG_DEBUG(("UrlClassifierExceptionList::%s - Adding entry: %s",
                __FUNCTION__, entryString.get()));

  mEntries.AppendElement(aEntry);
  return NS_OK;
}

NS_IMETHODIMP
UrlClassifierExceptionList::Matches(nsIURI* aURI, nsIURI* aTopLevelURI,
                                    bool aIsPrivateBrowsing, bool* aResult) {
  NS_ENSURE_ARG_POINTER(aURI);
  NS_ENSURE_ARG_POINTER(aResult);

  *aResult = false;

  UC_LOG_DEBUG(
      ("UrlClassifierExceptionList::%s - aURI: %s, aTopLevelURI: %s, "
       "aIsPrivateBrowsing: %d",
       __FUNCTION__, aURI->GetSpecOrDefault().get(),
       aTopLevelURI ? aTopLevelURI->GetSpecOrDefault().get() : "null",
       aIsPrivateBrowsing));

  for (auto& entry : mEntries) {
    nsresult rv =
        entry->Matches(aURI, aTopLevelURI, aIsPrivateBrowsing, aResult);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      continue;
    }
    if (*aResult) {
      // Match found, return immediately.
      if (MOZ_LOG_TEST(UrlClassifierCommon::sLog, LogLevel::Debug)) {
        nsAutoCString entryString;
        Unused << entry->Describe(entryString);
        UC_LOG_DEBUG(
            ("UrlClassifierExceptionList::%s - Exception list match found. "
             "entry: %s",
             __FUNCTION__, entryString.get()));
      }
      return NS_OK;
    }
  }

  // No match found, return false.
  UC_LOG_DEBUG(("%s - No match found", __FUNCTION__));
  return NS_OK;
}

NS_IMETHODIMP
UrlClassifierExceptionList::TestGetEntries(
    nsTArray<RefPtr<nsIUrlClassifierExceptionListEntry>>& aEntries) {
  aEntries = mEntries.Clone();
  return NS_OK;
}
}  // namespace mozilla::net
