/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_CacheablePerformanceTimingData_h
#define mozilla_dom_CacheablePerformanceTimingData_h

#include <stdint.h>

#include "nsCOMPtr.h"
#include "nsITimedChannel.h"
#include "nsStringFwd.h"
#include "nsTArray.h"

class nsIHttpChannel;

namespace mozilla::dom {

class IPCPerformanceTimingData;

// The subset of PerformanceResourceTiming data that can be cached for the
// subsequent requests from a compatible principal.
//
// This includes the data extracted from the server response, but doesn't
// include any timing data.
class CacheablePerformanceTimingData {
 public:
  CacheablePerformanceTimingData() = default;

  CacheablePerformanceTimingData(nsITimedChannel* aChannel,
                                 nsIHttpChannel* aHttpChannel);

 protected:
  explicit CacheablePerformanceTimingData(
      const CacheablePerformanceTimingData& aOther);

  explicit CacheablePerformanceTimingData(
      const IPCPerformanceTimingData& aIPCData);

 public:
  bool IsInitialized() const { return mInitialized; }

  const nsString& NextHopProtocol() const { return mNextHopProtocol; }

  uint64_t EncodedBodySize() const { return mEncodedBodySize; }

  uint64_t DecodedBodySize() const { return mDecodedBodySize; }

  uint16_t ResponseStatus() const { return mResponseStatus; }

  const nsString& ContentType() const { return mContentType; }

  uint8_t RedirectCountReal() const { return mRedirectCount; }
  uint8_t GetRedirectCount() const;

  bool AllRedirectsSameOrigin() const { return mAllRedirectsSameOrigin; }

  // Cached result of CheckBodyInfoAccessAllowedForOrigin.
  nsITimedChannel::BodyInfoAccess BodyInfoAccessAllowed() const {
    return mBodyInfoAccessAllowed;
  }

  // Cached result of CheckTimingAllowedForOrigin. If false, security sensitive
  // attributes of the resourceTiming object will be set to 0
  bool TimingAllowed() const { return mTimingAllowed; }

  nsTArray<nsCOMPtr<nsIServerTiming>> GetServerTiming();

 protected:
  void SetCacheablePropertiesFromHttpChannel(nsIHttpChannel* aHttpChannel,
                                             nsITimedChannel* aChannel);

 private:
  // Checks if the bodyInfo for Resource and Navigation Timing should be
  // kept opaque or exposed, per Fetch spec.
  nsITimedChannel::BodyInfoAccess CheckBodyInfoAccessAllowedForOrigin(
      nsIHttpChannel* aResourceChannel, nsITimedChannel* aChannel);

  // Checks if the resource is either same origin as the page that started
  // the load, or if the response contains the Timing-Allow-Origin header
  // with a value of * or matching the domain of the loading Principal
  bool CheckTimingAllowedForOrigin(nsIHttpChannel* aResourceChannel,
                                   nsITimedChannel* aChannel);

 protected:
  uint64_t mEncodedBodySize = 0;
  uint64_t mDecodedBodySize = 0;

  uint16_t mResponseStatus = 0;

  uint8_t mRedirectCount = 0;

  nsITimedChannel::BodyInfoAccess mBodyInfoAccessAllowed =
      nsITimedChannel::BodyInfoAccess::DISALLOWED;

  bool mAllRedirectsSameOrigin = false;

  bool mAllRedirectsPassTAO = false;

  bool mSecureConnection = false;

  bool mTimingAllowed = false;

  bool mInitialized = false;

  nsString mNextHopProtocol;
  nsString mContentType;

  nsTArray<nsCOMPtr<nsIServerTiming>> mServerTiming;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_CacheablePerformanceTimingData_h
