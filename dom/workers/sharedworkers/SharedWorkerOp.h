/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_sharedworkerop_h__
#define mozilla_dom_sharedworkerop_h__

#include "mozilla/SchedulerGroup.h"

#include "mozilla/dom/RemoteWorkerOp.h"
#include "mozilla/dom/SharedWorkerOpArgs.h"

namespace mozilla::dom {

using remoteworker::RemoteWorkerState;

class SharedWorkerOp : public RemoteWorkerOp {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(SharedWorkerOp, override)

  explicit SharedWorkerOp(SharedWorkerOpArgs&& aArgs);

  bool MaybeStart(RemoteWorkerChild* aOwner,
                  RemoteWorkerState& aState) override;

  void StartOnMainThread(RefPtr<RemoteWorkerChild>& aOwner) final;

  void Start(RemoteWorkerNonLifeCycleOpControllerChild* aOwner,
             RemoteWorkerState& aState) final;

  void Cancel() override;

 private:
  ~SharedWorkerOp();

  bool IsTerminationOp() const;

  SharedWorkerOpArgs mOpArgs;

#ifdef DEBUG
  bool mStarted = false;
#endif
};

}  // namespace mozilla::dom

#endif
