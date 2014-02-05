/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "nsEventListenerService.h"
#include "nsCOMArray.h"
#include "nsEventListenerManager.h"
#include "nsMemory.h"
#include "nsCxPusher.h"
#include "nsIXPConnect.h"
#include "nsJSUtils.h"
#include "nsEventDispatcher.h"
#include "nsIJSEventListener.h"
#ifdef MOZ_JSDEBUGGER
#include "jsdIDebuggerService.h"
#endif
#include "nsDOMClassInfoID.h"
#include "mozilla/BasicEvents.h"
#include "mozilla/Maybe.h"
#include "nsServiceManagerUtils.h"

using namespace mozilla;
using namespace mozilla::dom;
using mozilla::AutoSafeJSContext;

NS_IMPL_CYCLE_COLLECTION_1(nsEventListenerInfo, mListener)

DOMCI_DATA(EventListenerInfo, nsEventListenerInfo)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsEventListenerInfo)
  NS_INTERFACE_MAP_ENTRY(nsIEventListenerInfo)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(EventListenerInfo)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsEventListenerInfo)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsEventListenerInfo)

NS_IMETHODIMP
nsEventListenerInfo::GetType(nsAString& aType)
{
  aType = mType;
  return NS_OK;
}

NS_IMETHODIMP
nsEventListenerInfo::GetCapturing(bool* aCapturing)
{
  *aCapturing = mCapturing;
  return NS_OK;
}

NS_IMETHODIMP
nsEventListenerInfo::GetAllowsUntrusted(bool* aAllowsUntrusted)
{
  *aAllowsUntrusted = mAllowsUntrusted;
  return NS_OK;
}

NS_IMETHODIMP
nsEventListenerInfo::GetInSystemEventGroup(bool* aInSystemEventGroup)
{
  *aInSystemEventGroup = mInSystemEventGroup;
  return NS_OK;
}

NS_IMETHODIMP
nsEventListenerInfo::GetListenerObject(JSContext* aCx,
                                       JS::MutableHandle<JS::Value> aObject)
{
  mozilla::Maybe<JSAutoCompartment> ac;
  GetJSVal(aCx, ac, aObject);
  return NS_OK;
}

NS_IMPL_ISUPPORTS1(nsEventListenerService, nsIEventListenerService)

bool
nsEventListenerInfo::GetJSVal(JSContext* aCx,
                              mozilla::Maybe<JSAutoCompartment>& aAc,
                              JS::MutableHandle<JS::Value> aJSVal)
{
  aJSVal.setNull();
  nsCOMPtr<nsIXPConnectWrappedJS> wrappedJS = do_QueryInterface(mListener);
  if (wrappedJS) {
    JS::Rooted<JSObject*> object(aCx, wrappedJS->GetJSObject());
    if (!object) {
      return false;
    }
    aAc.construct(aCx, object);
    aJSVal.setObject(*object);
    return true;
  }

  nsCOMPtr<nsIJSEventListener> jsl = do_QueryInterface(mListener);
  if (jsl && jsl->GetHandler().HasEventHandler()) {
    JS::Handle<JSObject*> handler(jsl->GetHandler().Ptr()->Callable());
    if (handler) {
      aAc.construct(aCx, handler);
      aJSVal.setObject(*handler);
      return true;
    }
  }
  return false;
}

NS_IMETHODIMP
nsEventListenerInfo::ToSource(nsAString& aResult)
{
  aResult.SetIsVoid(true);

  AutoSafeJSContext cx;
  mozilla::Maybe<JSAutoCompartment> ac;
  JS::Rooted<JS::Value> v(cx);
  if (GetJSVal(cx, ac, &v)) {
    JSString* str = JS_ValueToSource(cx, v);
    if (str) {
      nsDependentJSString depStr;
      if (depStr.init(cx, str)) {
        aResult.Assign(depStr);
      }
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsEventListenerInfo::GetDebugObject(nsISupports** aRetVal)
{
  *aRetVal = nullptr;

#ifdef MOZ_JSDEBUGGER
  nsresult rv = NS_OK;
  nsCOMPtr<jsdIDebuggerService> jsd =
    do_GetService("@mozilla.org/js/jsd/debugger-service;1", &rv);
  NS_ENSURE_SUCCESS(rv, NS_OK);

  bool isOn = false;
  jsd->GetIsOn(&isOn);
  NS_ENSURE_TRUE(isOn, NS_OK);

  AutoSafeJSContext cx;
  mozilla::Maybe<JSAutoCompartment> ac;
  JS::Rooted<JS::Value> v(cx);
  if (GetJSVal(cx, ac, &v)) {
    nsCOMPtr<jsdIValue> jsdValue;
    rv = jsd->WrapValue(v, getter_AddRefs(jsdValue));
    NS_ENSURE_SUCCESS(rv, rv);
    jsdValue.forget(aRetVal);
  }
#endif

  return NS_OK;
}

NS_IMETHODIMP
nsEventListenerService::GetListenerInfoFor(nsIDOMEventTarget* aEventTarget,
                                           uint32_t* aCount,
                                           nsIEventListenerInfo*** aOutArray)
{
  NS_ENSURE_ARG_POINTER(aEventTarget);
  *aCount = 0;
  *aOutArray = nullptr;
  nsCOMArray<nsIEventListenerInfo> listenerInfos;

  nsCOMPtr<EventTarget> eventTarget = do_QueryInterface(aEventTarget);
  NS_ENSURE_TRUE(eventTarget, NS_ERROR_NO_INTERFACE);

  nsEventListenerManager* elm = eventTarget->GetExistingListenerManager();
  if (elm) {
    elm->GetListenerInfo(&listenerInfos);
  }

  int32_t count = listenerInfos.Count();
  if (count == 0) {
    return NS_OK;
  }

  *aOutArray =
    static_cast<nsIEventListenerInfo**>(
      nsMemory::Alloc(sizeof(nsIEventListenerInfo*) * count));
  NS_ENSURE_TRUE(*aOutArray, NS_ERROR_OUT_OF_MEMORY);

  for (int32_t i = 0; i < count; ++i) {
    NS_ADDREF((*aOutArray)[i] = listenerInfos[i]);
  }
  *aCount = count;
  return NS_OK;
}

NS_IMETHODIMP
nsEventListenerService::GetEventTargetChainFor(nsIDOMEventTarget* aEventTarget,
                                               uint32_t* aCount,
                                               nsIDOMEventTarget*** aOutArray)
{
  *aCount = 0;
  *aOutArray = nullptr;
  NS_ENSURE_ARG(aEventTarget);
  WidgetEvent event(true, NS_EVENT_NULL);
  nsCOMArray<EventTarget> targets;
  nsresult rv = nsEventDispatcher::Dispatch(aEventTarget, nullptr, &event,
                                            nullptr, nullptr, nullptr, &targets);
  NS_ENSURE_SUCCESS(rv, rv);
  int32_t count = targets.Count();
  if (count == 0) {
    return NS_OK;
  }

  *aOutArray =
    static_cast<nsIDOMEventTarget**>(
      nsMemory::Alloc(sizeof(nsIDOMEventTarget*) * count));
  NS_ENSURE_TRUE(*aOutArray, NS_ERROR_OUT_OF_MEMORY);

  for (int32_t i = 0; i < count; ++i) {
    NS_ADDREF((*aOutArray)[i] = targets[i]);
  }
  *aCount = count;

  return NS_OK;
}

NS_IMETHODIMP
nsEventListenerService::HasListenersFor(nsIDOMEventTarget* aEventTarget,
                                        const nsAString& aType,
                                        bool* aRetVal)
{
  nsCOMPtr<EventTarget> eventTarget = do_QueryInterface(aEventTarget);
  NS_ENSURE_TRUE(eventTarget, NS_ERROR_NO_INTERFACE);

  nsEventListenerManager* elm = eventTarget->GetExistingListenerManager();
  *aRetVal = elm && elm->HasListenersFor(aType);
  return NS_OK;
}

NS_IMETHODIMP
nsEventListenerService::AddSystemEventListener(nsIDOMEventTarget *aTarget,
                                               const nsAString& aType,
                                               nsIDOMEventListener* aListener,
                                               bool aUseCapture)
{
  NS_PRECONDITION(aTarget, "Missing target");
  NS_PRECONDITION(aListener, "Missing listener");

  nsCOMPtr<EventTarget> eventTarget = do_QueryInterface(aTarget);
  NS_ENSURE_TRUE(eventTarget, NS_ERROR_NO_INTERFACE);

  nsEventListenerManager* manager = eventTarget->GetOrCreateListenerManager();
  NS_ENSURE_STATE(manager);

  EventListenerFlags flags =
    aUseCapture ? TrustedEventsAtSystemGroupCapture() :
                  TrustedEventsAtSystemGroupBubble();
  manager->AddEventListenerByType(aListener, aType, flags);
  return NS_OK;
}

NS_IMETHODIMP
nsEventListenerService::RemoveSystemEventListener(nsIDOMEventTarget *aTarget,
                                                  const nsAString& aType,
                                                  nsIDOMEventListener* aListener,
                                                  bool aUseCapture)
{
  NS_PRECONDITION(aTarget, "Missing target");
  NS_PRECONDITION(aListener, "Missing listener");

  nsCOMPtr<EventTarget> eventTarget = do_QueryInterface(aTarget);
  NS_ENSURE_TRUE(eventTarget, NS_ERROR_NO_INTERFACE);

  nsEventListenerManager* manager = eventTarget->GetExistingListenerManager();
  if (manager) {
    EventListenerFlags flags =
      aUseCapture ? TrustedEventsAtSystemGroupCapture() :
                    TrustedEventsAtSystemGroupBubble();
    manager->RemoveEventListenerByType(aListener, aType, flags);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsEventListenerService::AddListenerForAllEvents(nsIDOMEventTarget* aTarget,
                                                nsIDOMEventListener* aListener,
                                                bool aUseCapture,
                                                bool aWantsUntrusted,
                                                bool aSystemEventGroup)
{
  NS_ENSURE_STATE(aTarget && aListener);

  nsCOMPtr<EventTarget> eventTarget = do_QueryInterface(aTarget);
  NS_ENSURE_TRUE(eventTarget, NS_ERROR_NO_INTERFACE);

  nsEventListenerManager* manager = eventTarget->GetOrCreateListenerManager();
  NS_ENSURE_STATE(manager);
  manager->AddListenerForAllEvents(aListener, aUseCapture, aWantsUntrusted,
                               aSystemEventGroup);
  return NS_OK;
}

NS_IMETHODIMP
nsEventListenerService::RemoveListenerForAllEvents(nsIDOMEventTarget* aTarget,
                                                   nsIDOMEventListener* aListener,
                                                   bool aUseCapture,
                                                   bool aSystemEventGroup)
{
  NS_ENSURE_STATE(aTarget && aListener);

  nsCOMPtr<EventTarget> eventTarget = do_QueryInterface(aTarget);
  NS_ENSURE_TRUE(eventTarget, NS_ERROR_NO_INTERFACE);

  nsEventListenerManager* manager = eventTarget->GetExistingListenerManager();
  if (manager) {
    manager->RemoveListenerForAllEvents(aListener, aUseCapture, aSystemEventGroup);
  }
  return NS_OK;
}

nsresult
NS_NewEventListenerService(nsIEventListenerService** aResult)
{
  *aResult = new nsEventListenerService();
  NS_ADDREF(*aResult);
  return NS_OK;
}
