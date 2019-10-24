/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BackgroundUtils.h"

#include "MainThreadUtils.h"
#include "mozilla/Assertions.h"
#include "mozilla/ContentPrincipal.h"
#include "mozilla/NullPrincipal.h"
#include "mozilla/ipc/PBackgroundSharedTypes.h"
#include "mozilla/ipc/URIUtils.h"
#include "mozilla/net/CookieSettings.h"
#include "mozilla/net/NeckoChannelParams.h"
#include "ExpandedPrincipal.h"
#include "nsIScriptSecurityManager.h"
#include "nsIURI.h"
#include "nsNetUtil.h"
#include "mozilla/LoadInfo.h"
#include "nsContentUtils.h"
#include "nsString.h"
#include "nsTArray.h"
#include "mozilla/nsRedirectHistoryEntry.h"
#include "URIUtils.h"
#include "mozilla/dom/nsCSPUtils.h"
#include "mozilla/dom/nsCSPContext.h"

namespace mozilla {

using mozilla::BasePrincipal;
using mozilla::Maybe;
using mozilla::dom::ServiceWorkerDescriptor;
using namespace mozilla::net;

namespace ipc {

already_AddRefed<nsIPrincipal> PrincipalInfoToPrincipal(
    const PrincipalInfo& aPrincipalInfo, nsresult* aOptionalResult) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aPrincipalInfo.type() != PrincipalInfo::T__None);

  nsresult stackResult;
  nsresult& rv = aOptionalResult ? *aOptionalResult : stackResult;

  nsCOMPtr<nsIScriptSecurityManager> secMan =
      nsContentUtils::GetSecurityManager();
  if (!secMan) {
    return nullptr;
  }

  nsCOMPtr<nsIPrincipal> principal;

  switch (aPrincipalInfo.type()) {
    case PrincipalInfo::TSystemPrincipalInfo: {
      rv = secMan->GetSystemPrincipal(getter_AddRefs(principal));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return nullptr;
      }

      return principal.forget();
    }

    case PrincipalInfo::TNullPrincipalInfo: {
      const NullPrincipalInfo& info = aPrincipalInfo.get_NullPrincipalInfo();

      nsCOMPtr<nsIURI> uri;
      rv = NS_NewURI(getter_AddRefs(uri), info.spec());
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return nullptr;
      }

      principal = NullPrincipal::Create(info.attrs(), uri);
      return principal.forget();
    }

    case PrincipalInfo::TContentPrincipalInfo: {
      const ContentPrincipalInfo& info =
          aPrincipalInfo.get_ContentPrincipalInfo();

      nsCOMPtr<nsIURI> uri;
      rv = NS_NewURI(getter_AddRefs(uri), info.spec());
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return nullptr;
      }

      principal = BasePrincipal::CreateContentPrincipal(uri, info.attrs());
      if (NS_WARN_IF(!principal)) {
        return nullptr;
      }

      // Origin must match what the_new_principal.getOrigin returns.
      nsAutoCString originNoSuffix;
      rv = principal->GetOriginNoSuffix(originNoSuffix);
      if (NS_WARN_IF(NS_FAILED(rv)) ||
          !info.originNoSuffix().Equals(originNoSuffix)) {
        MOZ_CRASH("Origin must be available when deserialized");
      }

      if (info.domain()) {
        nsCOMPtr<nsIURI> domain;
        rv = NS_NewURI(getter_AddRefs(domain), *info.domain());
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return nullptr;
        }

        rv = principal->SetDomain(domain);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return nullptr;
        }
      }

      if (!info.baseDomain().IsVoid()) {
        nsAutoCString baseDomain;
        rv = principal->GetBaseDomain(baseDomain);
        if (NS_WARN_IF(NS_FAILED(rv)) ||
            !info.baseDomain().Equals(baseDomain)) {
          MOZ_CRASH("Base domain must be available when deserialized");
        }
      }

      return principal.forget();
    }

    case PrincipalInfo::TExpandedPrincipalInfo: {
      const ExpandedPrincipalInfo& info =
          aPrincipalInfo.get_ExpandedPrincipalInfo();

      nsTArray<nsCOMPtr<nsIPrincipal>> allowlist;
      nsCOMPtr<nsIPrincipal> alPrincipal;

      for (uint32_t i = 0; i < info.allowlist().Length(); i++) {
        alPrincipal = PrincipalInfoToPrincipal(info.allowlist()[i], &rv);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return nullptr;
        }
        // append that principal to the allowlist
        allowlist.AppendElement(alPrincipal);
      }

      RefPtr<ExpandedPrincipal> expandedPrincipal =
          ExpandedPrincipal::Create(allowlist, info.attrs());
      if (!expandedPrincipal) {
        NS_WARNING("could not instantiate expanded principal");
        return nullptr;
      }

      principal = expandedPrincipal;
      return principal.forget();
    }

    default:
      MOZ_CRASH("Unknown PrincipalInfo type!");
  }

  MOZ_CRASH("Should never get here!");
}

already_AddRefed<nsIContentSecurityPolicy> CSPInfoToCSP(
    const CSPInfo& aCSPInfo, Document* aRequestingDoc,
    nsresult* aOptionalResult) {
  MOZ_ASSERT(NS_IsMainThread());

  nsresult stackResult;
  nsresult& rv = aOptionalResult ? *aOptionalResult : stackResult;

  nsCOMPtr<nsIContentSecurityPolicy> csp = new nsCSPContext();

  if (aRequestingDoc) {
    rv = csp->SetRequestContextWithDocument(aRequestingDoc);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return nullptr;
    }
  } else {
    nsCOMPtr<nsIPrincipal> requestingPrincipal =
        PrincipalInfoToPrincipal(aCSPInfo.requestPrincipalInfo(), &rv);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return nullptr;
    }

    nsCOMPtr<nsIURI> selfURI;
    if (!aCSPInfo.selfURISpec().IsEmpty()) {
      rv = NS_NewURI(getter_AddRefs(selfURI), aCSPInfo.selfURISpec());
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return nullptr;
      }
    }
    rv = csp->SetRequestContextWithPrincipal(requestingPrincipal, selfURI,
                                             aCSPInfo.referrer(),
                                             aCSPInfo.innerWindowID());
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return nullptr;
    }
  }
  csp->SetSkipAllowInlineStyleCheck(aCSPInfo.skipAllowInlineStyleCheck());

  for (uint32_t i = 0; i < aCSPInfo.policyInfos().Length(); i++) {
    const PolicyInfo& policyInfo = aCSPInfo.policyInfos()[i];
    rv = csp->AppendPolicy(NS_ConvertUTF8toUTF16(policyInfo.policy()),
                           policyInfo.reportOnly(),
                           policyInfo.deliveredViaMetaTag());
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return nullptr;
    }
  }
  return csp.forget();
}

nsresult CSPToCSPInfo(nsIContentSecurityPolicy* aCSP, CSPInfo* aCSPInfo) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aCSP);
  MOZ_ASSERT(aCSPInfo);

  if (!aCSP || !aCSPInfo) {
    return NS_ERROR_FAILURE;
  }

  uint32_t count = 0;
  nsresult rv = aCSP->GetPolicyCount(&count);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<nsIPrincipal> requestPrincipal = aCSP->GetRequestPrincipal();

  PrincipalInfo requestingPrincipalInfo;
  rv = PrincipalToPrincipalInfo(requestPrincipal, &requestingPrincipalInfo);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<nsIURI> selfURI = aCSP->GetSelfURI();
  nsAutoCString selfURISpec;
  if (selfURI) {
    selfURI->GetSpec(selfURISpec);
  }

  nsAutoString referrer;
  aCSP->GetReferrer(referrer);

  uint64_t windowID = aCSP->GetInnerWindowID();
  bool skipAllowInlineStyleCheck = aCSP->GetSkipAllowInlineStyleCheck();

  nsTArray<PolicyInfo> policyInfos;
  for (uint32_t i = 0; i < count; ++i) {
    const nsCSPPolicy* policy = aCSP->GetPolicy(i);
    MOZ_ASSERT(policy);

    nsAutoString policyString;
    policy->toString(policyString);
    policyInfos.AppendElement(PolicyInfo(NS_ConvertUTF16toUTF8(policyString),
                                         policy->getReportOnlyFlag(),
                                         policy->getDeliveredViaMetaTagFlag()));
  }
  *aCSPInfo =
      CSPInfo(std::move(policyInfos), requestingPrincipalInfo, selfURISpec,
              referrer, windowID, skipAllowInlineStyleCheck);
  return NS_OK;
}

nsresult PrincipalToPrincipalInfo(nsIPrincipal* aPrincipal,
                                  PrincipalInfo* aPrincipalInfo,
                                  bool aSkipBaseDomain) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aPrincipal);
  MOZ_ASSERT(aPrincipalInfo);

  nsresult rv;
  if (aPrincipal->GetIsNullPrincipal()) {
    nsCOMPtr<nsIURI> uri;
    rv = aPrincipal->GetURI(getter_AddRefs(uri));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (NS_WARN_IF(!uri)) {
      return NS_ERROR_FAILURE;
    }

    nsAutoCString spec;
    rv = uri->GetSpec(spec);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    *aPrincipalInfo =
        NullPrincipalInfo(aPrincipal->OriginAttributesRef(), spec);
    return NS_OK;
  }

  if (aPrincipal->IsSystemPrincipal()) {
    *aPrincipalInfo = SystemPrincipalInfo();
    return NS_OK;
  }

  // might be an expanded principal
  auto* basePrin = BasePrincipal::Cast(aPrincipal);
  if (basePrin->Is<ExpandedPrincipal>()) {
    auto* expanded = basePrin->As<ExpandedPrincipal>();

    nsTArray<PrincipalInfo> allowlistInfo;
    PrincipalInfo info;

    for (auto& prin : expanded->AllowList()) {
      rv = PrincipalToPrincipalInfo(prin, &info, aSkipBaseDomain);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      // append that spec to the allowlist
      allowlistInfo.AppendElement(info);
    }

    *aPrincipalInfo = ExpandedPrincipalInfo(aPrincipal->OriginAttributesRef(),
                                            std::move(allowlistInfo));
    return NS_OK;
  }

  // must be a content principal

  nsCOMPtr<nsIURI> uri;
  rv = aPrincipal->GetURI(getter_AddRefs(uri));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (NS_WARN_IF(!uri)) {
    return NS_ERROR_FAILURE;
  }

  nsAutoCString spec;
  rv = uri->GetSpec(spec);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCString originNoSuffix;
  rv = aPrincipal->GetOriginNoSuffix(originNoSuffix);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<nsIURI> domainUri;
  rv = aPrincipal->GetDomain(getter_AddRefs(domainUri));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  Maybe<nsCString> domain;
  if (domainUri) {
    domain.emplace();
    rv = domainUri->GetSpec(domain.ref());
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  // This attribute is not crucial.
  nsCString baseDomain;
  if (aSkipBaseDomain) {
    baseDomain.SetIsVoid(true);
  } else {
    if (NS_FAILED(aPrincipal->GetBaseDomain(baseDomain))) {
      NS_WARNING("Failed to get base domain!");
      baseDomain.SetIsVoid(true);
    }
  }

  *aPrincipalInfo =
      ContentPrincipalInfo(aPrincipal->OriginAttributesRef(), originNoSuffix,
                           spec, domain, baseDomain);
  return NS_OK;
}

bool IsPincipalInfoPrivate(const PrincipalInfo& aPrincipalInfo) {
  if (aPrincipalInfo.type() != ipc::PrincipalInfo::TContentPrincipalInfo) {
    return false;
  }

  const ContentPrincipalInfo& info = aPrincipalInfo.get_ContentPrincipalInfo();
  return !!info.attrs().mPrivateBrowsingId;
}

already_AddRefed<nsIRedirectHistoryEntry> RHEntryInfoToRHEntry(
    const RedirectHistoryEntryInfo& aRHEntryInfo) {
  nsresult rv;
  nsCOMPtr<nsIPrincipal> principal =
      PrincipalInfoToPrincipal(aRHEntryInfo.principalInfo(), &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return nullptr;
  }

  nsCOMPtr<nsIURI> referrerUri = DeserializeURI(aRHEntryInfo.referrerUri());

  nsCOMPtr<nsIRedirectHistoryEntry> entry = new nsRedirectHistoryEntry(
      principal, referrerUri, aRHEntryInfo.remoteAddress());

  return entry.forget();
}

nsresult RHEntryToRHEntryInfo(nsIRedirectHistoryEntry* aRHEntry,
                              RedirectHistoryEntryInfo* aRHEntryInfo) {
  MOZ_ASSERT(aRHEntry);
  MOZ_ASSERT(aRHEntryInfo);

  nsresult rv;
  aRHEntry->GetRemoteAddress(aRHEntryInfo->remoteAddress());

  nsCOMPtr<nsIURI> referrerUri;
  rv = aRHEntry->GetReferrerURI(getter_AddRefs(referrerUri));
  NS_ENSURE_SUCCESS(rv, rv);
  SerializeURI(referrerUri, aRHEntryInfo->referrerUri());

  nsCOMPtr<nsIPrincipal> principal;
  rv = aRHEntry->GetPrincipal(getter_AddRefs(principal));
  NS_ENSURE_SUCCESS(rv, rv);

  return PrincipalToPrincipalInfo(principal, &aRHEntryInfo->principalInfo());
}

nsresult LoadInfoToLoadInfoArgs(nsILoadInfo* aLoadInfo,
                                Maybe<LoadInfoArgs>* aOptionalLoadInfoArgs) {
  if (!aLoadInfo) {
    // if there is no loadInfo, then there is nothing to serialize
    *aOptionalLoadInfoArgs = Nothing();
    return NS_OK;
  }

  nsresult rv = NS_OK;
  Maybe<PrincipalInfo> loadingPrincipalInfo;
  if (aLoadInfo->LoadingPrincipal()) {
    PrincipalInfo loadingPrincipalInfoTemp;
    rv = PrincipalToPrincipalInfo(aLoadInfo->LoadingPrincipal(),
                                  &loadingPrincipalInfoTemp);
    NS_ENSURE_SUCCESS(rv, rv);
    loadingPrincipalInfo = Some(loadingPrincipalInfoTemp);
  }

  PrincipalInfo triggeringPrincipalInfo;
  rv = PrincipalToPrincipalInfo(aLoadInfo->TriggeringPrincipal(),
                                &triggeringPrincipalInfo);
  NS_ENSURE_SUCCESS(rv, rv);

  Maybe<PrincipalInfo> principalToInheritInfo;
  if (aLoadInfo->PrincipalToInherit()) {
    PrincipalInfo principalToInheritInfoTemp;
    rv = PrincipalToPrincipalInfo(aLoadInfo->PrincipalToInherit(),
                                  &principalToInheritInfoTemp);
    NS_ENSURE_SUCCESS(rv, rv);
    principalToInheritInfo = Some(principalToInheritInfoTemp);
  }

  Maybe<PrincipalInfo> sandboxedLoadingPrincipalInfo;
  if (aLoadInfo->GetLoadingSandboxed()) {
    PrincipalInfo sandboxedLoadingPrincipalInfoTemp;
    rv = PrincipalToPrincipalInfo(aLoadInfo->GetSandboxedLoadingPrincipal(),
                                  &sandboxedLoadingPrincipalInfoTemp);
    NS_ENSURE_SUCCESS(rv, rv);
    sandboxedLoadingPrincipalInfo = Some(sandboxedLoadingPrincipalInfoTemp);
  }

  Maybe<PrincipalInfo> topLevelPrincipalInfo;
  if (aLoadInfo->GetTopLevelPrincipal()) {
    PrincipalInfo topLevelPrincipalInfoTemp;
    rv = PrincipalToPrincipalInfo(aLoadInfo->GetTopLevelPrincipal(),
                                  &topLevelPrincipalInfoTemp);
    NS_ENSURE_SUCCESS(rv, rv);
    topLevelPrincipalInfo = Some(topLevelPrincipalInfoTemp);
  }

  Maybe<PrincipalInfo> topLevelStorageAreaPrincipalInfo;
  if (aLoadInfo->GetTopLevelStorageAreaPrincipal()) {
    PrincipalInfo topLevelStorageAreaPrincipalInfoTemp;
    rv = PrincipalToPrincipalInfo(aLoadInfo->GetTopLevelStorageAreaPrincipal(),
                                  &topLevelStorageAreaPrincipalInfoTemp);
    NS_ENSURE_SUCCESS(rv, rv);
    topLevelStorageAreaPrincipalInfo =
        Some(topLevelStorageAreaPrincipalInfoTemp);
  }

  Maybe<URIParams> optionalResultPrincipalURI;
  nsCOMPtr<nsIURI> resultPrincipalURI;
  Unused << aLoadInfo->GetResultPrincipalURI(
      getter_AddRefs(resultPrincipalURI));
  if (resultPrincipalURI) {
    SerializeURI(resultPrincipalURI, optionalResultPrincipalURI);
  }

  nsTArray<RedirectHistoryEntryInfo> redirectChainIncludingInternalRedirects;
  for (const nsCOMPtr<nsIRedirectHistoryEntry>& redirectEntry :
       aLoadInfo->RedirectChainIncludingInternalRedirects()) {
    RedirectHistoryEntryInfo* entry =
        redirectChainIncludingInternalRedirects.AppendElement();
    rv = RHEntryToRHEntryInfo(redirectEntry, entry);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsTArray<RedirectHistoryEntryInfo> redirectChain;
  for (const nsCOMPtr<nsIRedirectHistoryEntry>& redirectEntry :
       aLoadInfo->RedirectChain()) {
    RedirectHistoryEntryInfo* entry = redirectChain.AppendElement();
    rv = RHEntryToRHEntryInfo(redirectEntry, entry);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsTArray<PrincipalInfo> ancestorPrincipals;
  ancestorPrincipals.SetCapacity(aLoadInfo->AncestorPrincipals().Length());
  for (const auto& principal : aLoadInfo->AncestorPrincipals()) {
    rv =
        PrincipalToPrincipalInfo(principal, ancestorPrincipals.AppendElement());
    NS_ENSURE_SUCCESS(rv, rv);
  }

  Maybe<IPCClientInfo> ipcClientInfo;
  const Maybe<ClientInfo>& clientInfo = aLoadInfo->GetClientInfo();
  if (clientInfo.isSome()) {
    ipcClientInfo.emplace(clientInfo.ref().ToIPC());
  }

  Maybe<IPCClientInfo> ipcReservedClientInfo;
  const Maybe<ClientInfo>& reservedClientInfo =
      aLoadInfo->GetReservedClientInfo();
  if (reservedClientInfo.isSome()) {
    ipcReservedClientInfo.emplace(reservedClientInfo.ref().ToIPC());
  }

  Maybe<IPCClientInfo> ipcInitialClientInfo;
  const Maybe<ClientInfo>& initialClientInfo =
      aLoadInfo->GetInitialClientInfo();
  if (initialClientInfo.isSome()) {
    ipcInitialClientInfo.emplace(initialClientInfo.ref().ToIPC());
  }

  Maybe<IPCServiceWorkerDescriptor> ipcController;
  const Maybe<ServiceWorkerDescriptor>& controller = aLoadInfo->GetController();
  if (controller.isSome()) {
    ipcController.emplace(controller.ref().ToIPC());
  }

  nsAutoString cspNonce;
  Unused << NS_WARN_IF(NS_FAILED(aLoadInfo->GetCspNonce(cspNonce)));

  nsCOMPtr<nsICookieSettings> cookieSettings;
  rv = aLoadInfo->GetCookieSettings(getter_AddRefs(cookieSettings));
  NS_ENSURE_SUCCESS(rv, rv);

  CookieSettingsArgs cookieSettingsArgs;
  static_cast<CookieSettings*>(cookieSettings.get())
      ->Serialize(cookieSettingsArgs);

  Maybe<CSPInfo> maybeCspToInheritInfo;
  nsCOMPtr<nsIContentSecurityPolicy> cspToInherit =
      aLoadInfo->GetCspToInherit();
  if (cspToInherit) {
    CSPInfo cspToInheritInfo;
    Unused << NS_WARN_IF(
        NS_FAILED(CSPToCSPInfo(cspToInherit, &cspToInheritInfo)));
    maybeCspToInheritInfo.emplace(cspToInheritInfo);
  }

  *aOptionalLoadInfoArgs = Some(LoadInfoArgs(
      loadingPrincipalInfo, triggeringPrincipalInfo, principalToInheritInfo,
      sandboxedLoadingPrincipalInfo, topLevelPrincipalInfo,
      topLevelStorageAreaPrincipalInfo, optionalResultPrincipalURI,
      aLoadInfo->GetSecurityFlags(), aLoadInfo->InternalContentPolicyType(),
      static_cast<uint32_t>(aLoadInfo->GetTainting()),
      aLoadInfo->GetBlockAllMixedContent(),
      aLoadInfo->GetUpgradeInsecureRequests(),
      aLoadInfo->GetBrowserUpgradeInsecureRequests(),
      aLoadInfo->GetBrowserWouldUpgradeInsecureRequests(),
      aLoadInfo->GetForceAllowDataURI(),
      aLoadInfo->GetAllowInsecureRedirectToDataURI(),
      aLoadInfo->GetBypassCORSChecks(),
      aLoadInfo->GetSkipContentPolicyCheckForWebRequest(),
      aLoadInfo->GetForceInheritPrincipalDropped(),
      aLoadInfo->GetInnerWindowID(), aLoadInfo->GetOuterWindowID(),
      aLoadInfo->GetParentOuterWindowID(), aLoadInfo->GetTopOuterWindowID(),
      aLoadInfo->GetFrameOuterWindowID(), aLoadInfo->GetBrowsingContextID(),
      aLoadInfo->GetFrameBrowsingContextID(),
      aLoadInfo->GetInitialSecurityCheckDone(),
      aLoadInfo->GetIsInThirdPartyContext(), aLoadInfo->GetIsDocshellReload(),
      aLoadInfo->GetIsFormSubmission(), aLoadInfo->GetSendCSPViolationEvents(),
      aLoadInfo->GetOriginAttributes(), redirectChainIncludingInternalRedirects,
      redirectChain, ancestorPrincipals, aLoadInfo->AncestorOuterWindowIDs(),
      ipcClientInfo, ipcReservedClientInfo, ipcInitialClientInfo, ipcController,
      aLoadInfo->CorsUnsafeHeaders(), aLoadInfo->GetForcePreflight(),
      aLoadInfo->GetIsPreflight(), aLoadInfo->GetLoadTriggeredFromExternal(),
      aLoadInfo->GetServiceWorkerTaintingSynthesized(),
      aLoadInfo->GetDocumentHasUserInteracted(),
      aLoadInfo->GetDocumentHasLoaded(), cspNonce,
      aLoadInfo->GetSkipContentSniffing(),
      aLoadInfo->GetIsFromProcessingFrameAttributes(), cookieSettingsArgs,
      aLoadInfo->GetRequestBlockingReason(), maybeCspToInheritInfo));

  return NS_OK;
}

nsresult LoadInfoArgsToLoadInfo(
    const Maybe<LoadInfoArgs>& aOptionalLoadInfoArgs,
    nsILoadInfo** outLoadInfo) {
  return LoadInfoArgsToLoadInfo(aOptionalLoadInfoArgs, nullptr, nullptr,
                                outLoadInfo);
}
nsresult LoadInfoArgsToLoadInfo(
    const Maybe<LoadInfoArgs>& aOptionalLoadInfoArgs, nsINode* aLoadingContext,
    nsINode* aCspToInheritLoadingContext, nsILoadInfo** outLoadInfo) {
  RefPtr<LoadInfo> loadInfo;
  nsresult rv = LoadInfoArgsToLoadInfo(aOptionalLoadInfoArgs, aLoadingContext,
                                       aCspToInheritLoadingContext,
                                       getter_AddRefs(loadInfo));
  NS_ENSURE_SUCCESS(rv, rv);

  loadInfo.forget(outLoadInfo);
  return NS_OK;
}

nsresult LoadInfoArgsToLoadInfo(
    const Maybe<LoadInfoArgs>& aOptionalLoadInfoArgs, LoadInfo** outLoadInfo) {
  return LoadInfoArgsToLoadInfo(aOptionalLoadInfoArgs, nullptr, nullptr,
                                outLoadInfo);
}
nsresult LoadInfoArgsToLoadInfo(
    const Maybe<LoadInfoArgs>& aOptionalLoadInfoArgs, nsINode* aLoadingContext,
    nsINode* aCspToInheritLoadingContext, LoadInfo** outLoadInfo) {
  if (aOptionalLoadInfoArgs.isNothing()) {
    *outLoadInfo = nullptr;
    return NS_OK;
  }

  const LoadInfoArgs& loadInfoArgs = aOptionalLoadInfoArgs.ref();

  nsresult rv = NS_OK;
  nsCOMPtr<nsIPrincipal> loadingPrincipal;
  if (loadInfoArgs.requestingPrincipalInfo().isSome()) {
    loadingPrincipal = PrincipalInfoToPrincipal(
        loadInfoArgs.requestingPrincipalInfo().ref(), &rv);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsIPrincipal> triggeringPrincipal =
      PrincipalInfoToPrincipal(loadInfoArgs.triggeringPrincipalInfo(), &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIPrincipal> principalToInherit;
  if (loadInfoArgs.principalToInheritInfo().isSome()) {
    principalToInherit = PrincipalInfoToPrincipal(
        loadInfoArgs.principalToInheritInfo().ref(), &rv);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsCOMPtr<nsIPrincipal> sandboxedLoadingPrincipal;
  if (loadInfoArgs.sandboxedLoadingPrincipalInfo().isSome()) {
    sandboxedLoadingPrincipal = PrincipalInfoToPrincipal(
        loadInfoArgs.sandboxedLoadingPrincipalInfo().ref(), &rv);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsCOMPtr<nsIPrincipal> topLevelPrincipal;
  if (loadInfoArgs.topLevelPrincipalInfo().isSome()) {
    topLevelPrincipal = PrincipalInfoToPrincipal(
        loadInfoArgs.topLevelPrincipalInfo().ref(), &rv);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsCOMPtr<nsIPrincipal> topLevelStorageAreaPrincipal;
  if (loadInfoArgs.topLevelStorageAreaPrincipalInfo().isSome()) {
    topLevelStorageAreaPrincipal = PrincipalInfoToPrincipal(
        loadInfoArgs.topLevelStorageAreaPrincipalInfo().ref(), &rv);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsCOMPtr<nsIURI> resultPrincipalURI;
  if (loadInfoArgs.resultPrincipalURI().isSome()) {
    resultPrincipalURI = DeserializeURI(loadInfoArgs.resultPrincipalURI());
    NS_ENSURE_TRUE(resultPrincipalURI, NS_ERROR_UNEXPECTED);
  }

  RedirectHistoryArray redirectChainIncludingInternalRedirects;
  for (const RedirectHistoryEntryInfo& entryInfo :
       loadInfoArgs.redirectChainIncludingInternalRedirects()) {
    nsCOMPtr<nsIRedirectHistoryEntry> redirectHistoryEntry =
        RHEntryInfoToRHEntry(entryInfo);
    NS_ENSURE_SUCCESS(rv, rv);
    redirectChainIncludingInternalRedirects.AppendElement(
        redirectHistoryEntry.forget());
  }

  RedirectHistoryArray redirectChain;
  for (const RedirectHistoryEntryInfo& entryInfo :
       loadInfoArgs.redirectChain()) {
    nsCOMPtr<nsIRedirectHistoryEntry> redirectHistoryEntry =
        RHEntryInfoToRHEntry(entryInfo);
    NS_ENSURE_SUCCESS(rv, rv);
    redirectChain.AppendElement(redirectHistoryEntry.forget());
  }

  nsTArray<nsCOMPtr<nsIPrincipal>> ancestorPrincipals;
  ancestorPrincipals.SetCapacity(loadInfoArgs.ancestorPrincipals().Length());
  for (const PrincipalInfo& principalInfo : loadInfoArgs.ancestorPrincipals()) {
    nsCOMPtr<nsIPrincipal> ancestorPrincipal =
        PrincipalInfoToPrincipal(principalInfo, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    ancestorPrincipals.AppendElement(ancestorPrincipal.forget());
  }

  Maybe<ClientInfo> clientInfo;
  if (loadInfoArgs.clientInfo().isSome()) {
    clientInfo.emplace(ClientInfo(loadInfoArgs.clientInfo().ref()));
  }

  Maybe<ClientInfo> reservedClientInfo;
  if (loadInfoArgs.reservedClientInfo().isSome()) {
    reservedClientInfo.emplace(
        ClientInfo(loadInfoArgs.reservedClientInfo().ref()));
  }

  Maybe<ClientInfo> initialClientInfo;
  if (loadInfoArgs.initialClientInfo().isSome()) {
    initialClientInfo.emplace(
        ClientInfo(loadInfoArgs.initialClientInfo().ref()));
  }

  // We can have an initial client info or a reserved client info, but not both.
  MOZ_DIAGNOSTIC_ASSERT(reservedClientInfo.isNothing() ||
                        initialClientInfo.isNothing());
  NS_ENSURE_TRUE(
      reservedClientInfo.isNothing() || initialClientInfo.isNothing(),
      NS_ERROR_UNEXPECTED);

  Maybe<ServiceWorkerDescriptor> controller;
  if (loadInfoArgs.controller().isSome()) {
    controller.emplace(
        ServiceWorkerDescriptor(loadInfoArgs.controller().ref()));
  }

  nsCOMPtr<nsICookieSettings> cookieSettings;
  CookieSettings::Deserialize(loadInfoArgs.cookieSettings(),
                              getter_AddRefs(cookieSettings));

  nsCOMPtr<nsIContentSecurityPolicy> cspToInherit;
  Maybe<mozilla::ipc::CSPInfo> cspToInheritInfo =
      loadInfoArgs.cspToInheritInfo();
  if (cspToInheritInfo.isSome()) {
    nsCOMPtr<Document> doc = do_QueryInterface(aCspToInheritLoadingContext);
    cspToInherit = CSPInfoToCSP(cspToInheritInfo.ref(), doc);
  }

  RefPtr<mozilla::LoadInfo> loadInfo = new mozilla::LoadInfo(
      loadingPrincipal, triggeringPrincipal, principalToInherit,
      sandboxedLoadingPrincipal, topLevelPrincipal,
      topLevelStorageAreaPrincipal, resultPrincipalURI, cookieSettings,
      cspToInherit, clientInfo, reservedClientInfo, initialClientInfo,
      controller, loadInfoArgs.securityFlags(),
      loadInfoArgs.contentPolicyType(),
      static_cast<LoadTainting>(loadInfoArgs.tainting()),
      loadInfoArgs.blockAllMixedContent(),
      loadInfoArgs.upgradeInsecureRequests(),
      loadInfoArgs.browserUpgradeInsecureRequests(),
      loadInfoArgs.browserWouldUpgradeInsecureRequests(),
      loadInfoArgs.forceAllowDataURI(),
      loadInfoArgs.allowInsecureRedirectToDataURI(),
      loadInfoArgs.bypassCORSChecks(),
      loadInfoArgs.skipContentPolicyCheckForWebRequest(),
      loadInfoArgs.forceInheritPrincipalDropped(), loadInfoArgs.innerWindowID(),
      loadInfoArgs.outerWindowID(), loadInfoArgs.parentOuterWindowID(),
      loadInfoArgs.topOuterWindowID(), loadInfoArgs.frameOuterWindowID(),
      loadInfoArgs.browsingContextID(), loadInfoArgs.frameBrowsingContextID(),
      loadInfoArgs.initialSecurityCheckDone(),
      loadInfoArgs.isInThirdPartyContext(), loadInfoArgs.isDocshellReload(),
      loadInfoArgs.isFormSubmission(), loadInfoArgs.sendCSPViolationEvents(),
      loadInfoArgs.originAttributes(), redirectChainIncludingInternalRedirects,
      redirectChain, std::move(ancestorPrincipals),
      loadInfoArgs.ancestorOuterWindowIDs(), loadInfoArgs.corsUnsafeHeaders(),
      loadInfoArgs.forcePreflight(), loadInfoArgs.isPreflight(),
      loadInfoArgs.loadTriggeredFromExternal(),
      loadInfoArgs.serviceWorkerTaintingSynthesized(),
      loadInfoArgs.documentHasUserInteracted(),
      loadInfoArgs.documentHasLoaded(), loadInfoArgs.cspNonce(),
      loadInfoArgs.skipContentSniffing(), loadInfoArgs.requestBlockingReason(),
      aLoadingContext);

  if (loadInfoArgs.isFromProcessingFrameAttributes()) {
    loadInfo->SetIsFromProcessingFrameAttributes();
  }

  loadInfo.forget(outLoadInfo);
  return NS_OK;
}

void LoadInfoToParentLoadInfoForwarder(
    nsILoadInfo* aLoadInfo, ParentLoadInfoForwarderArgs* aForwarderArgsOut) {
  if (!aLoadInfo) {
    *aForwarderArgsOut = ParentLoadInfoForwarderArgs(
        false, false, Nothing(), nsILoadInfo::TAINTING_BASIC,
        false,  // SkipContentSniffing
        false,  // serviceWorkerTaintingSynthesized
        false,  // documentHasUserInteracted
        false,  // documentHasLoaded
        Maybe<CookieSettingsArgs>(),
        nsILoadInfo::BLOCKING_REASON_NONE);  // requestBlockingReason
    return;
  }

  Maybe<IPCServiceWorkerDescriptor> ipcController;
  Maybe<ServiceWorkerDescriptor> controller(aLoadInfo->GetController());
  if (controller.isSome()) {
    ipcController.emplace(controller.ref().ToIPC());
  }

  uint32_t tainting = nsILoadInfo::TAINTING_BASIC;
  Unused << aLoadInfo->GetTainting(&tainting);

  Maybe<CookieSettingsArgs> cookieSettingsArgs;

  nsCOMPtr<nsICookieSettings> cookieSettings;
  nsresult rv = aLoadInfo->GetCookieSettings(getter_AddRefs(cookieSettings));
  CookieSettings* cs = static_cast<CookieSettings*>(cookieSettings.get());
  if (NS_SUCCEEDED(rv) && cookieSettings && cs->HasBeenChanged()) {
    CookieSettingsArgs args;
    cs->Serialize(args);
    cookieSettingsArgs = Some(args);
  }

  *aForwarderArgsOut = ParentLoadInfoForwarderArgs(
      aLoadInfo->GetAllowInsecureRedirectToDataURI(),
      aLoadInfo->GetBypassCORSChecks(), ipcController, tainting,
      aLoadInfo->GetSkipContentSniffing(),
      aLoadInfo->GetServiceWorkerTaintingSynthesized(),
      aLoadInfo->GetDocumentHasUserInteracted(),
      aLoadInfo->GetDocumentHasLoaded(), cookieSettingsArgs,
      aLoadInfo->GetRequestBlockingReason());
}

nsresult MergeParentLoadInfoForwarder(
    ParentLoadInfoForwarderArgs const& aForwarderArgs, nsILoadInfo* aLoadInfo) {
  if (!aLoadInfo) {
    return NS_OK;
  }

  nsresult rv;

  rv = aLoadInfo->SetAllowInsecureRedirectToDataURI(
      aForwarderArgs.allowInsecureRedirectToDataURI());
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aLoadInfo->SetBypassCORSChecks(aForwarderArgs.bypassCORSChecks());
  NS_ENSURE_SUCCESS(rv, rv);

  aLoadInfo->ClearController();
  auto& controller = aForwarderArgs.controller();
  if (controller.isSome()) {
    aLoadInfo->SetController(ServiceWorkerDescriptor(controller.ref()));
  }

  if (aForwarderArgs.serviceWorkerTaintingSynthesized()) {
    aLoadInfo->SynthesizeServiceWorkerTainting(
        static_cast<LoadTainting>(aForwarderArgs.tainting()));
  } else {
    aLoadInfo->MaybeIncreaseTainting(aForwarderArgs.tainting());
  }

  rv = aLoadInfo->SetSkipContentSniffing(aForwarderArgs.skipContentSniffing());
  NS_ENSURE_SUCCESS(rv, rv);

  MOZ_ALWAYS_SUCCEEDS(aLoadInfo->SetDocumentHasUserInteracted(
      aForwarderArgs.documentHasUserInteracted()));
  MOZ_ALWAYS_SUCCEEDS(
      aLoadInfo->SetDocumentHasLoaded(aForwarderArgs.documentHasLoaded()));
  MOZ_ALWAYS_SUCCEEDS(aLoadInfo->SetRequestBlockingReason(
      aForwarderArgs.requestBlockingReason()));

  const Maybe<CookieSettingsArgs>& cookieSettingsArgs =
      aForwarderArgs.cookieSettings();
  if (cookieSettingsArgs.isSome()) {
    nsCOMPtr<nsICookieSettings> cookieSettings;
    nsresult rv = aLoadInfo->GetCookieSettings(getter_AddRefs(cookieSettings));
    if (NS_SUCCEEDED(rv) && cookieSettings) {
      static_cast<CookieSettings*>(cookieSettings.get())
          ->Merge(cookieSettingsArgs.ref());
    }
  }

  return NS_OK;
}

void LoadInfoToChildLoadInfoForwarder(
    nsILoadInfo* aLoadInfo, ChildLoadInfoForwarderArgs* aForwarderArgsOut) {
  if (!aLoadInfo) {
    *aForwarderArgsOut =
        ChildLoadInfoForwarderArgs(Nothing(), Nothing(), Nothing(), 0);
    return;
  }

  Maybe<IPCClientInfo> ipcReserved;
  Maybe<ClientInfo> reserved(aLoadInfo->GetReservedClientInfo());
  if (reserved.isSome()) {
    ipcReserved.emplace(reserved.ref().ToIPC());
  }

  Maybe<IPCClientInfo> ipcInitial;
  Maybe<ClientInfo> initial(aLoadInfo->GetInitialClientInfo());
  if (initial.isSome()) {
    ipcInitial.emplace(initial.ref().ToIPC());
  }

  Maybe<IPCServiceWorkerDescriptor> ipcController;
  Maybe<ServiceWorkerDescriptor> controller(aLoadInfo->GetController());
  if (controller.isSome()) {
    ipcController.emplace(controller.ref().ToIPC());
  }

  *aForwarderArgsOut =
      ChildLoadInfoForwarderArgs(ipcReserved, ipcInitial, ipcController,
                                 aLoadInfo->GetRequestBlockingReason());
}

nsresult MergeChildLoadInfoForwarder(
    const ChildLoadInfoForwarderArgs& aForwarderArgs, nsILoadInfo* aLoadInfo) {
  if (!aLoadInfo) {
    return NS_OK;
  }

  Maybe<ClientInfo> reservedClientInfo;
  auto& ipcReserved = aForwarderArgs.reservedClientInfo();
  if (ipcReserved.isSome()) {
    reservedClientInfo.emplace(ClientInfo(ipcReserved.ref()));
  }

  Maybe<ClientInfo> initialClientInfo;
  auto& ipcInitial = aForwarderArgs.initialClientInfo();
  if (ipcInitial.isSome()) {
    initialClientInfo.emplace(ClientInfo(ipcInitial.ref()));
  }

  // There should only be at most one reserved or initial ClientInfo.
  if (NS_WARN_IF(reservedClientInfo.isSome() && initialClientInfo.isSome())) {
    return NS_ERROR_FAILURE;
  }

  // If we received no reserved or initial ClientInfo, then we must not
  // already have one set.  There are no use cases where this should
  // happen and we don't have a way to clear the current value.
  if (NS_WARN_IF(reservedClientInfo.isNothing() &&
                 initialClientInfo.isNothing() &&
                 (aLoadInfo->GetReservedClientInfo().isSome() ||
                  aLoadInfo->GetInitialClientInfo().isSome()))) {
    return NS_ERROR_FAILURE;
  }

  if (reservedClientInfo.isSome()) {
    // We need to override here instead of simply set the value.  This
    // allows us to change the reserved client.  This is necessary when
    // the ClientChannelHelper created a new reserved client in the
    // child-side of the redirect.
    aLoadInfo->OverrideReservedClientInfoInParent(reservedClientInfo.ref());
  } else if (initialClientInfo.isSome()) {
    aLoadInfo->SetInitialClientInfo(initialClientInfo.ref());
  }

  aLoadInfo->ClearController();
  auto& controller = aForwarderArgs.controller();
  if (controller.isSome()) {
    aLoadInfo->SetController(ServiceWorkerDescriptor(controller.ref()));
  }

  uint32_t blockingReason = aForwarderArgs.requestBlockingReason();
  if (blockingReason) {
    // We only want to override when non-null, so that any earlier set non-null
    // value is not reverted to 0.
    aLoadInfo->SetRequestBlockingReason(blockingReason);
  }

  return NS_OK;
}

}  // namespace ipc
}  // namespace mozilla
