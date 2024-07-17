/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/NavigationTransition.h"
#include "mozilla/dom/NavigationTransitionBinding.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_0(NavigationTransition)
NS_IMPL_CYCLE_COLLECTING_ADDREF(NavigationTransition)
NS_IMPL_CYCLE_COLLECTING_RELEASE(NavigationTransition)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(NavigationTransition)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

JSObject* NavigationTransition::WrapObject(JSContext* aCx,
                                           JS::Handle<JSObject*> aGivenProto) {
  return NavigationTransition_Binding::Wrap(aCx, this, aGivenProto);
}

}  // namespace mozilla::dom
