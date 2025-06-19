/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebrtcTaskQueueWrapper.h"

#include "api/task_queue/task_queue_factory.h"
#include "mozilla/DataMutex.h"
#include "mozilla/RecursiveMutex.h"
#include "mozilla/TaskQueue.h"
#include "mozilla/media/MediaUtils.h"  // For media::Await
#include "VideoUtils.h"

namespace mozilla {
enum class DeletionPolicy : uint8_t { Blocking, NonBlocking };

/**
 * A wrapper around Mozilla TaskQueues in the shape of a libwebrtc TaskQueue.
 *
 * Allows libwebrtc to use Mozilla threads where tooling, e.g. profiling, is set
 * up and just works.
 *
 * Mozilla APIs like Runnables, MozPromise, etc. can also be used with the
 * wrapped TaskQueue to run things on the right thread when interacting with
 * libwebrtc.
 */
template <DeletionPolicy Deletion>
class WebrtcTaskQueueWrapper : public webrtc::TaskQueueBase {
 public:
  class TaskQueueObserver final : public TaskQueue::Observer {
   public:
    NS_INLINE_DECL_THREADSAFE_REFCOUNTING(TaskQueueObserver, override);

    template <typename Wrapper>
    explicit TaskQueueObserver(Wrapper aOwner)
        : mOwner(std::forward<Wrapper>(aOwner)) {}

    void WillProcessEvent(TaskQueue* aQueue) override {
      if constexpr (Deletion == DeletionPolicy::Blocking) {
        mCurrent.emplace(mOwner);
      } else {
        static_assert(Deletion == DeletionPolicy::NonBlocking);
        mCurrent.emplace(mOwner.get());
      }
    }
    void DidProcessEvent(TaskQueue* aQueue) override { mCurrent = Nothing(); }

   private:
    ~TaskQueueObserver() override = default;
    // If NonBlocking, a TaskQueue owns this observer, which owns mOwner, which
    // must be kept alive. There are no cycles.
    //
    // If Blocking, mOwner owns the TaskQueue, which owns us. mOwner is owned
    // externally. It must be a weak reference here, or we'd have a cycle.
    //
    // mOwner is safe because the underlying TaskQueue first finishes shutdown,
    // then the observer is destroyed, then the WebrtcTaskQueueWrapper is
    // destroyed. See the WebrtcTaskQueueWrawpper::Delete for more details.
    std::conditional_t<Deletion == DeletionPolicy::NonBlocking,
                       UniquePtr<WebrtcTaskQueueWrapper>,
                       WebrtcTaskQueueWrapper*> const mOwner;
    Maybe<CurrentTaskQueueSetter> mCurrent;
  };

 public:
  template <typename Target>
  WebrtcTaskQueueWrapper(Target aTaskQueue, nsCString aName)
      : mTaskQueue(std::forward<Target>(aTaskQueue)), mName(std::move(aName)) {}
  ~WebrtcTaskQueueWrapper() = default;

  void Delete() override {
    {
      // Scope this to make sure it does not race against the promise chain we
      // set up below.
      auto hasShutdown = mHasShutdown.Lock();
      *hasShutdown = true;
    }

    MOZ_RELEASE_ASSERT(Deletion == DeletionPolicy::NonBlocking ||
                       !mTaskQueue->IsOnCurrentThread());

    if constexpr (Deletion == DeletionPolicy::NonBlocking) {
      delete this;
      return;
    }

    nsCOMPtr<nsISerialEventTarget> backgroundTaskQueue;
    NS_CreateBackgroundTaskQueue(__func__, getter_AddRefs(backgroundTaskQueue));
    if (NS_WARN_IF(!backgroundTaskQueue)) {
      // Ok... that's pretty broken. Try main instead.
      MOZ_ASSERT(false);
      backgroundTaskQueue = GetMainThreadSerialEventTarget();
    }

    RefPtr<GenericPromise> shutdownPromise = mTaskQueue->BeginShutdown()->Then(
        backgroundTaskQueue, __func__, [this] {
          // Wait until shutdown is complete, then delete for real. Although we
          // prevent queued tasks from executing with mHasShutdown, that is a
          // member variable, which means we still need to ensure that the
          // queue is done executing tasks before destroying it.
          mTaskQueue->SetObserver(nullptr);
          delete this;
          return GenericPromise::CreateAndResolve(true, __func__);
        });
    media::Await(backgroundTaskQueue.forget(), shutdownPromise);
  }

  already_AddRefed<Runnable> CreateTaskRunner(
      absl::AnyInvocable<void() &&>&& aTask) {
    return NS_NewRunnableFunction(
        __func__, [this, task = std::move(aTask)]() mutable {
          auto hasShutdownGuard = mHasShutdown.ConstLock();
          if (*hasShutdownGuard) {
            return;
          }
          std::move(task)();
        });
  }

  void PostTaskImpl(absl::AnyInvocable<void() &&> aTask,
                    const PostTaskTraits& aTraits,
                    const webrtc::Location& aLocation) override {
    MOZ_ALWAYS_SUCCEEDS(
        mTaskQueue->Dispatch(CreateTaskRunner(std::move(aTask))));
  }

  void PostDelayedTaskImpl(absl::AnyInvocable<void() &&> aTask,
                           webrtc::TimeDelta aDelay,
                           const PostDelayedTaskTraits& aTraits,
                           const webrtc::Location& aLocation) override {
    if (aDelay.ms() == 0) {
      // AbstractThread::DelayedDispatch doesn't support delay 0
      PostTaskImpl(std::move(aTask), PostTaskTraits{}, aLocation);
      return;
    }
    MOZ_ALWAYS_SUCCEEDS(mTaskQueue->DelayedDispatch(
        CreateTaskRunner(std::move(aTask)), aDelay.ms()));
  }

  // If Blocking, access is through WebrtcTaskQueueWrapper, which has to keep
  // mTaskQueue alive. If NonBlocking, mTaskQueue keeps WebrtcTaskQueueWrapper
  // alive through the observer. We must not create a cycle.
  const std::conditional_t<Deletion == DeletionPolicy::Blocking,
                           RefPtr<TaskQueue>, TaskQueue*>
      mTaskQueue;
  const nsCString mName;

  // This is a recursive mutex because a TaskRunner holding this mutex while
  // running its runnable may end up running other - tail dispatched - runnables
  // too, and they'll again try to grab the mutex.
  // The mutex must be held while running the runnable since otherwise there'd
  // be a race between shutting down the underlying task queue and the runnable
  // dispatching to that task queue (and we assert it succeeds in e.g.,
  // PostTask()).
  DataMutexBase<bool, RecursiveMutex> mHasShutdown{
      false, "WebrtcTaskQueueWrapper::mHasShutdown"};
};

template <DeletionPolicy Deletion>
class DefaultDelete<WebrtcTaskQueueWrapper<Deletion>>
    : public webrtc::TaskQueueDeleter {
 public:
  void operator()(WebrtcTaskQueueWrapper<Deletion>* aPtr) const {
    webrtc::TaskQueueDeleter::operator()(aPtr);
  }
};

std::unique_ptr<webrtc::TaskQueueBase, webrtc::TaskQueueDeleter>
CreateWebrtcTaskQueue(already_AddRefed<nsIEventTarget> aTarget,
                      const nsCString& aName, bool aSupportsTailDispatch) {
  using Wrapper = WebrtcTaskQueueWrapper<DeletionPolicy::Blocking>;
  auto tq =
      TaskQueue::Create(std::move(aTarget), aName.get(), aSupportsTailDispatch);
  auto wrapper = MakeUnique<Wrapper>(std::move(tq), aName);
  auto observer = MakeRefPtr<Wrapper::TaskQueueObserver>(wrapper.get());
  wrapper->mTaskQueue->SetObserver(observer);
  return std::unique_ptr<webrtc::TaskQueueBase, webrtc::TaskQueueDeleter>(
      wrapper.release());
}

RefPtr<TaskQueue> CreateWebrtcTaskQueueWrapper(
    already_AddRefed<nsIEventTarget> aTarget, const nsCString& aName,
    bool aSupportsTailDispatch) {
  using Wrapper = WebrtcTaskQueueWrapper<DeletionPolicy::NonBlocking>;
  auto tq =
      TaskQueue::Create(std::move(aTarget), aName.get(), aSupportsTailDispatch);
  auto wrapper = MakeUnique<Wrapper>(tq.get(), aName);
  auto observer = MakeRefPtr<Wrapper::TaskQueueObserver>(std::move(wrapper));
  tq->SetObserver(observer);
  return tq;
}

UniquePtr<webrtc::TaskQueueFactory> CreateWebrtcTaskQueueFactory() {
  class SharedThreadPoolWebRtcTaskQueueFactory
      : public webrtc::TaskQueueFactory {
   public:
    std::unique_ptr<webrtc::TaskQueueBase, webrtc::TaskQueueDeleter>
    CreateTaskQueue(absl::string_view aName,
                    Priority aPriority) const override {
      // libwebrtc will dispatch some tasks sync, i.e., block the origin thread
      // until they've run, and that doesn't play nice with tail dispatching
      // since there will never be a tail. DeletionPolicy::Blocking because this
      // is for libwebrtc use and that's what they expect.
      nsCString name(aName.data(), aName.size());
      constexpr bool supportTailDispatch = false;
      // XXX Do something with aPriority
      return CreateWebrtcTaskQueue(
          GetMediaThreadPool(MediaThreadType::WEBRTC_WORKER), name,
          supportTailDispatch);
    }
  };

  return WrapUnique(new SharedThreadPoolWebRtcTaskQueueFactory);
}

}  // namespace mozilla
