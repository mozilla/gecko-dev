/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// HttpLog.h should generally be included first
#include "HttpLog.h"

#include <algorithm>
#include "Http2WebTransportStream.h"
#include "Http2WebTransportSession.h"
#include "Capsule.h"
#include "CapsuleEncoder.h"
#include "nsIOService.h"

namespace mozilla::net {

NS_IMPL_ISUPPORTS(Http2WebTransportStream, nsIOutputStreamCallback,
                  nsIInputStreamCallback)

Http2WebTransportStream::Http2WebTransportStream(
    Http2WebTransportSessionImpl* aWebTransportSession, StreamId aStreamId,
    uint64_t aInitialMaxStreamData, uint64_t aInitialLocalMaxStreamData,
    std::function<void(Result<RefPtr<WebTransportStreamBase>, nsresult>&&)>&&
        aCallback)
    : WebTransportStreamBase(aWebTransportSession->GetStreamId(),
                             std::move(aCallback)),
      mWebTransportSession(aWebTransportSession),
      mStreamId(aStreamId),
      mOwnerThread(GetCurrentSerialEventTarget()),
      mFc(aStreamId, aInitialMaxStreamData),
      mReceiverFc(aStreamId, aInitialLocalMaxStreamData) {
  LOG(("Http2WebTransportStream outgoing ctor:%p", this));
  mStreamRole = OUTGOING;
  mStreamType = mStreamId.StreamType();
}

Http2WebTransportStream::Http2WebTransportStream(
    Http2WebTransportSessionImpl* aWebTransportSession,
    uint64_t aInitialMaxStreamData, uint64_t aInitialLocalMaxStreamData,
    StreamId aStreamId)
    : WebTransportStreamBase(aWebTransportSession->GetStreamId(), nullptr),
      mWebTransportSession(aWebTransportSession),
      mStreamId(aStreamId),
      mOwnerThread(GetCurrentSerialEventTarget()),
      mFc(aStreamId, aInitialMaxStreamData),
      mReceiverFc(aStreamId, aInitialLocalMaxStreamData) {
  LOG(("Http2WebTransportStream incoming ctor:%p", this));
  mStreamRole = INCOMING;
  mStreamType = mStreamId.StreamType();
}

Http2WebTransportStream::~Http2WebTransportStream() {
  LOG(("Http2WebTransportStream dtor:%p", this));
}

nsresult Http2WebTransportStream::Init() {
  nsresult rv = NS_OK;
  auto resultCallback = MakeScopeExit([&] {
    if (NS_FAILED(rv)) {
      mSendState = SEND_DONE;
      mRecvState = RECV_DONE;
      if (mStreamReadyCallback) {
        mStreamReadyCallback(Err(rv));
      }

    } else {
      mSocketInCondition = NS_OK;
      mSocketOutCondition = NS_OK;
      RefPtr<WebTransportStreamBase> stream = this;
      if (mStreamReadyCallback) {
        mStreamReadyCallback(stream);
      }
    }
    mStreamReadyCallback = nullptr;
  });

  if (mStreamRole == INCOMING) {
    rv = InitInputPipe();
    if (NS_FAILED(rv)) {
      return rv;
    }

    if (mStreamType == WebTransportStreamType::BiDi) {
      rv = InitOutputPipe();
    }

    return rv;
  }

  MOZ_ASSERT(mStreamRole == OUTGOING);
  rv = InitOutputPipe();
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (mStreamType == WebTransportStreamType::BiDi) {
    rv = InitInputPipe();
  }

  if (mSendStreamPipeIn) {
    rv = mSendStreamPipeIn->AsyncWait(this, 0, 0, mOwnerThread);
  }

  return rv;
}

class StreamId Http2WebTransportStream::WebTransportStreamId() const {
  return mStreamId;
}

uint64_t Http2WebTransportStream::GetStreamId() const { return mStreamId; }

void Http2WebTransportStream::SendStopSending(uint8_t aErrorCode) {
  if (mSentStopSending || !mWebTransportSession) {
    // https://www.ietf.org/archive/id/draft-ietf-webtrans-http2-11.html#section-6.3
    // A WT_STOP_SENDING capsule MUST NOT be sent multiple times for the same
    // stream.
    return;
  }

  mSentStopSending = true;
  mStopSendingCapsule.emplace(
      Capsule::WebTransportStopSending(aErrorCode, mStreamId));
  mWebTransportSession->StreamHasCapsuleToSend();
  mRecvState = RECV_DONE;
}

void Http2WebTransportStream::SendFin() {}

void Http2WebTransportStream::Reset(uint64_t aErrorCode) {}

already_AddRefed<nsIWebTransportSendStreamStats>
Http2WebTransportStream::GetSendStreamStats() {
  return nullptr;
}

already_AddRefed<nsIWebTransportReceiveStreamStats>
Http2WebTransportStream::GetReceiveStreamStats() {
  return nullptr;
}

bool Http2WebTransportStream::RecvDone() const { return false; }

void Http2WebTransportStream::SetSendOrder(Maybe<int64_t> aSendOrder) {}

NS_IMETHODIMP
Http2WebTransportStream::OnInputStreamReady(nsIAsyncInputStream* aIn) {
  LOG1(
      ("Http2WebTransportStream::OnInputStreamReady [this=%p stream=%p "
       "state=%d]",
       this, aIn, mSendState));
  if (mSendState == SEND_DONE) {
    // already closed
    return NS_OK;
  }

  uint32_t sendBytes = 0;
  return mSendStreamPipeIn->ReadSegments(
      ReadRequestSegment, this, nsIOService::gDefaultSegmentSize, &sendBytes);
}

NS_IMETHODIMP
Http2WebTransportStream::OnOutputStreamReady(nsIAsyncOutputStream* aOut) {
  if (!mCurrentOut) {
    if (mOutgoingQueue.IsEmpty()) {
      return NS_OK;
    }
    mCurrentOut = mOutgoingQueue.Pop();
  }

  while (mCurrentOut && mReceiveStreamPipeOut) {
    char* writeBuffer = reinterpret_cast<char*>(const_cast<uint8_t*>(
                            mCurrentOut->GetData().Elements())) +
                        mWriteOffset;
    uint32_t toWrite = mCurrentOut->GetData().Length() - mWriteOffset;

    uint32_t wrote = 0;
    nsresult rv = mReceiveStreamPipeOut->Write(writeBuffer, toWrite, &wrote);
    if (rv == NS_BASE_STREAM_WOULD_BLOCK) {
      mSocketInCondition =
          mReceiveStreamPipeOut->AsyncWait(this, 0, 0, nullptr);
      return mSocketInCondition;
    }

    if (NS_FAILED(rv)) {
      LOG(("Http2WebTransportStream::OnOutputStreamReady %p failed %u\n", this,
           static_cast<uint32_t>(rv)));
      // TODO: close this stream
      mSocketInCondition = rv;
      mCurrentOut = nullptr;
      mRecvState = RECV_DONE;
      return NS_OK;
    }

    // Retire when sending data to the consumer.
    mReceiverFc.AddRetired(wrote);
    mWebTransportSession->ReceiverFc().AddRetired(wrote);

    mWriteOffset += wrote;

    if (toWrite == wrote) {
      mWriteOffset = 0;
      mCurrentOut = mOutgoingQueue.IsEmpty() ? nullptr : mOutgoingQueue.Pop();
    }
  }
  return NS_OK;
}

// static
nsresult Http2WebTransportStream::ReadRequestSegment(
    nsIInputStream* stream, void* closure, const char* buf, uint32_t offset,
    uint32_t count, uint32_t* countRead) {
  Http2WebTransportStream* wtStream = (Http2WebTransportStream*)closure;
  LOG(("Http2WebTransportStream::ReadRequestSegment %p count=%u", wtStream,
       count));
  *countRead = 0;
  if (!wtStream->mWebTransportSession) {
    return NS_ERROR_UNEXPECTED;
  }

  uint64_t limit =
      std::min(wtStream->mWebTransportSession->SessionDataFc().Available(),
               wtStream->mFc.Available());
  if (limit < count) {
    if (wtStream->mWebTransportSession->SessionDataFc().Available() < count) {
      LOG(("blocked by session level flow control"));
      wtStream->mWebTransportSession->SessionDataFc().Blocked();
    }
    if (wtStream->mFc.Available() < count) {
      LOG(("blocked by stream level flow control"));
      wtStream->mFc.Blocked();
    }
    return NS_BASE_STREAM_WOULD_BLOCK;
  }

  nsTArray<uint8_t> data;
  data.AppendElements(buf, count);
  Capsule capsule = Capsule::WebTransportStreamData(wtStream->mStreamId, false,
                                                    std::move(data));
  UniquePtr<CapsuleEncoder> encoder = MakeUnique<CapsuleEncoder>();
  encoder->EncodeCapsule(capsule);
  wtStream->mCapsuleQueue.Push(std::move(encoder));
  wtStream->mTotalSent += count;
  wtStream->mFc.Consume(count);
  wtStream->mWebTransportSession->SessionDataFc().Consume(count);
  *countRead = count;
  return NS_OK;
}

void Http2WebTransportStream::TakeOutputCapsule(
    mozilla::Queue<UniquePtr<CapsuleEncoder>>& aOutput) {
  LOG(("Http2WebTransportStream::TakeOutputCapsule %p", this));
  if (mCapsuleQueue.IsEmpty()) {
    mSendStreamPipeIn->AsyncWait(this, 0, 0, mOwnerThread);
    return;
  }
  while (!mCapsuleQueue.IsEmpty()) {
    UniquePtr<CapsuleEncoder> entry = mCapsuleQueue.Pop();
    aOutput.Push(std::move(entry));
  }
  mSendStreamPipeIn->AsyncWait(this, 0, 0, mOwnerThread);
}

void Http2WebTransportStream::WriteMaintenanceCapsules(
    mozilla::Queue<UniquePtr<CapsuleEncoder>>& aOutput) {
  if (mStopSendingCapsule) {
    UniquePtr<CapsuleEncoder> encoder = MakeUnique<CapsuleEncoder>();
    encoder->EncodeCapsule(*mStopSendingCapsule);
    mStopSendingCapsule = Nothing();
    aOutput.Push(std::move(encoder));
  }

  auto dataBlocked = mFc.CreateStreamDataBlockedCapsule();
  if (dataBlocked) {
    aOutput.Push(MakeUnique<CapsuleEncoder>(dataBlocked.ref()));
  }

  auto maxStreamData = mReceiverFc.CreateMaxStreamDataCapsule();
  if (maxStreamData) {
    aOutput.Push(MakeUnique<CapsuleEncoder>(maxStreamData.ref()));
  }

  // Keep reading data from the consumer.
  mSendStreamPipeIn->AsyncWait(this, 0, 0, mOwnerThread);
}

nsresult Http2WebTransportStream::OnCapsule(Capsule&& aCapsule) {
  switch (aCapsule.Type()) {
    case CapsuleType::WT_STREAM: {
      LOG(("Handling WT_STREAM\n"));
      WebTransportStreamDataCapsule& streamData =
          aCapsule.GetWebTransportStreamDataCapsule();
      return HandleStreamData(false, std::move(streamData.mData));
    }
    case CapsuleType::WT_STREAM_FIN:
      LOG(("Handling WT_STREAM_FIN\n"));
      break;
    case CapsuleType::WT_MAX_STREAM_DATA: {
      LOG(("Handling WT_MAX_STREAM_DATA\n"));
      WebTransportMaxStreamDataCapsule& maxStreamData =
          aCapsule.GetWebTransportMaxStreamDataCapsule();
      return HandleMaxStreamData(maxStreamData.mLimit);
    }
    case CapsuleType::WT_STREAM_DATA_BLOCKED:
      LOG(("Handling WT_STREAM_DATA_BLOCKED\n"));
      break;
    default:
      LOG(("Unhandled capsule type\n"));
      break;
  }
  return NS_OK;
}

nsresult Http2WebTransportStream::HandleMaxStreamData(uint64_t aLimit) {
  mFc.Update(aLimit);
  return NS_OK;
}

void Http2WebTransportStream::OnStopSending() { mSendState = SEND_DONE; }

void Http2WebTransportStream::Close(nsresult aResult) {
  if (mSendStreamPipeIn) {
    mSendStreamPipeIn->AsyncWait(nullptr, 0, 0, nullptr);
    mSendStreamPipeIn->CloseWithStatus(aResult);
  }
  if (mReceiveStreamPipeOut) {
    mReceiveStreamPipeOut->AsyncWait(nullptr, 0, 0, nullptr);
    mReceiveStreamPipeOut->CloseWithStatus(aResult);
  }
  mSendState = SEND_DONE;
  mRecvState = RECV_DONE;
  mWebTransportSession = nullptr;
}

nsresult Http2WebTransportStream::HandleStreamData(bool aFin,
                                                   nsTArray<uint8_t>&& aData) {
  LOG(("Http2WebTransportStream::HandleStreamData [this=%p, state=%d aFin=%d",
       this, static_cast<uint32_t>(mRecvState), aFin));

  if (NS_FAILED(mSocketInCondition)) {
    mRecvState = RECV_DONE;
  }

  uint32_t countWrittenSingle = 0;
  switch (mRecvState) {
    case READING: {
      size_t length = aData.Length();
      if (length) {
        auto newConsumed =
            mReceiverFc.SetConsumed(mReceiverFc.Consumed() + length);
        if (newConsumed.isErr()) {
          mSocketInCondition = newConsumed.unwrapErr();
        } else {
          if (!mWebTransportSession->ReceiverFc().Consume(
                  newConsumed.unwrap())) {
            LOG(("Exceed session flow control limit"));
            mSocketInCondition = NS_ERROR_NOT_AVAILABLE;
          } else {
            mOutgoingQueue.Push(MakeUnique<StreamData>(std::move(aData)));
            mSocketInCondition = OnOutputStreamReady(mReceiveStreamPipeOut);
          }
        }
      } else if (mTotalReceived) {
        // https://www.ietf.org/archive/id/draft-ietf-webtrans-http2-10.html#section-6.4
        // Empty WT_STREAM capsules MUST NOT be used unless they open or close a
        // stream
        // TODO: Handle empty stream capsule
      }

      mTotalReceived += length;

      LOG((
          "Http2WebTransportStream::HandleStreamData "
          "countWrittenSingle=%" PRIu32 " socketin=%" PRIx32 " [this=%p]",
          countWrittenSingle, static_cast<uint32_t>(mSocketInCondition), this));

      if (NS_FAILED(mSocketInCondition)) {
        mReceiveStreamPipeOut->Close();
        mRecvState = RECV_DONE;
      } else {
        if (aFin) {
          mRecvState = RECEIVED_FIN;
        }
      }
    } break;
    case RECEIVED_FIN:
      mRecvState = RECV_DONE;
      break;
    case RECV_DONE:
      mSocketInCondition = NS_ERROR_UNEXPECTED;
      break;
    default:
      mSocketInCondition = NS_ERROR_UNEXPECTED;
      break;
  }

  return mSocketInCondition;
}

}  // namespace mozilla::net
