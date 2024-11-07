/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_RemoteLazyInputStreamThread_h
#define mozilla_RemoteLazyInputStreamThread_h

#include "mozilla/RemoteLazyInputStreamChild.h"
#include "nsIEventTarget.h"
#include "nsIObserver.h"
#include "nsTArray.h"

class nsIThread;

namespace mozilla {

class RemoteLazyInputStreamChild;

// XXX Rename this class since it's used by LSNG too.
class RemoteLazyInputStreamThread final : public nsISerialEventTarget,
                                          public nsIDirectTaskDispatcher {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIEVENTTARGET_FULL
  NS_DECL_NSISERIALEVENTTARGET
  NS_DECL_NSIDIRECTTASKDISPATCHER

  explicit RemoteLazyInputStreamThread(
      MovingNotNull<nsCOMPtr<nsIThread>> aThread)
      : mThread(std::move(aThread)) {}

  static already_AddRefed<RemoteLazyInputStreamThread> Get();

  static already_AddRefed<RemoteLazyInputStreamThread> GetOrCreate();

 private:
  ~RemoteLazyInputStreamThread() = default;

  // As long as we can access gRemoteLazyThread, mThread remains a valid
  // object. We rely on it failing on late dispatch after its shutdown.
  const NotNull<nsCOMPtr<nsIThread>> mThread;
};

bool IsOnDOMFileThread();

void AssertIsOnDOMFileThread();

}  // namespace mozilla

#endif  // mozilla_RemoteLazyInputStreamThread_h
