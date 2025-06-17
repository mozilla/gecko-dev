/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// HttpLog.h should generally be included first
#include "HttpLog.h"

#include "Http2Stream.h"
#include "nsHttp.h"
#include "nsHttpConnectionInfo.h"
#include "nsHttpRequestHead.h"
#include "nsISocketTransport.h"
#include "Http2Session.h"
#include "nsIRequestContext.h"
#include "nsHttpTransaction.h"
#include "nsSocketTransportService2.h"
#include "mozilla/glean/NetwerkProtocolHttpMetrics.h"

namespace mozilla::net {

Http2Stream::Http2Stream(nsAHttpTransaction* httpTransaction,
                         Http2Session* session, int32_t priority, uint64_t bcId)
    : Http2StreamBase((httpTransaction->QueryHttpTransaction())
                          ? httpTransaction->QueryHttpTransaction()->BrowserId()
                          : 0,
                      session, priority, bcId),
      mTransaction(httpTransaction) {
  LOG1(("Http2Stream::Http2Stream %p trans=%p", this, httpTransaction));
}

Http2Stream::~Http2Stream() {}

void Http2Stream::CloseStream(nsresult reason) {
  mTransaction->Close(reason);
  mSession = nullptr;
}

uint32_t Http2Stream::GetWireStreamId() {
  // >0 even numbered IDs are pushed streams.
  // odd numbered IDs are pulled streams.
  // 0 is the sink for a pushed stream.
  if (!mStreamID) {
    return 0;
  }

  if (mState == RESERVED_BY_REMOTE) {
    // h2-14 prevents sending a window update in this state
    return 0;
  }
  return mStreamID;
}

nsresult Http2Stream::OnWriteSegment(char* buf, uint32_t count,
                                     uint32_t* countWritten) {
  LOG3(("Http2Stream::OnWriteSegment %p count=%d state=%x 0x%X\n", this, count,
        mUpstreamState, mStreamID));

  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  MOZ_ASSERT(mSegmentWriter);

  return Http2StreamBase::OnWriteSegment(buf, count, countWritten);
}

nsresult Http2Stream::CallToReadData(uint32_t count, uint32_t* countRead) {
  return mTransaction->ReadSegments(this, count, countRead);
}

nsresult Http2Stream::CallToWriteData(uint32_t count, uint32_t* countWritten) {
  return mTransaction->WriteSegments(this, count, countWritten);
}

// This is really a headers frame, but open is pretty clear from a workflow pov
nsresult Http2Stream::GenerateHeaders(nsCString& aCompressedData,
                                      uint8_t& firstFrameFlags) {
  nsHttpRequestHead* head = mTransaction->RequestHead();
  nsAutoCString requestURI;
  head->RequestURI(requestURI);
  RefPtr<Http2Session> session = Session();
  LOG3(("Http2Stream %p Stream ID 0x%X [session=%p] for URI %s\n", this,
        mStreamID, session.get(), requestURI.get()));

  nsAutoCString authorityHeader;
  nsresult rv = head->GetHeader(nsHttp::Host, authorityHeader);
  if (NS_FAILED(rv)) {
    MOZ_ASSERT(false);
    return rv;
  }

  nsDependentCString scheme(head->IsHTTPS() ? "https" : "http");

  nsAutoCString method;
  nsAutoCString path;
  head->Method(method);
  head->Path(path);

  bool mayAddTEHeader = true;
  nsAutoCString teHeader;
  rv = head->GetHeader(nsHttp::TE, teHeader);
  if (NS_SUCCEEDED(rv) && teHeader.Equals("moz_no_te_trailers"_ns)) {
    // This is a hack that allows extensions or devtools to
    // skip adding the TE header. This is necessary to be able to
    // ship interventions when websites misbehave when the TE:trailers
    // header is sent (see bug 1954533).
    mayAddTEHeader = false;
  }

  rv = session->Compressor()->EncodeHeaderBlock(
      mFlatHttpRequestHeaders, method, path, authorityHeader, scheme,
      EmptyCString(), false, aCompressedData, mayAddTEHeader);
  NS_ENSURE_SUCCESS(rv, rv);

  int64_t clVal = session->Compressor()->GetParsedContentLength();
  if (clVal != -1) {
    mRequestBodyLenRemaining = clVal;
  }

  // Determine whether to put the fin bit on the header frame or whether
  // to wait for a data packet to put it on.

  if (head->IsGet() || head->IsHead()) {
    // for GET and HEAD place the fin bit right on the
    // header packet
    firstFrameFlags |= Http2Session::kFlag_END_STREAM;
  } else if (head->IsPost() || head->IsPut() || head->IsConnect()) {
    // place fin in a data frame even for 0 length messages for iterop
  } else if (!mRequestBodyLenRemaining) {
    // for other HTTP extension methods, rely on the content-length
    // to determine whether or not to put fin on headers
    firstFrameFlags |= Http2Session::kFlag_END_STREAM;
  }

  // The size of the input headers is approximate
  uint32_t ratio =
      aCompressedData.Length() * 100 /
      (11 + requestURI.Length() + mFlatHttpRequestHeaders.Length());

  glean::spdy::syn_ratio.AccumulateSingleSample(ratio);

  return NS_OK;
}

}  // namespace mozilla::net
