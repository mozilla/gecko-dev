/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_WEBRTC_LIBWEBRTCGLUE_WEBRTCENVIRONMENTWRAPPER_H_
#define DOM_MEDIA_WEBRTC_LIBWEBRTCGLUE_WEBRTCENVIRONMENTWRAPPER_H_

#include "domstubs.h"
#include "jsapi/PeerConnectionCtx.h"  // for MozTrialsConfig
#include "nsISupportsImpl.h"
#include "SystemTime.h"

// libwebrtc includes
#include "api/environment/environment.h"
#include "api/rtc_event_log/rtc_event_log.h"

namespace mozilla {

class WebrtcEnvironmentWrapper {
 public:
  static RefPtr<WebrtcEnvironmentWrapper> Create(
      const dom::RTCStatsTimestampMaker& aTimestampMaker);

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(WebrtcEnvironmentWrapper)

  WebrtcEnvironmentWrapper(const WebrtcEnvironmentWrapper&) = delete;
  WebrtcEnvironmentWrapper& operator=(const WebrtcEnvironmentWrapper&) = delete;
  WebrtcEnvironmentWrapper(WebrtcEnvironmentWrapper&&) = delete;
  WebrtcEnvironmentWrapper& operator=(WebrtcEnvironmentWrapper&&) = delete;

  const webrtc::Environment& Environment() const { return mEnv; }

 protected:
  virtual ~WebrtcEnvironmentWrapper() = default;

  WebrtcEnvironmentWrapper(
      UniquePtr<webrtc::RtcEventLog>&& aEventLog,
      UniquePtr<webrtc::TaskQueueFactory>&& aTaskQueueFactory,
      UniquePtr<webrtc::FieldTrialsView>&& aTrials,
      const dom::RTCStatsTimestampMaker& aTimestampMaker);

  const UniquePtr<webrtc::RtcEventLog> mEventLog;
  const UniquePtr<webrtc::TaskQueueFactory> mTaskQueueFactory;
  const UniquePtr<webrtc::FieldTrialsView> mTrials;
  RTCStatsTimestampMakerRealtimeClock mClock;
  webrtc::Environment mEnv;
};

}  // namespace mozilla

#endif
