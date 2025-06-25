/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "device_info_fake.h"
#include <string.h>

namespace webrtc::videocapturemodule {

int32_t DeviceInfoFake::GetDeviceName(
    uint32_t aDeviceNumber, char* aDeviceNameUTF8, uint32_t aDeviceNameLength,
    char* aDeviceUniqueIdUTF8, uint32_t aDeviceUniqueIdUTF8Length,
    char* aProductUniqueIdUTF8, uint32_t aProductUniqueIdUTF8Length,
    pid_t* aPid, bool* deviceIsPlaceholder) {
  if (aDeviceNumber != 0) {
    return -1;
  }

  strncpy(aDeviceNameUTF8, kName, aDeviceNameLength - 1);
  aDeviceNameUTF8[aDeviceNameLength - 1] = '\0';

  strncpy(aDeviceUniqueIdUTF8, kId, aDeviceUniqueIdUTF8Length - 1);
  aDeviceUniqueIdUTF8[aDeviceUniqueIdUTF8Length - 1] = '\0';

  return 0;
}

int32_t DeviceInfoFake::NumberOfCapabilities(const char* aDeviceUniqueIdUTF8) {
  if (strcmp(aDeviceUniqueIdUTF8, kId) == 0) {
    return 2;
  }
  return 0;
}

int32_t DeviceInfoFake::GetCapability(const char* aDeviceUniqueIdUTF8,
                                      const uint32_t aDeviceCapabilityNumber,
                                      VideoCaptureCapability& aCapability) {
  if (strcmp(aDeviceUniqueIdUTF8, kId) != 0) {
    return -1;
  }

  switch (aDeviceCapabilityNumber) {
    case 0:
      aCapability.width = 640;
      aCapability.height = 480;
      aCapability.maxFPS = 30;
      aCapability.videoType = VideoType::kI420;
      return 0;
    case 1:
      aCapability.width = 1280;
      aCapability.height = 720;
      aCapability.maxFPS = 10;
      aCapability.videoType = VideoType::kI420;
      return 0;
    default:
      return -1;
  }
}
}  // namespace webrtc::videocapturemodule
