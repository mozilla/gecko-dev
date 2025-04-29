/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_Http2WebTransportStream_h
#define mozilla_net_Http2WebTransportStream_h

#include <functional>

#include "mozilla/Queue.h"
#include "WebTransportFlowControl.h"
#include "WebTransportStreamBase.h"

namespace mozilla::net {

class Capsule;
class Http2WebTransportSessionImpl;

class StreamData final {
 public:
  explicit StreamData(nsTArray<uint8_t>&& aData) : mData(std::move(aData)) {
    MOZ_COUNT_CTOR(StreamData);
  }

  MOZ_COUNTED_DTOR(StreamData)

  const nsTArray<uint8_t>& GetData() const { return mData; }

 private:
  nsTArray<uint8_t> mData;
};

class Http2WebTransportStream final : public WebTransportStreamBase {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIINPUTSTREAMCALLBACK
  NS_DECL_NSIOUTPUTSTREAMCALLBACK

  explicit Http2WebTransportStream(
      Http2WebTransportSessionImpl* aWebTransportSession, StreamId aStreamId,
      uint64_t aInitialMaxStreamData, uint64_t aInitialLocalMaxStreamData,
      std::function<void(Result<RefPtr<WebTransportStreamBase>, nsresult>&&)>&&
          aCallback);

  explicit Http2WebTransportStream(
      Http2WebTransportSessionImpl* aWebTransportSession,
      uint64_t aInitialMaxStreamData, uint64_t aInitialLocalMaxStreamData,
      StreamId aStreamId);

  nsresult Init();

  StreamId WebTransportStreamId() const override;
  uint64_t GetStreamId() const override;
  void SendStopSending(uint8_t aErrorCode) override;
  void SendFin() override;
  void Reset(uint64_t aErrorCode) override;
  already_AddRefed<nsIWebTransportSendStreamStats> GetSendStreamStats()
      override;
  already_AddRefed<nsIWebTransportReceiveStreamStats> GetReceiveStreamStats()
      override;
  bool RecvDone() const override;
  void SetSendOrder(Maybe<int64_t> aSendOrder) override;
  SenderFlowControlBase* SenderFc() override { return &mFc; }
  ReceiverFlowControlBase* ReceiverFc() override { return &mReceiverFc; }

  nsresult OnCapsule(Capsule&& aCapsule);
  void Close(nsresult aResult);
  void WriteMaintenanceCapsules(
      mozilla::Queue<UniquePtr<CapsuleEncoder>>& aOutput);
  void TakeOutputCapsule(mozilla::Queue<UniquePtr<CapsuleEncoder>>& aOutput);

  void OnStopSending();

 private:
  virtual ~Http2WebTransportStream();

  static nsresult ReadRequestSegment(nsIInputStream*, void*, const char*,
                                     uint32_t, uint32_t, uint32_t*);

  nsresult HandleStreamData(bool aFin, nsTArray<uint8_t>&& aData);
  nsresult HandleMaxStreamData(uint64_t aLimit);
  nsresult HandleStopSending(uint64_t aError);

  RefPtr<Http2WebTransportSessionImpl> mWebTransportSession;
  class StreamId mStreamId{0u};
  nsTArray<uint8_t> mBuffer;
  uint64_t mTotalSent = 0;
  uint64_t mTotalReceived = 0;
  uint32_t mWriteOffset = 0;
  bool mSentStopSending = false;
  // The queue used for passing data to the upper layer.
  // When mReceiveStreamPipeOut->Write() returns NS_BASE_STREAM_WOULD_BLOCK, we
  // need to store the data in this queue.
  mozilla::Queue<UniquePtr<StreamData>> mOutgoingQueue;
  mozilla::Queue<UniquePtr<CapsuleEncoder>> mCapsuleQueue;
  UniquePtr<StreamData> mCurrentOut;
  const RefPtr<nsISerialEventTarget> mOwnerThread;
  SenderFlowControlStreamId mFc;
  ReceiverFlowControlStreamId mReceiverFc;
  Maybe<Capsule> mStopSendingCapsule;
};
}  // namespace mozilla::net

#endif
