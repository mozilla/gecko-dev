/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#include "webrtc/modules/video_capture/ios/device_info_ios_objc.h"
#include "webrtc/modules/video_capture/ios/rtc_video_capture_ios_objc.h"
#include "webrtc/system_wrappers/interface/ref_count.h"
#include "webrtc/system_wrappers/interface/scoped_refptr.h"
#include "webrtc/system_wrappers/interface/trace.h"

using namespace webrtc;
using namespace videocapturemodule;

VideoCaptureModule* VideoCaptureImpl::Create(const int32_t capture_id,
                                             const char* deviceUniqueIdUTF8) {
  return VideoCaptureIos::Create(capture_id, deviceUniqueIdUTF8);
}

VideoCaptureIos::VideoCaptureIos(const int32_t capture_id)
    : VideoCaptureImpl(capture_id), is_capturing_(false), id_(capture_id) {
  capability_.width = kDefaultWidth;
  capability_.height = kDefaultHeight;
  capability_.maxFPS = kDefaultFrameRate;
  capture_device_ = nil;
}

VideoCaptureIos::~VideoCaptureIos() {
  if (is_capturing_) {
    [capture_device_ stopCapture];
    capture_device_ = nil;
  }
}

VideoCaptureModule* VideoCaptureIos::Create(const int32_t capture_id,
                                            const char* deviceUniqueIdUTF8) {
  if (!deviceUniqueIdUTF8[0]) {
    return NULL;
  }

  RefCountImpl<VideoCaptureIos>* capture_module =
      new RefCountImpl<VideoCaptureIos>(capture_id);

  const int32_t name_length = strlen(deviceUniqueIdUTF8);
  if (name_length > kVideoCaptureUniqueNameLength)
    return NULL;

  capture_module->_deviceUniqueId = new char[name_length + 1];
  strncpy(capture_module->_deviceUniqueId, deviceUniqueIdUTF8, name_length + 1);
  capture_module->_deviceUniqueId[name_length] = '\0';

  capture_module->capture_device_ =
      [[RTCVideoCaptureIosObjC alloc] initWithOwner:capture_module
                                          captureId:capture_module->id_];
  if (!capture_module->capture_device_) {
    return NULL;
  }

  if (![capture_module->capture_device_ setCaptureDeviceByUniqueId:[
              [NSString alloc] initWithCString:deviceUniqueIdUTF8
                                      encoding:NSUTF8StringEncoding]]) {
    return NULL;
  }
  return capture_module;
}

int32_t VideoCaptureIos::StartCapture(
    const VideoCaptureCapability& capability) {
  capability_ = capability;

  if (![capture_device_ startCaptureWithCapability:capability]) {
    return -1;
  }

  is_capturing_ = true;

  return 0;
}

int32_t VideoCaptureIos::StopCapture() {
  if (![capture_device_ stopCapture]) {
    return -1;
  }

  is_capturing_ = false;
  return 0;
}

bool VideoCaptureIos::CaptureStarted() { return is_capturing_; }

int32_t VideoCaptureIos::CaptureSettings(VideoCaptureCapability& settings) {
  settings = capability_;
  settings.rawType = kVideoNV12;
  return 0;
}
