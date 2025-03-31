/* -*- Mode: c++; c-basic-offset: 2; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef OhttpHelper_h__
#define OhttpHelper_h__

#include "mozilla/java/GeckoResultWrappers.h"
#include "mozilla/java/WebRequestWrappers.h"

#include "mozilla/StaticPtr.h"

#include "nsIChannel.h"
#include "nsISupports.h"

namespace mozilla::widget {
/*
 * Used just for packing OHTTP related functionality.
 */
class OhttpHelper final {
 public:
  class OhttpRequest final {
   public:
    NS_INLINE_DECL_REFCOUNTING(OhttpRequest)

    java::WebRequest::GlobalRef request;
    int32_t flags = 0;
    java::GeckoResult::GlobalRef result;

   private:
    ~OhttpRequest() = default;
  };

  static nsresult EnsurePrefsRead();

  static void QueueOhttpRequest(OhttpRequest* aRequest);

  static bool IsConfigReady() {
    return sInitializationBitset & InitializationBit::CONFIG_FETCHED;
  }

  static nsresult CreateChannel(java::WebRequest::Param aRequest, nsIURI* aUri,
                                nsIChannel** outChannel);

  static nsresult FetchConfigAndFulfillRequests();

  static void FailRequests(nsresult aStatus);

 private:
  static nsresult CreateConfigRequest(nsIChannel** outChannel);

  enum InitializationBit : uint8_t {
    PREFS = 1 << 0,
    PENDING_REQUESTS = 1 << 1,
    CONFIG_FETCHING = 1 << 2,
    CONFIG_FETCHED = 1 << 3,
  };

  // See InitializationBit enum for bit values.
  static inline uint8_t sInitializationBitset = 0;

  static inline StaticAutoPtr<nsTArray<uint8_t>> sConfigData;
  MOZ_CONSTINIT static inline nsCString sConfigUrl;
  MOZ_CONSTINIT static inline nsCString sRelayUrl;

  static inline StaticAutoPtr<nsTArray<RefPtr<OhttpRequest>>> sPendingRequests;
};

}  // namespace mozilla::widget

#endif  // OhttpHelper_h__
