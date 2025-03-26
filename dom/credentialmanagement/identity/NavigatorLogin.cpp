/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/NavigatorLogin.h"
#include "nsCycleCollectionParticipant.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(NavigatorLogin, mOwner)

NavigatorLogin::~NavigatorLogin() = default;

JSObject* NavigatorLogin::WrapObject(JSContext* aCx,
                                     JS::Handle<JSObject*> aGivenProto) {
  return NavigatorLogin_Binding::Wrap(aCx, this, aGivenProto);
}

NavigatorLogin::NavigatorLogin(nsIGlobalObject* aGlobal)
    : mOwner(aGlobal){
      MOZ_ASSERT(mOwner);
    };

already_AddRefed<mozilla::dom::Promise> NavigatorLogin::SetStatus(
    const LoginStatus& aStatus, mozilla::ErrorResult& aRv) {

  RefPtr<Promise> promise = Promise::Create(mOwner, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }
  promise->MaybeRejectWithNotSupportedError(
      "navigator.login.setStatus not implemented"_ns);
  return promise.forget();
}

}  // namespace mozilla::dom
