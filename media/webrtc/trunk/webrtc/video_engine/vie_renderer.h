/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VIDEO_ENGINE_VIE_RENDERER_H_
#define WEBRTC_VIDEO_ENGINE_VIE_RENDERER_H_

#include <vector>

#include "webrtc/modules/video_render/include/video_render_defines.h"
#include "webrtc/video_engine/include/vie_render.h"
#include "webrtc/video_engine/vie_frame_provider_base.h"

namespace webrtc {

class VideoRender;
class VideoRenderCallback;
class ViERenderManager;

class ViEExternalRendererImpl : public VideoRenderCallback {
 public:
  ViEExternalRendererImpl();
  virtual ~ViEExternalRendererImpl() {}

  int SetViEExternalRenderer(ExternalRenderer* external_renderer,
                             RawVideoType video_input_format);

  // Implements VideoRenderCallback.
  virtual int32_t RenderFrame(const uint32_t stream_id,
                              const I420VideoFrame& video_frame);

 private:
  void NotifyFrameSizeChange(const uint32_t stream_id,
                             const I420VideoFrame& video_frame);
  int32_t ConvertAndRenderFrame(uint32_t stream_id,
                                const I420VideoFrame& video_frame);
  ExternalRenderer* external_renderer_;
  RawVideoType external_renderer_format_;
  int external_renderer_width_;
  int external_renderer_height_;
  // Converted_frame_ in color format specified by render_format_.
  std::vector<uint8_t> converted_frame_;
};

class ViERenderer: public ViEFrameCallback {
 public:
  static ViERenderer* CreateViERenderer(const int32_t render_id,
                                        const int32_t engine_id,
                                        VideoRender& render_module,
                                        ViERenderManager& render_manager,
                                        const uint32_t z_order,
                                        const float left,
                                        const float top,
                                        const float right,
                                        const float bottom);
  ~ViERenderer(void);

  int32_t StartRender();
  int32_t StopRender();

  int SetExpectedRenderDelay(int render_delay);

  int32_t ConfigureRenderer(const unsigned int z_order,
                            const float left,
                            const float top,
                            const float right,
                            const float bottom);

  VideoRender& RenderModule();

  int32_t SetTimeoutImage(const I420VideoFrame& timeout_image,
                          const int32_t timeout_value);
  int32_t SetRenderStartImage(const I420VideoFrame& start_image);
  int32_t SetExternalRenderer(const int32_t render_id,
                              RawVideoType video_input_format,
                              ExternalRenderer* external_renderer);

  int32_t SetVideoRenderCallback(const int32_t render_id,
                                 VideoRenderCallback* callback);

 private:
  ViERenderer(const int32_t render_id, const int32_t engine_id,
                VideoRender& render_module,
                ViERenderManager& render_manager);

  int32_t Init(const uint32_t z_order,
               const float left,
               const float top,
               const float right,
               const float bottom);

  // Implement ViEFrameCallback
  virtual void DeliverFrame(int id,
                            I420VideoFrame* video_frame,
                            const std::vector<uint32_t>& csrcs);
  virtual void DelayChanged(int id, int frame_delay);
  virtual int GetPreferedFrameSettings(int* width,
                                       int* height,
                                       int* frame_rate);
  virtual void ProviderDestroyed(int id);

  uint32_t render_id_;
  VideoRender& render_module_;
  ViERenderManager& render_manager_;
  VideoRenderCallback* render_callback_;
  ViEExternalRendererImpl* incoming_external_callback_;
};

}  // namespace webrtc

#endif  // WEBRTC_VIDEO_ENGINE_VIE_RENDERER_H_
