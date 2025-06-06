/* -*- Mode: c++; c-basic-offset: 2; tab-width: 20; indent-tabs-mode: nil; -*-
 * vim: set sw=2 ts=4 expandtab:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_widget_EventDispatcher_h
#define mozilla_widget_EventDispatcher_h

#include "mozilla/java/EventDispatcherNatives.h"
#include "mozilla/widget/EventDispatcherBase.h"

namespace mozilla::widget {

/**
 * EventDispatcher is the Gecko counterpart to the Java EventDispatcher class.
 * Together, they make up a unified event bus. Events dispatched from the Java
 * side may notify event listeners on the Gecko side, and vice versa.
 */
class EventDispatcher final
    : public EventDispatcherBase,
      public java::EventDispatcher::Natives<EventDispatcher> {
  using NativesBase = java::EventDispatcher::Natives<EventDispatcher>;

 public:
  void Attach(java::EventDispatcher::Param aDispatcher);
  void Detach();

  void Activate();

  using EventDispatcherBase::HasGeckoListener;
  bool HasGeckoListener(jni::String::Param aEvent) {
    return EventDispatcherBase::HasGeckoListener(aEvent->ToString());
  }

  using EventDispatcherBase::DispatchToGecko;
  void DispatchToGecko(jni::String::Param aEvent, jni::Object::Param aData,
                       jni::Object::Param aCallback);

  bool HasEmbedderListener(const nsAString& aEvent) override;
  nsresult DispatchToEmbedder(JSContext* aCx, const nsAString& aEvent,
                              JS::Handle<JS::Value> aData,
                              nsIGeckoViewEventCallback* aCallback) override;

  static nsresult UnboxBundle(JSContext* aCx, jni::Object::Param aData,
                              JS::MutableHandle<JS::Value> aOut);

 private:
  friend class java::EventDispatcher::Natives<EventDispatcher>;

  virtual ~EventDispatcher() = default;

  void Shutdown();

  java::EventDispatcher::WeakRef mDispatcher;
};

}  // namespace mozilla::widget

#endif  // mozilla_widget_EventDispatcher_h
