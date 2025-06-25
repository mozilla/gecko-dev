/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_SYSTEMSERVICES_FAKE_VIDEO_CAPTURE_DEVICE_INFO_FAKE_H_
#define DOM_MEDIA_SYSTEMSERVICES_FAKE_VIDEO_CAPTURE_DEVICE_INFO_FAKE_H_

namespace webrtc::videocapturemodule {

/**
 * DeviceInfoFake skeleton, soon to be updated.
 */
class DeviceInfoFake {
 public:
  static constexpr const char* kName = "Fake Video Source";
  static constexpr const char* kId = "fake-video-source-0";
};

}  // namespace webrtc::videocapturemodule

#endif
