/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// HttpLog.h should generally be included first
#include "HttpLog.h"

#include "Http2WebTransportSession.h"
#include "Http2Session.h"

namespace mozilla::net {

Http2WebTransportSession::Http2WebTransportSession(
    Http2Session* aSession, int32_t aPriority, uint64_t aBcId,
    nsHttpConnectionInfo* aConnectionInfo)
    : Http2StreamTunnel(aSession, aPriority, aBcId, aConnectionInfo) {
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
  return NS_OK;
}

}  // namespace mozilla::net
