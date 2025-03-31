/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// HttpLog.h should generally be included first
#include "HttpLog.h"

#include "Capsule.h"
#include "CapsuleEncoder.h"
#include "Http2WebTransportSession.h"
#include "Http2Session.h"
#include "mozilla/net/NeqoHttp3Conn.h"
#include "nsIWebTransport.h"
#include "nsIOService.h"

namespace mozilla::net {

NS_IMPL_ISUPPORTS_INHERITED(Http2WebTransportSession, Http2StreamTunnel,
                            nsIOutputStreamCallback, nsIInputStreamCallback)

Http2WebTransportSession::Http2WebTransportSession(
    Http2Session* aSession, int32_t aPriority, uint64_t aBcId,
    nsHttpConnectionInfo* aConnectionInfo)
    : Http2StreamTunnel(aSession, aPriority, aBcId, aConnectionInfo),
      mCapsuleParser(MakeUnique<CapsuleParser>(this)) {
  LOG(("Http2WebTransportSession ctor:%p", this));
}

Http2WebTransportSession::~Http2WebTransportSession() {
  LOG(("Http2WebTransportSession dtor:%p", this));
}

void Http2WebTransportSession::CloseStream(nsresult aReason) {
  LOG(("Http2WebTransportSession::CloseStream this=%p aReason=%x", this,
       static_cast<uint32_t>(aReason)));
  if (mTransaction) {
    mTransaction->Close(aReason);
    mTransaction = nullptr;
  }
  Http2StreamTunnel::CloseStream(aReason);
}

nsresult Http2WebTransportSession::GenerateHeaders(nsCString& aCompressedData,
                                                   uint8_t& aFirstFrameFlags) {
  nsHttpRequestHead* head = mTransaction->RequestHead();

  nsAutoCString authorityHeader;
  nsresult rv = head->GetHeader(nsHttp::Host, authorityHeader);
  NS_ENSURE_SUCCESS(rv, rv);

  RefPtr<Http2Session> session = Session();
  LOG3(("Http2WebTransportSession %p Stream ID 0x%X [session=%p] for %s\n",
        this, mStreamID, session.get(), authorityHeader.get()));

  nsAutoCString path;
  head->Path(path);

  rv = session->Compressor()->EncodeHeaderBlock(
      mFlatHttpRequestHeaders, "CONNECT"_ns, path, authorityHeader, "https"_ns,
      "webtransport"_ns, false, aCompressedData);
  NS_ENSURE_SUCCESS(rv, rv);

  mRequestBodyLenRemaining = 0x0fffffffffffffffULL;

  if (mInput) {
    mInput->AsyncWait(this, 0, 0, nullptr);
  }
  return NS_OK;
}

bool Http2WebTransportSession::OnCapsule(Capsule&& aCapsule) {
  switch (aCapsule.Type()) {
    case CapsuleType::CLOSE_WEBTRANSPORT_SESSION:
      LOG(("Handling CLOSE_WEBTRANSPORT_SESSION\n"));
      break;
    case CapsuleType::DRAIN_WEBTRANSPORT_SESSION:
      LOG(("Handling DRAIN_WEBTRANSPORT_SESSION\n"));
      break;
    case CapsuleType::PADDING:
      LOG(("Handling PADDING\n"));
      break;
    case CapsuleType::WT_RESET_STREAM:
      LOG(("Handling WT_RESET_STREAM\n"));
      break;
    case CapsuleType::WT_STOP_SENDING:
      LOG(("Handling WT_STOP_SENDING\n"));
      break;
    case CapsuleType::WT_STREAM:
      LOG(("Handling WT_STREAM\n"));
      break;
    case CapsuleType::WT_STREAM_FIN:
      LOG(("Handling WT_STREAM_FIN\n"));
      break;
    case CapsuleType::WT_MAX_DATA:
      LOG(("Handling WT_MAX_DATA\n"));
      break;
    case CapsuleType::WT_MAX_STREAM_DATA:
      LOG(("Handling WT_MAX_STREAM_DATA\n"));
      break;
    case CapsuleType::WT_MAX_STREAMS_BIDI:
      LOG(("Handling WT_MAX_STREAMS_BIDI\n"));
      break;
    case CapsuleType::WT_MAX_STREAMS_UNIDI:
      LOG(("Handling WT_MAX_STREAMS_UNIDI\n"));
      break;
    case CapsuleType::WT_DATA_BLOCKED:
      LOG(("Handling WT_DATA_BLOCKED\n"));
      break;
    case CapsuleType::WT_STREAM_DATA_BLOCKED:
      LOG(("Handling WT_STREAM_DATA_BLOCKED\n"));
      break;
    case CapsuleType::WT_STREAMS_BLOCKED_BIDI:
      LOG(("Handling WT_STREAMS_BLOCKED_BIDI\n"));
      break;
    case CapsuleType::WT_STREAMS_BLOCKED_UNIDI:
      LOG(("Handling WT_STREAMS_BLOCKED_UNIDI\n"));
      break;
    default:
      LOG(("Unhandled capsule type\n"));
      break;
  }
  return true;
}

void Http2WebTransportSession::OnCapsuleParseFailure(nsresult aError) {}

NS_IMETHODIMP
Http2WebTransportSession::OnInputStreamReady(nsIAsyncInputStream* aIn) {
  char buffer[nsIOService::gDefaultSegmentSize];
  uint32_t remainingCapacity = sizeof(buffer);
  uint32_t read = 0;

  while (remainingCapacity > 0) {
    uint32_t count = 0;
    nsresult rv = mInput->Read(buffer + read, remainingCapacity, &count);
    if (rv == NS_BASE_STREAM_WOULD_BLOCK) {
      break;
    }

    if (NS_FAILED(rv)) {
      LOG(("Http2WebTransportSession::OnInputStreamReady %p failed %u\n", this,
           static_cast<uint32_t>(rv)));
      // TODO: close connection
      return rv;
    }

    // base stream closed
    if (count == 0) {
      LOG((
          "Http2WebTransportSession::OnInputStreamReady %p connection closed\n",
          this));
      // Close with NS_BASE_STREAM_CLOSED
      return NS_OK;
    }

    remainingCapacity -= count;
    read += count;
  }

  if (read > 0) {
    Http2Session::LogIO(nullptr, this, "Http2WebTransportSession", buffer,
                        read);

    mCapsuleParser->ProcessCapsuleData(reinterpret_cast<uint8_t*>(buffer),
                                       read);
  }

  mInput->AsyncWait(this, 0, 0, nullptr);
  return NS_OK;
}

NS_IMETHODIMP
Http2WebTransportSession::OnOutputStreamReady(nsIAsyncOutputStream* aOut) {
  while (!mOutgoingQueue.empty() && mOutput) {
    CapsuleEncoder& data = mOutgoingQueue.front();
    auto buffer = data.GetBuffer();
    const char* writeBuffer =
        reinterpret_cast<const char*>(buffer.Elements()) + mWriteOffset;
    uint32_t toWrite = buffer.Length() - mWriteOffset;

    uint32_t wrote = 0;
    nsresult rv = mOutput->Write(writeBuffer, toWrite, &wrote);
    if (rv == NS_BASE_STREAM_WOULD_BLOCK) {
      mOutput->AsyncWait(this, 0, 0, nullptr);
      return NS_OK;
    }

    if (NS_FAILED(rv)) {
      LOG(("Http2WebTransportSession::OnOutputStreamReady %p failed %u\n", this,
           static_cast<uint32_t>(rv)));
      // TODO: close connection
      return NS_OK;
    }

    mWriteOffset += wrote;

    if (toWrite == wrote) {
      mWriteOffset = 0;
      mOutgoingQueue.pop_front();
    }
  }

  if (mInput) {
    mInput->AsyncWait(this, 0, 0, nullptr);
  }
  return NS_OK;
}

void Http2WebTransportSession::SendCapsule(CapsuleEncoder&& aEncoder) {
  LOG(("Http2WebTransportSession::SendCapsule %p mSendClosed=%d", this,
       mSendClosed));
  if (mSendClosed) {
    return;
  }

  mOutgoingQueue.emplace_back(std::move(aEncoder));

  if (mOutput) {
    OnOutputStreamReady(mOutput);
  }
}

void Http2WebTransportSession::CloseSession(uint32_t aStatus,
                                            const nsACString& aReason) {
  LOG(("Http2WebTransportSession::CloseSession %p aStatus=%x", this, aStatus));

  SetSentFin(true);

  Capsule capsule = Capsule::CloseWebTransportSession(aStatus, aReason);
  CapsuleEncoder encoder;
  encoder.EncodeCapsule(capsule);
  SendCapsule(std::move(encoder));
}

uint64_t Http2WebTransportSession::StreamId() const { return mStreamID; }

void Http2WebTransportSession::GetMaxDatagramSize() {}

void Http2WebTransportSession::SendDatagram(nsTArray<uint8_t>&& aData,
                                            uint64_t aTrackingId) {}

}  // namespace mozilla::net
