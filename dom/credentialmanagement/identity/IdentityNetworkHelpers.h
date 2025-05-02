/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_IdentityNetworkHelpers_h
#define mozilla_dom_IdentityNetworkHelpers_h

#include "mozilla/Components.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/Promise-inl.h"
#include "mozilla/dom/Request.h"
#include "mozilla/dom/Response.h"
#include "mozilla/dom/WindowGlobalParent.h"
#include "mozilla/MozPromise.h"
#include "mozilla/dom/IdentityCredentialBinding.h"
#include "nsICredentialChooserService.h"

namespace mozilla::dom {

class IdentityNetworkHelpers {
 public:
  static RefPtr<MozPromise<IdentityProviderWellKnown, nsresult, true>>
  FetchWellKnownHelper(nsIURI* aWellKnown, nsIPrincipal* aTriggeringPrincipal);

  static RefPtr<MozPromise<
      std::tuple<Maybe<IdentityProviderWellKnown>, IdentityProviderAPIConfig>,
      nsresult, true>>
  FetchConfigHelper(nsIURI* aConfig, nsIPrincipal* aTriggeringPrincipal,
                    Maybe<IdentityProviderWellKnown> aWellKnownConfig);

  static RefPtr<MozPromise<IdentityProviderAccountList, nsresult, true>>
  FetchAccountsHelper(nsIURI* aAccountsEndpoint,
                      nsIPrincipal* aTriggeringPrincipal);

  static RefPtr<MozPromise<IdentityProviderToken, nsresult, true>>
  FetchTokenHelper(nsIURI* aAccountsEndpoint, const nsCString& aBody,
                   nsIPrincipal* aTriggeringPrincipal);

  static RefPtr<MozPromise<DisconnectedAccount, nsresult, true>>
  FetchDisconnectHelper(nsIURI* aAccountsEndpoint, const nsCString& aBody,
                        nsIPrincipal* aTriggeringPrincipal);
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_IdentityNetworkHelpers_h
