/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RemoteWorkerNonLifeCycleOpControllerChild.h"
#include "mozilla/dom/ServiceWorkerOp.h"
#include "mozilla/dom/SharedWorkerOp.h"
#include "mozilla/dom/WorkerCommon.h"
#include "mozilla/dom/WorkerPrivate.h"
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

  *lock = VariantType<Canceled>();
}

void RemoteWorkerNonLifeCycleOpControllerChild::TransistionStateToKilled() {
  auto lock = mState.Lock();
  MOZ_ASSERT(lock->is<Canceled>());
  *lock = VariantType<Killed>();

  if (!CanSend()) {
    return;
  }
  Unused << SendTerminated();
  if (GetIPCChannel()) {
    GetIPCChannel()->Close();
  }
}

void RemoteWorkerNonLifeCycleOpControllerChild::ErrorPropagation(
    nsresult aError) {
  if (!CanSend()) {
    return;
  }
  Unused << SendError(aError);
}

void RemoteWorkerNonLifeCycleOpControllerChild::StartOp(
    RefPtr<RemoteWorkerOp>&& aOp) {
  MOZ_ASSERT(aOp);
  auto lock = mState.Lock();
  // ServiceWorkerOp/SharedWorkerOp handles the Canceled/Killed state cases.
  aOp->Start(this, lock.ref());
}

IPCResult RemoteWorkerNonLifeCycleOpControllerChild::RecvExecOp(
    SharedWorkerOpArgs&& aOpArgs) {
  MOZ_ASSERT(aOpArgs.type() ==
             SharedWorkerOpArgs::TSharedWorkerPortIdentifierOpArgs);
  StartOp(new SharedWorkerOp(std::move(aOpArgs)));

  return IPC_OK();
}

IPCResult RemoteWorkerNonLifeCycleOpControllerChild::RecvExecServiceWorkerOp(
    ServiceWorkerOpArgs&& aOpArgs, ExecServiceWorkerOpResolver&& aResolve) {
  MOZ_ASSERT(
      aOpArgs.type() !=
          ServiceWorkerOpArgs::TParentToChildServiceWorkerFetchEventOpArgs,
      "FetchEvent operations should be sent via PFetchEventOp(Proxy) actors!");

  MOZ_ASSERT(aOpArgs.type() !=
                 ServiceWorkerOpArgs::TServiceWorkerTerminateWorkerOpArgs,
             "Terminate operations should be sent via PRemoteWorker actros!");

  StartOp(ServiceWorkerOp::Create(std::move(aOpArgs), std::move(aResolve)));
  return IPC_OK();
}

IPCResult RemoteWorkerNonLifeCycleOpControllerChild::RecvShutdown() {
  if (GetIPCChannel()) {
    GetIPCChannel()->Close();
  }
  return IPC_OK();
}

}  // namespace mozilla::dom
