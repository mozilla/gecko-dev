/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AnimationFrameProvider.h"
#include "MainThreadUtils.h"
#include "mozilla/Assertions.h"
#include "mozilla/dom/HTMLVideoElement.h"

namespace mozilla::dom {

FrameRequestManager::FrameRequestManager() = default;
FrameRequestManager::~FrameRequestManager() = default;

void FrameRequestManager::Schedule(HTMLVideoElement* aElement) {
  if (!mVideoCallbacks.Contains(aElement)) {
    mVideoCallbacks.AppendElement(aElement);
  }
}

bool FrameRequestManager::Cancel(HTMLVideoElement* aElement) {
  return mVideoCallbacks.RemoveElement(aElement);
}

void FrameRequestManager::Unlink() {
  FrameRequestManagerBase::Unlink();
  mVideoCallbacks.Clear();
}

void FrameRequestManager::Traverse(nsCycleCollectionTraversalCallback& aCB) {
  FrameRequestManagerBase::Traverse(aCB);
  for (auto& i : mVideoCallbacks) {
    NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(
        aCB, "FrameRequestManager::mVideoCallbacks[i]");
    aCB.NoteXPCOMChild(ToSupports(i));
  }
}
void FrameRequestManager::Take(
    nsTArray<RefPtr<HTMLVideoElement>>& aVideoCallbacks) {
  MOZ_ASSERT(NS_IsMainThread());
  aVideoCallbacks = std::move(mVideoCallbacks);
}

}  // namespace mozilla::dom
