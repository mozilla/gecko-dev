/* -*- Mode: c++; c-basic-offset: 2; tab-width: 20; indent-tabs-mode: nil; -*-
 * vim: set sw=2 ts=4 expandtab:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_widget_EventDispatcher_h
#define mozilla_widget_EventDispatcher_h

#include <objc/objc.h>
#include <CoreFoundation/CoreFoundation.h>

#include "mozilla/widget/EventDispatcherBase.h"

namespace mozilla::widget {

/**
 * EventDispatcher is the Gecko counterpart to the Swift EventDispatcher class.
 * Together, they make up a unified event bus. Events dispatched from the Swift
 * side may notify event listeners on the Gecko side, and vice versa.
 */
class EventDispatcher final : public EventDispatcherBase {
 public:
  void Attach(id aDispatcher);
  void Detach();

  bool HasEmbedderListener(const nsAString& aEvent) override;
  nsresult DispatchToEmbedder(JSContext* aCx, const nsAString& aEvent,
                              JS::Handle<JS::Value> aData,
                              nsIGeckoViewEventCallback* aCallback) override;

  static nsresult UnboxBundle(JSContext* aCx, CFDictionaryRef aData,
                              JS::MutableHandle<JS::Value> aOut);

 private:
  virtual ~EventDispatcher();

  void Shutdown();

  id mDispatcher = nullptr;
};

}  // namespace mozilla::widget

#endif  // mozilla_widget_EventDispatcher_h
