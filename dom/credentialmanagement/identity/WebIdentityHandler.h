/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_WebIdentityHandler_h
#define mozilla_dom_WebIdentityHandler_h

#include "mozilla/dom/AbortFollower.h"
#include "mozilla/dom/CredentialManagementBinding.h"
#include "mozilla/dom/WebIdentityChild.h"
#include "nsCycleCollectionParticipant.h"
#include "nsPIDOMWindow.h"

namespace mozilla::dom {

class WebIdentityHandler final : public AbortFollower {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(WebIdentityHandler)

  explicit WebIdentityHandler(nsPIDOMWindowInner* aWindow) : mWindow(aWindow) {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(aWindow);
  }

  bool MaybeCreateActor();
  void ActorDestroyed();

  // AbortFollower
  void RunAbortAlgorithm() override;

  void GetCredential(const CredentialRequestOptions& aOptions,
                     bool aSameOriginWithAncestors,
                     const RefPtr<Promise>& aPromise);

  void PreventSilentAccess(const RefPtr<Promise>& aPromise);

  void Disconnect(const IdentityCredentialDisconnectOptions& aOptions,
                  const RefPtr<Promise>& aPromise);

  void SetLoginStatus(const LoginStatus& aStatus,
                      const RefPtr<Promise>& aPromise);

 private:
  ~WebIdentityHandler();
  nsCOMPtr<nsPIDOMWindowInner> mWindow;
  RefPtr<WebIdentityChild> mActor;
  RefPtr<Promise> mGetPromise;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_WebIdentityHandler_h
