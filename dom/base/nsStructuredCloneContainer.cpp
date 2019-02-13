/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsStructuredCloneContainer.h"

#include "nsCOMPtr.h"
#include "nsIGlobalObject.h"
#include "nsIVariant.h"
#include "nsIXPConnect.h"
#include "nsServiceManagerUtils.h"
#include "nsContentUtils.h"
#include "jsapi.h"
#include "jsfriendapi.h"
#include "js/StructuredClone.h"
#include "xpcpublic.h"

#include "mozilla/Base64.h"
#include "mozilla/dom/ScriptSettings.h"

using namespace mozilla;

NS_IMPL_ADDREF(nsStructuredCloneContainer)
NS_IMPL_RELEASE(nsStructuredCloneContainer)

NS_INTERFACE_MAP_BEGIN(nsStructuredCloneContainer)
  NS_INTERFACE_MAP_ENTRY(nsIStructuredCloneContainer)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

nsStructuredCloneContainer::nsStructuredCloneContainer()
  : mData(nullptr), mSize(0), mVersion(0)
{
}

nsStructuredCloneContainer::~nsStructuredCloneContainer()
{
  free(mData);
}

nsresult
nsStructuredCloneContainer::InitFromJSVal(JS::Handle<JS::Value> aData,
                                          JSContext* aCx)
{
  NS_ENSURE_STATE(!mData);

  uint64_t* jsBytes = nullptr;
  bool success = JS_WriteStructuredClone(aCx, aData, &jsBytes, &mSize,
                                           nullptr, nullptr,
                                           JS::UndefinedHandleValue);
  NS_ENSURE_STATE(success);
  NS_ENSURE_STATE(jsBytes);

  // Copy jsBytes into our own buffer.
  mData = (uint64_t*) malloc(mSize);
  if (!mData) {
    mSize = 0;
    mVersion = 0;

    JS_ClearStructuredClone(jsBytes, mSize, nullptr, nullptr);
    return NS_ERROR_FAILURE;
  }
  else {
    mVersion = JS_STRUCTURED_CLONE_VERSION;
  }

  memcpy(mData, jsBytes, mSize);

  JS_ClearStructuredClone(jsBytes, mSize, nullptr, nullptr);
  return NS_OK;
}

nsresult
nsStructuredCloneContainer::InitFromBase64(const nsAString &aData,
                                           uint32_t aFormatVersion,
                                           JSContext *aCx)
{
  NS_ENSURE_STATE(!mData);

  NS_ConvertUTF16toUTF8 data(aData);

  nsAutoCString binaryData;
  nsresult rv = Base64Decode(data, binaryData);
  NS_ENSURE_SUCCESS(rv, rv);

  // Copy the string's data into our own buffer.
  mData = (uint64_t*) malloc(binaryData.Length());
  NS_ENSURE_STATE(mData);
  memcpy(mData, binaryData.get(), binaryData.Length());

  mSize = binaryData.Length();
  mVersion = aFormatVersion;
  return NS_OK;
}

nsresult
nsStructuredCloneContainer::DeserializeToJsval(JSContext* aCx,
                                               JS::MutableHandle<JS::Value> aValue)
{
  aValue.setNull();
  JS::Rooted<JS::Value> jsStateObj(aCx);
  bool hasTransferable = false;
  bool success = JS_ReadStructuredClone(aCx, mData, mSize, mVersion,
                                        &jsStateObj, nullptr, nullptr) &&
                 JS_StructuredCloneHasTransferables(mData, mSize,
                                                    &hasTransferable);
  // We want to be sure that mData doesn't contain transferable objects
  MOZ_ASSERT(!hasTransferable);
  NS_ENSURE_STATE(success && !hasTransferable);

  aValue.set(jsStateObj);
  return NS_OK;
}

nsresult
nsStructuredCloneContainer::DeserializeToVariant(JSContext *aCx,
                                                 nsIVariant **aData)
{
  NS_ENSURE_STATE(mData);
  NS_ENSURE_ARG_POINTER(aData);
  *aData = nullptr;

  // Deserialize to a JS::Value.
  JS::Rooted<JS::Value> jsStateObj(aCx);
  nsresult rv = DeserializeToJsval(aCx, &jsStateObj);
  NS_ENSURE_SUCCESS(rv, rv);

  // Now wrap the JS::Value as an nsIVariant.
  nsCOMPtr<nsIVariant> varStateObj;
  nsCOMPtr<nsIXPConnect> xpconnect = do_GetService(nsIXPConnect::GetCID());
  NS_ENSURE_STATE(xpconnect);
  xpconnect->JSValToVariant(aCx, jsStateObj, getter_AddRefs(varStateObj));
  NS_ENSURE_STATE(varStateObj);

  NS_ADDREF(*aData = varStateObj);
  return NS_OK;
}

nsresult
nsStructuredCloneContainer::GetDataAsBase64(nsAString &aOut)
{
  NS_ENSURE_STATE(mData);
  aOut.Truncate();

  nsAutoCString binaryData(reinterpret_cast<char*>(mData), mSize);
  nsAutoCString base64Data;
  nsresult rv = Base64Encode(binaryData, base64Data);
  NS_ENSURE_SUCCESS(rv, rv);

  aOut.Assign(NS_ConvertASCIItoUTF16(base64Data));
  return NS_OK;
}

nsresult
nsStructuredCloneContainer::GetSerializedNBytes(uint64_t *aSize)
{
  NS_ENSURE_STATE(mData);
  NS_ENSURE_ARG_POINTER(aSize);

  // mSize is a size_t, while aSize is a uint64_t.  We rely on an implicit cast
  // here so that we'll get a compile error if a size_t-to-uint64_t cast is
  // narrowing.
  *aSize = mSize;

  return NS_OK;
}

nsresult
nsStructuredCloneContainer::GetFormatVersion(uint32_t *aFormatVersion)
{
  NS_ENSURE_STATE(mData);
  NS_ENSURE_ARG_POINTER(aFormatVersion);
  *aFormatVersion = mVersion;
  return NS_OK;
}
