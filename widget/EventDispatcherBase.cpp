/* -*- Mode: c++; c-basic-offset: 2; tab-width: 20; indent-tabs-mode: nil; -*-
 * vim: set sw=2 ts=4 expandtab:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EventDispatcherBase.h"

#include "js/Array.h"
#include "mozilla/ProfilerMarkers.h"
#include "mozilla/dom/ScriptSettings.h"
#include "nsJSUtils.h"

namespace mozilla::widget {

namespace {
// Helper type used internally to transform a pair of
// `nsIGeckoViewEventCallback` and `nsIGeckoViewEventFinalizer` into a single
// `nsIGeckoViewEventCallback` which invokes the finalizer in its destructor.
class FinalizingCallbackDelegate final : public nsIGeckoViewEventCallback {
 public:
  NS_DECL_ISUPPORTS
  NS_FORWARD_NSIGECKOVIEWEVENTCALLBACK(mCallback->);

  FinalizingCallbackDelegate(nsIGeckoViewEventCallback* aCallback,
                             nsIGeckoViewEventFinalizer* aFinalizer)
      : mCallback(aCallback), mFinalizer(aFinalizer) {}

  nsIGeckoViewEventCallback* WrappedCallback() { return mCallback; }

 private:
  virtual ~FinalizingCallbackDelegate() {
    if (mFinalizer) {
      mFinalizer->OnFinalize();
    }
  }

  const nsCOMPtr<nsIGeckoViewEventCallback> mCallback;
  const nsCOMPtr<nsIGeckoViewEventFinalizer> mFinalizer;
};

NS_IMPL_ISUPPORTS(FinalizingCallbackDelegate, nsIGeckoViewEventCallback)
}  // namespace

// This type is threadsafe refcounted, as it could theoretically be accessed
// from off-main-thread, but must only be destroyed on the main thread (due to
// holding main-thread only references to JS objects).
NS_IMPL_ADDREF(EventDispatcherBase)
NS_IMPL_RELEASE_WITH_DESTROY(EventDispatcherBase, Destroy())
NS_IMPL_QUERY_INTERFACE(EventDispatcherBase, nsIGeckoViewEventDispatcher)

void EventDispatcherBase::Destroy() {
  NS_PROXY_DELETE_TO_EVENT_TARGET(EventDispatcherBase,
                                  GetMainThreadSerialEventTarget());
}

nsresult EventDispatcherBase::DispatchToGeckoInternal(
    ListenersList* list, const nsAString& aEvent, JS::Handle<JS::Value> aData,
    nsIGeckoViewEventCallback* aCallback) {
  mLock.NoteOnMainThread();

  dom::AutoNoJSAPI nojsapi;

  for (const auto& ent : list->ForwardRange()) {
    // NOTE: Hold a strong reference to the listener, as the observer array can
    // be mutated during this call.
    nsCOMPtr<nsIGeckoViewEventListener> listener = ent;
    nsresult rv = listener->OnEvent(aEvent, aData, aCallback);

    // Discard any errors encountered while dispatching so we don't miss
    // listeners.
    Unused << NS_WARN_IF(NS_FAILED(rv));
  }

  return NS_OK;
}

NS_IMETHODIMP
EventDispatcherBase::Dispatch(JS::Handle<JS::Value> aEvent,
                              JS::Handle<JS::Value> aData,
                              nsIGeckoViewEventCallback* aCallback,
                              nsIGeckoViewEventFinalizer* aFinalizer,
                              JSContext* aCx) {
  AssertIsOnMainThread();
  mLock.NoteOnMainThread();

  // Manually convert the event string from JS.
  // See bug 1334728 for why AString is not used here.
  if (!aEvent.isString()) {
    NS_WARNING("Invalid event name");
    return NS_ERROR_INVALID_ARG;
  }
  nsAutoJSString event;
  if (!event.init(aCx, aEvent.toString())) {
    JS_ClearPendingException(aCx);
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // If a finalizer is provided, use FinalizingCallbackDelegate to wrap the
  // type.
  nsCOMPtr<nsIGeckoViewEventCallback> callback =
      (aCallback && aFinalizer)
          ? new FinalizingCallbackDelegate(aCallback, aFinalizer)
          : aCallback;

  return DispatchInternal(aCx, event, aData, callback);
}

nsresult EventDispatcherBase::DispatchInternal(
    JSContext* aCx, const nsAString& aEvent, JS::Handle<JS::Value> aData,
    nsIGeckoViewEventCallback* aCallback) {
  mLock.NoteOnMainThread();

  // Don't need to lock here because we're on the main thread, and we can't
  // race against Register/UnregisterListener.

  if (ListenersList* list = mListenersMap.Get(aEvent)) {
    return DispatchToGeckoInternal(list, aEvent, aData, aCallback);
  }

  return DispatchToEmbedder(aCx, aEvent, aData, aCallback);
}

// Given a JS value which is either a string or an array of strings, call the
// given `aCallback` method for each string with the mutex held.
nsresult EventDispatcherBase::IterateEvents(
    JSContext* aCx, JS::Handle<JS::Value> aEvents,
    IterateEventsCallback aCallback, nsIGeckoViewEventListener* aListener) {
  MutexAutoLock lock(mLock.Lock());
  mLock.NoteExclusiveAccess();

  auto processEvent = [&](JS::Handle<JS::Value> event) -> nsresult {
    nsAutoJSString str;
    if (!str.init(aCx, event.toString())) {
      JS_ClearPendingException(aCx);
      return NS_ERROR_OUT_OF_MEMORY;
    }
    (this->*aCallback)(str, aListener);
    return NS_OK;
  };

  // NOTE: This does manual jsapi processing, rather than using something like
  // WebIDL for simplicity for historical reasons.
  // It may be related to wanting to avoid invalid values being passed in and
  // coerced to strings.
  if (aEvents.isString()) {
    return processEvent(aEvents);
  }

  bool isArray = false;
  NS_ENSURE_TRUE(aEvents.isObject(), NS_ERROR_INVALID_ARG);
  if (!JS::IsArrayObject(aCx, aEvents, &isArray)) {
    JS_ClearPendingException(aCx);
    return NS_ERROR_INVALID_ARG;
  }
  NS_ENSURE_TRUE(isArray, NS_ERROR_INVALID_ARG);

  JS::Rooted<JSObject*> events(aCx, &aEvents.toObject());
  uint32_t length = 0;
  if (!JS::GetArrayLength(aCx, events, &length)) {
    JS_ClearPendingException(aCx);
    return NS_ERROR_INVALID_ARG;
  }
  NS_ENSURE_TRUE(length, NS_ERROR_INVALID_ARG);

  for (size_t i = 0; i < length; i++) {
    JS::Rooted<JS::Value> event(aCx);
    if (!JS_GetElement(aCx, events, i, &event)) {
      JS_ClearPendingException(aCx);
      return NS_ERROR_INVALID_ARG;
    }
    NS_ENSURE_TRUE(event.isString(), NS_ERROR_INVALID_ARG);

    nsresult rv = processEvent(event);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  return NS_OK;
}

void EventDispatcherBase::RegisterEventLocked(
    const nsAString& aEvent, nsIGeckoViewEventListener* aListener) {
  ListenersList* list = mListenersMap.GetOrInsertNew(aEvent);

  // NOTE: Previously this code would return an error if the entry already
  // existed, but only in debug builds. This has been upgraded to a debug
  // assert, making the code infallible for more consistency between debug &
  // release builds in terms of runtime behaviour.
  if (NS_WARN_IF(list->Contains(aListener))) {
    MOZ_ASSERT_UNREACHABLE("Attempt to register the same listener twice");
    return;
  }

  list->AppendElement(aListener);
}

NS_IMETHODIMP
EventDispatcherBase::RegisterListener(nsIGeckoViewEventListener* aListener,
                                      JS::Handle<JS::Value> aEvents,
                                      JSContext* aCx) {
  AssertIsOnMainThread();
  return IterateEvents(aCx, aEvents, &EventDispatcherBase::RegisterEventLocked,
                       aListener);
}

void EventDispatcherBase::UnregisterEventLocked(
    const nsAString& aEvent, nsIGeckoViewEventListener* aListener) {
  // NOTE: Previously this code would return an error if the entry didn't exist
  // but only in debug builds. This has been upgraded to a debug assert, making
  // the code infallible for more consistency between debug & release builds in
  // terms of runtime behaviour.
  ListenersList* list = mListenersMap.Get(aEvent);
  MOZ_ASSERT(list);
  NS_ENSURE_TRUE_VOID(list);

  DebugOnly<bool> found = list->RemoveElement(aListener);
  MOZ_ASSERT(found);

  // NOTE: We intentionally do not remove the entry from `mListenersMap` here,
  // as other code higher up the stack could be holding a reference to this
  // nsTObserverArray through an iterator.
}

NS_IMETHODIMP
EventDispatcherBase::UnregisterListener(nsIGeckoViewEventListener* aListener,
                                        JS::Handle<JS::Value> aEvents,
                                        JSContext* aCx) {
  AssertIsOnMainThread();
  return IterateEvents(aCx, aEvents,
                       &EventDispatcherBase::UnregisterEventLocked, aListener);
}

bool EventDispatcherBase::HasGeckoListener(const nsAString& aEvent) {
  // NOTE: This can be called on any thread, so must hold the mutex.
  MutexAutoLock lock(mLock.Lock());
  mLock.NoteLockHeld();

  ListenersList* list = mListenersMap.Get(aEvent);
  return list && !list->IsEmpty();
}

nsresult EventDispatcherBase::DispatchToGecko(
    JSContext* aCx, const nsAString& aEvent, JS::Handle<JS::Value> aData,
    nsIGeckoViewEventCallback* aCallback) {
  mLock.NoteOnMainThread();

  // If there are no Gecko listeners for this event, abort early.
  ListenersList* list = mListenersMap.Get(aEvent);
  if (!list || list->IsEmpty()) {
    return NS_OK;
  }

  AUTO_PROFILER_MARKER_TEXT("DispatchToGecko", OTHER, {}, aEvent);

  // Actually call the Gecko listeners.
  return DispatchToGeckoInternal(list, aEvent, aData, aCallback);
}

void EventDispatcherBase::Shutdown() {
  ListenersMap listeners;
  {
    MutexAutoLock lock(mLock.Lock());
    mLock.NoteExclusiveAccess();

    // Ensure listeners are dropped while the lock isn't held.
    listeners = std::move(mListenersMap);
    mListenersMap.Clear();
  }
}

}  // namespace mozilla::widget
