/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <memory>
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "gtest/gtest-spi.h"
#include "mozilla/SharedThreadPool.h"
#include "mozilla/SyncRunnable.h"
#include "mozilla/TaskQueue.h"
#include "mozilla/Unused.h"
#include "nsITargetShutdownTask.h"
#include "VideoUtils.h"

namespace TestTaskQueue {

using namespace mozilla;
using testing::_;
using testing::InSequence;
using testing::MockFunction;
using testing::StrEq;

TEST(TaskQueue, EventOrder)
{
  RefPtr<TaskQueue> tq1 =
      TaskQueue::Create(GetMediaThreadPool(MediaThreadType::SUPERVISOR),
                        "TestTaskQueue tq1", true);
  RefPtr<TaskQueue> tq2 =
      TaskQueue::Create(GetMediaThreadPool(MediaThreadType::SUPERVISOR),
                        "TestTaskQueue tq2", true);
  RefPtr<TaskQueue> tq3 =
      TaskQueue::Create(GetMediaThreadPool(MediaThreadType::SUPERVISOR),
                        "TestTaskQueue tq3", true);

  bool errored = false;
  int counter = 0;
  int sync = 0;
  Monitor monitor MOZ_UNANNOTATED("TaskQueue::EventOrder::monitor");

  // We expect task1 happens before task3.
  for (int i = 0; i < 10000; ++i) {
    Unused << tq1->Dispatch(
        NS_NewRunnableFunction(
            "TestTaskQueue::TaskQueue_EventOrder_Test::TestBody",
            [&]() {
              Unused << tq2->Dispatch(NS_NewRunnableFunction(
                  "TestTaskQueue::TaskQueue_EventOrder_Test::TestBody",
                  []() {  // task0
                  }));
              Unused << tq3->Dispatch(NS_NewRunnableFunction(
                  "TestTaskQueue::TaskQueue_EventOrder_Test::TestBody",
                  [&]() {  // task1
                    EXPECT_EQ(1, ++counter);
                    errored = counter != 1;
                    MonitorAutoLock mon(monitor);
                    ++sync;
                    mon.Notify();
                  }));
              Unused << tq2->Dispatch(NS_NewRunnableFunction(
                  "TestTaskQueue::TaskQueue_EventOrder_Test::TestBody",
                  [&]() {  // task2
                    Unused << tq3->Dispatch(NS_NewRunnableFunction(
                        "TestTaskQueue::TaskQueue_EventOrder_Test::TestBody",
                        [&]() {  // task3
                          EXPECT_EQ(0, --counter);
                          errored = counter != 0;
                          MonitorAutoLock mon(monitor);
                          ++sync;
                          mon.Notify();
                        }));
                  }));
            }),
        AbstractThread::TailDispatch);

    // Ensure task1 and task3 are done before next loop.
    MonitorAutoLock mon(monitor);
    while (sync != 2) {
      mon.Wait();
    }
    sync = 0;

    if (errored) {
      break;
    }
  }

  tq1->BeginShutdown();
  tq1->AwaitShutdownAndIdle();
  tq2->BeginShutdown();
  tq2->AwaitShutdownAndIdle();
  tq3->BeginShutdown();
  tq3->AwaitShutdownAndIdle();
}

TEST(TaskQueue, GetCurrentSerialEventTarget)
{
  RefPtr<TaskQueue> tq1 =
      TaskQueue::Create(GetMediaThreadPool(MediaThreadType::SUPERVISOR),
                        "TestTaskQueue GetCurrentSerialEventTarget", false);
  Unused << tq1->Dispatch(NS_NewRunnableFunction(
      "TestTaskQueue::TestCurrentSerialEventTarget::TestBody", [tq1]() {
        nsCOMPtr<nsISerialEventTarget> thread = GetCurrentSerialEventTarget();
        EXPECT_EQ(thread, tq1);
      }));
  tq1->BeginShutdown();
  tq1->AwaitShutdownAndIdle();
}

TEST(TaskQueue, DirectTaskGetCurrentSerialEventTarget)
{
  RefPtr<TaskQueue> tq1 = TaskQueue::Create(
      GetMediaThreadPool(MediaThreadType::SUPERVISOR),
      "TestTaskQueue DirectTaskGetCurrentSerialEventTarget", true);
  Unused << tq1->Dispatch(NS_NewRunnableFunction(
      "TestTaskQueue::DirectTaskGetCurrentSerialEventTarget::TestBody", [&]() {
        AbstractThread::DispatchDirectTask(NS_NewRunnableFunction(
            "TestTaskQueue::DirectTaskGetCurrentSerialEventTarget::DirectTask",
            [&] { EXPECT_EQ(GetCurrentSerialEventTarget(), tq1); }));
      }));
  tq1->BeginShutdown();
  tq1->AwaitShutdownAndIdle();
}

namespace {

class TestShutdownTask final : public nsITargetShutdownTask {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS

  explicit TestShutdownTask(std::function<void()> aCallback)
      : mCallback(std::move(aCallback)) {}

  void TargetShutdown() override {
    if (mCallback) {
      mCallback();
    }
  }

 private:
  ~TestShutdownTask() = default;
  std::function<void()> mCallback;
};

NS_IMPL_ISUPPORTS(TestShutdownTask, nsITargetShutdownTask)

}  // namespace

TEST(TaskQueue, ShutdownTask)
{
  auto shutdownTaskRun = std::make_shared<bool>();
  auto runnableFromShutdownRun = std::make_shared<bool>();

  RefPtr<TaskQueue> tq = TaskQueue::Create(
      GetMediaThreadPool(MediaThreadType::SUPERVISOR), "Testing TaskQueue");

  nsCOMPtr<nsITargetShutdownTask> shutdownTask = new TestShutdownTask([=] {
    EXPECT_TRUE(tq->IsOnCurrentThread());

    ASSERT_FALSE(*shutdownTaskRun);
    *shutdownTaskRun = true;

    nsCOMPtr<nsITargetShutdownTask> dummyTask = new TestShutdownTask([] {});
    nsresult rv = tq->RegisterShutdownTask(dummyTask);
    EXPECT_TRUE(rv == NS_ERROR_UNEXPECTED);

    MOZ_ALWAYS_SUCCEEDS(
        tq->Dispatch(NS_NewRunnableFunction("afterShutdownTask", [=] {
          EXPECT_TRUE(tq->IsOnCurrentThread());

          nsCOMPtr<nsITargetShutdownTask> dummyTask =
              new TestShutdownTask([] {});
          nsresult rv = tq->RegisterShutdownTask(dummyTask);
          EXPECT_TRUE(rv == NS_ERROR_UNEXPECTED);

          ASSERT_FALSE(*runnableFromShutdownRun);
          *runnableFromShutdownRun = true;
        })));
  });
  MOZ_ALWAYS_SUCCEEDS(tq->RegisterShutdownTask(shutdownTask));

  ASSERT_FALSE(*shutdownTaskRun);
  ASSERT_FALSE(*runnableFromShutdownRun);

  RefPtr<mozilla::SyncRunnable> syncWithThread =
      new mozilla::SyncRunnable(NS_NewRunnableFunction("dummy", [] {}));
  MOZ_ALWAYS_SUCCEEDS(syncWithThread->DispatchToThread(tq));

  ASSERT_FALSE(*shutdownTaskRun);
  ASSERT_FALSE(*runnableFromShutdownRun);

  tq->BeginShutdown();
  tq->AwaitShutdownAndIdle();

  ASSERT_TRUE(*shutdownTaskRun);
  ASSERT_TRUE(*runnableFromShutdownRun);
}

TEST(TaskQueue, UnregisteredShutdownTask)
{
  RefPtr<TaskQueue> tq = TaskQueue::Create(
      GetMediaThreadPool(MediaThreadType::SUPERVISOR), "Testing TaskQueue");

  nsCOMPtr<nsITargetShutdownTask> shutdownTask =
      new TestShutdownTask([=] { MOZ_CRASH("should not be run"); });

  MOZ_ALWAYS_SUCCEEDS(tq->RegisterShutdownTask(shutdownTask));

  RefPtr<mozilla::SyncRunnable> syncWithThread =
      new mozilla::SyncRunnable(NS_NewRunnableFunction("dummy", [] {}));
  MOZ_ALWAYS_SUCCEEDS(syncWithThread->DispatchToThread(tq));

  MOZ_ALWAYS_SUCCEEDS(tq->UnregisterShutdownTask(shutdownTask));

  tq->BeginShutdown();
  tq->AwaitShutdownAndIdle();
}

// Mock function to register code flow and current targets. Targets are 1)
// TaskQueue::Observer's task queue, 2) AbstractThread::GetCurrent(), and 3)
// GetCurrentSerialEventTarget().
using ObserverCheckpoint = MockFunction<void(
    const char*, TaskQueue*, AbstractThread*, nsISerialEventTarget*)>;
// Helpers because the thread args are usually the same.
// These are macros because EXPECT_CALL is a macro, and derives line numbers
// this way.
#define EXPECT_OBS_CALL(cp, str, tq) \
  EXPECT_CALL(cp, Call(StrEq(str), tq, tq, tq))
#define EXPECT_RUNNABLE_CALL(cp, str, tq) \
  EXPECT_CALL(cp, Call(StrEq(str), nullptr, tq, tq))
#define EXPECT_OBSDTOR_CALL(cp, str)                                      \
  EXPECT_CALL(cp, Call(StrEq(str), nullptr, AbstractThread::MainThread(), \
                       GetMainThreadSerialEventTarget()))
class Observer final : public TaskQueue::Observer {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(Observer, override);

  explicit Observer(ObserverCheckpoint& aFunc) : mFunc(aFunc) {}
  void WillProcessEvent(TaskQueue* aTaskQueue) override {
    mFunc.Call(__func__, aTaskQueue, AbstractThread::GetCurrent(),
               GetCurrentSerialEventTarget());
  };
  void DidProcessEvent(TaskQueue* aTaskQueue) override {
    EXPECT_EQ(aTaskQueue, AbstractThread::GetCurrent());
    mFunc.Call(__func__, aTaskQueue, AbstractThread::GetCurrent(),
               GetCurrentSerialEventTarget());
  }

 private:
  ~Observer() override {
    mFunc.Call(__func__, nullptr, AbstractThread::GetCurrent(),
               GetCurrentSerialEventTarget());
  }

  ObserverCheckpoint& mFunc;
};

TEST(TaskQueue, Observer)
{
  RefPtr<TaskQueue> taskQueue = TaskQueue::Create(
      do_AddRef(AbstractThread::MainThread()), "Testing TaskQueue");
  TaskQueue* tq = taskQueue;

  ObserverCheckpoint checkpoint;
  {
    InSequence seq;
    EXPECT_OBS_CALL(checkpoint, "WillProcessEvent", tq);
    EXPECT_RUNNABLE_CALL(checkpoint, "Runnable", tq);
    EXPECT_OBS_CALL(checkpoint, "DidProcessEvent", tq);
    EXPECT_OBSDTOR_CALL(checkpoint, "~Observer");
  }

  {
    auto obs = MakeRefPtr<Observer>(checkpoint);
    tq->SetObserver(obs);
  }

  MOZ_ALWAYS_SUCCEEDS(tq->Dispatch(NS_NewRunnableFunction(__func__, [&] {
    checkpoint.Call("Runnable", nullptr, AbstractThread::GetCurrent(),
                    GetCurrentSerialEventTarget());
  })));
  NS_ProcessPendingEvents(nullptr);

  tq->BeginShutdown();
}

TEST(TaskQueue, ObserverTransactional)
{
  RefPtr<TaskQueue> taskQueue = TaskQueue::Create(
      do_AddRef(AbstractThread::MainThread()), "Testing TaskQueue");
  TaskQueue* tq = taskQueue;

  ObserverCheckpoint checkpoint;
  {
    InSequence seq;
    EXPECT_RUNNABLE_CALL(checkpoint, "Runnable1", tq);
    EXPECT_OBS_CALL(checkpoint, "WillProcessEvent", tq);
    EXPECT_RUNNABLE_CALL(checkpoint, "Runnable2", tq);
    EXPECT_OBS_CALL(checkpoint, "DidProcessEvent", tq);
    EXPECT_RUNNABLE_CALL(checkpoint, "~Observer", _);
  }

  {
    auto obs = MakeRefPtr<Observer>(checkpoint);
    MOZ_ALWAYS_SUCCEEDS(tq->Dispatch(NS_NewRunnableFunction(__func__, [&] {
      tq->SetObserver(obs);
      checkpoint.Call("Runnable1", nullptr, AbstractThread::GetCurrent(),
                      GetCurrentSerialEventTarget());
    })));
    NS_ProcessPendingEvents(nullptr);
  }

  MOZ_ALWAYS_SUCCEEDS(tq->Dispatch(NS_NewRunnableFunction(__func__, [&] {
    // Note this technically destroys the observer on a different event target
    // than if it's destroyed through shutdown.
    tq->SetObserver(nullptr);
    checkpoint.Call("Runnable2", nullptr, AbstractThread::GetCurrent(),
                    GetCurrentSerialEventTarget());
  })));
  NS_ProcessPendingEvents(nullptr);

  tq->BeginShutdown();
}

TEST(TaskQueue, ObserverDirectTask)
{
  RefPtr<TaskQueue> taskQueue =
      TaskQueue::Create(do_AddRef(AbstractThread::MainThread()),
                        "Testing TaskQueue", /*aSupportsTailDispatch=*/true);
  TaskQueue* tq = taskQueue;
  ObserverCheckpoint checkpoint;

  {
    auto obs = MakeRefPtr<Observer>(checkpoint);
    tq->SetObserver(obs);
  }

  {
    InSequence seq;
    EXPECT_OBS_CALL(checkpoint, "WillProcessEvent", tq);
    EXPECT_RUNNABLE_CALL(checkpoint, "Runnable1", tq);
    EXPECT_RUNNABLE_CALL(checkpoint, "Runnable1.Direct", tq);
    EXPECT_OBS_CALL(checkpoint, "DidProcessEvent", tq);
    EXPECT_OBS_CALL(checkpoint, "WillProcessEvent", tq);
    EXPECT_RUNNABLE_CALL(checkpoint, "Runnable2", tq);
    EXPECT_OBS_CALL(checkpoint, "DidProcessEvent", tq);
    EXPECT_OBSDTOR_CALL(checkpoint, "~Observer");
  }

  MOZ_ALWAYS_SUCCEEDS(tq->Dispatch(NS_NewRunnableFunction(__func__, [&] {
    checkpoint.Call("Runnable1", nullptr, AbstractThread::GetCurrent(),
                    GetCurrentSerialEventTarget());
    tq->TailDispatcher().AddDirectTask(
        NS_NewRunnableFunction("TestDirectTask", [&] {
          checkpoint.Call("Runnable1.Direct", nullptr,
                          AbstractThread::GetCurrent(),
                          GetCurrentSerialEventTarget());
        }));
  })));

  MOZ_ALWAYS_SUCCEEDS(tq->Dispatch(NS_NewRunnableFunction(__func__, [&] {
    checkpoint.Call("Runnable2", nullptr, AbstractThread::GetCurrent(),
                    GetCurrentSerialEventTarget());
  })));
  NS_ProcessPendingEvents(nullptr);

  tq->BeginShutdown();
}

#undef EXPECT_OBS_CALL
#undef EXPECT_RUNNABLE_CALL
#undef EXPECT_CP_CALL

TEST(AbstractThread, GetCurrentSerialEventTarget)
{
  RefPtr<AbstractThread> mainThread = AbstractThread::GetCurrent();
  EXPECT_EQ(mainThread, AbstractThread::MainThread());
  Unused << mainThread->Dispatch(NS_NewRunnableFunction(
      "TestAbstractThread::TestCurrentSerialEventTarget::TestBody",
      [mainThread]() {
        nsCOMPtr<nsISerialEventTarget> thread = GetCurrentSerialEventTarget();
        EXPECT_EQ(thread, mainThread);
      }));

  // Spin the event loop.
  NS_ProcessPendingEvents(nullptr);
}

TEST(AbstractThread, DirectTaskGetCurrentSerialEventTarget)
{
  RefPtr<AbstractThread> mainThread = AbstractThread::GetCurrent();
  EXPECT_EQ(mainThread, AbstractThread::MainThread());
  Unused << mainThread->Dispatch(NS_NewRunnableFunction(
      "TestAbstractThread::DirectTaskGetCurrentSerialEventTarget::TestBody",
      [&]() {
        AbstractThread::DispatchDirectTask(NS_NewRunnableFunction(
            "TestAbstractThread::DirectTaskGetCurrentSerialEventTarget::"
            "DirectTask",
            [&] {
              // NOTE: Currently we don't set the SerialEventTarget guard when
              //       running direct tasks on `AbstractThread::MainThread()`.
              //       See bug 1971198 for context.
              EXPECT_NONFATAL_FAILURE(
                  EXPECT_EQ(GetCurrentSerialEventTarget(), mainThread), "");
            }));
      }));

  // Spin the event loop.
  NS_ProcessPendingEvents(nullptr);
}

}  // namespace TestTaskQueue
