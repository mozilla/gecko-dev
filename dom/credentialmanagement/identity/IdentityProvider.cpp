/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/IdentityProvider.h"
#include "nsIGlobalObject.h"
#include "mozilla/dom/WebIdentityHandler.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(IdentityProvider, mOwner)

IdentityProvider::~IdentityProvider() = default;

JSObject* IdentityProvider::WrapObject(JSContext* aCx,
                                       JS::Handle<JSObject*> aGivenProto) {
  return IdentityProvider_Binding::Wrap(aCx, this, aGivenProto);
}

IdentityProvider::IdentityProvider(nsIGlobalObject* aGlobal) : mOwner(aGlobal) {
  MOZ_ASSERT(mOwner);
}

// static
void IdentityProvider::Close(const GlobalObject& aGlobal) {
  nsCOMPtr<nsPIDOMWindowInner> window =
      do_QueryInterface(aGlobal.GetAsSupports());
  NS_ENSURE_TRUE_VOID(window);
  Unused << window->Close();
}

// static
already_AddRefed<Promise> IdentityProvider::Resolve(
    const GlobalObject& aGlobal, const nsACString& aToken,
    const IdentityResolveOptions& aOptions, ErrorResult& aRv) {
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aGlobal.GetAsSupports());
  MOZ_ASSERT(global);
  RefPtr<Promise> promise = Promise::Create(global, aRv);
  if (NS_WARN_IF(aRv.Failed() || !promise)) {
    return nullptr;
  }
  RefPtr<nsPIDOMWindowInner> window = global->GetAsInnerWindow();
  MOZ_ASSERT(window);
  if (!window) {
    promise->MaybeRejectWithNotAllowedError(
        "IdentityProvider.resolve be called within a window.");
    return promise.forget();
  }
  WebIdentityHandler* identityHandler = window->GetOrCreateWebIdentityHandler();
  if (!identityHandler) {
    promise->MaybeRejectWithNotAllowedError(
        "IdentityProvider.resolve could not find a pending request to resolve");
    return promise.forget();
  }
  identityHandler->ResolveContinuationWindow(aToken, aOptions)
      ->Then(
          GetCurrentSerialEventTarget(), __func__,
          [promise, window](nsresult aSuccess) {
            MOZ_ASSERT(NS_SUCCEEDED(aSuccess));
            promise->MaybeResolveWithUndefined();
            window->Close();
          },
          [promise](nsresult aFailure) {
            promise->MaybeRejectWithNotAllowedError(
                "IdentityProvider.resolve could not find a pending request to "
                "resolve");
          });
  return promise.forget();
}

}  // namespace dom
}  // namespace mozilla
