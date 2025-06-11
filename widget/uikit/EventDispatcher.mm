/* -*- Mode: c++; c-basic-offset: 2; tab-width: 20; indent-tabs-mode: nil; -*-
 * vim: set sw=2 ts=4 expandtab:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EventDispatcher.h"
#include "mozilla/widget/GeckoViewSupport.h"

@interface EventDispatcherImpl : NSObject <GeckoEventDispatcher> {
  RefPtr<mozilla::widget::EventDispatcher> mDispatcher;
}

- (id)initWithDispatcher:(mozilla::widget::EventDispatcher*)dispatcher;
@end

@implementation EventDispatcherImpl
- (id)initWithDispatcher:(mozilla::widget::EventDispatcher*)dispatcher {
  self = [super init];
  self->mDispatcher = dispatcher;
  return self;
}
@end

namespace mozilla::widget {

NS_IMPL_ISUPPORTS(EventDispatcher, nsIGeckoViewEventDispatcher)

NS_IMETHODIMP
EventDispatcher::Dispatch(JS::Handle<JS::Value> aEvent,
                          JS::Handle<JS::Value> aData,
                          nsIGeckoViewEventCallback* aCallback,
                          nsIGeckoViewEventFinalizer* aFinalizer,
                          JSContext* aCx) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
EventDispatcher::RegisterListener(nsIGeckoViewEventListener* aListener,
                                  JS::Handle<JS::Value> aEvents,
                                  JSContext* aCx) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
EventDispatcher::UnregisterListener(nsIGeckoViewEventListener* aListener,
                                    JS::Handle<JS::Value> aEvents,
                                    JSContext* aCx) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

bool EventDispatcher::HasListener(const char16_t* aEvent) { return false; }

void EventDispatcher::Attach(id aDispatcher) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aDispatcher);

  id<SwiftEventDispatcher> prevDispatcher =
      (id<SwiftEventDispatcher>)mDispatcher;
  id<SwiftEventDispatcher> newDispatcher =
      (id<SwiftEventDispatcher>)aDispatcher;

  if (prevDispatcher && prevDispatcher == newDispatcher) {
    // Nothing to do if the new dispatcher is the same.
    return;
  }

  [prevDispatcher attach:nil];
  [prevDispatcher release];

  mDispatcher = [newDispatcher retain];

  EventDispatcherImpl* proxy =
      [[EventDispatcherImpl alloc] initWithDispatcher:this];
  [newDispatcher attach:[proxy autorelease]];
}

void EventDispatcher::Shutdown() {
  if (mDispatcher) {
    [mDispatcher release];
  }
  mDispatcher = nullptr;
}

void EventDispatcher::Detach() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mDispatcher);

  // SetAttachedToGecko will call disposeNative for us later on the Gecko
  // thread to make sure all pending dispatchToGecko calls have completed.
  if (mDispatcher) {
    [(id<SwiftEventDispatcher>)mDispatcher attach:nil];
  }

  Shutdown();
}

}  // namespace mozilla::widget
