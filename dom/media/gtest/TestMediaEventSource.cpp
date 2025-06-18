/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "gmock/gmock-matchers.h"  // testing::ElementsAre

#include "mozilla/SharedThreadPool.h"
#include "mozilla/TaskQueue.h"
#include "mozilla/UniquePtr.h"
#include "MediaEventSource.h"
#include "VideoUtils.h"
#include <memory>
#include <type_traits>

using namespace mozilla;
using testing::InSequence;
using testing::MockFunction;
using testing::StrEq;

// TODO(bug 1954634): Once all of our toolchains support c++ requires
// expression, we should be validating that ineligible function signatures will
// not compile. (eg; NonExclusive does not work with non-const refs or rvalue
// refs)

/*
 * Test if listeners receive the event data correctly.
 */
TEST(MediaEventSource, SingleListener)
{
  RefPtr<TaskQueue> queue =
      TaskQueue::Create(GetMediaThreadPool(MediaThreadType::SUPERVISOR),
                        "TestMediaEventSource SingleListener");

  MediaEventProducer<int> source;

  static std::vector<int> callbackLog;
  callbackLog.clear();

  auto func = [&](int j) { callbackLog.push_back(j); };
  MediaEventListener listener = source.Connect(queue, func);

  // Call Notify 3 times. The listener should be also called 3 times.
  source.Notify(3);
  source.Notify(5);
  source.Notify(7);

  queue->BeginShutdown();
  queue->AwaitShutdownAndIdle();

  // Verify the event data is passed correctly to the listener.
  EXPECT_THAT(callbackLog, testing::ElementsAre(3, 5, 7));

  listener.Disconnect();
}

TEST(MediaEventSource, MultiListener)
{
  RefPtr<TaskQueue> queue =
      TaskQueue::Create(GetMediaThreadPool(MediaThreadType::SUPERVISOR),
                        "TestMediaEventSource MultiListener");

  MediaEventProducer<int> source;

  static std::vector<int> callbackLog;
  callbackLog.clear();

  auto func1 = [&](int k) { callbackLog.push_back(k * 2); };
  auto func2 = [&](int k) { callbackLog.push_back(k * 3); };
  MediaEventListener listener1 = source.Connect(queue, func1);
  MediaEventListener listener2 = source.Connect(queue, func2);

  // Both listeners should receive the event.
  source.Notify(11);

  queue->BeginShutdown();
  queue->AwaitShutdownAndIdle();

  // Verify the event data is passed correctly to the listener.
  EXPECT_THAT(callbackLog, testing::ElementsAre(22, 33));

  listener1.Disconnect();
  listener2.Disconnect();
}

/*
 * Test if disconnecting a listener prevents events from coming.
 */
TEST(MediaEventSource, DisconnectAfterNotification)
{
  RefPtr<TaskQueue> queue =
      TaskQueue::Create(GetMediaThreadPool(MediaThreadType::SUPERVISOR),
                        "TestMediaEventSource DisconnectAfterNotification");

  MediaEventProducer<int> source;

  static std::vector<int> callbackLog;
  callbackLog.clear();

  MediaEventListener listener;
  auto func = [&](int j) {
    callbackLog.push_back(j);
    listener.Disconnect();
  };
  listener = source.Connect(queue, func);

  // Call Notify() twice. Since we disconnect the listener when receiving
  // the 1st event, the 2nd event should not reach the listener.
  source.Notify(11);
  source.Notify(11);

  queue->BeginShutdown();
  queue->AwaitShutdownAndIdle();

  // Check only the 1st event is received.
  EXPECT_THAT(callbackLog, testing::ElementsAre(11));
}

TEST(MediaEventSource, DisconnectBeforeNotification)
{
  RefPtr<TaskQueue> queue =
      TaskQueue::Create(GetMediaThreadPool(MediaThreadType::SUPERVISOR),
                        "TestMediaEventSource DisconnectBeforeNotification");

  MediaEventProducer<int> source;

  static std::vector<int> callbackLog;
  callbackLog.clear();

  auto func1 = [&](int k) { callbackLog.push_back(k * 2); };
  auto func2 = [&](int k) { callbackLog.push_back(k * 3); };
  MediaEventListener listener1 = source.Connect(queue, func1);
  MediaEventListener listener2 = source.Connect(queue, func2);

  // Disconnect listener2 before notification. Only listener1 should receive
  // the event.
  listener2.Disconnect();
  source.Notify(11);

  queue->BeginShutdown();
  queue->AwaitShutdownAndIdle();

  EXPECT_THAT(callbackLog, testing::ElementsAre(22));

  listener1.Disconnect();
}

/*
 * Test we don't hit the assertion when calling Connect() and Disconnect()
 * repeatedly.
 */
TEST(MediaEventSource, DisconnectAndConnect)
{
  RefPtr<TaskQueue> queue =
      TaskQueue::Create(GetMediaThreadPool(MediaThreadType::SUPERVISOR),
                        "TestMediaEventSource DisconnectAndConnect");

  MediaEventProducerExc<int> source;
  MediaEventListener listener = source.Connect(queue, []() {});
  listener.Disconnect();
  listener = source.Connect(queue, []() {});
  listener.Disconnect();
}

/*
 * Test void event type.
 */
TEST(MediaEventSource, VoidEventType)
{
  RefPtr<TaskQueue> queue =
      TaskQueue::Create(GetMediaThreadPool(MediaThreadType::SUPERVISOR),
                        "TestMediaEventSource VoidEventType");

  MediaEventProducer<void> source;

  static std::vector<int> callbackLog;
  callbackLog.clear();

  // Test function object.
  auto func = [&]() { callbackLog.push_back(1); };
  MediaEventListener listener1 = source.Connect(queue, func);

  // Test member function.
  struct Foo {
    Foo() {}
    void OnNotify() { callbackLog.push_back(2); }
  } foo;
  MediaEventListener listener2 = source.Connect(queue, &foo, &Foo::OnNotify);

  // Call Notify 2 times. The listener should be also called 2 times.
  source.Notify();
  source.Notify();

  queue->BeginShutdown();
  queue->AwaitShutdownAndIdle();

  EXPECT_THAT(callbackLog, testing::ElementsAre(1, 2, 1, 2));

  listener1.Disconnect();
  listener2.Disconnect();
}

/*
 * Test listeners can take various event types (T, const T&, and void).
 */
TEST(MediaEventSource, ListenerType1)
{
  RefPtr<TaskQueue> queue =
      TaskQueue::Create(GetMediaThreadPool(MediaThreadType::SUPERVISOR),
                        "TestMediaEventSource ListenerType1");

  MediaEventProducer<int> source;

  static std::vector<int> callbackLog;
  callbackLog.clear();

  // Test various argument types.
  // func(int&&) and func(int&) are ineligible because we're in NonExclusive
  // mode, which passes a const.
  auto func1 = [&](int j) { callbackLog.push_back(1); };
  auto func2 = [&](const int& j) { callbackLog.push_back(2); };
  auto func3 = [&]() { callbackLog.push_back(3); };
  MediaEventListener listener1 = source.Connect(queue, func1);
  MediaEventListener listener2 = source.Connect(queue, func2);
  MediaEventListener listener3 = source.Connect(queue, func3);

  source.Notify(1);

  queue->BeginShutdown();
  queue->AwaitShutdownAndIdle();

  EXPECT_THAT(callbackLog, testing::ElementsAre(1, 2, 3));

  listener1.Disconnect();
  listener2.Disconnect();
  listener3.Disconnect();
}

TEST(MediaEventSource, ListenerType2)
{
  RefPtr<TaskQueue> queue =
      TaskQueue::Create(GetMediaThreadPool(MediaThreadType::SUPERVISOR),
                        "TestMediaEventSource ListenerType2");

  MediaEventProducer<int> source;

  static std::vector<int> callbackLog;
  callbackLog.clear();

  struct Foo {
    void OnNotify1(const int& i) { callbackLog.push_back(1); }
    void OnNotify2() { callbackLog.push_back(2); }
    void OnNotify3(int i) const { callbackLog.push_back(3); }
    void OnNotify4(int i) volatile { callbackLog.push_back(4); }
  } foo;

  // Test member functions which might be CV qualified.
  MediaEventListener listener1 = source.Connect(queue, &foo, &Foo::OnNotify1);
  MediaEventListener listener2 = source.Connect(queue, &foo, &Foo::OnNotify2);
  MediaEventListener listener3 = source.Connect(queue, &foo, &Foo::OnNotify3);
  MediaEventListener listener4 = source.Connect(queue, &foo, &Foo::OnNotify4);

  source.Notify(1);

  queue->BeginShutdown();
  queue->AwaitShutdownAndIdle();

  EXPECT_THAT(callbackLog, testing::ElementsAre(1, 2, 3, 4));

  listener1.Disconnect();
  listener2.Disconnect();
  listener3.Disconnect();
  listener4.Disconnect();
}

struct SomeEvent {
  explicit SomeEvent(int& aCount) : mCount(aCount) {}
  // Increment mCount when copy constructor is called to know how many times
  // the event data is copied.
  SomeEvent(const SomeEvent& aOther) : mCount(aOther.mCount) { ++mCount; }
  SomeEvent(SomeEvent&& aOther) : mCount(aOther.mCount) {}
  int& mCount;
};

/*
 * Test we don't have unnecessary copies of the event data.
 */
TEST(MediaEventSource, ZeroCopyNonExclusiveOneTarget)
{
  RefPtr<TaskQueue> queue =
      TaskQueue::Create(GetMediaThreadPool(MediaThreadType::SUPERVISOR),
                        "TestMediaEventSource ZeroCopyNonExclusiveOneTarget");

  MediaEventProducer<SomeEvent> source;
  int copies = 0;

  static std::vector<int> callbackLog;
  callbackLog.clear();

  auto func = []() { callbackLog.push_back(1); };
  struct Foo {
    void OnNotify() { callbackLog.push_back(2); }
  } foo;

  MediaEventListener listener1 = source.Connect(queue, func);
  MediaEventListener listener2 = source.Connect(queue, &foo, &Foo::OnNotify);

  // We expect i to be 0 since Notify can take ownership of the temp object,
  // and use it as shared state for all listeners.
  source.Notify(SomeEvent(copies));

  queue->BeginShutdown();
  queue->AwaitShutdownAndIdle();
  EXPECT_EQ(copies, 0);

  EXPECT_THAT(callbackLog, testing::ElementsAre(1, 2));

  listener1.Disconnect();
  listener2.Disconnect();
}

TEST(MediaEventSource, ZeroCopyNonExclusiveTwoTarget)
{
  RefPtr<TaskQueue> queue1 = TaskQueue::Create(
      GetMediaThreadPool(MediaThreadType::SUPERVISOR),
      "TestMediaEventSource ZeroCopyNonExclusiveTwoTarget(first)");
  RefPtr<TaskQueue> queue2 = TaskQueue::Create(
      GetMediaThreadPool(MediaThreadType::SUPERVISOR),
      "TestMediaEventSource ZeroCopyNonExclusiveTwoTarget(second)");

  MediaEventProducer<SomeEvent> source;
  int copies = 0;

  static std::vector<int> callbackLog1;
  callbackLog1.clear();

  static std::vector<int> callbackLog2;
  callbackLog2.clear();

  auto func1 = []() { callbackLog1.push_back(1); };
  struct Foo1 {
    void OnNotify() { callbackLog1.push_back(2); }
  } foo1;

  auto func2 = []() { callbackLog2.push_back(1); };
  struct Foo2 {
    void OnNotify() { callbackLog2.push_back(2); }
  } foo2;

  MediaEventListener listener1 = source.Connect(queue1, func1);
  MediaEventListener listener2 = source.Connect(queue1, &foo1, &Foo1::OnNotify);
  MediaEventListener listener3 = source.Connect(queue2, func2);
  MediaEventListener listener4 = source.Connect(queue2, &foo2, &Foo2::OnNotify);

  // We expect i to be 0 since Notify can take ownership of the temp object,
  // and use it as shared state for all listeners.
  source.Notify(SomeEvent(copies));

  queue1->BeginShutdown();
  queue1->AwaitShutdownAndIdle();
  queue2->BeginShutdown();
  queue2->AwaitShutdownAndIdle();
  EXPECT_EQ(copies, 0);
  EXPECT_THAT(callbackLog1, testing::ElementsAre(1, 2));
  EXPECT_THAT(callbackLog2, testing::ElementsAre(1, 2));

  listener1.Disconnect();
  listener2.Disconnect();
  listener3.Disconnect();
  listener4.Disconnect();
}

TEST(MediaEventSource, ZeroCopyOneCopyPerThreadOneTarget)
{
  RefPtr<TaskQueue> queue = TaskQueue::Create(
      GetMediaThreadPool(MediaThreadType::SUPERVISOR),
      "TestMediaEventSource ZeroCopyOneCopyPerThreadOneTarget");

  MediaEventProducerOneCopyPerThread<SomeEvent> source;
  int copies = 0;

  static std::vector<int> callbackLog;
  callbackLog.clear();

  auto func = []() { callbackLog.push_back(1); };
  struct Foo {
    void OnNotify() { callbackLog.push_back(2); }
  } foo;

  MediaEventListener listener1 = source.Connect(queue, func);
  MediaEventListener listener2 = source.Connect(queue, &foo, &Foo::OnNotify);

  // We expect i to be 0 since Notify can take ownership of the temp object,
  // which is then used to notify listeners on the single target.
  source.Notify(SomeEvent(copies));

  queue->BeginShutdown();
  queue->AwaitShutdownAndIdle();
  EXPECT_EQ(copies, 0);
  EXPECT_THAT(callbackLog, testing::ElementsAre(1, 2));

  listener1.Disconnect();
  listener2.Disconnect();
}

TEST(MediaEventSource, ZeroCopyOneCopyPerThreadNoArglessCopy)
{
  RefPtr<TaskQueue> queue1 = TaskQueue::Create(
      GetMediaThreadPool(MediaThreadType::SUPERVISOR),
      "TestMediaEventSource ZeroCopyOneCopyPerThreadNoArglessCopy(first)");
  RefPtr<TaskQueue> queue2 = TaskQueue::Create(
      GetMediaThreadPool(MediaThreadType::SUPERVISOR),
      "TestMediaEventSource ZeroCopyOneCopyPerThreadNoArglessCopy(second)");

  MediaEventProducerOneCopyPerThread<SomeEvent> source;
  int copies = 0;

  // func(SomeEvent&&) is ineligible, because OneCopyPerThread passes an lvalue
  // ref.
  auto arglessFunc = []() {};
  auto func = [](SomeEvent& aEvent) {};
  auto func2 = [](const SomeEvent& aEvent) {};
  struct Foo {
    void OnNotify(SomeEvent& aEvent) {}
    void OnNotify2(const SomeEvent& aEvent) {}
  } foo;

  MediaEventListener listener1 = source.Connect(queue1, func);
  MediaEventListener listener2 = source.Connect(queue1, &foo, &Foo::OnNotify);
  MediaEventListener listener3 = source.Connect(queue1, func2);
  MediaEventListener listener4 = source.Connect(queue1, &foo, &Foo::OnNotify2);
  MediaEventListener listener5 = source.Connect(queue2, arglessFunc);

  // We expect i to be 0 since Notify can take ownership of the temp object,
  // and use it to notify the listeners on queue1, since none of the listeners
  // on queue2 take arguments.
  source.Notify(SomeEvent(copies));

  queue1->BeginShutdown();
  queue1->AwaitShutdownAndIdle();
  queue2->BeginShutdown();
  queue2->AwaitShutdownAndIdle();
  EXPECT_EQ(copies, 0);
  listener1.Disconnect();
  listener2.Disconnect();
  listener3.Disconnect();
  listener4.Disconnect();
  listener5.Disconnect();
}

TEST(MediaEventSource, CopyForAdditionalTargets)
{
  RefPtr<TaskQueue> queue1 =
      TaskQueue::Create(GetMediaThreadPool(MediaThreadType::SUPERVISOR),
                        "TestMediaEventSource CopyForAdditionalTargets(first)");
  RefPtr<TaskQueue> queue2 = TaskQueue::Create(
      GetMediaThreadPool(MediaThreadType::SUPERVISOR),
      "TestMediaEventSource CopyForAdditionalTargets(second)");

  MediaEventProducerOneCopyPerThread<SomeEvent> source;
  int copies = 0;

  static std::vector<int> callbackLog1;
  callbackLog1.clear();
  auto func1 = [](SomeEvent& aEvent) { callbackLog1.push_back(0); };
  struct Foo1 {
    void OnNotify(SomeEvent& aEvent) { callbackLog1.push_back(1); }
  } foo1;

  static std::vector<int> callbackLog2;
  callbackLog2.clear();
  auto func2 = [](const SomeEvent& aEvent) { callbackLog2.push_back(0); };
  struct Foo2 {
    void OnNotify(const SomeEvent& aEvent) { callbackLog2.push_back(1); }
  } foo2;

  MediaEventListener listener1 = source.Connect(queue1, func1);
  MediaEventListener listener2 = source.Connect(queue1, &foo1, &Foo1::OnNotify);
  MediaEventListener listener3 = source.Connect(queue2, func2);
  MediaEventListener listener4 = source.Connect(queue2, &foo2, &Foo2::OnNotify);

  // We expect i to be 1 since Notify can take ownership of the temp object,
  // make a copy for the listeners on queue1, and then give the original to the
  // listeners on queue2.
  source.Notify(SomeEvent(copies));

  queue1->BeginShutdown();
  queue1->AwaitShutdownAndIdle();
  queue2->BeginShutdown();
  queue2->AwaitShutdownAndIdle();
  EXPECT_EQ(copies, 1);
  EXPECT_THAT(callbackLog1, testing::ElementsAre(0, 1));
  EXPECT_THAT(callbackLog2, testing::ElementsAre(0, 1));

  listener1.Disconnect();
  listener2.Disconnect();
  listener3.Disconnect();
  listener4.Disconnect();
}

TEST(MediaEventSource, CopyEventUnneeded)
{
  RefPtr<TaskQueue> queue =
      TaskQueue::Create(GetMediaThreadPool(MediaThreadType::SUPERVISOR),
                        "TestMediaEventSource CopyEventUnneeded");

  MediaEventProducer<SomeEvent> source;
  int copies = 0;

  static std::vector<int> callbackLog;
  callbackLog.clear();
  auto func = []() { callbackLog.push_back(0); };
  struct Foo {
    void OnNotify() { callbackLog.push_back(1); }
  } foo;

  MediaEventListener listener1 = source.Connect(queue, func);
  MediaEventListener listener2 = source.Connect(queue, &foo, &Foo::OnNotify);

  // Non-temporary; if Notify takes the event at all, it will need to make at
  // least one copy. It should not need to take it at all, since all listeners
  // are argless.
  std::unique_ptr<SomeEvent> event(new SomeEvent(copies));
  // SomeEvent won't be copied at all since the listeners take no arguments.
  source.Notify(*event);

  queue->BeginShutdown();
  queue->AwaitShutdownAndIdle();
  EXPECT_EQ(copies, 0);
  EXPECT_THAT(callbackLog, testing::ElementsAre(0, 1));

  listener1.Disconnect();
  listener2.Disconnect();
}

/*
 * Test move-only types.
 */
TEST(MediaEventSource, MoveOnly)
{
  RefPtr<TaskQueue> queue =
      TaskQueue::Create(GetMediaThreadPool(MediaThreadType::SUPERVISOR),
                        "TestMediaEventSource MoveOnly");

  MediaEventProducerExc<UniquePtr<int>> source;
  static std::vector<int> callbackLog;
  callbackLog.clear();

  auto func = [](UniquePtr<int>&& aEvent) { callbackLog.push_back(*aEvent); };
  MediaEventListener listener = source.Connect(queue, func);

  // It is OK to pass an rvalue which is move-only.
  source.Notify(UniquePtr<int>(new int(20)));
  // It is an error to pass an lvalue which is move-only.
  // UniquePtr<int> event(new int(30));
  // source.Notify(event);

  queue->BeginShutdown();
  queue->AwaitShutdownAndIdle();

  EXPECT_THAT(callbackLog, testing::ElementsAre(20));

  listener.Disconnect();
}

TEST(MediaEventSource, ExclusiveConstLvalueRef)
{
  RefPtr<TaskQueue> queue =
      TaskQueue::Create(GetMediaThreadPool(MediaThreadType::SUPERVISOR),
                        "TestMediaEventSource ExclusiveConstLvalueRef");

  MediaEventProducerExc<UniquePtr<int>> source;
  static std::vector<int> callbackLog;
  callbackLog.clear();

  auto func = [](const UniquePtr<int>& aEvent) {
    callbackLog.push_back(*aEvent);
  };
  MediaEventListener listener = source.Connect(queue, func);

  source.Notify(UniquePtr<int>(new int(20)));

  queue->BeginShutdown();
  queue->AwaitShutdownAndIdle();

  EXPECT_THAT(callbackLog, testing::ElementsAre(20));

  listener.Disconnect();
}

TEST(MediaEventSource, ExclusiveNoArgs)
{
  RefPtr<TaskQueue> queue =
      TaskQueue::Create(GetMediaThreadPool(MediaThreadType::SUPERVISOR),
                        "TestMediaEventSource ExclusiveNoArgs");

  MediaEventProducerExc<UniquePtr<int>> source;
  static int callbackCount = 0;

  auto func = []() { ++callbackCount; };
  MediaEventListener listener = source.Connect(queue, func);

  source.Notify(UniquePtr<int>(new int(20)));

  queue->BeginShutdown();
  queue->AwaitShutdownAndIdle();

  ASSERT_EQ(callbackCount, 1);

  listener.Disconnect();
}

struct RefCounter {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(RefCounter)
  explicit RefCounter(int aVal) : mVal(aVal) {}
  int mVal;

 private:
  ~RefCounter() = default;
};

/*
 * Test we should copy instead of move in NonExclusive mode
 * for each listener must get a copy.
 */
TEST(MediaEventSource, NoMove)
{
  RefPtr<TaskQueue> queue =
      TaskQueue::Create(GetMediaThreadPool(MediaThreadType::SUPERVISOR),
                        "TestMediaEventSource NoMove");

  MediaEventProducer<RefPtr<RefCounter>> source;

  auto func1 = [](const RefPtr<RefCounter>& aEvent) {
    EXPECT_EQ(aEvent->mVal, 20);
  };
  auto func2 = [](const RefPtr<RefCounter>& aEvent) {
    EXPECT_EQ(aEvent->mVal, 20);
  };
  MediaEventListener listener1 = source.Connect(queue, func1);
  MediaEventListener listener2 = source.Connect(queue, func2);

  // We should copy this rvalue instead of move it in NonExclusive mode.
  RefPtr<RefCounter> val = new RefCounter(20);
  source.Notify(std::move(val));

  queue->BeginShutdown();
  queue->AwaitShutdownAndIdle();
  listener1.Disconnect();
  listener2.Disconnect();
}

/*
 * Rvalue lambda should be moved instead of copied.
 */
TEST(MediaEventSource, MoveLambda)
{
  RefPtr<TaskQueue> queue =
      TaskQueue::Create(GetMediaThreadPool(MediaThreadType::SUPERVISOR),
                        "TestMediaEventSource MoveLambda");

  MediaEventProducer<void> source;

  int counter = 0;
  SomeEvent someEvent(counter);

  auto func = [someEvent]() {};
  // someEvent is copied when captured by the lambda.
  EXPECT_EQ(someEvent.mCount, 1);

  // someEvent should be copied for we pass |func| as an lvalue.
  MediaEventListener listener1 = source.Connect(queue, func);
  EXPECT_EQ(someEvent.mCount, 2);

  // someEvent should be moved for we pass |func| as an rvalue.
  MediaEventListener listener2 = source.Connect(queue, std::move(func));
  EXPECT_EQ(someEvent.mCount, 2);

  listener1.Disconnect();
  listener2.Disconnect();
}

template <typename Bool>
struct DestroyChecker {
  explicit DestroyChecker(Bool* aIsDestroyed) : mIsDestroyed(aIsDestroyed) {
    EXPECT_FALSE(*mIsDestroyed);
  }
  ~DestroyChecker() {
    EXPECT_FALSE(*mIsDestroyed);
    *mIsDestroyed = true;
  }

 private:
  Bool* const mIsDestroyed;
};

class ClassForDestroyCheck final : private DestroyChecker<bool> {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(ClassForDestroyCheck);

  explicit ClassForDestroyCheck(bool* aIsDestroyed)
      : DestroyChecker(aIsDestroyed) {}

  int32_t RefCountNums() const { return mRefCnt; }

 protected:
  ~ClassForDestroyCheck() = default;
};

TEST(MediaEventSource, ResetFuncReferenceAfterDisconnect)
{
  const RefPtr<TaskQueue> queue = TaskQueue::Create(
      GetMediaThreadPool(MediaThreadType::SUPERVISOR),
      "TestMediaEventSource ResetFuncReferenceAfterDisconnect");
  MediaEventProducer<void> source;

  // Using a class that supports refcounting to check the object destruction.
  bool isDestroyed = false;
  auto object = MakeRefPtr<ClassForDestroyCheck>(&isDestroyed);
  EXPECT_FALSE(isDestroyed);
  EXPECT_EQ(object->RefCountNums(), 1);

  // Function holds a strong reference to object.
  MediaEventListener listener = source.Connect(queue, [ptr = object] {});
  EXPECT_FALSE(isDestroyed);
  EXPECT_EQ(object->RefCountNums(), 2);

  // This should destroy the function and release the object reference from the
  // function on the task queue,
  listener.Disconnect();
  queue->BeginShutdown();
  queue->AwaitShutdownAndIdle();
  EXPECT_FALSE(isDestroyed);
  EXPECT_EQ(object->RefCountNums(), 1);

  // No one is holding reference to object, it should be destroyed
  // immediately.
  object = nullptr;
  EXPECT_TRUE(isDestroyed);
}

TEST(MediaEventSource, ResetTargetAfterDisconnect)
{
  RefPtr<TaskQueue> queue =
      TaskQueue::Create(GetMediaThreadPool(MediaThreadType::SUPERVISOR),
                        "TestMediaEventSource ResetTargetAfterDisconnect");
  MediaEventProducer<void> source;
  MediaEventListener listener = source.Connect(queue, [] {});

  // MediaEventListener::Disconnect eventually gives up its target
  listener.Disconnect();
  queue->AwaitIdle();

  // `queue` should be the last reference to the TaskQueue, meaning that this
  // Release destroys it.
  EXPECT_EQ(queue.forget().take()->Release(), 0u);
}

TEST(MediaEventSource, TailDispatch)
{
  MockFunction<void(const char*)> checkpoint;
  {
    InSequence seq;
    EXPECT_CALL(checkpoint, Call(StrEq("normal runnable")));
    EXPECT_CALL(checkpoint, Call(StrEq("source1")));
    EXPECT_CALL(checkpoint, Call(StrEq("tail-dispatched runnable")));
    EXPECT_CALL(checkpoint, Call(StrEq("source2")));
  }

  MediaEventProducer<void> source1;
  MediaEventListener listener1 = source1.Connect(
      AbstractThread::MainThread(), [&] { checkpoint.Call("source1"); });
  MediaEventProducer<void> source2;
  MediaEventListener listener2 = source2.Connect(
      AbstractThread::MainThread(), [&] { checkpoint.Call("source2"); });

  AbstractThread::MainThread()->Dispatch(NS_NewRunnableFunction(__func__, [&] {
    // Notify, using tail-dispatch.
    source1.Notify();
    // Dispatch runnable, using tail-dispatch.
    AbstractThread::MainThread()->Dispatch(NS_NewRunnableFunction(
        __func__, [&] { checkpoint.Call("tail-dispatched runnable"); }));
    // Notify other event, using tail-dispatch.
    source2.Notify();
    // Dispatch runnable to the underlying event target, i.e. without
    // tail-dispatch. Doesn't dispatch from a direct task so should run before
    // tail-dispatched tasks.
    GetMainThreadSerialEventTarget()->Dispatch(NS_NewRunnableFunction(
        __func__, [&] { checkpoint.Call("normal runnable"); }));
  }));

  NS_ProcessPendingEvents(nullptr);

  listener1.Disconnect();
  listener2.Disconnect();
}
