/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RemoteWorkerNonLifeCycleOpControllerChild.h"
#include "mozilla/dom/WorkerCommon.h"
#include "mozilla/dom/WorkerRef.h"

namespace mozilla::dom {

using remoteworker::Canceled;
using remoteworker::Killed;
using remoteworker::Running;

/* static */
RefPtr<RemoteWorkerNonLifeCycleOpControllerChild>
RemoteWorkerNonLifeCycleOpControllerChild::Create() {
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(GetCurrentThreadWorkerPrivate());

  RefPtr<RemoteWorkerNonLifeCycleOpControllerChild> actor =
      MakeAndAddRef<RemoteWorkerNonLifeCycleOpControllerChild>();
  return actor;
}

RemoteWorkerNonLifeCycleOpControllerChild::
    RemoteWorkerNonLifeCycleOpControllerChild()
    : mState(VariantType<remoteworker::Running>(),
             "RemoteWorkerNonLifeCycleOpControllerChild") {}

RemoteWorkerNonLifeCycleOpControllerChild::
    ~RemoteWorkerNonLifeCycleOpControllerChild() = default;

void RemoteWorkerNonLifeCycleOpControllerChild::TransistionStateToCanceled() {
  auto lock = mState.Lock();
  MOZ_ASSERT(lock->is<Running>());

  /*Canceling pending/processing operations here*/

  *lock = VariantType<Canceled>();
}

void RemoteWorkerNonLifeCycleOpControllerChild::TransistionStateToKilled() {
  auto lock = mState.Lock();
  MOZ_ASSERT(lock->is<Canceled>());
  Unused << SendTerminated();
  *lock = VariantType<Killed>();
}

IPCResult RemoteWorkerNonLifeCycleOpControllerChild::RecvShutdown() {
  return IPC_OK();
}

}  // namespace mozilla::dom
