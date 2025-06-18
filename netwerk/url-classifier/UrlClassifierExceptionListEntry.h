/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_UrlClassifierExceptionListEntry_h
#define mozilla_UrlClassifierExceptionListEntry_h

#include "mozilla/extensions/MatchPattern.h"
#include "nsIUrlClassifierExceptionListEntry.h"
#include "nsString.h"
#include "nsISupports.h"

namespace mozilla::net {

/**
 * @see nsIUrlClassifierExceptionListEntry
 */
class UrlClassifierExceptionListEntry final
    : public nsIUrlClassifierExceptionListEntry {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIURLCLASSIFIEREXCEPTIONLISTENTRY

  UrlClassifierExceptionListEntry() = default;

  UrlClassifierExceptionListEntry(
      const nsACString& aUrlPattern, const nsACString& aTopLevelUrlPattern,
      bool aIsPrivateBrowsingOnly,
      const nsTArray<nsCString>& aClassifierFeatures)
      : mUrlPattern(aUrlPattern),
        mTopLevelUrlPattern(aTopLevelUrlPattern),
        mIsPrivateBrowsingOnly(aIsPrivateBrowsingOnly) {
    mClassifierFeatures = aClassifierFeatures.Clone();
  }

 private:
  ~UrlClassifierExceptionListEntry() = default;

  nsIUrlClassifierExceptionListEntry::Category mCategory;
  nsCString mUrlPattern;
  nsCString mTopLevelUrlPattern;
  bool mIsPrivateBrowsingOnly{};
  nsTArray<nsCString> mFilterContentBlockingCategories;
  nsTArray<nsCString> mClassifierFeatures;

  RefPtr<extensions::MatchPatternCore> mMatcher;
  RefPtr<extensions::MatchPatternCore> mTopLevelMatcher;
};

}  // namespace mozilla::net

#endif
