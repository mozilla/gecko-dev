/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_IdentityProvider_h
#define mozilla_dom_IdentityProvider_h

#include "ErrorList.h"
#include "mozilla/dom/CanonicalBrowsingContext.h"
#include "mozilla/dom/IdentityCredentialBinding.h"
#include "nsIGlobalObject.h"
#include "nsISupports.h"
#include "nsWrapperCache.h"
#include "nsCOMPtr.h"

namespace mozilla::dom {

class IdentityProvider : public nsWrapperCache {
 public:
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(IdentityProvider)
  NS_DECL_CYCLE_COLLECTION_NATIVE_WRAPPERCACHE_CLASS(IdentityProvider)

  explicit IdentityProvider(nsIGlobalObject* aGlobal);
  nsIGlobalObject* GetParentObject() const { return mOwner; }
  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  static void Close(const GlobalObject& aGlobal);
  static already_AddRefed<Promise> Resolve(
      const GlobalObject& aGlobal, const nsACString& aToken,
      const IdentityResolveOptions& aOptions, ErrorResult& aRv);

 protected:
  virtual ~IdentityProvider();

 private:
  nsCOMPtr<nsIGlobalObject> mOwner;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_IdentityProvider_h
