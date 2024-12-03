/* -*- Mode: c++; c-basic-offset: 2; tab-width: 20; indent-tabs-mode: nil; -*-
 * vim: set sw=2 ts=4 expandtab:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_widget_EventDispatcher_h
#define mozilla_widget_EventDispatcher_h

#include <objc/objc.h>

#include "nsIGeckoViewBridge.h"
#include "nsPIDOMWindow.h"

namespace mozilla {
namespace widget {

/**
 * EventDispatcher is the Gecko counterpart to the Swift EventDispatcher class.
 * Together, they make up a unified event bus. Events dispatched from the Swift
 * side may notify event listeners on the Gecko side, and vice versa.
 */
class EventDispatcher final : public nsIGeckoViewEventDispatcher {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIGECKOVIEWEVENTDISPATCHER

  EventDispatcher() {}

  void Attach(id aDispatcher);
  void Detach();

  bool HasListener(const char16_t* aEvent);

 private:
  virtual ~EventDispatcher() {}

  void Shutdown();

  id mDispatcher = nullptr;
};

}  // namespace widget
}  // namespace mozilla

#endif  // mozilla_widget_EventDispatcher_h
