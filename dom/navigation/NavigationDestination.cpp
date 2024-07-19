/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/NavigationDestination.h"
#include "mozilla/dom/NavigationDestinationBinding.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_0(NavigationDestination)
NS_IMPL_CYCLE_COLLECTING_ADDREF(NavigationDestination)
NS_IMPL_CYCLE_COLLECTING_RELEASE(NavigationDestination)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(NavigationDestination)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

JSObject* NavigationDestination::WrapObject(JSContext* aCx,
                                            JS::Handle<JSObject*> aGivenProto) {
  return NavigationDestination_Binding::Wrap(aCx, this, aGivenProto);
}

}  // namespace mozilla::dom
