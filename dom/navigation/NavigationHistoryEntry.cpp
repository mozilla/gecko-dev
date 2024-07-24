/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/NavigationHistoryEntry.h"
#include "mozilla/dom/NavigationHistoryEntryBinding.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_INHERITED(NavigationHistoryEntry,
                                   DOMEventTargetHelper);
NS_IMPL_ADDREF_INHERITED(NavigationHistoryEntry, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(NavigationHistoryEntry, DOMEventTargetHelper)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(NavigationHistoryEntry)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

JSObject* NavigationHistoryEntry::WrapObject(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return NavigationHistoryEntry_Binding::Wrap(aCx, this, aGivenProto);
}

}  // namespace mozilla::dom
