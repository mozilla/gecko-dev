/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ReplacedHttpResponse.h"

namespace mozilla::net {

NS_IMPL_ISUPPORTS(ReplacedHttpResponse, nsIReplacedHttpResponse)

NS_IMETHODIMP
ReplacedHttpResponse::Init() { return NS_OK; }

NS_IMETHODIMP
ReplacedHttpResponse::GetResponseStatus(uint32_t* aResponseStatus) {
  *aResponseStatus = mResponseStatus;
  return NS_OK;
}

NS_IMETHODIMP
ReplacedHttpResponse::SetResponseStatus(uint32_t aValue) {
  mResponseStatus = aValue;
  return NS_OK;
}

NS_IMETHODIMP
ReplacedHttpResponse::GetResponseStatusText(nsACString& aResponseStatusText) {
  aResponseStatusText.Assign(mResponseStatusText);
  return NS_OK;
}

NS_IMETHODIMP
ReplacedHttpResponse::SetResponseStatusText(
    const nsACString& aResponseStatusText) {
  mResponseStatusText.Assign(aResponseStatusText);
  return NS_OK;
}

NS_IMETHODIMP
ReplacedHttpResponse::GetResponseBody(nsACString& aResponseBody) {
  aResponseBody.Assign(mResponseBody);
  return NS_OK;
}

NS_IMETHODIMP
ReplacedHttpResponse::SetResponseBody(const nsACString& aResponseBody) {
  mResponseBody.Assign(aResponseBody);
  return NS_OK;
}

NS_IMETHODIMP
ReplacedHttpResponse::SetResponseHeader(const nsACString& header,
                                        const nsACString& value, bool merge) {
  return mResponseHeaders.SetHeader(header, value, merge,
                                    nsHttpHeaderArray::eVarietyResponse);
}

NS_IMETHODIMP
ReplacedHttpResponse::VisitResponseHeaders(nsIHttpHeaderVisitor* visitor) {
  return mResponseHeaders.VisitHeaders(visitor);
}
}  // namespace mozilla::net
