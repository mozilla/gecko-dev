/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ContentPrincipal.h"

#include "mozIThirdPartyUtil.h"
#include "nsContentUtils.h"
#include "nscore.h"
#include "nsScriptSecurityManager.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "pratom.h"
#include "nsIURI.h"
#include "nsIURL.h"
#include "nsIStandardURL.h"
#include "nsIURIWithSpecialOrigin.h"
#include "nsIURIMutator.h"
#include "nsJSPrincipals.h"
#include "nsIEffectiveTLDService.h"
#include "nsIClassInfoImpl.h"
#include "nsIObjectInputStream.h"
#include "nsIObjectOutputStream.h"
#include "nsIProtocolHandler.h"
#include "nsError.h"
#include "nsIContentSecurityPolicy.h"
#include "nsNetCID.h"
#include "js/Wrapper.h"

#include "mozilla/dom/BlobURLProtocolHandler.h"
#include "mozilla/dom/nsCSPContext.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/ExtensionPolicyService.h"
#include "mozilla/Preferences.h"
#include "mozilla/HashFunctions.h"

using namespace mozilla;

static inline ExtensionPolicyService& EPS() {
  return ExtensionPolicyService::GetSingleton();
}

NS_IMPL_CLASSINFO(ContentPrincipal, nullptr, nsIClassInfo::MAIN_THREAD_ONLY,
                  NS_PRINCIPAL_CID)
NS_IMPL_QUERY_INTERFACE_CI(ContentPrincipal, nsIPrincipal, nsISerializable)
NS_IMPL_CI_INTERFACE_GETTER(ContentPrincipal, nsIPrincipal, nsISerializable)

ContentPrincipal::ContentPrincipal() : BasePrincipal(eCodebasePrincipal) {}

ContentPrincipal::~ContentPrincipal() {
  // let's clear the principal within the csp to avoid a tangling pointer
  if (mCSP) {
    static_cast<nsCSPContext*>(mCSP.get())->clearLoadingPrincipal();
  }
}

nsresult ContentPrincipal::Init(nsIURI* aCodebase,
                                const OriginAttributes& aOriginAttributes,
                                const nsACString& aOriginNoSuffix) {
  NS_ENSURE_ARG(aCodebase);

  // Assert that the URI we get here isn't any of the schemes that we know we
  // should not get here.  These schemes always either inherit their principal
  // or fall back to a null principal.  These are schemes which return
  // URI_INHERITS_SECURITY_CONTEXT from their protocol handler's
  // GetProtocolFlags function.
  bool hasFlag;
  Unused << hasFlag;  // silence possible compiler warnings.
  MOZ_DIAGNOSTIC_ASSERT(
      NS_SUCCEEDED(NS_URIChainHasFlags(
          aCodebase, nsIProtocolHandler::URI_INHERITS_SECURITY_CONTEXT,
          &hasFlag)) &&
      !hasFlag);

  mCodebase = aCodebase;
  FinishInit(aOriginNoSuffix, aOriginAttributes);

  return NS_OK;
}

nsresult ContentPrincipal::GetScriptLocation(nsACString& aStr) {
  return mCodebase->GetSpec(aStr);
}

/* static */ nsresult ContentPrincipal::GenerateOriginNoSuffixFromURI(
    nsIURI* aURI, nsACString& aOriginNoSuffix) {
  if (!aURI) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIURI> origin = NS_GetInnermostURI(aURI);
  if (!origin) {
    return NS_ERROR_FAILURE;
  }

  MOZ_ASSERT(!NS_IsAboutBlank(origin),
             "The inner URI for about:blank must be moz-safe-about:blank");

  // Handle non-strict file:// uris.
  if (!nsScriptSecurityManager::GetStrictFileOriginPolicy() &&
      NS_URIIsLocalFile(origin)) {
    // If strict file origin policy is not in effect, all local files are
    // considered to be same-origin, so return a known dummy origin here.
    aOriginNoSuffix.AssignLiteral("file://UNIVERSAL_FILE_URI_ORIGIN");
    return NS_OK;
  }

  nsresult rv;
// NB: This is only compiled for Thunderbird/Suite.
#if IS_ORIGIN_IS_FULL_SPEC_DEFINED
  bool fullSpec = false;
  rv = NS_URIChainHasFlags(origin, nsIProtocolHandler::ORIGIN_IS_FULL_SPEC,
                           &fullSpec);
  NS_ENSURE_SUCCESS(rv, rv);
  if (fullSpec) {
    return origin->GetAsciiSpec(aOriginNoSuffix);
  }
#endif

  // We want the invariant that prinA.origin == prinB.origin i.f.f.
  // prinA.equals(prinB). However, this requires that we impose certain
  // constraints on the behavior and origin semantics of principals, and in
  // particular, forbid creating origin strings for principals whose equality
  // constraints are not expressible as strings (i.e. object equality).
  // Moreover, we want to forbid URIs containing the magic "^" we use as a
  // separating character for origin attributes.
  //
  // These constraints can generally be achieved by restricting .origin to
  // nsIStandardURL-based URIs, but there are a few other URI schemes that we
  // need to handle.
  bool isBehaved;
  if ((NS_SUCCEEDED(origin->SchemeIs("about", &isBehaved)) && isBehaved) ||
      (NS_SUCCEEDED(origin->SchemeIs("moz-safe-about", &isBehaved)) &&
       isBehaved &&
       // We generally consider two about:foo origins to be same-origin, but
       // about:blank is special since it can be generated from different
       // sources. We check for moz-safe-about:blank since origin is an
       // innermost URI.
       !origin->GetSpecOrDefault().EqualsLiteral("moz-safe-about:blank")) ||
      (NS_SUCCEEDED(origin->SchemeIs("indexeddb", &isBehaved)) && isBehaved)) {
    rv = origin->GetAsciiSpec(aOriginNoSuffix);
    NS_ENSURE_SUCCESS(rv, rv);

    int32_t pos = aOriginNoSuffix.FindChar('?');
    int32_t hashPos = aOriginNoSuffix.FindChar('#');

    if (hashPos != kNotFound && (pos == kNotFound || hashPos < pos)) {
      pos = hashPos;
    }

    if (pos != kNotFound) {
      aOriginNoSuffix.Truncate(pos);
    }

    // These URIs could technically contain a '^', but they never should.
    if (NS_WARN_IF(aOriginNoSuffix.FindChar('^', 0) != -1)) {
      aOriginNoSuffix.Truncate();
      return NS_ERROR_FAILURE;
    }
    return NS_OK;
  }

  // This URL can be a blobURL. In this case, we should use the 'parent'
  // principal instead.
  nsCOMPtr<nsIPrincipal> blobPrincipal;
  if (dom::BlobURLProtocolHandler::GetBlobURLPrincipal(
          origin, getter_AddRefs(blobPrincipal))) {
    MOZ_ASSERT(blobPrincipal);
    return blobPrincipal->GetOriginNoSuffix(aOriginNoSuffix);
  }

  // If we reached this branch, we can only create an origin if we have a
  // nsIStandardURL.  So, we query to a nsIStandardURL, and fail if we aren't
  // an instance of an nsIStandardURL nsIStandardURLs have the good property
  // of escaping the '^' character in their specs, which means that we can be
  // sure that the caret character (which is reserved for delimiting the end
  // of the spec, and the beginning of the origin attributes) is not present
  // in the origin string
  nsCOMPtr<nsIStandardURL> standardURL = do_QueryInterface(origin);
  if (!standardURL) {
    return NS_ERROR_FAILURE;
  }

  // See whether we have a useful hostPort. If we do, use that.
  nsAutoCString hostPort;
  bool isChrome = false;
  rv = origin->SchemeIs("chrome", &isChrome);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!isChrome) {
    rv = origin->GetAsciiHostPort(hostPort);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  if (!hostPort.IsEmpty()) {
    rv = origin->GetScheme(aOriginNoSuffix);
    NS_ENSURE_SUCCESS(rv, rv);
    aOriginNoSuffix.AppendLiteral("://");
    aOriginNoSuffix.Append(hostPort);
    return NS_OK;
  }

  rv = aURI->GetAsciiSpec(aOriginNoSuffix);
  NS_ENSURE_SUCCESS(rv, rv);

  // The origin, when taken from the spec, should not contain the ref part of
  // the URL.

  int32_t pos = aOriginNoSuffix.FindChar('?');
  int32_t hashPos = aOriginNoSuffix.FindChar('#');

  if (hashPos != kNotFound && (pos == kNotFound || hashPos < pos)) {
    pos = hashPos;
  }

  if (pos != kNotFound) {
    aOriginNoSuffix.Truncate(pos);
  }

  return NS_OK;
}

bool ContentPrincipal::SubsumesInternal(
    nsIPrincipal* aOther,
    BasePrincipal::DocumentDomainConsideration aConsideration) {
  MOZ_ASSERT(aOther);

  // For ContentPrincipal, Subsumes is equivalent to Equals.
  if (aOther == this) {
    return true;
  }

  // If either the subject or the object has changed its principal by
  // explicitly setting document.domain then the other must also have
  // done so in order to be considered the same origin. This prevents
  // DNS spoofing based on document.domain (154930)
  nsresult rv;
  if (aConsideration == ConsiderDocumentDomain) {
    // Get .domain on each principal.
    nsCOMPtr<nsIURI> thisDomain, otherDomain;
    GetDomain(getter_AddRefs(thisDomain));
    aOther->GetDomain(getter_AddRefs(otherDomain));

    // If either has .domain set, we have equality i.f.f. the domains match.
    // Otherwise, we fall through to the non-document-domain-considering case.
    if (thisDomain || otherDomain) {
      bool isMatch =
          nsScriptSecurityManager::SecurityCompareURIs(thisDomain, otherDomain);
#ifdef DEBUG
      if (isMatch) {
        nsAutoCString thisSiteOrigin, otherSiteOrigin;
        MOZ_ALWAYS_SUCCEEDS(GetSiteOrigin(thisSiteOrigin));
        MOZ_ALWAYS_SUCCEEDS(aOther->GetSiteOrigin(otherSiteOrigin));
        MOZ_ASSERT(
            thisSiteOrigin == otherSiteOrigin,
            "SubsumesConsideringDomain passed with mismatched siteOrigin!");
      }
#endif
      return isMatch;
    }
  }

  nsCOMPtr<nsIURI> otherURI;
  rv = aOther->GetURI(getter_AddRefs(otherURI));
  NS_ENSURE_SUCCESS(rv, false);

  // Compare codebases.
  return nsScriptSecurityManager::SecurityCompareURIs(mCodebase, otherURI);
}

NS_IMETHODIMP
ContentPrincipal::GetURI(nsIURI** aURI) {
  NS_ADDREF(*aURI = mCodebase);
  return NS_OK;
}

bool ContentPrincipal::MayLoadInternal(nsIURI* aURI) {
  MOZ_ASSERT(aURI);

#if defined(MOZ_THUNDERBIRD) || defined(MOZ_SUITE)
  nsCOMPtr<nsIURIWithSpecialOrigin> uriWithSpecialOrigin =
      do_QueryInterface(aURI);
  if (uriWithSpecialOrigin) {
    nsCOMPtr<nsIURI> origin;
    nsresult rv = uriWithSpecialOrigin->GetOrigin(getter_AddRefs(origin));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return false;
    }
    MOZ_ASSERT(origin);
    OriginAttributes attrs;
    RefPtr<BasePrincipal> principal =
        BasePrincipal::CreateCodebasePrincipal(origin, attrs);
    return nsIPrincipal::Subsumes(principal);
  }
#endif

  nsCOMPtr<nsIPrincipal> blobPrincipal;
  if (dom::BlobURLProtocolHandler::GetBlobURLPrincipal(
          aURI, getter_AddRefs(blobPrincipal))) {
    MOZ_ASSERT(blobPrincipal);
    return nsIPrincipal::Subsumes(blobPrincipal);
  }

  // If this principal is associated with an addon, check whether that addon
  // has been given permission to load from this domain.
  if (AddonAllowsLoad(aURI)) {
    return true;
  }

  if (nsScriptSecurityManager::SecurityCompareURIs(mCodebase, aURI)) {
    return true;
  }

  // If strict file origin policy is in effect, local files will always fail
  // SecurityCompareURIs unless they are identical. Explicitly check file origin
  // policy, in that case.
  if (nsScriptSecurityManager::GetStrictFileOriginPolicy() &&
      NS_URIIsLocalFile(aURI) &&
      NS_RelaxStrictFileOriginPolicy(aURI, mCodebase)) {
    return true;
  }

  return false;
}

uint32_t ContentPrincipal::GetHashValue() {
  MOZ_ASSERT(mCodebase, "Need a codebase");

  return nsScriptSecurityManager::HashPrincipalByOrigin(this);
}

NS_IMETHODIMP
ContentPrincipal::GetDomain(nsIURI** aDomain) {
  if (!mDomain) {
    *aDomain = nullptr;
    return NS_OK;
  }

  NS_ADDREF(*aDomain = mDomain);
  return NS_OK;
}

NS_IMETHODIMP
ContentPrincipal::SetDomain(nsIURI* aDomain) {
  MOZ_ASSERT(aDomain);

  mDomain = aDomain;
  SetHasExplicitDomain();

  // Recompute all wrappers between compartments using this principal and other
  // non-chrome compartments.
  AutoSafeJSContext cx;
  JSPrincipals* principals =
      nsJSPrincipals::get(static_cast<nsIPrincipal*>(this));
  bool success =
      js::RecomputeWrappers(cx, js::ContentCompartmentsOnly(),
                            js::CompartmentsWithPrincipals(principals));
  NS_ENSURE_TRUE(success, NS_ERROR_FAILURE);
  success =
      js::RecomputeWrappers(cx, js::CompartmentsWithPrincipals(principals),
                            js::ContentCompartmentsOnly());
  NS_ENSURE_TRUE(success, NS_ERROR_FAILURE);

  // Set the changed-document-domain flag on compartments containing realms
  // using this principal.
  auto cb = [](JSContext*, void*, JS::Handle<JS::Realm*> aRealm) {
    JS::Compartment* comp = JS::GetCompartmentForRealm(aRealm);
    xpc::SetCompartmentChangedDocumentDomain(comp);
  };
  JS::IterateRealmsWithPrincipals(cx, principals, nullptr, cb);

  return NS_OK;
}

static nsresult GetSpecialBaseDomain(const nsCOMPtr<nsIURI>& aCodebase,
                                     bool* aHandled, nsACString& aBaseDomain) {
  *aHandled = false;

  // Special handling for a file URI.
  if (NS_URIIsLocalFile(aCodebase)) {
    // If strict file origin policy is not in effect, all local files are
    // considered to be same-origin, so return a known dummy domain here.
    if (!nsScriptSecurityManager::GetStrictFileOriginPolicy()) {
      *aHandled = true;
      aBaseDomain.AssignLiteral("UNIVERSAL_FILE_URI_ORIGIN");
      return NS_OK;
    }

    // Otherwise, we return the file path.
    nsCOMPtr<nsIURL> url = do_QueryInterface(aCodebase);

    if (url) {
      *aHandled = true;
      return url->GetFilePath(aBaseDomain);
    }
  }

  bool hasNoRelativeFlag;
  nsresult rv = NS_URIChainHasFlags(
      aCodebase, nsIProtocolHandler::URI_NORELATIVE, &hasNoRelativeFlag);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (hasNoRelativeFlag) {
    *aHandled = true;
    return aCodebase->GetSpec(aBaseDomain);
  }

  return NS_OK;
}

NS_IMETHODIMP
ContentPrincipal::GetBaseDomain(nsACString& aBaseDomain) {
  // Handle some special URIs first.
  bool handled;
  nsresult rv = GetSpecialBaseDomain(mCodebase, &handled, aBaseDomain);
  NS_ENSURE_SUCCESS(rv, rv);

  if (handled) {
    return NS_OK;
  }

  // For everything else, we ask the TLD service via the ThirdPartyUtil.
  nsCOMPtr<mozIThirdPartyUtil> thirdPartyUtil =
      do_GetService(THIRDPARTYUTIL_CONTRACTID);
  if (!thirdPartyUtil) {
    return NS_ERROR_FAILURE;
  }

  return thirdPartyUtil->GetBaseDomain(mCodebase, aBaseDomain);
}

NS_IMETHODIMP
ContentPrincipal::GetSiteOrigin(nsACString& aSiteOrigin) {
  // Handle some special URIs first.
  nsAutoCString baseDomain;
  bool handled;
  nsresult rv = GetSpecialBaseDomain(mCodebase, &handled, baseDomain);
  NS_ENSURE_SUCCESS(rv, rv);

  if (handled) {
    // This is a special URI ("file:", "about:", "view-source:", etc). Just
    // return the origin.
    return GetOrigin(aSiteOrigin);
  }

  // For everything else, we ask the TLD service. Note that, unlike in
  // GetBaseDomain, we don't use ThirdPartyUtil.getBaseDomain because if the
  // host is an IP address that returns the raw address and we can't use it with
  // SetHost below because SetHost expects '[' and ']' around IPv6 addresses.
  // See bug 1491728.
  nsCOMPtr<nsIEffectiveTLDService> tldService =
      do_GetService(NS_EFFECTIVETLDSERVICE_CONTRACTID);
  if (!tldService) {
    return NS_ERROR_FAILURE;
  }

  bool gotBaseDomain = false;
  rv = tldService->GetBaseDomain(mCodebase, 0, baseDomain);
  if (NS_SUCCEEDED(rv)) {
    gotBaseDomain = true;
  } else {
    // If this is an IP address or something like "localhost", we just continue
    // with gotBaseDomain = false.
    if (rv != NS_ERROR_HOST_IS_IP_ADDRESS &&
        rv != NS_ERROR_INSUFFICIENT_DOMAIN_LEVELS) {
      return rv;
    }
  }

  // NOTE: Calling `SetHostPort` with a portless domain is insufficient to clear
  // the port, so an extra `SetPort` call has to be made.
  nsCOMPtr<nsIURI> siteUri;
  NS_MutateURI mutator(mCodebase);
  mutator.SetUserPass(EmptyCString()).SetPort(-1);
  if (gotBaseDomain) {
    mutator.SetHost(baseDomain);
  }
  rv = mutator.Finalize(siteUri);
  MOZ_ASSERT(NS_SUCCEEDED(rv), "failed to create siteUri");
  NS_ENSURE_SUCCESS(rv, rv);

  rv = GenerateOriginNoSuffixFromURI(siteUri, aSiteOrigin);
  MOZ_ASSERT(NS_SUCCEEDED(rv), "failed to create siteOriginNoSuffix");
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString suffix;
  rv = GetOriginSuffix(suffix);
  MOZ_ASSERT(NS_SUCCEEDED(rv), "failed to create suffix");
  NS_ENSURE_SUCCESS(rv, rv);

  aSiteOrigin.Append(suffix);
  return NS_OK;
}

nsresult ContentPrincipal::GetSiteIdentifier(SiteIdentifier& aSite) {
  nsCString siteOrigin;
  nsresult rv = GetSiteOrigin(siteOrigin);
  NS_ENSURE_SUCCESS(rv, rv);

  RefPtr<BasePrincipal> principal = CreateCodebasePrincipal(siteOrigin);
  if (!principal) {
    NS_WARNING("could not instantiate codebase principal");
    return NS_ERROR_FAILURE;
  }

  aSite.Init(principal);
  return NS_OK;
}

WebExtensionPolicy* ContentPrincipal::AddonPolicy() {
  if (!mAddon.isSome()) {
    NS_ENSURE_TRUE(mCodebase, nullptr);

    bool isMozExt;
    if (NS_SUCCEEDED(mCodebase->SchemeIs("moz-extension", &isMozExt)) &&
        isMozExt) {
      mAddon.emplace(EPS().GetByURL(mCodebase.get()));
    } else {
      mAddon.emplace(nullptr);
    }
  }

  return mAddon.value();
}

NS_IMETHODIMP
ContentPrincipal::GetAddonId(nsAString& aAddonId) {
  auto policy = AddonPolicy();
  if (policy) {
    policy->GetId(aAddonId);
  } else {
    aAddonId.Truncate();
  }
  return NS_OK;
}

NS_IMETHODIMP
ContentPrincipal::Read(nsIObjectInputStream* aStream) {
  nsCOMPtr<nsISupports> supports;
  nsCOMPtr<nsIURI> codebase;
  nsresult rv = NS_ReadOptionalObject(aStream, true, getter_AddRefs(supports));
  if (NS_FAILED(rv)) {
    return rv;
  }

  codebase = do_QueryInterface(supports);
  // Enforce re-parsing about: URIs so that if they change, we continue to use
  // their new principals correctly.
  bool isAbout = false;
  if (NS_SUCCEEDED(codebase->SchemeIs("about", &isAbout)) && isAbout) {
    nsAutoCString spec;
    codebase->GetSpec(spec);
    NS_ENSURE_SUCCESS(NS_NewURI(getter_AddRefs(codebase), spec),
                      NS_ERROR_FAILURE);
  }

  nsCOMPtr<nsIURI> domain;
  rv = NS_ReadOptionalObject(aStream, true, getter_AddRefs(supports));
  if (NS_FAILED(rv)) {
    return rv;
  }

  domain = do_QueryInterface(supports);

  nsAutoCString suffix;
  rv = aStream->ReadCString(suffix);
  NS_ENSURE_SUCCESS(rv, rv);

  OriginAttributes attrs;
  bool ok = attrs.PopulateFromSuffix(suffix);
  NS_ENSURE_TRUE(ok, NS_ERROR_FAILURE);

  rv = NS_ReadOptionalObject(aStream, true, getter_AddRefs(supports));
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString originNoSuffix;
  rv = GenerateOriginNoSuffixFromURI(codebase, originNoSuffix);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = Init(codebase, attrs, originNoSuffix);
  NS_ENSURE_SUCCESS(rv, rv);

  mCSP = do_QueryInterface(supports, &rv);
  // make sure setRequestContext is called after Init(),
  // to make sure  the principals URI been initalized.
  if (mCSP) {
    mCSP->SetRequestContext(nullptr, this);
  }

  // Note: we don't call SetDomain here because we don't need the wrapper
  // recomputation code there (we just created this principal).
  mDomain = domain;
  if (mDomain) {
    SetHasExplicitDomain();
  }

  return NS_OK;
}

NS_IMETHODIMP
ContentPrincipal::Write(nsIObjectOutputStream* aStream) {
  NS_ENSURE_STATE(mCodebase);
  nsresult rv = NS_WriteOptionalCompoundObject(aStream, mCodebase,
                                               NS_GET_IID(nsIURI), true);
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = NS_WriteOptionalCompoundObject(aStream, mDomain, NS_GET_IID(nsIURI),
                                      true);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsAutoCString suffix;
  OriginAttributesRef().CreateSuffix(suffix);

  rv = aStream->WriteStringZ(suffix.get());
  NS_ENSURE_SUCCESS(rv, rv);

  rv = NS_WriteOptionalCompoundObject(
      aStream, mCSP, NS_GET_IID(nsIContentSecurityPolicy), true);
  if (NS_FAILED(rv)) {
    return rv;
  }

  return NS_OK;
}
