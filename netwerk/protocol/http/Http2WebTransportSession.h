/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_Http2WebTransportSession_h
#define mozilla_net_Http2WebTransportSession_h

#include <list>

#include "CapsuleParser.h"
#include "Http2StreamTunnel.h"
#include "mozilla/UniquePtr.h"
#include "nsRefPtrHashtable.h"
#include "nsTHashMap.h"
#include "nsHashKeys.h"
#include "WebTransportSessionBase.h"
#include "WebTransportStreamBase.h"

namespace mozilla::net {

class CapsuleEncoder;
class Http2WebTransportStream;

class Http2WebTransportSession final : public WebTransportSessionBase,
                                       public Http2StreamTunnel,
                                       public nsIOutputStreamCallback,
                                       public nsIInputStreamCallback,
                                       public CapsuleParser::Listener {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIOUTPUTSTREAMCALLBACK
  NS_DECL_NSIINPUTSTREAMCALLBACK

  Http2WebTransportSession(Http2Session* aSession, int32_t aPriority,
                           uint64_t aBcId,
                           nsHttpConnectionInfo* aConnectionInfo);
  Http2WebTransportSession* GetHttp2WebTransportSession() override {
    return this;
  }
  void CloseSession(uint32_t aStatus, const nsACString& aReason) override;
  void CloseStream(nsresult aReason) override;
  uint64_t GetStreamId() const override;
  void GetMaxDatagramSize() override;
  void SendDatagram(nsTArray<uint8_t>&& aData, uint64_t aTrackingId) override;
  void CreateOutgoingBidirectionalStream(
      std::function<void(Result<RefPtr<WebTransportStreamBase>, nsresult>&&)>&&
          aCallback) override;
  void CreateOutgoingUnidirectionalStream(
      std::function<void(Result<RefPtr<WebTransportStreamBase>, nsresult>&&)>&&
          aCallback) override;

  bool OnCapsule(Capsule&& aCapsule) override;
  void OnCapsuleParseFailure(nsresult aError) override;
  void StartReading() override;

 private:
  friend class Http2WebTransportStream;

  virtual ~Http2WebTransportSession();
  nsresult GenerateHeaders(nsCString& aCompressedData,
                           uint8_t& aFirstFrameFlags) override;
  void SendCapsule(CapsuleEncoder&& aCapsule);
  void CreateOutgoingStreamInternal(
      bool aBidi,
      std::function<void(Result<RefPtr<WebTransportStreamBase>, nsresult>&&)>&&
          aCallback);
  Result<StreamId, nsresult> GetNextOutgoingStreamId(bool aBidi);

  size_t mWriteOffset{0};
  std::list<CapsuleEncoder> mOutgoingQueue;
  UniquePtr<CapsuleParser> mCapsuleParser;

  StreamId mNextStreamID{0};
  nsRefPtrHashtable<nsUint64HashKey, Http2WebTransportStream> mOutgoingStreams;
  nsRefPtrHashtable<nsUint64HashKey, Http2WebTransportStream> mIncomingStreams;
};

}  // namespace mozilla::net

#endif  // mozilla_net_Http2WebTransportSession_h
