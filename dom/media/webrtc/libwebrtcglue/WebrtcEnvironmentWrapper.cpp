/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "WebrtcEnvironmentWrapper.h"

#include "WebrtcTaskQueueWrapper.h"

// libwebrtc includes
#include "api/environment/environment_factory.h"

namespace mozilla {

/* static */ RefPtr<WebrtcEnvironmentWrapper> WebrtcEnvironmentWrapper::Create(
    const dom::RTCStatsTimestampMaker& aTimestampMaker) {
  RefPtr<WebrtcEnvironmentWrapper> wrapper = new WebrtcEnvironmentWrapper(
      MakeUnique<webrtc::RtcEventLogNull>(),
      MakeUnique<SharedThreadPoolWebRtcTaskQueueFactory>(),
      WrapUnique(new webrtc::MozTrialsConfig()), aTimestampMaker);

  return wrapper;
}

WebrtcEnvironmentWrapper::WebrtcEnvironmentWrapper(
    UniquePtr<webrtc::RtcEventLog>&& aEventLog,
    UniquePtr<webrtc::TaskQueueFactory>&& aTaskQueueFactory,
    UniquePtr<webrtc::FieldTrialsView>&& aTrials,
    const dom::RTCStatsTimestampMaker& aTimestampMaker)
    : mEventLog(std::move(aEventLog)),
      mTaskQueueFactory(std::move(aTaskQueueFactory)),
      mTrials(std::move(aTrials)),
      mClock(aTimestampMaker),
      mEnv(webrtc::CreateEnvironment(mEventLog.get(),
                                     mClock.GetRealTimeClockRaw(),
                                     mTaskQueueFactory.get(), mTrials.get())) {}

}  // namespace mozilla
