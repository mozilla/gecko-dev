/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/CredentialChosenCallback.h"
#include "mozilla/dom/Credential.h"

namespace mozilla {

using dom::Credential;
using dom::Promise;

nsresult CredentialChosenCallback::Notify(Credential* aCredential) {
  MOZ_ASSERT(NS_IsMainThread());
  if (aCredential) {
    mPromise->MaybeResolve(aCredential);
  } else {
    mPromise->MaybeResolve(JS::NullValue());
  }

  return NS_OK;
}

nsresult CredentialChosenCallback::GetName(nsACString& aName) {
  aName.AssignLiteral("CredentialChosenCallback");
  return NS_OK;
}

NS_IMPL_ISUPPORTS(CredentialChosenCallback, nsICredentialChosenCallback,
                  nsINamed)

}  // namespace mozilla
