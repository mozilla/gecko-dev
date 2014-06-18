/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// HttpLog.h should generally be included first
#include "HttpLog.h"

// Log on level :5, instead of default :4.
#undef LOG
#define LOG(args) LOG5(args)
#undef LOG_ENABLED
#define LOG_ENABLED() LOG5_ENABLED()

#include <algorithm>

#include "Http2Compression.h"
#include "Http2Session.h"
#include "Http2Stream.h"
#include "Http2Push.h"
#include "TunnelUtils.h"

#include "mozilla/Telemetry.h"
#include "nsAlgorithm.h"
#include "nsHttp.h"
#include "nsHttpHandler.h"
#include "nsHttpRequestHead.h"
#include "nsISocketTransport.h"
#include "prnetdb.h"

#ifdef DEBUG
// defined by the socket transport service while active
extern PRThread *gSocketThread;
#endif

namespace mozilla {
namespace net {

Http2Stream::Http2Stream(nsAHttpTransaction *httpTransaction,
                         Http2Session *session,
                         int32_t priority)
  : mStreamID(0)
  , mSession(session)
  , mUpstreamState(GENERATING_HEADERS)
  , mState(IDLE)
  , mAllHeadersSent(0)
  , mAllHeadersReceived(0)
  , mTransaction(httpTransaction)
  , mSocketTransport(session->SocketTransport())
  , mSegmentReader(nullptr)
  , mSegmentWriter(nullptr)
  , mChunkSize(session->SendingChunkSize())
  , mRequestBlockedOnRead(0)
  , mRecvdFin(0)
  , mRecvdReset(0)
  , mSentReset(0)
  , mCountAsActive(0)
  , mSentFin(0)
  , mSentWaitingFor(0)
  , mSetTCPSocketBuffer(0)
  , mTxInlineFrameSize(Http2Session::kDefaultBufferSize)
  , mTxInlineFrameUsed(0)
  , mTxStreamFrameSize(0)
  , mRequestBodyLenRemaining(0)
  , mLocalUnacked(0)
  , mBlockedOnRwin(false)
  , mTotalSent(0)
  , mTotalRead(0)
  , mPushSource(nullptr)
  , mIsTunnel(false)
{
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);

  LOG3(("Http2Stream::Http2Stream %p", this));

  mServerReceiveWindow = session->GetServerInitialStreamWindow();
  mClientReceiveWindow = session->PushAllowance();

  mTxInlineFrame = new uint8_t[mTxInlineFrameSize];

  PR_STATIC_ASSERT(nsISupportsPriority::PRIORITY_LOWEST <= kNormalPriority);

  // values of priority closer to 0 are higher priority for the priority
  // argument. This value is used as a group, which maps to a
  // weight that is related to the nsISupportsPriority that we are given.
  int32_t httpPriority;
  if (priority >= nsISupportsPriority::PRIORITY_LOWEST) {
    httpPriority = kWorstPriority;
  } else if (priority <= nsISupportsPriority::PRIORITY_HIGHEST) {
    httpPriority = kBestPriority;
  } else {
    httpPriority = kNormalPriority + priority;
  }
  MOZ_ASSERT(httpPriority >= 0);
  SetPriority(static_cast<uint32_t>(httpPriority));
}

Http2Stream::~Http2Stream()
{
  ClearTransactionsBlockedOnTunnel();
  mStreamID = Http2Session::kDeadStreamID;
}

// ReadSegments() is used to write data down the socket. Generally, HTTP
// request data is pulled from the approriate transaction and
// converted to HTTP/2 data. Sometimes control data like a window-update is
// generated instead.

nsresult
Http2Stream::ReadSegments(nsAHttpSegmentReader *reader,
                          uint32_t count,
                          uint32_t *countRead)
{
  LOG3(("Http2Stream %p ReadSegments reader=%p count=%d state=%x",
        this, reader, count, mUpstreamState));

  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);

  nsresult rv = NS_ERROR_UNEXPECTED;
  mRequestBlockedOnRead = 0;

  if (mRecvdFin || mRecvdReset) {
    // Don't transmit any request frames if the peer cannot respond
    LOG3(("Http2Stream %p ReadSegments request stream aborted due to"
          " response side closure\n", this));
    return NS_ERROR_ABORT;
  }

  // avoid runt chunks if possible by anticipating
  // full data frames
  if (count > (mChunkSize + 8)) {
    uint32_t numchunks = count / (mChunkSize + 8);
    count = numchunks * (mChunkSize + 8);
  }

  switch (mUpstreamState) {
  case GENERATING_HEADERS:
  case GENERATING_BODY:
  case SENDING_BODY:
    // Call into the HTTP Transaction to generate the HTTP request
    // stream. That stream will show up in OnReadSegment().
    mSegmentReader = reader;
    rv = mTransaction->ReadSegments(this, count, countRead);
    mSegmentReader = nullptr;

    LOG3(("Http2Stream::ReadSegments %p trans readsegments rv %x read=%d\n",
          this, rv, *countRead));

    // Check to see if the transaction's request could be written out now.
    // If not, mark the stream for callback when writing can proceed.
    if (NS_SUCCEEDED(rv) &&
        mUpstreamState == GENERATING_HEADERS &&
        !mAllHeadersSent)
      mSession->TransactionHasDataToWrite(this);

    // mTxinlineFrameUsed represents any queued un-sent frame. It might
    // be 0 if there is no such frame, which is not a gurantee that we
    // don't have more request body to send - just that any data that was
    // sent comprised a complete HTTP/2 frame. Likewise, a non 0 value is
    // a queued, but complete, http/2 frame length.

    // Mark that we are blocked on read if the http transaction needs to
    // provide more of the request message body and there is nothing queued
    // for writing
    if (rv == NS_BASE_STREAM_WOULD_BLOCK && !mTxInlineFrameUsed)
      mRequestBlockedOnRead = 1;

    // If the sending flow control window is open (!mBlockedOnRwin) then
    // continue sending the request
    if (!mBlockedOnRwin &&
        !mTxInlineFrameUsed && NS_SUCCEEDED(rv) && (!*countRead)) {
      LOG3(("Http2Stream::ReadSegments %p 0x%X: Sending request data complete, "
            "mUpstreamState=%x\n",this, mStreamID, mUpstreamState));
      if (mSentFin) {
        ChangeState(UPSTREAM_COMPLETE);
      } else {
        GenerateDataFrameHeader(0, true);
        ChangeState(SENDING_FIN_STREAM);
        mSession->TransactionHasDataToWrite(this);
        rv = NS_BASE_STREAM_WOULD_BLOCK;
      }
    }
    break;

  case SENDING_FIN_STREAM:
    // We were trying to send the FIN-STREAM but were blocked from
    // sending it out - try again.
    if (!mSentFin) {
      mSegmentReader = reader;
      rv = TransmitFrame(nullptr, nullptr, false);
      mSegmentReader = nullptr;
      MOZ_ASSERT(NS_FAILED(rv) || !mTxInlineFrameUsed,
                 "Transmit Frame should be all or nothing");
      if (NS_SUCCEEDED(rv))
        ChangeState(UPSTREAM_COMPLETE);
    } else {
      rv = NS_OK;
      mTxInlineFrameUsed = 0;         // cancel fin data packet
      ChangeState(UPSTREAM_COMPLETE);
    }

    *countRead = 0;

    // don't change OK to WOULD BLOCK. we are really done sending if OK
    break;

  case UPSTREAM_COMPLETE:
    *countRead = 0;
    rv = NS_OK;
    break;

  default:
    MOZ_ASSERT(false, "Http2Stream::ReadSegments unknown state");
    break;
  }

  return rv;
}

// WriteSegments() is used to read data off the socket. Generally this is
// just a call through to the associate nsHttpTransaciton for this stream
// for the remaining data bytes indicated by the current DATA frame.

nsresult
Http2Stream::WriteSegments(nsAHttpSegmentWriter *writer,
                           uint32_t count,
                           uint32_t *countWritten)
{
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);
  MOZ_ASSERT(!mSegmentWriter, "segment writer in progress");

  LOG3(("Http2Stream::WriteSegments %p count=%d state=%x",
        this, count, mUpstreamState));

  mSegmentWriter = writer;
  nsresult rv = mTransaction->WriteSegments(this, count, countWritten);
  mSegmentWriter = nullptr;

  return rv;
}

void
Http2Stream::CreatePushHashKey(const nsCString &scheme,
                               const nsCString &hostHeader,
                               uint64_t serial,
                               const nsCSubstring &pathInfo,
                               nsCString &outOrigin,
                               nsCString &outKey)
{
  outOrigin = scheme;
  outOrigin.AppendLiteral("://");
  outOrigin.Append(hostHeader);

  outKey = outOrigin;
  outKey.AppendLiteral("/[http2.");
  outKey.AppendInt(serial);
  outKey.Append(']');
  outKey.Append(pathInfo);
}

nsresult
Http2Stream::ParseHttpRequestHeaders(const char *buf,
                                     uint32_t avail,
                                     uint32_t *countUsed)
{
  // Returns NS_OK even if the headers are incomplete
  // set mAllHeadersSent flag if they are complete

  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);
  MOZ_ASSERT(mUpstreamState == GENERATING_HEADERS);

  LOG3(("Http2Stream::ParseHttpRequestHeaders %p avail=%d state=%x",
        this, avail, mUpstreamState));

  mFlatHttpRequestHeaders.Append(buf, avail);
  nsHttpRequestHead *head = mTransaction->RequestHead();

  // We can use the simple double crlf because firefox is the
  // only client we are parsing
  int32_t endHeader = mFlatHttpRequestHeaders.Find("\r\n\r\n");

  if (endHeader == kNotFound) {
    // We don't have all the headers yet
    LOG3(("Http2Stream::ParseHttpRequestHeaders %p "
          "Need more header bytes. Len = %d",
          this, mFlatHttpRequestHeaders.Length()));
    *countUsed = avail;
    return NS_OK;
  }

  // We have recvd all the headers, trim the local
  // buffer of the final empty line, and set countUsed to reflect
  // the whole header has been consumed.
  uint32_t oldLen = mFlatHttpRequestHeaders.Length();
  mFlatHttpRequestHeaders.SetLength(endHeader + 2);
  *countUsed = avail - (oldLen - endHeader) + 4;
  mAllHeadersSent = 1;

  nsAutoCString authorityHeader;
  nsAutoCString hashkey;
  head->GetHeader(nsHttp::Host, authorityHeader);

  CreatePushHashKey(nsDependentCString(head->IsHTTPS() ? "https" : "http"),
                    authorityHeader, mSession->Serial(),
                    head->RequestURI(),
                    mOrigin, hashkey);

  // check the push cache for GET
  if (head->IsGet()) {
    // from :scheme, :authority, :path
    nsILoadGroupConnectionInfo *loadGroupCI = mTransaction->LoadGroupConnectionInfo();
    SpdyPushCache *cache = nullptr;
    if (loadGroupCI)
      loadGroupCI->GetSpdyPushCache(&cache);

    Http2PushedStream *pushedStream = nullptr;
    // we remove the pushedstream from the push cache so that
    // it will not be used for another GET. This does not destroy the
    // stream itself - that is done when the transactionhash is done with it.
    if (cache)
      pushedStream = cache->RemovePushedStreamHttp2(hashkey);

    LOG3(("Pushed Stream Lookup "
          "session=%p key=%s loadgroupci=%p cache=%p hit=%p\n",
          mSession, hashkey.get(), loadGroupCI, cache, pushedStream));

    if (pushedStream) {
      LOG3(("Pushed Stream Match located id=0x%X key=%s\n",
            pushedStream->StreamID(), hashkey.get()));
      pushedStream->SetConsumerStream(this);
      mPushSource = pushedStream;
      SetSentFin(true);
      AdjustPushedPriority();

      // This stream has been activated (and thus counts against the concurrency
      // limit intentionally), but will not be registered via
      // RegisterStreamID (below) because of the push match.
      // Release that semaphore count immediately (instead of waiting for
      // cleanup stream) so we can initiate more pull streams.
      mSession->MaybeDecrementConcurrent(this);

      // There is probably pushed data buffered so trigger a read manually
      // as we can't rely on future network events to do it
      mSession->ConnectPushedStream(this);
      return NS_OK;
    }
  }

  // It is now OK to assign a streamID that we are assured will
  // be monotonically increasing amongst new streams on this
  // session
  mStreamID = mSession->RegisterStreamID(this);
  MOZ_ASSERT(mStreamID & 1, "Http2 Stream Channel ID must be odd");
  LOG3(("Stream ID 0x%X [session=%p] for URI %s\n",
        mStreamID, mSession,
        nsCString(head->RequestURI()).get()));

  if (mStreamID >= 0x80000000) {
    // streamID must fit in 31 bits. Evading This is theoretically possible
    // because stream ID assignment is asynchronous to stream creation
    // because of the protocol requirement that the new stream ID
    // be monotonically increasing. In reality this is really not possible
    // because new streams stop being added to a session with millions of
    // IDs still available and no race condition is going to bridge that gap;
    // so we can be comfortable on just erroring out for correctness in that
    // case.
    LOG3(("Stream assigned out of range ID: 0x%X", mStreamID));
    return NS_ERROR_UNEXPECTED;
  }

  // Now we need to convert the flat http headers into a set
  // of HTTP/2 headers by writing to mTxInlineFrame{sz}

  nsCString compressedData;
  nsDependentCString scheme(head->IsHTTPS() ? "https" : "http");
  if (head->IsConnect()) {
    MOZ_ASSERT(mTransaction->QuerySpdyConnectTransaction());
    mIsTunnel = true;
    mRequestBodyLenRemaining = 0x0fffffffffffffffULL;

    // Our normal authority has an implicit port, best to use an
    // explicit one with a tunnel
    nsHttpConnectionInfo *ci = mTransaction->ConnectionInfo();
    if (!ci) {
      return NS_ERROR_UNEXPECTED;
    }
    authorityHeader = ci->GetHost();
    authorityHeader.Append(':');
    authorityHeader.AppendInt(ci->Port());
  }

  mSession->Compressor()->EncodeHeaderBlock(mFlatHttpRequestHeaders,
                                            head->Method(),
                                            head->RequestURI(),
                                            authorityHeader,
                                            scheme,
                                            head->IsConnect(),
                                            compressedData);

  // Determine whether to put the fin bit on the header frame or whether
  // to wait for a data packet to put it on.
  uint8_t firstFrameFlags =  Http2Session::kFlag_PRIORITY;

  if (head->IsGet() ||
      head->IsHead()) {
    // for GET and HEAD place the fin bit right on the
    // header packet

    SetSentFin(true);
    firstFrameFlags |= Http2Session::kFlag_END_STREAM;
  } else if (head->IsPost() ||
             head->IsPut() ||
             head->IsConnect() ||
             head->IsOptions()) {
    // place fin in a data frame even for 0 length messages for iterop
  } else if (!mRequestBodyLenRemaining) {
    // for other HTTP extension methods, rely on the content-length
    // to determine whether or not to put fin on headers
    SetSentFin(true);
    firstFrameFlags |= Http2Session::kFlag_END_STREAM;
  }

  // split this one HEADERS frame up into N HEADERS + CONTINUATION frames if it exceeds the
  // 2^14-1 limit for 1 frame. Do it by inserting header size gaps in the existing
  // frame for the new headers and for the first one a priority field. There is
  // no question this is ugly, but a 16KB HEADERS frame should be a long
  // tail event, so this is really just for correctness and a nop in the base case.
  //

  MOZ_ASSERT(!mTxInlineFrameUsed);

  uint32_t dataLength = compressedData.Length();
  uint32_t maxFrameData = Http2Session::kMaxFrameData - 5; // 5 bytes for priority
  uint32_t numFrames = 1;

  if (dataLength > maxFrameData) {
    numFrames += ((dataLength - maxFrameData) + Http2Session::kMaxFrameData - 1) /
      Http2Session::kMaxFrameData;
    MOZ_ASSERT (numFrames > 1);
  }

  // note that we could still have 1 frame for 0 bytes of data. that's ok.

  uint32_t messageSize = dataLength;
  messageSize += 13; // frame header + priority overhead in HEADERS frame
  messageSize += (numFrames - 1) * 8; // frame header overhead in CONTINUATION frames

  EnsureBuffer(mTxInlineFrame, dataLength + messageSize,
               mTxInlineFrameUsed, mTxInlineFrameSize);

  mTxInlineFrameUsed += messageSize;
  LOG3(("%p Generating %d bytes of HEADERS for stream 0x%X with priority weight %u frames %u\n",
        this, mTxInlineFrameUsed, mStreamID, mPriorityWeight, numFrames));

  uint32_t outputOffset = 0;
  uint32_t compressedDataOffset = 0;
  for (uint32_t idx = 0; idx < numFrames; ++idx) {
    uint32_t flags, frameLen;
    bool lastFrame = (idx == numFrames - 1);

    flags = 0;
    frameLen = maxFrameData;
    if (!idx) {
      flags |= firstFrameFlags;
      // Only the first frame needs the 4-byte offset
      maxFrameData = Http2Session::kMaxFrameData;
    }
    if (lastFrame) {
      frameLen = dataLength;
      flags |= Http2Session::kFlag_END_HEADERS;
    }
    dataLength -= frameLen;

    mSession->CreateFrameHeader(
      mTxInlineFrame.get() + outputOffset,
      frameLen + (idx ? 0 : 5),
      (idx) ? Http2Session::FRAME_TYPE_CONTINUATION : Http2Session::FRAME_TYPE_HEADERS,
      flags, mStreamID);
    outputOffset += 8;

    if (!idx) {
      // Priority - Dependency is 0, weight is our gecko-calculated weight,
      // non-exclusive dependency
      memset(mTxInlineFrame.get() + outputOffset, 0, 4);
      memcpy(mTxInlineFrame.get() + outputOffset + 4, &mPriorityWeight, 1);
      outputOffset += 5;
    }

    memcpy(mTxInlineFrame.get() + outputOffset,
           compressedData.BeginReading() + compressedDataOffset, frameLen);
    compressedDataOffset += frameLen;
    outputOffset += frameLen;
  }

  Telemetry::Accumulate(Telemetry::SPDY_SYN_SIZE, compressedData.Length());

  // The size of the input headers is approximate
  uint32_t ratio =
    compressedData.Length() * 100 /
    (11 + head->RequestURI().Length() +
     mFlatHttpRequestHeaders.Length());

  const char *beginBuffer = mFlatHttpRequestHeaders.BeginReading();
  int32_t crlfIndex = mFlatHttpRequestHeaders.Find("\r\n");
  while (true) {
    int32_t startIndex = crlfIndex + 2;

    crlfIndex = mFlatHttpRequestHeaders.Find("\r\n", false, startIndex);
    if (crlfIndex == -1)
      break;

    int32_t colonIndex = mFlatHttpRequestHeaders.Find(":", false, startIndex,
                                                      crlfIndex - startIndex);
    if (colonIndex == -1)
      break;

    nsDependentCSubstring name = Substring(beginBuffer + startIndex,
                                           beginBuffer + colonIndex);
    // all header names are lower case in spdy
    ToLowerCase(name);

    if (name.EqualsLiteral("content-length")) {
      nsCString *val = new nsCString();
      int32_t valueIndex = colonIndex + 1;
      while (valueIndex < crlfIndex && beginBuffer[valueIndex] == ' ')
        ++valueIndex;

      nsDependentCSubstring v = Substring(beginBuffer + valueIndex,
                                          beginBuffer + crlfIndex);
      val->Append(v);

      int64_t len;
      if (nsHttp::ParseInt64(val->get(), nullptr, &len))
        mRequestBodyLenRemaining = len;
      break;
    }
  }

  mFlatHttpRequestHeaders.Truncate();
  Telemetry::Accumulate(Telemetry::SPDY_SYN_RATIO, ratio);
  return NS_OK;
}

void
Http2Stream::AdjustInitialWindow()
{
  // The default initial_window is sized for pushed streams. When we
  // generate a client pulled stream we want to disable flow control for
  // the stream with a window update. Do the same for pushed streams
  // when they connect to a pull.

  // >0 even numbered IDs are pushed streams.
  // odd numbered IDs are pulled streams.
  // 0 is the sink for a pushed stream.
  Http2Stream *stream = this;
  if (!mStreamID) {
    MOZ_ASSERT(mPushSource);
    if (!mPushSource)
      return;
    stream = mPushSource;
    MOZ_ASSERT(stream->mStreamID);
    MOZ_ASSERT(!(stream->mStreamID & 1)); // is a push stream

    // If the pushed stream has recvd a FIN, there is no reason to update
    // the window
    if (stream->RecvdFin() || stream->RecvdReset())
      return;
  }

  uint8_t *packet = mTxInlineFrame.get() + mTxInlineFrameUsed;
  EnsureBuffer(mTxInlineFrame, mTxInlineFrameUsed + 12,
               mTxInlineFrameUsed, mTxInlineFrameSize);
  mTxInlineFrameUsed += 12;

  mSession->CreateFrameHeader(packet, 4,
                              Http2Session::FRAME_TYPE_WINDOW_UPDATE,
                              0, stream->mStreamID);

  MOZ_ASSERT(mClientReceiveWindow <= ASpdySession::kInitialRwin);
  uint32_t bump = ASpdySession::kInitialRwin - mClientReceiveWindow;
  mClientReceiveWindow += bump;
  bump = PR_htonl(bump);
  memcpy(packet + 8, &bump, 4);
  LOG3(("AdjustInitialwindow increased flow control window %p 0x%X\n",
        this, stream->mStreamID));
}

void
Http2Stream::AdjustPushedPriority()
{
  // >0 even numbered IDs are pushed streams. odd numbered IDs are pulled streams.
  // 0 is the sink for a pushed stream.

  if (mStreamID || !mPushSource)
    return;

  MOZ_ASSERT(mPushSource->mStreamID && !(mPushSource->mStreamID & 1));

  // If the pushed stream has recvd a FIN, there is no reason to update
  // the window
  if (mPushSource->RecvdFin() || mPushSource->RecvdReset())
    return;

  uint8_t *packet = mTxInlineFrame.get() + mTxInlineFrameUsed;
  EnsureBuffer(mTxInlineFrame, mTxInlineFrameUsed + 13,
               mTxInlineFrameUsed, mTxInlineFrameSize);
  mTxInlineFrameUsed += 13;

  mSession->CreateFrameHeader(packet, 5,
                              Http2Session::FRAME_TYPE_PRIORITY,
                              Http2Session::kFlag_PRIORITY,
                              mPushSource->mStreamID);

  mPushSource->SetPriority(mPriority);
  memset(packet + 8, 0, 4);
  memcpy(packet + 12, &mPriorityWeight, 1);

  LOG3(("AdjustPushedPriority %p id 0x%X to weight %X\n", this, mPushSource->mStreamID,
        mPriorityWeight));
}

void
Http2Stream::UpdateTransportReadEvents(uint32_t count)
{
  mTotalRead += count;
  if (!mSocketTransport) {
    return;
  }

  mTransaction->OnTransportStatus(mSocketTransport,
                                  NS_NET_STATUS_RECEIVING_FROM,
                                  mTotalRead);
}

void
Http2Stream::UpdateTransportSendEvents(uint32_t count)
{
  mTotalSent += count;

  // normally on non-windows platform we use TCP autotuning for
  // the socket buffers, and this works well (managing enough
  // buffers for BDP while conserving memory) for HTTP even when
  // it creates really deep queues. However this 'buffer bloat' is
  // a problem for http/2 because it ruins the low latency properties
  // necessary for PING and cancel to work meaningfully.
  //
  // If this stream represents a large upload, disable autotuning for
  // the session and cap the send buffers by default at 128KB.
  // (10Mbit/sec @ 100ms)
  //
  uint32_t bufferSize = gHttpHandler->SpdySendBufferSize();
  if ((mTotalSent > bufferSize) && !mSetTCPSocketBuffer) {
    mSetTCPSocketBuffer = 1;
    mSocketTransport->SetSendBufferSize(bufferSize);
  }

  if (mUpstreamState != SENDING_FIN_STREAM)
    mTransaction->OnTransportStatus(mSocketTransport,
                                    NS_NET_STATUS_SENDING_TO,
                                    mTotalSent);

  if (!mSentWaitingFor && !mRequestBodyLenRemaining) {
    mSentWaitingFor = 1;
    mTransaction->OnTransportStatus(mSocketTransport,
                                    NS_NET_STATUS_WAITING_FOR,
                                    0);
  }
}

nsresult
Http2Stream::TransmitFrame(const char *buf,
                           uint32_t *countUsed,
                           bool forceCommitment)
{
  // If TransmitFrame returns SUCCESS than all the data is sent (or at least
  // buffered at the session level), if it returns WOULD_BLOCK then none of
  // the data is sent.

  // You can call this function with no data and no out parameter in order to
  // flush internal buffers that were previously blocked on writing. You can
  // of course feed new data to it as well.

  LOG3(("Http2Stream::TransmitFrame %p inline=%d stream=%d",
        this, mTxInlineFrameUsed, mTxStreamFrameSize));
  if (countUsed)
    *countUsed = 0;

  if (!mTxInlineFrameUsed) {
    MOZ_ASSERT(!buf);
    return NS_OK;
  }

  MOZ_ASSERT(mTxInlineFrameUsed, "empty stream frame in transmit");
  MOZ_ASSERT(mSegmentReader, "TransmitFrame with null mSegmentReader");
  MOZ_ASSERT((buf && countUsed) || (!buf && !countUsed),
             "TransmitFrame arguments inconsistent");

  uint32_t transmittedCount;
  nsresult rv;

  // In the (relatively common) event that we have a small amount of data
  // split between the inlineframe and the streamframe, then move the stream
  // data into the inlineframe via copy in order to coalesce into one write.
  // Given the interaction with ssl this is worth the small copy cost.
  if (mTxStreamFrameSize && mTxInlineFrameUsed &&
      mTxStreamFrameSize < Http2Session::kDefaultBufferSize &&
      mTxInlineFrameUsed + mTxStreamFrameSize < mTxInlineFrameSize) {
    LOG3(("Coalesce Transmit"));
    memcpy (mTxInlineFrame + mTxInlineFrameUsed,
            buf, mTxStreamFrameSize);
    if (countUsed)
      *countUsed += mTxStreamFrameSize;
    mTxInlineFrameUsed += mTxStreamFrameSize;
    mTxStreamFrameSize = 0;
  }

  rv =
    mSegmentReader->CommitToSegmentSize(mTxStreamFrameSize + mTxInlineFrameUsed,
                                        forceCommitment);

  if (rv == NS_BASE_STREAM_WOULD_BLOCK) {
    MOZ_ASSERT(!forceCommitment, "forceCommitment with WOULD_BLOCK");
    mSession->TransactionHasDataToWrite(this);
  }
  if (NS_FAILED(rv))     // this will include WOULD_BLOCK
    return rv;

  // This function calls mSegmentReader->OnReadSegment to report the actual http/2
  // bytes through to the session object and then the HttpConnection which calls
  // the socket write function. It will accept all of the inline and stream
  // data because of the above 'commitment' even if it has to buffer

  rv = mSession->BufferOutput(reinterpret_cast<char*>(mTxInlineFrame.get()),
                              mTxInlineFrameUsed,
                              &transmittedCount);
  LOG3(("Http2Stream::TransmitFrame for inline BufferOutput session=%p "
        "stream=%p result %x len=%d",
        mSession, this, rv, transmittedCount));

  MOZ_ASSERT(rv != NS_BASE_STREAM_WOULD_BLOCK,
             "inconsistent inline commitment result");

  if (NS_FAILED(rv))
    return rv;

  MOZ_ASSERT(transmittedCount == mTxInlineFrameUsed,
             "inconsistent inline commitment count");

  Http2Session::LogIO(mSession, this, "Writing from Inline Buffer",
                       reinterpret_cast<char*>(mTxInlineFrame.get()),
                       transmittedCount);

  if (mTxStreamFrameSize) {
    if (!buf) {
      // this cannot happen
      MOZ_ASSERT(false, "Stream transmit with null buf argument to "
                 "TransmitFrame()");
      LOG3(("Stream transmit with null buf argument to TransmitFrame()\n"));
      return NS_ERROR_UNEXPECTED;
    }

    // If there is already data buffered, just add to that to form
    // a single TLS Application Data Record - otherwise skip the memcpy
    if (mSession->AmountOfOutputBuffered()) {
      rv = mSession->BufferOutput(buf, mTxStreamFrameSize,
                                  &transmittedCount);
    } else {
      rv = mSession->OnReadSegment(buf, mTxStreamFrameSize,
                                   &transmittedCount);
    }

    LOG3(("Http2Stream::TransmitFrame for regular session=%p "
          "stream=%p result %x len=%d",
          mSession, this, rv, transmittedCount));

    MOZ_ASSERT(rv != NS_BASE_STREAM_WOULD_BLOCK,
               "inconsistent stream commitment result");

    if (NS_FAILED(rv))
      return rv;

    MOZ_ASSERT(transmittedCount == mTxStreamFrameSize,
               "inconsistent stream commitment count");

    Http2Session::LogIO(mSession, this, "Writing from Transaction Buffer",
                         buf, transmittedCount);

    *countUsed += mTxStreamFrameSize;
  }

  mSession->FlushOutputQueue();

  // calling this will trigger waiting_for if mRequestBodyLenRemaining is 0
  UpdateTransportSendEvents(mTxInlineFrameUsed + mTxStreamFrameSize);

  mTxInlineFrameUsed = 0;
  mTxStreamFrameSize = 0;

  return NS_OK;
}

void
Http2Stream::ChangeState(enum upstreamStateType newState)
{
  LOG3(("Http2Stream::ChangeState() %p from %X to %X",
        this, mUpstreamState, newState));
  mUpstreamState = newState;
}

void
Http2Stream::GenerateDataFrameHeader(uint32_t dataLength, bool lastFrame)
{
  LOG3(("Http2Stream::GenerateDataFrameHeader %p len=%d last=%d",
        this, dataLength, lastFrame));

  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);
  MOZ_ASSERT(!mTxInlineFrameUsed, "inline frame not empty");
  MOZ_ASSERT(!mTxStreamFrameSize, "stream frame not empty");

  uint8_t frameFlags = 0;
  if (lastFrame) {
    frameFlags |= Http2Session::kFlag_END_STREAM;
    if (dataLength)
      SetSentFin(true);
  }

  mSession->CreateFrameHeader(mTxInlineFrame.get(),
                              dataLength,
                              Http2Session::FRAME_TYPE_DATA,
                              frameFlags, mStreamID);

  mTxInlineFrameUsed = 8;
  mTxStreamFrameSize = dataLength;
}

// ConvertHeaders is used to convert the response headers
// into HTTP/1 format and report some telemetry
nsresult
Http2Stream::ConvertResponseHeaders(Http2Decompressor *decompressor,
                                    nsACString &aHeadersIn,
                                    nsACString &aHeadersOut)
{
  aHeadersOut.Truncate();
  aHeadersOut.SetCapacity(aHeadersIn.Length() + 512);

  nsresult rv =
    decompressor->DecodeHeaderBlock(reinterpret_cast<const uint8_t *>(aHeadersIn.BeginReading()),
                                    aHeadersIn.Length(),
                                    aHeadersOut);
  if (NS_FAILED(rv)) {
    LOG3(("Http2Stream::ConvertHeaders %p decode Error\n", this));
    return NS_ERROR_ILLEGAL_VALUE;
  }

  nsAutoCString status;
  decompressor->GetStatus(status);
  if (status.IsEmpty()) {
    LOG3(("Http2Stream::ConvertHeaders %p Error - no status\n", this));
    return NS_ERROR_ILLEGAL_VALUE;
  }

  if (mIsTunnel) {
    nsresult errcode;
    if (status.ToInteger(&errcode) != 200) {
      LOG3(("Http2Stream %p Tunnel not 200", this));
      return NS_ERROR_ABORT;
    }
  }

  if (aHeadersIn.Length() && aHeadersOut.Length()) {
    Telemetry::Accumulate(Telemetry::SPDY_SYN_REPLY_SIZE, aHeadersIn.Length());
    uint32_t ratio =
      aHeadersIn.Length() * 100 / aHeadersOut.Length();
    Telemetry::Accumulate(Telemetry::SPDY_SYN_REPLY_RATIO, ratio);
  }

  aHeadersIn.Truncate();
  aHeadersOut.Append("X-Firefox-Spdy: " NS_HTTP2_DRAFT_TOKEN "\r\n\r\n");
  LOG (("decoded response headers are:\n%s", aHeadersOut.BeginReading()));
  if (mIsTunnel) {
    aHeadersOut.Truncate();
    LOG(("SpdyStream3::ConvertHeaders %p 0x%X headers removed for tunnel\n",
         this, mStreamID));
  }
  return NS_OK;
}

// ConvertHeaders is used to convert the response headers
// into HTTP/1 format and report some telemetry
nsresult
Http2Stream::ConvertPushHeaders(Http2Decompressor *decompressor,
                                nsACString &aHeadersIn,
                                nsACString &aHeadersOut)
{
  aHeadersOut.Truncate();
  nsresult rv =
    decompressor->DecodeHeaderBlock(reinterpret_cast<const uint8_t *>(aHeadersIn.BeginReading()),
                                    aHeadersIn.Length(),
                                    aHeadersOut);
  if (NS_FAILED(rv)) {
    LOG3(("Http2Stream::ConvertPushHeaders %p Error\n", this));
    return NS_ERROR_ILLEGAL_VALUE;
  }

  nsCString method;
  decompressor->GetHost(mHeaderHost);
  decompressor->GetScheme(mHeaderScheme);
  decompressor->GetPath(mHeaderPath);

  if (mHeaderHost.IsEmpty() || mHeaderScheme.IsEmpty() || mHeaderPath.IsEmpty()) {
    LOG3(("Http2Stream::ConvertPushHeaders %p Error - missing required "
          "host=%s scheme=%s path=%s\n", this, mHeaderHost.get(), mHeaderScheme.get(),
          mHeaderPath.get()));
    return NS_ERROR_ILLEGAL_VALUE;
  }

  decompressor->GetMethod(method);
  if (!method.EqualsLiteral("GET")) {
    LOG3(("Http2Stream::ConvertPushHeaders %p Error - method not supported: %s\n",
          this, method.get()));
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  aHeadersIn.Truncate();
  LOG (("decoded push headers are:\n%s", aHeadersOut.BeginReading()));
  return NS_OK;
}

void
Http2Stream::Close(nsresult reason)
{
  mTransaction->Close(reason);
}

void
Http2Stream::SetAllHeadersReceived()
{
  if (mAllHeadersReceived) {
    return;
  }

  mAllHeadersReceived = 1;
  if (mIsTunnel) {
    MapStreamToHttpConnection();
    ClearTransactionsBlockedOnTunnel();
  }
  return;
}

bool
Http2Stream::AllowFlowControlledWrite()
{
  return (mSession->ServerSessionWindow() > 0) && (mServerReceiveWindow > 0);
}

void
Http2Stream::UpdateServerReceiveWindow(int32_t delta)
{
  mServerReceiveWindow += delta;

  if (mBlockedOnRwin && AllowFlowControlledWrite()) {
    LOG3(("Http2Stream::UpdateServerReceived UnPause %p 0x%X "
          "Open stream window\n", this, mStreamID));
    mSession->TransactionHasDataToWrite(this);  }
}

void
Http2Stream::SetPriority(uint32_t newPriority)
{
  int32_t httpPriority = static_cast<int32_t>(newPriority);
  if (httpPriority > kWorstPriority) {
    httpPriority = kWorstPriority;
  } else if (httpPriority < kBestPriority) {
    httpPriority = kBestPriority;
  }
  mPriority = static_cast<uint32_t>(httpPriority);
  mPriorityWeight = (nsISupportsPriority::PRIORITY_LOWEST + 1) -
    (httpPriority - kNormalPriority);
}

void
Http2Stream::SetPriorityDependency(uint32_t newDependency, uint8_t newWeight,
                                   bool exclusive)
{
  // XXX - we ignore this for now... why is the server sending priority frames?!
  LOG3(("Http2Stream::SetPriorityDependency %p 0x%X received dependency=0x%X "
        "weight=%u exclusive=%d", this, mStreamID, newDependency, newWeight,
        exclusive));
}

void
Http2Stream::SetRecvdFin(bool aStatus)
{
  mRecvdFin = aStatus ? 1 : 0;
  if (!aStatus)
    return;

  if (mState == OPEN || mState == RESERVED_BY_REMOTE) {
    mState = CLOSED_BY_REMOTE;
  } else if (mState == CLOSED_BY_LOCAL) {
    mState = CLOSED;
  }
}

void
Http2Stream::SetSentFin(bool aStatus)
{
  mSentFin = aStatus ? 1 : 0;
  if (!aStatus)
    return;

  if (mState == OPEN || mState == RESERVED_BY_REMOTE) {
    mState = CLOSED_BY_LOCAL;
  } else if (mState == CLOSED_BY_REMOTE) {
    mState = CLOSED;
  }
}

void
Http2Stream::SetRecvdReset(bool aStatus)
{
  mRecvdReset = aStatus ? 1 : 0;
  if (!aStatus)
    return;
  mState = CLOSED;
}

void
Http2Stream::SetSentReset(bool aStatus)
{
  mSentReset = aStatus ? 1 : 0;
  if (!aStatus)
    return;
  mState = CLOSED;
}

//-----------------------------------------------------------------------------
// nsAHttpSegmentReader
//-----------------------------------------------------------------------------

nsresult
Http2Stream::OnReadSegment(const char *buf,
                           uint32_t count,
                           uint32_t *countRead)
{
  LOG3(("Http2Stream::OnReadSegment %p count=%d state=%x",
        this, count, mUpstreamState));

  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);
  MOZ_ASSERT(mSegmentReader, "OnReadSegment with null mSegmentReader");

  nsresult rv = NS_ERROR_UNEXPECTED;
  uint32_t dataLength;

  switch (mUpstreamState) {
  case GENERATING_HEADERS:
    // The buffer is the HTTP request stream, including at least part of the
    // HTTP request header. This state's job is to build a HEADERS frame
    // from the header information. count is the number of http bytes available
    // (which may include more than the header), and in countRead we return
    // the number of those bytes that we consume (i.e. the portion that are
    // header bytes)

    rv = ParseHttpRequestHeaders(buf, count, countRead);
    if (NS_FAILED(rv))
      return rv;
    LOG3(("ParseHttpRequestHeaders %p used %d of %d. complete = %d",
          this, *countRead, count, mAllHeadersSent));
    if (mAllHeadersSent) {
      SetHTTPState(OPEN);
      AdjustInitialWindow();
      // This version of TransmitFrame cannot block
      rv = TransmitFrame(nullptr, nullptr, true);
      ChangeState(GENERATING_BODY);
      break;
    }
    MOZ_ASSERT(*countRead == count, "Header parsing not complete but unused data");
    break;

  case GENERATING_BODY:
    // if there is session flow control and either the stream window is active and
    // exhaused or the session window is exhausted then suspend
    if (!AllowFlowControlledWrite()) {
      *countRead = 0;
      LOG3(("Http2Stream this=%p, id 0x%X request body suspended because "
            "remote window is stream=%ld session=%ld.\n", this, mStreamID,
            mServerReceiveWindow, mSession->ServerSessionWindow()));
      mBlockedOnRwin = true;
      return NS_BASE_STREAM_WOULD_BLOCK;
    }
    mBlockedOnRwin = false;

    // The chunk is the smallest of: availableData, configured chunkSize,
    // stream window, session window, or 14 bit framing limit.
    // Its amazing we send anything at all.
    dataLength = std::min(count, mChunkSize);

    if (dataLength > Http2Session::kMaxFrameData)
      dataLength = Http2Session::kMaxFrameData;

    if (dataLength > mSession->ServerSessionWindow())
      dataLength = static_cast<uint32_t>(mSession->ServerSessionWindow());

    if (dataLength > mServerReceiveWindow)
      dataLength = static_cast<uint32_t>(mServerReceiveWindow);

    LOG3(("Http2Stream this=%p id 0x%X send calculation "
          "avail=%d chunksize=%d stream window=%d session window=%d "
          "max frame=%d USING=%d\n", this, mStreamID,
          count, mChunkSize, mServerReceiveWindow, mSession->ServerSessionWindow(),
          Http2Session::kMaxFrameData, dataLength));

    mSession->DecrementServerSessionWindow(dataLength);
    mServerReceiveWindow -= dataLength;

    LOG3(("Http2Stream %p id %x request len remaining %u, "
          "count avail %u, chunk used %u",
          this, mStreamID, mRequestBodyLenRemaining, count, dataLength));
    if (!dataLength && mRequestBodyLenRemaining) {
      return NS_BASE_STREAM_WOULD_BLOCK;
    }
    if (dataLength > mRequestBodyLenRemaining) {
      return NS_ERROR_UNEXPECTED;
    }
    mRequestBodyLenRemaining -= dataLength;
    GenerateDataFrameHeader(dataLength, !mRequestBodyLenRemaining);
    ChangeState(SENDING_BODY);
    // NO BREAK

  case SENDING_BODY:
    MOZ_ASSERT(mTxInlineFrameUsed, "OnReadSegment Send Data Header 0b");
    rv = TransmitFrame(buf, countRead, false);
    MOZ_ASSERT(NS_FAILED(rv) || !mTxInlineFrameUsed,
               "Transmit Frame should be all or nothing");

    LOG3(("TransmitFrame() rv=%x returning %d data bytes. "
          "Header is %d Body is %d.",
          rv, *countRead, mTxInlineFrameUsed, mTxStreamFrameSize));

    // normalize a partial write with a WOULD_BLOCK into just a partial write
    // as some code will take WOULD_BLOCK to mean an error with nothing
    // written (e.g. nsHttpTransaction::ReadRequestSegment()
    if (rv == NS_BASE_STREAM_WOULD_BLOCK && *countRead)
      rv = NS_OK;

    // If that frame was all sent, look for another one
    if (!mTxInlineFrameUsed)
      ChangeState(GENERATING_BODY);
    break;

  case SENDING_FIN_STREAM:
    MOZ_ASSERT(false, "resuming partial fin stream out of OnReadSegment");
    break;

  default:
    MOZ_ASSERT(false, "Http2Stream::OnReadSegment non-write state");
    break;
  }

  return rv;
}

//-----------------------------------------------------------------------------
// nsAHttpSegmentWriter
//-----------------------------------------------------------------------------

nsresult
Http2Stream::OnWriteSegment(char *buf,
                            uint32_t count,
                            uint32_t *countWritten)
{
  LOG3(("Http2Stream::OnWriteSegment %p count=%d state=%x 0x%X\n",
        this, count, mUpstreamState, mStreamID));

  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);
  MOZ_ASSERT(mSegmentWriter);

  if (!mPushSource)
    return mSegmentWriter->OnWriteSegment(buf, count, countWritten);

  nsresult rv;
  rv = mPushSource->GetBufferedData(buf, count, countWritten);
  if (NS_FAILED(rv))
    return rv;

  mSession->ConnectPushedStream(this);
  return NS_OK;
}

/// connect tunnels

void
Http2Stream::ClearTransactionsBlockedOnTunnel()
{
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);

  if (!mIsTunnel) {
    return;
  }
  gHttpHandler->ConnMgr()->ProcessPendingQ(mTransaction->ConnectionInfo());
}

void
Http2Stream::MapStreamToHttpConnection()
{
  nsRefPtr<SpdyConnectTransaction> qiTrans(mTransaction->QuerySpdyConnectTransaction());
  MOZ_ASSERT(qiTrans);
  qiTrans->MapStreamToHttpConnection(mSocketTransport,
                                     mTransaction->ConnectionInfo());
}

} // namespace mozilla::net
} // namespace mozilla

