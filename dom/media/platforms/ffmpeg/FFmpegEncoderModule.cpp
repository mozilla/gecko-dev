/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FFmpegEncoderModule.h"

#include "FFmpegLog.h"
#include "FFmpegAudioEncoder.h"
#include "FFmpegUtils.h"
#include "FFmpegVideoEncoder.h"

// This must be the last header included
#include "FFmpegLibs.h"

#include "mozilla/gfx/gfxVars.h"
#include "mozilla/StaticPrefs_media.h"

using mozilla::media::EncodeSupport;
using mozilla::media::EncodeSupportSet;

namespace mozilla {

template <int V>
/* static */ void FFmpegEncoderModule<V>::Init(FFmpegLibWrapper* aLib) {
#if defined(MOZ_USE_HWDECODE)
#  if defined(XP_WIN) && !defined(MOZ_FFVPX_AUDIOONLY)
  static constexpr AVCodecID kCodecIDs[] = {
      AV_CODEC_ID_AV1,
      AV_CODEC_ID_VP9,
  };
  for (const auto& codecId : kCodecIDs) {
    const auto* codec =
        FFmpegDataEncoder<V>::FindHardwareEncoder(aLib, codecId);
    if (!codec) {
      MOZ_LOG(
          sPEMLog, LogLevel::Debug,
          ("No codec or encoder for %s on d3d11va", AVCodecToString(codecId)));
      continue;
    }
    for (int i = 0;
         const AVCodecHWConfig* config = aLib->avcodec_get_hw_config(codec, i);
         ++i) {
      if (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX) {
        sSupportedHWCodecs.AppendElement(static_cast<uint32_t>(codecId));
        MOZ_LOG(sPEMLog, LogLevel::Debug,
                ("Support %s on d3d11va", AVCodecToString(codecId)));
        break;
      }
    }
  }
#  elif MOZ_WIDGET_GTK
  // UseXXXHWEncode are already set in gfxPlatform at the startup.
#    define ADD_HW_CODEC(codec)                          \
      if (gfx::gfxVars::Use##codec##HwEncode()) {        \
        sSupportedHWCodecs.AppendElement(                \
            static_cast<uint32_t>(AV_CODEC_ID_##codec)); \
      }

// These patented video codecs can only be encoded via hardware by using
// the system ffmpeg, not supported by ffvpx.
#    ifndef FFVPX_VERSION
  ADD_HW_CODEC(H264);
#      if LIBAVCODEC_VERSION_MAJOR >= 55
  ADD_HW_CODEC(HEVC);
#      endif
#    endif  // !FFVPX_VERSION

// The following royalty-free video codecs can be encoded via hardware using
// ffvpx.
#    if LIBAVCODEC_VERSION_MAJOR >= 54
  ADD_HW_CODEC(VP8);
#    endif
#    if LIBAVCODEC_VERSION_MAJOR >= 55
  ADD_HW_CODEC(VP9);
#    endif
#    if LIBAVCODEC_VERSION_MAJOR >= 59
  ADD_HW_CODEC(AV1);
#    endif

  for (const auto& codec : sSupportedHWCodecs) {
    MOZ_LOG(sPEMLog, LogLevel::Debug,
            ("Support %s for hw encoding",
             AVCodecToString(static_cast<AVCodecID>(codec))));
  }
#    undef ADD_HW_CODEC
#  endif  // XP_WIN, MOZ_WIDGET_GTK
#endif    // MOZ_USE_HWDECODE
}  // namespace mozilla

template <int V>
EncodeSupportSet FFmpegEncoderModule<V>::Supports(
    const EncoderConfig& aConfig) const {
  if (!CanLikelyEncode(aConfig)) {
    return EncodeSupportSet{};
  }
  // We only support L1T2 and L1T3 ScalabilityMode in VPX and AV1 encoders via
  // libvpx and libaom for now.
  if ((aConfig.mScalabilityMode != ScalabilityMode::None)) {
    if (aConfig.mCodec == CodecType::AV1) {
      // libaom only supports SVC in CBR mode.
      if (aConfig.mBitrateMode != BitrateMode::Constant) {
        return EncodeSupportSet{};
      }
    } else if (aConfig.mCodec != CodecType::VP8 &&
               aConfig.mCodec != CodecType::VP9) {
      return EncodeSupportSet{};
    }
  }
  return SupportsCodec(aConfig.mCodec);
}

template <int V>
EncodeSupportSet FFmpegEncoderModule<V>::SupportsCodec(CodecType aCodec) const {
  AVCodecID id = GetFFmpegEncoderCodecId<V>(aCodec);
  if (id == AV_CODEC_ID_NONE) {
    return EncodeSupportSet{};
  }
  EncodeSupportSet supports;
  if (StaticPrefs::media_ffvpx_hw_enabled() &&
      FFmpegDataEncoder<V>::FindHardwareEncoder(mLib, id) &&
      sSupportedHWCodecs.Contains(static_cast<uint32_t>(id))) {
    supports += EncodeSupport::HardwareEncode;
  }
  if (FFmpegDataEncoder<V>::FindSoftwareEncoder(mLib, id)) {
    supports += EncodeSupport::SoftwareEncode;
  }
  return supports;
}

template <int V>
already_AddRefed<MediaDataEncoder> FFmpegEncoderModule<V>::CreateVideoEncoder(
    const EncoderConfig& aConfig, const RefPtr<TaskQueue>& aTaskQueue) const {
  AVCodecID codecId = GetFFmpegEncoderCodecId<V>(aConfig.mCodec);
  if (codecId == AV_CODEC_ID_NONE) {
    FFMPEGV_LOG("No ffmpeg encoder for %s", GetCodecTypeString(aConfig.mCodec));
    return nullptr;
  }

  RefPtr<MediaDataEncoder> encoder =
      new FFmpegVideoEncoder<V>(mLib, codecId, aTaskQueue, aConfig);
  FFMPEGV_LOG("ffmpeg %s encoder: %s has been created",
              GetCodecTypeString(aConfig.mCodec),
              encoder->GetDescriptionName().get());
  return encoder.forget();
}

template <int V>
already_AddRefed<MediaDataEncoder> FFmpegEncoderModule<V>::CreateAudioEncoder(
    const EncoderConfig& aConfig, const RefPtr<TaskQueue>& aTaskQueue) const {
  AVCodecID codecId = GetFFmpegEncoderCodecId<V>(aConfig.mCodec);
  if (codecId == AV_CODEC_ID_NONE) {
    FFMPEGV_LOG("No ffmpeg encoder for %s", GetCodecTypeString(aConfig.mCodec));
    return nullptr;
  }

  RefPtr<MediaDataEncoder> encoder =
      new FFmpegAudioEncoder<V>(mLib, codecId, aTaskQueue, aConfig);
  FFMPEGA_LOG("ffmpeg %s encoder: %s has been created",
              GetCodecTypeString(aConfig.mCodec),
              encoder->GetDescriptionName().get());
  return encoder.forget();
}

template class FFmpegEncoderModule<LIBAV_VER>;

}  // namespace mozilla
