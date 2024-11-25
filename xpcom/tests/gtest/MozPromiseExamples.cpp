/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include <chrono>
#include <thread>
#include "ErrorList.h"
#include "gtest/gtest.h"
#include "mozilla/MozPromise.h"
#include "mozilla/SpinEventLoopUntil.h"
#include "nsISupportsImpl.h"
#include "nsThreadUtils.h"

using namespace mozilla;

// Simple function to be able to distinguish threads in output
size_t tid() {
  return std::hash<std::thread::id>{}(std::this_thread::get_id());
}

// Invoking something on a background thread, but getting the completion on the
// main thread.
TEST(MozPromiseExamples, InvokeAsync)
{
  bool done = false;
  InvokeAsync(
      GetCurrentSerialEventTarget(), __func__,
      []() {
        printf("[%zu] Doing some work on a background thread...\n", tid());
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        printf("[%zu] Done...\n", tid());

        // Simulate various outcomes:
        srand(getpid());
        switch (rand() % 4) {
          case 0:
            return GenericPromise::CreateAndResolve(true, __func__);
          case 1:
            return GenericPromise::CreateAndResolve(false, __func__);
          case 2:
            return GenericPromise::CreateAndReject(NS_ERROR_OUT_OF_MEMORY,
                                                   __func__);
          default:
            return GenericPromise::CreateAndReject(NS_ERROR_FAILURE, __func__);
        }
      })
      ->Then(
          GetMainThreadSerialEventTarget(), __func__,
          [&done](GenericPromise::ResolveOrRejectValue&& aResult) {
            if (aResult.IsReject()) {
              printf("[%zu] Back on the main thread, the task failed: 0x%x\n",
                     tid(), (unsigned int)aResult.RejectValue());
              done = true;
              return;
            }
            printf("[%zu] back on the main thread, sucess, return value: %s\n",
                   tid(), aResult.ResolveValue() ? "true" : "false");
            done = true;
          });

  // Process all events and check that `done` was effectively set to true. This
  // is just for the purpose of this test.
  MOZ_ALWAYS_TRUE(SpinEventLoopUntil(
      "xpcom:TEST(MozPromiseExamples, OneOff)"_ns, [&done]() { return done; }));
  EXPECT_TRUE(done);
}

class Something final {
 public:
  explicit Something(uint32_t aMilliseconds = 100)
      : mMilliseconds(aMilliseconds) {}
  RefPtr<GenericPromise> DoIt() {
    // Do no dispatch the async task twice if still underway.
    if (mPromise) {
      return mPromise;
    }
    mPromise = mHolder.Ensure(__func__);
    // Kick off some work to another thread...
    std::thread([self = RefPtr{this}, this] {
      printf("[%zu] Working...\n", tid());
      std::this_thread::sleep_for(std::chrono::milliseconds(mMilliseconds));
      printf("[%zu] Resolving from background thread\n", tid());
      self->mHolder.Resolve(true, __func__);
    }).detach();
    return mPromise;
  }

 private:
  ~Something() = default;
  const uint32_t mMilliseconds;
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(Something)
  RefPtr<GenericPromise> mPromise;
  MozPromiseHolder<GenericPromise> mHolder{};
};

// Waiting for something asynchronous to complete, from outside the instance
TEST(MozPromiseExamples, OneOff)
{
  RefPtr<Something> thing(new Something);
  bool done = false;

  thing->DoIt()->Then(
      GetCurrentSerialEventTarget(), __func__,
      [&done, thing](bool aResult) {
        printf("[%zu] Success: %s\n", tid(), aResult ? "true" : "false");
        done = true;
      },
      [&done](nsresult aError) {
        printf("[%zu] Failure: 0x%x\n", tid(), (unsigned)aError);
        done = true;
      });

  // Process all events and check that `done` was effectively set to true. This
  // is just for the purpose of this test.
  MOZ_ALWAYS_TRUE(SpinEventLoopUntil(
      "xpcom:TEST(MozPromiseExamples, OneOff)"_ns, [&done]() { return done; }));
}

class SomethingCancelable final {
 public:
  RefPtr<GenericPromise> DoIt() {
    if (mPromise) {
      return mPromise;
    }
    mPromise = mHolder.Ensure(__func__);
    // Kick off some work to another thread...
    std::thread([self = RefPtr{this}] {
      printf("[%zu] Working...\n", tid());
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      // This is printed: despite being canceled, the thread runs normally and
      // resolves its promise.
      printf("[%zu] Resolving from background thread\n", tid());
      self->mHolder.Resolve(true, __func__);
    }).detach();
    return mPromise;
  }

 private:
  ~SomethingCancelable() = default;
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(SomethingCancelable)
  MozPromiseHolder<GenericPromise> mHolder{};
  RefPtr<GenericPromise> mPromise;
  MozPromiseRequestHolder<GenericPromise> mRequest;
};

// Kick of an asynchronous job, and cancel it
TEST(MozPromiseExamples, OneOffCancelable)
{
  RefPtr<SomethingCancelable> thing(new SomethingCancelable);

  // Start a job that takes 100ms
  MozPromiseRequestHolder<GenericPromise> holder;
  thing->DoIt()
      ->Then(GetCurrentSerialEventTarget(), __func__,
             [&holder] {
               holder.Complete();
               // This is never printed: in this example we disconnect the
               // request before completion.
               printf("[%zu] Async work finished", tid());
             })
      ->Track(holder);
  // But cancel it after just 10ms
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  holder.Disconnect();
}

// Waiting for multiple asynchronous tasks to complete, from outside the
// instance
TEST(MozPromiseExamples, MultipleWaits)
{
  nsTArray<RefPtr<Something>> things;
  uint32_t count = 10;
  while (count--) {
    things.AppendElement(new Something(count * 10));
  }
  bool done = false;

  nsTArray<RefPtr<GenericPromise>> promises;
  for (auto& thing : things) {
    promises.AppendElement(thing->DoIt());
  }

  GenericPromise::All(GetCurrentSerialEventTarget(), promises)
      ->Then(
          GetCurrentSerialEventTarget(), __func__,
          [&done](nsTArray<bool>&& aResults) {
            nsCString results;
            for (auto b : aResults) {
              results.AppendPrintf("%s, ", b ? "true" : "false");
            }
            printf("[%zu] All succeeded: %s\n", tid(), results.get());
            done = true;
          },
          [&done](nsresult aError) {
            printf("[%zu] One failed: 0x%x\n", tid(), (unsigned)aError);
            done = true;
          });

  // Process all events and check that `done` was effectively set to true. This
  // is just for the purpose of this test.
  MOZ_ALWAYS_TRUE(SpinEventLoopUntil(
      "xpcom:TEST(MozPromiseExamples, OneOff)"_ns, [&done]() { return done; }));
}

RefPtr<GenericPromise> SyncOperation(uint32_t aConstraint) {
  printf("[%zu] SyncOperation(%" PRIu32 ")\n", tid(), aConstraint);
  if (aConstraint > 5) {
    return GenericPromise::CreateAndReject(NS_ERROR_UNEXPECTED, __func__);
  }

  return GenericPromise::CreateAndResolve(true, __func__);
}

// This test uses various MozPromise facilities and prints a message to the
// console, to show how the scheduling works.
TEST(MozPromiseExamples, SyncReturn)
{
  bool done = false;
  // Dispatch a runnable to the current even loop, for the sole purpose of
  // understanding ordering.
  NS_DispatchToCurrentThread(NS_NewRunnableFunction("Initial runnable", [] {
    printf("[%zu] Dispatched before sync promise operation\n", tid());
  }));
  // SyncOperation synchronously returns a resolved promise. However, `Then`
  // works by dispatching so the printf will happen after InitialRunnable.
  SyncOperation(3)->Then(
      GetCurrentSerialEventTarget(), __func__,
      [](bool aResult) {
        printf("[%zu] Sync promise value: %s\n", tid(),
               aResult ? "true" : "false");
      },
      [](nsresult aError) {
        printf("[%zu] Error: 0x%x\n", tid(), (unsigned)aError);
      });
  // Now call the same method, but invoke it async on the current event queue.
  // The resolve will also be in its own event loop task. It follows that this
  // will be printed after the "Final Runnable" below.
  // MozPromise can be put in tail dispatch mode,or sync mode, and in those
  // cases, the ordering will be different.
  InvokeAsync(GetCurrentSerialEventTarget(), __func__,
              []() { return SyncOperation(4); })
      ->Then(
          GetCurrentSerialEventTarget(), __func__,
          [&done](bool aResult) {
            printf("[%zu] Sync promise value (InvokeAsync): %s\n", tid(),
                   aResult ? "true" : "false");
            done = true;
          },
          [](nsresult aError) {
            printf("[%zu] Error (InvokeAsync): 0x%x\n", tid(),
                   (unsigned)aError);
          });
  // Dispatch a runnable to the current even loop, for the sole purpose of
  // understanding ordering.
  NS_DispatchToCurrentThread(NS_NewRunnableFunction("Final runnable", [] {
    printf("[%zu] Dispatched after sync promise operation\n", tid());
  }));

  // The output will be as such (omitting the thread ids):
  // [...] SyncOperation(3)
  // [...] Dispatched before sync promise operation
  // [...] Sync promise value: true
  // [...] SyncOperation(4)
  // [...] Dispatched after sync promise operation
  // [...] Sync promise value (InvokeAsync): true

  // Process all events and check that `done` was effectively set to true. This
  // is just for the purpose of this test.
  MOZ_ALWAYS_TRUE(SpinEventLoopUntil(
      "xpcom:TEST(MozPromiseExamples, OneOff)"_ns, [&done]() { return done; }));
}

using IntPromise = MozPromise<int, nsresult, true>;
using UintPromise = MozPromise<unsigned, nsresult, true>;

class SomethingSync {
 public:
  RefPtr<GenericPromise> DoSomethingSync() {
    return GenericPromise::CreateAndResolve(true, "Returning true");
  }
};

TEST(MozPromiseExamples, Chaining)
{
  bool done = false;
  SomethingSync s;
  // Do something that returns a bool, then chain it to a promise that returns
  // an int, then to a promise that returns an unsigned.
  s.DoSomethingSync()
      ->Then(GetCurrentSerialEventTarget(), __func__,
             [](GenericPromise::ResolveOrRejectValue&& aValue) {
               if (aValue.IsResolve()) {
                 // Depending on the value of the bool, find the proper signed
                 // integer value.
                 return IntPromise::CreateAndResolve(
                     aValue.ResolveValue() ? 3 : 5,
                     "Example IntPromise Resolver");
               }
               return IntPromise::CreateAndReject(
                   aValue.RejectValue(), "Example IntPromise Rejecter");
             })
      ->Then(GetCurrentSerialEventTarget(), __func__,
             [&done](IntPromise::ResolveOrRejectValue&& aValue) {
               if (aValue.IsResolve()) {
                 // Depending on the value of the signed integer, find the
                 // proper unsigned integer value.
                 done = true;
                 return UintPromise::CreateAndResolve(
                     static_cast<unsigned>(aValue.ResolveValue()),
                     "Example UintPromise Resolver");
               }
               return UintPromise::CreateAndReject(
                   aValue.RejectValue(), "Example UintPromise Rejecter");
             });

  // Process all events and check that `done` was effectively set to true. This
  // is just for the purpose of this test.
  MOZ_ALWAYS_TRUE(
      SpinEventLoopUntil("xpcom:TEST(MozPromiseExamples, Chaining)"_ns,
                         [&done]() { return done; }));
}

// - converting an async legacy callback interface to a modern MozPromise
// version with MozPromiseHolder.
