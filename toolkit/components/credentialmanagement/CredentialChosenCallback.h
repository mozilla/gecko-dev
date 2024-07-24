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

#include "mozilla/dom/Promise.h"

namespace mozilla {

using dom::Credential;
using dom::Promise;

class CredentialChosenCallback final : public nsICredentialChosenCallback,
                                       public nsINamed {
 public:
  explicit CredentialChosenCallback(Promise* aPromise) : mPromise(aPromise) {}

  NS_IMETHOD
  Notify(Credential* aCredential) override;

  NS_IMETHOD
  GetName(nsACString& aName) override;

  NS_DECL_ISUPPORTS

 private:
  ~CredentialChosenCallback() = default;
  RefPtr<Promise> mPromise;
};

}  // namespace mozilla

#endif /* MOZILLA_CREDENTIALCHOSENCALLBACK_H_ */
