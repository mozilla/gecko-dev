/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PEMFactory.h"

#include "PlatformEncoderModule.h"

#ifdef MOZ_APPLEMEDIA
#  include "AppleEncoderModule.h"
#endif

#ifdef MOZ_WIDGET_ANDROID
#  include "AndroidEncoderModule.h"
#endif

#ifdef XP_WIN
#  include "WMFEncoderModule.h"
#endif

#ifdef MOZ_FFMPEG
#  include "FFmpegRuntimeLinker.h"
#endif

#include "FFVPXRuntimeLinker.h"

#include "GMPEncoderModule.h"

#include "mozilla/StaticMutex.h"
#include "mozilla/StaticPrefs_media.h"
#include "mozilla/gfx/gfxVars.h"

namespace mozilla {

LazyLogModule sPEMLog("PlatformEncoderModule");

#define LOGE(fmt, ...)                       \
  MOZ_LOG(sPEMLog, mozilla::LogLevel::Error, \
          ("[PEMFactory] %s: " fmt, __func__, ##__VA_ARGS__))
#define LOG(fmt, ...)                        \
  MOZ_LOG(sPEMLog, mozilla::LogLevel::Debug, \
          ("[PEMFactory] %s: " fmt, __func__, ##__VA_ARGS__))

static CodecType MediaCodecToCodecType(media::MediaCodec aCodec) {
  switch (aCodec) {
    case media::MediaCodec::H264:
      return CodecType::H264;
    case media::MediaCodec::VP8:
      return CodecType::VP8;
    case media::MediaCodec::VP9:
      return CodecType::VP9;
    case media::MediaCodec::AV1:
      return CodecType::AV1;
    case media::MediaCodec::HEVC:
      return CodecType::H265;
    case media::MediaCodec::AAC:
      return CodecType::AAC;
    case media::MediaCodec::FLAC:
      return CodecType::Flac;
    case media::MediaCodec::Opus:
      return CodecType::Opus;
    case media::MediaCodec::Vorbis:
      return CodecType::Vorbis;
    case media::MediaCodec::MP3:
    case media::MediaCodec::Wave:
    case media::MediaCodec::SENTINEL:
      return CodecType::Unknown;
    default:
      MOZ_ASSERT_UNREACHABLE("Unhandled MediaCodec type!");
      return CodecType::Unknown;
  }
}

PEMFactory::PEMFactory() {
  gfx::gfxVars::Initialize();
#ifdef MOZ_APPLEMEDIA
  RefPtr<PlatformEncoderModule> m(new AppleEncoderModule());
  mCurrentPEMs.AppendElement(m);
#endif

#ifdef MOZ_WIDGET_ANDROID
  mCurrentPEMs.AppendElement(new AndroidEncoderModule());
#endif

#ifdef XP_WIN
  mCurrentPEMs.AppendElement(new WMFEncoderModule());
#endif

  if (StaticPrefs::media_ffmpeg_encoder_enabled()) {
    if (RefPtr<PlatformEncoderModule> pem =
            FFVPXRuntimeLinker::CreateEncoder()) {
      mCurrentPEMs.AppendElement(pem);
    }
  }

#ifdef MOZ_FFMPEG
  if (StaticPrefs::media_ffmpeg_enabled() &&
      StaticPrefs::media_ffmpeg_encoder_enabled()) {
    if (RefPtr<PlatformEncoderModule> pem =
            FFmpegRuntimeLinker::CreateEncoder()) {
      mCurrentPEMs.AppendElement(pem);
    }
  }
#endif

  if (StaticPrefs::media_gmp_encoder_enabled()) {
    auto pem = MakeRefPtr<GMPEncoderModule>();
    if (StaticPrefs::media_gmp_encoder_preferred()) {
      mCurrentPEMs.InsertElementAt(0, std::move(pem));
    } else {
      mCurrentPEMs.AppendElement(std::move(pem));
    }
  }
}

already_AddRefed<MediaDataEncoder> PEMFactory::CreateEncoder(
    const EncoderConfig& aConfig, const RefPtr<TaskQueue>& aTaskQueue) {
  RefPtr<PlatformEncoderModule> m = FindPEM(aConfig);
  if (!m) {
    return nullptr;
  }

  return aConfig.IsVideo() ? m->CreateVideoEncoder(aConfig, aTaskQueue)
                           : nullptr;
}

RefPtr<PlatformEncoderModule::CreateEncoderPromise>
PEMFactory::CreateEncoderAsync(const EncoderConfig& aConfig,
                               const RefPtr<TaskQueue>& aTaskQueue) {
  return CheckAndMaybeCreateEncoder(aConfig, 0, aTaskQueue);
}

RefPtr<PlatformEncoderModule::CreateEncoderPromise>
PEMFactory::CheckAndMaybeCreateEncoder(const EncoderConfig& aConfig,
                                       uint32_t aIndex,
                                       const RefPtr<TaskQueue>& aTaskQueue) {
  for (uint32_t i = aIndex; i < mCurrentPEMs.Length(); i++) {
    if (mCurrentPEMs[i]->Supports(aConfig).isEmpty()) {
      continue;
    }
    return CreateEncoderWithPEM(mCurrentPEMs[i], aConfig, aTaskQueue)
        ->Then(
            GetCurrentSerialEventTarget(), __func__,
            [](RefPtr<MediaDataEncoder>&& aEncoder) {
              return PlatformEncoderModule::CreateEncoderPromise::
                  CreateAndResolve(std::move(aEncoder), __func__);
            },
            [self = RefPtr{this}, i, config = aConfig,
             aTaskQueue](const MediaResult& aError) mutable {
              // Try the next PEM.
              return self->CheckAndMaybeCreateEncoder(config, i + 1,
                                                      aTaskQueue);
            });
  }
  return PlatformEncoderModule::CreateEncoderPromise::CreateAndReject(
      MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                  nsPrintfCString("Error no encoder found for %s",
                                  GetCodecTypeString(aConfig.mCodec))
                      .get()),
      __func__);
}

RefPtr<PlatformEncoderModule::CreateEncoderPromise>
PEMFactory::CreateEncoderWithPEM(PlatformEncoderModule* aPEM,
                                 const EncoderConfig& aConfig,
                                 const RefPtr<TaskQueue>& aTaskQueue) {
  MOZ_ASSERT(aPEM);
  MediaResult result = NS_OK;

  if (aConfig.IsAudio()) {
    return aPEM->AsyncCreateEncoder(aConfig, aTaskQueue)
        ->Then(
            GetCurrentSerialEventTarget(), __func__,
            [config = aConfig](RefPtr<MediaDataEncoder>&& aEncoder) {
              RefPtr<MediaDataEncoder> decoder = std::move(aEncoder);
              return PlatformEncoderModule::CreateEncoderPromise::
                  CreateAndResolve(decoder, __func__);
            },
            [](const MediaResult& aError) {
              return PlatformEncoderModule::CreateEncoderPromise::
                  CreateAndReject(aError, __func__);
            });
  }

  if (!aConfig.IsVideo()) {
    return PlatformEncoderModule::CreateEncoderPromise::CreateAndReject(
        MediaResult(
            NS_ERROR_DOM_MEDIA_FATAL_ERR,
            RESULT_DETAIL(
                "Encoder configuration error, expected audio or video.")),
        __func__);
  }

  return aPEM->AsyncCreateEncoder(aConfig, aTaskQueue);
}

media::EncodeSupportSet PEMFactory::Supports(
    const EncoderConfig& aConfig) const {
  RefPtr<PlatformEncoderModule> found;
  for (const auto& m : mCurrentPEMs) {
    media::EncodeSupportSet supports = m->Supports(aConfig);
    if (!supports.isEmpty()) {
      // TODO name
      LOG("Checking if %s supports codec %s: yes", m->GetName(),
          GetCodecTypeString(aConfig.mCodec));
      return supports;
    }
    LOG("Checking if %s supports codec %s: no", m->GetName(),
        GetCodecTypeString(aConfig.mCodec));
  }
  return media::EncodeSupportSet{};
}

media::EncodeSupportSet PEMFactory::SupportsCodec(CodecType aCodec) const {
  media::EncodeSupportSet supports{};
  for (const auto& m : mCurrentPEMs) {
    media::EncodeSupportSet pemSupports = m->SupportsCodec(aCodec);
    // TODO name
    LOG("Checking if %s supports codec %d: %s", m->GetName(),
        static_cast<int>(aCodec), pemSupports.isEmpty() ? "no" : "yes");
    supports += pemSupports;
  }
  if (supports.isEmpty()) {
    LOG("No PEM support %d", static_cast<int>(aCodec));
  }
  return supports;
}

already_AddRefed<PlatformEncoderModule> PEMFactory::FindPEM(
    const EncoderConfig& aConfig) const {
  RefPtr<PlatformEncoderModule> found;
  for (const auto& m : mCurrentPEMs) {
    if (!m->Supports(aConfig).isEmpty()) {
      found = m;
      break;
    }
  }
  return found.forget();
}

StaticMutex PEMFactory::sSupportedMutex;

/* static */
media::MediaCodecsSupported PEMFactory::Supported(bool aForceRefresh) {
  StaticMutexAutoLock lock(sSupportedMutex);

  static auto calculate = []() {
    auto pem = MakeRefPtr<PEMFactory>();
    MediaCodecsSupported supported;
    for (const auto& cd : MCSInfo::GetAllCodecDefinitions()) {
      auto codecType = MediaCodecToCodecType(cd.codec);
      if (codecType == CodecType::Unknown) {
        continue;
      }
      supported += MCSInfo::GetEncodeMediaCodecsSupported(
          cd.codec, pem->SupportsCodec(codecType));
    }
    return supported;
  };

  static MediaCodecsSupported supported = calculate();
  if (aForceRefresh) {
    supported = calculate();
  }

  return supported;
}

}  // namespace mozilla

#undef LOGE
#undef LOG
