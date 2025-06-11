/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_WEBRTC_LIBWEBRTCGLUE_TASKQUEUEWRAPPER_H_
#define DOM_MEDIA_WEBRTC_LIBWEBRTCGLUE_TASKQUEUEWRAPPER_H_

#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/RefPtr.h"
#include "mozilla/UniquePtr.h"
#include "nsStringFwd.h"

class nsIEventTarget;

namespace webrtc {
class TaskQueueBase;
struct TaskQueueDeleter;
class TaskQueueFactory;
}  // namespace webrtc

namespace mozilla {
class TaskQueue;

/**
 * Creates a libwebrtc task queue backed by a mozilla::TaskQueue.
 *
 * While in a task running on the returned task queue, both
 * webrtc::TaskQueueBase::Current() and mozilla::AbstractThread::GetCurrent()
 * will work as expected.
 *
 * Releasing the returned task queue will synchronously shut down the underlying
 * mozilla::TaskQueue. Execution will be blocked until the underlying task queue
 * has finished running any pending tasks. The returned task queue must not be
 * released while on itself, or a deadlock will occur.
 */
std::unique_ptr<webrtc::TaskQueueBase, webrtc::TaskQueueDeleter>
CreateWebrtcTaskQueue(already_AddRefed<nsIEventTarget> aTarget,
                      const nsCString& aName, bool aSupportsTailDispatch);

/**
 * Creates a mozilla task queue that also exposes a webrtc::TaskQueueBase.
 *
 * While in a task running on the returned task queue, both
 * webrtc::TaskQueueBase::Current() and mozilla::AbstractThread::GetCurrent()
 * will work as expected.
 *
 * webrtc::TaskQueueBase is not refcounted and the representation here is only
 * accessible through webrtc::TaskQueueBase::Current(). The returned task queue
 * controls the lifetime of the webrtc::TaskQueueBase instance, which will be
 * destroyed as the returned task queue finishes shutdown. The thread on which
 * it is destroyed is not guaranteed.
 *
 * Shutdown of the returned task queue is asynchronous, either through
 * BeginShutdown(), or through releasing all references to it. See
 * mozilla::TaskQueue.
 */
RefPtr<TaskQueue> CreateWebrtcTaskQueueWrapper(
    already_AddRefed<nsIEventTarget> aTarget, const nsCString& aName,
    bool aSupportsTailDispatch);

/**
 * Creates a libwebrtc task queue factory that returns webrtc::TaskQueueBase
 * instances backed by mozilla::TaskQueues. See CreateWebrtcTaskQueue above.
 */
UniquePtr<webrtc::TaskQueueFactory> CreateWebrtcTaskQueueFactory();

}  // namespace mozilla

#endif
