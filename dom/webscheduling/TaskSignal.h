/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_TaskSignal_h
#define mozilla_dom_TaskSignal_h

#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/dom/AbortSignal.h"
#include "mozilla/dom/WebTaskSchedulingBinding.h"

namespace mozilla::dom {
class TaskSignal final : public AbortSignal {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(TaskSignal, AbortSignal)

  IMPL_EVENT_HANDLER(prioritychange);

  static already_AddRefed<TaskSignal> Create(nsIGlobalObject* aGlobalObject,
                                             TaskPriority aPriority);

  static already_AddRefed<TaskSignal> Any(
      GlobalObject& aGlobal,
      const Sequence<OwningNonNull<AbortSignal>>& aSignals,
      const TaskSignalAnyInit& aInit);

  TaskPriority Priority() const { return mPriority; }

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override {
    return TaskSignal_Binding::Wrap(aCx, this, aGivenProto);
  }

  void SetPriority(TaskPriority aPriority) { mPriority = aPriority; }

  bool IsTaskSignal() const override { return true; }

  bool PriorityChanging() const { return mPriorityChanging; }

  void SetPriorityChanging(bool aPriorityChanging) {
    mPriorityChanging = aPriorityChanging;
  }

  void RunPriorityChangeAlgorithms();

  void SetWebTaskScheduler(WebTaskScheduler* aScheduler);

  // https://wicg.github.io/scheduling-apis/#tasksignal-has-fixed-priority
  bool HasFixedPriority() const { return mDependent && !mSourceTaskSignal; }

  nsTArray<RefPtr<TaskSignal>>& DependentTaskSignals() {
    return mDependentTaskSignals;
  }

 private:
  TaskSignal(nsIGlobalObject* aGlobal, TaskPriority aPriority)
      : AbortSignal(aGlobal, SignalAborted::No, JS::UndefinedHandleValue),
        mPriority(aPriority),
        mPriorityChanging(false) {
    AbortSignal::Init();
  }

  TaskPriority mPriority;

  // https://wicg.github.io/scheduling-apis/#tasksignal-priority-changing
  bool mPriorityChanging;

  nsTArray<WeakPtr<WebTaskScheduler>> mSchedulers;

  // https://wicg.github.io/scheduling-apis/#tasksignal-source-signal
  WeakPtr<TaskSignal> mSourceTaskSignal;

  // https://wicg.github.io/scheduling-apis/#tasksignal-dependent-signals
  nsTArray<RefPtr<TaskSignal>> mDependentTaskSignals;

  ~TaskSignal() = default;
};
}  // namespace mozilla::dom
#endif
