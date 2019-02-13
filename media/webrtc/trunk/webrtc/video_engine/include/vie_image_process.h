/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This sub-API supports the following functionalities:
//  - Effect filters
//  - Deflickering
//  - Color enhancement

#ifndef WEBRTC_VIDEO_ENGINE_INCLUDE_VIE_IMAGE_PROCESS_H_
#define WEBRTC_VIDEO_ENGINE_INCLUDE_VIE_IMAGE_PROCESS_H_

#include "webrtc/common_types.h"

namespace webrtc {

class EncodedImageCallback;
class I420FrameCallback;
class VideoEngine;

// This class declares an abstract interface for a user defined effect filter.
// The effect filter is registered using RegisterCaptureEffectFilter(),
// RegisterSendEffectFilter() or RegisterRenderEffectFilter() and deregistered
// with the corresponding deregister function.
class WEBRTC_DLLEXPORT ViEEffectFilter {
 public:
  // This method is called with an I420 video frame allowing the user to
  // modify the video frame.
  virtual int Transform(int size,
                        unsigned char* frame_buffer,
                        int64_t ntp_time_ms,
                        unsigned int timestamp,
                        unsigned int width,
                        unsigned int height) = 0;
 protected:
  ViEEffectFilter() {}
  virtual ~ViEEffectFilter() {}
};

class WEBRTC_DLLEXPORT ViEImageProcess {
 public:
  // Factory for the ViEImageProcess sub‐API and increases an internal
  // reference counter if successful. Returns NULL if the API is not supported
  // or if construction fails.
  static ViEImageProcess* GetInterface(VideoEngine* video_engine);

  // Releases the ViEImageProcess sub-API and decreases an internal reference
  // counter. Returns the new reference count. This value should be zero
  // for all sub-API:s before the VideoEngine object can be safely deleted.
  virtual int Release() = 0;

  // This function registers a EffectFilter to use for a specified capture
  // device.
  virtual int RegisterCaptureEffectFilter(const int capture_id,
                                          ViEEffectFilter& capture_filter) = 0;

  // This function deregisters a EffectFilter for a specified capture device.
  virtual int DeregisterCaptureEffectFilter(const int capture_id) = 0;

  // This function registers an EffectFilter to use for a specified channel.
  virtual int RegisterSendEffectFilter(const int video_channel,
                                       ViEEffectFilter& send_filter) = 0;

  // This function deregisters a send effect filter for a specified channel.
  virtual int DeregisterSendEffectFilter(const int video_channel) = 0;

  // This function registers a EffectFilter to use for the rendered video
  // stream on an incoming channel.
  virtual int RegisterRenderEffectFilter(const int video_channel,
                                         ViEEffectFilter& render_filter) = 0;

  // This function deregisters a render effect filter for a specified channel.
  virtual int DeregisterRenderEffectFilter(const int video_channel) = 0;

  // All cameras run the risk of getting in almost perfect sync with
  // florescent lamps, which will result in a very annoying flickering of the
  // image. Most cameras have some type of filter to protect against this but
  // not all of them succeed. Enabling this function will remove the flicker.
  virtual int EnableDeflickering(const int capture_id, const bool enable) = 0;

  // TODO(pbos): Remove this function when removed from fakewebrtcvideoengine.h.
  virtual int EnableDenoising(const int capture_id, const bool enable) {
    return -1;
  }

  // This function enhances the colors on the decoded video stream, enabled by
  // default.
  virtual int EnableColorEnhancement(const int video_channel,
                                     const bool enable) = 0;

  // New-style callbacks, used by VideoSendStream/VideoReceiveStream.
  virtual void RegisterPreEncodeCallback(
      int video_channel,
      I420FrameCallback* pre_encode_callback) = 0;
  virtual void DeRegisterPreEncodeCallback(int video_channel) = 0;

  virtual void RegisterPostEncodeImageCallback(
      int video_channel,
      EncodedImageCallback* post_encode_callback) {}
  virtual void DeRegisterPostEncodeCallback(int video_channel) {}

  virtual void RegisterPreDecodeImageCallback(
      int video_channel,
      EncodedImageCallback* pre_decode_callback) {}
  virtual void DeRegisterPreDecodeCallback(int video_channel) {}

  virtual void RegisterPreRenderCallback(
      int video_channel,
      I420FrameCallback* pre_render_callback) = 0;
  virtual void DeRegisterPreRenderCallback(int video_channel) = 0;

 protected:
  ViEImageProcess() {}
  virtual ~ViEImageProcess() {}
};

}  // namespace webrtc

#endif  // WEBRTC_VIDEO_ENGINE_INCLUDE_VIE_IMAGE_PROCESS_H_
