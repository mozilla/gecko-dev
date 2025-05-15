/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TaskSignal.h"
#include "WebTaskScheduler.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_INHERITED(TaskSignal, AbortSignal,
                                   mDependentTaskSignals)

NS_IMPL_ISUPPORTS_CYCLE_COLLECTION_INHERITED_0(TaskSignal, AbortSignal)

already_AddRefed<TaskSignal> TaskSignal::Create(nsIGlobalObject* aGlobalObject,
                                                TaskPriority aPriority) {
  return do_AddRef(new TaskSignal(aGlobalObject, aPriority));
}

void TaskSignal::RunPriorityChangeAlgorithms() {
  for (const WeakPtr<WebTaskScheduler>& scheduler : mSchedulers) {
    if (scheduler) {
      scheduler->RunTaskSignalPriorityChange(this);
    }
  }
}
void TaskSignal::SetWebTaskScheduler(WebTaskScheduler* aScheduler) {
  mSchedulers.AppendElement(aScheduler);
}

// https://wicg.github.io/scheduling-apis/#create-a-dependent-task-signal
/* static */
already_AddRefed<TaskSignal> TaskSignal::Any(
    GlobalObject& aGlobal, const Sequence<OwningNonNull<AbortSignal>>& aSignals,
    const TaskSignalAnyInit& aInit) {
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aGlobal.GetAsSupports());
  // 1. Let resultSignal be the result of creating a dependent signal from
  // signals using the TaskSignal interface and realm.
  RefPtr<AbortSignal> abortSignal =
      AbortSignal::Any(global, aSignals, [](nsIGlobalObject* aGlobal) {
        // Use User_visible as a temporary priority for now, it'll alwasy be set
        // again later.
        RefPtr<TaskSignal> signal =
            new TaskSignal(aGlobal, TaskPriority::User_visible);
        return signal.forget();
      });

  if (!abortSignal) {
    return nullptr;
  }

  RefPtr<TaskSignal> resultSignal = static_cast<TaskSignal*>(abortSignal.get());

  // 2. Set resultSignal’s dependent to true.
  resultSignal->mDependent = true;

  // 3. If init["priority"] is a TaskPriority, then:
  if (aInit.mPriority.IsTaskPriority()) {
    // 3.1 Set resultSignal’s priority to init["priority"].
    resultSignal->SetPriority(aInit.mPriority.GetAsTaskPriority());
    return resultSignal.forget();
  }

  // 4. Otherwise:
  // 4.1. Let sourceSignal be init["priority"].
  OwningNonNull<TaskSignal> sourceSignal = aInit.mPriority.GetAsTaskSignal();

  // 4.2. Set resultSignal’s priority to sourceSignal’s priority.
  resultSignal->SetPriority(sourceSignal->Priority());

  // 4.3 If sourceSignal does not have fixed priority, then:
  if (!sourceSignal->HasFixedPriority()) {
    // 4.3.1 If sourceSignal’s dependent is true, then set sourceSignal to
    // sourceSignal’s source signal
    if (sourceSignal->mDependent) {
      sourceSignal = sourceSignal->mSourceTaskSignal;
    }
    // 4.3.2. Assert: sourceSignal is not dependent.
    MOZ_ASSERT(!sourceSignal->mDependent);
    // 4.3.3. Set resultSignal’s source signal to a weak reference to
    // sourceSignal.
    resultSignal->mSourceTaskSignal = sourceSignal;
    // 4.3.4. Append resultSignal to sourceSignal’s dependent signals.
    sourceSignal->mDependentTaskSignals.AppendElement(resultSignal);
  }
  return resultSignal.forget();
}
}  // namespace mozilla::dom
