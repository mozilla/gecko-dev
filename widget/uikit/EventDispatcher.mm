/* -*- Mode: c++; c-basic-offset: 2; tab-width: 20; indent-tabs-mode: nil; -*-
 * vim: set sw=2 ts=4 expandtab:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EventDispatcher.h"

#include "CFTypeRefPtr.h"
#include "mozilla/MacStringHelpers.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/dom/TypedArray.h"
#include "mozilla/widget/GeckoViewSupport.h"

using namespace mozilla;
using namespace mozilla::widget;

namespace mozilla::widget::detail {

nsresult BoxValue(JSContext* aCx, JS::Handle<JS::Value> aData,
                  CFTypeRefPtr<CFTypeRef>& aOut);

nsresult BoxObject(JSContext* aCx, JS::Handle<JSObject*> aObj,
                   CFTypeRefPtr<CFTypeRef>& aOut) {
  JS::Rooted<JS::IdVector> ids(aCx, JS::IdVector(aCx));
  if (!JS_Enumerate(aCx, aObj, &ids)) {
    return NS_ERROR_FAILURE;
  }

  auto dict = CFTypeRefPtr<CFMutableDictionaryRef>::WrapUnderCreateRule(
      CFDictionaryCreateMutable(kCFAllocatorDefault, (CFIndex)ids.length(),
                                &kCFCopyStringDictionaryKeyCallBacks,
                                &kCFTypeDictionaryValueCallBacks));
  if (!dict) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  CFTypeRefPtr<CFTypeRef> valCF;
  JS::Rooted<JS::Value> valJS(aCx);
  for (size_t i = 0; i < ids.length(); ++i) {
    nsAutoJSString id;
    if (!id.init(aCx, ids[i])) {
      return NS_ERROR_FAILURE;
    }
    auto key = CFTypeRefPtr<CFStringRef>::WrapUnderCreateRule(
        CFStringCreateWithCharacters(kCFAllocatorDefault,
                                     (const UniChar*)id.BeginReading(),
                                     (CFIndex)id.Length()));
    if (!key) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    if (!JS_GetPropertyById(aCx, aObj, ids[i], &valJS)) {
      return NS_ERROR_FAILURE;
    }
    nsresult rv = BoxValue(aCx, valJS, valCF);
    NS_ENSURE_SUCCESS(rv, rv);
    CFDictionaryAddValue(dict.get(), key.get(), valCF ? valCF.get() : kCFNull);
  }

  // NOTE: A CFRetain() and CFRelease() could be avoided here if we could steal
  // the pointer from `dict`.
  aOut.AssignUnderGetRule(dict.get());
  return NS_OK;
}

nsresult BoxArray(JSContext* aCx, JS::Handle<JSObject*> aArray,
                  CFTypeRefPtr<CFTypeRef>& aOut) {
  uint32_t length = 0;
  if (!JS::GetArrayLength(aCx, aArray, &length)) {
    return NS_ERROR_FAILURE;
  }

  auto array =
      CFTypeRefPtr<CFMutableArrayRef>::WrapUnderCreateRule(CFArrayCreateMutable(
          kCFAllocatorDefault, (CFIndex)length, &kCFTypeArrayCallBacks));
  if (!array) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  CFTypeRefPtr<CFTypeRef> valCF;
  JS::Rooted<JS::Value> valJS(aCx);
  for (size_t i = 0; i < length; ++i) {
    if (!JS_GetElement(aCx, aArray, i, &valJS)) {
      return NS_ERROR_FAILURE;
    }
    nsresult rv = BoxValue(aCx, valJS, valCF);
    NS_ENSURE_SUCCESS(rv, rv);
    CFArrayAppendValue(array.get(), valCF.get());
  }

  // NOTE: A CFRetain() and CFRelease() could be avoided here if we could steal
  // the pointer from `array`.
  aOut.AssignUnderGetRule(array.get());
  return NS_OK;
}

nsresult BoxTypedArray(JSContext* aCx, JS::Handle<JSObject*> aTypedArray,
                       CFTypeRefPtr<CFTypeRef>& aOut) {
  dom::RootedSpiderMonkeyInterface<dom::Uint8Array> typedArray(aCx);
  if (!typedArray.Init(aTypedArray)) {
    return NS_ERROR_INVALID_ARG;
  }
  if (JS::IsArrayBufferViewShared(typedArray.Obj())) {
    return NS_ERROR_INVALID_ARG;
  }
  if (JS::IsLargeArrayBufferView(typedArray.Obj())) {
    return NS_ERROR_INVALID_ARG;
  }
  if (JS::IsResizableArrayBufferView(typedArray.Obj())) {
    return NS_ERROR_INVALID_ARG;
  }

  typedArray.ProcessData(
      [&](const Span<uint8_t>& aData, JS::AutoCheckCannotGC&&) {
        aOut.AssignUnderCreateRule(CFDataCreate(
            kCFAllocatorDefault, aData.data(), (CFIndex)aData.size()));
      });
  if (!aOut) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return NS_OK;
}

nsresult BoxValue(JSContext* aCx, JS::Handle<JS::Value> aData,
                  CFTypeRefPtr<CFTypeRef>& aOut) {
  if (aData.isNullOrUndefined()) {
    aOut = nullptr;
  } else if (aData.isBoolean()) {
    aOut.AssignUnderGetRule(aData.toBoolean() ? kCFBooleanTrue
                                              : kCFBooleanFalse);
  } else if (aData.isInt32()) {
    int32_t value = aData.toInt32();
    aOut.AssignUnderCreateRule(
        CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &value));
    return aOut ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
  } else if (aData.isNumber()) {
    double value = aData.toDouble();
    aOut.AssignUnderCreateRule(
        CFNumberCreate(kCFAllocatorDefault, kCFNumberDoubleType, &value));
    return aOut ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
  } else if (aData.isString()) {
    nsAutoJSString str;
    if (!str.init(aCx, aData)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    aOut.AssignUnderCreateRule(CFStringCreateWithCharacters(
        kCFAllocatorDefault, (const UniChar*)str.BeginReading(),
        (CFIndex)str.Length()));
    return aOut ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
  } else if (aData.isObject()) {
    JS::Rooted<JSObject*> obj(aCx, &aData.toObject());

    // Array objects
    bool isArray = false;
    if (!JS::IsArrayObject(aCx, obj, &isArray)) {
      return NS_ERROR_FAILURE;
    }
    if (isArray) {
      return BoxArray(aCx, obj, aOut);
    }

    // Typed array objects
    if (JS_IsTypedArrayObject(obj)) {
      return BoxTypedArray(aCx, obj, aOut);
    }

    // Plain JS objects
    return BoxObject(aCx, obj, aOut);
  } else {
    return NS_ERROR_INVALID_ARG;
  }
  return NS_OK;
}

nsresult BoxData(const nsAString& aEvent, JSContext* aCx,
                 JS::Handle<JS::Value> aData, CFTypeRefPtr<CFTypeRef>& aOut,
                 bool aObjectOnly) {
  nsresult rv = NS_ERROR_INVALID_ARG;

  if (!aObjectOnly) {
    rv = BoxValue(aCx, aData, aOut);
  } else if (aData.isNullOrUndefined()) {
    aOut = nil;
    rv = NS_OK;
  } else if (aData.isObject()) {
    JS::Rooted<JSObject*> obj(aCx, &aData.toObject());
    rv = BoxObject(aCx, obj, aOut);
  }

  if (rv != NS_ERROR_INVALID_ARG) {
    return rv;
  }

  NS_ConvertUTF16toUTF8 event(aEvent);
  if (JS_IsExceptionPending(aCx)) {
    JS::WarnUTF8(aCx, "Error dispatching %s", event.get());
  } else {
    JS_ReportErrorUTF8(aCx, "Invalid event data for %s", event.get());
  }
  return NS_ERROR_INVALID_ARG;
}

nsresult UnboxValue(JSContext* aCx, CFTypeRef aData,
                    JS::MutableHandle<JS::Value> aOut);

nsresult UnboxString(JSContext* aCx, CFStringRef aValue,
                     JS::MutableHandle<JS::Value> aOut) {
  CFIndex length = CFStringGetLength(aValue);

  JS::UniqueTwoByteChars chars(
      (char16_t*)JS_string_malloc(aCx, length * sizeof(char16_t)));
  if (!chars) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  CFStringGetCharacters(aValue, CFRangeMake(0, length), (UniChar*)chars.get());

  JSString* str = JS_NewUCString(aCx, std::move(chars), length);
  if (!str) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  aOut.setString(str);
  return NS_OK;
}

nsresult UnboxBundle(JSContext* aCx, CFDictionaryRef aData,
                     JS::MutableHandle<JS::Value> aOut) {
  size_t count = CFDictionaryGetCount(aData);

  AutoTArray<CFTypeRef, 10> keys;
  AutoTArray<CFTypeRef, 10> values;
  keys.SetLength(count);
  values.SetLength(count);
  CFDictionaryGetKeysAndValues(aData, keys.Elements(), values.Elements());

  JS::Rooted<JSObject*> obj(aCx, JS_NewPlainObject(aCx));
  if (!obj) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  JS::Rooted<JS::Value> key(aCx);
  JS::Rooted<JS::Value> value(aCx);
  JS::Rooted<jsid> id(aCx);
  for (size_t i = 0; i < count; ++i) {
    nsresult rv = UnboxValue(aCx, keys[i], &key);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = UnboxValue(aCx, values[i], &value);
    NS_ENSURE_SUCCESS(rv, rv);
    if (!JS_ValueToId(aCx, key, &id)) {
      return NS_ERROR_FAILURE;
    }
    if (!JS_DefinePropertyById(aCx, obj, id, value, JSPROP_ENUMERATE)) {
      return NS_ERROR_FAILURE;
    }
  }

  aOut.setObject(*obj);
  return NS_OK;
}

nsresult UnboxArray(JSContext* aCx, CFArrayRef aData,
                    JS::MutableHandle<JS::Value> aOut) {
  size_t count = CFArrayGetCount(aData);

  JS::Rooted<JSObject*> arr(aCx, JS::NewArrayObject(aCx, count));
  if (!arr) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  JS::Rooted<JS::Value> elt(aCx);
  for (size_t i = 0; i < count; ++i) {
    nsresult rv =
        UnboxValue(aCx, CFArrayGetValueAtIndex(aData, (CFIndex)i), &elt);
    NS_ENSURE_SUCCESS(rv, rv);
    if (!JS_SetElement(aCx, arr, i, elt)) {
      return NS_ERROR_FAILURE;
    }
  }

  aOut.setObject(*arr);
  return NS_OK;
}

nsresult UnboxValue(JSContext* aCx, CFTypeRef aData,
                    JS::MutableHandle<JS::Value> aOut) {
  CFTypeID typeID = aData ? CFGetTypeID(aData) : CFNullGetTypeID();

  if (typeID == CFNullGetTypeID()) {
    aOut.setNull();
  } else if (typeID == CFBooleanGetTypeID()) {
    aOut.setBoolean(CFBooleanGetValue((CFBooleanRef)aData));
  } else if (typeID == CFNumberGetTypeID()) {
    double numberValue = 0;
    CFNumberGetValue((CFNumberRef)aData, kCFNumberDoubleType, &numberValue);
    aOut.setDouble(numberValue);
  } else if (typeID == CFStringGetTypeID()) {
    return UnboxString(aCx, (CFStringRef)aData, aOut);
  } else if (typeID == CFDataGetTypeID()) {
    size_t length = CFDataGetLength((CFDataRef)aData);
    const uint8_t* data = CFDataGetBytePtr((CFDataRef)aData);

    IgnoredErrorResult error;
    dom::Uint8Array::Create(aCx, Span{data, length}, error);
    return error.StealNSResult();
  } else if (typeID == CFDictionaryGetTypeID()) {
    return UnboxBundle(aCx, (CFDictionaryRef)aData, aOut);
  } else if (typeID == CFArrayGetTypeID()) {
    return UnboxArray(aCx, (CFArrayRef)aData, aOut);
  } else {
    NS_WARNING("Invalid type");
    return NS_ERROR_INVALID_ARG;
  }
  return NS_OK;
}

nsresult UnboxData(const nsAString& aEvent, JSContext* aCx, CFTypeRef aData,
                   JS::MutableHandle<JS::Value> aOut, bool aBundleOnly) {
  MOZ_ASSERT(NS_IsMainThread());

  // NOTE: aBundleOnly is used to maintain behaviour parity with Android.
  nsresult rv = NS_ERROR_INVALID_ARG;
  if (!aBundleOnly) {
    rv = UnboxValue(aCx, aData, aOut);
  } else if (!aData || CFGetTypeID(aData) == CFNullGetTypeID()) {
    aOut.setNull();
    rv = NS_OK;
  } else if (CFGetTypeID(aData) == CFDictionaryGetTypeID()) {
    rv = UnboxBundle(aCx, (CFDictionaryRef)aData, aOut);
  }
  if (rv != NS_ERROR_INVALID_ARG) {
    return rv;
  }

  if (JS_IsExceptionPending(aCx)) {
    JS::WarnUTF8(aCx, "Error dispatching %s",
                 NS_ConvertUTF16toUTF8(aEvent).get());
  } else {
    JS_ReportErrorUTF8(aCx, "Invalid event data for %s",
                       NS_ConvertUTF16toUTF8(aEvent).get());
  }
  return NS_ERROR_INVALID_ARG;
}

}  // namespace mozilla::widget::detail

namespace {
class SwiftCallbackDelegate final : public nsIGeckoViewEventCallback {
 public:
  NS_DECL_ISUPPORTS

  explicit SwiftCallbackDelegate(id<EventCallback> aCallback)
      : mCallback(aCallback) {
    [aCallback retain];
  }

  NS_IMETHOD OnSuccess(JS::Handle<JS::Value> aData, JSContext* aCx) override {
    return Call(aCx, aData, [&](const CFTypeRefPtr<CFTypeRef>& data) {
      [mCallback sendSuccess:(id)data.get()];
    });
  }

  NS_IMETHOD OnError(JS::Handle<JS::Value> aData, JSContext* aCx) override {
    return Call(aCx, aData, [&](const CFTypeRefPtr<CFTypeRef>& data) {
      [mCallback sendError:(id)data.get()];
    });
  }

 private:
  virtual ~SwiftCallbackDelegate() { [mCallback release]; }

  template <class F>
  nsresult Call(JSContext* aCx, JS::Handle<JS::Value> aData, F aCall) {
    MOZ_ASSERT(NS_IsMainThread());

    CFTypeRefPtr<CFTypeRef> data;
    nsresult rv =
        mozilla::widget::detail::BoxData(u"callback"_ns, aCx, aData, data,
                                         /* ObjectOnly */ false);
    NS_ENSURE_SUCCESS(rv, rv);

    dom::AutoNoJSAPI nojsapi;

    aCall(data);
    return NS_OK;
  }

  id<EventCallback> mCallback;
};

NS_IMPL_ISUPPORTS(SwiftCallbackDelegate, nsIGeckoViewEventCallback)
}  // namespace

static void CallbackFromSwift(nsIGeckoViewEventCallback* aCallback,
                              CFTypeRef aData,
                              nsresult (nsIGeckoViewEventCallback::*aCall)(
                                  JS::Handle<JS::Value>, JSContext*)) {
  MOZ_ASSERT(NS_IsMainThread());

  // Use either the attached window's realm or a default realm.

  dom::AutoJSAPI jsapi;
  NS_ENSURE_TRUE_VOID(jsapi.Init(xpc::PrivilegedJunkScope()));

  JS::Rooted<JS::Value> data(jsapi.cx());
  nsresult rv = mozilla::widget::detail::UnboxData(u"callback"_ns, jsapi.cx(),
                                                   aData, &data,
                                                   /* BundleOnly */ false);
  NS_ENSURE_SUCCESS_VOID(rv);

  rv = (aCallback->*aCall)(data, jsapi.cx());
  NS_ENSURE_SUCCESS_VOID(rv);
}

// Objective-C wrapper for a nsIGeckoViewEventCallback.
@interface NativeCallbackDelegateSupport : NSObject <EventCallback> {
  nsCOMPtr<nsIGeckoViewEventCallback> mCallback;
}

- (id)initWithCallback:(nsIGeckoViewEventCallback*)callback;
- (void)sendSuccess:(id)response;
- (void)sendError:(id)response;
@end

@implementation NativeCallbackDelegateSupport
- (id)initWithCallback:(nsIGeckoViewEventCallback*)callback {
  self = [super init];
  mCallback = callback;
  return self;
}
- (void)sendSuccess:(id)response {
  AssertIsOnMainThread();
  CallbackFromSwift(mCallback, (CFTypeRef)response,
                    &nsIGeckoViewEventCallback::OnSuccess);
}
- (void)sendError:(id)response {
  AssertIsOnMainThread();
  CallbackFromSwift(mCallback, (CFTypeRef)response,
                    &nsIGeckoViewEventCallback::OnError);
}
@end

// Objective-C wrapper for an EventDispatcher.
@interface EventDispatcherImpl : NSObject <GeckoEventDispatcher> {
  RefPtr<EventDispatcher> mDispatcher;
}

- (id)initWithDispatcher:(EventDispatcher*)dispatcher;

@end

@implementation EventDispatcherImpl

- (id)initWithDispatcher:(EventDispatcher*)dispatcher {
  self = [super init];
  self->mDispatcher = dispatcher;
  return self;
}

- (void)dispatchToGecko:(NSString*)type
                message:(id)message
               callback:(id<EventCallback>)callback {
  AssertIsOnMainThread();

  nsString event;
  CopyNSStringToXPCOMString(type, event);

  nsCOMPtr<nsIGeckoViewEventCallback> geckoCb;
  if (callback) {
    geckoCb = new SwiftCallbackDelegate(callback);
  }

  dom::AutoJSAPI jsapi;
  NS_ENSURE_TRUE_VOID(jsapi.Init(xpc::PrivilegedJunkScope()));

  JS::Rooted<JS::Value> data(jsapi.cx());
  nsresult rv = mozilla::widget::detail::UnboxData(
      event, jsapi.cx(), (CFTypeRef)message, &data, /* BundleOnly */ true);
  NS_ENSURE_SUCCESS_VOID(rv);

  mDispatcher->DispatchToGecko(jsapi.cx(), event, data, geckoCb);
}

- (BOOL)hasListener:(NSString*)type {
  nsString event;
  CopyNSStringToXPCOMString(type, event);

  return mDispatcher->HasGeckoListener(event);
}

@end

namespace mozilla::widget {

bool EventDispatcher::HasEmbedderListener(const nsAString& aEvent) {
  id<SwiftEventDispatcher> dispatcher = (id<SwiftEventDispatcher>)mDispatcher;
  return [dispatcher hasListener:XPCOMStringToNSString(aEvent)];
}

nsresult EventDispatcher::DispatchToEmbedder(
    JSContext* aCx, const nsAString& aEvent, JS::Handle<JS::Value> aData,
    nsIGeckoViewEventCallback* aCallback) {
  // Convert the data payload to CoreFoundation types
  CFTypeRefPtr<CFTypeRef> data;
  nsresult rv =
      detail::BoxData(aEvent, aCx, aData, data, /* ObjectOnly */ true);
  NS_ENSURE_SUCCESS(rv, rv);

  // Wrap the callback if provided into a Swift callback.
  NativeCallbackDelegateSupport* callback = nil;
  if (aCallback) {
    callback = [[[NativeCallbackDelegateSupport alloc]
        initWithCallback:aCallback] autorelease];
  }

  // Call the swift dispatcher.
  dom::AutoNoJSAPI nojsapi;
  id<SwiftEventDispatcher> dispatcher = (id<SwiftEventDispatcher>)mDispatcher;
  [dispatcher dispatchToSwift:XPCOMStringToNSString(aEvent)
                      message:(id)data.get()
                     callback:callback];
  return NS_OK;
}

void EventDispatcher::Attach(id aDispatcher) {
  AssertIsOnMainThread();
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
  AssertIsOnMainThread();

  if (mDispatcher) {
    [mDispatcher release];
  }
  mDispatcher = nullptr;
}

void EventDispatcher::Detach() {
  AssertIsOnMainThread();
  MOZ_ASSERT(mDispatcher);

  // SetAttachedToGecko will call disposeNative for us later on the Gecko
  // thread to make sure all pending dispatchToGecko calls have completed.
  if (mDispatcher) {
    [(id<SwiftEventDispatcher>)mDispatcher attach:nil];
  }

  Shutdown();
}

EventDispatcher::~EventDispatcher() {
  if (mDispatcher) {
    [mDispatcher release];
  }
  mDispatcher = nullptr;
}

/* static */
nsresult EventDispatcher::UnboxBundle(JSContext* aCx, CFDictionaryRef aData,
                                      JS::MutableHandle<JS::Value> aOut) {
  return detail::UnboxBundle(aCx, aData, aOut);
}

}  // namespace mozilla::widget
