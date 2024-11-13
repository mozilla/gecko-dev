/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_RemoteWorkerOp_h
#define mozilla_dom_RemoteWorkerOp_h

#include "mozilla/RefPtr.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/Variant.h"

namespace mozilla::dom {

class RemoteWorkerChild;
class RemtoeWorkerNonfLifeCycleOpControllerChild;
class RemoteWorkerOp;
// class RemoteWorkerNonLifeCycleOpControllerChild;

namespace remoteworker {

struct WorkerPrivateAccessibleState {
  ~WorkerPrivateAccessibleState();
  RefPtr<WorkerPrivate> mWorkerPrivate;
};

// Initial state, mWorkerPrivate is initially null but will be initialized on
// the main thread by ExecWorkerOnMainThread when the WorkerPrivate is
// created.  The state will transition to Running or Canceled, also from the
// main thread.
struct Pending : WorkerPrivateAccessibleState {
  nsTArray<RefPtr<RemoteWorkerOp>> mPendingOps;
};

// Running, with the state transition happening on the main thread as a result
// of the worker successfully processing our initialization runnable,
// indicating that top-level script execution successfully completed.  Because
// all of our state transitions happen on the main thread and are posed in
// terms of the main thread's perspective of the worker's state, it's very
// possible for us to skip directly from Pending to Canceled because we decide
// to cancel/terminate the worker prior to it finishing script loading or
// reporting back to us.
struct Running : WorkerPrivateAccessibleState {};

// Cancel() has been called on the WorkerPrivate on the main thread by a
// TerminationOp, top-level script evaluation has failed and canceled the
// worker, or in the case of a SharedWorker, close() has been called on
// the global scope by content code and the worker has advanced to the
// Canceling state.  (Dedicated Workers can also self close, but they will
// never be RemoteWorkers.  Although a SharedWorker can own DedicatedWorkers.)
// Browser shutdown will result in a TerminationOp thanks to use of a shutdown
// blocker in the parent, so the RuntimeService shouldn't get involved, but we
// would also handle that case acceptably too.
//
// Because worker self-closing is still handled by dispatching a runnable to
// the main thread to effectively call WorkerPrivate::Cancel(), there isn't
// a race between a worker deciding to self-close and our termination ops.
//
// In this state, we have dropped the reference to the WorkerPrivate and will
// no longer be dispatching runnables to the worker.  We wait in this state
// until the termination lambda is invoked letting us know that the worker has
// entirely shutdown and we can advanced to the Killed state.
struct Canceled {};

// The worker termination lambda has been invoked and we know the Worker is
// entirely shutdown.  (Inherently it is possible for us to advance to this
// state while the nsThread for the worker is still in the process of
// shutting down, but no more worker code will run on it.)
//
// This name is chosen to match the Worker's own state model.
struct Killed {};

using RemoteWorkerState = Variant<Pending, Running, Canceled, Killed>;

}  // namespace remoteworker

class RemoteWorkerOp {
 public:
  NS_INLINE_DECL_PURE_VIRTUAL_REFCOUNTING

  virtual ~RemoteWorkerOp() = default;

  virtual bool MaybeStart(RemoteWorkerChild* aOwner,
                          remoteworker::RemoteWorkerState& aState) = 0;

  virtual void StartOnMainThread(RefPtr<RemoteWorkerChild>& aOwner) = 0;

  virtual void Start(RemoteWorkerNonLifeCycleOpControllerChild* aOwner,
                     remoteworker::RemoteWorkerState& aState) = 0;

  virtual void Cancel() = 0;
};

}  // namespace mozilla::dom

#endif
