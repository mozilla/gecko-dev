/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RemoteWorkerDebuggerManagerParent.h"
#include "RemoteWorkerDebuggerParent.h"
#include "mozilla/dom/WorkerDebuggerManager.h"

namespace mozilla::dom {

RemoteWorkerDebuggerManagerParent::RemoteWorkerDebuggerManagerParent() {
  MOZ_ASSERT_DEBUG_OR_FUZZING(XRE_IsParentProcess() && NS_IsMainThread());
}

RemoteWorkerDebuggerManagerParent::~RemoteWorkerDebuggerManagerParent() {
  MOZ_ASSERT_DEBUG_OR_FUZZING(XRE_IsParentProcess() && NS_IsMainThread());
}

mozilla::ipc::IPCResult RemoteWorkerDebuggerManagerParent::RecvRegister(
    const RemoteWorkerDebuggerInfo& aDebuggerInfo,
    mozilla::ipc::Endpoint<PRemoteWorkerDebuggerParent>&& aParentEp) {
  MOZ_ASSERT_DEBUG_OR_FUZZING(XRE_IsParentProcess() && NS_IsMainThread());
  RefPtr<WorkerDebuggerManager> manager = WorkerDebuggerManager::Get();
  MOZ_ASSERT_DEBUG_OR_FUZZING(manager);

  nsCOMPtr<nsIWorkerDebugger> debugger =
      manager->GetDebuggerById(aDebuggerInfo.Id());
  MOZ_ASSERT_DEBUG_OR_FUZZING(!debugger);

  debugger = MakeRefPtr<RemoteWorkerDebuggerParent>(aDebuggerInfo,
                                                    std::move(aParentEp));

  manager->RegisterDebugger(debugger);

  return IPC_OK();
}

}  // end of namespace mozilla::dom
