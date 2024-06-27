/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RemoteWorkerNonLifeCycleOpControllerChild.h"
#include "mozilla/dom/WorkerCommon.h"
#include "mozilla/dom/WorkerRef.h"

namespace mozilla::dom {

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
    RemoteWorkerNonLifeCycleOpControllerChild() = default;

RemoteWorkerNonLifeCycleOpControllerChild::
    ~RemoteWorkerNonLifeCycleOpControllerChild() = default;

IPCResult RemoteWorkerNonLifeCycleOpControllerChild::RecvShutdown() {
  return IPC_OK();
}

}  // namespace mozilla::dom
