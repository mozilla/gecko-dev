/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/CookieChangeEvent.h"
#include "mozilla/dom/CookieChangeEventBinding.h"

namespace mozilla::dom {

CookieChangeEvent::CookieChangeEvent(EventTarget* aOwner,
                                     nsPresContext* aPresContext,
                                     WidgetEvent* aEvent)
    : Event(aOwner, aPresContext, aEvent) {}

CookieChangeEvent::~CookieChangeEvent() = default;

JSObject* CookieChangeEvent::WrapObjectInternal(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return mozilla::dom::CookieChangeEvent_Binding::Wrap(aCx, this, aGivenProto);
}

void CookieChangeEvent::GetChanged(nsTArray<CookieListItem>& aList) const {
  aList = mChanged.Clone();
}

void CookieChangeEvent::GetDeleted(nsTArray<CookieListItem>& aList) const {
  aList = mDeleted.Clone();
}

/* static */ already_AddRefed<CookieChangeEvent> CookieChangeEvent::Constructor(
    const GlobalObject& aGlobal, const nsAString& aType,
    const CookieChangeEventInit& aEventInit) {
  nsCOMPtr<EventTarget> t = do_QueryInterface(aGlobal.GetAsSupports());
  RefPtr<CookieChangeEvent> event = new CookieChangeEvent(t, nullptr, nullptr);
  bool trusted = event->Init(t);

  event->InitEvent(aType, aEventInit.mBubbles, aEventInit.mCancelable);
  event->SetTrusted(trusted);

  if (aEventInit.mChanged.WasPassed()) {
    event->mChanged = aEventInit.mChanged.Value();
  }

  if (aEventInit.mDeleted.WasPassed()) {
    event->mDeleted = aEventInit.mDeleted.Value();
  }

  return event.forget();
}

}  // namespace mozilla::dom
