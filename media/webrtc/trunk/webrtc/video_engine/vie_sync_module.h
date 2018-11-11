/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// ViESyncModule is responsible for synchronization audio and video for a given
// VoE and ViE channel couple.

#ifndef WEBRTC_VIDEO_ENGINE_VIE_SYNC_MODULE_H_
#define WEBRTC_VIDEO_ENGINE_VIE_SYNC_MODULE_H_

#include "webrtc/base/scoped_ptr.h"
#include "webrtc/modules/interface/module.h"
#include "webrtc/system_wrappers/interface/tick_util.h"
#include "webrtc/video_engine/stream_synchronization.h"
#include "webrtc/voice_engine/include/voe_video_sync.h"

namespace webrtc {

class CriticalSectionWrapper;
class RtpRtcp;
class VideoCodingModule;
class ViEChannel;
class VoEVideoSync;

class ViESyncModule : public Module {
 public:
  ViESyncModule(VideoCodingModule* vcm,
                ViEChannel* vie_channel);
  ~ViESyncModule();

  int ConfigureSync(int voe_channel_id,
                    VoEVideoSync* voe_sync_interface,
                    RtpRtcp* video_rtcp_module,
                    RtpReceiver* video_receiver);

  int VoiceChannel();

  // Set target delay for buffering mode (0 = real-time mode).
  int SetTargetBufferingDelay(int target_delay_ms);

  // Implements Module.
  int64_t TimeUntilNextProcess() override;
  int32_t Process() override;

 private:
  rtc::scoped_ptr<CriticalSectionWrapper> data_cs_;
  VideoCodingModule* vcm_;
  ViEChannel* vie_channel_;
  RtpReceiver* video_receiver_;
  RtpRtcp* video_rtp_rtcp_;
  int voe_channel_id_;
  VoEVideoSync* voe_sync_interface_;
  TickTime last_sync_time_;
  rtc::scoped_ptr<StreamSynchronization> sync_;
  StreamSynchronization::Measurements audio_measurement_;
  StreamSynchronization::Measurements video_measurement_;
};

}  // namespace webrtc

#endif  // WEBRTC_VIDEO_ENGINE_VIE_SYNC_MODULE_H_
