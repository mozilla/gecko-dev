/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_NavigationHistoryEntry_h___
#define mozilla_dom_NavigationHistoryEntry_h___

#include "mozilla/DOMEventTargetHelper.h"

namespace mozilla::dom {

class NavigationHistoryEntry final : public DOMEventTargetHelper {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(NavigationHistoryEntry,
                                           DOMEventTargetHelper)

  void GetUrl(nsAString& aResult) const {}
  void GetKey(nsAString& aResult) const {}
  void GetId(nsAString& aResult) const {}
  int64_t Index() const { return {}; }
  bool SameDocument() const { return {}; }

  void GetState(JSContext* aCx, JS::MutableHandle<JS::Value> aResult,
                ErrorResult& aRv) {}

  IMPL_EVENT_HANDLER(dispose);

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

 private:
  ~NavigationHistoryEntry() = default;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_NavigationHistoryEntry_h___
