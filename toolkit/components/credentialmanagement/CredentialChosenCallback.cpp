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

nsresult CredentialChosenCallback::Notify(const nsACString& aCredentialId) {
  MOZ_ASSERT(NS_IsMainThread());

  if (!mResult) {
    return NS_OK;
  }

  if (aCredentialId.IsVoid()) {
    mResult->Reject(NS_OK, __func__);
    mResult = nullptr;
    return NS_OK;
  }

  for (auto option : mOptions) {
    if (option.id().Equals(NS_ConvertUTF8toUTF16(aCredentialId))) {
      mResult->Resolve(option, __func__);
      mResult = nullptr;
      return NS_OK;
    }
  }

  mResult->Reject(nsresult::NS_ERROR_NO_CONTENT, __func__);
  mResult = nullptr;
  return NS_OK;
}

nsresult CredentialChosenCallback::GetName(nsACString& aName) {
  aName.AssignLiteral("CredentialChosenCallback");
  return NS_OK;
}

NS_IMPL_ISUPPORTS(CredentialChosenCallback, nsICredentialChosenCallback,
                  nsINamed)

}  // namespace mozilla
