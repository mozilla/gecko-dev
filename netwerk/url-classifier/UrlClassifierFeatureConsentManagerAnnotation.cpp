/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "UrlClassifierFeatureConsentManagerAnnotation.h"

#include "Classifier.h"
#include "mozilla/Logging.h"
#include "mozilla/StaticPrefs_privacy.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/net/UrlClassifierCommon.h"
#include "nsIChannel.h"
#include "nsIClassifiedChannel.h"
#include "nsIWebProgressListener.h"
#include "nsContentUtils.h"

namespace mozilla {
namespace net {

namespace {

#define CONSENTMANAGER_ANNOTATION_FEATURE_NAME "consentmanager-annotation"

#define URLCLASSIFIER_CONSENTMANAGER_ANNOTATION_BLOCKLIST \
  "urlclassifier.features.consentmanager.annotate.blocklistTables"
#define URLCLASSIFIER_CONSENTMANAGER_ANNOTATION_BLOCKLIST_TEST_ENTRIES \
  "urlclassifier.features.consentmanager.annotate.blocklistHosts"
#define URLCLASSIFIER_CONSENTMANAGER_ANNOTATION_ENTITYLIST \
  "urlclassifier.features.consentmanager.annotate.allowlistTables"
#define URLCLASSIFIER_CONSENTMANAGER_ANNOTATION_ENTITYLIST_TEST_ENTRIES \
  "urlclassifier.features.consentmanager.annotate.allowlistHosts"
#define URLCLASSIFIER_CONSENTMANAGER_ANNOTATION_EXCEPTION_URLS \
  "urlclassifier.features.consentmanager.annotate.skipURLs"
#define TABLE_CONSENTMANAGER_ANNOTATION_BLOCKLIST_PREF \
  "consentmanager-annotate-blocklist-pref"
#define TABLE_CONSENTMANAGER_ANNOTATION_ENTITYLIST_PREF \
  "consentmanager-annotate-allowlist-pref"

static StaticRefPtr<UrlClassifierFeatureConsentManagerAnnotation>
    gFeatureConsentManagerAnnotation;

}  // namespace

UrlClassifierFeatureConsentManagerAnnotation::
    UrlClassifierFeatureConsentManagerAnnotation()
    : UrlClassifierFeatureAntiTrackingBase(
          nsLiteralCString(CONSENTMANAGER_ANNOTATION_FEATURE_NAME),
          nsLiteralCString(URLCLASSIFIER_CONSENTMANAGER_ANNOTATION_BLOCKLIST),
          nsLiteralCString(URLCLASSIFIER_CONSENTMANAGER_ANNOTATION_ENTITYLIST),
          nsLiteralCString(
              URLCLASSIFIER_CONSENTMANAGER_ANNOTATION_BLOCKLIST_TEST_ENTRIES),
          nsLiteralCString(
              URLCLASSIFIER_CONSENTMANAGER_ANNOTATION_ENTITYLIST_TEST_ENTRIES),
          nsLiteralCString(TABLE_CONSENTMANAGER_ANNOTATION_BLOCKLIST_PREF),
          nsLiteralCString(TABLE_CONSENTMANAGER_ANNOTATION_ENTITYLIST_PREF),
          nsLiteralCString(
              URLCLASSIFIER_CONSENTMANAGER_ANNOTATION_EXCEPTION_URLS)) {}

/* static */ const char* UrlClassifierFeatureConsentManagerAnnotation::Name() {
  return CONSENTMANAGER_ANNOTATION_FEATURE_NAME;
}

/* static */
void UrlClassifierFeatureConsentManagerAnnotation::MaybeInitialize() {
  MOZ_ASSERT(XRE_IsParentProcess());
  UC_LOG_LEAK(
      ("UrlClassifierFeatureConsentManagerAnnotation::MaybeInitialize"));

  if (!gFeatureConsentManagerAnnotation) {
    gFeatureConsentManagerAnnotation =
        new UrlClassifierFeatureConsentManagerAnnotation();
    gFeatureConsentManagerAnnotation->InitializePreferences();
  }
}

/* static */
void UrlClassifierFeatureConsentManagerAnnotation::MaybeShutdown() {
  UC_LOG_LEAK(("UrlClassifierFeatureConsentManagerAnnotation::MaybeShutdown"));

  if (gFeatureConsentManagerAnnotation) {
    gFeatureConsentManagerAnnotation->ShutdownPreferences();
    gFeatureConsentManagerAnnotation = nullptr;
  }
}

/* static */
already_AddRefed<UrlClassifierFeatureConsentManagerAnnotation>
UrlClassifierFeatureConsentManagerAnnotation::MaybeCreate(
    nsIChannel* aChannel) {
  MOZ_ASSERT(aChannel);

  UC_LOG_LEAK(
      ("UrlClassifierFeatureConsentManagerAnnotation::MaybeCreate - channel %p",
       aChannel));

  if (!StaticPrefs::
          privacy_trackingprotection_consentmanager_annotate_channels()) {
    return nullptr;
  }

  // We also don't need to annotate the channel if we are not blocking trackers
  if (!StaticPrefs::privacy_trackingprotection_enabled() &&
      !(NS_UsePrivateBrowsing(aChannel) &&
        StaticPrefs::privacy_trackingprotection_pbmode_enabled())) {
    return nullptr;
  }

  MaybeInitialize();
  MOZ_ASSERT(gFeatureConsentManagerAnnotation);

  RefPtr<UrlClassifierFeatureConsentManagerAnnotation> self =
      gFeatureConsentManagerAnnotation;
  return self.forget();
}

/* static */
already_AddRefed<nsIUrlClassifierFeature>
UrlClassifierFeatureConsentManagerAnnotation::GetIfNameMatches(
    const nsACString& aName) {
  if (!aName.EqualsLiteral(CONSENTMANAGER_ANNOTATION_FEATURE_NAME)) {
    return nullptr;
  }

  MaybeInitialize();
  MOZ_ASSERT(gFeatureConsentManagerAnnotation);

  RefPtr<UrlClassifierFeatureConsentManagerAnnotation> self =
      gFeatureConsentManagerAnnotation;
  return self.forget();
}

NS_IMETHODIMP
UrlClassifierFeatureConsentManagerAnnotation::ProcessChannel(
    nsIChannel* aChannel, const nsTArray<nsCString>& aList,
    const nsTArray<nsCString>& aHashes, bool* aShouldContinue) {
  NS_ENSURE_ARG_POINTER(aChannel);
  NS_ENSURE_ARG_POINTER(aShouldContinue);

  // This is not a blocking feature.
  *aShouldContinue = true;

  UC_LOG(
      ("UrlClassifierFeatureConsentManagerAnnotation::ProcessChannel - "
       "annotating channel %p",
       aChannel));

  static std::vector<UrlClassifierCommon::ClassificationData>
      sClassificationData = {
          {"consent-manager-track-"_ns,
           nsIClassifiedChannel::ClassificationFlags::
               CLASSIFIED_CONSENTMANAGER},
      };

  uint32_t flags = UrlClassifierCommon::TablesToClassificationFlags(
      aList, sClassificationData,
      nsIClassifiedChannel::ClassificationFlags::CLASSIFIED_CONSENTMANAGER);

  UrlClassifierCommon::SetTrackingInfo(aChannel, aList, aHashes);

  UrlClassifierCommon::AnnotateChannelWithoutNotifying(aChannel, flags);

  return NS_OK;
}

NS_IMETHODIMP
UrlClassifierFeatureConsentManagerAnnotation::GetURIByListType(
    nsIChannel* aChannel, nsIUrlClassifierFeature::listType aListType,
    nsIUrlClassifierFeature::URIType* aURIType, nsIURI** aURI) {
  NS_ENSURE_ARG_POINTER(aChannel);
  NS_ENSURE_ARG_POINTER(aURIType);
  NS_ENSURE_ARG_POINTER(aURI);

  if (aListType == nsIUrlClassifierFeature::blocklist) {
    *aURIType = nsIUrlClassifierFeature::blocklistURI;
    return aChannel->GetURI(aURI);
  }

  MOZ_ASSERT(aListType == nsIUrlClassifierFeature::entitylist);

  *aURIType = nsIUrlClassifierFeature::pairwiseEntitylistURI;
  return UrlClassifierCommon::CreatePairwiseEntityListURI(aChannel, aURI);
}

}  // namespace net
}  // namespace mozilla
