/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SDK_ANDROID_SRC_JNI_VIDEOENCODERFACTORYWRAPPER_H_
#define SDK_ANDROID_SRC_JNI_VIDEOENCODERFACTORYWRAPPER_H_

#include <jni.h>
#include <vector>

#include "api/video_codecs/sdp_video_format.h"
#include "api/video_codecs/video_encoder_factory.h"
#include "sdk/android/src/jni/jni_helpers.h"

namespace webrtc {
namespace jni {

// Wrapper for Java VideoEncoderFactory class. Delegates method calls through
// JNI and wraps the encoder inside VideoEncoderWrapper.
class VideoEncoderFactoryWrapper : public VideoEncoderFactory {
 public:
  VideoEncoderFactoryWrapper(JNIEnv* jni, jobject encoder_factory);

  std::unique_ptr<VideoEncoder> CreateVideoEncoder(
      const SdpVideoFormat& format) override;

  // Returns a list of supported codecs in order of preference.
  std::vector<SdpVideoFormat> GetSupportedFormats() const override {
    return supported_formats_;
  }

  CodecInfo QueryVideoEncoder(const SdpVideoFormat& format) const override;

 private:
  std::vector<SdpVideoFormat> GetSupportedFormats(JNIEnv* jni) const;

  const ScopedGlobalRef<jobject> encoder_factory_;

  jmethodID create_encoder_method_;
  jmethodID get_supported_codecs_method_;

  std::vector<SdpVideoFormat> supported_formats_;
};

}  // namespace jni
}  // namespace webrtc

#endif  // SDK_ANDROID_SRC_JNI_VIDEOENCODERFACTORYWRAPPER_H_
