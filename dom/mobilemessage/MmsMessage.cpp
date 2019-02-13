/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MmsMessage.h"
#include "nsIDOMClassInfo.h"
#include "jsapi.h" // For OBJECT_TO_JSVAL and JS_NewDateObjectMsec
#include "jsfriendapi.h" // For js_DateGetMsecSinceEpoch
#include "nsJSUtils.h"
#include "nsContentUtils.h"
#include "nsTArrayHelpers.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/mobilemessage/Constants.h" // For MessageType
#include "mozilla/dom/mobilemessage/SmsTypes.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/dom/ToJSValue.h"
#include "mozilla/dom/ipc/BlobChild.h"
#include "mozilla/dom/ipc/BlobParent.h"

using namespace mozilla::dom::mobilemessage;

namespace mozilla {
namespace dom {

NS_INTERFACE_MAP_BEGIN(MmsMessage)
  NS_INTERFACE_MAP_ENTRY(nsIDOMMozMmsMessage)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(MozMmsMessage)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(MmsMessage)
NS_IMPL_RELEASE(MmsMessage)

MmsMessage::MmsMessage(int32_t                          aId,
                       uint64_t                         aThreadId,
                       const nsAString&                 aIccId,
                       DeliveryState                    aDelivery,
                       const nsTArray<MmsDeliveryInfo>& aDeliveryInfo,
                       const nsAString&                 aSender,
                       const nsTArray<nsString>&        aReceivers,
                       uint64_t                         aTimestamp,
                       uint64_t                         aSentTimestamp,
                       bool                             aRead,
                       const nsAString&                 aSubject,
                       const nsAString&                 aSmil,
                       const nsTArray<Attachment>&      aAttachments,
                       uint64_t                         aExpiryDate,
                       bool                             aReadReportRequested)
  : mId(aId),
    mThreadId(aThreadId),
    mIccId(aIccId),
    mDelivery(aDelivery),
    mDeliveryInfo(aDeliveryInfo),
    mSender(aSender),
    mReceivers(aReceivers),
    mTimestamp(aTimestamp),
    mSentTimestamp(aSentTimestamp),
    mRead(aRead),
    mSubject(aSubject),
    mSmil(aSmil),
    mAttachments(aAttachments),
    mExpiryDate(aExpiryDate),
    mReadReportRequested(aReadReportRequested)
{
}

MmsMessage::MmsMessage(const mobilemessage::MmsMessageData& aData)
  : mId(aData.id())
  , mThreadId(aData.threadId())
  , mIccId(aData.iccId())
  , mDelivery(aData.delivery())
  , mSender(aData.sender())
  , mReceivers(aData.receivers())
  , mTimestamp(aData.timestamp())
  , mSentTimestamp(aData.sentTimestamp())
  , mRead(aData.read())
  , mSubject(aData.subject())
  , mSmil(aData.smil())
  , mExpiryDate(aData.expiryDate())
  , mReadReportRequested(aData.readReportRequested())
{
  uint32_t len = aData.attachments().Length();
  mAttachments.SetCapacity(len);
  for (uint32_t i = 0; i < len; i++) {
    MmsAttachment att;
    const MmsAttachmentData &element = aData.attachments()[i];
    att.mId = element.id();
    att.mLocation = element.location();

    // mContent is not going to be exposed to JS directly so we can use
    // nullptr as parent.
    if (element.contentParent()) {
      nsRefPtr<BlobImpl> impl = static_cast<BlobParent*>(element.contentParent())->GetBlobImpl();
      att.mContent = Blob::Create(nullptr, impl);
    } else if (element.contentChild()) {
      nsRefPtr<BlobImpl> impl = static_cast<BlobChild*>(element.contentChild())->GetBlobImpl();
      att.mContent = Blob::Create(nullptr, impl);
    } else {
      NS_WARNING("MmsMessage: Unable to get attachment content.");
    }
    mAttachments.AppendElement(att);
  }

  len = aData.deliveryInfo().Length();
  mDeliveryInfo.SetCapacity(len);
  for (uint32_t i = 0; i < len; i++) {
    MmsDeliveryInfo info;
    const MmsDeliveryInfoData &infoData = aData.deliveryInfo()[i];

    // Prepare |info.mReceiver|.
    info.mReceiver = infoData.receiver();

    // Prepare |info.mDeliveryStatus|.
    nsString statusStr;
    switch (infoData.deliveryStatus()) {
      case eDeliveryStatus_NotApplicable:
        statusStr = DELIVERY_STATUS_NOT_APPLICABLE;
        break;
      case eDeliveryStatus_Success:
        statusStr = DELIVERY_STATUS_SUCCESS;
        break;
      case eDeliveryStatus_Pending:
        statusStr = DELIVERY_STATUS_PENDING;
        break;
      case eDeliveryStatus_Error:
        statusStr = DELIVERY_STATUS_ERROR;
        break;
      case eDeliveryStatus_Reject:
        statusStr = DELIVERY_STATUS_REJECTED;
        break;
      case eDeliveryStatus_Manual:
        statusStr = DELIVERY_STATUS_MANUAL;
        break;
      case eDeliveryStatus_EndGuard:
      default:
        MOZ_CRASH("We shouldn't get any other delivery status!");
    }
    info.mDeliveryStatus = statusStr;

    // Prepare |info.mDeliveryTimestamp|.
    info.mDeliveryTimestamp = infoData.deliveryTimestamp();

    // Prepare |info.mReadStatus|.
    nsString statusReadString;
    switch(infoData.readStatus()) {
      case eReadStatus_NotApplicable:
        statusReadString = READ_STATUS_NOT_APPLICABLE;
        break;
      case eReadStatus_Success:
        statusReadString = READ_STATUS_SUCCESS;
        break;
      case eReadStatus_Pending:
        statusReadString = READ_STATUS_PENDING;
        break;
      case eReadStatus_Error:
        statusReadString = READ_STATUS_ERROR;
        break;
      case eReadStatus_EndGuard:
      default:
        MOZ_CRASH("We shouldn't get any other read status!");
    }
    info.mReadStatus = statusReadString;

    // Prepare |info.mReadTimestamp|.
    info.mReadTimestamp = infoData.readTimestamp();

    mDeliveryInfo.AppendElement(info);
  }
}

/* static */ nsresult
MmsMessage::Create(int32_t aId,
                   uint64_t aThreadId,
                   const nsAString& aIccId,
                   const nsAString& aDelivery,
                   const JS::Value& aDeliveryInfo,
                   const nsAString& aSender,
                   const JS::Value& aReceivers,
                   uint64_t aTimestamp,
                   uint64_t aSentTimestamp,
                   bool aRead,
                   const nsAString& aSubject,
                   const nsAString& aSmil,
                   const JS::Value& aAttachments,
                   uint64_t aExpiryDate,
                   bool aIsReadReportRequested,
                   JSContext* aCx,
                   nsIDOMMozMmsMessage** aMessage)
{
  *aMessage = nullptr;

  // Set |delivery|.
  DeliveryState delivery;
  if (aDelivery.Equals(DELIVERY_SENT)) {
    delivery = eDeliveryState_Sent;
  } else if (aDelivery.Equals(DELIVERY_RECEIVED)) {
    delivery = eDeliveryState_Received;
  } else if (aDelivery.Equals(DELIVERY_SENDING)) {
    delivery = eDeliveryState_Sending;
  } else if (aDelivery.Equals(DELIVERY_NOT_DOWNLOADED)) {
    delivery = eDeliveryState_NotDownloaded;
  } else if (aDelivery.Equals(DELIVERY_ERROR)) {
    delivery = eDeliveryState_Error;
  } else {
    return NS_ERROR_INVALID_ARG;
  }

  // Set |deliveryInfo|.
  if (!aDeliveryInfo.isObject()) {
    return NS_ERROR_INVALID_ARG;
  }
  JS::Rooted<JSObject*> deliveryInfoObj(aCx, &aDeliveryInfo.toObject());
  if (!JS_IsArrayObject(aCx, deliveryInfoObj)) {
    return NS_ERROR_INVALID_ARG;
  }

  uint32_t length;
  MOZ_ALWAYS_TRUE(JS_GetArrayLength(aCx, deliveryInfoObj, &length));

  nsTArray<MmsDeliveryInfo> deliveryInfo;
  JS::Rooted<JS::Value> infoJsVal(aCx);
  for (uint32_t i = 0; i < length; ++i) {
    if (!JS_GetElement(aCx, deliveryInfoObj, i, &infoJsVal) ||
        !infoJsVal.isObject()) {
      return NS_ERROR_INVALID_ARG;
    }

    MmsDeliveryInfo info;
    if (!info.Init(aCx, infoJsVal)) {
      return NS_ERROR_TYPE_ERR;
    }

    deliveryInfo.AppendElement(info);
  }

  // Set |receivers|.
  if (!aReceivers.isObject()) {
    return NS_ERROR_INVALID_ARG;
  }
  JS::Rooted<JSObject*> receiversObj(aCx, &aReceivers.toObject());
  if (!JS_IsArrayObject(aCx, receiversObj)) {
    return NS_ERROR_INVALID_ARG;
  }

  MOZ_ALWAYS_TRUE(JS_GetArrayLength(aCx, receiversObj, &length));

  nsTArray<nsString> receivers;
  JS::Rooted<JS::Value> receiverJsVal(aCx);
  for (uint32_t i = 0; i < length; ++i) {
    if (!JS_GetElement(aCx, receiversObj, i, &receiverJsVal) ||
        !receiverJsVal.isString()) {
      return NS_ERROR_INVALID_ARG;
    }

    nsAutoJSString receiverStr;
    if (!receiverStr.init(aCx, receiverJsVal.toString())) {
      return NS_ERROR_FAILURE;
    }

    receivers.AppendElement(receiverStr);
  }

  // Set |attachments|.
  if (!aAttachments.isObject()) {
    return NS_ERROR_INVALID_ARG;
  }
  JS::Rooted<JSObject*> attachmentsObj(aCx, &aAttachments.toObject());
  if (!JS_IsArrayObject(aCx, attachmentsObj)) {
    return NS_ERROR_INVALID_ARG;
  }

  nsTArray<Attachment> attachments;
  MOZ_ALWAYS_TRUE(JS_GetArrayLength(aCx, attachmentsObj, &length));

  JS::Rooted<JS::Value> attachmentJsVal(aCx);
  for (uint32_t i = 0; i < length; ++i) {
    if (!JS_GetElement(aCx, attachmentsObj, i, &attachmentJsVal)) {
      return NS_ERROR_INVALID_ARG;
    }

    MmsAttachment attachment;
    if (!attachment.Init(aCx, attachmentJsVal)) {
      return NS_ERROR_TYPE_ERR;
    }

    attachments.AppendElement(attachment);
  }

  nsCOMPtr<nsIDOMMozMmsMessage> message = new MmsMessage(aId,
                                                         aThreadId,
                                                         aIccId,
                                                         delivery,
                                                         deliveryInfo,
                                                         aSender,
                                                         receivers,
                                                         aTimestamp,
                                                         aSentTimestamp,
                                                         aRead,
                                                         aSubject,
                                                         aSmil,
                                                         attachments,
                                                         aExpiryDate,
                                                         aIsReadReportRequested);
  message.forget(aMessage);
  return NS_OK;
}

bool
MmsMessage::GetData(ContentParent* aParent,
                    mobilemessage::MmsMessageData& aData)
{
  NS_ASSERTION(aParent, "aParent is null");

  aData.id() = mId;
  aData.threadId() = mThreadId;
  aData.iccId() = mIccId;
  aData.delivery() = mDelivery;
  aData.sender().Assign(mSender);
  aData.receivers() = mReceivers;
  aData.timestamp() = mTimestamp;
  aData.sentTimestamp() = mSentTimestamp;
  aData.read() = mRead;
  aData.subject() = mSubject;
  aData.smil() = mSmil;
  aData.expiryDate() = mExpiryDate;
  aData.readReportRequested() = mReadReportRequested;

  aData.deliveryInfo().SetCapacity(mDeliveryInfo.Length());
  for (uint32_t i = 0; i < mDeliveryInfo.Length(); i++) {
    MmsDeliveryInfoData infoData;
    const MmsDeliveryInfo &info = mDeliveryInfo[i];

    // Prepare |infoData.mReceiver|.
    infoData.receiver().Assign(info.mReceiver);

    // Prepare |infoData.mDeliveryStatus|.
    DeliveryStatus status;
    if (info.mDeliveryStatus.Equals(DELIVERY_STATUS_NOT_APPLICABLE)) {
      status = eDeliveryStatus_NotApplicable;
    } else if (info.mDeliveryStatus.Equals(DELIVERY_STATUS_SUCCESS)) {
      status = eDeliveryStatus_Success;
    } else if (info.mDeliveryStatus.Equals(DELIVERY_STATUS_PENDING)) {
      status = eDeliveryStatus_Pending;
    } else if (info.mDeliveryStatus.Equals(DELIVERY_STATUS_ERROR)) {
      status = eDeliveryStatus_Error;
    } else if (info.mDeliveryStatus.Equals(DELIVERY_STATUS_REJECTED)) {
      status = eDeliveryStatus_Reject;
    } else if (info.mDeliveryStatus.Equals(DELIVERY_STATUS_MANUAL)) {
      status = eDeliveryStatus_Manual;
    } else {
      return false;
    }
    infoData.deliveryStatus() = status;

    // Prepare |infoData.mDeliveryTimestamp|.
    infoData.deliveryTimestamp() = info.mDeliveryTimestamp;

    // Prepare |infoData.mReadStatus|.
    ReadStatus readStatus;
    if (info.mReadStatus.Equals(READ_STATUS_NOT_APPLICABLE)) {
      readStatus = eReadStatus_NotApplicable;
    } else if (info.mReadStatus.Equals(READ_STATUS_SUCCESS)) {
      readStatus = eReadStatus_Success;
    } else if (info.mReadStatus.Equals(READ_STATUS_PENDING)) {
      readStatus = eReadStatus_Pending;
    } else if (info.mReadStatus.Equals(READ_STATUS_ERROR)) {
      readStatus = eReadStatus_Error;
    } else {
      return false;
    }
    infoData.readStatus() = readStatus;

    // Prepare |infoData.mReadTimestamp|.
    infoData.readTimestamp() = info.mReadTimestamp;

    aData.deliveryInfo().AppendElement(infoData);
  }

  aData.attachments().SetCapacity(mAttachments.Length());
  for (uint32_t i = 0; i < mAttachments.Length(); i++) {
    MmsAttachmentData mma;
    const Attachment &element = mAttachments[i];
    mma.id().Assign(element.id);
    mma.location().Assign(element.location);

    // This is a workaround. Sometimes the blob we get from the database
    // doesn't have a valid last modified date, making the ContentParent
    // send a "Mystery Blob" to the ContentChild. Attempting to get the
    // last modified date of blob can force that value to be initialized.
    nsRefPtr<BlobImpl> impl = element.content->Impl();
    if (impl && impl->IsDateUnknown()) {
      ErrorResult rv;
      impl->GetLastModified(rv);
      if (rv.Failed()) {
        NS_WARNING("Failed to get last modified date!");
        rv.SuppressException();
      }
    }

    mma.contentParent() = aParent->GetOrCreateActorForBlob(element.content);
    if (!mma.contentParent()) {
      return false;
    }
    aData.attachments().AppendElement(mma);
  }

  return true;
}

NS_IMETHODIMP
MmsMessage::GetType(nsAString& aType)
{
  aType = NS_LITERAL_STRING("mms");
  return NS_OK;
}

NS_IMETHODIMP
MmsMessage::GetId(int32_t* aId)
{
  *aId = mId;
  return NS_OK;
}

NS_IMETHODIMP
MmsMessage::GetThreadId(uint64_t* aThreadId)
{
  *aThreadId = mThreadId;
  return NS_OK;
}

NS_IMETHODIMP
MmsMessage::GetIccId(nsAString& aIccId)
{
  aIccId = mIccId;
  return NS_OK;
}

NS_IMETHODIMP
MmsMessage::GetDelivery(nsAString& aDelivery)
{
  switch (mDelivery) {
    case eDeliveryState_Received:
      aDelivery = DELIVERY_RECEIVED;
      break;
    case eDeliveryState_Sending:
      aDelivery = DELIVERY_SENDING;
      break;
    case eDeliveryState_Sent:
      aDelivery = DELIVERY_SENT;
      break;
    case eDeliveryState_Error:
      aDelivery = DELIVERY_ERROR;
      break;
    case eDeliveryState_NotDownloaded:
      aDelivery = DELIVERY_NOT_DOWNLOADED;
      break;
    case eDeliveryState_Unknown:
    case eDeliveryState_EndGuard:
    default:
      MOZ_CRASH("We shouldn't get any other delivery state!");
  }

  return NS_OK;
}

NS_IMETHODIMP
MmsMessage::GetDeliveryInfo(JSContext* aCx, JS::MutableHandle<JS::Value> aDeliveryInfo)
{
  // TODO Bug 850525 It'd be better to depend on the delivery of MmsMessage
  // to return a more correct value. Ex, if .delivery = 'received', we should
  // also make .deliveryInfo = null, since the .deliveryInfo is useless.
  uint32_t length = mDeliveryInfo.Length();
  if (length == 0) {
    aDeliveryInfo.setNull();
    return NS_OK;
  }

  if (!ToJSValue(aCx, mDeliveryInfo, aDeliveryInfo)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return NS_OK;
}

NS_IMETHODIMP
MmsMessage::GetSender(nsAString& aSender)
{
  aSender = mSender;
  return NS_OK;
}

NS_IMETHODIMP
MmsMessage::GetReceivers(JSContext* aCx, JS::MutableHandle<JS::Value> aReceivers)
{
  JS::Rooted<JSObject*> receiversObj(aCx);
  nsresult rv = nsTArrayToJSArray(aCx, mReceivers, &receiversObj);
  NS_ENSURE_SUCCESS(rv, rv);

  aReceivers.setObject(*receiversObj);
  return NS_OK;
}

NS_IMETHODIMP
MmsMessage::GetTimestamp(DOMTimeStamp* aTimestamp)
{
  *aTimestamp = mTimestamp;
  return NS_OK;
}

NS_IMETHODIMP
MmsMessage::GetSentTimestamp(DOMTimeStamp* aSentTimestamp)
{
  *aSentTimestamp = mSentTimestamp;
  return NS_OK;
}

NS_IMETHODIMP
MmsMessage::GetRead(bool* aRead)
{
  *aRead = mRead;
  return NS_OK;
}

NS_IMETHODIMP
MmsMessage::GetSubject(nsAString& aSubject)
{
  aSubject = mSubject;
  return NS_OK;
}

NS_IMETHODIMP
MmsMessage::GetSmil(nsAString& aSmil)
{
  aSmil = mSmil;
  return NS_OK;
}

NS_IMETHODIMP
MmsMessage::GetAttachments(JSContext* aCx, JS::MutableHandle<JS::Value> aAttachments)
{
  uint32_t length = mAttachments.Length();

  JS::Rooted<JSObject*> attachments(
    aCx, JS_NewArrayObject(aCx, length));
  NS_ENSURE_TRUE(attachments, NS_ERROR_OUT_OF_MEMORY);

  for (uint32_t i = 0; i < length; ++i) {
    const Attachment &attachment = mAttachments[i];

    JS::Rooted<JSObject*> attachmentObj(aCx, JS_NewPlainObject(aCx));
    NS_ENSURE_TRUE(attachmentObj, NS_ERROR_OUT_OF_MEMORY);

    JS::Rooted<JSString*> tmpJsStr(aCx);

    // Get |attachment.mId|.
    tmpJsStr = JS_NewUCStringCopyN(aCx,
                                   attachment.id.get(),
                                   attachment.id.Length());
    NS_ENSURE_TRUE(tmpJsStr, NS_ERROR_OUT_OF_MEMORY);

    if (!JS_DefineProperty(aCx, attachmentObj, "id", tmpJsStr, JSPROP_ENUMERATE)) {
      return NS_ERROR_FAILURE;
    }

    // Get |attachment.mLocation|.
    tmpJsStr = JS_NewUCStringCopyN(aCx,
                                   attachment.location.get(),
                                   attachment.location.Length());
    NS_ENSURE_TRUE(tmpJsStr, NS_ERROR_OUT_OF_MEMORY);

    if (!JS_DefineProperty(aCx, attachmentObj, "location", tmpJsStr, JSPROP_ENUMERATE)) {
      return NS_ERROR_FAILURE;
    }

    // Get |attachment.mContent|.

    // Duplicating the File with the correct parent object.
    nsIGlobalObject *global = xpc::NativeGlobal(JS::CurrentGlobalOrNull(aCx));
    MOZ_ASSERT(global);
    nsRefPtr<Blob> newBlob = Blob::Create(global, attachment.content->Impl());

    JS::Rooted<JS::Value> val(aCx);
    if (!ToJSValue(aCx, newBlob, &val)) {
      return NS_ERROR_FAILURE;
    }

    if (!JS_DefineProperty(aCx, attachmentObj, "content", val, JSPROP_ENUMERATE)) {
      return NS_ERROR_FAILURE;
    }

    if (!JS_DefineElement(aCx, attachments, i, attachmentObj, JSPROP_ENUMERATE)) {
      return NS_ERROR_FAILURE;
    }
  }

  aAttachments.setObject(*attachments);
  return NS_OK;
}

NS_IMETHODIMP
MmsMessage::GetExpiryDate(DOMTimeStamp* aExpiryDate)
{
  *aExpiryDate = mExpiryDate;
  return NS_OK;
}

NS_IMETHODIMP
MmsMessage::GetReadReportRequested(bool* aReadReportRequested)
{
  *aReadReportRequested = mReadReportRequested;
  return NS_OK;
}


} // namespace dom
} // namespace mozilla
