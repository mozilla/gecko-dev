/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_NavigationTransition_h___
#define mozilla_dom_NavigationTransition_h___

#include "nsWrapperCache.h"

class nsIGlobalObject;

namespace mozilla::dom {

class NavigationHistoryEntry;
enum class NavigationType : uint8_t;

class NavigationTransition final : public nsISupports, public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(NavigationTransition)

  enum NavigationType NavigationType() const { return {}; }
  already_AddRefed<NavigationHistoryEntry> From() const { return {}; }
  already_AddRefed<Promise> Finished() const { return {}; }

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;
  nsIGlobalObject* GetParentObject() const { return {}; }

 private:
  ~NavigationTransition() = default;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_NavigationTransition_h___
