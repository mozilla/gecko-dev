/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_PLATFORMS_FFMPEG_FFMPEGENCODERMODULE_H_
#define DOM_MEDIA_PLATFORMS_FFMPEG_FFMPEGENCODERMODULE_H_

#include "FFmpegLibWrapper.h"
#include "PlatformEncoderModule.h"

namespace mozilla {

extern LazyLogModule sPEMLog;

template <int V>
class FFmpegEncoderModule final : public PlatformEncoderModule {
 public:
  virtual ~FFmpegEncoderModule() = default;

  static void Init(FFmpegLibWrapper* aLib) {
#if defined(MOZ_USE_HWDECODE)
#  if defined(XP_WIN) && !defined(MOZ_FFVPX_AUDIOONLY)
    static nsTArray<AVCodecID> kCodecIDs({
        AV_CODEC_ID_AV1,
        AV_CODEC_ID_VP9,
    });
    for (const auto& codecId : kCodecIDs) {
      const auto* codec =
          FFmpegDataEncoder<V>::FindHardwareEncoder(aLib, codecId);
      if (!codec) {
        MOZ_LOG(sPEMLog, LogLevel::Debug,
                ("No codec or encoder for %s on d3d11va",
                 AVCodecToString(codecId)));
        continue;
      }
      for (int i = 0; const AVCodecHWConfig* config =
                          aLib->avcodec_get_hw_config(codec, i);
           ++i) {
        if (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX) {
          sSupportedHWCodecs.AppendElement(codecId);
          MOZ_LOG(sPEMLog, LogLevel::Debug,
                  ("Support %s on d3d11va", AVCodecToString(codecId)));
          break;
        }
      }
    }
#  elif MOZ_WIDGET_GTK
    // UseXXXHWEncode are already set in gfxPlatform at the startup.
#    define ADD_HW_CODEC(codec)                                \
      if (gfx::gfxVars::Use##codec##HwEncode()) {              \
        sSupportedHWCodecs.AppendElement(AV_CODEC_ID_##codec); \
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
              ("Support %s for hw encoding", AVCodecToString(codec)));
    }
#    undef ADD_HW_CODEC
#  endif  // XP_WIN, MOZ_WIDGET_GTK
#endif    // MOZ_USE_HWDECODE
  }

  static already_AddRefed<PlatformEncoderModule> Create(
      FFmpegLibWrapper* aLib) {
    RefPtr<PlatformEncoderModule> pem = new FFmpegEncoderModule(aLib);
    return pem.forget();
  }
  media::EncodeSupportSet Supports(const EncoderConfig& aConfig) const override;
  media::EncodeSupportSet SupportsCodec(CodecType aCodec) const override;

  const char* GetName() const override { return "FFmpeg Encoder Module"; }

  already_AddRefed<MediaDataEncoder> CreateVideoEncoder(
      const EncoderConfig& aConfig,
      const RefPtr<TaskQueue>& aTaskQueue) const override;

  already_AddRefed<MediaDataEncoder> CreateAudioEncoder(
      const EncoderConfig& aConfig,
      const RefPtr<TaskQueue>& aTaskQueue) const override;

 protected:
  explicit FFmpegEncoderModule(FFmpegLibWrapper* aLib) : mLib(aLib) {
    MOZ_ASSERT(mLib);
  }

 private:
  // This refers to a static FFmpegLibWrapper, so raw pointer is adequate.
  const FFmpegLibWrapper* mLib;  // set in constructor
  MOZ_RUNINIT static inline nsTArray<AVCodecID> sSupportedHWCodecs;
};

}  // namespace mozilla

#endif /* DOM_MEDIA_PLATFORMS_FFMPEG_FFMPEGENCODERMODULE_H_ */
