/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_NavigationDestination_h___
#define mozilla_dom_NavigationDestination_h___

#include "mozilla/ErrorResult.h"
#include "nsWrapperCache.h"

class nsIGlobalObject;

namespace mozilla::dom {

class NavigationDestination final : public nsISupports, public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(NavigationDestination)

  void GetUrl(nsString& aRetVal) const {}
  void GetKey(nsString& aRetVal) const {}
  void GetId(nsString& aRetVal) const {}
  int64_t Index() const { return {}; }
  bool SameDocument() const { return {}; }

  void GetState(JSContext* cx, JS::MutableHandle<JS::Value> aRetVal,
                ErrorResult& aRv) const {}

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;
  nsIGlobalObject* GetParentObject() const { return {}; }

 private:
  ~NavigationDestination() = default;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_NavigationDestination_h___
