/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "WebrtcTaskQueueWrapper.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "mozilla/TaskQueue.h"
#include "nsThreadUtils.h"

using testing::InSequence;
using testing::MockFunction;

namespace mozilla {

RefPtr<TaskQueue> MakeTestWebrtcTaskQueueWrapper() {
  return CreateWebrtcTaskQueueWrapper(do_AddRef(GetCurrentSerialEventTarget()),
                                      "TestWebrtcTaskQueueWrapper"_ns, true);
}

TEST(TestWebrtcTaskQueueWrapper, TestCurrent)
{
  auto wt = MakeTestWebrtcTaskQueueWrapper();

  MockFunction<void(int)> checkpoint;
  {
    InSequence s;
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(checkpoint, Call(2));
  }

  EXPECT_TRUE(NS_SUCCEEDED(wt->Dispatch(NS_NewRunnableFunction(__func__, [&] {
    checkpoint.Call(2);
    EXPECT_TRUE(wt->IsCurrentThreadIn());
  }))));
  checkpoint.Call(1);
  NS_ProcessPendingEvents(nullptr);
}

TEST(TestWebrtcTaskQueueWrapper, TestDispatchDirectTask)
{
  auto wt = MakeTestWebrtcTaskQueueWrapper();

  MockFunction<void(int)> checkpoint;
  {
    InSequence s;
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(checkpoint, Call(2));
    EXPECT_CALL(checkpoint, Call(3));
    EXPECT_CALL(checkpoint, Call(4));
  }

  EXPECT_TRUE(NS_SUCCEEDED(wt->Dispatch(NS_NewRunnableFunction(__func__, [&] {
    checkpoint.Call(2);
    AbstractThread::DispatchDirectTask(
        NS_NewRunnableFunction("TestDispatchDirectTask Inner", [&] {
          checkpoint.Call(3);
          EXPECT_TRUE(wt->IsCurrentThreadIn());
        }));
  }))));

  EXPECT_TRUE(NS_SUCCEEDED(wt->Dispatch(NS_NewRunnableFunction(__func__, [&] {
    checkpoint.Call(4);
    EXPECT_TRUE(wt->IsCurrentThreadIn());
  }))));
  checkpoint.Call(1);
  NS_ProcessPendingEvents(nullptr);
}

}  // namespace mozilla
