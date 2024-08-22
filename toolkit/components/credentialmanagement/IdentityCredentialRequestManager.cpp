/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IdentityCredentialRequestManager.h"
#include "mozilla/dom/IdentityCredentialBinding.h"
#include "mozilla/dom/WindowGlobalParent.h"
#include "nsContentUtils.h"
#include "nsNetUtil.h"
#include "mozilla/BasePrincipal.h"

namespace mozilla {

NS_IMPL_ISUPPORTS0(IdentityCredentialRequestManager);

StaticRefPtr<IdentityCredentialRequestManager>
    IdentityCredentialRequestManager::sSingleton;

// static
IdentityCredentialRequestManager*
IdentityCredentialRequestManager::GetInstance() {
  if (!sSingleton) {
    sSingleton = new IdentityCredentialRequestManager();
    ClearOnShutdown(&sSingleton);
  }
  return sSingleton;
}

nsresult IdentityCredentialRequestManager::StorePendingRequest(
    const nsCOMPtr<nsIPrincipal>& aRPPrincipal,
    const dom::IdentityCredentialRequestOptions& aRequest,
    const RefPtr<
        dom::IdentityCredential::GetIPCIdentityCredentialPromise::Private>&
        aPromise,
    const RefPtr<dom::CanonicalBrowsingContext>& aBrowsingContext) {
  MOZ_ASSERT(aRPPrincipal);

  if (!aRequest.mProviders.WasPassed()) {
    return NS_ERROR_DOM_INVALID_ACCESS_ERR;
  }

  for (const auto& provider : aRequest.mProviders.Value()) {
    if (!provider.mLoginURL.WasPassed()) {
      continue;
    }
    nsCOMPtr<nsIURI> idpOriginURI;
    nsAutoCString idpOriginString;
    if (provider.mOrigin.WasPassed()) {
      idpOriginString = provider.mOrigin.Value();
    } else {
      // Infer the origin from the loginURL if one wasn't provided
      idpOriginString = provider.mLoginURL.Value();
    }
    nsresult rv = NS_NewURI(getter_AddRefs(idpOriginURI), idpOriginString);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return NS_ERROR_DOM_BAD_URI;
    }
    nsCOMPtr<nsIPrincipal> idpPrincipal = BasePrincipal::CreateContentPrincipal(
        idpOriginURI, aRPPrincipal->OriginAttributesRef());

    NS_ENSURE_TRUE(idpPrincipal, NS_ERROR_FAILURE);
    nsTArray<PendingRequestEntry>& list =
        mPendingRequests.LookupOrInsert(idpPrincipal);
    list.AppendElement(PendingRequestEntry(aRPPrincipal, aRequest, aPromise,
                                           aBrowsingContext));
  }
  return NS_OK;
}

void IdentityCredentialRequestManager::NotifyOfStoredCredential(
    const nsCOMPtr<nsIPrincipal>& aIDPPrincipal,
    const dom::IPCIdentityCredential& aCredential) {
  MOZ_ASSERT(aIDPPrincipal);
  auto listLookup = mPendingRequests.Lookup(aIDPPrincipal);
  if (listLookup) {
    for (auto& entry : listLookup.Data()) {
      if (!entry.mBrowsingContext) {
        continue;
      }
      // We must (asynchronously) test if this credential should be sent down
      // to the site.
      dom::IdentityCredential::AllowedToCollectCredential(
          entry.mRPPrincipal, entry.mBrowsingContext, entry.mRequestOptions,
          aCredential)
          ->Then(
              GetCurrentSerialEventTarget(), __func__,
              [aCredential, entry](bool effectiveCredential) {
                if (effectiveCredential) {
                  entry.mPromise->Resolve(aCredential, __func__);
                }
              },
              []() {});
    }
  }
}

}  // namespace mozilla
