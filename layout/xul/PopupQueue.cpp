/* -*- Mode: C++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 2; -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PopupQueue.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/dom/Element.h"
#include "mozilla/StaticPtr.h"
#include "nsThreadUtils.h"

using namespace mozilla;

StaticRefPtr<PopupQueue> gPopupQueue;

already_AddRefed<PopupQueue> PopupQueue::GetInstance() {
  if (!gPopupQueue) {
    gPopupQueue = new PopupQueue();
    ClearOnShutdown(&gPopupQueue);
  }
  return do_AddRef(gPopupQueue);
}

void PopupQueue::Enqueue(Element* aPopup,
                         MoveOnlyFunction<void(Element*)>&& aCallback) {
  if (!aCallback) {
    return;
  }

  if (Store(aPopup, false, std::move(aCallback)) && !mShowing) {
    MaybeShowNext();
  }
}

void PopupQueue::Show(Element* aPopup,
                      MoveOnlyFunction<void(Element*)>&& aCallback) {
  if (!aCallback) {
    return;
  }

  MoveOnlyFunction<void(Element*)> callback = std::move(aCallback);

  if (Store(aPopup, true, nullptr)) {
    ++mShowing;
    callback(aPopup);
  }
}

bool PopupQueue::Store(Element* aPopup, bool aShown,
                       MoveOnlyFunction<void(Element*)>&& aCallback) {
  // Let's avoid the same popup shown multiple time, if it's already on screen.
  if (!aShown) {
    for (const PendingPopup& popup : mQueue) {
      if (popup.mPopup == aPopup) {
        return false;
      }
    }
  }

  mQueue.AppendElement(PendingPopup{aPopup,
                                    /* if not shown, it's qeueable */ !aShown,
                                    aShown, std::move(aCallback)});
  return true;
}

void PopupQueue::NotifyDismissed(Element* aPopup, bool aRemoveAll) {
  if (mQueue.IsEmpty()) {
    return;
  }

  for (uint32_t i = 0; i < mQueue.Length(); ++i) {
    if (mQueue[i].mPopup == aPopup) {
      if (mShowing && mQueue[i].mShown) {
        --mShowing;
      }

      mQueue.RemoveElementAt(i);

      if (!aRemoveAll) {
        break;
      }
    }
  }

  if (!mQueue.IsEmpty() && !mShowing) {
    NS_DispatchToMainThread(NewRunnableMethod("PopupQueue::MaybeShowNext", this,
                                              &PopupQueue::MaybeShowNext));
  }
}

void PopupQueue::MaybeShowNext() {
  if (mQueue.IsEmpty() || mShowing) {
    return;
  }

  PendingPopup& popup = mQueue[0];

  ++mShowing;
  MOZ_ASSERT(!popup.mShown);
  popup.mShown = true;

  MoveOnlyFunction<void(Element*)> callback = std::move(popup.mCallback);
  MOZ_ASSERT(!popup.mCallback);

  callback(mQueue[0].mPopup);
}

PopupQueue::Element* PopupQueue::RetrieveQueueableShownPopup() const {
  for (auto& popup : mQueue) {
    if (popup.mQueueable && popup.mShown) {
      return popup.mPopup;
    }
  }

  return nullptr;
}
