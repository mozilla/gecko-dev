/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MediaEventSource_h_
#define MediaEventSource_h_

#include <type_traits>
#include <utility>

#include "mozilla/Atomics.h"
#include "mozilla/DataMutex.h"
#include "mozilla/Mutex.h"

#include "mozilla/Unused.h"

#include "nsISupportsImpl.h"
#include "nsTArray.h"
#include "nsThreadUtils.h"

namespace mozilla {

/**
 * A thread-safe tool to communicate "revocation" across threads. It is used to
 * disconnect a listener from the event source to prevent future notifications
 * from coming. Revoke() can be called on any thread. However, it is recommended
 * to be called on the target thread to avoid race condition.
 *
 * RevocableToken is not exposed to the client code directly.
 * Use MediaEventListener below to do the job.
 */
class RevocableToken {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(RevocableToken);

 public:
  RevocableToken() = default;

  virtual void Revoke() = 0;
  virtual bool IsRevoked() const = 0;

 protected:
  // Virtual destructor is required since we might delete a Listener object
  // through its base type pointer.
  virtual ~RevocableToken() = default;
};

enum class ListenerPolicy : int8_t {
  // Allow at most one listener. Move will be used when possible
  // to pass the event data to save copy.
  Exclusive,
  // Allow multiple listeners, which will be given thread-scoped refs of
  // the event data. For N targets/threads, this results in N-1 copies.
  OneCopyPerThread,
  // Allow multiple listeners, which will all be given a const& to the same
  // event data.
  NonExclusive
};

namespace detail {

/**
 * Define how an event type is passed internally in MediaEventSource and to the
 * listeners. Specialized for the void type to pass a dummy bool instead.
 */
template <typename T>
struct EventTypeTraits {
  typedef T ArgType;
};

template <>
struct EventTypeTraits<void> {
  typedef bool ArgType;
};

/**
 * Test if a method function or lambda accepts one or more arguments.
 */
template <typename T>
class TakeArgsHelper {
  template <typename C>
  static std::false_type test(void (C::*)(), int);
  template <typename C>
  static std::false_type test(void (C::*)() const, int);
  template <typename C>
  static std::false_type test(void (C::*)() volatile, int);
  template <typename C>
  static std::false_type test(void (C::*)() const volatile, int);
  template <typename F>
  static std::false_type test(F&&, decltype(std::declval<F>()(), 0));
  static std::true_type test(...);

 public:
  using type = decltype(test(std::declval<T>(), 0));
};

template <typename T>
struct TakeArgs : public TakeArgsHelper<T>::type {};

/**
 * Encapsulate a raw pointer to be captured by a lambda without causing
 * static-analysis errors.
 */
template <typename T>
class RawPtr {
 public:
  explicit RawPtr(T* aPtr) : mPtr(aPtr) {}
  T* get() const { return mPtr; }

 private:
  T* const mPtr;
};

// A list of listeners with some helper functions. Used to batch notifications
// for a single thread/target.
template <typename Listener>
class ListenerBatch {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(ListenerBatch);

  explicit ListenerBatch(nsCOMPtr<nsIEventTarget>&& aTarget)
      : mTarget(std::move(aTarget)) {}

  bool MaybeAddListener(const RefPtr<Listener>& aListener) {
    auto target = aListener->GetTarget();
    // It does not matter what batch disconnected listeners go in.
    if (!target) {
      // It _also_ does not matter if we actually add a disconnected listener.
      // Pretend we did, but don't.
      return true;
    }
    if (target != mTarget) {
      return false;
    }
    mListeners.AppendElement(aListener);
    return true;
  }

  bool CanTakeArgs() const {
    for (auto& listener : mListeners) {
      if (listener->CanTakeArgs()) {
        return true;
      }
    }
    return false;
  }

  template <typename Storage>
  void ApplyWithArgsTuple(Storage&& aStorage) {
    std::apply(
        [&](auto&&... aArgs) mutable {
          for (auto& listener : mListeners) {
            if (listener->CanTakeArgs()) {
              listener->ApplyWithArgsImpl(
                  std::forward<decltype(aArgs)>(aArgs)...);
            } else {
              listener->ApplyWithNoArgs();
            }
          }
        },
        std::forward<Storage>(aStorage));
  }

  void ApplyWithNoArgs() {
    for (auto& listener : mListeners) {
      listener->ApplyWithNoArgs();
    }
  }

  void DispatchTask(already_AddRefed<nsIRunnable> aTask) {
    // Every listener might or might not have disconnected, so find the
    // first one that can actually perform the dispatch. If all of them are
    // disconnected, this is a no-op, which is fine.
    nsCOMPtr task = aTask;
    for (auto& listener : mListeners) {
      if (listener->TryDispatchTask(do_AddRef(task))) {
        return;
      }
    }
  }

  size_t Length() const { return mListeners.Length(); }

 private:
  ~ListenerBatch() = default;
  nsTArray<RefPtr<Listener>> mListeners;
  nsCOMPtr<nsIEventTarget> mTarget;
};

// Bottom-level base class for Listeners. Declares virtual functions that are
// always present, regardless of template parameters. This is where we handle
// the fact that different listeners have different targets, and even different
// ways of dispatching to those targets.
class ListenerBase : public RevocableToken {
 public:
  virtual bool TryDispatchTask(already_AddRefed<nsIRunnable> aTask) = 0;

  // True if the underlying listener function takes non-zero arguments.
  virtual bool CanTakeArgs() const = 0;
  // Invoke the underlying listener function. Should be called only when
  // CanTakeArgs() returns false.
  virtual void ApplyWithNoArgs() = 0;

  virtual nsCOMPtr<nsIEventTarget> GetTarget() const = 0;
};

// This is where we handle the fact that listeners will typically have
// different function signatures on their callbacks, and also the fact that
// different policies require different function signatures when perfect
// forwarding.
// We cannot simply have a single virtual ApplyWithArgs function in our
// superclass, because it is not possible to have a templated virtual function,
// and the policies all have different params that are passed (eg; NonExclusive
// passes const refs, which is not compatible with passing by rvalue ref or
// non-const lvalue ref)
template <ListenerPolicy, typename...>
class Listener;

template <typename... As>
class Listener<ListenerPolicy::Exclusive, As...> : public ListenerBase {
 public:
  virtual void ApplyWithArgsImpl(As&&... aEvents) = 0;
};

template <typename... As>
class Listener<ListenerPolicy::OneCopyPerThread, As...> : public ListenerBase {
 public:
  virtual void ApplyWithArgsImpl(As&... aEvents) = 0;
};

template <typename... As>
class Listener<ListenerPolicy::NonExclusive, As...> : public ListenerBase {
 public:
  virtual void ApplyWithArgsImpl(const As&... aEvents) = 0;
};

/**
 * Store the registered event target and function so it knows where and to
 * whom to send the event data.
 * The only way to make the existence of a virtual function override contingent
 * on a template parameter is to use template specialization. So, this
 * implements everything but ApplyWithArgs, which will be handled by yet
 * another subclass.
 */
template <ListenerPolicy Policy, typename Function, typename... As>
class ListenerImpl : public Listener<Policy, As...> {
  // Strip CV and reference from Function.
  using FunctionStorage = std::decay_t<Function>;
  using SelfType = ListenerImpl<Policy, Function, As...>;

 public:
  ListenerImpl(nsCOMPtr<nsIEventTarget>&& aTarget, Function&& aFunction)
      : mData(MakeRefPtr<Data>(std::move(aTarget),
                               std::forward<Function>(aFunction)),
              "MediaEvent ListenerImpl::mData") {}

 protected:
  virtual ~ListenerImpl() {
    MOZ_ASSERT(IsRevoked(), "Must disconnect the listener.");
  }

  nsCOMPtr<nsIEventTarget> GetTarget() const override {
    auto d = mData.Lock();
    if (d.ref()) {
      return d.ref()->mTarget;
    }
    return nullptr;
  }

  bool TryDispatchTask(already_AddRefed<nsIRunnable> aTask) override {
    nsCOMPtr task = aTask;
    RefPtr<Data> data;
    {
      auto d = mData.Lock();
      data = *d;
    }
    if (!data) {
      return false;
    }
    data->mTarget->Dispatch(task.forget());
    return true;
  }

  bool CanTakeArgs() const override { return TakeArgs<FunctionStorage>::value; }

  template <typename... Ts>
  void ApplyWithArgs(Ts&&... aEvents) {
    if constexpr (TakeArgs<Function>::value) {
      // Don't call the listener if it is disconnected.
      RefPtr<Data> data;
      {
        auto d = mData.Lock();
        data = *d;
      }
      if (!data) {
        return;
      }
      MOZ_DIAGNOSTIC_ASSERT(data->mTarget->IsOnCurrentThread());
      data->mFunction(std::forward<Ts>(aEvents)...);
    } else {
      MOZ_CRASH(
          "Don't use ApplyWithArgsImpl on listeners that don't take args! Use "
          "ApplyWithNoArgsImpl instead.");
    }
  }

  // |F| takes no arguments.
  void ApplyWithNoArgs() override {
    if constexpr (!TakeArgs<Function>::value) {
      // Don't call the listener if it is disconnected.
      RefPtr<Data> data;
      {
        auto d = mData.Lock();
        data = *d;
      }
      if (!data) {
        return;
      }
      MOZ_DIAGNOSTIC_ASSERT(data->mTarget->IsOnCurrentThread());
      data->mFunction();
    } else {
      MOZ_CRASH(
          "Don't use ApplyWithNoArgsImpl on listeners that take args! Use "
          "ApplyWithArgsImpl instead.");
    }
  }

  void Revoke() override {
    {
      auto data = mData.Lock();
      *data = nullptr;
    }
  }

  bool IsRevoked() const override {
    auto data = mData.Lock();
    return !*data;
  }

  struct RefCountedMediaEventListenerData {
    // Keep ref-counting here since Data holds a template member, leading to
    // instances of varying size, which the memory leak logging system dislikes.
    NS_INLINE_DECL_THREADSAFE_REFCOUNTING(RefCountedMediaEventListenerData)
   protected:
    virtual ~RefCountedMediaEventListenerData() = default;
  };
  struct Data : public RefCountedMediaEventListenerData {
    Data(nsCOMPtr<nsIEventTarget>&& aTarget, Function&& aFunction)
        : mTarget(std::move(aTarget)),
          mFunction(std::forward<Function>(aFunction)) {
      MOZ_DIAGNOSTIC_ASSERT(mTarget);
    }
    const nsCOMPtr<nsIEventTarget> mTarget;
    FunctionStorage mFunction;
  };

  // Storage for target and function. Also used to track revocation.
  mutable DataMutex<RefPtr<Data>> mData;
};

// We're finally at the end of the class hierarchy. The implementation code
// within ApplyWithArgs is a one-liner that is mostly identical for all
// policies; this could have all been a single template function if these
// weren't virtual functions.
template <ListenerPolicy, typename, typename...>
class ListenerImplFinal;

template <typename Function, typename... As>
class ListenerImplFinal<ListenerPolicy::Exclusive, Function, As...> final
    : public ListenerImpl<ListenerPolicy::Exclusive, Function, As...> {
 public:
  using BaseType = ListenerImpl<ListenerPolicy::Exclusive, Function, As...>;
  ListenerImplFinal(nsIEventTarget* aTarget, Function&& aFunction)
      : BaseType(aTarget, std::forward<Function>(aFunction)) {}

  void ApplyWithArgsImpl(As&&... aEvents) override {
    BaseType::ApplyWithArgs(std::move(aEvents)...);
  }
};

template <typename Function, typename... As>
class ListenerImplFinal<ListenerPolicy::OneCopyPerThread, Function, As...> final
    : public ListenerImpl<ListenerPolicy::OneCopyPerThread, Function, As...> {
 public:
  using BaseType =
      ListenerImpl<ListenerPolicy::OneCopyPerThread, Function, As...>;
  ListenerImplFinal(nsIEventTarget* aTarget, Function&& aFunction)
      : BaseType(aTarget, std::forward<Function>(aFunction)) {}

  void ApplyWithArgsImpl(As&... aEvents) override {
    BaseType::ApplyWithArgs(aEvents...);
  }
};

template <typename Function, typename... As>
class ListenerImplFinal<ListenerPolicy::NonExclusive, Function, As...> final
    : public ListenerImpl<ListenerPolicy::NonExclusive, Function, As...> {
 public:
  using BaseType = ListenerImpl<ListenerPolicy::NonExclusive, Function, As...>;
  ListenerImplFinal(nsIEventTarget* aTarget, Function&& aFunction)
      : BaseType(aTarget, std::forward<Function>(aFunction)) {}

  void ApplyWithArgsImpl(const As&... aEvents) override {
    BaseType::ApplyWithArgs(aEvents...);
  }
};

/**
 * Return true if any type is a reference type.
 */
template <typename Head, typename... Tails>
struct IsAnyReference {
  static const bool value =
      std::is_reference_v<Head> || IsAnyReference<Tails...>::value;
};

template <typename T>
struct IsAnyReference<T> {
  static const bool value = std::is_reference_v<T>;
};

}  // namespace detail

template <ListenerPolicy, typename... Ts>
class MediaEventSourceImpl;

/**
 * Not thread-safe since this is not meant to be shared and therefore only
 * move constructor is provided. Used to hold the result of
 * MediaEventSource<T>::Connect() and call Disconnect() to disconnect the
 * listener from an event source.
 */
class MediaEventListener {
  template <ListenerPolicy, typename... Ts>
  friend class MediaEventSourceImpl;

 public:
  MediaEventListener() = default;

  MediaEventListener(MediaEventListener&& aOther)
      : mToken(std::move(aOther.mToken)) {}

  MediaEventListener& operator=(MediaEventListener&& aOther) {
    MOZ_ASSERT(!mToken, "Must disconnect the listener.");
    mToken = std::move(aOther.mToken);
    return *this;
  }

  ~MediaEventListener() {
    MOZ_ASSERT(!mToken, "Must disconnect the listener.");
  }

  void Disconnect() {
    mToken->Revoke();
    mToken = nullptr;
  }

  void DisconnectIfExists() {
    if (mToken) {
      Disconnect();
    }
  }

 private:
  // Avoid exposing RevocableToken directly to the client code so that
  // listeners can be disconnected in a controlled manner.
  explicit MediaEventListener(RevocableToken* aToken) : mToken(aToken) {}
  RefPtr<RevocableToken> mToken;
};

/**
 * A generic and thread-safe class to implement the observer pattern.
 */
template <ListenerPolicy Lp, typename... Es>
class MediaEventSourceImpl {
  static_assert(!detail::IsAnyReference<Es...>::value,
                "Ref-type not supported!");

  template <typename T>
  using ArgType = typename detail::EventTypeTraits<T>::ArgType;

  using Listener = detail::Listener<Lp, ArgType<Es>...>;

  template <typename Func>
  using ListenerImpl = detail::ListenerImplFinal<Lp, Func, ArgType<Es>...>;

  using ListenerBatch = typename detail::ListenerBatch<Listener>;

  template <typename Method>
  using TakeArgs = detail::TakeArgs<Method>;

  void PruneListeners() {
    mListeners.RemoveElementsBy(
        [](const auto& listener) { return listener->IsRevoked(); });
  }

  template <typename Function>
  MediaEventListener ConnectInternal(nsIEventTarget* aTarget,
                                     Function&& aFunction) {
    MutexAutoLock lock(mMutex);
    PruneListeners();
    MOZ_ASSERT(Lp != ListenerPolicy::Exclusive || mListeners.IsEmpty());
    auto l = mListeners.AppendElement();
    *l = new ListenerImpl<Function>(aTarget, std::forward<Function>(aFunction));
    return MediaEventListener(*l);
  }

 public:
  /**
   * Register a function to receive notifications from the event source.
   *
   * @param aTarget The event target on which the function will run.
   * @param aFunction A function to be called on the target thread. The function
   *                  parameter must be convertible from |EventType|.
   * @return An object used to disconnect from the event source.
   */
  template <typename Function>
  MediaEventListener Connect(nsIEventTarget* aTarget, Function&& aFunction) {
    return ConnectInternal(aTarget, std::forward<Function>(aFunction));
  }

  /**
   * As above.
   *
   * Note we deliberately keep a weak reference to |aThis| in order not to
   * change its lifetime. This is because notifications are dispatched
   * asynchronously and removing a listener doesn't always break the reference
   * cycle for the pending event could still hold a reference to |aThis|.
   *
   * The caller must call MediaEventListener::Disconnect() to avoid dangling
   * pointers.
   */
  template <typename This, typename Method>
  MediaEventListener Connect(nsIEventTarget* aTarget, This* aThis,
                             Method aMethod) {
    if constexpr (TakeArgs<Method>::value) {
      detail::RawPtr<This> thiz(aThis);
      if constexpr (Lp == ListenerPolicy::Exclusive) {
        return ConnectInternal(aTarget, [=](ArgType<Es>&&... aEvents) {
          (thiz.get()->*aMethod)(std::move(aEvents)...);
        });
      } else if constexpr (Lp == ListenerPolicy::OneCopyPerThread) {
        return ConnectInternal(aTarget, [=](ArgType<Es>&... aEvents) {
          (thiz.get()->*aMethod)(aEvents...);
        });
      } else if constexpr (Lp == ListenerPolicy::NonExclusive) {
        return ConnectInternal(aTarget, [=](const ArgType<Es>&... aEvents) {
          (thiz.get()->*aMethod)(aEvents...);
        });
      }
    } else {
      detail::RawPtr<This> thiz(aThis);
      return ConnectInternal(aTarget, [=]() { (thiz.get()->*aMethod)(); });
    }
  }

 protected:
  MediaEventSourceImpl() : mMutex("MediaEventSourceImpl::mMutex") {}

  template <typename... Ts>
  void NotifyInternal(Ts&&... aEvents) {
    MutexAutoLock lock(mMutex);
    nsTArray<RefPtr<ListenerBatch>> listenerBatches;
    for (size_t i = 0; i < mListeners.Length();) {
      auto& l = mListeners[i];
      // Remove disconnected listeners.
      // It is not optimal but is simple and works well.
      nsCOMPtr<nsIEventTarget> target = l->GetTarget();
      if (!target) {
        mListeners.RemoveElementAt(i);
        continue;
      }

      ++i;

      // Find a batch (or create one) for this listener's target, and add the
      // listener to it.
      bool added = false;
      for (auto& batch : listenerBatches) {
        if (batch->MaybeAddListener(l)) {
          added = true;
          break;
        }
      }

      if (!added) {
        // The listener might not have a target anymore, but we still place it
        // in a Batch with the target we observed up top.
        listenerBatches.AppendElement(new ListenerBatch(nsCOMPtr(target)));
        Unused << listenerBatches.LastElement()->MaybeAddListener(l);
      }
    }

    if (listenerBatches.Length()) {
      NotificationPolicy<Lp>::DispatchNotifications(
          listenerBatches, std::forward<Ts>(aEvents)...);
    }
  }

  using Listeners = nsTArray<RefPtr<Listener>>;

  template <ListenerPolicy>
  class NotificationPolicy;

  template <>
  class NotificationPolicy<ListenerPolicy::Exclusive> {
   public:
    template <typename... Ts>
    static void DispatchNotifications(
        const nsTArray<RefPtr<ListenerBatch>>& aListenerBatches,
        Ts&&... aEvents) {
      using Storage = std::tuple<std::decay_t<Ts>...>;
      // Should we let extra argless listeners slide?
      MOZ_ASSERT(aListenerBatches.Length() == 1);
      auto& batch = aListenerBatches[0];
      if (batch->CanTakeArgs()) {
        Storage storage(std::move(aEvents)...);
        batch->DispatchTask(NS_NewRunnableFunction(
            "ListenerBatch::DispatchTask(with args)",
            [batch, storage = std::move(storage)]() mutable {
              batch->ApplyWithArgsTuple(std::move(storage));
            }));
      } else {
        batch->DispatchTask(
            NewRunnableMethod("ListenerBatch::DispatchTask(without args)",
                              batch, &ListenerBatch::ApplyWithNoArgs));
      }
    }
  };

  template <>
  class NotificationPolicy<ListenerPolicy::OneCopyPerThread> {
   public:
    template <typename... Ts>
    static void DispatchNotifications(
        const nsTArray<RefPtr<ListenerBatch>>& aListenerBatches,
        Ts&&... aEvents) {
      using Storage = std::tuple<std::decay_t<Ts>...>;

      // Find the last batch that takes args, and remember the index; that
      // batch will get the original copy (aEvents).
      Maybe<size_t> lastBatchWithArgs;
      for (size_t i = 0; i < aListenerBatches.Length(); ++i) {
        if (aListenerBatches[i]->CanTakeArgs()) {
          lastBatchWithArgs = Some(i);
        }
      }

      Storage storage(std::move(aEvents)...);
      for (size_t i = 0; i < aListenerBatches.Length(); ++i) {
        auto& batch = aListenerBatches[i];
        if (batch->CanTakeArgs()) {
          if (i != *lastBatchWithArgs) {
            // Copy |storage| into everything but the last args-taking dispatch
            batch->DispatchTask(
                NS_NewRunnableFunction("ListenerBatch::DispatchTask(with args)",
                                       [batch, storage]() mutable {
                                         batch->ApplyWithArgsTuple(storage);
                                       }));
          } else {
            // Move |storage| into the last args-taking dispatch
            batch->DispatchTask(NS_NewRunnableFunction(
                "ListenerBatch::DispatchTask(with args)",
                [batch, storage = std::move(storage)]() mutable {
                  batch->ApplyWithArgsTuple(storage);
                }));
          }
        } else {
          batch->DispatchTask(
              NewRunnableMethod("ListenerBatch::DispatchTask(without args)",
                                batch, &ListenerBatch::ApplyWithNoArgs));
        }
      }
    }
  };

  template <>
  class NotificationPolicy<ListenerPolicy::NonExclusive> {
   public:
    // This base class exists solely to keep the refcount logging code happy :(
    // It cannot handle templates inside the _THREADSAFE_REFCOUNTING macro
    // properly; it is all keyed off string matching, and that string would end
    // up being "SharedArgs<As>" verbatim, which is the same regardless of what
    // |As| is.
    class SharedArgsBase {
     public:
      NS_INLINE_DECL_THREADSAFE_REFCOUNTING(SharedArgsBase);

     protected:
      virtual ~SharedArgsBase() = default;
    };
    template <typename... As>
    class SharedArgs : public SharedArgsBase {
     public:
      using Storage = std::tuple<std::decay_t<As>...>;
      explicit SharedArgs(As&&... aArgs)
          : mStorage(std::forward<As>(aArgs)...) {}
      // We should not ever be copying this, it is refcounted
      SharedArgs(const SharedArgs& aOrig) = delete;

      void ApplyWithArgs(ListenerBatch* aBatch) {
        aBatch->ApplyWithArgsTuple(mStorage);
      }

     private:
      const Storage mStorage;
    };

    template <typename... Ts>
    static void DispatchNotifications(
        const nsTArray<RefPtr<ListenerBatch>>& aListenerBatches,
        Ts&&... aEvents) {
      // Lazy initted when we see the first args-taking batch
      RefPtr<SharedArgs<Ts...>> args;

      for (auto& batch : aListenerBatches) {
        if (batch->CanTakeArgs()) {
          if (!args) {
            // Lazy init
            args = MakeRefPtr<SharedArgs<Ts...>>(std::forward<Ts>(aEvents)...);
          }
          batch->DispatchTask(NewRunnableMethod<RefPtr<ListenerBatch>>(
              "ListenerBatch::DispatchTask(with args)", args,
              &SharedArgs<Ts...>::ApplyWithArgs, batch));
        } else {
          batch->DispatchTask(
              NewRunnableMethod("ListenerBatch::DispatchTask(without args)",
                                batch, &ListenerBatch::ApplyWithNoArgs));
        }
      }
    }
  };

 private:
  Mutex mMutex MOZ_UNANNOTATED;
  nsTArray<RefPtr<Listener>> mListeners;
};

template <typename... Es>
using MediaEventSource =
    MediaEventSourceImpl<ListenerPolicy::NonExclusive, Es...>;

template <typename... Es>
using MediaEventSourceExc =
    MediaEventSourceImpl<ListenerPolicy::Exclusive, Es...>;

template <typename... Es>
using MediaEventSourceOneCopyPerThread =
    MediaEventSourceImpl<ListenerPolicy::OneCopyPerThread, Es...>;

/**
 * A class to separate the interface of event subject (MediaEventSource)
 * and event publisher. Mostly used as a member variable to publish events
 * to the listeners.
 */
template <typename... Es>
class MediaEventProducer : public MediaEventSource<Es...> {
 public:
  template <typename... Ts>
  void Notify(Ts&&... aEvents) {
    this->NotifyInternal(std::forward<Ts>(aEvents)...);
  }
};

/**
 * Specialization for void type. A dummy bool is passed to NotifyInternal
 * since there is no way to pass a void value.
 */
template <>
class MediaEventProducer<void> : public MediaEventSource<void> {
 public:
  void Notify() { this->NotifyInternal(true /* dummy */); }
};

/**
 * A producer allowing at most one listener.
 */
template <typename... Es>
class MediaEventProducerExc : public MediaEventSourceExc<Es...> {
 public:
  template <typename... Ts>
  void Notify(Ts&&... aEvents) {
    this->NotifyInternal(std::forward<Ts>(aEvents)...);
  }
};

template <typename... Es>
class MediaEventProducerOneCopyPerThread
    : public MediaEventSourceOneCopyPerThread<Es...> {
 public:
  template <typename... Ts>
  void Notify(Ts&&... aEvents) {
    this->NotifyInternal(std::forward<Ts>(aEvents)...);
  }
};

/**
 * A class that facilitates forwarding MediaEvents from multiple sources of the
 * same type into a single source.
 *
 * Lifetimes are convenient. A forwarded source is disconnected either by
 * the source itself going away, or the forwarder being destroyed.
 *
 * Not threadsafe. The caller is responsible for calling Forward() in a
 * threadsafe manner.
 */
template <typename... Es>
class MediaEventForwarder : public MediaEventSource<Es...> {
 public:
  template <typename T>
  using ArgType = typename detail::EventTypeTraits<T>::ArgType;

  explicit MediaEventForwarder(nsCOMPtr<nsISerialEventTarget> aEventTarget)
      : mEventTarget(std::move(aEventTarget)) {}

  MediaEventForwarder(MediaEventForwarder&& aOther)
      : mEventTarget(aOther.mEventTarget),
        mListeners(std::move(aOther.mListeners)) {}

  ~MediaEventForwarder() { MOZ_ASSERT(mListeners.IsEmpty()); }

  MediaEventForwarder& operator=(MediaEventForwarder&& aOther) {
    MOZ_RELEASE_ASSERT(mEventTarget == aOther.mEventTarget);
    MOZ_ASSERT(mListeners.IsEmpty());
    mListeners = std::move(aOther.mListeners);
  }

  void Forward(MediaEventSource<Es...>& aSource) {
    // Forwarding a rawptr `this` here is fine, since DisconnectAll disconnect
    // all mListeners synchronously and prevents this handler from running.
    mListeners.AppendElement(
        aSource.Connect(mEventTarget, [this](const ArgType<Es>&... aEvents) {
          this->NotifyInternal(aEvents...);
        }));
  }

  template <typename Function>
  void ForwardIf(MediaEventSource<Es...>& aSource, Function&& aFunction) {
    // Forwarding a rawptr `this` here is fine, since DisconnectAll disconnect
    // all mListeners synchronously and prevents this handler from running.
    mListeners.AppendElement(aSource.Connect(
        mEventTarget, [this, func = aFunction](const ArgType<Es>&... aEvents) {
          if (!func()) {
            return;
          }
          this->NotifyInternal(aEvents...);
        }));
  }

  void DisconnectAll() {
    for (auto& l : mListeners) {
      l.Disconnect();
    }
    mListeners.Clear();
  }

 private:
  const nsCOMPtr<nsISerialEventTarget> mEventTarget;
  nsTArray<MediaEventListener> mListeners;
};

}  // namespace mozilla

#endif  // MediaEventSource_h_
