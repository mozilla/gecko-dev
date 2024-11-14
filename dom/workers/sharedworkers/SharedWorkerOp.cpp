/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SharedWorkerOp.h"
#include "mozilla/dom/MessagePort.h"
#include "mozilla/dom/RemoteWorkerChild.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/dom/WorkerRunnable.h"
#include "mozilla/dom/WorkerScope.h"

namespace mozilla::dom {

using remoteworker::Canceled;
using remoteworker::Killed;

namespace {

// Normal runnable because AddPortIdentifier() is going to exec JS code.
class MessagePortIdentifierRunnable final : public WorkerThreadRunnable {
 public:
  MessagePortIdentifierRunnable(RemoteWorkerChild* aActor,
                                const MessagePortIdentifier& aPortIdentifier)
      : WorkerThreadRunnable("MessagePortIdentifierRunnable"),
        mActor(aActor),
        mPortIdentifier(aPortIdentifier) {}

 private:
  bool WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override {
    if (aWorkerPrivate->GlobalScope()->IsDying()) {
      mPortIdentifier.ForceClose();
      return true;
    }
    mActor->AddPortIdentifier(aCx, aWorkerPrivate, mPortIdentifier);
    return true;
  }

  RefPtr<RemoteWorkerChild> mActor;
  UniqueMessagePortId mPortIdentifier;
};

}  // namespace

SharedWorkerOp::SharedWorkerOp(SharedWorkerOpArgs&& aArgs)
    : mOpArgs(std::move(aArgs)) {}

SharedWorkerOp::~SharedWorkerOp() { MOZ_ASSERT(mStarted); }

void SharedWorkerOp::Cancel() {
#ifdef DEBUG
  mStarted = true;
#endif
}

bool SharedWorkerOp::MaybeStart(RemoteWorkerChild* aOwner,
                                RemoteWorkerState& aState) {
  MOZ_ASSERT(!mStarted);
  MOZ_ASSERT(aOwner);
  // Thread: We are on the Worker Launcher thread.

  // Return false, indicating we should queue this op if our current state is
  // pending and this isn't a termination op (which should skip the line).
  if (aState.is<Pending>() && !IsTerminationOp()) {
    return false;
  }

  // If the worker is already shutting down (which should be unexpected
  // because we should be told new operations after a termination op), just
  // return true to indicate the op should be discarded.
  if (aState.is<Canceled>() || aState.is<Killed>()) {
#ifdef DEBUG
    mStarted = true;
#endif
    if (mOpArgs.type() ==
        SharedWorkerOpArgs::TSharedWorkerPortIdentifierOpArgs) {
      MessagePort::ForceClose(
          mOpArgs.get_SharedWorkerPortIdentifierOpArgs().portIdentifier());
    }
    return true;
  }

  MOZ_ASSERT(aState.is<Running>() || IsTerminationOp());

  RefPtr<SharedWorkerOp> self = this;
  RefPtr<RemoteWorkerChild> owner = aOwner;

  nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction(
      __func__, [self = std::move(self), owner = std::move(owner)]() mutable {
        {
          auto lock = owner->mState.Lock();

          if (NS_WARN_IF(lock->is<Canceled>() || lock->is<Killed>())) {
            self->Cancel();
            // Worker has already canceled, force close the MessagePort.
            if (self->mOpArgs.type() ==
                SharedWorkerOpArgs::TSharedWorkerPortIdentifierOpArgs) {
              MessagePort::ForceClose(
                  self->mOpArgs.get_SharedWorkerPortIdentifierOpArgs()
                      .portIdentifier());
            }
            return;
          }
        }

        self->StartOnMainThread(owner);
      });

  MOZ_ALWAYS_SUCCEEDS(SchedulerGroup::Dispatch(r.forget()));

#ifdef DEBUG
  mStarted = true;
#endif

  return true;
}

void SharedWorkerOp::StartOnMainThread(RefPtr<RemoteWorkerChild>& aOwner) {
  AssertIsOnMainThread();

  if (IsTerminationOp()) {
    aOwner->CloseWorkerOnMainThread();
    return;
  }

  auto lock = aOwner->mState.Lock();
  MOZ_ASSERT(lock->is<Running>());
  if (!lock->is<Running>()) {
    aOwner->ErrorPropagationDispatch(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  RefPtr<WorkerPrivate> workerPrivate = lock->as<Running>().mWorkerPrivate;

  MOZ_ASSERT(workerPrivate);

  if (mOpArgs.type() == SharedWorkerOpArgs::TSharedWorkerSuspendOpArgs) {
    workerPrivate->ParentWindowPaused();
  } else if (mOpArgs.type() == SharedWorkerOpArgs::TSharedWorkerResumeOpArgs) {
    workerPrivate->ParentWindowResumed();
  } else if (mOpArgs.type() == SharedWorkerOpArgs::TSharedWorkerFreezeOpArgs) {
    workerPrivate->Freeze(nullptr);
  } else if (mOpArgs.type() == SharedWorkerOpArgs::TSharedWorkerThawOpArgs) {
    workerPrivate->Thaw(nullptr);
  } else if (mOpArgs.type() ==
             SharedWorkerOpArgs::TSharedWorkerPortIdentifierOpArgs) {
    RefPtr<MessagePortIdentifierRunnable> r = new MessagePortIdentifierRunnable(
        aOwner,
        mOpArgs.get_SharedWorkerPortIdentifierOpArgs().portIdentifier());
    if (NS_WARN_IF(!r->Dispatch(workerPrivate))) {
      aOwner->ErrorPropagationDispatch(NS_ERROR_FAILURE);
    }
  } else if (mOpArgs.type() ==
             SharedWorkerOpArgs::TSharedWorkerAddWindowIDOpArgs) {
    aOwner->mWindowIDs.AppendElement(
        mOpArgs.get_SharedWorkerAddWindowIDOpArgs().windowID());
  } else if (mOpArgs.type() ==
             SharedWorkerOpArgs::TSharedWorkerRemoveWindowIDOpArgs) {
    aOwner->mWindowIDs.RemoveElement(
        mOpArgs.get_SharedWorkerRemoveWindowIDOpArgs().windowID());
  } else {
    MOZ_CRASH("Unknown SharedWorkerOpArgs type!");
  }
}

bool SharedWorkerOp::IsTerminationOp() const {
  return mOpArgs.type() == SharedWorkerOpArgs::TSharedWorkerTerminateOpArgs;
}

}  // namespace mozilla::dom
