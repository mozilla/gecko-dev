/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_RemoteWorkerNonLifeCycleOpControllerChild_h
#define mozilla_dom_RemoteWorkerNonLifeCycleOpControllerChild_h

#include "mozilla/dom/PRemoteWorkerNonLifeCycleOpControllerChild.h"
#include "nsISupportsImpl.h"

using mozilla::ipc::IPCResult;

namespace mozilla::dom {

class RemoteWorkerNonLifeCycleOpControllerChild final
    : public PRemoteWorkerNonLifeCycleOpControllerChild {
 public:
  NS_INLINE_DECL_REFCOUNTING(RemoteWorkerNonLifeCycleOpControllerChild, final)

  static RefPtr<RemoteWorkerNonLifeCycleOpControllerChild> Create();

  RemoteWorkerNonLifeCycleOpControllerChild();

  IPCResult RecvShutdown();

 private:
  ~RemoteWorkerNonLifeCycleOpControllerChild();
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_RemoteWorkerNonLifeCycleOpControllerChild_h
