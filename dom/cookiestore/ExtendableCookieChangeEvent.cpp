/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ExtendableCookieChangeEvent.h"
#include "mozilla/dom/ExtendableCookieChangeEventBinding.h"

namespace mozilla::dom {

NS_IMPL_ADDREF_INHERITED(ExtendableCookieChangeEvent, ExtendableEvent)
NS_IMPL_RELEASE_INHERITED(ExtendableCookieChangeEvent, ExtendableEvent)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ExtendableCookieChangeEvent)
NS_INTERFACE_MAP_END_INHERITING(ExtendableEvent)

NS_IMPL_CYCLE_COLLECTION_INHERITED(ExtendableCookieChangeEvent, ExtendableEvent)

ExtendableCookieChangeEvent::ExtendableCookieChangeEvent(EventTarget* aOwner)
    : ExtendableEvent(aOwner) {}

ExtendableCookieChangeEvent::~ExtendableCookieChangeEvent() = default;

JSObject* ExtendableCookieChangeEvent::WrapObjectInternal(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return mozilla::dom::ExtendableCookieChangeEvent_Binding::Wrap(aCx, this,
                                                                 aGivenProto);
}

void ExtendableCookieChangeEvent::GetChanged(
    nsTArray<CookieListItem>& aList) const {
  aList = mChanged.Clone();
}

void ExtendableCookieChangeEvent::GetDeleted(
    nsTArray<CookieListItem>& aList) const {
  aList = mDeleted.Clone();
}

/* static */ already_AddRefed<ExtendableCookieChangeEvent>
ExtendableCookieChangeEvent::Constructor(
    const GlobalObject& aGlobal, const nsAString& aType,
    const ExtendableCookieChangeEventInit& aEventInit) {
  nsCOMPtr<EventTarget> t = do_QueryInterface(aGlobal.GetAsSupports());
  RefPtr<ExtendableCookieChangeEvent> event =
      new ExtendableCookieChangeEvent(t);
  bool trusted = event->Init(t);

  event->InitEvent(aType, aEventInit.mBubbles, aEventInit.mCancelable);
  event->SetTrusted(trusted);
  event->SetComposed(aEventInit.mComposed);

  if (aEventInit.mChanged.WasPassed()) {
    event->mChanged = aEventInit.mChanged.Value();
  }

  if (aEventInit.mDeleted.WasPassed()) {
    event->mDeleted = aEventInit.mDeleted.Value();
  }

  return event.forget();
}

// static
already_AddRefed<ExtendableCookieChangeEvent>
ExtendableCookieChangeEvent::CreateForChangedCookie(
    EventTarget* aEventTarget, const CookieListItem& aItem) {
  RefPtr<ExtendableCookieChangeEvent> event =
      new ExtendableCookieChangeEvent(aEventTarget);

  event->InitEvent(u"cookiechange"_ns, false, false);
  event->SetTrusted(true);

  event->mChanged.AppendElement(aItem);
  return event.forget();
}

// static
already_AddRefed<ExtendableCookieChangeEvent>
ExtendableCookieChangeEvent::CreateForDeletedCookie(
    EventTarget* aEventTarget, const CookieListItem& aItem) {
  RefPtr<ExtendableCookieChangeEvent> event =
      new ExtendableCookieChangeEvent(aEventTarget);

  event->InitEvent(u"cookiechange"_ns, false, false);
  event->SetTrusted(true);

  event->mDeleted.AppendElement(aItem);
  return event.forget();
}

}  // namespace mozilla::dom
