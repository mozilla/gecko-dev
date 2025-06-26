/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_IDENTITYCREDENTIALREQUESTMANAGER_H_
#define MOZILLA_IDENTITYCREDENTIALREQUESTMANAGER_H_

#include "mozilla/dom/IdentityCredentialBinding.h"
#include "mozilla/dom/WebIdentityParent.h"
#include "nsISupports.h"
#include "nsIURI.h"

namespace mozilla {

class IdentityCredentialRequestManager final : nsISupports {
 public:
  NS_DECL_ISUPPORTS

  static IdentityCredentialRequestManager* GetInstance();

  IdentityCredentialRequestManager(IdentityCredentialRequestManager& other) =
      delete;
  void operator=(const IdentityCredentialRequestManager&) = delete;

  RefPtr<MozPromise<std::tuple<nsCString, Maybe<nsCString>>, nsresult, true>>
  GetTokenFromPopup(dom::WebIdentityParent* aRelyingPartyWindow,
                    nsIURI* aURLToOpen);

  nsresult MaybeResolvePopup(dom::WebIdentityParent* aPopupWindow,
                             const nsCString& aToken,
                             const dom::IdentityResolveOptions& aOptions);

  bool IsActivePopup(dom::WebIdentityParent* aPopupWindow);

 private:
  static StaticRefPtr<IdentityCredentialRequestManager> sSingleton;
  IdentityCredentialRequestManager() {};
  ~IdentityCredentialRequestManager() = default;

  nsTHashMap<uint64_t,
             RefPtr<MozPromise<std::tuple<nsCString, Maybe<nsCString>>,
                               nsresult, true>::Private>>
      mPendingTokenRequests;
};

}  // namespace mozilla

#endif /* MOZILLA_IDENTITYCREDENTIALSTORAGESERVICE_H_ */
