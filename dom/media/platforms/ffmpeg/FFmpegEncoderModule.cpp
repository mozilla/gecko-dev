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
#if (defined(XP_WIN) || defined(MOZ_WIDGET_GTK)) && \
    defined(MOZ_USE_HWDECODE) && !defined(MOZ_FFVPX_AUDIOONLY)
#  ifdef XP_WIN
  if (!XRE_IsGPUProcess()) {
    return;
  }
#  else
  if (!XRE_IsRDDProcess()) {
    return;
  }
#  endif

  struct CodecEntry {
    AVCodecID mId;
    bool mHwAllowed;
  };

  const CodecEntry kCodecIDs[] = {
  // The following open video codecs can be encoded via hardware by using the
  // system ffmpeg or ffvpx.
#  if LIBAVCODEC_VERSION_MAJOR >= 59
      {AV_CODEC_ID_AV1, gfx::gfxVars::UseAV1HwEncode()},
#  endif
#  if LIBAVCODEC_VERSION_MAJOR >= 55
      {AV_CODEC_ID_VP9, gfx::gfxVars::UseVP9HwEncode()},
#  endif
#  if defined(MOZ_WIDGET_GTK) && LIBAVCODEC_VERSION_MAJOR >= 54
      {AV_CODEC_ID_VP8, gfx::gfxVars::UseVP8HwEncode()},
#  endif

  // These proprietary video codecs can only be encoded via hardware by using
  // the system ffmpeg, not supported by ffvpx.
#  if defined(MOZ_WIDGET_GTK) && !defined(FFVPX_VERSION)
#    if LIBAVCODEC_VERSION_MAJOR >= 55
      {AV_CODEC_ID_HEVC, gfx::gfxVars::UseHEVCHwEncode()},
#    endif
      {AV_CODEC_ID_H264, gfx::gfxVars::UseH264HwEncode()},
#  endif
  };

  for (const auto& entry : kCodecIDs) {
    if (!entry.mHwAllowed) {
      MOZ_LOG(
          sPDMLog, LogLevel::Debug,
          ("Hw codec disabled by gfxVars for %s", AVCodecToString(entry.mId)));
      continue;
    }

    const auto* codec =
        FFmpegDataEncoder<V>::FindHardwareEncoder(aLib, entry.mId);
    if (!codec) {
      MOZ_LOG(sPDMLog, LogLevel::Debug,
              ("No hw codec or encoder for %s", AVCodecToString(entry.mId)));
      continue;
    }

    sSupportedHWCodecs.AppendElement(entry.mId);
    MOZ_LOG(sPDMLog, LogLevel::Debug,
            ("Support %s for hw encoding", AVCodecToString(entry.mId)));
  }
#endif  // (XP_WIN || MOZ_WIDGET_GTK) && MOZ_USE_HWDECODE &&
        // !MOZ_FFVPX_AUDIOONLY
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
#ifdef MOZ_USE_HWDECODE
  if (StaticPrefs::media_ffvpx_hw_enabled() &&
      FFmpegDataEncoder<V>::FindHardwareEncoder(mLib, id) &&
      sSupportedHWCodecs.Contains(static_cast<uint32_t>(id))) {
    supports += EncodeSupport::HardwareEncode;
  }
#endif
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
