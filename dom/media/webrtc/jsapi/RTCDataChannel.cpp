/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RTCDataChannel.h"

#include "base/basictypes.h"
#include "mozilla/Logging.h"

#include "RTCDataChannelDeclarations.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/EventListenerManager.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/MessageEvent.h"
#include "mozilla/dom/MessageEventBinding.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/dom/ToJSValue.h"
#include "mozilla/dom/TypedArray.h"
#include "mozilla/dom/Blob.h"

#include "nsError.h"
#include "nsContentUtils.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIScriptContext.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsProxyRelease.h"

#include "DataChannel.h"
#include "DataChannelLog.h"

// Since we've moved the windows.h include down here, we have to explicitly
// undef GetBinaryType, otherwise we'll get really odd conflicts
#ifdef GetBinaryType
#  undef GetBinaryType
#endif

namespace mozilla {
namespace dom {

RTCDataChannel::~RTCDataChannel() {
  // Don't call us anymore!  Likely isn't an issue (or maybe just less of
  // one) once we block GC until all the (appropriate) onXxxx handlers
  // are dropped. (See WebRTC spec)
  DC_DEBUG(("%p: Close()ing %p", this, mDataChannel.get()));
  mDataChannel->SetListener(nullptr);
  mDataChannel->Close();
}

/* virtual */
JSObject* RTCDataChannel::WrapObject(JSContext* aCx,
                                     JS::Handle<JSObject*> aGivenProto) {
  return RTCDataChannel_Binding::Wrap(aCx, this, aGivenProto);
}

NS_IMPL_CYCLE_COLLECTION_CLASS(RTCDataChannel)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(RTCDataChannel,
                                                  DOMEventTargetHelper)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(RTCDataChannel,
                                                DOMEventTargetHelper)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_ADDREF_INHERITED(RTCDataChannel, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(RTCDataChannel, DOMEventTargetHelper)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(RTCDataChannel)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

RTCDataChannel::RTCDataChannel(const nsACString& aLabel, bool aOrdered,
                               Nullable<uint16_t> aMaxLifeTime,
                               Nullable<uint16_t> aMaxRetransmits,
                               const nsACString& aProtocol, bool aNegotiated,
                               already_AddRefed<DataChannel>& aDataChannel,
                               nsPIDOMWindowInner* aWindow)
    : DOMEventTargetHelper(aWindow),
      mDataChannel(aDataChannel),
      mBinaryType(DC_BINARY_TYPE_BLOB),
      mCheckMustKeepAlive(true),
      mSentClose(false),
      mLabel(aLabel),
      mOrdered(aOrdered),
      mMaxPacketLifeTime(aMaxLifeTime),
      mMaxRetransmits(aMaxRetransmits),
      mProtocol(aProtocol),
      mNegotiated(aNegotiated) {}

nsresult RTCDataChannel::Init(nsPIDOMWindowInner* aDOMWindow) {
  nsresult rv;
  nsAutoString urlParam;

  MOZ_ASSERT(mDataChannel);
  mDataChannel->SetListener(this);

  // Now grovel through the objects to get a usable origin for onMessage
  nsCOMPtr<nsIScriptGlobalObject> sgo = do_QueryInterface(aDOMWindow);
  NS_ENSURE_STATE(sgo);
  nsCOMPtr<nsIScriptContext> scriptContext = sgo->GetContext();
  NS_ENSURE_STATE(scriptContext);

  nsCOMPtr<nsIScriptObjectPrincipal> scriptPrincipal(
      do_QueryInterface(aDOMWindow));
  NS_ENSURE_STATE(scriptPrincipal);
  nsCOMPtr<nsIPrincipal> principal = scriptPrincipal->GetPrincipal();
  NS_ENSURE_STATE(principal);

  // Attempt to kill "ghost" DataChannel (if one can happen): but usually too
  // early for check to fail
  rv = CheckCurrentGlobalCorrectness();
  NS_ENSURE_SUCCESS(rv, rv);

  rv = nsContentUtils::GetWebExposedOriginSerialization(principal, mOrigin);
  DC_DEBUG(("%s: origin = %s\n", __FUNCTION__,
            NS_LossyConvertUTF16toASCII(mOrigin).get()));
  return rv;
}

// Most of the GetFoo()/SetFoo()s don't need to touch shared resources and
// are safe after Close()
void RTCDataChannel::GetLabel(nsACString& aLabel) const { aLabel = mLabel; }

void RTCDataChannel::GetProtocol(nsACString& aProtocol) const {
  aProtocol = mProtocol;
}

Nullable<uint16_t> RTCDataChannel::GetId() const {
  Nullable<uint16_t> result = mDataChannel->GetStream();
  if (result.Value() == 65535) {
    result.SetNull();
  }
  return result;
}

Nullable<uint16_t> RTCDataChannel::GetMaxPacketLifeTime() const {
  return mMaxPacketLifeTime;
}

Nullable<uint16_t> RTCDataChannel::GetMaxRetransmits() const {
  return mMaxRetransmits;
}

bool RTCDataChannel::Negotiated() const { return mNegotiated; }

bool RTCDataChannel::Ordered() const { return mOrdered; }

RTCDataChannelState RTCDataChannel::ReadyState() const {
  return static_cast<RTCDataChannelState>(mDataChannel->GetReadyState());
}

uint32_t RTCDataChannel::BufferedAmount() const {
  if (!mSentClose) {
    return mDataChannel->GetBufferedAmount();
  }
  return 0;
}

uint32_t RTCDataChannel::BufferedAmountLowThreshold() const {
  return mDataChannel->GetBufferedAmountLowThreshold();
}

void RTCDataChannel::SetBufferedAmountLowThreshold(uint32_t aThreshold) {
  mDataChannel->SetBufferedAmountLowThreshold(aThreshold);
}

void RTCDataChannel::Close() {
  mDataChannel->Close();
  UpdateMustKeepAlive();
}

// All of the following is copy/pasted from WebSocket.cpp.
void RTCDataChannel::Send(const nsAString& aData, ErrorResult& aRv) {
  if (!CheckReadyState(aRv)) {
    return;
  }

  nsCString msgString;
  if (!AppendUTF16toUTF8(aData, msgString, fallible_t())) {
    aRv.Throw(NS_ERROR_FILE_TOO_BIG);
    return;
  }

  mDataChannel->SendMsg(std::move(msgString), aRv);
}

void RTCDataChannel::Send(Blob& aData, ErrorResult& aRv) {
  MOZ_ASSERT(NS_IsMainThread(), "Not running on main thread");

  if (!CheckReadyState(aRv)) {
    return;
  }

  nsCOMPtr<nsIInputStream> msgStream;
  aData.CreateInputStream(getter_AddRefs(msgStream), aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  uint64_t msgLength = aData.GetSize(aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  if (msgLength > UINT32_MAX) {
    aRv.Throw(NS_ERROR_FILE_TOO_BIG);
    return;
  }

  mDataChannel->SendBinaryBlob(aData, aRv);
}

void RTCDataChannel::Send(const ArrayBuffer& aData, ErrorResult& aRv) {
  MOZ_ASSERT(NS_IsMainThread(), "Not running on main thread");

  if (!CheckReadyState(aRv)) {
    return;
  }

  nsCString msgString;
  if (!aData.AppendDataTo(msgString)) {
    aRv.Throw(NS_ERROR_FILE_TOO_BIG);
    return;
  }

  mDataChannel->SendBinaryMsg(std::move(msgString), aRv);
}

void RTCDataChannel::Send(const ArrayBufferView& aData, ErrorResult& aRv) {
  MOZ_ASSERT(NS_IsMainThread(), "Not running on main thread");

  if (!CheckReadyState(aRv)) {
    return;
  }

  nsCString msgString;
  if (!aData.AppendDataTo(msgString)) {
    aRv.Throw(NS_ERROR_FILE_TOO_BIG);
    return;
  }

  mDataChannel->SendBinaryMsg(std::move(msgString), aRv);
}

bool RTCDataChannel::CheckReadyState(ErrorResult& aRv) {
  MOZ_ASSERT(NS_IsMainThread());
  DataChannelState state = DataChannelState::Closed;
  if (!mSentClose) {
    state = mDataChannel->GetReadyState();
  }

  // In reality, the DataChannel protocol allows this, but we want it to
  // look like WebSockets
  if (state == DataChannelState::Connecting) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return false;
  }

  if (state == DataChannelState::Closing || state == DataChannelState::Closed) {
    return false;
  }

  MOZ_ASSERT(state == DataChannelState::Open,
             "Unknown state in RTCDataChannel::Send");

  return true;
}

nsresult RTCDataChannel::DoOnMessageAvailable(const nsACString& aData,
                                              bool aBinary) {
  MOZ_ASSERT(NS_IsMainThread());

  DC_VERBOSE((
      "DoOnMessageAvailable%s\n",
      aBinary ? ((mBinaryType == DC_BINARY_TYPE_BLOB) ? " (blob)" : " (binary)")
              : ""));

  nsresult rv = CheckCurrentGlobalCorrectness();
  if (NS_FAILED(rv)) {
    return NS_OK;
  }

  AutoJSAPI jsapi;
  if (NS_WARN_IF(!jsapi.Init(GetOwnerWindow()))) {
    return NS_ERROR_FAILURE;
  }
  JSContext* cx = jsapi.cx();

  JS::Rooted<JS::Value> jsData(cx);

  if (aBinary) {
    if (mBinaryType == DC_BINARY_TYPE_BLOB) {
      RefPtr<Blob> blob =
          Blob::CreateStringBlob(GetOwnerGlobal(), aData, u""_ns);
      if (NS_WARN_IF(!blob)) {
        return NS_ERROR_FAILURE;
      }

      if (!ToJSValue(cx, blob, &jsData)) {
        return NS_ERROR_FAILURE;
      }
    } else if (mBinaryType == DC_BINARY_TYPE_ARRAYBUFFER) {
      ErrorResult error;
      JS::Rooted<JSObject*> arrayBuf(cx, ArrayBuffer::Create(cx, aData, error));
      RETURN_NSRESULT_ON_FAILURE(error);
      jsData.setObject(*arrayBuf);
    } else {
      MOZ_CRASH("Unknown binary type!");
      return NS_ERROR_UNEXPECTED;
    }
  } else {
    NS_ConvertUTF8toUTF16 utf16data(aData);
    JSString* jsString =
        JS_NewUCStringCopyN(cx, utf16data.get(), utf16data.Length());
    NS_ENSURE_TRUE(jsString, NS_ERROR_FAILURE);

    jsData.setString(jsString);
  }

  RefPtr<MessageEvent> event = new MessageEvent(this, nullptr, nullptr);

  event->InitMessageEvent(nullptr, u"message"_ns, CanBubble::eNo,
                          Cancelable::eNo, jsData, mOrigin, u""_ns, nullptr,
                          Sequence<OwningNonNull<MessagePort>>());
  event->SetTrusted(true);

  DC_DEBUG(
      ("%p(%p): %s - Dispatching\n", this, (void*)mDataChannel, __FUNCTION__));
  ErrorResult err;
  DispatchEvent(*event, err);
  if (err.Failed()) {
    DC_ERROR(("%p(%p): %s - Failed to dispatch message", this,
              (void*)mDataChannel, __FUNCTION__));
    NS_WARNING("Failed to dispatch the message event!!!");
  }
  return err.StealNSResult();
}

nsresult RTCDataChannel::OnMessageAvailable(const nsACString& aMessage) {
  MOZ_ASSERT(NS_IsMainThread());
  return DoOnMessageAvailable(aMessage, false);
}

nsresult RTCDataChannel::OnBinaryMessageAvailable(const nsACString& aMessage) {
  MOZ_ASSERT(NS_IsMainThread());
  return DoOnMessageAvailable(aMessage, true);
}

nsresult RTCDataChannel::OnSimpleEvent(const nsAString& aName) {
  MOZ_ASSERT(NS_IsMainThread());

  nsresult rv = CheckCurrentGlobalCorrectness();
  if (NS_FAILED(rv)) {
    return NS_OK;
  }

  RefPtr<Event> event = NS_NewDOMEvent(this, nullptr, nullptr);

  event->InitEvent(aName, CanBubble::eNo, Cancelable::eNo);
  event->SetTrusted(true);

  ErrorResult err;
  DispatchEvent(*event, err);
  return err.StealNSResult();
}

nsresult RTCDataChannel::OnChannelConnected() {
  DC_DEBUG(
      ("%p(%p): %s - Dispatching\n", this, (void*)mDataChannel, __FUNCTION__));

  return OnSimpleEvent(u"open"_ns);
}

nsresult RTCDataChannel::OnChannelClosed() {
  nsresult rv;
  // so we don't have to worry if we're notified from different paths in
  // the underlying code
  if (!mSentClose) {
    // Ok, we're done with it.
    mDataChannel->ReleaseConnection();
    DC_DEBUG(("%p(%p): %s - Dispatching\n", this, (void*)mDataChannel,
              __FUNCTION__));

    rv = OnSimpleEvent(u"close"_ns);
    // no more events can happen
    mSentClose = true;
  } else {
    rv = NS_OK;
  }
  DontKeepAliveAnyMore();
  return rv;
}

nsresult RTCDataChannel::OnBufferLow() {
  DC_DEBUG(
      ("%p(%p): %s - Dispatching\n", this, (void*)mDataChannel, __FUNCTION__));

  return OnSimpleEvent(u"bufferedamountlow"_ns);
}

nsresult RTCDataChannel::NotBuffered() {
  // In the rare case that we held off GC to let the buffer drain
  UpdateMustKeepAlive();
  return NS_OK;
}

//-----------------------------------------------------------------------------
// Methods that keep alive the DataChannel object when:
//   1. the object has registered event listeners that can be triggered
//      ("strong event listeners");
//   2. there are outgoing not sent messages.
//-----------------------------------------------------------------------------

void RTCDataChannel::UpdateMustKeepAlive() {
  MOZ_ASSERT(NS_IsMainThread());

  if (!mCheckMustKeepAlive) {
    return;
  }

  bool shouldKeepAlive = false;
  DataChannelState readyState = mDataChannel->GetReadyState();

  switch (readyState) {
    case DataChannelState::Connecting: {
      if (mListenerManager &&
          (mListenerManager->HasListenersFor(nsGkAtoms::onopen) ||
           mListenerManager->HasListenersFor(nsGkAtoms::onmessage) ||
           mListenerManager->HasListenersFor(nsGkAtoms::onerror) ||
           mListenerManager->HasListenersFor(nsGkAtoms::onbufferedamountlow) ||
           mListenerManager->HasListenersFor(nsGkAtoms::onclose))) {
        shouldKeepAlive = true;
      }
    } break;

    case DataChannelState::Open:
    case DataChannelState::Closing: {
      if (mDataChannel->GetBufferedAmount() != 0 ||
          (mListenerManager &&
           (mListenerManager->HasListenersFor(nsGkAtoms::onmessage) ||
            mListenerManager->HasListenersFor(nsGkAtoms::onerror) ||
            mListenerManager->HasListenersFor(nsGkAtoms::onbufferedamountlow) ||
            mListenerManager->HasListenersFor(nsGkAtoms::onclose)))) {
        shouldKeepAlive = true;
      }
    } break;

    case DataChannelState::Closed: {
      shouldKeepAlive = false;
    }
  }

  if (mSelfRef && !shouldKeepAlive) {
    ReleaseSelf();
  } else if (!mSelfRef && shouldKeepAlive) {
    mSelfRef = this;
  }
}

void RTCDataChannel::DontKeepAliveAnyMore() {
  MOZ_ASSERT(NS_IsMainThread());

  if (mSelfRef) {
    // Since we're on MainThread, force an eventloop trip to avoid deleting
    // ourselves.
    ReleaseSelf();
  }

  mCheckMustKeepAlive = false;
}

void RTCDataChannel::ReleaseSelf() {
  // release our self-reference (safely) by putting it in an event (always)
  NS_ReleaseOnMainThread("RTCDataChannel::mSelfRef", mSelfRef.forget(), true);
}

void RTCDataChannel::EventListenerAdded(nsAtom* aType) {
  MOZ_ASSERT(NS_IsMainThread());
  UpdateMustKeepAlive();
}

void RTCDataChannel::EventListenerRemoved(nsAtom* aType) {
  MOZ_ASSERT(NS_IsMainThread());
  UpdateMustKeepAlive();
}

/* static */
nsresult NS_NewDOMDataChannel(already_AddRefed<DataChannel>&& aDataChannel,
                              const nsACString& aLabel, bool aOrdered,
                              Nullable<uint16_t> aMaxLifeTime,
                              Nullable<uint16_t> aMaxRetransmits,
                              const nsACString& aProtocol, bool aNegotiated,
                              nsPIDOMWindowInner* aWindow,
                              RTCDataChannel** aDomDataChannel) {
  RefPtr<RTCDataChannel> domdc =
      new RTCDataChannel(aLabel, aOrdered, aMaxLifeTime, aMaxRetransmits,
                         aProtocol, aNegotiated, aDataChannel, aWindow);

  nsresult rv = domdc->Init(aWindow);
  NS_ENSURE_SUCCESS(rv, rv);

  domdc.forget(aDomDataChannel);
  return NS_OK;
}

}  // end namespace dom
}  // end namespace mozilla
