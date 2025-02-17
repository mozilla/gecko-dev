/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_Http2WebTransportSession_h
#define mozilla_net_Http2WebTransportSession_h

#include "Http2StreamTunnel.h"

namespace mozilla::net {

class Http2WebTransportSession : public Http2StreamTunnel {
 public:
  Http2WebTransportSession(Http2Session* aSession, int32_t aPriority,
                           uint64_t aBcId,
                           nsHttpConnectionInfo* aConnectionInfo);
  void CloseStream(nsresult aReason) override;

 protected:
  virtual ~Http2WebTransportSession();
  nsresult GenerateHeaders(nsCString& aCompressedData,
                           uint8_t& aFirstFrameFlags) override;
};

}  // namespace mozilla::net

#endif  // mozilla_net_Http2WebTransportSession_h
