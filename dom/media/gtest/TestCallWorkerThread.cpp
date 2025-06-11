/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "CallWorkerThread.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

using testing::InSequence;
using testing::MockFunction;

namespace mozilla {

RefPtr<CallWorkerThread> MakeTestCallWorkerThread() {
  return new CallWorkerThread(
      MakeUnique<TaskQueueWrapper<DeletionPolicy::NonBlocking>>(
          TaskQueue::Create(do_AddRef(GetCurrentSerialEventTarget()),
                            "MainTaskQueue", true),
          "TestCallWorkerThread"_ns));
}

TEST(TestCallWorkerThread, TestCurrent)
{
  auto wt = MakeTestCallWorkerThread();

  MockFunction<void(int)> checkpoint;
  {
    InSequence s;
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(checkpoint, Call(2));
  }

  wt->Dispatch(NS_NewRunnableFunction(__func__, [&] {
    checkpoint.Call(2);
    EXPECT_TRUE(wt->IsCurrentThreadIn());
  }));
  checkpoint.Call(1);
  NS_ProcessPendingEvents(nullptr);
}

TEST(TestCallWorkerThread, TestDispatchDirectTask)
{
  auto wt = MakeTestCallWorkerThread();

  MockFunction<void(int)> checkpoint;
  {
    InSequence s;
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(checkpoint, Call(2));
    EXPECT_CALL(checkpoint, Call(3));
    EXPECT_CALL(checkpoint, Call(4));
  }

  wt->Dispatch(NS_NewRunnableFunction(__func__, [&] {
    checkpoint.Call(2);
    AbstractThread::DispatchDirectTask(
        NS_NewRunnableFunction("TestDispatchDirectTask Inner", [&] {
          checkpoint.Call(3);
          EXPECT_TRUE(wt->IsCurrentThreadIn());
        }));
  }));

  wt->Dispatch(NS_NewRunnableFunction(__func__, [&] {
    checkpoint.Call(4);
    EXPECT_TRUE(wt->IsCurrentThreadIn());
  }));
  checkpoint.Call(1);
  NS_ProcessPendingEvents(nullptr);
}

}  // namespace mozilla
