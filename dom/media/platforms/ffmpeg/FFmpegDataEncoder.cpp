/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FFmpegDataEncoder.h"
#include "PlatformEncoderModule.h"

#include <utility>

#include "FFmpegLog.h"
#include "libavutil/error.h"
#include "mozilla/StaticMutex.h"
#include "mozilla/StaticPrefs_media.h"

#include "FFmpegUtils.h"

namespace mozilla {

template <>
AVCodecID GetFFmpegEncoderCodecId<LIBAV_VER>(CodecType aCodec) {
#if LIBAVCODEC_VERSION_MAJOR >= 58
  if (XRE_IsParentProcess() || XRE_IsContentProcess() ||
      StaticPrefs::media_use_remote_encoder_video()) {
    if (aCodec == CodecType::VP8) {
      return AV_CODEC_ID_VP8;
    }

    if (aCodec == CodecType::VP9) {
      return AV_CODEC_ID_VP9;
    }

    if (aCodec == CodecType::H264) {
      return AV_CODEC_ID_H264;
    }

    if (aCodec == CodecType::AV1) {
      return AV_CODEC_ID_AV1;
    }
  }

  if (XRE_IsParentProcess() || XRE_IsContentProcess() ||
      StaticPrefs::media_use_remote_encoder_audio()) {
    if (aCodec == CodecType::Opus) {
      return AV_CODEC_ID_OPUS;
    }

    if (aCodec == CodecType::Vorbis) {
      return AV_CODEC_ID_VORBIS;
    }
  }
#endif
  return AV_CODEC_ID_NONE;
}

/* static */
AVCodec* FFmpegDataEncoder<LIBAV_VER>::FindSoftwareEncoder(
    const FFmpegLibWrapper* aLib, AVCodecID aCodecId) {
  MOZ_ASSERT(aLib);

  AVCodec* fallbackCodec = nullptr;
  void* opaque = nullptr;
  while (AVCodec* codec = aLib->av_codec_iterate(&opaque)) {
    if (codec->id != aCodecId || !aLib->av_codec_is_encoder(codec) ||
        aLib->avcodec_get_hw_config(codec, 0)) {
      continue;
    }

    // Prioritize libx264 for now since it's the only h264 codec we tested.
    // Once libopenh264 is supported, we can simply use the first one we find.
    if (aCodecId == AV_CODEC_ID_H264 && strcmp(codec->name, "libx264") != 0) {
      if (!fallbackCodec) {
        fallbackCodec = codec;
      }
      continue;
    }

#if LIBAVCODEC_VERSION_MAJOR >= 57
    if (codec->capabilities & AV_CODEC_CAP_EXPERIMENTAL) {
      if (!fallbackCodec) {
        fallbackCodec = codec;
      }
      continue;
    }
#endif

    FFMPEGV_LOG("Using preferred software codec %s", codec->name);
    return codec;
  }

  if (fallbackCodec) {
    FFMPEGV_LOG("Using fallback software codec %s", fallbackCodec->name);
  }
  return fallbackCodec;
}

/* static */
AVCodec* FFmpegDataEncoder<LIBAV_VER>::FindHardwareEncoder(
    const FFmpegLibWrapper* aLib, AVCodecID aCodecId) {
  MOZ_ASSERT(aLib);

  AVCodec* fallbackCodec = nullptr;
  void* opaque = nullptr;
  while (AVCodec* codec = aLib->av_codec_iterate(&opaque)) {
    if (codec->id != aCodecId || !aLib->av_codec_is_encoder(codec) ||
        !aLib->avcodec_get_hw_config(codec, 0)) {
      continue;
    }

#if LIBAVCODEC_VERSION_MAJOR >= 57
    if (codec->capabilities & AV_CODEC_CAP_EXPERIMENTAL) {
      if (!fallbackCodec) {
        fallbackCodec = codec;
      }
      continue;
    }
#endif

    FFMPEGV_LOG("Using preferred hardware codec %s", codec->name);
    return codec;
  }

  if (fallbackCodec) {
    FFMPEGV_LOG("Using fallback hardware codec %s", fallbackCodec->name);
  }
  return fallbackCodec;
}

/* static */
Result<RefPtr<MediaRawData>, MediaResult>
FFmpegDataEncoder<LIBAV_VER>::CreateMediaRawData(AVPacket* aPacket) {
  MOZ_ASSERT(aPacket);

  // Copy frame data from AVPacket.
  auto data = MakeRefPtr<MediaRawData>();
  UniquePtr<MediaRawDataWriter> writer(data->CreateWriter());
  if (!writer->Append(aPacket->data, static_cast<size_t>(aPacket->size))) {
    return Err(MediaResult(NS_ERROR_OUT_OF_MEMORY,
                           "fail to allocate MediaRawData buffer"_ns));
  }
  return data;
}

StaticMutex FFmpegDataEncoder<LIBAV_VER>::sMutex;

FFmpegDataEncoder<LIBAV_VER>::FFmpegDataEncoder(
    const FFmpegLibWrapper* aLib, AVCodecID aCodecID,
    const RefPtr<TaskQueue>& aTaskQueue, const EncoderConfig& aConfig)
    : mLib(aLib),
      mCodecID(aCodecID),
      mTaskQueue(aTaskQueue),
      mConfig(aConfig),
      mCodecName(EmptyCString()),
      mCodecContext(nullptr),
      mFrame(nullptr),
      mVideoCodec(IsVideoCodec(aCodecID)) {
  MOZ_ASSERT(mLib);
  MOZ_ASSERT(mTaskQueue);
#if LIBAVCODEC_VERSION_MAJOR < 58
  MOZ_CRASH("FFmpegDataEncoder needs ffmpeg 58 at least.");
#endif
};

RefPtr<MediaDataEncoder::EncodePromise> FFmpegDataEncoder<LIBAV_VER>::Encode(
    const MediaData* aSample) {
  MOZ_ASSERT(aSample != nullptr);

  FFMPEG_LOG("Encode");
  return InvokeAsync(mTaskQueue, __func__,
                     [self = RefPtr<FFmpegDataEncoder<LIBAV_VER>>(this),
                      sample = RefPtr<const MediaData>(aSample)]() {
                       return self->ProcessEncode(sample);
                     });
}

RefPtr<MediaDataEncoder::ReconfigurationPromise>
FFmpegDataEncoder<LIBAV_VER>::Reconfigure(
    const RefPtr<const EncoderConfigurationChangeList>& aConfigurationChanges) {
  return InvokeAsync(mTaskQueue, this, __func__,
                     &FFmpegDataEncoder<LIBAV_VER>::ProcessReconfigure,
                     aConfigurationChanges);
}

RefPtr<MediaDataEncoder::EncodePromise> FFmpegDataEncoder<LIBAV_VER>::Drain() {
  FFMPEG_LOG("Drain");
  return InvokeAsync(mTaskQueue, this, __func__,
                     &FFmpegDataEncoder::ProcessDrain);
}

RefPtr<ShutdownPromise> FFmpegDataEncoder<LIBAV_VER>::Shutdown() {
  FFMPEG_LOG("Shutdown");
  return InvokeAsync(mTaskQueue, this, __func__,
                     &FFmpegDataEncoder::ProcessShutdown);
}

RefPtr<GenericPromise> FFmpegDataEncoder<LIBAV_VER>::SetBitrate(
    uint32_t aBitrate) {
  FFMPEG_LOG("SetBitrate");
  return GenericPromise::CreateAndReject(NS_ERROR_NOT_IMPLEMENTED, __func__);
}

RefPtr<MediaDataEncoder::EncodePromise>
FFmpegDataEncoder<LIBAV_VER>::ProcessEncode(RefPtr<const MediaData> aSample) {
  MOZ_ASSERT(mTaskQueue->IsOnCurrentThread());

  FFMPEG_LOG("ProcessEncode");

#if LIBAVCODEC_VERSION_MAJOR < 58
  // TODO(Bug 1868253): implement encode with avcodec_encode_video2().
  MOZ_CRASH("FFmpegDataEncoder needs ffmpeg 58 at least.");
  return EncodePromise::CreateAndReject(NS_ERROR_NOT_IMPLEMENTED, __func__);
#else

  auto rv = EncodeInputWithModernAPIs(std::move(aSample));
  if (rv.isErr()) {
    MediaResult e = rv.unwrapErr();
    FFMPEG_LOG("%s", e.Description().get());
    return EncodePromise::CreateAndReject(e, __func__);
  }

  return EncodePromise::CreateAndResolve(rv.unwrap(), __func__);
#endif
}

RefPtr<MediaDataEncoder::ReconfigurationPromise>
FFmpegDataEncoder<LIBAV_VER>::ProcessReconfigure(
    const RefPtr<const EncoderConfigurationChangeList>& aConfigurationChanges) {
  MOZ_ASSERT(mTaskQueue->IsOnCurrentThread());

  FFMPEG_LOG("ProcessReconfigure");

  bool ok = false;
  for (const auto& confChange : aConfigurationChanges->mChanges) {
    // A reconfiguration on the fly succeeds if all changes can be applied
    // successfuly. In case of failure, the encoder will be drained and
    // recreated.
    ok &= confChange.match(
        // Not supported yet
        [&](const DimensionsChange& aChange) -> bool { return false; },
        [&](const DisplayDimensionsChange& aChange) -> bool { return false; },
        [&](const BitrateModeChange& aChange) -> bool { return false; },
        [&](const BitrateChange& aChange) -> bool {
          // Verified on x264
          if (!strcmp(mCodecContext->codec->name, "libx264")) {
            MOZ_ASSERT(aChange.get().ref() != 0);
            mConfig.mBitrate = aChange.get().ref();
            mCodecContext->bit_rate =
                static_cast<FFmpegBitRate>(mConfig.mBitrate);
            return true;
          }
          return false;
        },
        [&](const FramerateChange& aChange) -> bool { return false; },
        [&](const UsageChange& aChange) -> bool { return false; },
        [&](const ContentHintChange& aChange) -> bool { return false; },
        [&](const SampleRateChange& aChange) -> bool { return false; },
        [&](const NumberOfChannelsChange& aChange) -> bool { return false; });
  };
  using P = MediaDataEncoder::ReconfigurationPromise;
  if (ok) {
    return P::CreateAndResolve(true, __func__);
  }
  return P::CreateAndReject(NS_ERROR_DOM_MEDIA_FATAL_ERR, __func__);
}

RefPtr<MediaDataEncoder::EncodePromise>
FFmpegDataEncoder<LIBAV_VER>::ProcessDrain() {
  MOZ_ASSERT(mTaskQueue->IsOnCurrentThread());

  FFMPEG_LOG("ProcessDrain");

#if LIBAVCODEC_VERSION_MAJOR < 58
  MOZ_CRASH("FFmpegDataEncoder needs ffmpeg 58 at least.");
  return EncodePromise::CreateAndReject(NS_ERROR_NOT_IMPLEMENTED, __func__);
#else
  auto rv = DrainWithModernAPIs();
  if (rv.isErr()) {
    MediaResult e = rv.unwrapErr();
    FFMPEG_LOG("%s", e.Description().get());
    return EncodePromise::CreateAndReject(rv.inspectErr(), __func__);
  }
  return EncodePromise::CreateAndResolve(rv.unwrap(), __func__);
#endif
}

RefPtr<ShutdownPromise> FFmpegDataEncoder<LIBAV_VER>::ProcessShutdown() {
  MOZ_ASSERT(mTaskQueue->IsOnCurrentThread());

  FFMPEG_LOG("ProcessShutdown");

  ShutdownInternal();

  // Don't shut mTaskQueue down since it's owned by others.
  return ShutdownPromise::CreateAndResolve(true, __func__);
}

void FFmpegDataEncoder<LIBAV_VER>::SetContextBitrate() {
  MOZ_ASSERT(mTaskQueue->IsOnCurrentThread());
  MOZ_ASSERT(mCodecContext);

  if (mConfig.mBitrateMode == BitrateMode::Constant) {
    mCodecContext->rc_max_rate = static_cast<FFmpegBitRate>(mConfig.mBitrate);
    mCodecContext->rc_min_rate = static_cast<FFmpegBitRate>(mConfig.mBitrate);
    mCodecContext->bit_rate = static_cast<FFmpegBitRate>(mConfig.mBitrate);
    FFMPEG_LOG("Encoding in CBR: %d", mConfig.mBitrate);
  } else {
    mCodecContext->rc_max_rate = static_cast<FFmpegBitRate>(mConfig.mBitrate);
    mCodecContext->rc_min_rate = 0;
    mCodecContext->bit_rate = static_cast<FFmpegBitRate>(mConfig.mBitrate);
    FFMPEG_LOG("Encoding in VBR: [%d;%d]", (int)mCodecContext->rc_min_rate,
               (int)mCodecContext->rc_max_rate);
  }
}

void FFmpegDataEncoder<LIBAV_VER>::ShutdownInternal() {
  MOZ_ASSERT(mTaskQueue->IsOnCurrentThread());

  FFMPEG_LOG("ShutdownInternal");

  DestroyFrame();

  if (mCodecContext) {
    CloseCodecContext();
    mLib->av_freep(&mCodecContext);
    mCodecContext = nullptr;
  }
}

Result<AVCodecContext*, MediaResult>
FFmpegDataEncoder<LIBAV_VER>::AllocateCodecContext(bool aHardware) {
  AVCodec* codec = aHardware ? FindHardwareEncoder(mLib, mCodecID)
                             : FindSoftwareEncoder(mLib, mCodecID);
  if (!codec) {
    return Err(MediaResult(
        NS_ERROR_DOM_MEDIA_FATAL_ERR,
        RESULT_DETAIL("failed to find ffmpeg encoder for codec id %d",
                      mCodecID)));
  }

  AVCodecContext* ctx = mLib->avcodec_alloc_context3(codec);
  if (!ctx) {
    return Err(MediaResult(
        NS_ERROR_OUT_OF_MEMORY,
        RESULT_DETAIL("failed to allocate ffmpeg context for codec %s",
                      codec->name)));
  }

  MOZ_ASSERT(ctx->codec == codec);

  return ctx;
}

int FFmpegDataEncoder<LIBAV_VER>::OpenCodecContext(const AVCodec* aCodec,
                                                   AVDictionary** aOptions) {
  MOZ_ASSERT(mCodecContext);

  StaticMutexAutoLock mon(sMutex);
  return mLib->avcodec_open2(mCodecContext, aCodec, aOptions);
}

void FFmpegDataEncoder<LIBAV_VER>::CloseCodecContext() {
  MOZ_ASSERT(mCodecContext);

  StaticMutexAutoLock mon(sMutex);
  mLib->avcodec_close(mCodecContext);
}

bool FFmpegDataEncoder<LIBAV_VER>::PrepareFrame() {
  MOZ_ASSERT(mTaskQueue->IsOnCurrentThread());

  // TODO: Merge the duplicate part with FFmpegDataDecoder's PrepareFrame.
#if LIBAVCODEC_VERSION_MAJOR >= 55
  if (mFrame) {
    mLib->av_frame_unref(mFrame);
  } else {
    mFrame = mLib->av_frame_alloc();
  }
#elif LIBAVCODEC_VERSION_MAJOR == 54
  if (mFrame) {
    mLib->avcodec_get_frame_defaults(mFrame);
  } else {
    mFrame = mLib->avcodec_alloc_frame();
  }
#else
  mLib->av_freep(&mFrame);
  mFrame = mLib->avcodec_alloc_frame();
#endif
  return !!mFrame;
}

void FFmpegDataEncoder<LIBAV_VER>::DestroyFrame() {
  MOZ_ASSERT(mTaskQueue->IsOnCurrentThread());
  if (mFrame) {
#if LIBAVCODEC_VERSION_MAJOR >= 55
    mLib->av_frame_unref(mFrame);
    mLib->av_frame_free(&mFrame);
#elif LIBAVCODEC_VERSION_MAJOR == 54
    mLib->avcodec_free_frame(&mFrame);
#else
    mLib->av_freep(&mFrame);
#endif
    mFrame = nullptr;
  }
}

// avcodec_send_frame and avcodec_receive_packet were introduced in version 58.
#if LIBAVCODEC_VERSION_MAJOR >= 58
Result<MediaDataEncoder::EncodedData, MediaResult>
FFmpegDataEncoder<LIBAV_VER>::EncodeWithModernAPIs() {
  // Initialize AVPacket.
  AVPacket* pkt = mLib->av_packet_alloc();

  if (!pkt) {
    return Err(
        MediaResult(NS_ERROR_OUT_OF_MEMORY, "failed to allocate packet"_ns));
  }

  auto freePacket = MakeScopeExit([this, &pkt] { mLib->av_packet_free(&pkt); });

  // Send frame and receive packets.
  if (int ret = mLib->avcodec_send_frame(mCodecContext, mFrame); ret < 0) {
    // In theory, avcodec_send_frame could sent -EAGAIN to signal its internal
    // buffers is full. In practice this can't happen as we only feed one frame
    // at a time, and we immediately call avcodec_receive_packet right after.
    // TODO: Create a NS_ERROR_DOM_MEDIA_ENCODE_ERR in ErrorList.py?
    return Err(MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                           RESULT_DETAIL("avcodec_send_frame error: %s",
                                         MakeErrorString(mLib, ret).get())));
  }

  EncodedData output;
  while (true) {
    int ret = mLib->avcodec_receive_packet(mCodecContext, pkt);
    if (ret == AVERROR(EAGAIN)) {
      // The encoder is asking for more inputs.
      FFMPEG_LOG("encoder is asking for more input!");
      break;
    }

    if (ret < 0) {
      // AVERROR_EOF is returned when the encoder has been fully flushed, but it
      // shouldn't happen here.
      return Err(MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                             RESULT_DETAIL("avcodec_receive_packet error: %s",
                                           MakeErrorString(mLib, ret).get())));
    }

    auto r = ToMediaRawData(pkt);
    mLib->av_packet_unref(pkt);
    if (r.isErr()) {
      MediaResult e = r.unwrapErr();
      FFMPEG_LOG("%s", e.Description().get());
      return Err(e);
    }

    RefPtr<MediaRawData> d = r.unwrap();
    if (!d) {
      // This can happen if e.g. DTX is enabled
      FFMPEG_LOG("No encoded packet output");
      continue;
    }
    output.AppendElement(std::move(d));
  }

  FFMPEG_LOG("Got %zu encoded data", output.Length());
  return std::move(output);
}

Result<MediaDataEncoder::EncodedData, MediaResult>
FFmpegDataEncoder<LIBAV_VER>::DrainWithModernAPIs() {
  MOZ_ASSERT(mTaskQueue->IsOnCurrentThread());
  MOZ_ASSERT(mCodecContext);

  // TODO: Create a common utility to merge the duplicate code below with
  // EncodeWithModernAPIs above.

  // Initialize AVPacket.
  AVPacket* pkt = mLib->av_packet_alloc();
  if (!pkt) {
    return Err(
        MediaResult(NS_ERROR_OUT_OF_MEMORY, "failed to allocate packet"_ns));
  }
  auto freePacket = MakeScopeExit([this, &pkt] { mLib->av_packet_free(&pkt); });

  // Enter draining mode by sending NULL to the avcodec_send_frame(). Note that
  // this can leave the encoder in a permanent EOF state after draining. As a
  // result, the encoder is unable to continue encoding. A new
  // AVCodecContext/encoder creation is required if users need to encode after
  // draining.
  //
  // TODO: Use `avcodec_flush_buffers` to drain the pending packets if
  // AV_CODEC_CAP_ENCODER_FLUSH is set in mCodecContext->codec->capabilities.
  if (int ret = mLib->avcodec_send_frame(mCodecContext, nullptr); ret < 0) {
    if (ret == AVERROR_EOF) {
      // The encoder has been flushed. Drain can be called multiple time.
      FFMPEG_LOG("encoder has been flushed!");
      return EncodedData();
    }

    return Err(MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                           RESULT_DETAIL("avcodec_send_frame error: %s",
                                         MakeErrorString(mLib, ret).get())));
  }

  EncodedData output;
  while (true) {
    int ret = mLib->avcodec_receive_packet(mCodecContext, pkt);
    if (ret == AVERROR_EOF) {
      FFMPEG_LOG("encoder has no more output packet!");
      break;
    }

    if (ret < 0) {
      // avcodec_receive_packet should not result in a -EAGAIN once it's in
      // draining mode.
      return Err(MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                             RESULT_DETAIL("avcodec_receive_packet error: %s",
                                           MakeErrorString(mLib, ret).get())));
    }

    auto r = ToMediaRawData(pkt);
    mLib->av_packet_unref(pkt);
    if (r.isErr()) {
      MediaResult e = r.unwrapErr();
      FFMPEG_LOG("%s", e.Description().get());
      return Err(e);
    }

    RefPtr<MediaRawData> d = r.unwrap();
    if (!d) {
      return Err(
          MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                      "failed to create a MediaRawData from the AVPacket"_ns));
    }
    output.AppendElement(std::move(d));
  }

  FFMPEG_LOG("Encoding successful, %zu packets", output.Length());

  // TODO: Evaluate a better solution (Bug 1869466)
  // TODO: Only re-create AVCodecContext when avcodec_flush_buffers is
  // unavailable.
  ShutdownInternal();
  MediaResult r = InitEncoder();
  if (NS_FAILED(r.Code())) {
    FFMPEG_LOG("%s", r.Description().get());
    return Err(r);
  }

  return std::move(output);
}
#endif  // LIBAVCODEC_VERSION_MAJOR >= 58

}  // namespace mozilla
