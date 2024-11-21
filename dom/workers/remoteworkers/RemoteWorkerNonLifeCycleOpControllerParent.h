/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_RemoteWorkerNonLifeCycleOpControllerParent_h
#define mozilla_dom_RemoteWorkerNonLifeCycleOpControllerParent_h

#include "mozilla/dom/PRemoteWorkerNonLifeCycleOpControllerParent.h"

using mozilla::ipc::IPCResult;

namespace mozilla::dom {

class RemoteWorkerController;

class RemoteWorkerNonLifeCycleOpControllerParent final
    : public PRemoteWorkerNonLifeCycleOpControllerParent {
 public:
  NS_INLINE_DECL_REFCOUNTING(RemoteWorkerNonLifeCycleOpControllerParent,
                             override);

  explicit RemoteWorkerNonLifeCycleOpControllerParent(
      RemoteWorkerController* aRemoteWorkerController);

  IPCResult RecvTerminated();

  IPCResult RecvError(const ErrorValue& aError);

  void Shutdown();

 private:
  ~RemoteWorkerNonLifeCycleOpControllerParent();

  RefPtr<RemoteWorkerController> mController;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_RemoteWorkerNonLifeCycleOpControllerParent_h
