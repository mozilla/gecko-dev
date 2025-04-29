/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_Http2WebTransportStream_h
#define mozilla_net_Http2WebTransportStream_h

#include <functional>
#include <list>

#include "WebTransportStreamBase.h"

namespace mozilla::net {

class Capsule;
class Http2WebTransportSession;

class StreamData {
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
      Http2WebTransportSession* aWebTransportSession, StreamId aStreamId,
      std::function<void(Result<RefPtr<WebTransportStreamBase>, nsresult>&&)>&&
          aCallback);

  explicit Http2WebTransportStream(
      Http2WebTransportSession* aWebTransportSession, StreamId aStreamId);

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

  nsresult OnCapsule(Capsule&& aCapsule);
  void Close(nsresult aResult);

 private:
  virtual ~Http2WebTransportStream();

  static nsresult ReadRequestSegment(nsIInputStream*, void*, const char*,
                                     uint32_t, uint32_t, uint32_t*);

  nsresult HandleStreamData(bool aFin, nsTArray<uint8_t>&& aData);

  RefPtr<Http2WebTransportSession> mWebTransportSession;
  class StreamId mStreamId{0u};
  nsTArray<uint8_t> mBuffer;
  uint64_t mTotalSent = 0;
  uint64_t mTotalReceived = 0;
  uint32_t mWriteOffset = 0;
  // The queue used for passing data to the upper layer.
  // When mReceiveStreamPipeOut->Write() returns NS_BASE_STREAM_WOULD_BLOCK, we
  // need to store the data in this queue.
  std::list<StreamData> mOutgoingQueue;
};
}  // namespace mozilla::net

#endif
