/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IntegrityPolicyService.h"

#include "nsContentSecurityManager.h"
#include "nsContentUtils.h"
#include "nsILoadInfo.h"
#include "nsString.h"

#include "mozilla/BasePrincipal.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/IntegrityPolicy.h"
#include "mozilla/dom/RequestBinding.h"
#include "mozilla/dom/SRIMetadata.h"
#include "mozilla/Logging.h"
#include "mozilla/net/SFVService.h"
#include "mozilla/StaticPrefs_security.h"

using namespace mozilla;

static LazyLogModule sIntegrityPolicyServiceLogModule("IntegrityPolicy");
#define LOG(fmt, ...)                                                 \
  MOZ_LOG_FMT(sIntegrityPolicyServiceLogModule, LogLevel::Debug, fmt, \
              ##__VA_ARGS__)

namespace mozilla::dom {

IntegrityPolicyService::~IntegrityPolicyService() = default;

/* nsIContentPolicy implementation */
NS_IMETHODIMP
IntegrityPolicyService::ShouldLoad(nsIURI* aContentLocation,
                                   nsILoadInfo* aLoadInfo, int16_t* aDecision) {
  LOG("ShouldLoad: [{}] Entered ShouldLoad", static_cast<void*>(aLoadInfo));

  *aDecision = nsIContentPolicy::ACCEPT;

  if (!StaticPrefs::security_integrity_policy_enabled()) {
    LOG("ShouldLoad: [{}] Integrity policy is disabled",
        static_cast<void*>(aLoadInfo));
    return NS_OK;
  }

  if (!aContentLocation) {
    LOG("ShouldLoad: [{}] No content location", static_cast<void*>(aLoadInfo));
    return NS_ERROR_FAILURE;
  }

  bool block = ShouldRequestBeBlocked(aContentLocation, aLoadInfo);
  *aDecision =
      block ? nsIContentPolicy::REJECT_SERVER : nsIContentPolicy::ACCEPT;
  return NS_OK;
}

NS_IMETHODIMP IntegrityPolicyService::ShouldProcess(nsIURI* aContentLocation,
                                                    nsILoadInfo* aLoadInfo,
                                                    int16_t* aDecision) {
  *aDecision = nsIContentPolicy::ACCEPT;
  return NS_OK;
}

// https://w3c.github.io/webappsec-subresource-integrity/#should-request-be-blocked-by-integrity-policy-section
bool IntegrityPolicyService::ShouldRequestBeBlocked(nsIURI* aContentLocation,
                                                    nsILoadInfo* aLoadInfo) {
  // Efficiency check: if we don't care about this type, we can skip.
  auto destination = IntegrityPolicy::ContentTypeToDestinationType(
      aLoadInfo->InternalContentPolicyType());
  if (destination.isNothing()) {
    LOG("ShouldLoad: [{}] Integrity policy doesn't handle this type={}",
        static_cast<void*>(aLoadInfo),
        static_cast<uint8_t>(aLoadInfo->InternalContentPolicyType()));
    return false;
  }

  // Exempt addons from integrity policy checks.
  // Top level document loads have null LoadingPrincipal, but we don't apply
  // integrity policy to top level document loads right now.
  if (BasePrincipal::Cast(aLoadInfo->TriggeringPrincipal())
          ->OverridesCSP(aLoadInfo->GetLoadingPrincipal())) {
    LOG("ShouldLoad: [{}] Got a request from an addon, allowing it.",
        static_cast<void*>(aLoadInfo));
    return false;
  }

  // 2. Let parsedMetadata be the result of calling parse metadata with
  // request’s integrity metadata.
  // In our case, parsedMetadata is in loadInfo.
  Maybe<RequestMode> maybeRequestMode;
  aLoadInfo->GetRequestMode(&maybeRequestMode);
  if (maybeRequestMode.isNothing()) {
    // We don't have a request mode set explicitly, get it from the secFlags.
    // Just make sure that we aren't trying to get it from a
    // nsILoadInfo::SEC_ONLY_FOR_EXPLICIT_CONTENTSEC_CHECK loadInfo. In those
    // cases, we have to set the requestMode explicitly.
    MOZ_ASSERT(aLoadInfo->GetSecurityFlags() !=
               nsILoadInfo::SEC_ONLY_FOR_EXPLICIT_CONTENTSEC_CHECK);

    maybeRequestMode = Some(nsContentSecurityManager::SecurityModeToRequestMode(
        aLoadInfo->GetSecurityMode()));
  }

  RequestMode requestMode = *maybeRequestMode;

  if (MOZ_LOG_TEST(sIntegrityPolicyServiceLogModule, LogLevel::Debug)) {
    nsAutoString integrityMetadata;
    aLoadInfo->GetIntegrityMetadata(integrityMetadata);

    LOG("ShouldLoad: [{}] uri={} destination={} "
        "requestMode={} integrityMetadata={}",
        static_cast<void*>(aLoadInfo), aContentLocation->GetSpecOrDefault(),
        static_cast<uint8_t>(*destination), static_cast<uint8_t>(requestMode),
        NS_ConvertUTF16toUTF8(integrityMetadata).get());
  }

  // 3. If parsedMetadata is not the empty set and request’s mode is either
  // "cors" or "same-origin", return "Allowed".
  if (requestMode == RequestMode::Cors ||
      requestMode == RequestMode::Same_origin) {
    nsAutoString integrityMetadata;
    aLoadInfo->GetIntegrityMetadata(integrityMetadata);

    SRIMetadata outMetadata;
    dom::SRICheck::IntegrityMetadata(integrityMetadata,
                                     aContentLocation->GetSpecOrDefault(),
                                     nullptr, &outMetadata);

    if (outMetadata.IsValid()) {
      LOG("ShouldLoad: [{}] Allowed because we have valid a integrity.",
          static_cast<void*>(aLoadInfo));
      return false;
    }
  }

  // 4. If request's url is local, return "Allowed".
  if (aContentLocation->SchemeIs("data") ||
      aContentLocation->SchemeIs("blob") ||
      aContentLocation->SchemeIs("about")) {
    LOG("ShouldLoad: [{}] Allowed because we have data or blob.",
        static_cast<void*>(aLoadInfo));
    return false;
  }

  // We only support integrity policy for documents so far.
  // TODO(fkilic): Add aLoadInfo->GetIntegrityPolicy(), instead of
  // getting the document and integrity policy from it.
  // It may do the same thing but it would be more organized.
  RefPtr<mozilla::dom::Document> doc;
  aLoadInfo->GetLoadingDocument(getter_AddRefs(doc));
  if (!doc) {
    LOG("ShouldLoad: [{}] No document", static_cast<void*>(aLoadInfo));
    return false;
  }

  // 5. Let policy be policyContainer’s integrity policy.
  // 6. Let reportPolicy be policyContainer’s report only integrity policy.
  // Our IntegrityPolicy struct contains both the enforcement and
  // report-only policies.
  RefPtr<IntegrityPolicy> policy = doc->GetIntegrityPolicy();
  if (!policy) {
    // 7. If both policy and reportPolicy are empty integrity policy structs,
    // return "Allowed".
    LOG("ShouldLoad: [{}] No integrity policy", static_cast<void*>(aLoadInfo));
    return false;
  }

  // TODO: 8. Let global be request’s client’s global object.
  // TODO: 9. If global is not a Window nor a WorkerGlobalScope, return
  // "Allowed".

  // Steps 10-13 in policy->PolicyContains(...)
  bool contains = false;
  bool roContains = false;
  policy->PolicyContains(*destination, &contains, &roContains);

  // TODO: 14. If block is true or reportBlock is true, then report violation
  // with request, block, reportBlock, policy and reportPolicy.
  MaybeReport(aContentLocation, aLoadInfo, contains, roContains);

  // 15. If block is true, then return "Blocked"; otherwise "Allowed".
  return contains;
}

void IntegrityPolicyService::MaybeReport(nsIURI* aContentLocation,
                                         nsILoadInfo* aLoadInfo, bool aEnforce,
                                         bool aReportOnly) {
  if (!aEnforce && !aReportOnly) {
    return;
  }

  if (nsContentUtils::IsPreloadType(aLoadInfo->InternalContentPolicyType())) {
    return;  // Don't report for preloads.
  }

  // We just report to the console for now. We should use the reporting API
  // in the future.
  uint64_t windowID = aLoadInfo->GetInnerWindowID();
  AutoTArray<nsString, 1> params = {
      NS_ConvertUTF8toUTF16(aContentLocation->GetSpecOrDefault())};
  nsAutoString localizedMsg;
  nsresult rv = nsContentUtils::FormatLocalizedString(
      nsContentUtils::eSECURITY_PROPERTIES,
      aReportOnly ? "IntegrityPolicyReportOnlyBlockResource"
                  : "IntegrityPolicyEnforceBlockResource",
      params, localizedMsg);
  NS_ENSURE_SUCCESS_VOID(rv);

  nsContentUtils::ReportToConsoleByWindowID(
      localizedMsg,
      aReportOnly ? nsIScriptError::warningFlag : nsIScriptError::errorFlag,
      "Security"_ns, windowID);
}

NS_IMPL_ISUPPORTS(IntegrityPolicyService, nsIContentPolicy)

}  // namespace mozilla::dom

#undef LOG
