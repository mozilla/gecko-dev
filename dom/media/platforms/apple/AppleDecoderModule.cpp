/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AppleDecoderModule.h"

#include <dlfcn.h>

#include "AppleATDecoder.h"
#include "AppleVTDecoder.h"
#include "MP4Decoder.h"
#include "VideoUtils.h"
#include "VPXDecoder.h"
#include "AOMDecoder.h"
#include "mozilla/Logging.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/StaticPrefs_media.h"
#include "mozilla/gfx/gfxVars.h"

extern "C" {
// Only exists from MacOS 11
extern void VTRegisterSupplementalVideoDecoderIfAvailable(
    CMVideoCodecType codecType) __attribute__((weak_import));
extern Boolean VTIsHardwareDecodeSupported(CMVideoCodecType codecType)
    __attribute__((weak_import));
}

namespace mozilla {

using media::DecodeSupport;
using media::DecodeSupportSet;
using media::MCSInfo;
using media::MediaCodec;

static inline CMVideoCodecType GetCMVideoCodecType(const MediaCodec& aCodec) {
  switch (aCodec) {
    case MediaCodec::H264:
      return kCMVideoCodecType_H264;
    case MediaCodec::AV1:
      return kCMVideoCodecType_AV1;
    case MediaCodec::VP9:
      return kCMVideoCodecType_VP9;
    default:
      return static_cast<CMVideoCodecType>(0);
  }
}

/* static */
void AppleDecoderModule::Init() {
  if (sInitialized) {
    return;
  }

  // Initialize all values to false first.
  for (auto& support : sCanUseHWDecoder) {
    support = false;
  }

  // H264 HW is supported since 10.6.
  sCanUseHWDecoder[MediaCodec::H264] = CanCreateHWDecoder(MediaCodec::H264);
  // VP9 HW is supported since 11.0 on Apple silicon.
  sCanUseHWDecoder[MediaCodec::VP9] =
      RegisterSupplementalDecoder(MediaCodec::VP9) &&
      CanCreateHWDecoder(MediaCodec::VP9);
  // AV1 HW is supported since 14.0 on Apple silicon.
  sCanUseHWDecoder[MediaCodec::AV1] =
      RegisterSupplementalDecoder(MediaCodec::AV1) &&
      CanCreateHWDecoder(MediaCodec::AV1);

  sInitialized = true;
}

nsresult AppleDecoderModule::Startup() {
  if (!sInitialized) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

already_AddRefed<MediaDataDecoder> AppleDecoderModule::CreateVideoDecoder(
    const CreateDecoderParams& aParams) {
  if (Supports(SupportDecoderParams(aParams), nullptr /* diagnostics */)
          .isEmpty()) {
    return nullptr;
  }
  RefPtr<MediaDataDecoder> decoder;
  if (IsVideoSupported(aParams.VideoConfig(), aParams.mOptions)) {
    decoder = new AppleVTDecoder(aParams.VideoConfig(), aParams.mImageContainer,
                                 aParams.mOptions, aParams.mKnowsCompositor,
                                 aParams.mTrackingId);
  }
  return decoder.forget();
}

already_AddRefed<MediaDataDecoder> AppleDecoderModule::CreateAudioDecoder(
    const CreateDecoderParams& aParams) {
  if (Supports(SupportDecoderParams(aParams), nullptr /* diagnostics */)
          .isEmpty()) {
    return nullptr;
  }
  RefPtr<MediaDataDecoder> decoder = new AppleATDecoder(aParams.AudioConfig());
  return decoder.forget();
}

DecodeSupportSet AppleDecoderModule::SupportsMimeType(
    const nsACString& aMimeType, DecoderDoctorDiagnostics* aDiagnostics) const {
  bool checkSupport = aMimeType.EqualsLiteral("audio/mp4a-latm") ||
                      MP4Decoder::IsH264(aMimeType) ||
                      VPXDecoder::IsVP9(aMimeType) ||
                      AOMDecoder::IsAV1(aMimeType);
  DecodeSupportSet supportType{};

  if (checkSupport) {
    UniquePtr<TrackInfo> trackInfo = CreateTrackInfoWithMIMEType(aMimeType);
    if (trackInfo && trackInfo->IsAudio()) {
      supportType = DecodeSupport::SoftwareDecode;
    } else if (trackInfo && trackInfo->IsVideo()) {
      supportType = Supports(SupportDecoderParams(*trackInfo), aDiagnostics);
    }
  }

  MOZ_LOG(sPDMLog, LogLevel::Debug,
          ("Apple decoder %s requested type '%s'",
           supportType.isEmpty() ? "rejects" : "supports",
           aMimeType.BeginReading()));
  return supportType;
}

DecodeSupportSet AppleDecoderModule::Supports(
    const SupportDecoderParams& aParams,
    DecoderDoctorDiagnostics* aDiagnostics) const {
  const auto& trackInfo = aParams.mConfig;
  if (trackInfo.IsAudio()) {
    return SupportsMimeType(trackInfo.mMimeType, aDiagnostics);
  }
  const bool checkSupport = trackInfo.GetAsVideoInfo() &&
                            IsVideoSupported(*trackInfo.GetAsVideoInfo());
  DecodeSupportSet dss{};
  if (!checkSupport) {
    return dss;
  }
  const MediaCodec codec =
      MCSInfo::GetMediaCodecFromMimeType(trackInfo.mMimeType);
  if (sCanUseHWDecoder[codec]) {
    dss += DecodeSupport::HardwareDecode;
  }
  switch (codec) {
    case MediaCodec::VP8:
      [[fallthrough]];
    case MediaCodec::VP9:
      if (StaticPrefs::media_rdd_vpx_enabled() &&
          StaticPrefs::media_utility_ffvpx_enabled()) {
        dss += DecodeSupport::SoftwareDecode;
      }
      break;
    default:
      dss += DecodeSupport::SoftwareDecode;
      break;
  }
  return dss;
}

bool AppleDecoderModule::IsVideoSupported(
    const VideoInfo& aConfig,
    const CreateDecoderParams::OptionSet& aOptions) const {
  if (MP4Decoder::IsH264(aConfig.mMimeType)) {
    return true;
  }
  if (AOMDecoder::IsAV1(aConfig.mMimeType)) {
    if (!sCanUseHWDecoder[MediaCodec::AV1] ||
        aOptions.contains(
            CreateDecoderParams::Option::HardwareDecoderNotAllowed)) {
      return false;
    }

    // HW AV1 decoder only supports 8 or 10 bit color.
    if (aConfig.mColorDepth != gfx::ColorDepth::COLOR_8 &&
        aConfig.mColorDepth != gfx::ColorDepth::COLOR_10) {
      return false;
    }

    if (aConfig.mColorSpace.isSome()) {
      if (*aConfig.mColorSpace == gfx::YUVColorSpace::Identity) {
        // HW AV1 decoder doesn't support RGB
        return false;
      }
    }

    if (aConfig.mExtraData && aConfig.mExtraData->Length() < 2) {
      return true;  // Assume it's okay.
    }
    // top 3 bits are the profile.
    int profile = aConfig.mExtraData->ElementAt(1) >> 5;
    // 0 is main profile
    return profile == 0;
  }

  if (!VPXDecoder::IsVP9(aConfig.mMimeType) ||
      !sCanUseHWDecoder[MediaCodec::VP9] ||
      aOptions.contains(
          CreateDecoderParams::Option::HardwareDecoderNotAllowed)) {
    return false;
  }
  if (VPXDecoder::IsVP9(aConfig.mMimeType) &&
      aOptions.contains(CreateDecoderParams::Option::LowLatency)) {
    // SVC layers are unsupported, and may be used in low latency use cases
    // (WebRTC).
    return false;
  }
  if (aConfig.HasAlpha()) {
    return false;
  }

  // HW VP9 decoder only supports 8 or 10 bit color.
  if (aConfig.mColorDepth != gfx::ColorDepth::COLOR_8 &&
      aConfig.mColorDepth != gfx::ColorDepth::COLOR_10) {
    return false;
  }

  // See if we have a vpcC box, and check further constraints.
  // HW VP9 Decoder supports Profile 0 & 2 (YUV420)
  if (aConfig.mExtraData && aConfig.mExtraData->Length() < 5) {
    return true;  // Assume it's okay.
  }
  int profile = aConfig.mExtraData->ElementAt(4);

  return profile == 0 || profile == 2;
}

/* static */
bool AppleDecoderModule::CanCreateHWDecoder(const MediaCodec& aCodec) {
  // Check whether HW decode should even be enabled
  if (!gfx::gfxVars::CanUseHardwareVideoDecoding()) {
    return false;
  }

  if (!VTIsHardwareDecodeSupported) {
    return false;
  }

  if (!VTIsHardwareDecodeSupported(GetCMVideoCodecType(aCodec))) {
    return false;
  }

  // H264 hardware decoding has been supported since macOS 10.6 on most Intel
  // GPUs (Sandy Bridge and later, 2011). If VTIsHardwareDecodeSupported is
  // already true, there's no need for further verification.
  if (aCodec == MediaCodec::H264) {
    return true;
  }

  // Build up a fake extradata to create an actual decoder to verify
  VideoInfo info(1920, 1080);
  if (aCodec == MediaCodec::AV1) {
    info.mMimeType = "video/av1";
    bool hasSeqHdr;
    AOMDecoder::AV1SequenceInfo seqInfo;
    AOMDecoder::OperatingPoint op;
    seqInfo.mOperatingPoints.AppendElement(op);
    seqInfo.mImage = {1920, 1080};
    AOMDecoder::WriteAV1CBox(seqInfo, info.mExtraData, hasSeqHdr);
  } else if (aCodec == MediaCodec::VP9) {
    info.mMimeType = "video/vp9";
    VPXDecoder::GetVPCCBox(info.mExtraData, VPXDecoder::VPXStreamInfo());
  }

  RefPtr<AppleVTDecoder> decoder =
      new AppleVTDecoder(info, nullptr, {}, nullptr, Nothing());
  auto release = MakeScopeExit([&]() { decoder->Shutdown(); });
  if (NS_FAILED(decoder->InitializeSession())) {
    MOZ_LOG(sPDMLog, LogLevel::Debug,
            ("Failed to initializing VT HW decoder session"));
    return false;
  }
  nsAutoCString failureReason;
  bool hwSupport = decoder->IsHardwareAccelerated(failureReason);
  if (!hwSupport) {
    MOZ_LOG(
        sPDMLog, LogLevel::Debug,
        ("VT decoder failed to use HW : '%s'", failureReason.BeginReading()));
  }
  return hwSupport;
}

/* static */
bool AppleDecoderModule::RegisterSupplementalDecoder(const MediaCodec& aCodec) {
#ifdef XP_MACOSX
  static bool sRegisterIfAvailable = [&]() {
    if (__builtin_available(macos 11.0, *)) {
      VTRegisterSupplementalVideoDecoderIfAvailable(
          GetCMVideoCodecType(aCodec));
      return true;
    }
    return false;
  }();
  return sRegisterIfAvailable;
#else  // iOS
  return false;
#endif
}

/* static */
already_AddRefed<PlatformDecoderModule> AppleDecoderModule::Create() {
  return MakeAndAddRef<AppleDecoderModule>();
}

}  // namespace mozilla
