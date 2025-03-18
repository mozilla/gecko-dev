/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_NavigationDestination_h___
#define mozilla_dom_NavigationDestination_h___

#include "nsISupports.h"

#include "nsStructuredCloneContainer.h"
#include "nsWrapperCache.h"

#include "mozilla/dom/BindingDeclarations.h"

class nsIGlobalObject;
class nsIURI;

namespace mozilla {
class ErrorResult;
}

namespace mozilla::dom {

class NavigationHistoryEntry;

// https://html.spec.whatwg.org/#the-navigationdestination-interface
class NavigationDestination final : public nsISupports, public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(NavigationDestination)

  NavigationDestination(nsIGlobalObject* aGlobal, nsIURI* aURI,
                        NavigationHistoryEntry* aEntry,
                        nsStructuredCloneContainer* aState,
                        bool aIsSameDocument);

  void GetUrl(nsString& aURL) const;
  void GetKey(nsString& aKey) const;
  void GetId(nsString& aId) const;
  int64_t Index() const;
  bool SameDocument() const;
  void GetState(JSContext* aCx, JS::MutableHandle<JS::Value> aRetVal,
                ErrorResult& aRv) const;

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;
  nsIGlobalObject* GetParentObject();

 private:
  ~NavigationDestination() = default;

  nsCOMPtr<nsIGlobalObject> mGlobal;

  // https://html.spec.whatwg.org/#concept-navigationdestination-url
  nsCOMPtr<nsIURI> mURL;

  // https://html.spec.whatwg.org/#concept-navigationdestination-entry
  RefPtr<NavigationHistoryEntry> mEntry;

  // https://html.spec.whatwg.org/#concept-navigationdestination-state
  RefPtr<nsStructuredCloneContainer> mState;

  // https://html.spec.whatwg.org/#concept-navigationdestination-samedocument
  bool mIsSameDocument = false;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_NavigationDestination_h___
