/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_NavigationHistoryEntry_h___
#define mozilla_dom_NavigationHistoryEntry_h___

#include "mozilla/DOMEventTargetHelper.h"

class nsStructuredCloneContainer;

namespace mozilla::dom {

class SessionHistoryInfo;

class NavigationHistoryEntry final : public DOMEventTargetHelper {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(NavigationHistoryEntry,
                                           DOMEventTargetHelper)

  NavigationHistoryEntry(nsPIDOMWindowInner* aWindow,
                         const SessionHistoryInfo* aSHInfo, int64_t aIndex);

  void GetUrl(nsAString& aResult) const;
  void GetKey(nsAString& aResult) const;
  void GetId(nsAString& aResult) const;
  int64_t Index() const;
  bool SameDocument() const;

  void GetState(JSContext* aCx, JS::MutableHandle<JS::Value> aResult,
                ErrorResult& aRv) const;
  void SetState(nsStructuredCloneContainer* aState);

  IMPL_EVENT_HANDLER(dispose);

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  bool IsSameEntry(const SessionHistoryInfo* aSHInfo) const;

  bool SharesDocumentWith(const SessionHistoryInfo& aSHInfo) const;

  const nsID& Key() const;

 private:
  ~NavigationHistoryEntry();

  Document* GetCurrentDocument() const;

  nsCOMPtr<nsPIDOMWindowInner> mWindow;
  UniquePtr<SessionHistoryInfo> mSHInfo;
  int64_t mIndex;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_NavigationHistoryEntry_h___
