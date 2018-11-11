/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VIDEO_ENGINE_VIE_IMPL_H_
#define WEBRTC_VIDEO_ENGINE_VIE_IMPL_H_

#include "webrtc/base/scoped_ptr.h"
#include "webrtc/common.h"
#include "webrtc/engine_configurations.h"
#include "webrtc/video_engine/vie_defines.h"

#include "webrtc/video_engine/vie_base_impl.h"

#ifdef WEBRTC_VIDEO_ENGINE_CAPTURE_API
#include "webrtc/video_engine/vie_capture_impl.h"
#endif
#ifdef WEBRTC_VIDEO_ENGINE_CODEC_API
#include "webrtc/video_engine/vie_codec_impl.h"
#endif
#ifdef WEBRTC_VIDEO_ENGINE_FILE_API
#include "webrtc/video_engine/vie_file_impl.h"
#endif
#ifdef WEBRTC_VIDEO_ENGINE_IMAGE_PROCESS_API
#include "webrtc/video_engine/vie_image_process_impl.h"
#endif
#include "webrtc/video_engine/vie_network_impl.h"
#ifdef WEBRTC_VIDEO_ENGINE_RENDER_API
#include "webrtc/video_engine/vie_render_impl.h"
#endif
#ifdef WEBRTC_VIDEO_ENGINE_RTP_RTCP_API
#include "webrtc/video_engine/vie_rtp_rtcp_impl.h"
#endif
#ifdef WEBRTC_VIDEO_ENGINE_EXTERNAL_CODEC_API
#include "webrtc/video_engine/vie_external_codec_impl.h"
#endif

namespace webrtc {

class VideoEngineImpl
    : public ViEBaseImpl,
#ifdef WEBRTC_VIDEO_ENGINE_CODEC_API
      public ViECodecImpl,
#endif
#ifdef WEBRTC_VIDEO_ENGINE_CAPTURE_API
      public ViECaptureImpl,
#endif
#ifdef WEBRTC_VIDEO_ENGINE_FILE_API
      public ViEFileImpl,
#endif
#ifdef WEBRTC_VIDEO_ENGINE_IMAGE_PROCESS_API
      public ViEImageProcessImpl,
#endif
      public ViENetworkImpl,
#ifdef WEBRTC_VIDEO_ENGINE_RENDER_API
      public ViERenderImpl,
#endif
#ifdef WEBRTC_VIDEO_ENGINE_RTP_RTCP_API
      public ViERTP_RTCPImpl,
#endif
#ifdef WEBRTC_VIDEO_ENGINE_EXTERNAL_CODEC_API
      public ViEExternalCodecImpl,
#endif
      public VideoEngine
{  // NOLINT
 public:
  VideoEngineImpl(const Config* config, bool owns_config)
      : ViEBaseImpl(*config),
#ifdef WEBRTC_VIDEO_ENGINE_CODEC_API
        ViECodecImpl(ViEBaseImpl::shared_data()),
#endif
#ifdef WEBRTC_VIDEO_ENGINE_CAPTURE_API
        ViECaptureImpl(ViEBaseImpl::shared_data()),
#endif
#ifdef WEBRTC_VIDEO_ENGINE_FILE_API
        ViEFileImpl(ViEBaseImpl::shared_data()),
#endif
#ifdef WEBRTC_VIDEO_ENGINE_IMAGE_PROCESS_API
        ViEImageProcessImpl(ViEBaseImpl::shared_data()),
#endif
        ViENetworkImpl(ViEBaseImpl::shared_data()),
#ifdef WEBRTC_VIDEO_ENGINE_RENDER_API
        ViERenderImpl(ViEBaseImpl::shared_data()),
#endif
#ifdef WEBRTC_VIDEO_ENGINE_RTP_RTCP_API
        ViERTP_RTCPImpl(ViEBaseImpl::shared_data()),
#endif
#ifdef WEBRTC_VIDEO_ENGINE_EXTERNAL_CODEC_API
        ViEExternalCodecImpl(ViEBaseImpl::shared_data()),
#endif
        own_config_(owns_config ? config : NULL)
  {}
  virtual ~VideoEngineImpl() {}

 private:
  // Placeholder for the case where this owns the config.
  rtc::scoped_ptr<const Config> own_config_;
};

}  // namespace webrtc

#endif  // WEBRTC_VIDEO_ENGINE_VIE_IMPL_H_
