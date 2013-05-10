/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MobileMessageThread.h"
#include "nsIDOMClassInfo.h"
#include "jsapi.h"           // For OBJECT_TO_JSVAL and JS_NewDateObjectMsec
#include "jsfriendapi.h"     // For js_DateGetMsecSinceEpoch
#include "nsJSUtils.h"       // For nsDependentJSString
#include "nsContentUtils.h"  // For nsTArrayHelpers.h
#include "nsTArrayHelpers.h" // For nsTArrayToJSArray
#include "Constants.h"       // For MessageType

using namespace mozilla::dom::mobilemessage;

namespace {

// Bug 819791 is not uplifted in b2g18 branch.
inline nsresult
InfallibleTArrayToJSArray(JSContext* aCx,
                          const InfallibleTArray<nsString>& aSourceArray,
                          JSObject** aResultArray)
{
  MOZ_ASSERT(aCx);
  JSAutoRequest ar(aCx);

  JSObject* arrayObj = JS_NewArrayObject(aCx, aSourceArray.Length(), nullptr);
  if (!arrayObj) {
    NS_WARNING("JS_NewArrayObject failed!");
    return NS_ERROR_OUT_OF_MEMORY;
  }

  for (uint32_t index = 0; index < aSourceArray.Length(); index++) {
    JSString* s = JS_NewUCStringCopyN(aCx, aSourceArray[index].BeginReading(),
                                      aSourceArray[index].Length());

    if(!s) {
      NS_WARNING("Memory allocation error!");
      return NS_ERROR_OUT_OF_MEMORY;
    }

    jsval wrappedVal = STRING_TO_JSVAL(s);

    if (!JS_SetElement(aCx, arrayObj, index, &wrappedVal)) {
      NS_WARNING("JS_SetElement failed!");
      return NS_ERROR_FAILURE;
    }
  }

  if (!JS_FreezeObject(aCx, arrayObj)) {
    NS_WARNING("JS_FreezeObject failed!");
    return NS_ERROR_FAILURE;
  }

  *aResultArray = arrayObj;
  return NS_OK;
}

} // anonymous namespace

DOMCI_DATA(MozMobileMessageThread, mozilla::dom::MobileMessageThread)

namespace mozilla {
namespace dom {

NS_INTERFACE_MAP_BEGIN(MobileMessageThread)
  NS_INTERFACE_MAP_ENTRY(nsIDOMMozMobileMessageThread)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(MozMobileMessageThread)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(MobileMessageThread)
NS_IMPL_RELEASE(MobileMessageThread)

/* static */ nsresult
MobileMessageThread::Create(const uint64_t aId,
                            const JS::Value& aParticipants,
                            const JS::Value& aTimestamp,
                            const nsAString& aBody,
                            const uint64_t aUnreadCount,
                            const nsAString& aLastMessageType,
                            JSContext* aCx,
                            nsIDOMMozMobileMessageThread** aThread)
{
  *aThread = nullptr;

  // ThreadData exposes these as references, so we can simply assign
  // to them.
  ThreadData data;
  data.id() = aId;
  data.body().Assign(aBody);
  data.unreadCount() = aUnreadCount;

  // Participants.
  {
    if (!aParticipants.isObject()) {
      return NS_ERROR_INVALID_ARG;
    }

    JSObject* obj = &aParticipants.toObject();
    if (!JS_IsArrayObject(aCx, obj)) {
      return NS_ERROR_INVALID_ARG;
    }

    uint32_t length;
    JS_ALWAYS_TRUE(JS_GetArrayLength(aCx, obj, &length));
    NS_ENSURE_TRUE(length, NS_ERROR_INVALID_ARG);

    for (uint32_t i = 0; i < length; ++i) {
      JS::Value val;

      if (!JS_GetElement(aCx, obj, i, &val) || !val.isString()) {
        return NS_ERROR_INVALID_ARG;
      }

      nsDependentJSString str;
      str.init(aCx, val.toString());
      data.participants().AppendElement(str);
    }
  }

  // We support both a Date object and a millisecond timestamp as a number.
  if (aTimestamp.isObject()) {
    JSObject& obj = aTimestamp.toObject();
    if (!JS_ObjectIsDate(aCx, &obj)) {
      return NS_ERROR_INVALID_ARG;
    }
    data.timestamp() = js_DateGetMsecSinceEpoch(&obj);
  } else {
    if (!aTimestamp.isNumber()) {
      return NS_ERROR_INVALID_ARG;
    }
    double number = aTimestamp.toNumber();
    if (static_cast<uint64_t>(number) != number) {
      return NS_ERROR_INVALID_ARG;
    }
    data.timestamp() = static_cast<uint64_t>(number);
  }

  // Set |aLastMessageType|.
  {
    MessageType lastMessageType;
    if (aLastMessageType.Equals(MESSAGE_TYPE_SMS)) {
      lastMessageType = eMessageType_SMS;
    } else if (aLastMessageType.Equals(MESSAGE_TYPE_MMS)) {
      lastMessageType = eMessageType_MMS;
    } else {
      return NS_ERROR_INVALID_ARG;
    }
    data.lastMessageType() = lastMessageType;
  }

  nsCOMPtr<nsIDOMMozMobileMessageThread> thread = new MobileMessageThread(data);
  thread.forget(aThread);
  return NS_OK;
}

MobileMessageThread::MobileMessageThread(const ThreadData& aData)
  : mData(aData)
{
  MOZ_ASSERT(aData.participants().Length());
}

NS_IMETHODIMP
MobileMessageThread::GetId(uint64_t* aId)
{
  *aId = mData.id();
  return NS_OK;
}

NS_IMETHODIMP
MobileMessageThread::GetBody(nsAString& aBody)
{
  aBody = mData.body();
  return NS_OK;
}

NS_IMETHODIMP
MobileMessageThread::GetUnreadCount(uint64_t* aUnreadCount)
{
  *aUnreadCount = mData.unreadCount();
  return NS_OK;
}

NS_IMETHODIMP
MobileMessageThread::GetParticipants(JSContext* aCx,
                                     JS::Value* aParticipants)
{
  JSObject* obj;

  nsresult rv = InfallibleTArrayToJSArray(aCx, mData.participants(), &obj);
  NS_ENSURE_SUCCESS(rv, rv);

  aParticipants->setObject(*obj);
  return NS_OK;
}

NS_IMETHODIMP
MobileMessageThread::GetTimestamp(JSContext* aCx,
                                  JS::Value* aDate)
{
  JSObject *obj = JS_NewDateObjectMsec(aCx, mData.timestamp());
  NS_ENSURE_TRUE(obj, NS_ERROR_FAILURE);

  *aDate = OBJECT_TO_JSVAL(obj);
  return NS_OK;
}

NS_IMETHODIMP
MobileMessageThread::GetLastMessageType(nsAString& aLastMessageType)
{
  switch (mData.lastMessageType()) {
    case eMessageType_SMS:
      aLastMessageType = MESSAGE_TYPE_SMS;
      break;
    case eMessageType_MMS:
      aLastMessageType = MESSAGE_TYPE_MMS;
      break;
    case eMessageType_EndGuard:
    default:
      MOZ_NOT_REACHED("We shouldn't get any other message type!");
      return NS_ERROR_UNEXPECTED;
  }

  return NS_OK;
}

} // namespace dom
} // namespace mozilla
