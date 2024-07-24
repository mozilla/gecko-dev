/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_AnimationFrameProvider_h
#define mozilla_dom_AnimationFrameProvider_h

#include "MainThreadUtils.h"
#include "mozilla/Assertions.h"
#include "mozilla/dom/AnimationFrameProviderBinding.h"
#include "mozilla/dom/HTMLVideoElement.h"
#include "mozilla/dom/RequestCallbackManager.h"

namespace mozilla::dom {

using FrameRequest = RequestCallbackEntry<FrameRequestCallback>;
using FrameRequestManagerBase = RequestCallbackManager<FrameRequestCallback>;

class FrameRequestManager final : public FrameRequestManagerBase {
 public:
  FrameRequestManager() = default;
  ~FrameRequestManager() = default;

  using FrameRequestManagerBase::Cancel;
  using FrameRequestManagerBase::Schedule;
  using FrameRequestManagerBase::Take;

  void Schedule(HTMLVideoElement* aElement) {
    if (!mVideoCallbacks.Contains(aElement)) {
      mVideoCallbacks.AppendElement(aElement);
    }
  }

  bool Cancel(HTMLVideoElement* aElement) {
    return mVideoCallbacks.RemoveElement(aElement);
  }

  bool IsEmpty() const {
    return FrameRequestManagerBase::IsEmpty() && mVideoCallbacks.IsEmpty();
  }

  void Take(nsTArray<RefPtr<HTMLVideoElement>>& aVideoCallbacks) {
    MOZ_ASSERT(NS_IsMainThread());
    aVideoCallbacks = std::move(mVideoCallbacks);
  }

  void Unlink() {
    FrameRequestManagerBase::Unlink();
    mVideoCallbacks.Clear();
  }

  void Traverse(nsCycleCollectionTraversalCallback& aCB) {
    FrameRequestManagerBase::Traverse(aCB);
    for (auto& i : mVideoCallbacks) {
      NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(
          aCB, "FrameRequestManager::mVideoCallbacks[i]");
      aCB.NoteXPCOMChild(ToSupports(i));
    }
  }

 private:
  nsTArray<RefPtr<HTMLVideoElement>> mVideoCallbacks;
};

}  // namespace mozilla::dom

#endif
