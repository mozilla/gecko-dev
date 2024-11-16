/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_VIDEO_VIDEO_STREAM_ENCODER_SETTINGS_H_
#define API_VIDEO_VIDEO_STREAM_ENCODER_SETTINGS_H_


#include "api/video/video_bitrate_allocator_factory.h"
#include "api/video_codecs/sdp_video_format.h"
#include "api/video_codecs/video_encoder.h"
#include "api/video_codecs/video_encoder_factory.h"

namespace webrtc {

class EncoderSwitchRequestCallback {
 public:
  virtual ~EncoderSwitchRequestCallback() {}

  // Requests switch to next negotiated encoder.
  virtual void RequestEncoderFallback() = 0;

  // Requests switch to a specific encoder. If the encoder is not available and
  // `allow_default_fallback` is `true` the default fallback is invoked.
  virtual void RequestEncoderSwitch(const SdpVideoFormat& format,
                                    bool allow_default_fallback) = 0;
};

struct VideoStreamEncoderSettings {
  explicit VideoStreamEncoderSettings(
      const VideoEncoder::Capabilities& capabilities)
      : capabilities(capabilities) {}

  // Enables the new method to estimate the cpu load from encoding, used for
  // cpu adaptation.
  bool experiment_cpu_load_estimator = false;

  // Ownership stays with WebrtcVideoEngine (delegated from PeerConnection).
  VideoEncoderFactory* encoder_factory = nullptr;

  // Requests the WebRtcVideoChannel to perform a codec switch.
  EncoderSwitchRequestCallback* encoder_switch_request_callback = nullptr;

  // Ownership stays with WebrtcVideoEngine (delegated from PeerConnection).
  VideoBitrateAllocatorFactory* bitrate_allocator_factory = nullptr;

  // Negotiated capabilities which the VideoEncoder may expect the other
  // side to use.
  VideoEncoder::Capabilities capabilities;

  // Enables the frame instrumentation generator that is required for automatic
  // corruption detection.
  bool enable_frame_instrumentation_generator = false;

  // According to spec, `requested_resolution` (called scaleResolutionDownTo in
  // the web API) MUST NOT modify the aspect ratio of the frame, e.g. a 1280x720
  // frame being restricted to maxWidth by maxHeight 720x720 should result in
  // 720x405. In order for this to work, the video source must not adapt the
  // input frame to the value of `requested_resolution`, as that would result in
  // stretched 720x720.
  //
  // In order not to break backwards compatibility with C++ usage of this API,
  // when `use_standard_requested_resolution` is false, the
  // `requested_resolution` is signaled back to the video source. This works as
  // long as the aspect ratio is the same, but breaks the web API use case.
  //
  // Spec:
  // https://w3c.github.io/webrtc-extensions/#dom-rtcrtpencodingparameters-scaleresolutiondownto
  //
  // TODO(https://crbug.com/webrtc/366284861): Change the default to true,
  // delete this flag and any code handling the legacy behavior.
  bool use_standard_requested_resolution = false;
};

}  // namespace webrtc

#endif  // API_VIDEO_VIDEO_STREAM_ENCODER_SETTINGS_H_
