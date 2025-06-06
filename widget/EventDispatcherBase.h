/* -*- Mode: c++; c-basic-offset: 2; tab-width: 20; indent-tabs-mode: nil; -*-
 * vim: set sw=2 ts=4 expandtab:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_widget_EventDispatcherBase_h
#define mozilla_widget_EventDispatcherBase_h

#include "mozilla/ErrorResult.h"
#include "mozilla/EventTargetAndLockCapability.h"
#include "mozilla/Mutex.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/dom/ToJSValue.h"
#include "nsClassHashtable.h"
#include "nsIGeckoViewBridge.h"
#include "nsHashKeys.h"
#include "nsTObserverArray.h"

namespace mozilla::widget {

/**
 * EventDispatcherBase is the core Gecko implementation of the EventDispatcher
 * type in either Java or Swift. Together they make up a unified event bus.
 * Events dispatched from the embedder may notify listeners on the Gecko side
 * and vice versa.
 */
class EventDispatcherBase : public nsIGeckoViewEventDispatcher {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIGECKOVIEWEVENTDISPATCHER

  EventDispatcherBase() = default;

  bool HasGeckoListener(const nsAString& aEvent) MOZ_EXCLUDES(mLock.Lock());
  nsresult DispatchToGecko(JSContext* aCx, const nsAString& aEvent,
                           JS::Handle<JS::Value> aData,
                           nsIGeckoViewEventCallback* aCallback)
      MOZ_REQUIRES(sMainThreadCapability);
  void Shutdown() MOZ_REQUIRES(sMainThreadCapability);

  virtual bool HasEmbedderListener(const nsAString& aEvent) = 0;
  virtual nsresult DispatchToEmbedder(JSContext* aCx, const nsAString& aEvent,
                                      JS::Handle<JS::Value> aData,
                                      nsIGeckoViewEventCallback* aCallback) = 0;

  // C++-friendly nsIGeckoViewEventDispatcher::Dispatch.
  // Invokes ToJSValue on the provided argument.
  template <class T>
  nsresult Dispatch(const nsAString& aEvent, T&& aData,
                    nsIGeckoViewEventCallback* aCallback = nullptr)
      MOZ_REQUIRES(sMainThreadCapability) {
    dom::AutoJSAPI jsapi;
    NS_ENSURE_TRUE(jsapi.Init(xpc::PrivilegedJunkScope()), NS_ERROR_FAILURE);

    JS::Rooted<JS::Value> data(jsapi.cx());
    if (!dom::ToJSValue(jsapi.cx(), std::forward<T>(aData), &data)) {
      return NS_ERROR_FAILURE;
    }

    return DispatchInternal(jsapi.cx(), aEvent, data, aCallback);
  }

 protected:
  virtual ~EventDispatcherBase() = default;

 private:
  void Destroy();

  using ListenersList =
      nsAutoTObserverArray<nsCOMPtr<nsIGeckoViewEventListener>, 1>;

  // NOTE: This must be a nsClassHashtable to ensure that adding new keys to
  // mListenersMap does not cause the ListenersList instances within the array
  // to be relocated in memory.
  using ListenersMap = nsClassHashtable<nsStringHashKey, ListenersList>;

  using IterateEventsCallback = void (EventDispatcherBase::*)(
      const nsAString&, nsIGeckoViewEventListener*);

  nsresult IterateEvents(JSContext* aCx, JS::Handle<JS::Value> aEvents,
                         IterateEventsCallback aCallback,
                         nsIGeckoViewEventListener* aListener)
      MOZ_REQUIRES(sMainThreadCapability);
  void RegisterEventLocked(const nsAString&, nsIGeckoViewEventListener*)
      MOZ_REQUIRES(mLock);
  void UnregisterEventLocked(const nsAString&, nsIGeckoViewEventListener*)
      MOZ_REQUIRES(mLock);

  nsresult DispatchInternal(JSContext* aCx, const nsAString& aEvent,
                            JS::Handle<JS::Value> aData,
                            nsIGeckoViewEventCallback* aCallback = nullptr)
      MOZ_REQUIRES(sMainThreadCapability);
  nsresult DispatchToGeckoInternal(ListenersList* list, const nsAString& aEvent,
                                   JS::Handle<JS::Value> aData,
                                   nsIGeckoViewEventCallback* aCallback)
      MOZ_REQUIRES(sMainThreadCapability);

  MainThreadAndLockCapability<Mutex> mLock{
      "mozilla::widget::EventDispatcherBase"};
  ListenersMap mListenersMap MOZ_GUARDED_BY(mLock);
};

}  // namespace mozilla::widget

#endif  // mozilla_widget_EventDispatcherBase_h
