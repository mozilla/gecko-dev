/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_PendingFullscreenEvent_h_
#define mozilla_PendingFullscreenEvent_h_

#include "mozilla/dom/Document.h"
#include "nsContentUtils.h"

namespace mozilla {

namespace dom {
class Document;
}

enum class FullscreenEventType {
  Change,
  Error,
};

/*
 * Class for dispatching a fullscreen event. It should be queued and
 * invoked as part of "run the fullscreen steps" algorithm.
 */
class PendingFullscreenEvent {
 public:
  PendingFullscreenEvent(FullscreenEventType aType, nsINode* aTarget)
      : mTarget(aTarget), mType(aType) {
    MOZ_ASSERT(aTarget);
  }

  MOZ_CAN_RUN_SCRIPT void Dispatch(dom::Document* aDoc) {
#ifdef DEBUG
    MOZ_ASSERT(!mDispatched);
    mDispatched = true;
#endif
    nsString name;
    switch (mType) {
      case FullscreenEventType::Change:
        name = u"fullscreenchange"_ns;
        break;
      case FullscreenEventType::Error:
        name = u"fullscreenerror"_ns;
        break;
    }
    nsINode* target = mTarget->GetComposedDoc() == aDoc ? mTarget.get() : aDoc;
    Unused << nsContentUtils::DispatchTrustedEvent(
        aDoc, target, name, CanBubble::eYes, Cancelable::eNo, Composed::eYes);
  }

  void Unlink() { mTarget = nullptr; }

  nsINode* Target() const { return mTarget.get(); }

 private:
  nsCOMPtr<nsINode> mTarget;
  FullscreenEventType mType;
#ifdef DEBUG
  bool mDispatched = false;
#endif
};

inline void ImplCycleCollectionTraverse(
    nsCycleCollectionTraversalCallback& aCallback,
    PendingFullscreenEvent& aField, const char* aName, uint32_t aFlags = 0) {
  CycleCollectionNoteChild(aCallback, aField.Target(), aName, aFlags);
}

inline void ImplCycleCollectionUnlink(PendingFullscreenEvent& aField) {
  aField.Unlink();
}

}  // namespace mozilla

#endif  // mozilla_PendingFullscreenEvent_h_
