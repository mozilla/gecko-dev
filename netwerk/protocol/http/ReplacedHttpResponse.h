/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsString.h"
#include "nsHttpHeaderArray.h"
#include "nsIReplacedHttpResponse.h"

namespace mozilla::net {

// A ReplaceHttpResponse holds data which will be used to override the response
// of a http channel before the request is sent over the network.
// See nsIHttpChannelInternal::setResponseOverride to override the response.
class ReplacedHttpResponse : nsIReplacedHttpResponse {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIREPLACEDHTTPRESPONSE

 private:
  virtual ~ReplacedHttpResponse() = default;
  uint16_t mResponseStatus = 0;
  nsCString mResponseStatusText;
  nsCString mResponseBody;
  nsHttpHeaderArray mResponseHeaders;
};

}  // namespace mozilla::net
