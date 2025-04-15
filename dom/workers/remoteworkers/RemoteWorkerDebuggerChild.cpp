/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RemoteWorkerDebuggerChild.h"
#include "mozilla/dom/WorkerCommon.h"
#include "mozilla/dom/WorkerPrivate.h"

namespace mozilla::dom {

RemoteWorkerDebuggerChild::RemoteWorkerDebuggerChild(
    WorkerPrivate* aWorkerPrivate)
    : mWorkerPrivate(aWorkerPrivate) {
  MOZ_ASSERT_DEBUG_OR_FUZZING(mWorkerPrivate);
  mWorkerPrivate->AssertIsOnWorkerThread();
}

RemoteWorkerDebuggerChild::~RemoteWorkerDebuggerChild() {
  MOZ_ASSERT_DEBUG_OR_FUZZING(!mWorkerPrivate);
}

mozilla::ipc::IPCResult RemoteWorkerDebuggerChild::RecvInitialize(
    const nsString& aURL) {
  // Send a WorkerDebuggerRunnable to compile debugger script.
  Unused << SendSetAsInitialized();
  return IPC_OK();
}

mozilla::ipc::IPCResult RemoteWorkerDebuggerChild::RecvPostMessage(
    const nsString& aMessage) {
  // Send a WorkerDebuggerRunnable to fire a message event on DebuggerGlobal.
  return IPC_OK();
}

mozilla::ipc::IPCResult RemoteWorkerDebuggerChild::RecvSetDebuggerReady(
    const bool& aReady) {
  // Should call mWorkerPrivate->SetIsDebuggerReady();
  return IPC_OK();
}

}  // namespace mozilla::dom
