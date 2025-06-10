/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IntegrityPolicy.h"

#include "nsCOMPtr.h"
#include "nsString.h"

#include "mozilla/dom/RequestBinding.h"
#include "mozilla/Logging.h"
#include "mozilla/net/SFVService.h"
#include "mozilla/StaticPrefs_security.h"

using namespace mozilla;

static LazyLogModule sIntegrityPolicyLogModule("IntegrityPolicy");
#define LOG(fmt, ...) \
  MOZ_LOG_FMT(sIntegrityPolicyLogModule, LogLevel::Debug, fmt, ##__VA_ARGS__)

namespace mozilla::dom {

IntegrityPolicy::~IntegrityPolicy() = default;

RequestDestination ContentTypeToDestination(nsContentPolicyType aType) {
  // From SecFetch.cpp
  // https://searchfox.org/mozilla-central/rev/f1e32fa7054859d37eea8804e220dfcc7fb53b03/dom/security/SecFetch.cpp#24-32
  switch (aType) {
    case nsIContentPolicy::TYPE_INTERNAL_SCRIPT:
    case nsIContentPolicy::TYPE_INTERNAL_SCRIPT_PRELOAD:
    case nsIContentPolicy::TYPE_INTERNAL_MODULE:
    case nsIContentPolicy::TYPE_INTERNAL_MODULE_PRELOAD:
    // We currently only support documents.
    // case nsIContentPolicy::TYPE_INTERNAL_WORKER_IMPORT_SCRIPTS:
    case nsIContentPolicy::TYPE_INTERNAL_CHROMEUTILS_COMPILED_SCRIPT:
    case nsIContentPolicy::TYPE_INTERNAL_FRAME_MESSAGEMANAGER_SCRIPT:
    case nsIContentPolicy::TYPE_SCRIPT:
      return RequestDestination::Script;

    default:
      return RequestDestination::_empty;
  }
}

Maybe<IntegrityPolicy::DestinationType> DOMRequestDestinationToDestinationType(
    RequestDestination aDestination) {
  switch (aDestination) {
    case RequestDestination::Script:
      return Some(IntegrityPolicy::DestinationType::Script);

    default:
      return Nothing{};
  }
}

Maybe<IntegrityPolicy::DestinationType>
IntegrityPolicy::ContentTypeToDestinationType(nsContentPolicyType aType) {
  return DOMRequestDestinationToDestinationType(
      ContentTypeToDestination(aType));
}

nsresult GetStringsFromInnerList(nsISFVInnerList* aList, bool aIsToken,
                                 nsTArray<nsCString>& aStrings) {
  nsTArray<RefPtr<nsISFVItem>> items;
  nsresult rv = aList->GetItems(items);
  NS_ENSURE_SUCCESS(rv, rv);

  for (auto& item : items) {
    nsCOMPtr<nsISFVBareItem> value;
    rv = item->GetValue(getter_AddRefs(value));
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoCString itemStr;
    if (aIsToken) {
      nsCOMPtr<nsISFVToken> itemToken(do_QueryInterface(value));
      NS_ENSURE_TRUE(itemToken, NS_ERROR_FAILURE);

      rv = itemToken->GetValue(itemStr);
      NS_ENSURE_SUCCESS(rv, rv);
    } else {
      nsCOMPtr<nsISFVString> itemString(do_QueryInterface(value));
      NS_ENSURE_TRUE(itemString, NS_ERROR_FAILURE);

      rv = itemString->GetValue(itemStr);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    aStrings.AppendElement(itemStr);
  }

  return NS_OK;
}

/* static */
Result<IntegrityPolicy::Sources, nsresult> ParseSources(
    nsISFVDictionary* aDict) {
  // sources, a list of sources, Initially empty.

  // 3. If dictionary["sources"] does not exist or if its value contains
  // "inline", append "inline" to integrityPolicy’s sources.
  nsCOMPtr<nsISFVItemOrInnerList> iil;
  nsresult rv = aDict->Get("sources"_ns, getter_AddRefs(iil));
  if (NS_FAILED(rv)) {
    // The key doesn't exists, set it to inline as per spec.
    return IntegrityPolicy::Sources(IntegrityPolicy::SourceType::Inline);
  }

  nsCOMPtr<nsISFVInnerList> il(do_QueryInterface(iil));
  NS_ENSURE_TRUE(il, Err(NS_ERROR_FAILURE));

  nsTArray<nsCString> sources;
  rv = GetStringsFromInnerList(il, true, sources);
  NS_ENSURE_SUCCESS(rv, Err(rv));

  IntegrityPolicy::Sources result;
  for (const auto& source : sources) {
    if (source.EqualsLiteral("inline")) {
      result += IntegrityPolicy::SourceType::Inline;
    } else {
      LOG("ParseSources: Unknown source: {}", source.get());
      // Unknown source, we don't know how to handle it
      continue;
    }
  }

  return result;
}

/* static */
Result<IntegrityPolicy::Destinations, nsresult> ParseDestinations(
    nsISFVDictionary* aDict) {
  // blocked destinations, a list of destinations, initially empty.

  nsCOMPtr<nsISFVItemOrInnerList> iil;
  nsresult rv = aDict->Get("blocked-destinations"_ns, getter_AddRefs(iil));
  if (NS_FAILED(rv)) {
    return IntegrityPolicy::Destinations();
  }

  // 4. If dictionary["blocked-destinations"] exists:
  nsCOMPtr<nsISFVInnerList> il(do_QueryInterface(iil));
  NS_ENSURE_TRUE(il, Err(NS_ERROR_FAILURE));

  nsTArray<nsCString> destinations;
  rv = GetStringsFromInnerList(il, true, destinations);
  NS_ENSURE_SUCCESS(rv, Err(rv));

  IntegrityPolicy::Destinations result;
  for (const auto& destination : destinations) {
    if (destination.EqualsLiteral("script")) {
      result += IntegrityPolicy::DestinationType::Script;
    } else {
      LOG("ParseDestinations: Unknown destination: {}", destination.get());
      // Unknown destination, we don't know how to handle it
      continue;
    }
  }

  return result;
}

/* static */
// https://w3c.github.io/webappsec-subresource-integrity/#processing-an-integrity-policy
nsresult IntegrityPolicy::ParseHeaders(const nsACString& aHeader,
                                       const nsACString& aHeaderRO,
                                       IntegrityPolicy** aPolicy) {
  if (!StaticPrefs::security_integrity_policy_enabled()) {
    return NS_OK;
  }

  // 1. Let integrityPolicy be a new integrity policy struct.
  // (Our struct contains two entries, one for the enforcement header and one
  // for report-only)
  RefPtr<IntegrityPolicy> policy = new IntegrityPolicy();

  LOG("[{}] Parsing headers: enforcement='{}' report-only='{}'",
      static_cast<void*>(policy), aHeader.Data(), aHeaderRO.Data());

  nsCOMPtr<nsISFVService> sfv = net::GetSFVService();
  NS_ENSURE_TRUE(sfv, NS_ERROR_FAILURE);

  for (const auto& isROHeader : {false, true}) {
    const auto& headerString = isROHeader ? aHeaderRO : aHeader;

    if (headerString.IsEmpty()) {
      LOG("[{}] No {} header.", static_cast<void*>(policy),
          isROHeader ? "report-only" : "enforcement");
      continue;
    }

    // 2. Let dictionary be the result of getting a structured field value from
    // headers given headerName and "dictionary".
    nsCOMPtr<nsISFVDictionary> dict;
    nsresult rv = sfv->ParseDictionary(headerString, getter_AddRefs(dict));
    if (NS_FAILED(rv)) {
      LOG("[{}] Failed to parse {} header.", static_cast<void*>(policy),
          isROHeader ? "report-only" : "enforcement");
      continue;
    }

    // 3. If dictionary["sources"] does not exist or if its value contains
    // "inline", append "inline" to integrityPolicy’s sources.
    auto sourcesResult = ParseSources(dict);
    if (sourcesResult.isErr()) {
      LOG("[{}] Failed to parse sources for {} header.",
          static_cast<void*>(policy),
          isROHeader ? "report-only" : "enforcement");
      continue;
    }

    // 4. If dictionary["blocked-destinations"] exists:
    auto destinationsResult = ParseDestinations(dict);
    if (destinationsResult.isErr()) {
      LOG("[{}] Failed to parse destinations for {} header.",
          static_cast<void*>(policy),
          isROHeader ? "report-only" : "enforcement");
      continue;
    }

    // 5. If dictionary["endpoints"] exists:
    // TODO: We don't support Reporting API, so we don't need to parse endpoints
    // (yay?)

    LOG("[{}] Creating policy for {} header. sources={} destinations={}",
        static_cast<void*>(policy), isROHeader ? "report-only" : "enforcement",
        sourcesResult.unwrap().serialize(),
        destinationsResult.unwrap().serialize());

    Entry entry = Entry(sourcesResult.unwrap(), destinationsResult.unwrap());
    if (isROHeader) {
      policy->mReportOnly.emplace(entry);
    } else {
      policy->mEnforcement.emplace(entry);
    }
  }

  // 6. Return integrityPolicy.
  policy.forget(aPolicy);

  LOG("[{}] Finished parsing headers.", static_cast<void*>(policy));

  return NS_OK;
}

void IntegrityPolicy::PolicyContains(DestinationType aDestination,
                                     bool* aContains, bool* aROContains) const {
  // 10. Let block be a boolean, initially false.
  *aContains = false;
  // 11. Let reportBlock be a boolean, initially false.
  *aROContains = false;

  // 12. If policy’s sources contains "inline" and policy’s blocked destinations
  // contains request’s destination, set block to true.
  if (mEnforcement && mEnforcement->mDestinations.contains(aDestination) &&
      mEnforcement->mSources.contains(SourceType::Inline)) {
    *aContains = true;
  }

  // 13. If reportPolicy’s sources contains "inline" and reportPolicy’s blocked
  // destinations contains request’s destination, set reportBlock to true.
  if (mReportOnly && mReportOnly->mDestinations.contains(aDestination) &&
      mReportOnly->mSources.contains(SourceType::Inline)) {
    *aROContains = true;
  }
}

NS_IMPL_ISUPPORTS(IntegrityPolicy, nsIIntegrityPolicy)

}  // namespace mozilla::dom

#undef LOG
