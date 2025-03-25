/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_CookieStoreManager_h
#define mozilla_dom_CookieStoreManager_h

#include "mozilla/dom/CookieStoreBinding.h"
#include "mozilla/dom/CookieStoreManagerBinding.h"
#include "nsWrapperCache.h"

class nsIGlobalObject;

namespace mozilla::dom {

class CookieStoreChild;
class Promise;

class CookieStoreManager final : public nsWrapperCache {
 public:
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(CookieStoreManager)
  NS_DECL_CYCLE_COLLECTION_NATIVE_WRAPPERCACHE_CLASS(CookieStoreManager)

  CookieStoreManager(nsIGlobalObject* aGlobalObject,
                     const nsACString& aServiceWorkerRegistrationScopeURL);

  nsIGlobalObject* GetParentObject() const { return mGlobalObject; }

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  already_AddRefed<Promise> Subscribe(
      const CopyableTArray<CookieStoreGetOptions>& aSubscriptions,
      ErrorResult& aRv);

  already_AddRefed<Promise> GetSubscriptions(ErrorResult& aRv);

  already_AddRefed<Promise> Unsubscribe(
      const CopyableTArray<CookieStoreGetOptions>& aSubscriptions,
      ErrorResult& aRv);

 private:
  ~CookieStoreManager();

  bool MaybeCreateActor();

  void Shutdown();

  enum class Action {
    eSubscribe,
    eUnsubscribe,
  };

  already_AddRefed<Promise> SubscribeOrUnsubscribe(
      Action aAction,
      const CopyableTArray<CookieStoreGetOptions>& aSubscriptions,
      ErrorResult& aRv);

  RefPtr<nsIGlobalObject> mGlobalObject;
  nsCString mScopeURL;

  RefPtr<CookieStoreChild> mActor;
};

}  // namespace mozilla::dom

#endif /* mozilla_dom_CookieStoreManager_h */
