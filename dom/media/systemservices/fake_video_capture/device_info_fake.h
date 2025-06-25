/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_SYSTEMSERVICES_FAKE_VIDEO_CAPTURE_DEVICE_INFO_FAKE_H_
#define DOM_MEDIA_SYSTEMSERVICES_FAKE_VIDEO_CAPTURE_DEVICE_INFO_FAKE_H_

#include "modules/video_capture/device_info_impl.h"

namespace webrtc::videocapturemodule {

/**
 * DeviceInfo implementation for the MediaEngineFakeVideoSource, so it can be
 * used in place of a real backend, allowing to exercise
 * PCameras/VideoEngine/CaptureCapabilities code without needing a real device
 * on a given platform.
 */
class DeviceInfoFake : public DeviceInfoImpl {
 public:
  ~DeviceInfoFake() override = default;

  // Implementation of DeviceInfoImpl.
  int32_t Init() override { return 0; }
  uint32_t NumberOfDevices() override { return 1; }
  int32_t GetDeviceName(uint32_t aDeviceNumber, char* aDeviceNameUTF8,
                        uint32_t aDeviceNameLength, char* aDeviceUniqueIdUTF8,
                        uint32_t aDeviceUniqueIdUTF8Length,
                        char* aProductUniqueIdUTF8 = nullptr,
                        uint32_t aProductUniqueIdUTF8Length = 0,
                        pid_t* aPid = nullptr,
                        bool* deviceIsPlaceholder = 0) override;
  int32_t NumberOfCapabilities(const char* aDeviceUniqueIdUTF8) override;
  int32_t GetCapability(const char* aDeviceUniqueIdUTF8,
                        const uint32_t aDeviceCapabilityNumber,
                        VideoCaptureCapability& aCapability) override;
  int32_t DisplayCaptureSettingsDialogBox(const char* aDeviceUniqueIdUTF8,
                                          const char* aDialogTitleUTF8,
                                          void* aParentWindow,
                                          uint32_t aPositionX,
                                          uint32_t aPositionY) override {
    return -1;
  }
  int32_t CreateCapabilityMap(const char* aDeviceUniqueIdUTF8) override {
    return -1;
  }

  static constexpr const char* kName = "Fake Video Source";
  static constexpr const char* kId = "fake-video-source-0";
};

}  // namespace webrtc::videocapturemodule

#endif
