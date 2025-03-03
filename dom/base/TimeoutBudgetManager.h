/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_timeoutbudgetmanager_h
#define mozilla_dom_timeoutbudgetmanager_h

#include "mozilla/TimeStamp.h"
#include "mozilla/dom/Timeout.h"

namespace mozilla::dom {

class TimeoutBudgetManager {
 public:
  void StartRecording(const TimeStamp& aNow);
  void StopRecording();
  TimeDuration RecordExecution(const TimeStamp& aNow, const Timeout* aTimeout);

 private:
  TimeStamp mStart{};
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_timeoutbudgetmanager_h
