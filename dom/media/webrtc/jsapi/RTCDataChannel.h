/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_WEBRTC_RTCDATACHANNEL_H_
#define DOM_MEDIA_WEBRTC_RTCDATACHANNEL_H_

#include "mozilla/Attributes.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/dom/Nullable.h"
#include "mozilla/dom/RTCDataChannelBinding.h"
#include "mozilla/dom/TypedArray.h"
#include "mozilla/net/DataChannelListener.h"

namespace mozilla {
class DataChannel;

namespace dom {
class Blob;

class RTCDataChannel final : public DOMEventTargetHelper,
                             public DataChannelListener {
 public:
  RTCDataChannel(const nsACString& aLabel, bool aOrdered,
                 Nullable<uint16_t> aMaxLifeTime,
                 Nullable<uint16_t> aMaxRetransmits,
                 const nsACString& aProtocol, bool aNegotiated,
                 already_AddRefed<DataChannel>& aDataChannel,
                 nsPIDOMWindowInner* aWindow);

  nsresult Init(nsPIDOMWindowInner* aDOMWindow);

  NS_DECL_ISUPPORTS_INHERITED

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(RTCDataChannel, DOMEventTargetHelper)

  // EventTarget
  using EventTarget::EventListenerAdded;
  void EventListenerAdded(nsAtom* aType) override;

  using EventTarget::EventListenerRemoved;
  void EventListenerRemoved(nsAtom* aType) override;

  JSObject* WrapObject(JSContext*, JS::Handle<JSObject*> aGivenProto) override;
  nsIGlobalObject* GetParentObject() const { return GetOwnerGlobal(); }

  // WebIDL
  void GetLabel(nsACString& aLabel) const;
  void GetProtocol(nsACString& aProtocol) const;
  Nullable<uint16_t> GetMaxPacketLifeTime() const;
  Nullable<uint16_t> GetMaxRetransmits() const;
  RTCDataChannelState ReadyState() const;
  uint32_t BufferedAmount() const;
  uint32_t BufferedAmountLowThreshold() const;
  void SetBufferedAmountLowThreshold(uint32_t aThreshold);
  IMPL_EVENT_HANDLER(open)
  IMPL_EVENT_HANDLER(error)
  IMPL_EVENT_HANDLER(close)
  void Close();
  IMPL_EVENT_HANDLER(message)
  IMPL_EVENT_HANDLER(bufferedamountlow)
  RTCDataChannelType BinaryType() const {
    return static_cast<RTCDataChannelType>(static_cast<int>(mBinaryType));
  }
  void SetBinaryType(RTCDataChannelType aType) {
    mBinaryType = static_cast<DataChannelBinaryType>(static_cast<int>(aType));
  }
  void Send(const nsAString& aData, ErrorResult& aRv);
  void Send(Blob& aData, ErrorResult& aRv);
  void Send(const ArrayBuffer& aData, ErrorResult& aRv);
  void Send(const ArrayBufferView& aData, ErrorResult& aRv);

  bool Negotiated() const;
  bool Ordered() const;
  Nullable<uint16_t> GetId() const;

  nsresult DoOnMessageAvailable(const nsACString& aMessage, bool aBinary);

  virtual nsresult OnMessageAvailable(const nsACString& aMessage) override;

  virtual nsresult OnBinaryMessageAvailable(
      const nsACString& aMessage) override;

  virtual nsresult OnSimpleEvent(const nsAString& aName);

  virtual nsresult OnChannelConnected() override;

  virtual nsresult OnChannelClosed() override;

  virtual nsresult OnBufferLow() override;

  virtual nsresult NotBuffered() override;

  // if there are "strong event listeners" or outgoing not sent messages
  // then this method keeps the object alive when js doesn't have strong
  // references to it.
  void UpdateMustKeepAlive();
  // ATTENTION, when calling this method the object can be released
  // (and possibly collected).
  void DontKeepAliveAnyMore();

 protected:
  ~RTCDataChannel();

 private:
  bool CheckReadyState(ErrorResult& aRv);

  void ReleaseSelf();

  // to keep us alive while we have listeners
  RefPtr<RTCDataChannel> mSelfRef;
  // Owning reference
  RefPtr<DataChannel> mDataChannel;
  nsString mOrigin;
  enum DataChannelBinaryType {
    DC_BINARY_TYPE_ARRAYBUFFER,
    DC_BINARY_TYPE_BLOB,
  };
  DataChannelBinaryType mBinaryType;
  bool mCheckMustKeepAlive;
  bool mSentClose;

  const nsCString mLabel;
  const bool mOrdered;
  const Nullable<uint16_t> mMaxPacketLifeTime;
  const Nullable<uint16_t> mMaxRetransmits;
  const nsCString mProtocol;
  const bool mNegotiated;
};

}  // namespace dom
}  // namespace mozilla
#endif  // DOM_MEDIA_WEBRTC_RTCDATACHANNEL_H_
