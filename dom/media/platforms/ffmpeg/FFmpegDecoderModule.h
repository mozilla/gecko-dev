/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __FFmpegDecoderModule_h__
#define __FFmpegDecoderModule_h__

#include "FFmpegAudioDecoder.h"
#include "FFmpegLibWrapper.h"
#include "FFmpegUtils.h"
#include "FFmpegVideoDecoder.h"
#include "MP4Decoder.h"
#include "PlatformDecoderModule.h"
#include "VideoUtils.h"
#include "VPXDecoder.h"
#include "mozilla/StaticPrefs_media.h"
#include "mozilla/gfx/gfxVars.h"

namespace mozilla {

template <int V>
class FFmpegDecoderModule : public PlatformDecoderModule {
 public:
  static void Init(FFmpegLibWrapper* aLib) {
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
    // The following open video codecs can be decoded via hardware by using the
    // system ffmpeg or ffvpx.
#  if LIBAVCODEC_VERSION_MAJOR >= 59
        {AV_CODEC_ID_AV1, gfx::gfxVars::UseAV1HwDecode()},
#  endif
#  if LIBAVCODEC_VERSION_MAJOR >= 55
        {AV_CODEC_ID_VP9, gfx::gfxVars::UseVP9HwDecode()},
#  endif
#  if defined(MOZ_WIDGET_GTK) && LIBAVCODEC_VERSION_MAJOR >= 54
        {AV_CODEC_ID_VP8, gfx::gfxVars::UseVP8HwDecode()},
#  endif

    // These proprietary video codecs can only be decoded via hardware by using
    // the system ffmpeg, not supported by ffvpx.
#  if defined(MOZ_WIDGET_GTK) && !defined(FFVPX_VERSION)
#    if LIBAVCODEC_VERSION_MAJOR >= 55
        {AV_CODEC_ID_HEVC, gfx::gfxVars::UseHEVCHwDecode()},
#    endif
        {AV_CODEC_ID_H264, gfx::gfxVars::UseH264HwDecode()},
#  endif
    };

    for (const auto& entry : kCodecIDs) {
      if (!entry.mHwAllowed) {
        MOZ_LOG(sPDMLog, LogLevel::Debug,
                ("Hw codec disabled by gfxVars for %s",
                 AVCodecToString(entry.mId)));
        continue;
      }

      const auto* codec =
          FFmpegDataDecoder<V>::FindHardwareAVCodec(aLib, entry.mId);
      if (!codec) {
        MOZ_LOG(sPDMLog, LogLevel::Debug,
                ("No hw codec or decoder for %s", AVCodecToString(entry.mId)));
        continue;
      }

      sSupportedHWCodecs.AppendElement(entry.mId);
      MOZ_LOG(sPDMLog, LogLevel::Debug,
              ("Support %s for hw decoding", AVCodecToString(entry.mId)));
    }
#endif  // (XP_WIN || MOZ_WIDGET_GTK) && MOZ_USE_HWDECODE &&
        // !MOZ_FFVPX_AUDIOONLY
  }

  static already_AddRefed<PlatformDecoderModule> Create(
      FFmpegLibWrapper* aLib) {
    RefPtr<PlatformDecoderModule> pdm = new FFmpegDecoderModule(aLib);

    return pdm.forget();
  }

  explicit FFmpegDecoderModule(FFmpegLibWrapper* aLib) : mLib(aLib) {}
  virtual ~FFmpegDecoderModule() = default;

  already_AddRefed<MediaDataDecoder> CreateVideoDecoder(
      const CreateDecoderParams& aParams) override {
    if (Supports(SupportDecoderParams(aParams), nullptr).isEmpty()) {
      return nullptr;
    }
    auto decoder = MakeRefPtr<FFmpegVideoDecoder<V>>(
        mLib, aParams.VideoConfig(), aParams.mKnowsCompositor,
        aParams.mImageContainer,
        aParams.mOptions.contains(CreateDecoderParams::Option::LowLatency),
        aParams.mOptions.contains(
            CreateDecoderParams::Option::HardwareDecoderNotAllowed),
        aParams.mOptions.contains(
            CreateDecoderParams::Option::Output8BitPerChannel),
        aParams.mTrackingId);

    // Ensure that decoding is exclusively performed using HW decoding in
    // the GPU process. If FFmpeg does not support HW decoding, reset the
    // decoder to allow PDMFactory to select an alternative HW-capable decoder
    // module if available. In contrast, in the RDD process, it is acceptable
    // to fallback to SW decoding when HW decoding is not available.
    if (XRE_IsGPUProcess() &&
        IsHWDecodingSupported(aParams.mConfig.mMimeType) &&
        !decoder->IsHardwareAccelerated()) {
      MOZ_LOG(sPDMLog, LogLevel::Debug,
              ("FFmpeg video decoder can't perform hw decoding, abort!"));
      Unused << decoder->Shutdown();
      decoder = nullptr;
    }
    return decoder.forget();
  }

  already_AddRefed<MediaDataDecoder> CreateAudioDecoder(
      const CreateDecoderParams& aParams) override {
    if (Supports(SupportDecoderParams(aParams), nullptr).isEmpty()) {
      return nullptr;
    }
    RefPtr<MediaDataDecoder> decoder = new FFmpegAudioDecoder<V>(mLib, aParams);
    return decoder.forget();
  }

  media::DecodeSupportSet SupportsMimeType(
      const nsACString& aMimeType,
      DecoderDoctorDiagnostics* aDiagnostics) const override {
    UniquePtr<TrackInfo> trackInfo = CreateTrackInfoWithMIMEType(aMimeType);
    if (!trackInfo) {
      return media::DecodeSupportSet{};
    }
    return Supports(SupportDecoderParams(*trackInfo), aDiagnostics);
  }

  media::DecodeSupportSet Supports(
      const SupportDecoderParams& aParams,
      DecoderDoctorDiagnostics* aDiagnostics) const override {
    // This should only be supported by MFMediaEngineDecoderModule.
    if (aParams.mMediaEngineId) {
      return media::DecodeSupportSet{};
    }

    const auto& trackInfo = aParams.mConfig;
    const nsACString& mimeType = trackInfo.mMimeType;
    if (XRE_IsGPUProcess() && !IsHWDecodingSupported(mimeType)) {
      MOZ_LOG(
          sPDMLog, LogLevel::Debug,
          ("FFmpeg decoder rejects requested type '%s' for hardware decoding",
           mimeType.BeginReading()));
      return media::DecodeSupportSet{};
    }

    // Temporary - forces use of VPXDecoder when alpha is present.
    // Bug 1263836 will handle alpha scenario once implemented. It will shift
    // the check for alpha to PDMFactory but not itself remove the need for a
    // check.
    if (VPXDecoder::IsVPX(mimeType) && trackInfo.GetAsVideoInfo()->HasAlpha()) {
      MOZ_LOG(sPDMLog, LogLevel::Debug,
              ("FFmpeg decoder rejects requested type '%s'",
               mimeType.BeginReading()));
      return media::DecodeSupportSet{};
    }

    if (VPXDecoder::IsVP9(mimeType) &&
        aParams.mOptions.contains(CreateDecoderParams::Option::LowLatency)) {
      // SVC layers are unsupported, and may be used in low latency use cases
      // (WebRTC).
      return media::DecodeSupportSet{};
    }

    if (MP4Decoder::IsHEVC(mimeType) && !StaticPrefs::media_hevc_enabled()) {
      MOZ_LOG(
          sPDMLog, LogLevel::Debug,
          ("FFmpeg decoder rejects requested type '%s' due to being disabled "
           "by the pref",
           mimeType.BeginReading()));
      return media::DecodeSupportSet{};
    }

    AVCodecID videoCodec = FFmpegVideoDecoder<V>::GetCodecId(mimeType);
    AVCodecID audioCodec = FFmpegAudioDecoder<V>::GetCodecId(
        mimeType,
        trackInfo.GetAsAudioInfo() ? *trackInfo.GetAsAudioInfo() : AudioInfo());
    if (audioCodec == AV_CODEC_ID_NONE && videoCodec == AV_CODEC_ID_NONE) {
      MOZ_LOG(sPDMLog, LogLevel::Debug,
              ("FFmpeg decoder rejects requested type '%s'",
               mimeType.BeginReading()));
      return media::DecodeSupportSet{};
    }
    AVCodecID codecId =
        audioCodec != AV_CODEC_ID_NONE ? audioCodec : videoCodec;
    AVCodec* codec = FFmpegDataDecoder<V>::FindAVCodec(mLib, codecId);
    MOZ_LOG(sPDMLog, LogLevel::Debug,
            ("FFmpeg decoder %s requested type '%s'",
             !!codec ? "supports" : "rejects", mimeType.BeginReading()));
    if (!codec) {
      return media::DecodeSupportSet{};
    }
    // This logic is mirrored in FFmpegDataDecoder<LIBAV_VER>::InitDecoder and
    // FFmpegVideoDecoder<LIBAV_VER>::InitVAAPIDecoder. We prefer to use our own
    // OpenH264 decoder through the plugin over ffmpeg by default due to broken
    // decoding with some versions.
    if (!strcmp(codec->name, "libopenh264") &&
        !StaticPrefs::media_ffmpeg_allow_openh264()) {
      MOZ_LOG(sPDMLog, LogLevel::Debug,
              ("FFmpeg decoder rejects as openh264 disabled by pref"));
      return media::DecodeSupportSet{};
    }
    media::DecodeSupportSet support = media::DecodeSupport::SoftwareDecode;
    if (IsHWDecodingSupported(mimeType)) {
      support += media::DecodeSupport::HardwareDecode;
    }
    return support;
  }

 protected:
  bool SupportsColorDepth(
      gfx::ColorDepth aColorDepth,
      DecoderDoctorDiagnostics* aDiagnostics) const override {
#if defined(MOZ_WIDGET_ANDROID)
    return aColorDepth == gfx::ColorDepth::COLOR_8;
#endif
    return true;
  }

  bool IsHWDecodingSupported(const nsACString& aMimeType) const {
    if (!gfx::gfxVars::IsInitialized() ||
        !gfx::gfxVars::CanUseHardwareVideoDecoding()) {
      return false;
    }
#ifdef FFVPX_VERSION
    if (!StaticPrefs::media_ffvpx_hw_enabled()) {
      return false;
    }
#endif
    AVCodecID videoCodec = FFmpegVideoDecoder<V>::GetCodecId(aMimeType);
    return sSupportedHWCodecs.Contains(videoCodec);
  }

 private:
  FFmpegLibWrapper* mLib;
  MOZ_RUNINIT static inline nsTArray<AVCodecID> sSupportedHWCodecs;
};

}  // namespace mozilla

#endif  // __FFmpegDecoderModule_h__
