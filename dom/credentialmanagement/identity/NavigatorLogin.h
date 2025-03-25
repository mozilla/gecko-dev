/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_NavigatorLogin_h
#define mozilla_dom_NavigatorLogin_h

#include "ErrorList.h"
#include "mozilla/dom/LoginStatusBinding.h"
#include "mozilla/Maybe.h"
#include "nsIGlobalObject.h"
#include "nsISupports.h"
#include "nsWrapperCache.h"
#include "nsCOMPtr.h"

namespace mozilla::dom {

class NavigatorLogin : public nsWrapperCache {
 public:
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(NavigatorLogin)
  NS_DECL_CYCLE_COLLECTION_NATIVE_WRAPPERCACHE_CLASS(NavigatorLogin)

  explicit NavigatorLogin(nsIGlobalObject* aGlobal);
  nsIGlobalObject* GetParentObject() const { return mOwner; }
  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  already_AddRefed<mozilla::dom::Promise> SetStatus(LoginStatus aStatus,
                                                    mozilla::ErrorResult& aRv);

  static Maybe<LoginStatus> GetLoginStatus(nsIPrincipal* aPrincipal);
  static nsresult SetLoginStatus(nsIPrincipal* aPrincipal, LoginStatus aStatus);
  static nsresult SetLoginStatus(nsIPrincipal* aPrincipal,
                                 const nsACString& aStatus);
  static nsresult ParseLoginStatusHeader(const nsACString& aStatus,
                                         LoginStatus& aResult);

 protected:
  virtual ~NavigatorLogin();

 private:
  nsCOMPtr<nsIGlobalObject> mOwner;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_NavigatorLogin_h
