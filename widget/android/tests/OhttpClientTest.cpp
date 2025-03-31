/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if defined(ENABLE_TESTS)

#  include "mozilla/widget/WebExecutorSupport.h"
#  include "mozilla/widget/OhttpClientTest.h"

namespace mozilla::widget {

NS_IMETHODIMP
OhttpClientTest::Fetch(const nsACString& url, const nsACString& method,
                       const nsACString& body,
                       const nsTArray<nsCString>& headerKeys,
                       const nsTArray<nsCString>& headerValues,
                       ohttpClientTestCallback* callback) {
  widget::WebExecutorSupport::TestOhttp(url, method, body, headerKeys,
                                        headerValues, callback);
  return NS_OK;
}

NS_IMPL_ISUPPORTS(OhttpClientTest, nsIOhttpClientTest)
}  // namespace mozilla::widget

#endif  // defined(ENABLE_TESTS)
