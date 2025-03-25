/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ExtendableCookieChangeEvent_h
#define mozilla_dom_ExtendableCookieChangeEvent_h

#include "mozilla/dom/ServiceWorkerEvents.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/ExtendableCookieChangeEventBinding.h"

namespace mozilla::dom {

class ExtendableCookieChangeEvent final : public ExtendableEvent {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(ExtendableCookieChangeEvent,
                                           ExtendableEvent)

  JSObject* WrapObjectInternal(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  static already_AddRefed<ExtendableCookieChangeEvent> Constructor(
      const GlobalObject& aGlobal, const nsAString& aType,
      const ExtendableCookieChangeEventInit& aEventInit);

  void GetChanged(nsTArray<CookieListItem>& aList) const;

  void GetDeleted(nsTArray<CookieListItem>& aList) const;

  static already_AddRefed<ExtendableCookieChangeEvent> CreateForChangedCookie(
      EventTarget* aEventTarget, const CookieListItem& aItem);

  static already_AddRefed<ExtendableCookieChangeEvent> CreateForDeletedCookie(
      EventTarget* aEventTarget, const CookieListItem& aItem);

 private:
  explicit ExtendableCookieChangeEvent(EventTarget* aOwner);
  ~ExtendableCookieChangeEvent();

  nsTArray<CookieListItem> mChanged;
  nsTArray<CookieListItem> mDeleted;
};

}  // namespace mozilla::dom

#endif /* mozilla_dom_ExtendableCookieChangeEvent_h */
