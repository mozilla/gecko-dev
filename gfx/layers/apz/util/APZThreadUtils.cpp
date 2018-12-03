/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/APZThreadUtils.h"

namespace mozilla {
namespace layers {

static bool sThreadAssertionsEnabled = true;
static MessageLoop* sControllerThread;

/*static*/ void APZThreadUtils::SetThreadAssertionsEnabled(bool aEnabled) {
  sThreadAssertionsEnabled = aEnabled;
}

/*static*/ bool APZThreadUtils::GetThreadAssertionsEnabled() {
  return sThreadAssertionsEnabled;
}

/*static*/ void APZThreadUtils::SetControllerThread(MessageLoop* aLoop) {
  // We must either be setting the initial controller thread, or removing it,
  // or re-using an existing controller thread.
  MOZ_ASSERT(!sControllerThread || !aLoop || sControllerThread == aLoop);
  sControllerThread = aLoop;
}

/*static*/ void APZThreadUtils::AssertOnControllerThread() {
  if (!GetThreadAssertionsEnabled()) {
    return;
  }

  MOZ_ASSERT(sControllerThread == MessageLoop::current());
}

/*static*/ void APZThreadUtils::RunOnControllerThread(
    RefPtr<Runnable>&& aTask) {
  RefPtr<Runnable> task = std::move(aTask);

  if (!sControllerThread) {
    // Could happen on startup
    NS_WARNING("Dropping task posted to controller thread");
    return;
  }

  if (sControllerThread == MessageLoop::current()) {
    task->Run();
  } else {
    sControllerThread->PostTask(task.forget());
  }
}

/*static*/ bool APZThreadUtils::IsControllerThread() {
  return sControllerThread == MessageLoop::current();
}

NS_IMPL_ISUPPORTS(GenericNamedTimerCallbackBase, nsITimerCallback, nsINamed)

}  // namespace layers
}  // namespace mozilla
