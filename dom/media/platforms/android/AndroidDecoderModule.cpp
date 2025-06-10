//* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <jni.h>

#ifdef MOZ_AV1
#  include "AOMDecoder.h"
#endif
#include "MediaInfo.h"
#include "RemoteDataDecoder.h"
#include "VPXDecoder.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Components.h"
#include "mozilla/StaticPrefs_media.h"
#include "mozilla/gfx/gfxVars.h"
#include "mozilla/java/HardwareCodecCapabilityUtilsWrappers.h"
#include "nsIGfxInfo.h"
#include "nsPromiseFlatString.h"
#include "prlog.h"

#undef LOG
#define LOG(arg, ...)                                     \
  MOZ_LOG(                                                \
      sAndroidDecoderModuleLog, mozilla::LogLevel::Debug, \
      ("AndroidDecoderModule(%p)::%s: " arg, this, __func__, ##__VA_ARGS__))
#define SLOG(arg, ...)                                        \
  MOZ_LOG(sAndroidDecoderModuleLog, mozilla::LogLevel::Debug, \
          ("%s: " arg, __func__, ##__VA_ARGS__))

using namespace mozilla;
using media::DecodeSupport;
using media::DecodeSupportSet;
using media::MCSInfo;
using media::MediaCodec;
using media::MediaCodecsSupport;
using media::MediaCodecsSupported;
using media::TimeUnit;

namespace mozilla {

mozilla::LazyLogModule sAndroidDecoderModuleLog("AndroidDecoderModule");

nsCString TranslateMimeType(const nsACString& aMimeType) {
  if (VPXDecoder::IsVPX(aMimeType, VPXDecoder::VP8)) {
    static constexpr auto vp8 = "video/x-vnd.on2.vp8"_ns;
    return vp8;
  }
  if (VPXDecoder::IsVPX(aMimeType, VPXDecoder::VP9)) {
    static constexpr auto vp9 = "video/x-vnd.on2.vp9"_ns;
    return vp9;
  }
  if (aMimeType.EqualsLiteral("video/av1")) {
    static constexpr auto av1 = "video/av01"_ns;
    return av1;
  }
  return nsCString(aMimeType);
}

AndroidDecoderModule::AndroidDecoderModule(CDMProxy* aProxy) {
  mProxy = static_cast<MediaDrmCDMProxy*>(aProxy);
}

/* static */ bool AndroidDecoderModule::AreSupportedMimeTypesReady() {
  StaticMutexAutoLock lock(sMutex);
  return sSupportedSwMimeTypes && sSupportedHwMimeTypes;
}

/* static */ bool AndroidDecoderModule::IsSupportedCodecsReady() {
  StaticMutexAutoLock lock(sMutex);
  return sSupportedCodecs;
}

/* static */
media::MediaCodecsSupported AndroidDecoderModule::GetSupportedCodecs() {
  if (!AreSupportedMimeTypesReady() || !IsSupportedCodecsReady()) {
    SetSupportedMimeTypes();
  }
  StaticMutexAutoLock lock(sMutex);
  return *sSupportedCodecs;
}

DecodeSupportSet AndroidDecoderModule::SupportsMimeType(
    const nsACString& aMimeType) {
  if (!AreSupportedMimeTypesReady()) {
    SetSupportedMimeTypes();
  }

  // Handle per-codec logic if the codec type can be determined from
  // the MIME type string. GetMediaCodecFromMimeType should handle every
  // type string that was hardcoded in this function previously.
  MediaCodec codec = MCSInfo::GetMediaCodecFromMimeType(aMimeType);
  switch (codec) {
    case MediaCodec::VP8:
      if (!gfx::gfxVars::UseVP8HwDecode()) {
        return media::DecodeSupportSet{};
      }
      break;

    case MediaCodec::VP9:
      if (!gfx::gfxVars::UseVP9HwDecode()) {
        return media::DecodeSupportSet{};
      }
      break;

    // Prefer the gecko decoder for opus/vorbis; stagefright crashes
    // on content demuxed from mp4.
    // Not all android devices support FLAC even when they say they do.
    // Always use our own software decoder (in ffvpx) for audio except for AAC
    case MediaCodec::MP3:
      [[fallthrough]];
    case MediaCodec::Opus:
      [[fallthrough]];
    case MediaCodec::Vorbis:
      [[fallthrough]];
    case MediaCodec::Wave:
      [[fallthrough]];
    case MediaCodec::FLAC:
      SLOG("Rejecting audio of type %s", aMimeType.Data());
      return media::DecodeSupportSet{};

    // H264 always reports software decode
    case MediaCodec::H264:
      return DecodeSupport::SoftwareDecode;

    case MediaCodec::HEVC:
      if (!StaticPrefs::media_hevc_enabled()) {
        SLOG("Rejecting HEVC as the preference is disabled");
        return media::DecodeSupportSet{};
      }
      break;

    // AV1 doesn't need any special handling.
    case MediaCodec::AV1:
      break;

    case MediaCodec::SENTINEL:
      [[fallthrough]];
    default:
      SLOG("Support check using default logic for %s", aMimeType.Data());
      break;
  }

  // If a codec has no special handling or can't be determined from the
  // MIME type string, check if the MIME type string itself is supported.
  {
    StaticMutexAutoLock lock(sMutex);
    if (sSupportedHwMimeTypes &&
        sSupportedHwMimeTypes->Contains(TranslateMimeType(aMimeType))) {
      return DecodeSupport::HardwareDecode;
    }
    if (sSupportedSwMimeTypes &&
        sSupportedSwMimeTypes->Contains(TranslateMimeType(aMimeType))) {
      return DecodeSupport::SoftwareDecode;
    }
  }
  return media::DecodeSupportSet{};
}

nsTArray<nsCString> AndroidDecoderModule::GetSupportedMimeTypes() {
  mozilla::jni::ObjectArray::LocalRef supportedTypes = mozilla::java::
      HardwareCodecCapabilityUtils::GetDecoderSupportedMimeTypes();

  nsTArray<nsCString> st = nsTArray<nsCString>();
  for (size_t i = 0; i < supportedTypes->Length(); i++) {
    st.AppendElement(
        jni::String::LocalRef(supportedTypes->GetElement(i))->ToCString());
  }

  return st;
}

nsTArray<nsCString> AndroidDecoderModule::GetSupportedMimeTypesPrefixed() {
  mozilla::jni::ObjectArray::LocalRef supportedTypes = mozilla::java::
      HardwareCodecCapabilityUtils::GetDecoderSupportedMimeTypesWithAccelInfo();

  nsTArray<nsCString> st = nsTArray<nsCString>();
  for (size_t i = 0; i < supportedTypes->Length(); i++) {
    st.AppendElement(
        jni::String::LocalRef(supportedTypes->GetElement(i))->ToCString());
  }

  return st;
}

void AndroidDecoderModule::SetSupportedMimeTypes() {
  SetSupportedMimeTypes(GetSupportedMimeTypesPrefixed());
}

// Inbound MIME types prefixed with SW/HW need to be processed
void AndroidDecoderModule::SetSupportedMimeTypes(
    nsTArray<nsCString>&& aSupportedTypes) {
  StaticMutexAutoLock lock(sMutex);
  // Return if support is already cached
  if (sSupportedSwMimeTypes && sSupportedHwMimeTypes && sSupportedCodecs) {
    return;
  }
  if (!sSupportedSwMimeTypes) {
    sSupportedSwMimeTypes = new nsTArray<nsCString>;
    if (NS_IsMainThread()) {
      ClearOnShutdown(&sSupportedSwMimeTypes);
    } else {
      Unused << NS_DispatchToMainThread(NS_NewRunnableFunction(__func__, []() {
        StaticMutexAutoLock lock(sMutex);
        ClearOnShutdown(&sSupportedSwMimeTypes);
      }));
    }
  }
  if (!sSupportedHwMimeTypes) {
    sSupportedHwMimeTypes = new nsTArray<nsCString>;
    if (NS_IsMainThread()) {
      ClearOnShutdown(&sSupportedHwMimeTypes);
    } else {
      Unused << NS_DispatchToMainThread(NS_NewRunnableFunction(__func__, []() {
        StaticMutexAutoLock lock(sMutex);
        ClearOnShutdown(&sSupportedHwMimeTypes);
      }));
    }
  }
  if (!sSupportedCodecs) {
    sSupportedCodecs = new MediaCodecsSupported();
    if (NS_IsMainThread()) {
      ClearOnShutdown(&sSupportedCodecs);
    } else {
      Unused << NS_DispatchToMainThread(NS_NewRunnableFunction(__func__, []() {
        StaticMutexAutoLock lock(sMutex);
        ClearOnShutdown(&sSupportedCodecs);
      }));
    }
  }

  // Process each MIME type string
  for (const auto& s : aSupportedTypes) {
    // Verify MIME type string present
    if (s.Length() < 4) {
      SLOG("No SW/HW support prefix found in codec string %s", s.Data());
      continue;
    }
    const auto mimeType = Substring(s, 3);
    if (mimeType.Length() == 0) {
      SLOG("No MIME type information found in codec string %s", s.Data());
      continue;
    }

    // Extract SW/HW support prefix
    const auto caps = Substring(s, 0, 2);
    DecodeSupport support{};
    if (caps == "SW"_ns) {
      sSupportedSwMimeTypes->AppendElement(mimeType);
      support = DecodeSupport::SoftwareDecode;
    } else if (caps == "HW"_ns) {
      sSupportedHwMimeTypes->AppendElement(mimeType);
      support = DecodeSupport::HardwareDecode;
    } else {
      SLOG("Error parsing acceleration info from JNI codec string %s",
           s.Data());
      continue;
    }
    const MediaCodec codec = MCSInfo::GetMediaCodecFromMimeType(mimeType);
    if (codec == MediaCodec::SENTINEL) {
      SLOG("Did not parse string %s to specific codec", s.Data());
      continue;
    }
    *sSupportedCodecs += MCSInfo::GetMediaCodecsSupportEnum(codec, support);
  }
}

DecodeSupportSet AndroidDecoderModule::SupportsMimeType(
    const nsACString& aMimeType, DecoderDoctorDiagnostics* aDiagnostics) const {
  return AndroidDecoderModule::SupportsMimeType(aMimeType);
}

bool AndroidDecoderModule::SupportsColorDepth(
    gfx::ColorDepth aColorDepth, DecoderDoctorDiagnostics* aDiagnostics) const {
  // 10-bit support is codec dependent so this is not entirely accurate.
  // Supports() will correct it.
  return aColorDepth == gfx::ColorDepth::COLOR_8 ||
         aColorDepth == gfx::ColorDepth::COLOR_10;
}

// Further check is needed because the base class uses the inaccurate
// SupportsColorDepth().
media::DecodeSupportSet AndroidDecoderModule::Supports(
    const SupportDecoderParams& aParams,
    DecoderDoctorDiagnostics* aDiagnostics) const {
  media::DecodeSupportSet support =
      PlatformDecoderModule::Supports(aParams, aDiagnostics);

  // Short-circuit.
  if (support.isEmpty()) {
    return support;
  }

#ifdef MOZ_AV1
  // For AV1, only allow HW decoder.
  if (AOMDecoder::IsAV1(aParams.MimeType()) &&
      (!StaticPrefs::media_av1_enabled() ||
       !support.contains(media::DecodeSupport::HardwareDecode))) {
    return media::DecodeSupportSet{};
  }
#endif

  // Check 10-bit video.
  const TrackInfo& trackInfo = aParams.mConfig;
  const VideoInfo* videoInfo = trackInfo.GetAsVideoInfo();
  if (!videoInfo || videoInfo->mColorDepth != gfx::ColorDepth::COLOR_10) {
    return support;
  }

  return java::HardwareCodecCapabilityUtils::Decodes10Bit(
             TranslateMimeType(aParams.MimeType()))
             ? support
             : media::DecodeSupportSet{};
}

static bool IsAV1MainProfile(const MediaByteBuffer* aBox) {
  if (!aBox || aBox->IsEmpty()) {
    return false;
  }
  AOMDecoder::AV1SequenceInfo av1Info;
  MediaResult seqHdrResult;
  AOMDecoder::TryReadAV1CBox(aBox, av1Info, seqHdrResult);
  return seqHdrResult.Code() == NS_OK && av1Info.mProfile == 0;
}

already_AddRefed<MediaDataDecoder> AndroidDecoderModule::CreateVideoDecoder(
    const CreateDecoderParams& aParams) {
  // Temporary - forces use of VPXDecoder when alpha is present.
  // Bug 1263836 will handle alpha scenario once implemented. It will shift
  // the check for alpha to PDMFactory but not itself remove the need for a
  // check.
  if (aParams.VideoConfig().HasAlpha()) {
    return nullptr;
  }

  if (AOMDecoder::IsAV1(aParams.mConfig.mMimeType) &&
      !IsAV1MainProfile(aParams.VideoConfig().mExtraData)) {
    return nullptr;
  }

  nsString drmStubId;
  if (mProxy) {
    drmStubId = mProxy->GetMediaDrmStubId();
  }

  RefPtr<MediaDataDecoder> decoder =
      RemoteDataDecoder::CreateVideoDecoder(aParams, drmStubId, mProxy);
  return decoder.forget();
}

already_AddRefed<MediaDataDecoder> AndroidDecoderModule::CreateAudioDecoder(
    const CreateDecoderParams& aParams) {
  const AudioInfo& config = aParams.AudioConfig();
  LOG("CreateAudioFormat with mimeType=%s, mRate=%d, channels=%d",
      config.mMimeType.Data(), config.mRate, config.mChannels);

  nsString drmStubId;
  if (mProxy) {
    drmStubId = mProxy->GetMediaDrmStubId();
  }
  RefPtr<MediaDataDecoder> decoder =
      RemoteDataDecoder::CreateAudioDecoder(aParams, drmStubId, mProxy);
  return decoder.forget();
}

/* static */
already_AddRefed<PlatformDecoderModule> AndroidDecoderModule::Create(
    CDMProxy* aProxy) {
  return MakeAndAddRef<AndroidDecoderModule>(aProxy);
}

}  // namespace mozilla
