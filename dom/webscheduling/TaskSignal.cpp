/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TaskSignal.h"
#include "WebTaskScheduler.h"

namespace mozilla::dom {

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
}  // namespace mozilla::dom
