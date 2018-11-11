/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "sdk/android/src/jni/videoencoderwrapper.h"

#include <utility>

#include "common_video/h264/h264_common.h"
#include "modules/include/module_common_types.h"
#include "modules/video_coding/include/video_codec_interface.h"
#include "modules/video_coding/include/video_error_codes.h"
#include "modules/video_coding/utility/vp8_header_parser.h"
#include "modules/video_coding/utility/vp9_uncompressed_header_parser.h"
#include "rtc_base/logging.h"
#include "rtc_base/random.h"
#include "rtc_base/timeutils.h"
#include "sdk/android/generated_video_jni/jni/VideoEncoderWrapper_jni.h"
#include "sdk/android/generated_video_jni/jni/VideoEncoder_jni.h"
#include "sdk/android/src/jni/class_loader.h"
#include "sdk/android/src/jni/encodedimage.h"
#include "sdk/android/src/jni/videocodecstatus.h"

namespace webrtc {
namespace jni {

static const int kMaxJavaEncoderResets = 3;

VideoEncoderWrapper::VideoEncoderWrapper(JNIEnv* jni, jobject j_encoder)
    : encoder_(jni, j_encoder),
      frame_type_class_(jni,
                        GetClass(jni, "org/webrtc/EncodedImage$FrameType")),
      int_array_class_(jni, jni->FindClass("[I")) {
  implementation_name_ = GetImplementationName(jni);

  initialized_ = false;
  num_resets_ = 0;

  Random random(rtc::TimeMicros());
  picture_id_ = random.Rand<uint16_t>() & 0x7FFF;
  tl0_pic_idx_ = random.Rand<uint8_t>();
}

int32_t VideoEncoderWrapper::InitEncode(const VideoCodec* codec_settings,
                                        int32_t number_of_cores,
                                        size_t max_payload_size) {
  JNIEnv* jni = AttachCurrentThreadIfNeeded();
  ScopedLocalRefFrame local_ref_frame(jni);

  number_of_cores_ = number_of_cores;
  codec_settings_ = *codec_settings;
  num_resets_ = 0;
  encoder_queue_ = rtc::TaskQueue::Current();

  return InitEncodeInternal(jni);
}

int32_t VideoEncoderWrapper::InitEncodeInternal(JNIEnv* jni) {
  bool automatic_resize_on;
  switch (codec_settings_.codecType) {
    case kVideoCodecVP8:
      automatic_resize_on = codec_settings_.VP8()->automaticResizeOn;
      break;
    case kVideoCodecVP9:
      automatic_resize_on = codec_settings_.VP9()->automaticResizeOn;
      break;
    default:
      automatic_resize_on = true;
  }

  jobject settings = Java_Settings_Constructor(
      jni, number_of_cores_, codec_settings_.width, codec_settings_.height,
      codec_settings_.startBitrate, codec_settings_.maxFramerate,
      automatic_resize_on);

  jobject callback = Java_VideoEncoderWrapper_createEncoderCallback(
      jni, jlongFromPointer(this));

  jobject ret =
      Java_VideoEncoder_initEncode(jni, *encoder_, settings, callback);

  if (JavaToNativeVideoCodecStatus(jni, ret) == WEBRTC_VIDEO_CODEC_OK) {
    initialized_ = true;
  }

  return HandleReturnCode(jni, ret);
}

int32_t VideoEncoderWrapper::RegisterEncodeCompleteCallback(
    EncodedImageCallback* callback) {
  callback_ = callback;
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t VideoEncoderWrapper::Release() {
  JNIEnv* jni = AttachCurrentThreadIfNeeded();
  ScopedLocalRefFrame local_ref_frame(jni);
  jobject ret = Java_VideoEncoder_release(jni, *encoder_);
  frame_extra_infos_.clear();
  initialized_ = false;
  encoder_queue_ = nullptr;
  return HandleReturnCode(jni, ret);
}

int32_t VideoEncoderWrapper::Encode(
    const VideoFrame& frame,
    const CodecSpecificInfo* /* codec_specific_info */,
    const std::vector<FrameType>* frame_types) {
  if (!initialized_) {
    // Most likely initializing the codec failed.
    return WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE;
  }

  JNIEnv* jni = AttachCurrentThreadIfNeeded();
  ScopedLocalRefFrame local_ref_frame(jni);

  // Construct encode info.
  jobjectArray j_frame_types = NativeToJavaFrameTypeArray(jni, *frame_types);
  jobject encode_info = Java_EncodeInfo_Constructor(jni, j_frame_types);

  FrameExtraInfo info;
  info.capture_time_ns = frame.timestamp_us() * rtc::kNumNanosecsPerMicrosec;
  info.timestamp_rtp = frame.timestamp();
  frame_extra_infos_.push_back(info);

  jobject ret = Java_VideoEncoder_encode(
      jni, *encoder_, NativeToJavaFrame(jni, frame), encode_info);
  return HandleReturnCode(jni, ret);
}

int32_t VideoEncoderWrapper::SetChannelParameters(uint32_t packet_loss,
                                                  int64_t rtt) {
  JNIEnv* jni = AttachCurrentThreadIfNeeded();
  ScopedLocalRefFrame local_ref_frame(jni);
  jobject ret = Java_VideoEncoder_setChannelParameters(
      jni, *encoder_, (jshort)packet_loss, (jlong)rtt);
  return HandleReturnCode(jni, ret);
}

int32_t VideoEncoderWrapper::SetRateAllocation(
    const BitrateAllocation& allocation,
    uint32_t framerate) {
  JNIEnv* jni = AttachCurrentThreadIfNeeded();
  ScopedLocalRefFrame local_ref_frame(jni);

  jobject j_bitrate_allocation = ToJavaBitrateAllocation(jni, allocation);
  jobject ret = Java_VideoEncoder_setRateAllocation(
      jni, *encoder_, j_bitrate_allocation, (jint)framerate);
  return HandleReturnCode(jni, ret);
}

VideoEncoderWrapper::ScalingSettings VideoEncoderWrapper::GetScalingSettings()
    const {
  JNIEnv* jni = AttachCurrentThreadIfNeeded();
  ScopedLocalRefFrame local_ref_frame(jni);
  jobject j_scaling_settings =
      Java_VideoEncoder_getScalingSettings(jni, *encoder_);
  bool isOn =
      Java_VideoEncoderWrapper_getScalingSettingsOn(jni, j_scaling_settings);

  rtc::Optional<int> low = JavaToNativeOptionalInt(
      jni,
      Java_VideoEncoderWrapper_getScalingSettingsLow(jni, j_scaling_settings));
  rtc::Optional<int> high = JavaToNativeOptionalInt(
      jni,
      Java_VideoEncoderWrapper_getScalingSettingsHigh(jni, j_scaling_settings));

  return (low && high) ? ScalingSettings(isOn, *low, *high)
                       : ScalingSettings(isOn);
}

const char* VideoEncoderWrapper::ImplementationName() const {
  return implementation_name_.c_str();
}

void VideoEncoderWrapper::OnEncodedFrame(JNIEnv* jni,
                                         jobject j_caller,
                                         jobject j_buffer,
                                         jint encoded_width,
                                         jint encoded_height,
                                         jlong capture_time_ns,
                                         jint frame_type,
                                         jint rotation,
                                         jboolean complete_frame,
                                         jobject j_qp) {
  const uint8_t* buffer =
      static_cast<uint8_t*>(jni->GetDirectBufferAddress(j_buffer));
  const size_t buffer_size = jni->GetDirectBufferCapacity(j_buffer);

  std::vector<uint8_t> buffer_copy(buffer_size);
  memcpy(buffer_copy.data(), buffer, buffer_size);
  const int qp = JavaToNativeOptionalInt(jni, j_qp).value_or(-1);

  encoder_queue_->PostTask(
      [
        this, task_buffer = std::move(buffer_copy), qp, encoded_width,
        encoded_height, capture_time_ns, frame_type, rotation, complete_frame
      ]() {
        FrameExtraInfo frame_extra_info;
        do {
          if (frame_extra_infos_.empty()) {
            RTC_LOG(LS_WARNING)
                << "Java encoder produced an unexpected frame with timestamp: "
                << capture_time_ns;
            return;
          }

          frame_extra_info = frame_extra_infos_.front();
          frame_extra_infos_.pop_front();
          // The encoder might drop frames so iterate through the queue until
          // we find a matching timestamp.
        } while (frame_extra_info.capture_time_ns != capture_time_ns);

        RTPFragmentationHeader header = ParseFragmentationHeader(task_buffer);
        EncodedImage frame(const_cast<uint8_t*>(task_buffer.data()),
                           task_buffer.size(), task_buffer.size());
        frame._encodedWidth = encoded_width;
        frame._encodedHeight = encoded_height;
        frame._timeStamp = frame_extra_info.timestamp_rtp;
        frame.capture_time_ms_ = capture_time_ns / rtc::kNumNanosecsPerMillisec;
        frame._frameType = (FrameType)frame_type;
        frame.rotation_ = (VideoRotation)rotation;
        frame._completeFrame = complete_frame;
        if (qp == -1) {
          frame.qp_ = ParseQp(task_buffer);
        } else {
          frame.qp_ = qp;
        }

        CodecSpecificInfo info(ParseCodecSpecificInfo(frame));
        callback_->OnEncodedImage(frame, &info, &header);
      });
}

int32_t VideoEncoderWrapper::HandleReturnCode(JNIEnv* jni, jobject code) {
  int32_t value = JavaToNativeVideoCodecStatus(jni, code);
  if (value < 0) {  // Any errors are represented by negative values.
    // Try resetting the codec.
    if (++num_resets_ <= kMaxJavaEncoderResets &&
        Release() == WEBRTC_VIDEO_CODEC_OK) {
      RTC_LOG(LS_WARNING) << "Reset Java encoder: " << num_resets_;
      return InitEncodeInternal(jni);
    }

    RTC_LOG(LS_WARNING) << "Falling back to software decoder.";
    return WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE;
  } else {
    return value;
  }
}

RTPFragmentationHeader VideoEncoderWrapper::ParseFragmentationHeader(
    const std::vector<uint8_t>& buffer) {
  RTPFragmentationHeader header;
  if (codec_settings_.codecType == kVideoCodecH264) {
    h264_bitstream_parser_.ParseBitstream(buffer.data(), buffer.size());

    // For H.264 search for start codes.
    const std::vector<H264::NaluIndex> nalu_idxs =
        H264::FindNaluIndices(buffer.data(), buffer.size());
    if (nalu_idxs.empty()) {
      RTC_LOG(LS_ERROR) << "Start code is not found!";
      RTC_LOG(LS_ERROR) << "Data:" << buffer[0] << " " << buffer[1] << " "
                        << buffer[2] << " " << buffer[3] << " " << buffer[4]
                        << " " << buffer[5];
    }
    header.VerifyAndAllocateFragmentationHeader(nalu_idxs.size());
    for (size_t i = 0; i < nalu_idxs.size(); i++) {
      header.fragmentationOffset[i] = nalu_idxs[i].payload_start_offset;
      header.fragmentationLength[i] = nalu_idxs[i].payload_size;
      header.fragmentationPlType[i] = 0;
      header.fragmentationTimeDiff[i] = 0;
    }
  } else {
    // Generate a header describing a single fragment.
    header.VerifyAndAllocateFragmentationHeader(1);
    header.fragmentationOffset[0] = 0;
    header.fragmentationLength[0] = buffer.size();
    header.fragmentationPlType[0] = 0;
    header.fragmentationTimeDiff[0] = 0;
  }
  return header;
}

int VideoEncoderWrapper::ParseQp(const std::vector<uint8_t>& buffer) {
  int qp;
  bool success;
  switch (codec_settings_.codecType) {
    case kVideoCodecVP8:
      success = vp8::GetQp(buffer.data(), buffer.size(), &qp);
      break;
    case kVideoCodecVP9:
      success = vp9::GetQp(buffer.data(), buffer.size(), &qp);
      break;
    case kVideoCodecH264:
      success = h264_bitstream_parser_.GetLastSliceQp(&qp);
      break;
    default:  // Default is to not provide QP.
      success = false;
      break;
  }
  return success ? qp : -1;  // -1 means unknown QP.
}

CodecSpecificInfo VideoEncoderWrapper::ParseCodecSpecificInfo(
    const EncodedImage& frame) {
  const bool key_frame = frame._frameType == kVideoFrameKey;

  CodecSpecificInfo info;
  memset(&info, 0, sizeof(info));
  info.codecType = codec_settings_.codecType;
  info.codec_name = implementation_name_.c_str();

  switch (codec_settings_.codecType) {
    case kVideoCodecVP8:
      info.codecSpecific.VP8.pictureId = picture_id_;
      info.codecSpecific.VP8.nonReference = false;
      info.codecSpecific.VP8.simulcastIdx = 0;
      info.codecSpecific.VP8.temporalIdx = kNoTemporalIdx;
      info.codecSpecific.VP8.layerSync = false;
      info.codecSpecific.VP8.tl0PicIdx = kNoTl0PicIdx;
      info.codecSpecific.VP8.keyIdx = kNoKeyIdx;
      break;
    case kVideoCodecVP9:
      if (key_frame) {
        gof_idx_ = 0;
      }
      info.codecSpecific.VP9.picture_id = picture_id_;
      info.codecSpecific.VP9.inter_pic_predicted = key_frame ? false : true;
      info.codecSpecific.VP9.flexible_mode = false;
      info.codecSpecific.VP9.ss_data_available = key_frame ? true : false;
      info.codecSpecific.VP9.tl0_pic_idx = tl0_pic_idx_++;
      info.codecSpecific.VP9.temporal_idx = kNoTemporalIdx;
      info.codecSpecific.VP9.spatial_idx = kNoSpatialIdx;
      info.codecSpecific.VP9.temporal_up_switch = true;
      info.codecSpecific.VP9.inter_layer_predicted = false;
      info.codecSpecific.VP9.gof_idx =
          static_cast<uint8_t>(gof_idx_++ % gof_.num_frames_in_gof);
      info.codecSpecific.VP9.num_spatial_layers = 1;
      info.codecSpecific.VP9.spatial_layer_resolution_present = false;
      if (info.codecSpecific.VP9.ss_data_available) {
        info.codecSpecific.VP9.spatial_layer_resolution_present = true;
        info.codecSpecific.VP9.width[0] = frame._encodedWidth;
        info.codecSpecific.VP9.height[0] = frame._encodedHeight;
        info.codecSpecific.VP9.gof.CopyGofInfoVP9(gof_);
      }
      break;
    default:
      break;
  }

  picture_id_ = (picture_id_ + 1) & 0x7FFF;

  return info;
}

jobject VideoEncoderWrapper::ToJavaBitrateAllocation(
    JNIEnv* jni,
    const BitrateAllocation& allocation) {
  jobjectArray j_allocation_array = jni->NewObjectArray(
      kMaxSpatialLayers, *int_array_class_, nullptr /* initial */);
  for (int spatial_i = 0; spatial_i < kMaxSpatialLayers; ++spatial_i) {
    jintArray j_array_spatial_layer = jni->NewIntArray(kMaxTemporalStreams);
    jint* array_spatial_layer =
        jni->GetIntArrayElements(j_array_spatial_layer, nullptr /* isCopy */);
    for (int temporal_i = 0; temporal_i < kMaxTemporalStreams; ++temporal_i) {
      array_spatial_layer[temporal_i] =
          allocation.GetBitrate(spatial_i, temporal_i);
    }
    jni->ReleaseIntArrayElements(j_array_spatial_layer, array_spatial_layer,
                                 JNI_COMMIT);

    jni->SetObjectArrayElement(j_allocation_array, spatial_i,
                               j_array_spatial_layer);
  }
  return Java_BitrateAllocation_Constructor(jni, j_allocation_array);
}

std::string VideoEncoderWrapper::GetImplementationName(JNIEnv* jni) const {
  jstring jname = Java_VideoEncoder_getImplementationName(jni, *encoder_);
  return JavaToStdString(jni, jname);
}

}  // namespace jni
}  // namespace webrtc
