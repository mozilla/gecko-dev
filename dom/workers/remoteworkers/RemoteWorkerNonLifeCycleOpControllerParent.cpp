/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RemoteWorkerNonLifeCycleOpControllerParent.h"
#include "mozilla/dom/RemoteWorkerController.h"

namespace mozilla::dom {

RemoteWorkerNonLifeCycleOpControllerParent::
    RemoteWorkerNonLifeCycleOpControllerParent(
        RemoteWorkerController* aController)
    : mController(aController) {
  MOZ_ASSERT(mController);
}

RemoteWorkerNonLifeCycleOpControllerParent::
    ~RemoteWorkerNonLifeCycleOpControllerParent() = default;

void RemoteWorkerNonLifeCycleOpControllerParent::Shutdown() {
  if (CanSend()) {
    Unused << SendShutdown();
  }
  if (mController) {
    mController = nullptr;
  }
  if (GetIPCChannel()) {
    GetIPCChannel()->Close();
  }
}

IPCResult RemoteWorkerNonLifeCycleOpControllerParent::RecvTerminated() {
  // mController could be nullptr when the controller had already shutted down.
  if (mController) {
    mController->mNonLifeCycleOpController = nullptr;
    mController = nullptr;
  }

  if (GetIPCChannel()) {
    GetIPCChannel()->Close();
  }

  return IPC_OK();
}

IPCResult RemoteWorkerNonLifeCycleOpControllerParent::RecvError(
    const ErrorValue& aError) {
  MOZ_ASSERT(mController);
  mController->ErrorPropagation(aError);
  return IPC_OK();
}

}  // namespace mozilla::dom
