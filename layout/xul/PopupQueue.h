/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef PopupQueue_h
#define PopupQueue_h

#include "mozilla/MoveOnlyFunction.h"
#include "nsTArray.h"

namespace mozilla::dom {
class Element;
}

class PopupQueue final {
  NS_INLINE_DECL_REFCOUNTING(PopupQueue)

  typedef class mozilla::dom::Element Element;

 public:
  static already_AddRefed<PopupQueue> GetInstance();

  void Enqueue(Element* aPopup,
               mozilla::MoveOnlyFunction<void(Element*)>&& aCallback);

  void Show(Element* aPopup,
            mozilla::MoveOnlyFunction<void(Element*)>&& aCallback);

  void NotifyDismissed(Element* aPopup, bool aRemoveAll = false);

  Element* RetrieveQueueableShownPopup() const;

 private:
  ~PopupQueue() = default;
  PopupQueue() = default;

  bool Store(Element* aPopup, bool aShown,
             mozilla::MoveOnlyFunction<void(Element*)>&& aCallback);

  struct PendingPopup {
    RefPtr<Element> mPopup;
    bool mQueueable;
    bool mShown;
    mozilla::MoveOnlyFunction<void(Element*)> mCallback;
  };

  void MaybeShowNext();

  nsTArray<PendingPopup> mQueue;
  uint32_t mShowing = 0;
};

#endif  // PopupQueue_h
