/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebrtcGmpVideoCodec.h"

#include <utility>
#include <vector>

#include "GMPLog.h"
#include "GMPUtils.h"
#include "MainThreadUtils.h"
#include "VideoConduit.h"
#include "gmp-video-frame-encoded.h"
#include "gmp-video-frame-i420.h"
#include "mozilla/CheckedInt.h"
#include "nsServiceManagerUtils.h"
#include "api/video/video_frame_type.h"
#include "common_video/include/video_frame_buffer.h"
#include "media/base/media_constants.h"
#include "modules/video_coding/include/video_codec_interface.h"
#include "modules/video_coding/svc/create_scalability_structure.h"

namespace mozilla {

using detail::InputImageData;

// QP scaling thresholds.
static const int kLowH264QpThreshold = 24;
static const int kHighH264QpThreshold = 37;

// Encoder.
WebrtcGmpVideoEncoder::WebrtcGmpVideoEncoder(
    const webrtc::SdpVideoFormat& aFormat, std::string aPCHandle)
    : mGMP(nullptr),
      mInitting(false),
      mConfiguredBitrateKbps(0),
      mHost(nullptr),
      mMaxPayloadSize(0),
      mNeedKeyframe(true),
      mSyncLayerCap(webrtc::kMaxTemporalStreams),
      mFormatParams(aFormat.parameters),
      mCallbackMutex("WebrtcGmpVideoEncoder encoded callback mutex"),
      mCallback(nullptr),
      mPCHandle(std::move(aPCHandle)) {
  mCodecParams.mCodecType = kGMPVideoCodecInvalid;
  mCodecParams.mMode = kGMPCodecModeInvalid;
  mCodecParams.mLogLevel = GetGMPLibraryLogLevel();
  MOZ_ASSERT(!mPCHandle.empty());
}

WebrtcGmpVideoEncoder::~WebrtcGmpVideoEncoder() {
  // We should not have been destroyed if we never closed our GMP
  MOZ_ASSERT(!mGMP);
}

static int WebrtcFrameTypeToGmpFrameType(webrtc::VideoFrameType aIn,
                                         GMPVideoFrameType* aOut) {
  MOZ_ASSERT(aOut);
  switch (aIn) {
    case webrtc::VideoFrameType::kVideoFrameKey:
      *aOut = kGMPKeyFrame;
      break;
    case webrtc::VideoFrameType::kVideoFrameDelta:
      *aOut = kGMPDeltaFrame;
      break;
    case webrtc::VideoFrameType::kEmptyFrame:
      *aOut = kGMPSkipFrame;
      break;
    default:
      MOZ_CRASH("Unexpected webrtc::FrameType");
  }

  return WEBRTC_VIDEO_CODEC_OK;
}

static int GmpFrameTypeToWebrtcFrameType(GMPVideoFrameType aIn,
                                         webrtc::VideoFrameType* aOut) {
  MOZ_ASSERT(aOut);
  switch (aIn) {
    case kGMPKeyFrame:
      *aOut = webrtc::VideoFrameType::kVideoFrameKey;
      break;
    case kGMPDeltaFrame:
      *aOut = webrtc::VideoFrameType::kVideoFrameDelta;
      break;
    case kGMPSkipFrame:
      *aOut = webrtc::VideoFrameType::kEmptyFrame;
      break;
    default:
      MOZ_CRASH("Unexpected GMPVideoFrameType");
  }

  return WEBRTC_VIDEO_CODEC_OK;
}

static webrtc::ScalabilityMode GmpCodecParamsToScalabilityMode(
    const GMPVideoCodec& aParams) {
  switch (aParams.mTemporalLayerNum) {
    case 1:
      return webrtc::ScalabilityMode::kL1T1;
    case 2:
      return webrtc::ScalabilityMode::kL1T2;
    case 3:
      return webrtc::ScalabilityMode::kL1T3;
    default:
      NS_WARNING(nsPrintfCString("Expected 1-3 temporal layers but got %d.\n",
                                 aParams.mTemporalLayerNum)
                     .get());
      MOZ_CRASH("Unexpected number of temporal layers");
  }
}

int32_t WebrtcGmpVideoEncoder::InitEncode(
    const webrtc::VideoCodec* aCodecSettings,
    const webrtc::VideoEncoder::Settings& aSettings) {
  if (!mEncodeQueue) {
    mEncodeQueue.emplace(GetCurrentSerialEventTarget());
  }
  mEncodeQueue->AssertOnCurrentThread();

  if (!mMPS) {
    mMPS = do_GetService("@mozilla.org/gecko-media-plugin-service;1");
  }
  MOZ_ASSERT(mMPS);

  if (!mGMPThread) {
    if (NS_WARN_IF(NS_FAILED(mMPS->GetThread(getter_AddRefs(mGMPThread))))) {
      return WEBRTC_VIDEO_CODEC_ERROR;
    }
  }

  if (aCodecSettings->numberOfSimulcastStreams > 1) {
    // Simulcast not implemented for GMP-H264
    return WEBRTC_VIDEO_CODEC_ERR_SIMULCAST_PARAMETERS_NOT_SUPPORTED;
  }

  if (aCodecSettings->simulcastStream[0].numberOfTemporalLayers > 1 &&
      !HaveGMPFor("encode-video"_ns, {"moz-h264-temporal-svc"_ns})) {
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }

  GMPVideoCodec codecParams{};
  codecParams.mGMPApiVersion = kGMPVersion36;
  codecParams.mLogLevel = GetGMPLibraryLogLevel();
  codecParams.mStartBitrate = aCodecSettings->startBitrate;
  codecParams.mMinBitrate = aCodecSettings->minBitrate;
  codecParams.mMaxBitrate = aCodecSettings->maxBitrate;
  codecParams.mMaxFramerate = aCodecSettings->maxFramerate;
  codecParams.mFrameDroppingOn = aCodecSettings->GetFrameDropEnabled();
  codecParams.mTemporalLayerNum =
      aCodecSettings->simulcastStream[0].GetNumberOfTemporalLayers();
  if (aCodecSettings->mode == webrtc::VideoCodecMode::kScreensharing) {
    codecParams.mMode = kGMPScreensharing;
  } else {
    codecParams.mMode = kGMPRealtimeVideo;
  }
  codecParams.mWidth = aCodecSettings->width;
  codecParams.mHeight = aCodecSettings->height;

  uint32_t maxPayloadSize = aSettings.max_payload_size;
  if (mFormatParams.count(cricket::kH264FmtpPacketizationMode) == 1 &&
      mFormatParams.at(cricket::kH264FmtpPacketizationMode) == "1") {
    maxPayloadSize = 0;  // No limit, use FUAs
  }

  mConfiguredBitrateKbps = codecParams.mMaxBitrate;

  MOZ_ALWAYS_SUCCEEDS(
      mGMPThread->Dispatch(NewRunnableMethod<GMPVideoCodec, int32_t, uint32_t>(
          __func__, this, &WebrtcGmpVideoEncoder::InitEncode_g, codecParams,
          aSettings.number_of_cores, maxPayloadSize)));

  // Since init of the GMP encoder is a multi-step async dispatch (including
  // dispatches to main), and since this function is invoked on main, there's
  // no safe way to block until this init is done. If an error occurs, we'll
  // handle it later.
  return WEBRTC_VIDEO_CODEC_OK;
}

void WebrtcGmpVideoEncoder::InitEncode_g(const GMPVideoCodec& aCodecParams,
                                         int32_t aNumberOfCores,
                                         uint32_t aMaxPayloadSize) {
  nsTArray<nsCString> tags;
  tags.AppendElement("h264"_ns);
  UniquePtr<GetGMPVideoEncoderCallback> callback(
      new InitDoneCallback(this, aCodecParams));
  mInitting = true;
  mMaxPayloadSize = aMaxPayloadSize;
  mSyncLayerCap = aCodecParams.mTemporalLayerNum;
  mSvcController = webrtc::CreateScalabilityStructure(
      GmpCodecParamsToScalabilityMode(aCodecParams));
  if (!mSvcController) {
    GMP_LOG_DEBUG(
        "GMP Encode: CreateScalabilityStructure for %d temporal layers failed",
        aCodecParams.mTemporalLayerNum);
    Close_g();
    NotifyGmpInitDone(mPCHandle, WEBRTC_VIDEO_CODEC_ERROR,
                      "GMP Encode: CreateScalabilityStructure failed");
    return;
  }
  nsresult rv =
      mMPS->GetGMPVideoEncoder(nullptr, &tags, ""_ns, std::move(callback));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    GMP_LOG_DEBUG("GMP Encode: GetGMPVideoEncoder failed");
    Close_g();
    NotifyGmpInitDone(mPCHandle, WEBRTC_VIDEO_CODEC_ERROR,
                      "GMP Encode: GetGMPVideoEncoder failed");
  }
}

int32_t WebrtcGmpVideoEncoder::GmpInitDone_g(GMPVideoEncoderProxy* aGMP,
                                             GMPVideoHost* aHost,
                                             std::string* aErrorOut) {
  if (!mInitting || !aGMP || !aHost) {
    *aErrorOut =
        "GMP Encode: Either init was aborted, "
        "or init failed to supply either a GMP Encoder or GMP host.";
    if (aGMP) {
      // This could destroy us, since aGMP may be the last thing holding a ref
      // Return immediately.
      aGMP->Close();
    }
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  mInitting = false;

  if (mGMP && mGMP != aGMP) {
    Close_g();
  }

  mGMP = aGMP;
  mHost = aHost;
  mCachedPluginId = Some(mGMP->GetPluginId());
  mInitPluginEvent.Notify(*mCachedPluginId);
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t WebrtcGmpVideoEncoder::GmpInitDone_g(GMPVideoEncoderProxy* aGMP,
                                             GMPVideoHost* aHost,
                                             const GMPVideoCodec& aCodecParams,
                                             std::string* aErrorOut) {
  int32_t r = GmpInitDone_g(aGMP, aHost, aErrorOut);
  if (r != WEBRTC_VIDEO_CODEC_OK) {
    // We might have been destroyed if GmpInitDone failed.
    // Return immediately.
    return r;
  }
  mCodecParams = aCodecParams;
  return InitEncoderForSize(aCodecParams.mWidth, aCodecParams.mHeight,
                            aErrorOut);
}

void WebrtcGmpVideoEncoder::Close_g() {
  GMPVideoEncoderProxy* gmp(mGMP);
  mGMP = nullptr;
  mHost = nullptr;
  mInitting = false;

  if (mCachedPluginId) {
    mReleasePluginEvent.Notify(*mCachedPluginId);
  }
  mCachedPluginId = Nothing();

  if (gmp) {
    // Do this last, since this could cause us to be destroyed
    gmp->Close();
  }
}

int32_t WebrtcGmpVideoEncoder::InitEncoderForSize(unsigned short aWidth,
                                                  unsigned short aHeight,
                                                  std::string* aErrorOut) {
  mCodecParams.mWidth = aWidth;
  mCodecParams.mHeight = aHeight;
  // Pass dummy codecSpecific data for now...
  nsTArray<uint8_t> codecSpecific;

  GMPErr err =
      mGMP->InitEncode(mCodecParams, codecSpecific, this, 1, mMaxPayloadSize);
  if (err != GMPNoErr) {
    *aErrorOut = "GMP Encode: InitEncode failed";
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t WebrtcGmpVideoEncoder::Encode(
    const webrtc::VideoFrame& aInputImage,
    const std::vector<webrtc::VideoFrameType>* aFrameTypes) {
  mEncodeQueue->AssertOnCurrentThread();
  MOZ_ASSERT(aInputImage.width() >= 0 && aInputImage.height() >= 0);
  if (!aFrameTypes) {
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  if (mConfiguredBitrateKbps == 0) {
    GMP_LOG_VERBOSE("GMP Encode: not enabled");
    MutexAutoLock lock(mCallbackMutex);
    if (mCallback) {
      mCallback->OnDroppedFrame(
          webrtc::EncodedImageCallback::DropReason::kDroppedByEncoder);
    }
    return WEBRTC_VIDEO_CODEC_OK;
  }

  // It is safe to copy aInputImage here because the frame buffer is held by
  // a refptr.
  MOZ_ALWAYS_SUCCEEDS(mGMPThread->Dispatch(
      NewRunnableMethod<webrtc::VideoFrame,
                        std::vector<webrtc::VideoFrameType>>(
          __func__, this, &WebrtcGmpVideoEncoder::Encode_g, aInputImage,
          *aFrameTypes)));

  return WEBRTC_VIDEO_CODEC_OK;
}

void WebrtcGmpVideoEncoder::RegetEncoderForResolutionChange(uint32_t aWidth,
                                                            uint32_t aHeight) {
  Close_g();

  UniquePtr<GetGMPVideoEncoderCallback> callback(
      new InitDoneForResolutionChangeCallback(this, aWidth, aHeight));

  // OpenH264 codec (at least) can't handle dynamic input resolution changes
  // re-init the plugin when the resolution changes
  // XXX allow codec to indicate it doesn't need re-init!
  nsTArray<nsCString> tags;
  tags.AppendElement("h264"_ns);
  mInitting = true;
  if (NS_WARN_IF(NS_FAILED(mMPS->GetGMPVideoEncoder(nullptr, &tags, ""_ns,
                                                    std::move(callback))))) {
    NotifyGmpInitDone(mPCHandle, WEBRTC_VIDEO_CODEC_ERROR,
                      "GMP Encode: GetGMPVideoEncoder failed");
  }
}

void WebrtcGmpVideoEncoder::Encode_g(
    const webrtc::VideoFrame& aInputImage,
    std::vector<webrtc::VideoFrameType> aFrameTypes) {
  if (!mGMP) {
    // destroyed via Terminate(), failed to init, or just not initted yet
    GMP_LOG_DEBUG("GMP Encode: not initted yet");
    return;
  }
  MOZ_ASSERT(mHost);

  if (static_cast<uint32_t>(aInputImage.width()) != mCodecParams.mWidth ||
      static_cast<uint32_t>(aInputImage.height()) != mCodecParams.mHeight) {
    GMP_LOG_DEBUG("GMP Encode: resolution change from %ux%u to %dx%d",
                  mCodecParams.mWidth, mCodecParams.mHeight,
                  aInputImage.width(), aInputImage.height());

    mNeedKeyframe = true;
    RegetEncoderForResolutionChange(aInputImage.width(), aInputImage.height());
    if (!mGMP) {
      // We needed to go async to re-get the encoder. Bail.
      return;
    }
  }

  GMPVideoFrame* ftmp = nullptr;
  GMPErr err = mHost->CreateFrame(kGMPI420VideoFrame, &ftmp);
  if (err != GMPNoErr) {
    GMP_LOG_DEBUG("GMP Encode: failed to create frame on host");
    return;
  }
  GMPUniquePtr<GMPVideoi420Frame> frame(static_cast<GMPVideoi420Frame*>(ftmp));
  const webrtc::I420BufferInterface* input_image =
      aInputImage.video_frame_buffer()->GetI420();
  // check for overflow of stride * height
  CheckedInt32 ysize =
      CheckedInt32(input_image->StrideY()) * input_image->height();
  MOZ_RELEASE_ASSERT(ysize.isValid());
  // I will assume that if that doesn't overflow, the others case - YUV
  // 4:2:0 has U/V widths <= Y, even with alignment issues.
  err = frame->CreateFrame(
      ysize.value(), input_image->DataY(),
      input_image->StrideU() * ((input_image->height() + 1) / 2),
      input_image->DataU(),
      input_image->StrideV() * ((input_image->height() + 1) / 2),
      input_image->DataV(), input_image->width(), input_image->height(),
      input_image->StrideY(), input_image->StrideU(), input_image->StrideV());
  if (err != GMPNoErr) {
    GMP_LOG_DEBUG("GMP Encode: failed to create frame");
    return;
  }
  frame->SetTimestamp((aInputImage.rtp_timestamp() * 1000ll) /
                      90);  // note: rounds down!

  GMPCodecSpecificInfo info{};
  info.mCodecType = kGMPVideoCodecH264;
  nsTArray<uint8_t> codecSpecificInfo;
  codecSpecificInfo.AppendElements((uint8_t*)&info,
                                   sizeof(GMPCodecSpecificInfo));

  nsTArray<GMPVideoFrameType> gmp_frame_types;
  for (const auto& frameType : aFrameTypes) {
    GMPVideoFrameType ft;

    if (mNeedKeyframe) {
      ft = kGMPKeyFrame;
    } else {
      int32_t ret = WebrtcFrameTypeToGmpFrameType(frameType, &ft);
      if (ret != WEBRTC_VIDEO_CODEC_OK) {
        GMP_LOG_DEBUG(
            "GMP Encode: failed to map webrtc frame type to gmp frame type");
        return;
      }
    }

    gmp_frame_types.AppendElement(ft);
  }
  mNeedKeyframe = false;

  auto frameConfigs =
      mSvcController->NextFrameConfig(gmp_frame_types[0] == kGMPKeyFrame);
  MOZ_ASSERT(frameConfigs.size() == 1);

  MOZ_RELEASE_ASSERT(mInputImageMap.IsEmpty() ||
                     mInputImageMap.LastElement().rtp_timestamp <
                         frame->Timestamp());
  mInputImageMap.AppendElement(InputImageData{
      frame->Timestamp(), aInputImage.timestamp_us(), frameConfigs[0]});

  GMP_LOG_DEBUG("GMP Encode: %" PRIu64, (frame->Timestamp()));
  err = mGMP->Encode(std::move(frame), codecSpecificInfo, gmp_frame_types);
  if (err != GMPNoErr) {
    GMP_LOG_DEBUG("GMP Encode: failed to encode frame");
  }
}

int32_t WebrtcGmpVideoEncoder::RegisterEncodeCompleteCallback(
    webrtc::EncodedImageCallback* aCallback) {
  MutexAutoLock lock(mCallbackMutex);
  mCallback = aCallback;

  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t WebrtcGmpVideoEncoder::Shutdown() {
  GMP_LOG_DEBUG("GMP Released:");
  RegisterEncodeCompleteCallback(nullptr);

  if (mGMPThread) {
    MOZ_ALWAYS_SUCCEEDS(mGMPThread->Dispatch(
        NewRunnableMethod(__func__, this, &WebrtcGmpVideoEncoder::Close_g)));
  }
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t WebrtcGmpVideoEncoder::SetRates(
    const webrtc::VideoEncoder::RateControlParameters& aParameters) {
  mEncodeQueue->AssertOnCurrentThread();
  MOZ_ASSERT(mGMPThread);
  MOZ_ASSERT(!aParameters.bitrate.IsSpatialLayerUsed(1),
             "No simulcast support for H264");
  auto old = mConfiguredBitrateKbps;
  mConfiguredBitrateKbps = aParameters.bitrate.GetSpatialLayerSum(0) / 1000;
  MOZ_ALWAYS_SUCCEEDS(
      mGMPThread->Dispatch(NewRunnableMethod<uint32_t, uint32_t, Maybe<double>>(
          __func__, this, &WebrtcGmpVideoEncoder::SetRates_g, old,
          mConfiguredBitrateKbps,
          aParameters.framerate_fps > 0.0 ? Some(aParameters.framerate_fps)
                                          : Nothing())));

  return WEBRTC_VIDEO_CODEC_OK;
}

WebrtcVideoEncoder::EncoderInfo WebrtcGmpVideoEncoder::GetEncoderInfo() const {
  WebrtcVideoEncoder::EncoderInfo info;
  info.supports_native_handle = false;
  info.implementation_name = "GMPOpenH264";
  info.scaling_settings = WebrtcVideoEncoder::ScalingSettings(
      kLowH264QpThreshold, kHighH264QpThreshold);
  info.is_hardware_accelerated = false;
  info.supports_simulcast = false;
  return info;
}

int32_t WebrtcGmpVideoEncoder::SetRates_g(uint32_t aOldBitRateKbps,
                                          uint32_t aNewBitRateKbps,
                                          Maybe<double> aFrameRate) {
  if (!mGMP) {
    // destroyed via Terminate()
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  mNeedKeyframe |= (aOldBitRateKbps == 0 && aNewBitRateKbps != 0);

  GMPErr err = mGMP->SetRates(
      aNewBitRateKbps, aFrameRate
                           .map([](double aFr) {
                             // Avoid rounding to 0
                             return std::max(1U, static_cast<uint32_t>(aFr));
                           })
                           .valueOr(mCodecParams.mMaxFramerate));
  if (err != GMPNoErr) {
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  return WEBRTC_VIDEO_CODEC_OK;
}

// GMPVideoEncoderCallback virtual functions.
void WebrtcGmpVideoEncoder::Terminated() {
  GMP_LOG_DEBUG("GMP Encoder Terminated: %p", (void*)this);

  GMPVideoEncoderProxy* gmp(mGMP);
  mGMP = nullptr;
  mHost = nullptr;
  mInitting = false;

  if (gmp) {
    // Do this last, since this could cause us to be destroyed
    gmp->Close();
  }

  // Could now notify that it's dead
}

void WebrtcGmpVideoEncoder::Encoded(
    GMPVideoEncodedFrame* aEncodedFrame,
    const nsTArray<uint8_t>& aCodecSpecificInfo) {
  MOZ_ASSERT(mGMPThread->IsOnCurrentThread());
  Maybe<InputImageData> data;
  auto rtp_comparator = [](const InputImageData& aA,
                           const InputImageData& aB) -> int32_t {
    const auto& a = aA.rtp_timestamp;
    const auto& b = aB.rtp_timestamp;
    return a < b ? -1 : a != b;
  };
  size_t nextIdx = mInputImageMap.IndexOfFirstElementGt(
      InputImageData{aEncodedFrame->TimeStamp(), 0}, rtp_comparator);
  const size_t numToRemove = nextIdx;
  size_t numFramesDropped = numToRemove;
  MOZ_ASSERT(nextIdx != 0);
  if (nextIdx != 0 && mInputImageMap.ElementAt(nextIdx - 1).rtp_timestamp ==
                          aEncodedFrame->TimeStamp()) {
    --numFramesDropped;
    data = Some(mInputImageMap.ElementAt(nextIdx - 1));
  }
  mInputImageMap.RemoveElementsAt(0, numToRemove);

  webrtc::VideoFrameType frt;
  GmpFrameTypeToWebrtcFrameType(aEncodedFrame->FrameType(), &frt);
  MOZ_ASSERT_IF(mCodecParams.mTemporalLayerNum > 1 &&
                    aEncodedFrame->FrameType() == kGMPKeyFrame,
                aEncodedFrame->GetTemporalLayerId() == 0);
  if (aEncodedFrame->FrameType() == kGMPKeyFrame &&
      !data->frame_config.IsKeyframe()) {
    GMP_LOG_WARNING("GMP Encoded non-requested keyframe at t=%" PRIu64,
                    aEncodedFrame->TimeStamp());
    // If there could be multiple encode jobs in flight this would be racy.
    auto frameConfigs = mSvcController->NextFrameConfig(/* restart =*/true);
    MOZ_ASSERT(frameConfigs.size() == 1);
    data->frame_config = frameConfigs[0];
  }

  MOZ_ASSERT((aEncodedFrame->FrameType() == kGMPKeyFrame) ==
             data->frame_config.IsKeyframe());
  MOZ_ASSERT_IF(
      mCodecParams.mTemporalLayerNum > 1,
      aEncodedFrame->GetTemporalLayerId() == data->frame_config.TemporalId());

  MutexAutoLock lock(mCallbackMutex);
  if (!mCallback) {
    return;
  }

  for (size_t i = 0; i < numFramesDropped; ++i) {
    mCallback->OnDroppedFrame(
        webrtc::EncodedImageCallback::DropReason::kDroppedByEncoder);
  }

  if (data.isNothing()) {
    MOZ_ASSERT_UNREACHABLE(
        "Unexpectedly didn't find an input image for this encoded frame");
    return;
  }

  webrtc::VideoFrameType ft;
  GmpFrameTypeToWebrtcFrameType(aEncodedFrame->FrameType(), &ft);
  uint64_t timestamp = (aEncodedFrame->TimeStamp() * 90ll + 999) / 1000;

  GMP_LOG_DEBUG("GMP Encoded: %" PRIu64 ", type %d, len %d",
                aEncodedFrame->TimeStamp(), aEncodedFrame->BufferType(),
                aEncodedFrame->Size());

  // Libwebrtc's RtpPacketizerH264 expects a 3- or 4-byte NALU start sequence
  // before the start of the NALU payload. {0,0,1} or {0,0,0,1}. We set this
  // in-place. Any other length of the length field we reject.
  if (NS_WARN_IF(!AdjustOpenH264NALUSequence(aEncodedFrame))) {
    return;
  }

  webrtc::EncodedImage unit;
  unit.SetEncodedData(webrtc::EncodedImageBuffer::Create(
      aEncodedFrame->Buffer(), aEncodedFrame->Size()));
  unit._frameType = ft;
  unit.SetRtpTimestamp(timestamp);
  unit.capture_time_ms_ = webrtc::Timestamp::Micros(data->timestamp_us).ms();
  unit._encodedWidth = aEncodedFrame->EncodedWidth();
  unit._encodedHeight = aEncodedFrame->EncodedHeight();

  webrtc::CodecSpecificInfo info;
#ifdef __LP64__
  // Only do these checks on some common builds to avoid build issues on more
  // exotic flavors.
  static_assert(
      sizeof(info.codecSpecific.H264) == 8,
      "webrtc::CodecSpecificInfoH264 has changed. We must handle the changes.");
  static_assert(
      sizeof(info) - sizeof(info.codecSpecific) -
              sizeof(info.generic_frame_info) -
              sizeof(info.template_structure) -
              sizeof(info.frame_instrumentation_data) ==
          24,
      "webrtc::CodecSpecificInfo's generic bits have changed. We must handle "
      "the changes.");
#endif
  info.codecType = webrtc::kVideoCodecH264;
  info.codecSpecific = {};
  info.codecSpecific.H264.packetization_mode =
      mFormatParams.count(cricket::kH264FmtpPacketizationMode) == 1 &&
              mFormatParams.at(cricket::kH264FmtpPacketizationMode) == "1"
          ? webrtc::H264PacketizationMode::NonInterleaved
          : webrtc::H264PacketizationMode::SingleNalUnit;
  info.codecSpecific.H264.temporal_idx = webrtc::kNoTemporalIdx;
  info.codecSpecific.H264.base_layer_sync = false;
  info.codecSpecific.H264.idr_frame =
      ft == webrtc::VideoFrameType::kVideoFrameKey;
  info.generic_frame_info = mSvcController->OnEncodeDone(data->frame_config);
  if (info.codecSpecific.H264.idr_frame &&
      info.generic_frame_info.has_value()) {
    info.template_structure = mSvcController->DependencyStructure();
  }

  if (mCodecParams.mTemporalLayerNum > 1) {
    int temporalIdx = std::max(0, aEncodedFrame->GetTemporalLayerId());
    unit.SetTemporalIndex(temporalIdx);
    info.codecSpecific.H264.temporal_idx = temporalIdx;
    info.scalability_mode = GmpCodecParamsToScalabilityMode(mCodecParams);

    if (temporalIdx == 0) {
      // Base layer. Reset the sync layer tracking.
      mSyncLayerCap = mCodecParams.mTemporalLayerNum;
    } else {
      // Decrease the sync layer tracking. base_layer_sync per upstream code
      // shall be true iff the layer in question only depends on layer 0, i.e.
      // the base layer. Note in L1T3 the frame dependencies (and cap) are:
      //       | Temporal | Dependency |       |
      // Frame | Layer    | Frame      | Sync? |  Cap
      // ===============================================
      //     0 |        0 |          0 | False | _ -> 3
      //     1 |        2 |          0 | True  | 3 -> 2
      //     2 |        1 |          0 | True  | 2 -> 1
      //     3 |        2 |          1 | False | 1 -> 2
      info.codecSpecific.H264.base_layer_sync = temporalIdx < mSyncLayerCap;
      mSyncLayerCap = temporalIdx;
    }
  }

  // Parse QP.
  mH264BitstreamParser.ParseBitstream(unit);
  unit.qp_ = mH264BitstreamParser.GetLastSliceQp().value_or(-1);

  mCallback->OnEncodedImage(unit, &info);
}

// Decoder.
WebrtcGmpVideoDecoder::WebrtcGmpVideoDecoder(std::string aPCHandle,
                                             TrackingId aTrackingId)
    : mGMP(nullptr),
      mInitting(false),
      mHost(nullptr),
      mCallbackMutex("WebrtcGmpVideoDecoder decoded callback mutex"),
      mCallback(nullptr),
      mDecoderStatus(GMPNoErr),
      mPCHandle(std::move(aPCHandle)),
      mTrackingId(std::move(aTrackingId)) {
  MOZ_ASSERT(!mPCHandle.empty());
}

WebrtcGmpVideoDecoder::~WebrtcGmpVideoDecoder() {
  // We should not have been destroyed if we never closed our GMP
  MOZ_ASSERT(!mGMP);
}

bool WebrtcGmpVideoDecoder::Configure(
    const webrtc::VideoDecoder::Settings& settings) {
  if (!mMPS) {
    mMPS = do_GetService("@mozilla.org/gecko-media-plugin-service;1");
  }
  MOZ_ASSERT(mMPS);

  if (!mGMPThread) {
    if (NS_WARN_IF(NS_FAILED(mMPS->GetThread(getter_AddRefs(mGMPThread))))) {
      return false;
    }
  }

  MOZ_ALWAYS_SUCCEEDS(
      mGMPThread->Dispatch(NewRunnableMethod<webrtc::VideoDecoder::Settings>(
          __func__, this, &WebrtcGmpVideoDecoder::Configure_g, settings)));

  return true;
}

void WebrtcGmpVideoDecoder::Configure_g(
    const webrtc::VideoDecoder::Settings& settings) {
  nsTArray<nsCString> tags;
  tags.AppendElement("h264"_ns);
  UniquePtr<GetGMPVideoDecoderCallback> callback(new InitDoneCallback(this));
  mInitting = true;
  nsresult rv =
      mMPS->GetGMPVideoDecoder(nullptr, &tags, ""_ns, std::move(callback));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    GMP_LOG_DEBUG("GMP Decode: GetGMPVideoDecoder failed");
    Close_g();
    NotifyGmpInitDone(mPCHandle, WEBRTC_VIDEO_CODEC_ERROR,
                      "GMP Decode: GetGMPVideoDecoder failed.");
  }
}

int32_t WebrtcGmpVideoDecoder::GmpInitDone_g(GMPVideoDecoderProxy* aGMP,
                                             GMPVideoHost* aHost,
                                             std::string* aErrorOut) {
  if (!mInitting || !aGMP || !aHost) {
    *aErrorOut =
        "GMP Decode: Either init was aborted, "
        "or init failed to supply either a GMP decoder or GMP host.";
    if (aGMP) {
      // This could destroy us, since aGMP may be the last thing holding a ref
      // Return immediately.
      aGMP->Close();
    }
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  mInitting = false;

  if (mGMP && mGMP != aGMP) {
    Close_g();
  }

  mGMP = aGMP;
  mHost = aHost;
  mCachedPluginId = Some(mGMP->GetPluginId());
  mInitPluginEvent.Notify(*mCachedPluginId);

  GMPVideoCodec codec{};
  codec.mGMPApiVersion = kGMPVersion34;
  codec.mLogLevel = GetGMPLibraryLogLevel();

  // XXX this is currently a hack
  // GMPVideoCodecUnion codecSpecific;
  // memset(&codecSpecific, 0, sizeof(codecSpecific));
  nsTArray<uint8_t> codecSpecific;
  nsresult rv = mGMP->InitDecode(codec, codecSpecific, this, 1);
  if (NS_FAILED(rv)) {
    *aErrorOut = "GMP Decode: InitDecode failed";
    mQueuedFrames.Clear();
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  // now release any frames that got queued waiting for InitDone
  if (!mQueuedFrames.IsEmpty()) {
    // So we're safe to call Decode_g(), which asserts it's empty
    nsTArray<UniquePtr<GMPDecodeData>> temp = std::move(mQueuedFrames);
    for (auto& queued : temp) {
      Decode_g(std::move(queued));
    }
  }

  // This is an ugly solution to asynchronous decoding errors
  // from Decode_g() not being returned to the synchronous Decode() method.
  // If we don't return an error code at this point, our caller ultimately won't
  // know to request a PLI and the video stream will remain frozen unless an IDR
  // happens to arrive for other reasons. Bug 1492852 tracks implementing a
  // proper solution.
  if (mDecoderStatus != GMPNoErr) {
    GMP_LOG_ERROR("%s: Decoder status is bad (%u)!", __PRETTY_FUNCTION__,
                  static_cast<unsigned>(mDecoderStatus));
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  return WEBRTC_VIDEO_CODEC_OK;
}

void WebrtcGmpVideoDecoder::Close_g() {
  GMPVideoDecoderProxy* gmp(mGMP);
  mGMP = nullptr;
  mHost = nullptr;
  mInitting = false;

  if (mCachedPluginId) {
    mReleasePluginEvent.Notify(*mCachedPluginId);
  }
  mCachedPluginId = Nothing();

  if (gmp) {
    // Do this last, since this could cause us to be destroyed
    gmp->Close();
  }
}

int32_t WebrtcGmpVideoDecoder::Decode(const webrtc::EncodedImage& aInputImage,
                                      bool aMissingFrames,
                                      int64_t aRenderTimeMs) {
  MOZ_ASSERT(mGMPThread);
  MOZ_ASSERT(!NS_IsMainThread());
  if (!aInputImage.size()) {
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  MediaInfoFlag flag = MediaInfoFlag::None;
  flag |= (aInputImage._frameType == webrtc::VideoFrameType::kVideoFrameKey
               ? MediaInfoFlag::KeyFrame
               : MediaInfoFlag::NonKeyFrame);
  flag |= MediaInfoFlag::SoftwareDecoding;
  flag |= MediaInfoFlag::VIDEO_H264;
  mPerformanceRecorder.Start((aInputImage.RtpTimestamp() * 1000ll) / 90,
                             "WebrtcGmpVideoDecoder"_ns, mTrackingId, flag);

  // This is an ugly solution to asynchronous decoding errors
  // from Decode_g() not being returned to the synchronous Decode() method.
  // If we don't return an error code at this point, our caller ultimately won't
  // know to request a PLI and the video stream will remain frozen unless an IDR
  // happens to arrive for other reasons. Bug 1492852 tracks implementing a
  // proper solution.
  auto decodeData =
      MakeUnique<GMPDecodeData>(aInputImage, aMissingFrames, aRenderTimeMs);

  MOZ_ALWAYS_SUCCEEDS(
      mGMPThread->Dispatch(NewRunnableMethod<UniquePtr<GMPDecodeData>&&>(
          __func__, this, &WebrtcGmpVideoDecoder::Decode_g,
          std::move(decodeData))));

  if (mDecoderStatus != GMPNoErr) {
    GMP_LOG_ERROR("%s: Decoder status is bad (%u)!", __PRETTY_FUNCTION__,
                  static_cast<unsigned>(mDecoderStatus));
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  return WEBRTC_VIDEO_CODEC_OK;
}

void WebrtcGmpVideoDecoder::Decode_g(UniquePtr<GMPDecodeData>&& aDecodeData) {
  if (!mGMP) {
    if (mInitting) {
      // InitDone hasn't been called yet (race)
      mQueuedFrames.AppendElement(std::move(aDecodeData));
      return;
    }
    // destroyed via Terminate(), failed to init, or just not initted yet
    GMP_LOG_DEBUG("GMP Decode: not initted yet");

    mDecoderStatus = GMPDecodeErr;
    return;
  }

  MOZ_ASSERT(mQueuedFrames.IsEmpty());
  MOZ_ASSERT(mHost);

  GMPVideoFrame* ftmp = nullptr;
  GMPErr err = mHost->CreateFrame(kGMPEncodedVideoFrame, &ftmp);
  if (err != GMPNoErr) {
    GMP_LOG_ERROR("%s: CreateFrame failed (%u)!", __PRETTY_FUNCTION__,
                  static_cast<unsigned>(err));
    mDecoderStatus = err;
    return;
  }

  GMPUniquePtr<GMPVideoEncodedFrame> frame(
      static_cast<GMPVideoEncodedFrame*>(ftmp));
  err = frame->CreateEmptyFrame(aDecodeData->mImage.size());
  if (err != GMPNoErr) {
    GMP_LOG_ERROR("%s: CreateEmptyFrame failed (%u)!", __PRETTY_FUNCTION__,
                  static_cast<unsigned>(err));
    mDecoderStatus = err;
    return;
  }

  // XXX At this point, we only will get mode1 data (a single length and a
  // buffer) Session_info.cc/etc code needs to change to support mode 0.
  *(reinterpret_cast<uint32_t*>(frame->Buffer())) = frame->Size();

  // XXX It'd be wonderful not to have to memcpy the encoded data!
  memcpy(frame->Buffer() + 4, aDecodeData->mImage.data() + 4,
         frame->Size() - 4);

  frame->SetEncodedWidth(aDecodeData->mImage._encodedWidth);
  frame->SetEncodedHeight(aDecodeData->mImage._encodedHeight);
  frame->SetTimeStamp((aDecodeData->mImage.RtpTimestamp() * 1000ll) /
                      90);  // rounds down
  frame->SetCompleteFrame(
      true);  // upstream no longer deals with incomplete frames
  frame->SetBufferType(GMP_BufferLength32);

  GMPVideoFrameType ft;
  int32_t ret =
      WebrtcFrameTypeToGmpFrameType(aDecodeData->mImage._frameType, &ft);
  if (ret != WEBRTC_VIDEO_CODEC_OK) {
    GMP_LOG_ERROR("%s: WebrtcFrameTypeToGmpFrameType failed (%u)!",
                  __PRETTY_FUNCTION__, static_cast<unsigned>(ret));
    mDecoderStatus = GMPDecodeErr;
    return;
  }

  GMPCodecSpecificInfo info{};
  info.mCodecType = kGMPVideoCodecH264;
  info.mCodecSpecific.mH264.mSimulcastIdx = 0;
  nsTArray<uint8_t> codecSpecificInfo;
  codecSpecificInfo.AppendElements((uint8_t*)&info,
                                   sizeof(GMPCodecSpecificInfo));

  GMP_LOG_DEBUG("GMP Decode: %" PRIu64 ", len %zu%s", frame->TimeStamp(),
                aDecodeData->mImage.size(),
                ft == kGMPKeyFrame ? ", KeyFrame" : "");

  nsresult rv = mGMP->Decode(std::move(frame), aDecodeData->mMissingFrames,
                             codecSpecificInfo, aDecodeData->mRenderTimeMs);
  if (NS_FAILED(rv)) {
    GMP_LOG_ERROR("%s: Decode failed (rv=%u)!", __PRETTY_FUNCTION__,
                  static_cast<unsigned>(rv));
    mDecoderStatus = GMPDecodeErr;
    return;
  }

  mDecoderStatus = GMPNoErr;
}

int32_t WebrtcGmpVideoDecoder::RegisterDecodeCompleteCallback(
    webrtc::DecodedImageCallback* aCallback) {
  MutexAutoLock lock(mCallbackMutex);
  mCallback = aCallback;

  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t WebrtcGmpVideoDecoder::ReleaseGmp() {
  GMP_LOG_DEBUG("GMP Released:");
  RegisterDecodeCompleteCallback(nullptr);

  if (mGMPThread) {
    MOZ_ALWAYS_SUCCEEDS(mGMPThread->Dispatch(
        NewRunnableMethod(__func__, this, &WebrtcGmpVideoDecoder::Close_g)));
  }
  return WEBRTC_VIDEO_CODEC_OK;
}

void WebrtcGmpVideoDecoder::Terminated() {
  GMP_LOG_DEBUG("GMP Decoder Terminated: %p", (void*)this);

  GMPVideoDecoderProxy* gmp(mGMP);
  mGMP = nullptr;
  mHost = nullptr;
  mInitting = false;

  if (gmp) {
    // Do this last, since this could cause us to be destroyed
    gmp->Close();
  }

  // Could now notify that it's dead
}

void WebrtcGmpVideoDecoder::Decoded(GMPVideoi420Frame* aDecodedFrame) {
  // we have two choices here: wrap the frame with a callback that frees
  // the data later (risking running out of shmems), or copy the data out
  // always.  Also, we can only Destroy() the frame on the gmp thread, so
  // copying is simplest if expensive.
  // I420 size including rounding...
  CheckedInt32 length =
      (CheckedInt32(aDecodedFrame->Stride(kGMPYPlane)) *
       aDecodedFrame->Height()) +
      (aDecodedFrame->Stride(kGMPVPlane) + aDecodedFrame->Stride(kGMPUPlane)) *
          ((aDecodedFrame->Height() + 1) / 2);
  int32_t size = length.value();
  MOZ_RELEASE_ASSERT(length.isValid() && size > 0);

  // Don't use MakeUniqueFallible here, because UniquePtr isn't copyable, and
  // the closure below in WrapI420Buffer uses std::function which _is_ copyable.
  // We'll alloc the buffer here, so we preserve the "fallible" nature, and
  // then hand a shared_ptr, which is copyable, to WrapI420Buffer.
  auto* falliblebuffer = new (std::nothrow) uint8_t[size];
  if (falliblebuffer) {
    auto buffer = std::shared_ptr<uint8_t>(falliblebuffer);

    // This is 3 separate buffers currently anyways, no use in trying to
    // see if we can use a single memcpy.
    uint8_t* buffer_y = buffer.get();
    memcpy(buffer_y, aDecodedFrame->Buffer(kGMPYPlane),
           aDecodedFrame->Stride(kGMPYPlane) * aDecodedFrame->Height());
    // Should this be aligned, making it non-contiguous?  Assume no, this is
    // already factored into the strides.
    uint8_t* buffer_u =
        buffer_y + aDecodedFrame->Stride(kGMPYPlane) * aDecodedFrame->Height();
    memcpy(buffer_u, aDecodedFrame->Buffer(kGMPUPlane),
           aDecodedFrame->Stride(kGMPUPlane) *
               ((aDecodedFrame->Height() + 1) / 2));
    uint8_t* buffer_v = buffer_u + aDecodedFrame->Stride(kGMPUPlane) *
                                       ((aDecodedFrame->Height() + 1) / 2);
    memcpy(buffer_v, aDecodedFrame->Buffer(kGMPVPlane),
           aDecodedFrame->Stride(kGMPVPlane) *
               ((aDecodedFrame->Height() + 1) / 2));

    MutexAutoLock lock(mCallbackMutex);
    if (mCallback) {
      // Note: the last parameter to WrapI420Buffer is named no_longer_used,
      // but is currently called in the destructor of WrappedYuvBuffer when
      // the buffer is "no_longer_used".
      rtc::scoped_refptr<webrtc::I420BufferInterface> video_frame_buffer =
          webrtc::WrapI420Buffer(
              aDecodedFrame->Width(), aDecodedFrame->Height(), buffer_y,
              aDecodedFrame->Stride(kGMPYPlane), buffer_u,
              aDecodedFrame->Stride(kGMPUPlane), buffer_v,
              aDecodedFrame->Stride(kGMPVPlane), [buffer] {});

      GMP_LOG_DEBUG("GMP Decoded: %" PRIu64, aDecodedFrame->Timestamp());
      auto videoFrame =
          webrtc::VideoFrame::Builder()
              .set_video_frame_buffer(video_frame_buffer)
              .set_timestamp_rtp(
                  // round up
                  (aDecodedFrame->UpdatedTimestamp() * 90ll + 999) / 1000)
              .build();
      mPerformanceRecorder.Record(
          static_cast<int64_t>(aDecodedFrame->Timestamp()),
          [&](DecodeStage& aStage) {
            aStage.SetImageFormat(DecodeStage::YUV420P);
            aStage.SetResolution(aDecodedFrame->Width(),
                                 aDecodedFrame->Height());
            aStage.SetColorDepth(gfx::ColorDepth::COLOR_8);
          });
      mCallback->Decoded(videoFrame);
    }
  }
  aDecodedFrame->Destroy();
}

}  // namespace mozilla
