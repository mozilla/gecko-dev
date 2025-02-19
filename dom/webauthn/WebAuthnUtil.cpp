/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "hasht.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/dom/WebAuthnUtil.h"
#include "mozpkix/pkixutil.h"
#include "nsComponentManagerUtils.h"
#include "nsHTMLDocument.h"
#include "nsICryptoHash.h"
#include "nsIEffectiveTLDService.h"
#include "nsIURIMutator.h"
#include "nsNetUtil.h"

namespace mozilla::dom {

bool IsValidAppId(const nsCOMPtr<nsIPrincipal>& aPrincipal,
                  const nsCString& aAppId) {
  // An AppID is a substitute for the RP ID that allows the caller to assert
  // credentials that were created using the legacy U2F protocol. While an RP ID
  // is the caller origin's effective domain, or a registrable suffix thereof,
  // an AppID is a URL (with a scheme and a possibly non-empty path) that is
  // same-site with the caller's origin.
  //
  // The U2F protocol nominally uses Algorithm 3.1.2 of [1] to validate AppIDs.
  // However, the WebAuthn spec [2] notes that it is not necessary to "implement
  // steps four and onward of" Algorithm 3.1.2. Instead, in step three, "the
  // comparison on the host is relaxed to accept hosts on the same site." Step
  // two is best seen as providing a default value for the AppId when one is not
  // provided. That leaves step 1 and the same-site check, which is what we
  // implement here.
  //
  // [1]
  // https://fidoalliance.org/specs/fido-v2.0-id-20180227/fido-appid-and-facets-v2.0-id-20180227.html#determining-if-a-caller-s-facetid-is-authorized-for-an-appid
  // [2] https://w3c.github.io/webauthn/#sctn-appid-extension

  auto* principal = BasePrincipal::Cast(aPrincipal);
  nsCOMPtr<nsIURI> callerUri;
  nsresult rv = principal->GetURI(getter_AddRefs(callerUri));
  if (NS_FAILED(rv)) {
    return false;
  }

  nsCOMPtr<nsIURI> appIdUri;
  rv = NS_NewURI(getter_AddRefs(appIdUri), aAppId);
  if (NS_FAILED(rv)) {
    return false;
  }

  // Step 1 of Algorithm 3.1.2. "If the AppID is not an HTTPS URL, and matches
  // the FacetID of the caller, no additional processing is necessary and the
  // operation may proceed." In the web context, the "FacetID" is defined as
  // "the Web Origin [RFC6454] of the web page triggering the FIDO operation,
  // written as a URI with an empty path. Default ports are omitted and any path
  // component is ignored."
  if (!appIdUri->SchemeIs("https")) {
    nsCString facetId;
    rv = principal->GetWebExposedOriginSerialization(facetId);
    return NS_SUCCEEDED(rv) && facetId == aAppId;
  }

  // Same site check
  nsCOMPtr<nsIEffectiveTLDService> tldService =
      do_GetService(NS_EFFECTIVETLDSERVICE_CONTRACTID);
  if (!tldService) {
    return false;
  }

  nsAutoCString baseDomainCaller;
  rv = tldService->GetBaseDomain(callerUri, 0, baseDomainCaller);
  if (NS_FAILED(rv)) {
    return false;
  }

  nsAutoCString baseDomainAppId;
  rv = tldService->GetBaseDomain(appIdUri, 0, baseDomainAppId);
  if (NS_FAILED(rv)) {
    return false;
  }

  if (baseDomainCaller == baseDomainAppId) {
    return true;
  }

  // Exceptions for Google Accounts from Bug 1436078. These were supposed to be
  // temporary, but users reported breakage when we tried to remove them (Bug
  // 1822703). We will need to keep them indefinitely.
  if (baseDomainCaller.EqualsLiteral("google.com") &&
      (aAppId.Equals("https://www.gstatic.com/securitykey/origins.json"_ns) ||
       aAppId.Equals(
           "https://www.gstatic.com/securitykey/a/google.com/origins.json"_ns))) {
    return true;
  }

  return false;
}

nsresult DefaultRpId(const nsCOMPtr<nsIPrincipal>& aPrincipal,
                     /* out */ nsACString& aRpId) {
  // [https://w3c.github.io/webauthn/#rp-id]
  // "By default, the RP ID for a WebAuthn operation is set to the caller's
  // origin's effective domain."
  auto* basePrin = BasePrincipal::Cast(aPrincipal);
  nsCOMPtr<nsIURI> uri;
  if (NS_FAILED(basePrin->GetURI(getter_AddRefs(uri)))) {
    return NS_ERROR_FAILURE;
  }
  return uri->GetAsciiHost(aRpId);
}

bool IsWebAuthnAllowedInDocument(const nsCOMPtr<Document>& aDoc) {
  MOZ_ASSERT(aDoc);
  return aDoc->IsHTMLOrXHTML();
}

bool IsWebAuthnAllowedForPrincipal(const nsCOMPtr<nsIPrincipal>& aPrincipal) {
  MOZ_ASSERT(aPrincipal);
  if (aPrincipal->GetIsNullPrincipal()) {
    return false;
  }
  if (aPrincipal->GetIsIpAddress()) {
    return false;
  }
  // This next test is not strictly necessary since CredentialsContainer is
  // [SecureContext] in our webidl.
  if (!aPrincipal->GetIsOriginPotentiallyTrustworthy()) {
    return false;
  }
  return true;
}

bool IsValidRpId(const nsCOMPtr<nsIPrincipal>& aPrincipal,
                 const nsACString& aRpId) {
  // This checks two of the conditions defined in
  // https://w3c.github.io/webauthn/#rp-id, namely that the RP ID value is
  //  (1) "a valid domain string", and
  //  (2) "a registrable domain suffix of or is equal to the caller's origin's
  //      effective domain"
  //
  // We do not check that the condition that "origin's scheme is https [, or]
  // the origin's host is localhost and its scheme is http". These are special
  // cases of secure contexts (https://www.w3.org/TR/secure-contexts/). We
  // expose WebAuthn in all secure contexts, which is slightly more lenient
  // than the spec's condition.

  // Condition (1)
  nsCString normalizedRpId;
  nsresult rv = NS_DomainToASCII(aRpId, normalizedRpId);
  if (NS_FAILED(rv)) {
    return false;
  }
  if (normalizedRpId != aRpId) {
    return false;
  }

  // Condition (2)
  // The "is a registrable domain suffix of or is equal to" condition is defined
  // in https://html.spec.whatwg.org/multipage/browsers.html#dom-document-domain
  // as a subroutine of the document.domain setter, and it is exposed in XUL as
  // the Document::IsValidDomain function. This function takes URIs as inputs
  // rather than domain strings, so we construct a target URI using the current
  // document URI as a template.
  auto* basePrin = BasePrincipal::Cast(aPrincipal);
  nsCOMPtr<nsIURI> currentURI;
  if (NS_FAILED(basePrin->GetURI(getter_AddRefs(currentURI)))) {
    return false;
  }
  nsCOMPtr<nsIURI> targetURI;
  rv = NS_MutateURI(currentURI).SetHost(aRpId).Finalize(targetURI);
  if (NS_FAILED(rv)) {
    return false;
  }
  return Document::IsValidDomain(currentURI, targetURI);
}

static nsresult HashCString(nsICryptoHash* aHashService, const nsACString& aIn,
                            /* out */ nsTArray<uint8_t>& aOut) {
  MOZ_ASSERT(aHashService);

  nsresult rv = aHashService->Init(nsICryptoHash::SHA256);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aHashService->Update(
      reinterpret_cast<const uint8_t*>(aIn.BeginReading()), aIn.Length());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsAutoCString fullHash;
  // Passing false below means we will get a binary result rather than a
  // base64-encoded string.
  rv = aHashService->Finish(false, fullHash);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  aOut.Clear();
  aOut.AppendElements(reinterpret_cast<uint8_t const*>(fullHash.BeginReading()),
                      fullHash.Length());

  return NS_OK;
}

nsresult HashCString(const nsACString& aIn, /* out */ nsTArray<uint8_t>& aOut) {
  nsresult srv;
  nsCOMPtr<nsICryptoHash> hashService =
      do_CreateInstance(NS_CRYPTO_HASH_CONTRACTID, &srv);
  if (NS_FAILED(srv)) {
    return srv;
  }

  srv = HashCString(hashService, aIn, aOut);
  if (NS_WARN_IF(NS_FAILED(srv))) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

}  // namespace mozilla::dom
