/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include "mozilla/TaskQueue.h"
#include "mozilla/MozPromise.h"

#include "nsISupportsImpl.h"
#include "mozilla/SharedThreadPool.h"
#include "VideoUtils.h"

using namespace mozilla;

typedef MozPromise<int, double, false> TestPromise;
typedef TestPromise::ResolveOrRejectValue RRValue;

class MOZ_STACK_CLASS AutoTaskQueue
{
public:
  AutoTaskQueue()
    : mTaskQueue(new TaskQueue(GetMediaThreadPool(MediaThreadType::PLAYBACK)))
  {}

  ~AutoTaskQueue()
  {
    mTaskQueue->AwaitShutdownAndIdle();
  }

  TaskQueue* Queue() { return mTaskQueue; }
private:
  RefPtr<TaskQueue> mTaskQueue;
};

class DelayedResolveOrReject : public Runnable
{
public:
  DelayedResolveOrReject(TaskQueue* aTaskQueue,
                         TestPromise::Private* aPromise,
                         TestPromise::ResolveOrRejectValue aValue,
                         int aIterations)
  : mTaskQueue(aTaskQueue)
  , mPromise(aPromise)
  , mValue(aValue)
  , mIterations(aIterations)
  {}

  NS_IMETHOD Run() override
  {
    MOZ_ASSERT(mTaskQueue->IsCurrentThreadIn());
    if (!mPromise) {
      // Canceled.
      return NS_OK;
    }

    if (--mIterations == 0) {
      mPromise->ResolveOrReject(mValue, __func__);
    } else {
      nsCOMPtr<nsIRunnable> r = this;
      mTaskQueue->Dispatch(r.forget());
    }

    return NS_OK;
  }

  void Cancel() {
    mPromise = nullptr;
  }

protected:
  ~DelayedResolveOrReject() {}

private:
  RefPtr<TaskQueue> mTaskQueue;
  RefPtr<TestPromise::Private> mPromise;
  TestPromise::ResolveOrRejectValue mValue;
  int mIterations;
};

template<typename FunctionType>
void
RunOnTaskQueue(TaskQueue* aQueue, FunctionType aFun)
{
  nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction(aFun);
  aQueue->Dispatch(r.forget());
}

// std::function can't come soon enough. :-(
#define DO_FAIL []()->void { EXPECT_TRUE(false); }

TEST(MozPromise, BasicResolve)
{
  AutoTaskQueue atq;
  RefPtr<TaskQueue> queue = atq.Queue();
  RunOnTaskQueue(queue, [queue] () -> void {
    TestPromise::CreateAndResolve(42, __func__)->Then(queue, __func__,
      [queue] (int aResolveValue) -> void { EXPECT_EQ(aResolveValue, 42); queue->BeginShutdown(); },
      DO_FAIL);
  });
}

TEST(MozPromise, BasicReject)
{
  AutoTaskQueue atq;
  RefPtr<TaskQueue> queue = atq.Queue();
  RunOnTaskQueue(queue, [queue] () -> void {
    TestPromise::CreateAndReject(42.0, __func__)->Then(queue, __func__,
      DO_FAIL,
      [queue] (int aRejectValue) -> void { EXPECT_EQ(aRejectValue, 42.0); queue->BeginShutdown(); });
  });
}

TEST(MozPromise, AsyncResolve)
{
  AutoTaskQueue atq;
  RefPtr<TaskQueue> queue = atq.Queue();
  RunOnTaskQueue(queue, [queue] () -> void {
    RefPtr<TestPromise::Private> p = new TestPromise::Private(__func__);

    // Kick off three racing tasks, and make sure we get the one that finishes earliest.
    RefPtr<DelayedResolveOrReject> a = new DelayedResolveOrReject(queue, p, RRValue::MakeResolve(32), 10);
    RefPtr<DelayedResolveOrReject> b = new DelayedResolveOrReject(queue, p, RRValue::MakeResolve(42), 5);
    RefPtr<DelayedResolveOrReject> c = new DelayedResolveOrReject(queue, p, RRValue::MakeReject(32.0), 7);

    nsCOMPtr<nsIRunnable> ref = a.get();
    queue->Dispatch(ref.forget());
    ref = b.get();
    queue->Dispatch(ref.forget());
    ref = c.get();
    queue->Dispatch(ref.forget());

    p->Then(queue, __func__, [queue, a, b, c] (int aResolveValue) -> void {
      EXPECT_EQ(aResolveValue, 42);
      a->Cancel();
      b->Cancel();
      c->Cancel();
      queue->BeginShutdown();
    }, DO_FAIL);
  });
}

TEST(MozPromise, CompletionPromises)
{
  bool invokedPass = false;
  AutoTaskQueue atq;
  RefPtr<TaskQueue> queue = atq.Queue();
  RunOnTaskQueue(queue, [queue, &invokedPass] () -> void {
    TestPromise::CreateAndResolve(40, __func__)
    ->Then(queue, __func__,
      [] (int aVal) -> RefPtr<TestPromise> { return TestPromise::CreateAndResolve(aVal + 10, __func__); },
      DO_FAIL)
    ->CompletionPromise()
    ->Then(queue, __func__, [&invokedPass] () -> void { invokedPass = true; }, DO_FAIL)
    ->CompletionPromise()
    ->Then(queue, __func__,
      [queue] (int aVal) -> RefPtr<TestPromise> {
        RefPtr<TestPromise::Private> p = new TestPromise::Private(__func__);
        nsCOMPtr<nsIRunnable> resolver = new DelayedResolveOrReject(queue, p, RRValue::MakeResolve(aVal - 8), 10);
        queue->Dispatch(resolver.forget());
        return RefPtr<TestPromise>(p);
      },
      DO_FAIL)
    ->CompletionPromise()
    ->Then(queue, __func__,
      [queue] (int aVal) -> RefPtr<TestPromise> { return TestPromise::CreateAndReject(double(aVal - 42) + 42.0, __func__); },
      DO_FAIL)
    ->CompletionPromise()
    ->Then(queue, __func__,
      DO_FAIL,
      [queue, &invokedPass] (double aVal) -> void { EXPECT_EQ(aVal, 42.0); EXPECT_TRUE(invokedPass); queue->BeginShutdown(); });
  });
}

TEST(MozPromise, PromiseAllResolve)
{
  AutoTaskQueue atq;
  RefPtr<TaskQueue> queue = atq.Queue();
  RunOnTaskQueue(queue, [queue] () -> void {

    nsTArray<RefPtr<TestPromise>> promises;
    promises.AppendElement(TestPromise::CreateAndResolve(22, __func__));
    promises.AppendElement(TestPromise::CreateAndResolve(32, __func__));
    promises.AppendElement(TestPromise::CreateAndResolve(42, __func__));

    TestPromise::All(queue, promises)->Then(queue, __func__,
      [queue] (const nsTArray<int>& aResolveValues) -> void {
        EXPECT_EQ(aResolveValues.Length(), 3UL);
        EXPECT_EQ(aResolveValues[0], 22);
        EXPECT_EQ(aResolveValues[1], 32);
        EXPECT_EQ(aResolveValues[2], 42);
        queue->BeginShutdown();
      },
      DO_FAIL
    );
  });
}

TEST(MozPromise, PromiseAllReject)
{
  AutoTaskQueue atq;
  RefPtr<TaskQueue> queue = atq.Queue();
  RunOnTaskQueue(queue, [queue] () -> void {

    nsTArray<RefPtr<TestPromise>> promises;
    promises.AppendElement(TestPromise::CreateAndResolve(22, __func__));
    promises.AppendElement(TestPromise::CreateAndReject(32.0, __func__));
    promises.AppendElement(TestPromise::CreateAndResolve(42, __func__));
   // Ensure that more than one rejection doesn't cause a crash (bug #1207312)
    promises.AppendElement(TestPromise::CreateAndReject(52.0, __func__));

    TestPromise::All(queue, promises)->Then(queue, __func__,
      DO_FAIL,
      [queue] (float aRejectValue) -> void {
        EXPECT_EQ(aRejectValue, 32.0);
        queue->BeginShutdown();
      }
    );
  });
}

// Test we don't hit the assertions in MozPromise when exercising promise
// chaining upon task queue shutdown.
TEST(MozPromise, Chaining)
{
  AutoTaskQueue atq;
  RefPtr<TaskQueue> queue = atq.Queue();
  MozPromiseRequestHolder<TestPromise> holder;

  RunOnTaskQueue(queue, [queue, &holder] () {
    auto p = TestPromise::CreateAndResolve(42, __func__);
    const size_t kIterations = 100;
    for (size_t i = 0; i < kIterations; ++i) {
      p = p->Then(queue, __func__,
        [] (int aVal) {
          EXPECT_EQ(aVal, 42);
        },
        [] () {}
      )->CompletionPromise();

      if (i == kIterations / 2) {
        p->Then(queue, __func__,
          [queue, &holder] () {
            holder.Disconnect();
            queue->BeginShutdown();
          },
          DO_FAIL);
      }
    }
    // We will hit the assertion if we don't disconnect the leaf Request
    // in the promise chain.
    holder.Begin(p->Then(queue, __func__, [] () {}, [] () {}));
  });
}

#undef DO_FAIL
