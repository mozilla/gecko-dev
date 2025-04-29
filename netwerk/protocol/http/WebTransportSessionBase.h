/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_WebTransportSessionBase_h
#define mozilla_net_WebTransportSessionBase_h

#include <functional>

#include "nsISupportsImpl.h"
#include "nsTArray.h"

class WebTransportSessionEventListener;

namespace mozilla::net {

class WebTransportStreamBase;

class WebTransportSessionBase {
 public:
  NS_INLINE_DECL_PURE_VIRTUAL_REFCOUNTING

  WebTransportSessionBase() = default;

  void SetWebTransportSessionEventListener(
      WebTransportSessionEventListener* listener);

  virtual uint64_t GetStreamId() const = 0;
  virtual void CloseSession(uint32_t aStatus, const nsACString& aReason) = 0;
  virtual void GetMaxDatagramSize() = 0;
  virtual void SendDatagram(nsTArray<uint8_t>&& aData,
                            uint64_t aTrackingId) = 0;
  virtual void CreateOutgoingBidirectionalStream(
      std::function<void(Result<RefPtr<WebTransportStreamBase>, nsresult>&&)>&&
          aCallback) = 0;
  virtual void CreateOutgoingUnidirectionalStream(
      std::function<void(Result<RefPtr<WebTransportStreamBase>, nsresult>&&)>&&
          aCallback) = 0;
  virtual void StartReading() {}

 protected:
  virtual ~WebTransportSessionBase() = default;

  RefPtr<WebTransportSessionEventListener> mListener;
};

}  // namespace mozilla::net

#endif  // mozilla_net_WebTransportSessionBase_h
