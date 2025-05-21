/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "UrlClassifierFeatureAntiFraudAnnotation.h"

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

#define ANTIFRAUD_ANNOTATION_FEATURE_NAME "antifraud-annotation"

#define URLCLASSIFIER_ANTIFRAUD_ANNOTATION_BLOCKLIST \
  "urlclassifier.features.antifraud.annotate.blocklistTables"
#define URLCLASSIFIER_ANTIFRAUD_ANNOTATION_BLOCKLIST_TEST_ENTRIES \
  "urlclassifier.features.antifraud.annotate.blocklistHosts"
#define URLCLASSIFIER_ANTIFRAUD_ANNOTATION_ENTITYLIST \
  "urlclassifier.features.antifraud.annotate.allowlistTables"
#define URLCLASSIFIER_ANTIFRAUD_ANNOTATION_ENTITYLIST_TEST_ENTRIES \
  "urlclassifier.features.antifraud.annotate.allowlistHosts"
#define URLCLASSIFIER_ANTIFRAUD_ANNOTATION_EXCEPTION_URLS \
  "urlclassifier.features.antifraud.annotate.skipURLs"
#define TABLE_ANTIFRAUD_ANNOTATION_BLOCKLIST_PREF \
  "antifraud-annotate-blocklist-pref"
#define TABLE_ANTIFRAUD_ANNOTATION_ENTITYLIST_PREF \
  "antifraud-annotate-allowlist-pref"

static StaticRefPtr<UrlClassifierFeatureAntiFraudAnnotation>
    gFeatureAntiFraudAnnotation;

}  // namespace

UrlClassifierFeatureAntiFraudAnnotation::
    UrlClassifierFeatureAntiFraudAnnotation()
    : UrlClassifierFeatureAntiTrackingBase(
          nsLiteralCString(ANTIFRAUD_ANNOTATION_FEATURE_NAME),
          nsLiteralCString(URLCLASSIFIER_ANTIFRAUD_ANNOTATION_BLOCKLIST),
          nsLiteralCString(URLCLASSIFIER_ANTIFRAUD_ANNOTATION_ENTITYLIST),
          nsLiteralCString(
              URLCLASSIFIER_ANTIFRAUD_ANNOTATION_BLOCKLIST_TEST_ENTRIES),
          nsLiteralCString(
              URLCLASSIFIER_ANTIFRAUD_ANNOTATION_ENTITYLIST_TEST_ENTRIES),
          nsLiteralCString(TABLE_ANTIFRAUD_ANNOTATION_BLOCKLIST_PREF),
          nsLiteralCString(TABLE_ANTIFRAUD_ANNOTATION_ENTITYLIST_PREF),
          nsLiteralCString(URLCLASSIFIER_ANTIFRAUD_ANNOTATION_EXCEPTION_URLS)) {
}

/* static */ const char* UrlClassifierFeatureAntiFraudAnnotation::Name() {
  return ANTIFRAUD_ANNOTATION_FEATURE_NAME;
}

/* static */
void UrlClassifierFeatureAntiFraudAnnotation::MaybeInitialize() {
  MOZ_ASSERT(XRE_IsParentProcess());
  UC_LOG_LEAK(("UrlClassifierFeatureAntiFraudAnnotation::MaybeInitialize"));

  if (!gFeatureAntiFraudAnnotation) {
    gFeatureAntiFraudAnnotation = new UrlClassifierFeatureAntiFraudAnnotation();
    gFeatureAntiFraudAnnotation->InitializePreferences();
  }
}

/* static */
void UrlClassifierFeatureAntiFraudAnnotation::MaybeShutdown() {
  UC_LOG_LEAK(("UrlClassifierFeatureAntiFraudAnnotation::MaybeShutdown"));

  if (gFeatureAntiFraudAnnotation) {
    gFeatureAntiFraudAnnotation->ShutdownPreferences();
    gFeatureAntiFraudAnnotation = nullptr;
  }
}

/* static */
already_AddRefed<UrlClassifierFeatureAntiFraudAnnotation>
UrlClassifierFeatureAntiFraudAnnotation::MaybeCreate(nsIChannel* aChannel) {
  MOZ_ASSERT(aChannel);

  UC_LOG_LEAK(
      ("UrlClassifierFeatureAntiFraudAnnotation::MaybeCreate - channel %p",
       aChannel));

  if (!StaticPrefs::privacy_trackingprotection_antifraud_annotate_channels()) {
    return nullptr;
  }

  // We also don't need to annotate the channel if we are not blocking trackers
  if (!StaticPrefs::privacy_trackingprotection_fingerprinting_enabled()) {
    return nullptr;
  }

  MaybeInitialize();
  MOZ_ASSERT(gFeatureAntiFraudAnnotation);

  RefPtr<UrlClassifierFeatureAntiFraudAnnotation> self =
      gFeatureAntiFraudAnnotation;
  return self.forget();
}

/* static */
already_AddRefed<nsIUrlClassifierFeature>
UrlClassifierFeatureAntiFraudAnnotation::GetIfNameMatches(
    const nsACString& aName) {
  if (!aName.EqualsLiteral(ANTIFRAUD_ANNOTATION_FEATURE_NAME)) {
    return nullptr;
  }

  MaybeInitialize();
  MOZ_ASSERT(gFeatureAntiFraudAnnotation);

  RefPtr<UrlClassifierFeatureAntiFraudAnnotation> self =
      gFeatureAntiFraudAnnotation;
  return self.forget();
}

NS_IMETHODIMP
UrlClassifierFeatureAntiFraudAnnotation::ProcessChannel(
    nsIChannel* aChannel, const nsTArray<nsCString>& aList,
    const nsTArray<nsCString>& aHashes, bool* aShouldContinue) {
  NS_ENSURE_ARG_POINTER(aChannel);
  NS_ENSURE_ARG_POINTER(aShouldContinue);

  // This is not a blocking feature.
  *aShouldContinue = true;

  UC_LOG(
      ("UrlClassifierFeatureAntiFraudAnnotation::ProcessChannel - "
       "annotating channel %p",
       aChannel));

  static std::vector<UrlClassifierCommon::ClassificationData>
      sClassificationData = {
          {"consent-manager-track-"_ns,
           nsIClassifiedChannel::ClassificationFlags::CLASSIFIED_ANTIFRAUD},
      };

  uint32_t flags = UrlClassifierCommon::TablesToClassificationFlags(
      aList, sClassificationData,
      nsIClassifiedChannel::ClassificationFlags::CLASSIFIED_ANTIFRAUD);

  UrlClassifierCommon::SetTrackingInfo(aChannel, aList, aHashes);

  UrlClassifierCommon::AnnotateChannelWithoutNotifying(aChannel, flags);

  return NS_OK;
}

NS_IMETHODIMP
UrlClassifierFeatureAntiFraudAnnotation::GetURIByListType(
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
