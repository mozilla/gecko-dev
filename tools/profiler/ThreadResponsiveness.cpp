/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ThreadResponsiveness.h"
#include "platform.h"
#include "nsComponentManagerUtils.h"
#include "nsThreadUtils.h"
#include "nsITimer.h"
#include "mozilla/Monitor.h"
#include "ProfileEntry.h"

using mozilla::Monitor;
using mozilla::MonitorAutoLock;

class CheckResponsivenessTask : public nsRunnable,
                                public nsITimerCallback {
public:
  CheckResponsivenessTask()
    : mLastTracerTime(TimeStamp::Now())
    , mMonitor("CheckResponsivenessTask")
    , mTimer(nullptr)
    , mStop(false)
  {
    MOZ_COUNT_CTOR(CheckResponsivenessTask);
  }

  ~CheckResponsivenessTask()
  {
    MOZ_COUNT_DTOR(CheckResponsivenessTask);
  }

  NS_IMETHOD Run()
  {
    MonitorAutoLock mon(mMonitor);
    if (mStop)
      return NS_OK;

    // This is raced on because we might pause the thread here
    // for profiling so if we tried to use a monitor to protect
    // mLastTracerTime we could deadlock. We're risking seeing
    // a partial write which will show up as an outlier in our
    // performance data.
    mLastTracerTime = TimeStamp::Now();
    if (!mTimer) {
      mTimer = do_CreateInstance("@mozilla.org/timer;1");
    }
    mTimer->InitWithCallback(this, 16, nsITimer::TYPE_ONE_SHOT);

    return NS_OK;
  }

  NS_IMETHODIMP Notify(nsITimer* aTimer) MOZ_FINAL
  {
    NS_DispatchToMainThread(this);
    return NS_OK;
  }

  void Terminate() {
    MonitorAutoLock mon(mMonitor);
    mStop = true;
  }

  const TimeStamp& GetLastTracerTime() const {
    return mLastTracerTime;
  }

  NS_DECL_ISUPPORTS_INHERITED

private:
  TimeStamp mLastTracerTime;
  Monitor mMonitor;
  nsCOMPtr<nsITimer> mTimer;
  bool mStop;
};

NS_IMPL_ISUPPORTS_INHERITED(CheckResponsivenessTask, nsRunnable, nsITimerCallback)

ThreadResponsiveness::ThreadResponsiveness(ThreadProfile *aThreadProfile)
  : mThreadProfile(aThreadProfile)
  , mActiveTracerEvent(nullptr)
{
  MOZ_COUNT_CTOR(ThreadResponsiveness);
}

ThreadResponsiveness::~ThreadResponsiveness()
{
  MOZ_COUNT_DTOR(ThreadResponsiveness);
  if (mActiveTracerEvent) {
    mActiveTracerEvent->Terminate();
  }
}

void
ThreadResponsiveness::Update()
{
  return;
  if (!mActiveTracerEvent) {
    if (mThreadProfile->GetThreadInfo()->IsMainThread()) {
      mActiveTracerEvent = new CheckResponsivenessTask();
      NS_DispatchToMainThread(mActiveTracerEvent);
    } else if (mThreadProfile->GetThreadInfo()->GetThread()) {
      mActiveTracerEvent = new CheckResponsivenessTask();
      mThreadProfile->GetThreadInfo()->
        GetThread()->Dispatch(mActiveTracerEvent, NS_DISPATCH_NORMAL);
    }
  }

  if (mActiveTracerEvent) {
    mLastTracerTime = mActiveTracerEvent->GetLastTracerTime();
  }
}

