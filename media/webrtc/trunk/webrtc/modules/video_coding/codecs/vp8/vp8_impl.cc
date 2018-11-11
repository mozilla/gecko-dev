/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/video_coding/codecs/vp8/vp8_impl.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <algorithm>

// NOTE(ajm): Path provided by gyp.
#include "libyuv/scale.h"  // NOLINT
#include "libyuv/convert.h"  // NOLINT

#include "webrtc/base/checks.h"
#include "webrtc/common.h"
#include "webrtc/common_types.h"
#include "webrtc/common_video/libyuv/include/webrtc_libyuv.h"
#include "webrtc/experiments.h"
#include "webrtc/modules/interface/module_common_types.h"
#include "webrtc/modules/video_coding/codecs/interface/video_codec_interface.h"
#include "webrtc/modules/video_coding/codecs/vp8/include/vp8_common_types.h"
#include "webrtc/modules/video_coding/codecs/vp8/screenshare_layers.h"
#include "webrtc/modules/video_coding/codecs/vp8/temporal_layers.h"
#include "webrtc/system_wrappers/interface/tick_util.h"
#include "webrtc/system_wrappers/interface/trace_event.h"

namespace webrtc {
namespace {

enum { kVp8ErrorPropagationTh = 30 };
enum { kVp832ByteAlign = 32 };

// VP8 denoiser states.
enum denoiserState {
  kDenoiserOff,
  kDenoiserOnYOnly,
  kDenoiserOnYUV,
  kDenoiserOnYUVAggressive,
  // Adaptive mode defaults to kDenoiserOnYUV on key frame, but may switch
  // to kDenoiserOnYUVAggressive based on a computed noise metric.
  kDenoiserOnAdaptive
};

// Greatest common divisior
int GCD(int a, int b) {
  int c = a % b;
  while (c != 0) {
    a = b;
    b = c;
    c = a % b;
  }
  return b;
}

uint32_t SumStreamTargetBitrate(int streams, const VideoCodec& codec) {
  uint32_t bitrate_sum = 0;
  for (int i = 0; i < streams; ++i) {
    bitrate_sum += codec.simulcastStream[i].targetBitrate;
  }
  return bitrate_sum;
}

uint32_t SumStreamMaxBitrate(int streams, const VideoCodec& codec) {
  uint32_t bitrate_sum = 0;
  for (int i = 0; i < streams; ++i) {
    bitrate_sum += codec.simulcastStream[i].maxBitrate;
  }
  return bitrate_sum;
}

int NumberOfStreams(const VideoCodec& codec) {
  int streams =
      codec.numberOfSimulcastStreams < 1 ? 1 : codec.numberOfSimulcastStreams;
  uint32_t simulcast_max_bitrate = SumStreamMaxBitrate(streams, codec);
  if (simulcast_max_bitrate == 0) {
    streams = 1;
  }
  return streams;
}

bool ValidSimulcastResolutions(const VideoCodec& codec, int num_streams) {
  if (codec.width != codec.simulcastStream[num_streams - 1].width ||
      codec.height != codec.simulcastStream[num_streams - 1].height) {
    return false;
  }
  for (int i = 0; i < num_streams; ++i) {
    if (codec.width * codec.simulcastStream[i].height !=
        codec.height * codec.simulcastStream[i].width) {
      return false;
    }
  }
  return true;
}
}  // namespace

const float kTl1MaxTimeToDropFrames = 20.0f;

VP8EncoderImpl::VP8EncoderImpl()
    : encoded_complete_callback_(NULL),
      inited_(false),
      timestamp_(0),
      feedback_mode_(false),
      qp_max_(56),  // Setting for max quantizer.
      cpu_speed_default_(-6),
      rc_max_intra_target_(0),
      token_partitions_(VP8_ONE_TOKENPARTITION),
      down_scale_requested_(false),
      down_scale_bitrate_(0),
      tl0_frame_dropper_(),
      tl1_frame_dropper_(kTl1MaxTimeToDropFrames),
      key_frame_request_(kMaxSimulcastStreams, false) {
  uint32_t seed = static_cast<uint32_t>(TickTime::MillisecondTimestamp());
  srand(seed);

  picture_id_.reserve(kMaxSimulcastStreams);
  last_key_frame_picture_id_.reserve(kMaxSimulcastStreams);
  temporal_layers_.reserve(kMaxSimulcastStreams);
  raw_images_.reserve(kMaxSimulcastStreams);
  encoded_images_.reserve(kMaxSimulcastStreams);
  send_stream_.reserve(kMaxSimulcastStreams);
  cpu_speed_.assign(kMaxSimulcastStreams, -6);  // Set default to -6.
  encoders_.reserve(kMaxSimulcastStreams);
  configurations_.reserve(kMaxSimulcastStreams);
  downsampling_factors_.reserve(kMaxSimulcastStreams);
}

VP8EncoderImpl::~VP8EncoderImpl() {
  Release();
}

int VP8EncoderImpl::Release() {
  int ret_val = WEBRTC_VIDEO_CODEC_OK;

  while (!encoded_images_.empty()) {
    EncodedImage& image = encoded_images_.back();
    delete [] image._buffer;
    encoded_images_.pop_back();
  }
  while (!encoders_.empty()) {
    vpx_codec_ctx_t& encoder = encoders_.back();
    if (vpx_codec_destroy(&encoder)) {
      ret_val = WEBRTC_VIDEO_CODEC_MEMORY;
    }
    encoders_.pop_back();
  }
  configurations_.clear();
  send_stream_.clear();
  cpu_speed_.clear();
  while (!raw_images_.empty()) {
    vpx_img_free(&raw_images_.back());
    raw_images_.pop_back();
  }
  while (!temporal_layers_.empty()) {
    delete temporal_layers_.back();
    temporal_layers_.pop_back();
  }
  inited_ = false;
  return ret_val;
}

int VP8EncoderImpl::SetRates(uint32_t new_bitrate_kbit,
                                     uint32_t new_framerate) {
  if (!inited_) {
    return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  }
  if (encoders_[0].err) {
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
  if (new_framerate < 1) {
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }
  if (codec_.maxBitrate > 0 && new_bitrate_kbit > codec_.maxBitrate) {
    new_bitrate_kbit = codec_.maxBitrate;
  }
  if (new_bitrate_kbit < codec_.minBitrate) {
    new_bitrate_kbit = codec_.minBitrate;
  }
  if (codec_.numberOfSimulcastStreams > 0 &&
      new_bitrate_kbit < codec_.simulcastStream[0].minBitrate) {
    new_bitrate_kbit = codec_.simulcastStream[0].minBitrate;
  }
  codec_.maxFramerate = new_framerate;

  if (encoders_.size() == 1) {
    // 1:1.
    // Calculate a rough limit for when to trigger a potental down scale.
    uint32_t k_pixels_per_frame = codec_.width * codec_.height / 1000;
    // TODO(pwestin): we currently lack CAMA, this is a temporary fix to work
    // around the current limitations.
    // Only trigger keyframes if we are allowed to scale down.
    if (configurations_[0].rc_resize_allowed) {
      if (!down_scale_requested_) {
        if (k_pixels_per_frame > new_bitrate_kbit) {
          down_scale_requested_ = true;
          down_scale_bitrate_ = new_bitrate_kbit;
          key_frame_request_[0] = true;
        }
      } else {
        if (new_bitrate_kbit > (2 * down_scale_bitrate_) ||
            new_bitrate_kbit < (down_scale_bitrate_ / 2)) {
          down_scale_requested_ = false;
        }
      }
    }
  } else {
    // If we have more than 1 stream, reduce the qp_max for the low resolution
    // stream if frame rate is not too low. The trade-off with lower qp_max is
    // possibly more dropped frames, so we only do this if the frame rate is
    // above some threshold (base temporal layer is down to 1/4 for 3 layers).
    // We may want to condition this on bitrate later.
    if (new_framerate > 20) {
      configurations_[encoders_.size() - 1].rc_max_quantizer = 45;
    } else {
      // Go back to default value set in InitEncode.
      configurations_[encoders_.size() - 1].rc_max_quantizer = qp_max_;
    }
  }

  bool send_stream = true;
  int stream_bitrate = 0;
  size_t stream_idx = encoders_.size() - 1;
  for (size_t i = 0; i < encoders_.size(); ++i, --stream_idx) {
    if (encoders_.size() == 1) {
      stream_bitrate = new_bitrate_kbit;
    } else {
      stream_bitrate = GetStreamBitrate(stream_idx,
                                        new_bitrate_kbit,
                                        &send_stream);
      SetStreamState(send_stream, stream_idx);
    }

    unsigned int target_bitrate = stream_bitrate;
    unsigned int max_bitrate = codec_.maxBitrate;
    int framerate = new_framerate;
    // TODO(holmer): This is a temporary hack for screensharing, where we
    // interpret the startBitrate as the encoder target bitrate. This is
    // to allow for a different max bitrate, so if the codec can't meet
    // the target we still allow it to overshoot up to the max before dropping
    // frames. This hack should be improved.
    if (codec_.targetBitrate > 0 &&
        (codec_.codecSpecific.VP8.numberOfTemporalLayers == 2 ||
         codec_.simulcastStream[0].numberOfTemporalLayers == 2)) {
      int tl0_bitrate = std::min(codec_.targetBitrate, target_bitrate);
      max_bitrate = std::min(codec_.maxBitrate, target_bitrate);
      target_bitrate = tl0_bitrate;
      framerate = -1;
    }
    configurations_[i].rc_target_bitrate = target_bitrate;
    temporal_layers_[stream_idx]->ConfigureBitrates(target_bitrate,
                                                    max_bitrate,
                                                    framerate,
                                                    &configurations_[i]);
    if (vpx_codec_enc_config_set(&encoders_[i], &configurations_[i])) {
      return WEBRTC_VIDEO_CODEC_ERROR;
    }
  }
  quality_scaler_.ReportFramerate(new_framerate);
  return WEBRTC_VIDEO_CODEC_OK;
}

int VP8EncoderImpl::GetStreamBitrate(int stream_idx,
                                             uint32_t new_bitrate_kbit,
                                             bool* send_stream) const {
  // The bitrate needed to start sending this stream is given by the
  // minimum bitrate allowed for encoding this stream, plus the sum target
  // rates of all lower streams.
  uint32_t sum_target_lower_streams = (stream_idx == 0) ? 0 :
      SumStreamTargetBitrate(stream_idx, codec_);
  uint32_t bitrate_to_send_this_layer =
      codec_.simulcastStream[stream_idx].minBitrate + sum_target_lower_streams;
  if (new_bitrate_kbit >= bitrate_to_send_this_layer) {
    // We have enough bandwidth to send this stream.
    *send_stream = true;
    // Bitrate for this stream is the new bitrate (|new_bitrate_kbit|) minus the
    // sum target rates of the lower streams, and capped to a maximum bitrate.
    // The maximum cap depends on whether we send the next higher stream.
    // If we will be sending the next higher stream, |max_rate| is given by
    // current stream's |targetBitrate|, otherwise it's capped by |maxBitrate|.
    if (stream_idx < codec_.numberOfSimulcastStreams - 1) {
      uint32_t max_rate = codec_.simulcastStream[stream_idx].maxBitrate;
      if (new_bitrate_kbit >= SumStreamTargetBitrate(stream_idx + 1, codec_) +
          codec_.simulcastStream[stream_idx + 1].minBitrate) {
        max_rate = codec_.simulcastStream[stream_idx].targetBitrate;
      }
      return std::min(new_bitrate_kbit - sum_target_lower_streams, max_rate);
    } else {
        // For the highest stream (highest resolution), the |targetBitRate| and
        // |maxBitrate| are not used. Any excess bitrate (above the targets of
        // all lower streams) is given to this (highest resolution) stream.
        return new_bitrate_kbit - sum_target_lower_streams;
    }
  } else {
    // Not enough bitrate for this stream.
    // Return our max bitrate of |stream_idx| - 1, but we don't send it. We need
    // to keep this resolution coding in order for the multi-encoder to work.
    *send_stream = false;
    return 0;
  }
}

void VP8EncoderImpl::SetStreamState(bool send_stream,
                                            int stream_idx) {
  if (send_stream && !send_stream_[stream_idx]) {
    // Need a key frame if we have not sent this stream before.
    key_frame_request_[stream_idx] = true;
  }
  send_stream_[stream_idx] = send_stream;
}

void VP8EncoderImpl::SetupTemporalLayers(int num_streams,
                                                 int num_temporal_layers,
                                                 const VideoCodec& codec) {
  const Config default_options;
  const TemporalLayers::Factory& tl_factory =
      (codec.extra_options ? codec.extra_options : &default_options)
          ->Get<TemporalLayers::Factory>();
  if (num_streams == 1) {
    if (codec.mode == kScreensharing) {
      // Special mode when screensharing on a single stream.
      temporal_layers_.push_back(new ScreenshareLayers(num_temporal_layers,
                                                       rand(),
                                                       &tl0_frame_dropper_,
                                                       &tl1_frame_dropper_));
    } else {
      temporal_layers_.push_back(
          tl_factory.Create(num_temporal_layers, rand()));
    }
  } else {
    for (int i = 0; i < num_streams; ++i) {
      // TODO(andresp): crash if layers is invalid.
      int layers = codec.simulcastStream[i].numberOfTemporalLayers;
      if (layers < 1) layers = 1;
      temporal_layers_.push_back(tl_factory.Create(layers, rand()));
    }
  }
}

int VP8EncoderImpl::InitEncode(const VideoCodec* inst,
                                       int number_of_cores,
                                       size_t /*maxPayloadSize */) {
  if (inst == NULL) {
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }
  if (inst->maxFramerate < 1) {
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }
  // allow zero to represent an unspecified maxBitRate
  if (inst->maxBitrate > 0 && inst->startBitrate > inst->maxBitrate) {
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }
  if (inst->width <= 1 || inst->height <= 1) {
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }
  if (number_of_cores < 1) {
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }
  if (inst->codecSpecific.VP8.feedbackModeOn &&
      inst->numberOfSimulcastStreams > 1) {
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }
  if (inst->codecSpecific.VP8.automaticResizeOn &&
      inst->numberOfSimulcastStreams > 1) {
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }
  int retVal = Release();
  if (retVal < 0) {
    return retVal;
  }

  int number_of_streams = NumberOfStreams(*inst);
  bool doing_simulcast = (number_of_streams > 1);

  if (doing_simulcast && !ValidSimulcastResolutions(*inst, number_of_streams)) {
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }

  int num_temporal_layers = doing_simulcast ?
      inst->simulcastStream[0].numberOfTemporalLayers :
      inst->codecSpecific.VP8.numberOfTemporalLayers;

  // TODO(andresp): crash if num temporal layers is bananas.
  if (num_temporal_layers < 1) num_temporal_layers = 1;
  SetupTemporalLayers(number_of_streams, num_temporal_layers, *inst);

  feedback_mode_ = inst->codecSpecific.VP8.feedbackModeOn;

  timestamp_ = 0;
  codec_ = *inst;

  // Code expects simulcastStream resolutions to be correct, make sure they are
  // filled even when there are no simulcast layers.
  if (codec_.numberOfSimulcastStreams == 0) {
    codec_.simulcastStream[0].width = codec_.width;
    codec_.simulcastStream[0].height = codec_.height;
  }

  picture_id_.resize(number_of_streams);
  last_key_frame_picture_id_.resize(number_of_streams);
  encoded_images_.resize(number_of_streams);
  encoders_.resize(number_of_streams);
  configurations_.resize(number_of_streams);
  downsampling_factors_.resize(number_of_streams);
  raw_images_.resize(number_of_streams);
  send_stream_.resize(number_of_streams);
  send_stream_[0] = true;  // For non-simulcast case.
  cpu_speed_.resize(number_of_streams);
  std::fill(key_frame_request_.begin(), key_frame_request_.end(), false);

  int idx = number_of_streams - 1;
  for (int i = 0; i < (number_of_streams - 1); ++i, --idx) {
    int gcd = GCD(inst->simulcastStream[idx].width,
                  inst->simulcastStream[idx-1].width);
    downsampling_factors_[i].num = inst->simulcastStream[idx].width / gcd;
    downsampling_factors_[i].den = inst->simulcastStream[idx - 1].width / gcd;
    send_stream_[i] = false;
  }
  if (number_of_streams > 1) {
    send_stream_[number_of_streams - 1] = false;
    downsampling_factors_[number_of_streams - 1].num = 1;
    downsampling_factors_[number_of_streams - 1].den = 1;
  }
  for (int i = 0; i < number_of_streams; ++i) {
    // Random start, 16 bits is enough.
    picture_id_[i] = static_cast<uint16_t>(rand()) & 0x7FFF;
    last_key_frame_picture_id_[i] = -1;
    // allocate memory for encoded image
    if (encoded_images_[i]._buffer != NULL) {
      delete [] encoded_images_[i]._buffer;
    }
    // Reserve 100 extra bytes for overhead at small resolutions.
    encoded_images_[i]._size = CalcBufferSize(kI420, codec_.width, codec_.height)
                               + 100;
    encoded_images_[i]._buffer = new uint8_t[encoded_images_[i]._size];
    encoded_images_[i]._completeFrame = true;
  }
  // populate encoder configuration with default values
  if (vpx_codec_enc_config_default(vpx_codec_vp8_cx(),
                                   &configurations_[0], 0)) {
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
  // setting the time base of the codec
  configurations_[0].g_timebase.num = 1;
  configurations_[0].g_timebase.den = 90000;
  configurations_[0].g_lag_in_frames = 0;  // 0- no frame lagging

  // Set the error resilience mode according to user settings.
  switch (inst->codecSpecific.VP8.resilience) {
    case kResilienceOff:
      // TODO(marpan): We should set keep error resilience off for this mode,
      // independent of temporal layer settings, and make sure we set
      // |codecSpecific.VP8.resilience| = |kResilientStream| at higher level
      // code if we want to get error resilience on.
      configurations_[0].g_error_resilient = 1;
      break;
    case kResilientStream:
      configurations_[0].g_error_resilient = 1;  // TODO(holmer): Replace with
      // VPX_ERROR_RESILIENT_DEFAULT when we
      // drop support for libvpx 9.6.0.
      break;
    case kResilientFrames:
#ifdef INDEPENDENT_PARTITIONS
      configurations_[0]-g_error_resilient = VPX_ERROR_RESILIENT_DEFAULT |
      VPX_ERROR_RESILIENT_PARTITIONS;
      break;
#else
      return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;  // Not supported
#endif
  }

  // rate control settings
  configurations_[0].rc_dropframe_thresh =
      inst->codecSpecific.VP8.frameDroppingOn ? 30 : 0;
  configurations_[0].rc_end_usage = VPX_CBR;
  configurations_[0].g_pass = VPX_RC_ONE_PASS;
  // TODO(hellner): investigate why the following two lines produce
  // automaticResizeOn value of 3 when running
  // WebRtcVideoMediaChannelTest.GetStatsMultipleSendStreams inside the talk
  // framework.
  // configurations_[0].rc_resize_allowed =
  //    inst->codecSpecific.VP8.automaticResizeOn ? 1 : 0;
  configurations_[0].rc_resize_allowed = 0;
  // Handle resizing outside of libvpx when doing single-stream.
  if (inst->codecSpecific.VP8.automaticResizeOn && number_of_streams > 1) {
    configurations_[0].rc_resize_allowed = 1;
  }
  configurations_[0].rc_min_quantizer = 2;
  if (inst->qpMax >= configurations_[0].rc_min_quantizer) {
    qp_max_ = inst->qpMax;
  }
  configurations_[0].rc_max_quantizer = qp_max_;
  configurations_[0].rc_undershoot_pct = 100;
  configurations_[0].rc_overshoot_pct = 15;
  configurations_[0].rc_buf_initial_sz = 500;
  configurations_[0].rc_buf_optimal_sz = 600;
  configurations_[0].rc_buf_sz = 1000;

  // Set the maximum target size of any key-frame.
  rc_max_intra_target_ = MaxIntraTarget(configurations_[0].rc_buf_optimal_sz);

  if (feedback_mode_) {
    // Disable periodic key frames if we get feedback from the decoder
    // through SLI and RPSI.
    configurations_[0].kf_mode = VPX_KF_DISABLED;
  } else if (inst->codecSpecific.VP8.keyFrameInterval > 0) {
    configurations_[0].kf_mode = VPX_KF_AUTO;
    configurations_[0].kf_max_dist = inst->codecSpecific.VP8.keyFrameInterval;
  } else {
    configurations_[0].kf_mode = VPX_KF_DISABLED;
  }

  // Allow the user to set the complexity for the base stream.
  switch (inst->codecSpecific.VP8.complexity) {
    case kComplexityHigh:
      cpu_speed_[0] = -5;
      break;
    case kComplexityHigher:
      cpu_speed_[0] = -4;
      break;
    case kComplexityMax:
      cpu_speed_[0] = -3;
      break;
    default:
      cpu_speed_[0] = -6;
      break;
  }
  cpu_speed_default_ = cpu_speed_[0];
  // Set encoding complexity (cpu_speed) based on resolution and/or platform.
  cpu_speed_[0] = SetCpuSpeed(inst->width, inst->height);
  for (int i = 1; i < number_of_streams; ++i) {
    cpu_speed_[i] =
        SetCpuSpeed(inst->simulcastStream[number_of_streams - 1 - i].width,
                    inst->simulcastStream[number_of_streams - 1 - i].height);
  }
  configurations_[0].g_w = inst->width;
  configurations_[0].g_h = inst->height;

  // Determine number of threads based on the image size and #cores.
  // TODO(fbarchard): Consider number of Simulcast layers.
  configurations_[0].g_threads = NumberOfThreads(configurations_[0].g_w,
                                                 configurations_[0].g_h,
                                                 number_of_cores);

  // Creating a wrapper to the image - setting image data to NULL.
  // Actual pointer will be set in encode. Setting align to 1, as it
  // is meaningless (no memory allocation is done here).
  vpx_img_wrap(&raw_images_[0], VPX_IMG_FMT_I420, inst->width, inst->height,
               1, NULL);

  if (encoders_.size() == 1) {
    configurations_[0].rc_target_bitrate = inst->startBitrate;
    temporal_layers_[0]->ConfigureBitrates(inst->startBitrate,
                                           inst->maxBitrate,
                                           inst->maxFramerate,
                                           &configurations_[0]);
  } else {
    // Note the order we use is different from webm, we have lowest resolution
    // at position 0 and they have highest resolution at position 0.
    int stream_idx = encoders_.size() - 1;
    bool send_stream = true;
    int stream_bitrate = GetStreamBitrate(stream_idx,
                                          inst->startBitrate,
                                          &send_stream);
    SetStreamState(send_stream, stream_idx);
    configurations_[0].rc_target_bitrate = stream_bitrate;
    temporal_layers_[stream_idx]->ConfigureBitrates(stream_bitrate,
                                                    inst->maxBitrate,
                                                    inst->maxFramerate,
                                                    &configurations_[0]);
    --stream_idx;
    for (size_t i = 1; i < encoders_.size(); ++i, --stream_idx) {
      memcpy(&configurations_[i], &configurations_[0],
             sizeof(configurations_[0]));

      configurations_[i].g_w = inst->simulcastStream[stream_idx].width;
      configurations_[i].g_h = inst->simulcastStream[stream_idx].height;

      // Use 1 thread for lower resolutions.
      configurations_[i].g_threads = 1;

      // Setting alignment to 32 - as that ensures at least 16 for all
      // planes (32 for Y, 16 for U,V). Libvpx sets the requested stride for
      // the y plane, but only half of it to the u and v planes.
      vpx_img_alloc(&raw_images_[i], VPX_IMG_FMT_I420,
                    inst->simulcastStream[stream_idx].width,
                    inst->simulcastStream[stream_idx].height, kVp832ByteAlign);
      int stream_bitrate = GetStreamBitrate(stream_idx,
                                            inst->startBitrate,
                                            &send_stream);
      SetStreamState(send_stream, stream_idx);
      configurations_[i].rc_target_bitrate = stream_bitrate;
      temporal_layers_[stream_idx]->ConfigureBitrates(stream_bitrate,
                                                      inst->maxBitrate,
                                                      inst->maxFramerate,
                                                      &configurations_[i]);
    }
  }

  rps_.Init();
  quality_scaler_.Init(codec_.qpMax);
  quality_scaler_.ReportFramerate(codec_.maxFramerate);

  return InitAndSetControlSettings();
}

int VP8EncoderImpl::SetCpuSpeed(int width, int height) {
#if defined(WEBRTC_ARCH_ARM)
  // On mobile platform, always set to -12 to leverage between cpu usage
  // and video quality.
  return -12;
#else
  // For non-ARM, increase encoding complexity (i.e., use lower speed setting)
  // if resolution is below CIF. Otherwise, keep the default/user setting
  // (|cpu_speed_default_|) set on InitEncode via codecSpecific.VP8.complexity.
  if (width * height < 352 * 288)
    return (cpu_speed_default_ < -4) ? -4 : cpu_speed_default_;
  else
    return cpu_speed_default_;
#endif
}

int VP8EncoderImpl::NumberOfThreads(int width, int height, int cpus) {
  if (width * height >= 1920 * 1080 && cpus > 8) {
    return 8;  // 8 threads for 1080p on high perf machines.
  } else if (width * height > 1280 * 960 && cpus >= 6) {
    // 3 threads for 1080p.
    return 3;
  } else if (width * height > 640 * 480 && cpus >= 3) {
    // 2 threads for qHD/HD.
    return 2;
  } else {
    // 1 thread for VGA or less.
    return 1;
  }
}

int VP8EncoderImpl::InitAndSetControlSettings() {
  vpx_codec_flags_t flags = 0;
  flags |= VPX_CODEC_USE_OUTPUT_PARTITION;

  if (encoders_.size() > 1) {
    int error = vpx_codec_enc_init_multi(&encoders_[0],
                                 vpx_codec_vp8_cx(),
                                 &configurations_[0],
                                 encoders_.size(),
                                 flags,
                                 &downsampling_factors_[0]);
    if (error) {
      return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
    }
  } else {
    if (vpx_codec_enc_init(&encoders_[0],
                           vpx_codec_vp8_cx(),
                           &configurations_[0],
                           flags)) {
      return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
    }
  }
  // Enable denoising for the highest resolution stream, and for
  // the second highest resolution if we are doing more than 2
  // spatial layers/streams.
  // TODO(holmer): Investigate possibility of adding a libvpx API
  // for getting the denoised frame from the encoder and using that
  // when encoding lower resolution streams. Would it work with the
  // multi-res encoding feature?
  denoiserState denoiser_state = kDenoiserOnYOnly;
#ifdef WEBRTC_ARCH_ARM
  denoiser_state = kDenoiserOnYOnly;
#else
  denoiser_state = kDenoiserOnAdaptive;
#endif
  vpx_codec_control(&encoders_[0], VP8E_SET_NOISE_SENSITIVITY,
                    codec_.codecSpecific.VP8.denoisingOn ?
                    denoiser_state : kDenoiserOff);
  if (encoders_.size() > 2) {
    vpx_codec_control(&encoders_[1], VP8E_SET_NOISE_SENSITIVITY,
                      codec_.codecSpecific.VP8.denoisingOn ?
                      denoiser_state : kDenoiserOff);
  }
  for (size_t i = 0; i < encoders_.size(); ++i) {
    vpx_codec_control(&(encoders_[i]), VP8E_SET_STATIC_THRESHOLD,
                      codec_.mode == kScreensharing ? 300 : 1);
    vpx_codec_control(&(encoders_[i]), VP8E_SET_CPUUSED, cpu_speed_[i]);
    vpx_codec_control(&(encoders_[i]), VP8E_SET_TOKEN_PARTITIONS,
                      static_cast<vp8e_token_partitions>(token_partitions_));
    vpx_codec_control(&(encoders_[i]), VP8E_SET_MAX_INTRA_BITRATE_PCT,
                      rc_max_intra_target_);
    vpx_codec_control(&(encoders_[i]), VP8E_SET_SCREEN_CONTENT_MODE,
                      codec_.mode == kScreensharing);
  }
  inited_ = true;
  return WEBRTC_VIDEO_CODEC_OK;
}

uint32_t VP8EncoderImpl::MaxIntraTarget(uint32_t optimalBuffersize) {
  // Set max to the optimal buffer level (normalized by target BR),
  // and scaled by a scalePar.
  // Max target size = scalePar * optimalBufferSize * targetBR[Kbps].
  // This values is presented in percentage of perFrameBw:
  // perFrameBw = targetBR[Kbps] * 1000 / frameRate.
  // The target in % is as follows:

  float scalePar = 0.5;
  uint32_t targetPct = optimalBuffersize * scalePar * codec_.maxFramerate / 10;

  // Don't go below 3 times the per frame bandwidth.
  const uint32_t minIntraTh = 300;
  return (targetPct < minIntraTh) ? minIntraTh: targetPct;
}

int VP8EncoderImpl::Encode(
    const I420VideoFrame& frame,
    const CodecSpecificInfo* codec_specific_info,
    const std::vector<VideoFrameType>* frame_types) {
  TRACE_EVENT1("webrtc", "VP8::Encode", "timestamp", frame.timestamp());

  if (!inited_) {
    return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  }
  if (frame.IsZeroSize()) {
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }
  if (encoded_complete_callback_ == NULL) {
    return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  }

  // Only apply scaling to improve for single-layer streams. The scaling metrics
  // use framedrops as a signal and is only applicable when we drop frames.
  const bool use_quality_scaler = encoders_.size() == 1 &&
                                  configurations_[0].rc_dropframe_thresh > 0 &&
                                  codec_.codecSpecific.VP8.automaticResizeOn;
  const I420VideoFrame& input_image =
      use_quality_scaler ? quality_scaler_.GetScaledFrame(frame) : frame;

  if (input_image.width() != codec_.width ||
      input_image.height() != codec_.height) {
    int ret = UpdateCodecFrameSize(input_image);
    if (ret < 0)
      return ret;
  }

  // Since we are extracting raw pointers from |input_image| to
  // |raw_images_[0]|, the resolution of these frames must match. Note that
  // |input_image| might be scaled from |frame|. In that case, the resolution of
  // |raw_images_[0]| should have been updated in UpdateCodecFrameSize.
  DCHECK_EQ(input_image.width(), static_cast<int>(raw_images_[0].d_w));
  DCHECK_EQ(input_image.height(), static_cast<int>(raw_images_[0].d_h));

  // Image in vpx_image_t format.
  // Input image is const. VP8's raw image is not defined as const.
  raw_images_[0].planes[VPX_PLANE_Y] =
     const_cast<uint8_t*>(input_image.buffer(kYPlane));
  raw_images_[0].planes[VPX_PLANE_U] =
     const_cast<uint8_t*>(input_image.buffer(kUPlane));
  raw_images_[0].planes[VPX_PLANE_V] =
     const_cast<uint8_t*>(input_image.buffer(kVPlane));

  raw_images_[0].stride[VPX_PLANE_Y] = input_image.stride(kYPlane);
  raw_images_[0].stride[VPX_PLANE_U] = input_image.stride(kUPlane);
  raw_images_[0].stride[VPX_PLANE_V] = input_image.stride(kVPlane);

  for (size_t i = 1; i < encoders_.size(); ++i) {
    // Scale the image down a number of times by downsampling factor
    libyuv::I420Scale(
        raw_images_[i-1].planes[VPX_PLANE_Y],
        raw_images_[i-1].stride[VPX_PLANE_Y],
        raw_images_[i-1].planes[VPX_PLANE_U],
        raw_images_[i-1].stride[VPX_PLANE_U],
        raw_images_[i-1].planes[VPX_PLANE_V],
        raw_images_[i-1].stride[VPX_PLANE_V],
        raw_images_[i-1].d_w, raw_images_[i-1].d_h,
        raw_images_[i].planes[VPX_PLANE_Y], raw_images_[i].stride[VPX_PLANE_Y],
        raw_images_[i].planes[VPX_PLANE_U], raw_images_[i].stride[VPX_PLANE_U],
        raw_images_[i].planes[VPX_PLANE_V], raw_images_[i].stride[VPX_PLANE_V],
        raw_images_[i].d_w, raw_images_[i].d_h, libyuv::kFilterBilinear);
  }
  vpx_enc_frame_flags_t flags[kMaxSimulcastStreams];
  for (size_t i = 0; i < encoders_.size(); ++i) {
    int ret = temporal_layers_[i]->EncodeFlags(input_image.timestamp());
    if (ret < 0) {
      // Drop this frame.
      return WEBRTC_VIDEO_CODEC_OK;
    }
    flags[i] = ret;
  }
  bool send_key_frame = false;
  for (size_t i = 0; i < key_frame_request_.size() && i < send_stream_.size();
       ++i) {
    if (key_frame_request_[i] && send_stream_[i]) {
      send_key_frame = true;
      break;
    }
  }
  if (!send_key_frame && frame_types) {
    for (size_t i = 0; i < frame_types->size() && i < send_stream_.size();
         ++i) {
      if ((*frame_types)[i] == kKeyFrame && send_stream_[i]) {
        send_key_frame = true;
        break;
      }
    }
  }
  // The flag modification below (due to forced key frame, RPS, etc.,) for now
  // will be the same for all encoders/spatial layers.
  // TODO(marpan/holmer): Allow for key frame request to be set per encoder.
  bool only_predict_from_key_frame = false;
  if (send_key_frame) {
    // Adapt the size of the key frame when in screenshare with 1 temporal
    // layer.
    if (encoders_.size() == 1 && codec_.mode == kScreensharing
        && codec_.codecSpecific.VP8.numberOfTemporalLayers <= 1) {
      const uint32_t forceKeyFrameIntraTh = 100;
      vpx_codec_control(&(encoders_[0]), VP8E_SET_MAX_INTRA_BITRATE_PCT,
                        forceKeyFrameIntraTh);
    }
    // Key frame request from caller.
    // Will update both golden and alt-ref.
    for (size_t i = 0; i < encoders_.size(); ++i) {
      flags[i] = VPX_EFLAG_FORCE_KF;
    }
    std::fill(key_frame_request_.begin(), key_frame_request_.end(), false);
  } else if (codec_specific_info &&
      codec_specific_info->codecType == kVideoCodecVP8) {
    if (feedback_mode_) {
      // Handle RPSI and SLI messages and set up the appropriate encode flags.
      bool sendRefresh = false;
      if (codec_specific_info->codecSpecific.VP8.hasReceivedRPSI) {
        rps_.ReceivedRPSI(
            codec_specific_info->codecSpecific.VP8.pictureIdRPSI);
      }
      if (codec_specific_info->codecSpecific.VP8.hasReceivedSLI) {
        sendRefresh = rps_.ReceivedSLI(input_image.timestamp());
      }
      for (size_t i = 0; i < encoders_.size(); ++i) {
        flags[i] = rps_.EncodeFlags(picture_id_[i], sendRefresh,
                                    input_image.timestamp());
      }
    } else {
      if (codec_specific_info->codecSpecific.VP8.hasReceivedRPSI) {
        // Is this our last key frame? If not ignore.
        // |picture_id_| is defined per spatial stream/layer, so check that
        // |RPSI| matches the last key frame from any of the spatial streams.
        // If so, then all spatial streams for this encoding will predict from
        // its long-term reference (last key frame).
        int RPSI = codec_specific_info->codecSpecific.VP8.pictureIdRPSI;
        for (size_t i = 0; i < encoders_.size(); ++i) {
          if (last_key_frame_picture_id_[i] == RPSI) {
            // Request for a long term reference frame.
            // Note 1: overwrites any temporal settings.
            // Note 2: VP8_EFLAG_NO_UPD_ENTROPY is not needed as that flag is
            //         set by error_resilient mode.
            for (size_t j = 0; j < encoders_.size(); ++j) {
              flags[j] = VP8_EFLAG_NO_UPD_ARF;
              flags[j] |= VP8_EFLAG_NO_REF_GF;
              flags[j] |= VP8_EFLAG_NO_REF_LAST;
            }
            only_predict_from_key_frame = true;
            break;
          }
        }
      }
    }
  }
  // Set the encoder frame flags and temporal layer_id for each spatial stream.
  // Note that |temporal_layers_| are defined starting from lowest resolution at
  // position 0 to highest resolution at position |encoders_.size() - 1|,
  // whereas |encoder_| is from highest to lowest resolution.
  size_t stream_idx = encoders_.size() - 1;
  for (size_t i = 0; i < encoders_.size(); ++i, --stream_idx) {
    vpx_codec_control(&encoders_[i], VP8E_SET_FRAME_FLAGS, flags[stream_idx]);
    vpx_codec_control(&encoders_[i],
                      VP8E_SET_TEMPORAL_LAYER_ID,
                      temporal_layers_[stream_idx]->CurrentLayerId());
  }
  // TODO(holmer): Ideally the duration should be the timestamp diff of this
  // frame and the next frame to be encoded, which we don't have. Instead we
  // would like to use the duration of the previous frame. Unfortunately the
  // rate control seems to be off with that setup. Using the average input
  // frame rate to calculate an average duration for now.
  assert(codec_.maxFramerate > 0);
  uint32_t duration = 90000 / codec_.maxFramerate;

  // Note we must pass 0 for |flags| field in encode call below since they are
  // set above in |vpx_codec_control| function for each encoder/spatial layer.
  int error = vpx_codec_encode(&encoders_[0], &raw_images_[0], timestamp_,
                               duration, 0, VPX_DL_REALTIME);
  // Reset specific intra frame thresholds, following the key frame.
  if (send_key_frame) {
    vpx_codec_control(&(encoders_[0]), VP8E_SET_MAX_INTRA_BITRATE_PCT,
        rc_max_intra_target_);
  }
  if (error) {
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
  timestamp_ += duration;
  return GetEncodedPartitions(input_image, only_predict_from_key_frame);
}

// TODO(pbos): Make sure this works for properly for >1 encoders.
int VP8EncoderImpl::UpdateCodecFrameSize(
    const I420VideoFrame& input_image) {
  codec_.width = input_image.width();
  codec_.height = input_image.height();
  // Update the cpu_speed setting for resolution change.
  vpx_codec_control(&(encoders_[0]),
                    VP8E_SET_CPUUSED,
                    SetCpuSpeed(codec_.width, codec_.height));
  raw_images_[0].w = codec_.width;
  raw_images_[0].h = codec_.height;
  raw_images_[0].d_w = codec_.width;
  raw_images_[0].d_h = codec_.height;
  vpx_img_set_rect(&raw_images_[0], 0, 0, codec_.width, codec_.height);

  // Update encoder context for new frame size.
  // Change of frame size will automatically trigger a key frame.
  configurations_[0].g_w = codec_.width;
  configurations_[0].g_h = codec_.height;
  if (vpx_codec_enc_config_set(&encoders_[0], &configurations_[0])) {
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
  return WEBRTC_VIDEO_CODEC_OK;
}

void VP8EncoderImpl::PopulateCodecSpecific(
    CodecSpecificInfo* codec_specific,
    const vpx_codec_cx_pkt_t& pkt,
    int stream_idx,
    uint32_t timestamp,
    bool only_predicting_from_key_frame) {
  assert(codec_specific != NULL);
  codec_specific->codecType = kVideoCodecVP8;
  CodecSpecificInfoVP8* vp8Info = &(codec_specific->codecSpecific.VP8);
  vp8Info->pictureId = picture_id_[stream_idx];
  if (pkt.data.frame.flags & VPX_FRAME_IS_KEY) {
    last_key_frame_picture_id_[stream_idx] = picture_id_[stream_idx];
  }
  vp8Info->simulcastIdx = stream_idx;
  vp8Info->keyIdx = kNoKeyIdx;  // TODO(hlundin) populate this
  vp8Info->nonReference = (pkt.data.frame.flags & VPX_FRAME_IS_DROPPABLE) ?
      true : false;
  bool base_layer_sync_point = (pkt.data.frame.flags & VPX_FRAME_IS_KEY) ||
                                only_predicting_from_key_frame;
  temporal_layers_[stream_idx]->PopulateCodecSpecific(base_layer_sync_point,
                                                      vp8Info,
                                                      timestamp);
  // Prepare next.
  picture_id_[stream_idx] = (picture_id_[stream_idx] + 1) & 0x7FFF;
}

int VP8EncoderImpl::GetEncodedPartitions(
    const I420VideoFrame& input_image,
    bool only_predicting_from_key_frame) {
  int stream_idx = static_cast<int>(encoders_.size()) - 1;
  for (size_t encoder_idx = 0; encoder_idx < encoders_.size();
      ++encoder_idx, --stream_idx) {
    vpx_codec_iter_t iter = NULL;
    int part_idx = 0;
    encoded_images_[encoder_idx]._length = 0;
    encoded_images_[encoder_idx]._frameType = kDeltaFrame;
    RTPFragmentationHeader frag_info;
    // token_partitions_ is number of bits used.
    frag_info.VerifyAndAllocateFragmentationHeader((1 << token_partitions_)
                                                   + 1);
    CodecSpecificInfo codec_specific;
    const vpx_codec_cx_pkt_t *pkt = NULL;
    while ((pkt = vpx_codec_get_cx_data(&encoders_[encoder_idx],
                                        &iter)) != NULL) {
      switch (pkt->kind) {
        case VPX_CODEC_CX_FRAME_PKT: {
          uint32_t length = encoded_images_[encoder_idx]._length;
          memcpy(&encoded_images_[encoder_idx]._buffer[length],
                 pkt->data.frame.buf,
                 pkt->data.frame.sz);
          frag_info.fragmentationOffset[part_idx] = length;
          frag_info.fragmentationLength[part_idx] =  pkt->data.frame.sz;
          frag_info.fragmentationPlType[part_idx] = 0;  // not known here
          frag_info.fragmentationTimeDiff[part_idx] = 0;
          encoded_images_[encoder_idx]._length += pkt->data.frame.sz;
          assert(length <= encoded_images_[encoder_idx]._size);
          ++part_idx;
          break;
        }
        default:
          break;
      }
      // End of frame
      if ((pkt->data.frame.flags & VPX_FRAME_IS_FRAGMENT) == 0) {
        // check if encoded frame is a key frame
        if (pkt->data.frame.flags & VPX_FRAME_IS_KEY) {
          encoded_images_[encoder_idx]._frameType = kKeyFrame;
          rps_.EncodedKeyFrame(picture_id_[stream_idx]);
        }
        PopulateCodecSpecific(&codec_specific, *pkt, stream_idx,
                              input_image.timestamp(),
                              only_predicting_from_key_frame);
        break;
      }
    }
    encoded_images_[encoder_idx]._timeStamp = input_image.timestamp();
    encoded_images_[encoder_idx].capture_time_ms_ =
        input_image.render_time_ms();
    temporal_layers_[stream_idx]->FrameEncoded(
        encoded_images_[encoder_idx]._length,
        encoded_images_[encoder_idx]._timeStamp);
    if (send_stream_[stream_idx]) {
      if (encoded_images_[encoder_idx]._length > 0) {
        TRACE_COUNTER_ID1("webrtc", "EncodedFrameSize", encoder_idx,
                          encoded_images_[encoder_idx]._length);
        encoded_images_[encoder_idx]._encodedHeight =
            codec_.simulcastStream[stream_idx].height;
        encoded_images_[encoder_idx]._encodedWidth =
            codec_.simulcastStream[stream_idx].width;
        encoded_complete_callback_->Encoded(encoded_images_[encoder_idx],
                                            &codec_specific, &frag_info);
      }
    } else {
      // Required in case padding is applied to dropped frames.
      encoded_images_[encoder_idx]._length = 0;
      encoded_images_[encoder_idx]._frameType = kSkipFrame;
      codec_specific.codecType = kVideoCodecVP8;
      CodecSpecificInfoVP8* vp8Info = &(codec_specific.codecSpecific.VP8);
      vp8Info->pictureId = picture_id_[stream_idx];
      vp8Info->simulcastIdx = stream_idx;
      vp8Info->keyIdx = kNoKeyIdx;
      encoded_complete_callback_->Encoded(encoded_images_[encoder_idx],
                                          &codec_specific, NULL);
    }
  }
  if (encoders_.size() == 1 && send_stream_[0]) {
    if (encoded_images_[0]._length > 0) {
      int qp;
      vpx_codec_control(&encoders_[0], VP8E_GET_LAST_QUANTIZER_64, &qp);
      quality_scaler_.ReportEncodedFrame(qp);
    } else {
      quality_scaler_.ReportDroppedFrame();
    }
  }
  return WEBRTC_VIDEO_CODEC_OK;
}

int VP8EncoderImpl::SetChannelParameters(uint32_t packetLoss, int64_t rtt) {
  rps_.SetRtt(rtt);
  return WEBRTC_VIDEO_CODEC_OK;
}

int VP8EncoderImpl::RegisterEncodeCompleteCallback(
    EncodedImageCallback* callback) {
  encoded_complete_callback_ = callback;
  return WEBRTC_VIDEO_CODEC_OK;
}


VP8DecoderImpl::VP8DecoderImpl()
    : decode_complete_callback_(NULL),
      inited_(false),
      feedback_mode_(false),
      decoder_(NULL),
      last_keyframe_(),
      image_format_(VPX_IMG_FMT_NONE),
      ref_frame_(NULL),
      propagation_cnt_(-1),
      last_frame_width_(0),
      last_frame_height_(0),
      key_frame_required_(true) {
}

VP8DecoderImpl::~VP8DecoderImpl() {
  inited_ = true;  // in order to do the actual release
  Release();
}

int VP8DecoderImpl::Reset() {
  if (!inited_) {
    return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  }
  InitDecode(&codec_, 1);
  propagation_cnt_ = -1;
  return WEBRTC_VIDEO_CODEC_OK;
}

int VP8DecoderImpl::InitDecode(const VideoCodec* inst,
                                       int number_of_cores) {
  int ret_val = Release();
  if (ret_val < 0) {
    return ret_val;
  }
  if (decoder_ == NULL) {
    decoder_ = new vpx_codec_ctx_t;
  }
  if (inst && inst->codecType == kVideoCodecVP8) {
    feedback_mode_ = inst->codecSpecific.VP8.feedbackModeOn;
  }
  vpx_codec_dec_cfg_t  cfg;
  // Setting number of threads to a constant value (1)
  cfg.threads = 1;
  cfg.h = cfg.w = 0;  // set after decode

vpx_codec_flags_t flags = 0;
#ifndef WEBRTC_ARCH_ARM
  flags = VPX_CODEC_USE_POSTPROC;
#ifdef INDEPENDENT_PARTITIONS
  flags |= VPX_CODEC_USE_INPUT_PARTITION;
#endif
#endif

  if (vpx_codec_dec_init(decoder_, vpx_codec_vp8_dx(), &cfg, flags)) {
    return WEBRTC_VIDEO_CODEC_MEMORY;
  }

  // Save VideoCodec instance for later; mainly for duplicating the decoder.
  if (&codec_ != inst)
    codec_ = *inst;
  propagation_cnt_ = -1;

  inited_ = true;

  // Always start with a complete key frame.
  key_frame_required_ = true;
  return WEBRTC_VIDEO_CODEC_OK;
}

int VP8DecoderImpl::Decode(const EncodedImage& input_image,
                                   bool missing_frames,
                                   const RTPFragmentationHeader* fragmentation,
                                   const CodecSpecificInfo* codec_specific_info,
                                   int64_t /*render_time_ms*/) {
  if (!inited_) {
    return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  }
  if (decode_complete_callback_ == NULL) {
    return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  }
  if (input_image._buffer == NULL && input_image._length > 0) {
    // Reset to avoid requesting key frames too often.
    if (propagation_cnt_ > 0)
      propagation_cnt_ = 0;
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }

#ifdef INDEPENDENT_PARTITIONS
  if (fragmentation == NULL) {
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }
#endif

#ifndef WEBRTC_ARCH_ARM
  vp8_postproc_cfg_t ppcfg;
  // MFQE enabled to reduce key frame popping.
  ppcfg.post_proc_flag = VP8_MFQE | VP8_DEBLOCK;
  // For VGA resolutions and lower, enable the demacroblocker postproc.
  if (last_frame_width_ * last_frame_height_ <= 640 * 360) {
    ppcfg.post_proc_flag |= VP8_DEMACROBLOCK;
  }
  // Strength of deblocking filter. Valid range:[0,16]
  ppcfg.deblocking_level = 3;
  vpx_codec_control(decoder_, VP8_SET_POSTPROC, &ppcfg);
#endif

  // Always start with a complete key frame.
  if (key_frame_required_) {
    if (input_image._frameType != kKeyFrame)
      return WEBRTC_VIDEO_CODEC_ERROR;
    // We have a key frame - is it complete?
    if (input_image._completeFrame) {
      key_frame_required_ = false;
    } else {
      return WEBRTC_VIDEO_CODEC_ERROR;
    }
  }
  // Restrict error propagation using key frame requests. Disabled when
  // the feedback mode is enabled (RPS).
  // Reset on a key frame refresh.
  if (!feedback_mode_) {
    if (input_image._frameType == kKeyFrame && input_image._completeFrame) {
      propagation_cnt_ = -1;
    // Start count on first loss.
    } else if ((!input_image._completeFrame || missing_frames) &&
        propagation_cnt_ == -1) {
      propagation_cnt_ = 0;
    }
    if (propagation_cnt_ >= 0) {
      propagation_cnt_++;
    }
  }

  vpx_codec_iter_t iter = NULL;
  vpx_image_t* img;
  int ret;

  // Check for missing frames.
  if (missing_frames) {
    // Call decoder with zero data length to signal missing frames.
    if (vpx_codec_decode(decoder_, NULL, 0, 0, VPX_DL_REALTIME)) {
      // Reset to avoid requesting key frames too often.
      if (propagation_cnt_ > 0)
        propagation_cnt_ = 0;
      return WEBRTC_VIDEO_CODEC_ERROR;
    }
    img = vpx_codec_get_frame(decoder_, &iter);
    iter = NULL;
  }

#ifdef INDEPENDENT_PARTITIONS
  if (DecodePartitions(inputImage, fragmentation)) {
    // Reset to avoid requesting key frames too often.
    if (propagation_cnt_ > 0) {
      propagation_cnt_ = 0;
    }
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
#else
  uint8_t* buffer = input_image._buffer;
  if (input_image._length == 0) {
    buffer = NULL;  // Triggers full frame concealment.
  }
  if (vpx_codec_decode(decoder_, buffer, input_image._length, 0,
                       VPX_DL_REALTIME)) {
    // Reset to avoid requesting key frames too often.
    if (propagation_cnt_ > 0) {
      propagation_cnt_ = 0;
    }
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
#endif

  // Store encoded frame if key frame. (Used in Copy method.)
  if (input_image._frameType == kKeyFrame && input_image._buffer != NULL) {
    const uint32_t bytes_to_copy = input_image._length;
    if (last_keyframe_._size < bytes_to_copy) {
      delete [] last_keyframe_._buffer;
      last_keyframe_._buffer = NULL;
      last_keyframe_._size = 0;
    }
    uint8_t* temp_buffer = last_keyframe_._buffer;  // Save buffer ptr.
    uint32_t temp_size = last_keyframe_._size;  // Save size.
    last_keyframe_ = input_image;  // Shallow copy.
    last_keyframe_._buffer = temp_buffer;  // Restore buffer ptr.
    last_keyframe_._size = temp_size;  // Restore buffer size.
    if (!last_keyframe_._buffer) {
      // Allocate memory.
      last_keyframe_._size = bytes_to_copy;
      last_keyframe_._buffer = new uint8_t[last_keyframe_._size];
    }
    // Copy encoded frame.
    memcpy(last_keyframe_._buffer, input_image._buffer, bytes_to_copy);
    last_keyframe_._length = bytes_to_copy;
  }

  img = vpx_codec_get_frame(decoder_, &iter);
  ret = ReturnFrame(img, input_image._timeStamp, input_image.ntp_time_ms_);
  if (ret != 0) {
    // Reset to avoid requesting key frames too often.
    if (ret < 0 && propagation_cnt_ > 0)
      propagation_cnt_ = 0;
    return ret;
  }
  if (feedback_mode_) {
    // Whenever we receive an incomplete key frame all reference buffers will
    // be corrupt. If that happens we must request new key frames until we
    // decode a complete key frame.
    if (input_image._frameType == kKeyFrame && !input_image._completeFrame)
      return WEBRTC_VIDEO_CODEC_ERROR;
    // Check for reference updates and last reference buffer corruption and
    // signal successful reference propagation or frame corruption to the
    // encoder.
    int reference_updates = 0;
    if (vpx_codec_control(decoder_, VP8D_GET_LAST_REF_UPDATES,
                          &reference_updates)) {
      // Reset to avoid requesting key frames too often.
      if (propagation_cnt_ > 0) {
        propagation_cnt_ = 0;
      }
      return WEBRTC_VIDEO_CODEC_ERROR;
    }
    int corrupted = 0;
    if (vpx_codec_control(decoder_, VP8D_GET_FRAME_CORRUPTED, &corrupted)) {
      // Reset to avoid requesting key frames too often.
      if (propagation_cnt_ > 0)
        propagation_cnt_ = 0;
      return WEBRTC_VIDEO_CODEC_ERROR;
    }
    int16_t picture_id = -1;
    if (codec_specific_info) {
      picture_id = codec_specific_info->codecSpecific.VP8.pictureId;
    }
    if (picture_id > -1) {
      if (((reference_updates & VP8_GOLD_FRAME) ||
          (reference_updates & VP8_ALTR_FRAME)) && !corrupted) {
        decode_complete_callback_->ReceivedDecodedReferenceFrame(picture_id);
      }
      decode_complete_callback_->ReceivedDecodedFrame(picture_id);
    }
    if (corrupted) {
      // we can decode but with artifacts
      return WEBRTC_VIDEO_CODEC_REQUEST_SLI;
    }
  }
  // Check Vs. threshold
  if (propagation_cnt_ > kVp8ErrorPropagationTh) {
    // Reset to avoid requesting key frames too often.
    propagation_cnt_ = 0;
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
  return WEBRTC_VIDEO_CODEC_OK;
}

int VP8DecoderImpl::DecodePartitions(
    const EncodedImage& input_image,
    const RTPFragmentationHeader* fragmentation) {
  for (int i = 0; i < fragmentation->fragmentationVectorSize; ++i) {
    const uint8_t* partition = input_image._buffer +
        fragmentation->fragmentationOffset[i];
    const uint32_t partition_length =
        fragmentation->fragmentationLength[i];
    if (vpx_codec_decode(decoder_,
                         partition,
                         partition_length,
                         0,
                         VPX_DL_REALTIME)) {
      return WEBRTC_VIDEO_CODEC_ERROR;
    }
  }
  // Signal end of frame data. If there was no frame data this will trigger
  // a full frame concealment.
  if (vpx_codec_decode(decoder_, NULL, 0, 0, VPX_DL_REALTIME))
    return WEBRTC_VIDEO_CODEC_ERROR;
  return WEBRTC_VIDEO_CODEC_OK;
}

int VP8DecoderImpl::ReturnFrame(const vpx_image_t* img,
                                        uint32_t timestamp,
                                        int64_t ntp_time_ms) {
  if (img == NULL) {
    // Decoder OK and NULL image => No show frame
    return WEBRTC_VIDEO_CODEC_NO_OUTPUT;
  }
  last_frame_width_ = img->d_w;
  last_frame_height_ = img->d_h;
  // Allocate memory for decoded image.
  I420VideoFrame decoded_image(buffer_pool_.CreateBuffer(img->d_w, img->d_h),
                               timestamp, 0, kVideoRotation_0);
  libyuv::I420Copy(
      img->planes[VPX_PLANE_Y], img->stride[VPX_PLANE_Y],
      img->planes[VPX_PLANE_U], img->stride[VPX_PLANE_U],
      img->planes[VPX_PLANE_V], img->stride[VPX_PLANE_V],
      decoded_image.buffer(kYPlane), decoded_image.stride(kYPlane),
      decoded_image.buffer(kUPlane), decoded_image.stride(kUPlane),
      decoded_image.buffer(kVPlane), decoded_image.stride(kVPlane),
      img->d_w, img->d_h);
  decoded_image.set_ntp_time_ms(ntp_time_ms);
  int ret = decode_complete_callback_->Decoded(decoded_image);
  if (ret != 0)
    return ret;

  // Remember image format for later
  image_format_ = img->fmt;
  return WEBRTC_VIDEO_CODEC_OK;
}

int VP8DecoderImpl::RegisterDecodeCompleteCallback(
    DecodedImageCallback* callback) {
  decode_complete_callback_ = callback;
  return WEBRTC_VIDEO_CODEC_OK;
}

int VP8DecoderImpl::Release() {
  if (last_keyframe_._buffer != NULL) {
    delete [] last_keyframe_._buffer;
    last_keyframe_._buffer = NULL;
  }
  if (decoder_ != NULL) {
    if (vpx_codec_destroy(decoder_)) {
      return WEBRTC_VIDEO_CODEC_MEMORY;
    }
    delete decoder_;
    decoder_ = NULL;
  }
  if (ref_frame_ != NULL) {
    vpx_img_free(&ref_frame_->img);
    delete ref_frame_;
    ref_frame_ = NULL;
  }
  buffer_pool_.Release();
  inited_ = false;
  return WEBRTC_VIDEO_CODEC_OK;
}

VideoDecoder* VP8DecoderImpl::Copy() {
  // Sanity checks.
  if (!inited_) {
    // Not initialized.
    assert(false);
    return NULL;
  }
  if (last_frame_width_ == 0 || last_frame_height_ == 0) {
    // Nothing has been decoded before; cannot clone.
    return NULL;
  }
  if (last_keyframe_._buffer == NULL) {
    // Cannot clone if we have no key frame to start with.
    return NULL;
  }
  // Create a new VideoDecoder object
  VP8DecoderImpl* copy = new VP8DecoderImpl;

  // Initialize the new decoder
  if (copy->InitDecode(&codec_, 1) != WEBRTC_VIDEO_CODEC_OK) {
    delete copy;
    return NULL;
  }
  // Inject last key frame into new decoder.
  if (vpx_codec_decode(copy->decoder_, last_keyframe_._buffer,
                       last_keyframe_._length, NULL, VPX_DL_REALTIME)) {
    delete copy;
    return NULL;
  }
  // Allocate memory for reference image copy
  assert(last_frame_width_ > 0);
  assert(last_frame_height_ > 0);
  assert(image_format_ > VPX_IMG_FMT_NONE);
  // Check if frame format has changed.
  if (ref_frame_ &&
      (last_frame_width_ != static_cast<int>(ref_frame_->img.d_w) ||
          last_frame_height_ != static_cast<int>(ref_frame_->img.d_h) ||
          image_format_ != ref_frame_->img.fmt)) {
    vpx_img_free(&ref_frame_->img);
    delete ref_frame_;
    ref_frame_ = NULL;
  }


  if (!ref_frame_) {
    ref_frame_ = new vpx_ref_frame_t;
    // Setting alignment to 32 - as that ensures at least 16 for all
    // planes (32 for Y, 16 for U,V) - libvpx sets the requested stride
    // for the y plane, but only half of it to the u and v planes.
    if (!vpx_img_alloc(&ref_frame_->img,
                       static_cast<vpx_img_fmt_t>(image_format_),
                       last_frame_width_, last_frame_height_,
                       kVp832ByteAlign)) {
      assert(false);
      delete copy;
      return NULL;
    }
  }
  const vpx_ref_frame_type_t type_vec[] = { VP8_LAST_FRAME, VP8_GOLD_FRAME,
      VP8_ALTR_FRAME };
  for (uint32_t ix = 0;
      ix < sizeof(type_vec) / sizeof(vpx_ref_frame_type_t); ++ix) {
    ref_frame_->frame_type = type_vec[ix];
    if (CopyReference(copy) < 0) {
      delete copy;
      return NULL;
    }
  }
  // Copy all member variables (that are not set in initialization).
  copy->feedback_mode_ = feedback_mode_;
  copy->image_format_ = image_format_;
  copy->last_keyframe_ = last_keyframe_;  // Shallow copy.
  // Allocate memory. (Discard copied _buffer pointer.)
  copy->last_keyframe_._buffer = new uint8_t[last_keyframe_._size];
  memcpy(copy->last_keyframe_._buffer, last_keyframe_._buffer,
         last_keyframe_._length);

  return static_cast<VideoDecoder*>(copy);
}

int VP8DecoderImpl::CopyReference(VP8DecoderImpl* copy) {
  // The type of frame to copy should be set in ref_frame_->frame_type
  // before the call to this function.
  if (vpx_codec_control(decoder_, VP8_COPY_REFERENCE, ref_frame_)
      != VPX_CODEC_OK) {
    return -1;
  }
  if (vpx_codec_control(copy->decoder_, VP8_SET_REFERENCE, ref_frame_)
      != VPX_CODEC_OK) {
    return -1;
  }
  return 0;
}

}  // namespace webrtc
