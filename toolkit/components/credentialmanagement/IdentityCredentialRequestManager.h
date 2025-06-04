/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_IDENTITYCREDENTIALREQUESTMANAGER_H_
#define MOZILLA_IDENTITYCREDENTIALREQUESTMANAGER_H_

#include "mozilla/ClearOnShutdown.h"
#include "mozilla/PrincipalHashKey.h"
#include "mozilla/dom/IdentityCredential.h"
#include "mozilla/dom/IdentityCredentialBinding.h"
#include "mozilla/dom/IPCIdentityCredential.h"
#include "nsIPrincipal.h"
#include "nsTHashMap.h"

namespace mozilla {

class IdentityCredentialRequestManager final : nsISupports {
 public:
  NS_DECL_ISUPPORTS

  static IdentityCredentialRequestManager* GetInstance();

  // Store an active cross origin identity credential request happening from the
  // given principal and inner window ID. These accumulate forever, but if the
  // window goes away, we will be unable to notify of a store.
  nsresult StorePendingRequest(
      const nsCOMPtr<nsIPrincipal>& aRPPrincipal,
      const dom::IdentityCredentialRequestOptions& aRequest,
      const RefPtr<
          dom::IdentityCredential::GetIPCIdentityCredentialPromise::Private>&
          aPromise,
      const RefPtr<dom::CanonicalBrowsingContext>& aBrowsingContext);

  // If the given credential stored by the given principal would be effective
  // for a previously stored request, notify the window that stored that request
  // with the credential so it can resolve a promise with that credential data.
  void NotifyOfStoredCredential(const nsCOMPtr<nsIPrincipal>& aIDPPrincipal,
                                const dom::IPCIdentityCredential& aCredential);

  IdentityCredentialRequestManager(IdentityCredentialRequestManager& other) =
      delete;
  void operator=(const IdentityCredentialRequestManager&) = delete;

 private:
  static StaticRefPtr<IdentityCredentialRequestManager> sSingleton;
  IdentityCredentialRequestManager() {};
  ~IdentityCredentialRequestManager() = default;

  struct PendingRequestEntry {
    nsCOMPtr<nsIPrincipal> mRPPrincipal;
    dom::IdentityCredentialRequestOptions mRequestOptions;
    RefPtr<dom::IdentityCredential::GetIPCIdentityCredentialPromise::Private>
        mPromise;
    RefPtr<dom::CanonicalBrowsingContext> mBrowsingContext;

    PendingRequestEntry(
        nsIPrincipal* aRPPrincipal,
        const dom::IdentityCredentialRequestOptions& aRequestOptions,
        const RefPtr<
            dom::IdentityCredential::GetIPCIdentityCredentialPromise::Private>&
            aPromise,
        const RefPtr<dom::CanonicalBrowsingContext>& aBrowsingContext)
        : mRPPrincipal(aRPPrincipal),
          mRequestOptions(aRequestOptions),
          mPromise(aPromise),
          mBrowsingContext(aBrowsingContext) {}
  };
  nsTHashMap<PrincipalHashKey, nsTArray<PendingRequestEntry>> mPendingRequests;
};

}  // namespace mozilla

#endif /* MOZILLA_IDENTITYCREDENTIALSTORAGESERVICE_H_ */
