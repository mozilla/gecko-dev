/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/UniqueContentParentKeepAlive.h"
#include "mozilla/dom/ContentParent.h"

namespace mozilla::dom {

void ContentParentKeepAliveDeleter::operator()(ContentParent* aProcess) {
  AssertIsOnMainThread();
  if (RefPtr<ContentParent> process = dont_AddRef(aProcess)) {
    process->RemoveKeepAlive(mBrowserId);
  }
}

void ContentParentKeepAliveDeleter::operator()(
    ThreadsafeContentParentHandle* aHandle) {
  if (RefPtr<ThreadsafeContentParentHandle> handle = dont_AddRef(aHandle)) {
    NS_DispatchToMainThread(NS_NewRunnableFunction(
        "ThreadsafeContentParentKeepAliveDeleter",
        [handle = std::move(handle), browserId = mBrowserId]() {
          AssertIsOnMainThread();
          if (RefPtr<ContentParent> process = handle->GetContentParent()) {
            process->RemoveKeepAlive(browserId);
          }
        }));
  }
}

UniqueContentParentKeepAlive UniqueContentParentKeepAliveFromThreadsafe(
    UniqueThreadsafeContentParentKeepAlive aKeepAlive) {
  AssertIsOnMainThread();
  if (aKeepAlive) {
    uint64_t browserId = aKeepAlive.get_deleter().mBrowserId;
    RefPtr<ThreadsafeContentParentHandle> handle =
        dont_AddRef(aKeepAlive.release());
    RefPtr<ContentParent> process = handle->GetContentParent();
    return UniqueContentParentKeepAlive{process.forget().take(),
                                        {.mBrowserId = browserId}};
  }
  return nullptr;
}

UniqueThreadsafeContentParentKeepAlive UniqueContentParentKeepAliveToThreadsafe(
    UniqueContentParentKeepAlive aKeepAlive) {
  AssertIsOnMainThread();
  if (aKeepAlive) {
    uint64_t browserId = aKeepAlive.get_deleter().mBrowserId;
    RefPtr<ContentParent> process = dont_AddRef(aKeepAlive.release());
    RefPtr<ThreadsafeContentParentHandle> handle = process->ThreadsafeHandle();
    return UniqueThreadsafeContentParentKeepAlive{handle.forget().take(),
                                                  {.mBrowserId = browserId}};
  }
  return nullptr;
}

}  // namespace mozilla::dom
