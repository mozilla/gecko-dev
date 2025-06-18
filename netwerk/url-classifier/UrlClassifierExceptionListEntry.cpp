/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "UrlClassifierExceptionListEntry.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/StaticPrefs_privacy.h"

namespace mozilla::net {

NS_IMPL_ISUPPORTS(UrlClassifierExceptionListEntry,
                  nsIUrlClassifierExceptionListEntry)

NS_IMETHODIMP
UrlClassifierExceptionListEntry::Init(
    nsIUrlClassifierExceptionListEntry::Category aCategory,
    const nsACString& aUrlPattern, const nsACString& aTopLevelUrlPattern,
    bool aIsPrivateBrowsingOnly,
    const nsTArray<nsCString>& aFilterContentBlockingCategories,
    const nsTArray<nsCString>& aClassifierFeatures) {
  // Validate category.
  NS_ENSURE_TRUE(
      aCategory == nsIUrlClassifierExceptionListEntry::Category::
                       CATEGORY_INTERNAL_PREF ||
          aCategory ==
              nsIUrlClassifierExceptionListEntry::Category::CATEGORY_BASELINE ||
          aCategory == nsIUrlClassifierExceptionListEntry::Category::
                           CATEGORY_CONVENIENCE,
      NS_ERROR_INVALID_ARG);
  mCategory = aCategory;
  mUrlPattern = aUrlPattern;
  mTopLevelUrlPattern = aTopLevelUrlPattern;
  mIsPrivateBrowsingOnly = aIsPrivateBrowsingOnly;
  mFilterContentBlockingCategories = aFilterContentBlockingCategories.Clone();
  mClassifierFeatures = aClassifierFeatures.Clone();

  // Create pattern from urlPattern and topLevelUrlPattern strings.
  ErrorResult error;
  mMatcher = new extensions::MatchPatternCore(
      NS_ConvertUTF8toUTF16(mUrlPattern), false, false, error);
  RETURN_NSRESULT_ON_FAILURE(error);

  if (!mTopLevelUrlPattern.IsEmpty()) {
    mTopLevelMatcher = new extensions::MatchPatternCore(
        NS_ConvertUTF8toUTF16(mTopLevelUrlPattern), false, false, error);
    RETURN_NSRESULT_ON_FAILURE(error);
  }
  return NS_OK;
}

NS_IMETHODIMP
UrlClassifierExceptionListEntry::Matches(nsIURI* aURI, nsIURI* aTopLevelURI,
                                         bool aIsPrivateBrowsing,
                                         bool* aResult) {
  NS_ENSURE_ARG_POINTER(aURI);
  NS_ENSURE_ARG_POINTER(aResult);
  *aResult = false;

  MOZ_ASSERT(
      mCategory == nsIUrlClassifierExceptionListEntry::Category::
                       CATEGORY_INTERNAL_PREF ||
      mCategory ==
          nsIUrlClassifierExceptionListEntry::Category::CATEGORY_BASELINE ||
      mCategory ==
          nsIUrlClassifierExceptionListEntry::Category::CATEGORY_CONVENIENCE);

  // Check if the entry category is enabled. CATEGORY_INTERNAL_PREF always
  // applies.
  if ((mCategory ==
           nsIUrlClassifierExceptionListEntry::Category::CATEGORY_BASELINE &&
       !StaticPrefs::
           privacy_trackingprotection_allow_list_baseline_enabled()) ||
      (mCategory ==
           nsIUrlClassifierExceptionListEntry::Category::CATEGORY_CONVENIENCE &&
       !StaticPrefs::
           privacy_trackingprotection_allow_list_convenience_enabled())) {
    return NS_OK;
  }

  // Entry is scoped to private browsing only and we're not in private browsing.
  if (!aIsPrivateBrowsing && mIsPrivateBrowsingOnly) {
    return NS_OK;
  }

  // Next, check if the current content blocking category pref matches the
  // allowed content blocking categories for this exception entry.
  if (!mFilterContentBlockingCategories.IsEmpty()) {
    nsCString prefValue;
    nsresult rv =
        Preferences::GetCString("browser.contentblocking.category", prefValue);

    // If the pref is not set this check is skipped.
    if (NS_SUCCEEDED(rv) && !prefValue.IsEmpty()) {
      if (!mFilterContentBlockingCategories.Contains(prefValue)) {
        return NS_OK;
      }
    }
  }

  // Check if the load URI matches the urlPattern.
  if (!mMatcher->Matches(aURI)) {
    return NS_OK;
  }

  // If this entry filters for top level site, check if the top level URI
  // matches the topLevelUrlPattern. If the entry filters for top level site,
  // but the caller does not provide one, we will not match.
  if (mTopLevelMatcher &&
      (!aTopLevelURI || !mTopLevelMatcher->Matches(aTopLevelURI))) {
    return NS_OK;
  }

  *aResult = true;
  return NS_OK;
}

NS_IMETHODIMP
UrlClassifierExceptionListEntry::GetCategory(
    nsIUrlClassifierExceptionListEntry::Category* aCategory) {
  *aCategory = mCategory;
  return NS_OK;
}

NS_IMETHODIMP
UrlClassifierExceptionListEntry::GetUrlPattern(nsACString& aUrlPattern) {
  aUrlPattern = mUrlPattern;
  return NS_OK;
}

NS_IMETHODIMP
UrlClassifierExceptionListEntry::GetTopLevelUrlPattern(
    nsACString& aTopLevelUrlPattern) {
  aTopLevelUrlPattern = mTopLevelUrlPattern;
  return NS_OK;
}

NS_IMETHODIMP
UrlClassifierExceptionListEntry::GetIsPrivateBrowsingOnly(
    bool* aIsPrivateBrowsingOnly) {
  *aIsPrivateBrowsingOnly = mIsPrivateBrowsingOnly;
  return NS_OK;
}

NS_IMETHODIMP
UrlClassifierExceptionListEntry::GetFilterContentBlockingCategories(
    nsTArray<nsCString>& aFilterContentBlockingCategories) {
  aFilterContentBlockingCategories = mFilterContentBlockingCategories.Clone();
  return NS_OK;
}

NS_IMETHODIMP
UrlClassifierExceptionListEntry::GetClassifierFeatures(
    nsTArray<nsCString>& aClassifierFeatures) {
  aClassifierFeatures = mClassifierFeatures.Clone();
  return NS_OK;
}

NS_IMETHODIMP
UrlClassifierExceptionListEntry::Describe(nsACString& aDescription) {
  nsAutoCString categories;
  for (const auto& category : mFilterContentBlockingCategories) {
    if (!categories.IsEmpty()) {
      categories.AppendLiteral(", ");
    }
    categories.Append(category);
  }

  nsAutoCString classifierFeatures;
  for (const auto& feature : mClassifierFeatures) {
    if (!classifierFeatures.IsEmpty()) {
      classifierFeatures.AppendLiteral(", ");
    }
    classifierFeatures.Append(feature);
  }

  aDescription.AppendPrintf(
      "UrlClassifierExceptionListEntry(urlPattern='%s', "
      "topLevelUrlPattern='%s', isPrivateBrowsingOnly=%s, "
      "filterContentBlockingCategories=[%s], classifierFeatures=[%s])",
      mUrlPattern.get(), mTopLevelUrlPattern.get(),
      mIsPrivateBrowsingOnly ? "true" : "false", categories.get(),
      classifierFeatures.get());

  return NS_OK;
}

}  // namespace mozilla::net
