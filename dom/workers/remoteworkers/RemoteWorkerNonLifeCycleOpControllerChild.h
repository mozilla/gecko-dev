/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_RemoteWorkerNonLifeCycleOpControllerChild_h
#define mozilla_dom_RemoteWorkerNonLifeCycleOpControllerChild_h

#include "mozilla/DataMutex.h"
#include "mozilla/dom/PRemoteWorkerNonLifeCycleOpControllerChild.h"
#include "mozilla/dom/RemoteWorkerOp.h"
#include "mozilla/dom/ServiceWorkerOpArgs.h"
#include "mozilla/dom/SharedWorkerOpArgs.h"
#include "nsISupportsImpl.h"

using mozilla::ipc::IPCResult;

namespace mozilla::dom {

using remoteworker::RemoteWorkerState;

class ServiceWorkerOp;

class RemoteWorkerNonLifeCycleOpControllerChild final
    : public PRemoteWorkerNonLifeCycleOpControllerChild {
  friend class PRemoteWorkerNonLifeCycleOpControllerChild;
  friend class ServiceWorkerOp;

 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(
      RemoteWorkerNonLifeCycleOpControllerChild, final)

  static RefPtr<RemoteWorkerNonLifeCycleOpControllerChild> Create();

  RemoteWorkerNonLifeCycleOpControllerChild();

  IPCResult RecvExecOp(SharedWorkerOpArgs&& aOpArgs);

  IPCResult RecvExecServiceWorkerOp(ServiceWorkerOpArgs&& aOpArgs,
                                    ExecServiceWorkerOpResolver&& aResolve);

  IPCResult RecvShutdown();

  void TransistionStateToCanceled();
  void TransistionStateToKilled();

  void ErrorPropagation(nsresult aError);

 private:
  void StartOp(RefPtr<RemoteWorkerOp>&& aOp);
  ~RemoteWorkerNonLifeCycleOpControllerChild();

  DataMutex<RemoteWorkerState> mState;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_RemoteWorkerNonLifeCycleOpControllerChild_h
