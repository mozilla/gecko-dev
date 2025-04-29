/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_WebTransportStreamBase_h
#define mozilla_net_WebTransportStreamBase_h

#include "nsISupportsImpl.h"
#include "mozilla/net/neqo_glue_ffi_generated.h"
#include "mozilla/Mutex.h"
#include "nsCOMPtr.h"
#include "nsIAsyncInputStream.h"
#include "nsIAsyncOutputStream.h"

class nsIWebTransportSendStreamStats;
class nsIWebTransportReceiveStreamStats;

namespace mozilla::net {

// https://www.ietf.org/archive/id/draft-ietf-webtrans-http2-10.html#section-5.2-2
// Client initiated streams having even-numbered stream IDs and
// server-initiated streams having odd-numbered stream IDs. Similarly, they
// can be bidirectional or unidirectional, indicated by the second least
// significant bit of the stream ID.

class StreamId {
 public:
  constexpr explicit StreamId(uint64_t aId) : mId(aId) {}

  constexpr bool IsBiDi() const { return (mId & 0x02) == 0; }

  constexpr bool IsUni() const { return !IsBiDi(); }

  constexpr WebTransportStreamType StreamType() const {
    return IsBiDi() ? WebTransportStreamType::BiDi
                    : WebTransportStreamType::UniDi;
  }

  constexpr bool IsClientInitiated() const { return (mId & 0x01) == 0; }

  constexpr bool IsServerInitiated() const { return !IsClientInitiated(); }

  void Next() { mId += 4; }

  constexpr uint64_t Index() const { return mId >> 2; }

  constexpr bool operator==(uint64_t aVal) const { return mId == aVal; }

  static constexpr StreamId From(uint64_t aVal) { return StreamId(aVal); }

  constexpr operator uint64_t() const { return mId; }

 private:
  uint64_t mId = 0;
};

class SenderFlowControlBase;
class ReceiverFlowControlBase;

class WebTransportStreamBase : public nsIInputStreamCallback,
                               public nsIOutputStreamCallback {
 public:
  NS_INLINE_DECL_PURE_VIRTUAL_REFCOUNTING

  explicit WebTransportStreamBase(
      uint64_t aSessionId,
      std::function<void(Result<RefPtr<WebTransportStreamBase>, nsresult>&&)>&&
          aCallback);

  WebTransportStreamType StreamType() const { return mStreamType; }

  void GetWriterAndReader(nsIAsyncOutputStream** aOutOutputStream,
                          nsIAsyncInputStream** aOutInputStream);

  virtual StreamId WebTransportStreamId() const = 0;
  virtual uint64_t GetStreamId() const = 0;
  virtual void SendStopSending(uint8_t aErrorCode) = 0;
  virtual void SendFin() = 0;
  virtual void Reset(uint64_t aErrorCode) = 0;
  virtual already_AddRefed<nsIWebTransportSendStreamStats>
  GetSendStreamStats() = 0;
  virtual already_AddRefed<nsIWebTransportReceiveStreamStats>
  GetReceiveStreamStats() = 0;
  virtual bool RecvDone() const = 0;
  virtual void SetSendOrder(Maybe<int64_t> aSendOrder) = 0;
  // Used only for testing.
  virtual SenderFlowControlBase* SenderFc() { return nullptr; }
  virtual ReceiverFlowControlBase* ReceiverFc() { return nullptr; }

 protected:
  virtual ~WebTransportStreamBase();

  nsresult InitOutputPipe();
  nsresult InitInputPipe();

  uint64_t mSessionId{UINT64_MAX};
  WebTransportStreamType mStreamType{WebTransportStreamType::BiDi};

  enum StreamRole {
    INCOMING,
    OUTGOING,
  } mStreamRole{INCOMING};

  enum SendStreamState {
    WAITING_TO_ACTIVATE,
    WAITING_DATA,
    SENDING,
    SEND_DONE,
  } mSendState{WAITING_TO_ACTIVATE};

  enum RecvStreamState { BEFORE_READING, READING, RECEIVED_FIN, RECV_DONE };
  Atomic<RecvStreamState> mRecvState{BEFORE_READING};

  nsresult mSocketOutCondition = NS_ERROR_NOT_INITIALIZED;
  nsresult mSocketInCondition = NS_ERROR_NOT_INITIALIZED;

  std::function<void(Result<RefPtr<WebTransportStreamBase>, nsresult>&&)>
      mStreamReadyCallback;

  Mutex mMutex{"WebTransportStreamBase::mMutex"};
  nsCOMPtr<nsIAsyncInputStream> mSendStreamPipeIn;
  nsCOMPtr<nsIAsyncOutputStream> mSendStreamPipeOut MOZ_GUARDED_BY(mMutex);

  nsCOMPtr<nsIAsyncInputStream> mReceiveStreamPipeIn MOZ_GUARDED_BY(mMutex);
  nsCOMPtr<nsIAsyncOutputStream> mReceiveStreamPipeOut;
};

}  // namespace mozilla::net

inline nsISupports* ToSupports(mozilla::net::WebTransportStreamBase* aStream) {
  return static_cast<nsIInputStreamCallback*>(aStream);
}

#endif  // mozilla_net_WebTransportStreamBase_h
