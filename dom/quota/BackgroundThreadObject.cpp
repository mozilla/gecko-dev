/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BackgroundThreadObject.h"

#include "nsThreadUtils.h"
#include "mozilla/ipc/BackgroundParent.h"

namespace mozilla::dom::quota {

BackgroundThreadObject::BackgroundThreadObject()
    : mOwningThread(GetCurrentSerialEventTarget()) {
  AssertIsOnOwningThread();
}

BackgroundThreadObject::BackgroundThreadObject(
    nsISerialEventTarget* aOwningThread)
    : mOwningThread(aOwningThread) {}

#ifdef DEBUG

void BackgroundThreadObject::AssertIsOnOwningThread() const {
  mozilla::ipc::AssertIsOnBackgroundThread();
  MOZ_ASSERT(mOwningThread);
  bool current;
  MOZ_ASSERT(NS_SUCCEEDED(mOwningThread->IsOnCurrentThread(&current)));
  MOZ_ASSERT(current);
}

#endif  // DEBUG

nsISerialEventTarget* BackgroundThreadObject::OwningThread() const {
  MOZ_ASSERT(mOwningThread);
  return mOwningThread;
}

}  // namespace mozilla::dom::quota
