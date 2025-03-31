/* -*- Mode: c++; c-basic-offset: 2; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WebExecutorSupport_h__
#define WebExecutorSupport_h__

#include "mozilla/java/GeckoWebExecutorNatives.h"
#include "mozilla/java/GeckoResultWrappers.h"
#include "mozilla/java/WebRequestWrappers.h"

#if defined(ENABLE_TESTS)
#  include "nsIOhttpClientTest.h"
#endif  // defined(ENABLE_TESTS)

class nsIChannel;

namespace mozilla {
namespace widget {

class WebExecutorSupport final
    : public java::GeckoWebExecutor::Natives<WebExecutorSupport> {
 public:
  static void Fetch(jni::Object::Param request, int32_t flags,
                    jni::Object::Param result);
  static void Resolve(jni::String::Param aUri, jni::Object::Param result);

  static void CompleteWithError(java::GeckoResult::Param aResult,
                                nsresult aStatus, nsIChannel* aChannel);

  static void CompleteWithError(java::GeckoResult::Param aResult,
                                nsresult aStatus) {
    CompleteWithError(aResult, aStatus, nullptr);
  }

  static nsresult CreateStreamLoader(java::WebRequest::Param aRequest,
                                     int32_t aFlags,
                                     java::GeckoResult::Param aResult);

  // Ohttp requires fetching config first, so we need to queue the request if
  // the config is not fetched yet.
  static nsresult PerformOrQueueOhttpRequest(java::WebRequest::Param aRequest,
                                             int32_t aFlags,
                                             java::GeckoResult::Param aResult,
                                             bool bypassConfigCache = false);
#if defined(ENABLE_TESTS)
  // Used for testing OHTTP. Porting all of the OHTTP server code to Java would
  // be quite a bit of work, so we're just going to test it in JS.
  static void TestOhttp(const nsACString& url, const nsACString& method,
                        const nsACString& body,
                        const nsTArray<nsCString>& headerKeys,
                        const nsTArray<nsCString>& headerValues,
                        ohttpClientTestCallback* callback);
#endif  // defined(ENABLE_TESTS)
};

}  // namespace widget
}  // namespace mozilla

#endif  // WebExecutorSupport_h__
