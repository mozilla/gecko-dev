/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "video_capture_fake.h"

#include "device_info_fake.h"
#include "FakeVideoSource.h"

using mozilla::FakeVideoSource;
using mozilla::MakeRefPtr;
using mozilla::TimeDuration;

namespace webrtc::videocapturemodule {
rtc::scoped_refptr<webrtc::VideoCaptureModule> VideoCaptureFake::Create(
    nsISerialEventTarget* aTarget) {
  return rtc::make_ref_counted<VideoCaptureFake>(aTarget);
}

VideoCaptureFake::VideoCaptureFake(nsISerialEventTarget* aTarget)
    : mSource(MakeRefPtr<FakeVideoSource>(aTarget)) {
  size_t len = strlen(DeviceInfoFake::kId);
  _deviceUniqueId = new (std::nothrow) char[len + 1];
  if (_deviceUniqueId) {
    memcpy(_deviceUniqueId, DeviceInfoFake::kId, len + 1);
  }
}

int32_t VideoCaptureFake::StartCapture(
    const VideoCaptureCapability& aCapability) {
  return mSource->StartCapture(
      aCapability.width, aCapability.height,
      TimeDuration::FromSeconds(1.0 / aCapability.maxFPS));
}

int32_t VideoCaptureFake::StopCapture() { return mSource->StopCapture(); }

bool VideoCaptureFake::CaptureStarted() { return mSource->CaptureStarted(); }

int32_t VideoCaptureFake::CaptureSettings(VideoCaptureCapability& aSettings) {
  return {};
}

void VideoCaptureFake::SetTrackingId(uint32_t aTrackingIdProcId) {
  mSource->SetTrackingId(aTrackingIdProcId);
}

}  // namespace webrtc::videocapturemodule
