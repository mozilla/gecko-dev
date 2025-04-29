/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_Http3WebTransportStream_h
#define mozilla_net_Http3WebTransportStream_h

#include <functional>

#include "WebTransportStreamBase.h"
#include "Http3StreamBase.h"
#include "mozilla/Maybe.h"
#include "mozilla/Result.h"
#include "mozilla/ResultVariant.h"

class nsIWebTransportSendStreamStats;
class nsIWebTransportReceiveStreamStats;

namespace mozilla::net {

class Http3WebTransportSession;

class Http3WebTransportStream final : public WebTransportStreamBase,
                                      public Http3StreamBase,
                                      public nsAHttpSegmentWriter,
                                      public nsAHttpSegmentReader {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSAHTTPSEGMENTWRITER
  NS_DECL_NSAHTTPSEGMENTREADER
  NS_DECL_NSIINPUTSTREAMCALLBACK
  NS_DECL_NSIOUTPUTSTREAMCALLBACK

  explicit Http3WebTransportStream(
      Http3Session* aSession, uint64_t aSessionId, WebTransportStreamType aType,
      std::function<void(Result<RefPtr<WebTransportStreamBase>, nsresult>&&)>&&
          aCallback);
  explicit Http3WebTransportStream(Http3Session* aSession, uint64_t aSessionId,
                                   WebTransportStreamType aType,
                                   uint64_t aStreamId);

  class StreamId WebTransportStreamId() const override;
  uint64_t GetStreamId() const override;

  Http3WebTransportSession* GetHttp3WebTransportSession() override {
    return nullptr;
  }
  Http3WebTransportStream* GetHttp3WebTransportStream() override {
    return this;
  }
  Http3Stream* GetHttp3Stream() override { return nullptr; }

  void SetSendOrder(Maybe<int64_t> aSendOrder) override;

  [[nodiscard]] nsresult ReadSegments() override;
  [[nodiscard]] nsresult WriteSegments() override;

  bool Done() const override;
  void Close(nsresult aResult) override;

  void SetResponseHeaders(nsTArray<uint8_t>& aResponseHeaders, bool fin,
                          bool interim) override {}

  uint64_t SessionId() const { return mSessionId; }

  void SendFin() override;
  void Reset(uint64_t aErrorCode) override;
  void SendStopSending(uint8_t aErrorCode) override;

  already_AddRefed<nsIWebTransportSendStreamStats> GetSendStreamStats()
      override;
  already_AddRefed<nsIWebTransportReceiveStreamStats> GetReceiveStreamStats()
      override;

  // When mRecvState is RECV_DONE, this means we already received the FIN.
  bool RecvDone() const override { return mRecvState == RECV_DONE; }

 private:
  friend class Http3WebTransportSession;
  virtual ~Http3WebTransportStream();

  nsresult TryActivating();
  static nsresult ReadRequestSegment(nsIInputStream*, void*, const char*,
                                     uint32_t, uint32_t, uint32_t*);
  static nsresult WritePipeSegment(nsIOutputStream*, void*, char*, uint32_t,
                                   uint32_t, uint32_t*);

  uint64_t mTotalSent = 0;
  uint64_t mTotalReceived = 0;
  // TODO: neqo doesn't expose this information for now.
  uint64_t mTotalAcknowledged = 0;
  bool mSendFin{false};
  // The error code used to reset the stream. Should be only set once.
  Maybe<uint64_t> mResetError;
  // The error code used for STOP_SENDING. Should be only set once.
  Maybe<uint8_t> mStopSendingError;

  // This is used when SendFin or Reset is called when mSendState is SENDING.
  nsTArray<std::function<void()>> mPendingTasks;
};

}  // namespace mozilla::net

#endif  // mozilla_net_Http3WebTransportStream_h
