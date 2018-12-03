/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_SchedulerGroup_h
#define mozilla_SchedulerGroup_h

#include "mozilla/AbstractEventQueue.h"
#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/LinkedList.h"
#include "mozilla/Queue.h"
#include "mozilla/TaskCategory.h"
#include "mozilla/ThreadLocal.h"
#include "mozilla/TimeStamp.h"
#include "nsCOMPtr.h"
#include "nsILabelableRunnable.h"
#include "nsISupportsImpl.h"
#include "nsThreadUtils.h"

class nsIEventTarget;
class nsIRunnable;
class nsISerialEventTarget;

namespace mozilla {
class AbstractThread;
namespace dom {
class DocGroup;
class TabGroup;
}  // namespace dom

#define NS_SCHEDULERGROUPRUNNABLE_IID                \
  {                                                  \
    0xd31b7420, 0x872b, 0x4cfb, {                    \
      0xa9, 0xc6, 0xae, 0x4c, 0x0f, 0x06, 0x36, 0x74 \
    }                                                \
  }

// The "main thread" in Gecko will soon be a set of cooperatively scheduled
// "fibers". Global state in Gecko will be partitioned into a series of "groups"
// (with roughly one group per tab). Runnables will be annotated with the set of
// groups that they touch. Two runnables may run concurrently on different
// fibers as long as they touch different groups.
//
// A SchedulerGroup is an abstract class to represent a "group". Essentially the
// only functionality offered by a SchedulerGroup is the ability to dispatch
// runnables to the group. TabGroup, DocGroup, and SystemGroup are the concrete
// implementations of SchedulerGroup.
class SchedulerGroup : public LinkedListElement<SchedulerGroup> {
 public:
  SchedulerGroup();

  NS_INLINE_DECL_PURE_VIRTUAL_REFCOUNTING

  // This method returns true if all members of the "group" are in a
  // "background" state.
  virtual bool IsBackground() const { return false; }

  // This function returns true if it's currently safe to run code associated
  // with this SchedulerGroup. It will return true either if we're inside an
  // unlabeled runnable or if we're inside a runnable labeled with this
  // SchedulerGroup.
  bool IsSafeToRun() const { return !sTlsValidatingAccess.get() || mIsRunning; }

  // This function returns true if it's currently safe to run unlabeled code
  // with no known SchedulerGroup. It will only return true if we're inside an
  // unlabeled runnable.
  static bool IsSafeToRunUnlabeled() { return !sTlsValidatingAccess.get(); }

  // Ensure that it's valid to access the TabGroup at this time.
  void ValidateAccess() const { MOZ_ASSERT(IsSafeToRun()); }

  enum EnqueueStatus {
    NewlyQueued,
    AlreadyQueued,
  };

  // Records that this SchedulerGroup had an event enqueued in some
  // queue. Returns whether the SchedulerGroup was already in a queue before
  // EnqueueEvent() was called.
  EnqueueStatus EnqueueEvent() {
    mEventCount++;
    return mEventCount == 1 ? NewlyQueued : AlreadyQueued;
  }

  enum DequeueStatus {
    StillQueued,
    NoLongerQueued,
  };

  // Records that this SchedulerGroup had an event dequeued from some
  // queue. Returns whether the SchedulerGroup is still in a queue after
  // DequeueEvent() returns.
  DequeueStatus DequeueEvent() {
    mEventCount--;
    return mEventCount == 0 ? NoLongerQueued : StillQueued;
  }

  class Runnable final : public mozilla::Runnable,
                         public nsIRunnablePriority,
                         public nsILabelableRunnable {
   public:
    Runnable(already_AddRefed<nsIRunnable>&& aRunnable, SchedulerGroup* aGroup,
             dom::DocGroup* aDocGroup);

    bool GetAffectedSchedulerGroups(SchedulerGroupSet& aGroups) override;

    SchedulerGroup* Group() const { return mGroup; }
    dom::DocGroup* DocGroup() const;

#ifdef MOZ_COLLECTING_RUNNABLE_TELEMETRY
    NS_IMETHOD GetName(nsACString& aName) override;
#endif

    bool IsBackground() const { return mGroup->IsBackground(); }

    NS_DECL_ISUPPORTS_INHERITED
    NS_DECL_NSIRUNNABLE
    NS_DECL_NSIRUNNABLEPRIORITY

    NS_DECLARE_STATIC_IID_ACCESSOR(NS_SCHEDULERGROUPRUNNABLE_IID);

   private:
    friend class SchedulerGroup;

    ~Runnable() = default;

    nsCOMPtr<nsIRunnable> mRunnable;
    RefPtr<SchedulerGroup> mGroup;
    RefPtr<dom::DocGroup> mDocGroup;
  };
  friend class Runnable;

  bool* GetValidAccessPtr() { return &mIsRunning; }

  virtual nsresult Dispatch(TaskCategory aCategory,
                            already_AddRefed<nsIRunnable>&& aRunnable);

  virtual nsISerialEventTarget* EventTargetFor(TaskCategory aCategory) const;

  // Must always be called on the main thread. The returned AbstractThread can
  // always be used off the main thread.
  AbstractThread* AbstractMainThreadFor(TaskCategory aCategory);

  // This method performs a safe cast. It returns null if |this| is not of the
  // requested type.
  virtual dom::TabGroup* AsTabGroup() { return nullptr; }

  static nsresult UnlabeledDispatch(TaskCategory aCategory,
                                    already_AddRefed<nsIRunnable>&& aRunnable);

  static void MarkVsyncReceived();

  static void MarkVsyncRan();

  void SetIsRunning(bool aIsRunning) { mIsRunning = aIsRunning; }
  bool IsRunning() const { return mIsRunning; }

  enum ValidationType {
    StartValidation,
    EndValidation,
  };
  static void SetValidatingAccess(ValidationType aType);

  struct EpochQueueEntry {
    nsCOMPtr<nsIRunnable> mRunnable;
    uintptr_t mEpochNumber;

    EpochQueueEntry(already_AddRefed<nsIRunnable> aRunnable, uintptr_t aEpoch)
        : mRunnable(aRunnable), mEpochNumber(aEpoch) {}
  };

  using RunnableEpochQueue = Queue<EpochQueueEntry, 32>;

  RunnableEpochQueue& GetQueue(mozilla::EventPriority aPriority) {
    return mEventQueues[size_t(aPriority)];
  }

 protected:
  nsresult DispatchWithDocGroup(TaskCategory aCategory,
                                already_AddRefed<nsIRunnable>&& aRunnable,
                                dom::DocGroup* aDocGroup);

  static nsresult InternalUnlabeledDispatch(
      TaskCategory aCategory, already_AddRefed<Runnable>&& aRunnable);

  // Implementations are guaranteed that this method is called on the main
  // thread.
  virtual AbstractThread* AbstractMainThreadForImpl(TaskCategory aCategory);

  // Helper method to create an event target specific to a particular
  // TaskCategory.
  virtual already_AddRefed<nsISerialEventTarget> CreateEventTargetFor(
      TaskCategory aCategory);

  // Given an event target returned by |dispatcher->CreateEventTargetFor|, this
  // function returns |dispatcher|.
  static SchedulerGroup* FromEventTarget(nsIEventTarget* aEventTarget);

  nsresult LabeledDispatch(TaskCategory aCategory,
                           already_AddRefed<nsIRunnable>&& aRunnable,
                           dom::DocGroup* aDocGroup);

  void CreateEventTargets(bool aNeedValidation);

  // Shuts down this dispatcher. If aXPCOMShutdown is true, invalidates this
  // dispatcher.
  void Shutdown(bool aXPCOMShutdown);

  static MOZ_THREAD_LOCAL(bool) sTlsValidatingAccess;

  bool mIsRunning;

  // Number of events that are currently enqueued for this SchedulerGroup
  // (across all queues).
  size_t mEventCount = 0;

  nsCOMPtr<nsISerialEventTarget> mEventTargets[size_t(TaskCategory::Count)];
  RefPtr<AbstractThread> mAbstractThreads[size_t(TaskCategory::Count)];
  RunnableEpochQueue mEventQueues[size_t(mozilla::EventPriority::Count)];
};

NS_DEFINE_STATIC_IID_ACCESSOR(SchedulerGroup::Runnable,
                              NS_SCHEDULERGROUPRUNNABLE_IID);

}  // namespace mozilla

#endif  // mozilla_SchedulerGroup_h
