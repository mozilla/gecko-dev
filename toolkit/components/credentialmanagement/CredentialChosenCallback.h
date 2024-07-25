/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_CREDENTIALCHOSENCALLBACK_H_
#define MOZILLA_CREDENTIALCHOSENCALLBACK_H_

#include "nsICredentialChosenCallback.h"
#include "nsINamed.h"
#include "nsStringFwd.h"
#include "nsTArray.h"

#include "mozilla/dom/IdentityCredentialSerializationHelpers.h"
#include "mozilla/dom/Promise.h"

namespace mozilla {

using dom::IPCIdentityCredential;
using dom::Promise;

class CredentialChosenCallback final : public nsICredentialChosenCallback,
                                       public nsINamed {
 public:
  explicit CredentialChosenCallback(
      CopyableTArray<IPCIdentityCredential>&& aOptions,
      const RefPtr<MozPromise<IPCIdentityCredential, nsresult, true>::Private>&
          aResult)
      : mOptions(aOptions), mResult(aResult) {}

  NS_IMETHOD
  Notify(const nsACString& aCredential) override;

  NS_IMETHOD
  GetName(nsACString& aName) override;

  NS_DECL_ISUPPORTS

 private:
  ~CredentialChosenCallback() {
    if (mResult) {
      mResult->Reject(NS_ERROR_FAILURE, __func__);
    }
  };

  // mOptions is the list of credentials presented to the user in the credential
  // chooser. We maintain this list so we can resolve with the user's choice of
  // credential.
  CopyableTArray<IPCIdentityCredential> mOptions;

  // mResult is a promise that will fulfill once the user has made a choice.
  // Dismissal is represented as a reject(NS_OK), and selection resolves with
  // an entry of mOptions.
  RefPtr<MozPromise<IPCIdentityCredential, nsresult, true>::Private> mResult;
};

}  // namespace mozilla

#endif /* MOZILLA_CREDENTIALCHOSENCALLBACK_H_ */
