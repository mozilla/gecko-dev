/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIGlobalObject.h"

#include "mozilla/dom/NavigationBinding.h"
#include "mozilla/dom/NavigationHistoryEntry.h"
#include "mozilla/dom/NavigationTransition.h"
#include "mozilla/dom/NavigationTransitionBinding.h"
#include "mozilla/dom/Promise.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(NavigationTransition, mGlobalObject,
                                      mFrom, mFinished)
NS_IMPL_CYCLE_COLLECTING_ADDREF(NavigationTransition)
NS_IMPL_CYCLE_COLLECTING_RELEASE(NavigationTransition)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(NavigationTransition)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NavigationTransition::NavigationTransition(nsIGlobalObject* aGlobalObject,
                                           enum NavigationType aNavigationType,
                                           NavigationHistoryEntry* aFrom,
                                           Promise* aFinished)
    : mNavigationType(aNavigationType), mFrom(aFrom), mFinished(aFinished) {}

// https://html.spec.whatwg.org/#dom-navigationtransition-navigationtype
enum NavigationType NavigationTransition::NavigationType() const {
  return mNavigationType;
}

// https://html.spec.whatwg.org/#dom-navigationtransition-from
NavigationHistoryEntry* NavigationTransition::From() const { return mFrom; }

// https://html.spec.whatwg.org/#dom-navigationtransition-finished
Promise* NavigationTransition::Finished() const { return mFinished; }

JSObject* NavigationTransition::WrapObject(JSContext* aCx,
                                           JS::Handle<JSObject*> aGivenProto) {
  return NavigationTransition_Binding::Wrap(aCx, this, aGivenProto);
}

nsIGlobalObject* NavigationTransition::GetParentObject() const {
  return mGlobalObject;
}

}  // namespace mozilla::dom
