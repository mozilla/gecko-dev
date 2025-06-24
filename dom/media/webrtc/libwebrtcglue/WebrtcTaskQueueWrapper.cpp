/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebrtcTaskQueueWrapper.h"

#include "api/task_queue/task_queue_factory.h"
#include "mozilla/TaskQueue.h"
#include "nsThreadUtils.h"
#include "VideoUtils.h"

#ifdef MOZ_COLLECTING_RUNNABLE_TELEMETRY
#  include "fmt/format.h"
#endif

namespace mozilla {
#ifdef MOZ_COLLECTING_RUNNABLE_TELEMETRY
class InvocableRunnable final : public nsIRunnable, public nsINamed {
  // Storage for caching the name returned from GetName().
  Maybe<nsAutoCString> mName;

  // These are used to construct mName.
  const char* const mTaskQueueName;
  const WebrtcLocation mLocation;

  absl::AnyInvocable<void() &&> mTask;

  ~InvocableRunnable() = default;

 public:
  NS_DECL_THREADSAFE_ISUPPORTS

  InvocableRunnable(const char* aTaskQueueName,
                    absl::AnyInvocable<void() &&>&& aTask,
                    const WebrtcLocation& aLocation)
      : mTaskQueueName(aTaskQueueName),
        mLocation(aLocation),
        mTask(std::move(aTask)) {}

  NS_IMETHOD Run() override {
    std::move(mTask)();
    return NS_OK;
  }

  NS_IMETHOD GetName(nsACString& aName) override {
    if (mName) {
      aName = *mName;
      return NS_OK;
    }

    Maybe<nsDependentCSubstring> fileName;
    if (mLocation.mFile) {
      nsDependentCString f(mLocation.mFile);
#  ifdef XP_WIN
      // On Windows, path separators are inconsistent per
      // https://github.com/llvm/llvm-project/issues/45076.
      int32_t i = f.RFindCharInSet("/\\");
#  else
      int32_t i = f.RFindChar('/');
#  endif
      if (i == kNotFound) {
        fileName.emplace(f);
      } else {
        fileName.emplace(f, i + 1);
      }
    }

    mName.emplace();
    if (mLocation.mFunction && fileName && mLocation.mLine) {
      mName->AppendFmt(FMT_STRING("{} - {} ({}:{})"), mTaskQueueName,
                       mLocation.mFunction, *fileName, mLocation.mLine);
    } else if (fileName && mLocation.mLine) {
      mName->AppendFmt(FMT_STRING("{} - InvocableRunnable ({}:{})"),
                       mTaskQueueName, *fileName, mLocation.mLine);
    } else {
      mName->AppendFmt(FMT_STRING("{} - InvocableRunnable"), mTaskQueueName);
    }
    aName = *mName;

    return NS_OK;
  }
};

NS_IMPL_ISUPPORTS(InvocableRunnable, nsIRunnable, nsINamed)
#endif

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
  WebrtcTaskQueueWrapper(Target aTaskQueue, const nsACString& aName)
      : mTaskQueue(std::forward<Target>(aTaskQueue)), mName(aName) {}
  ~WebrtcTaskQueueWrapper() = default;

  void Delete() override {
    if constexpr (Deletion == DeletionPolicy::Blocking) {
      MOZ_RELEASE_ASSERT(!mTaskQueue->IsOnCurrentThread());
      // Don't call into the task queue if non-blocking as it is in the middle
      // of its dtor. There'd be nothing to wait for anyway.
      mTaskQueue->BeginShutdown();
      mTaskQueue->AwaitShutdownAndIdle();
      mTaskQueue->SetObserver(nullptr);
    }

    delete this;
  }

  already_AddRefed<nsIRunnable> WrapInvocable(
      absl::AnyInvocable<void() &&>&& aTask, const WebrtcLocation& aLocation) {
#ifdef MOZ_COLLECTING_RUNNABLE_TELEMETRY
    return MakeAndAddRef<InvocableRunnable>(mName.get(), std::move(aTask),
                                            aLocation);
#else
    return NS_NewRunnableFunction(
        "InvocableRunnable",
        [task = std::move(aTask)]() mutable { std::move(task)(); });
#endif
  }

  void PostTaskImpl(absl::AnyInvocable<void() &&> aTask,
                    const PostTaskTraits& aTraits,
                    const webrtc::Location& aLocation) override {
    MOZ_ALWAYS_SUCCEEDS(
        mTaskQueue->Dispatch(WrapInvocable(std::move(aTask), aLocation)));
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
        WrapInvocable(std::move(aTask), aLocation), aDelay.ms()));
  }

  // If Blocking, access is through WebrtcTaskQueueWrapper, which has to keep
  // mTaskQueue alive. If NonBlocking, mTaskQueue keeps WebrtcTaskQueueWrapper
  // alive through the observer. We must not create a cycle.
  const std::conditional_t<Deletion == DeletionPolicy::Blocking,
                           RefPtr<TaskQueue>, TaskQueue*>
      mTaskQueue;
  // Storage for mTaskQueue's null-terminated const char* name.
  const nsCString mName;
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
                      const nsACString& aName, bool aSupportsTailDispatch) {
  using Wrapper = WebrtcTaskQueueWrapper<DeletionPolicy::Blocking>;
  const auto& flat = PromiseFlatCString(aName);
  auto tq =
      TaskQueue::Create(std::move(aTarget), flat.get(), aSupportsTailDispatch);
  auto wrapper = MakeUnique<Wrapper>(std::move(tq), flat);
  auto observer = MakeRefPtr<Wrapper::TaskQueueObserver>(wrapper.get());
  wrapper->mTaskQueue->SetObserver(observer);
  return std::unique_ptr<webrtc::TaskQueueBase, webrtc::TaskQueueDeleter>(
      wrapper.release());
}

RefPtr<TaskQueue> CreateWebrtcTaskQueueWrapper(
    already_AddRefed<nsIEventTarget> aTarget, const nsACString& aName,
    bool aSupportsTailDispatch) {
  using Wrapper = WebrtcTaskQueueWrapper<DeletionPolicy::NonBlocking>;
  const auto& flat = PromiseFlatCString(aName);
  auto tq =
      TaskQueue::Create(std::move(aTarget), flat.get(), aSupportsTailDispatch);
  auto wrapper = MakeUnique<Wrapper>(tq.get(), flat);
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
      constexpr bool supportTailDispatch = false;
      // XXX Do something with aPriority
      return CreateWebrtcTaskQueue(
          GetMediaThreadPool(MediaThreadType::WEBRTC_WORKER),
          nsDependentCSubstring(aName.data(), aName.size()),
          supportTailDispatch);
    }
  };

  return WrapUnique(new SharedThreadPoolWebRtcTaskQueueFactory);
}

}  // namespace mozilla
