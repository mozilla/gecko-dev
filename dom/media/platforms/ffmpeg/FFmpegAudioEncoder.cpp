/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FFmpegAudioEncoder.h"

#include "FFmpegRuntimeLinker.h"
#include "FFmpegLog.h"
#include "FFmpegUtils.h"
#include "MediaData.h"

#include "AudioSegment.h"

namespace mozilla {

FFmpegAudioEncoder<LIBAV_VER>::FFmpegAudioEncoder(
    const FFmpegLibWrapper* aLib, AVCodecID aCodecID,
    const RefPtr<TaskQueue>& aTaskQueue, const EncoderConfig& aConfig)
    : FFmpegDataEncoder(aLib, aCodecID, aTaskQueue, aConfig) {}

RefPtr<MediaDataEncoder::InitPromise> FFmpegAudioEncoder<LIBAV_VER>::Init() {
  FFMPEGA_LOG("Init");
  return InvokeAsync(mTaskQueue, __func__, [self = RefPtr(this)]() {
    MediaResult r = self->InitEncoder();
    if (NS_FAILED(r.Code())) {
      FFMPEGV_LOG("%s", r.Description().get());
      return InitPromise::CreateAndReject(r, __func__);
    }
    return InitPromise::CreateAndResolve(true, __func__);
  });
}

nsCString FFmpegAudioEncoder<LIBAV_VER>::GetDescriptionName() const {
#ifdef USING_MOZFFVPX
  return "ffvpx audio encoder"_ns;
#else
  const char* lib =
#  if defined(MOZ_FFMPEG)
      FFmpegRuntimeLinker::LinkStatusLibraryName();
#  else
      "no library: ffmpeg disabled during build";
#  endif
  return nsPrintfCString("ffmpeg audio encoder (%s)", lib);
#endif
}

void FFmpegAudioEncoder<LIBAV_VER>::ResamplerDestroy::operator()(
    SpeexResamplerState* aResampler) {
  speex_resampler_destroy(aResampler);
}

MediaResult FFmpegAudioEncoder<LIBAV_VER>::InitEncoder() {
  MOZ_ASSERT(mTaskQueue->IsOnCurrentThread());

  ForceEnablingFFmpegDebugLogs();

  FFMPEG_LOG("FFmpegAudioEncoder::InitEncoder");

  // Initialize the common members of the encoder instance
  auto r = AllocateCodecContext(/* aHardware */ false);
  if (r.isErr()) {
    return r.unwrapErr();
  }
  mCodecContext = r.unwrap();
  const AVCodec* codec = mCodecContext->codec;
  mCodecName = codec->name;

#if LIBAVCODEC_VERSION_MAJOR >= 60
  mCodecContext->flags |= AV_CODEC_FLAG_FRAME_DURATION;
#endif

  // Find a compatible input rate for the codec, update the encoder config, and
  // note the rate at which this instance was configured.
  mInputSampleRate = AssertedCast<int>(mConfig.mSampleRate);
  if (codec->supported_samplerates) {
    // Ensure the sample-rate list is sorted, iterate and either find that the
    // sample rate is supported, or pick the same rate just above the audio
    // input sample-rate (as to not lose information). If the audio is higher
    // than the highest supported sample-rate, down-sample to the highest
    // sample-rate supported by the codec. This is the case when encoding high
    // samplerate audio to opus.
    AutoTArray<int, 16> supportedSampleRates;
    IterateZeroTerminated(codec->supported_samplerates,
                          [&supportedSampleRates](int aRate) mutable {
                            supportedSampleRates.AppendElement(aRate);
                          });
    supportedSampleRates.Sort();

    for (const auto& rate : supportedSampleRates) {
      if (mInputSampleRate == rate) {
        mConfig.mSampleRate = rate;
        break;
      }
      if (mInputSampleRate < rate) {
        // This rate is the smallest supported rate above the content's rate.
        mConfig.mSampleRate = rate;
        break;
      }
      if (mInputSampleRate > rate) {
        mConfig.mSampleRate = rate;
      }
    }
  }

  if (mConfig.mSampleRate != AssertedCast<uint32_t>(mInputSampleRate)) {
    // Need to resample to targetRate
    int err;
    SpeexResamplerState* resampler = speex_resampler_init(
        mConfig.mNumberOfChannels, mInputSampleRate, mConfig.mSampleRate,
        SPEEX_RESAMPLER_QUALITY_DEFAULT, &err);
    if (!err) {
      mResampler.reset(resampler);
    } else {
      FFMPEG_LOG(
          "Error creating resampler in FFmpegAudioEncoder %dHz -> %dHz (%dch)",
          mInputSampleRate, mConfig.mSampleRate, mConfig.mNumberOfChannels);
    }
  }

  // And now the audio-specific part
  mCodecContext->sample_rate = AssertedCast<int>(mConfig.mSampleRate);

#if LIBAVCODEC_VERSION_MAJOR >= 60
  // Gecko's ordering intentionnally matches ffmepg's ordering
  mLib->av_channel_layout_default(&mCodecContext->ch_layout,
                                  AssertedCast<int>(mConfig.mNumberOfChannels));
#else
  mCodecContext->channels = AssertedCast<int>(mConfig.mNumberOfChannels);
#endif

  switch (mConfig.mCodec) {
    case CodecType::Opus:
      // When using libopus, ffmpeg supports interleaved float and s16 input.
      mCodecContext->sample_fmt = AV_SAMPLE_FMT_FLT;
      break;
    case CodecType::Vorbis:
      // When using libvorbis, ffmpeg only supports planar f32 input.
      mCodecContext->sample_fmt = AV_SAMPLE_FMT_FLTP;
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("Not supported");
  }

  if (mConfig.mCodec == CodecType::Opus) {
    // Default is VBR
    if (mConfig.mBitrateMode == BitrateMode::Constant) {
      mLib->av_opt_set(mCodecContext->priv_data, "vbr", "off", 0);
    }
    if (mConfig.mCodecSpecific.isSome()) {
      MOZ_ASSERT(mConfig.mCodecSpecific->is<OpusSpecific>());
      const OpusSpecific& specific = mConfig.mCodecSpecific->as<OpusSpecific>();
      // This attribute maps directly to complexity
      mCodecContext->compression_level = specific.mComplexity;
      FFMPEG_LOG("Opus complexity set to %d", specific.mComplexity);
      float frameDurationMs =
          AssertedCast<float>(specific.mFrameDuration) / 1000.f;
      if (mLib->av_opt_set_double(mCodecContext->priv_data, "frame_duration",
                                  frameDurationMs, 0)) {
        return MediaResult(
            NS_ERROR_FAILURE,
            "Error setting the frame duration on Opus encoder"_ns);
      }
      FFMPEG_LOG("Opus frame duration set to %0.2f", frameDurationMs);
      if (specific.mPacketLossPerc) {
        if (mLib->av_opt_set_int(
                mCodecContext->priv_data, "packet_loss",
                AssertedCast<int64_t>(specific.mPacketLossPerc), 0)) {
          return MediaResult(
              NS_ERROR_FAILURE,
              RESULT_DETAIL(
                  "Error setting the packet loss percentage to %" PRIu64
                  " on Opus encoder",
                  specific.mPacketLossPerc));
        }
        FFMPEG_LOGV("Packet loss set to %d%% in Opus encoder",
                    AssertedCast<int>(specific.mPacketLossPerc));
      }
      if (specific.mUseInBandFEC) {
        if (mLib->av_opt_set(mCodecContext->priv_data, "fec", "on", 0)) {
          return MediaResult(
              NS_ERROR_FAILURE,
              RESULT_DETAIL("Error %s FEC on Opus encoder",
                            specific.mUseInBandFEC ? "enabling" : "disabling"));
        }
        FFMPEG_LOGV("In-band FEC enabled for Opus encoder.");
      }
      if (specific.mUseDTX) {
        if (mLib->av_opt_set(mCodecContext->priv_data, "dtx", "on", 0)) {
          return MediaResult(
              NS_ERROR_FAILURE,
              RESULT_DETAIL("Error %s DTX on Opus encoder",
                            specific.mUseDTX ? "enabling" : "disabling"));
        }
        // DTX packets are a TOC byte, and possibly one byte of length, packets
        // 3 bytes and larger are to be returned.
        mDtxThreshold = 3;
      }
      // TODO: format
      // https://bugzilla.mozilla.org/show_bug.cgi?id=1876066
    }
  }
  // Override the time base: always the sample-rate the encoder is running at
  mCodecContext->time_base =
      AVRational{.num = 1, .den = mCodecContext->sample_rate};

#if LIBAVCODEC_VERSION_MAJOR >= 60
  mCodecContext->flags |= AV_CODEC_FLAG_FRAME_DURATION;
#endif

  SetContextBitrate();

  AVDictionary* options = nullptr;
  if (int ret = OpenCodecContext(mCodecContext->codec, &options); ret < 0) {
    return MediaResult(
        NS_ERROR_DOM_MEDIA_FATAL_ERR,
        RESULT_DETAIL("failed to open %s avcodec: %s", mCodecName.get(),
                      MakeErrorString(mLib, ret).get()));
  }
  mLib->av_dict_free(&options);

  FFMPEGA_LOG(
      "%s has been initialized with sample-format: %d, bitrate: %" PRIi64
      ", sample-rate: %d, channels: %d, time_base: %d/%d",
      mCodecName.get(), static_cast<int>(mCodecContext->sample_fmt),
      static_cast<int64_t>(mCodecContext->bit_rate), mCodecContext->sample_rate,
      mConfig.mNumberOfChannels, mCodecContext->time_base.num,
      mCodecContext->time_base.den);

  return NS_OK;
}

// avcodec_send_frame and avcodec_receive_packet were introduced in version 58.
#if LIBAVCODEC_VERSION_MAJOR >= 58

Result<MediaDataEncoder::EncodedData, MediaResult>
FFmpegAudioEncoder<LIBAV_VER>::EncodeOnePacket(Span<float> aSamples,
                                               media::TimeUnit aPts) {
  MOZ_ASSERT(mTaskQueue->IsOnCurrentThread());
  MOZ_ASSERT(aSamples.Length() % mConfig.mNumberOfChannels == 0);

  // Allocate AVFrame.
  if (!PrepareFrame()) {
    return Err(
        MediaResult(NS_ERROR_OUT_OF_MEMORY, "failed to allocate frame"_ns));
  }

  uint32_t frameCount = aSamples.Length() / mConfig.mNumberOfChannels;

  // This method assumes that the audio has been packetized appropriately --
  // packets smaller than the packet size are allowed when draining.
  MOZ_ASSERT(AssertedCast<int>(frameCount) <= mCodecContext->frame_size);

  ChannelCount(mFrame) = AssertedCast<int>(mConfig.mNumberOfChannels);

#  if LIBAVCODEC_VERSION_MAJOR >= 60
  int rv = mLib->av_channel_layout_copy(&mFrame->ch_layout,
                                        &mCodecContext->ch_layout);
  if (rv < 0) {
    return Err(MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                           RESULT_DETAIL("channel layout copy error: %s",
                                         MakeErrorString(mLib, rv).get())));
  }
#  endif

  mFrame->sample_rate = AssertedCast<int>(mConfig.mSampleRate);
  // Not a mistake, nb_samples is per channel in ffmpeg
  mFrame->nb_samples = AssertedCast<int>(frameCount);
  // Audio is converted below if needed
  mFrame->format = mCodecContext->sample_fmt;
  // Set presentation timestamp and duration of the AVFrame.
#  if LIBAVCODEC_VERSION_MAJOR >= 59
  mFrame->time_base =
      AVRational{.num = 1, .den = static_cast<int>(mConfig.mSampleRate)};
#  endif
  mFrame->pts = aPts.ToTicksAtRate(mConfig.mSampleRate);
#  if LIBAVCODEC_VERSION_MAJOR >= 60
  mFrame->duration = frameCount;
#  else
  mFrame->pkt_duration = frameCount;
#  endif

  if (int ret = mLib->av_frame_get_buffer(mFrame, 16); ret < 0) {
    return Err(MediaResult(NS_ERROR_OUT_OF_MEMORY,
                           RESULT_DETAIL("failed to allocate frame data: %s",
                                         MakeErrorString(mLib, ret).get())));
  }

  // Make sure AVFrame is writable.
  if (int ret = mLib->av_frame_make_writable(mFrame); ret < 0) {
    return Err(MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                           RESULT_DETAIL("failed to make frame writable: %s",
                                         MakeErrorString(mLib, ret).get())));
  }

  // The input is always in f32 interleaved for now
  if (mCodecContext->sample_fmt == AV_SAMPLE_FMT_FLT) {
    PodCopy(reinterpret_cast<float*>(mFrame->data[0]), aSamples.data(),
            aSamples.Length());
  } else {
    MOZ_ASSERT(mCodecContext->sample_fmt == AV_SAMPLE_FMT_FLTP);
    for (uint32_t i = 0; i < mConfig.mNumberOfChannels; i++) {
      DeinterleaveAndConvertBuffer(aSamples.data(), mFrame->nb_samples,
                                   mConfig.mNumberOfChannels, mFrame->data);
    }
  }

  // Now send the AVFrame to ffmpeg for encoding, same code for audio and video.
  return FFmpegDataEncoder<LIBAV_VER>::EncodeWithModernAPIs();
}

Result<MediaDataEncoder::EncodedData, MediaResult> FFmpegAudioEncoder<
    LIBAV_VER>::EncodeInputWithModernAPIs(RefPtr<const MediaData> aSample) {
  MOZ_ASSERT(mTaskQueue->IsOnCurrentThread());
  MOZ_ASSERT(mCodecContext);
  MOZ_ASSERT(aSample);

  RefPtr<const AudioData> sample(aSample->As<AudioData>());

  FFMPEG_LOG("Encoding %" PRIu32 " frames of audio at pts: %s",
             sample->Frames(), sample->mTime.ToString().get());

  if ((!mResampler && sample->mRate != mConfig.mSampleRate) ||
      (mResampler &&
       sample->mRate != AssertedCast<uint32_t>(mInputSampleRate)) ||
      sample->mChannels != mConfig.mNumberOfChannels) {
    return Err(MediaResult(NS_ERROR_DOM_ENCODING_NOT_SUPPORTED_ERR,
                           "Rate or sample-rate at the input of the encoder "
                           "different from what has been configured "
                           "initially"_ns));
  }

  // ffmpeg expects exactly sized input audio packets most of the time.
  // Packetization is performed if needed, and audio packets of the correct size
  // are fed to ffmpeg, with timestamps extrapolated the timestamp found on
  // the input MediaData.

  if (!mPacketizer) {
    media::TimeUnit basePts = media::TimeUnit::Zero(mConfig.mSampleRate);
    basePts += sample->mTime;
    mPacketizer.emplace(mCodecContext->frame_size, sample->mChannels,
                        basePts.ToTicksAtRate(mConfig.mSampleRate),
                        mConfig.mSampleRate);
  }

  if (!mFirstPacketPts.IsValid()) {
    mFirstPacketPts = sample->mTime;
  }

  Span<float> audio = sample->Data();

  if (mResampler) {
    // Ensure that all input frames are consumed each time by oversizing the
    // output buffer.
    int bufferLengthGuess = std::ceil(2. * static_cast<float>(audio.size()) *
                                      mConfig.mSampleRate / mInputSampleRate);
    mTempBuffer.SetLength(bufferLengthGuess);
    uint32_t inputFrames = audio.size() / mConfig.mNumberOfChannels;
    uint32_t inputFramesProcessed = inputFrames;
    uint32_t outputFrames = bufferLengthGuess / mConfig.mNumberOfChannels;
    DebugOnly<int> rv = speex_resampler_process_interleaved_float(
        mResampler.get(), audio.data(), &inputFramesProcessed,
        mTempBuffer.Elements(), &outputFrames);
    audio = Span<float>(mTempBuffer.Elements(),
                        outputFrames * mConfig.mNumberOfChannels);
    MOZ_ASSERT(inputFrames == inputFramesProcessed,
               "increate the buffer to consume all input each time");
    MOZ_ASSERT(rv == RESAMPLER_ERR_SUCCESS);
  }

  EncodedData output;
  MediaResult rv = NS_OK;

  mPacketizer->Input(audio.data(), audio.Length() / mConfig.mNumberOfChannels);

  // Dequeue and encode each packet
  while (mPacketizer->PacketsAvailable() && rv.Code() == NS_OK) {
    mTempBuffer.SetLength(mCodecContext->frame_size *
                          mConfig.mNumberOfChannels);
    media::TimeUnit pts = mPacketizer->Output(mTempBuffer.Elements());
    auto audio = Span(mTempBuffer.Elements(), mTempBuffer.Length());
    FFMPEG_LOG("Encoding %" PRIu32 " frames, pts: %s",
               mPacketizer->PacketSize(), pts.ToString().get());
    auto encodeResult = EncodeOnePacket(audio, pts);
    if (encodeResult.isOk()) {
      output.AppendElements(std::move(encodeResult.unwrap()));
    } else {
      return encodeResult;
    }
    pts += media::TimeUnit(mPacketizer->PacketSize(), mConfig.mSampleRate);
  }
  return std::move(output);
}

Result<MediaDataEncoder::EncodedData, MediaResult>
FFmpegAudioEncoder<LIBAV_VER>::DrainWithModernAPIs() {
  MOZ_ASSERT(mTaskQueue->IsOnCurrentThread());

  // If there's no packetizer, or it's empty, we can proceed immediately.
  if (!mPacketizer || mPacketizer->FramesAvailable() == 0) {
    return FFmpegDataEncoder<LIBAV_VER>::DrainWithModernAPIs();
  }
  EncodedData output;
  MediaResult rv = NS_OK;
  // Dequeue and encode each packet
  mTempBuffer.SetLength(mCodecContext->frame_size *
                        mPacketizer->ChannelCount());
  uint32_t written;
  media::TimeUnit pts = mPacketizer->Drain(mTempBuffer.Elements(), written);
  auto audio =
      Span(mTempBuffer.Elements(), written * mPacketizer->ChannelCount());
  auto encodeResult = EncodeOnePacket(audio, pts);
  if (encodeResult.isOk()) {
    auto array = encodeResult.unwrap();
    output.AppendElements(std::move(array));
  } else {
    return encodeResult;
  }
  // Now, drain the encoder
  auto drainResult = FFmpegDataEncoder<LIBAV_VER>::DrainWithModernAPIs();
  if (drainResult.isOk()) {
    auto array = drainResult.unwrap();
    output.AppendElements(std::move(array));
  } else {
    return drainResult;
  }
  return std::move(output);
}
#endif  // if LIBAVCODEC_VERSION_MAJOR >= 58

Result<RefPtr<MediaRawData>, MediaResult>
FFmpegAudioEncoder<LIBAV_VER>::ToMediaRawData(AVPacket* aPacket) {
  MOZ_ASSERT(mTaskQueue->IsOnCurrentThread());
  MOZ_ASSERT(aPacket);

  if (aPacket->size < mDtxThreshold) {
    FFMPEG_LOG(
        "DTX enabled and packet is %d bytes (threshold %d), not returning.",
        aPacket->size, mDtxThreshold);
    return RefPtr<MediaRawData>(nullptr);
  }

  auto creationResult = CreateMediaRawData(aPacket);
  if (creationResult.isErr()) {
    return Err(creationResult.unwrapErr());
  }

  RefPtr<MediaRawData> data = creationResult.unwrap();

  data->mKeyframe = (aPacket->flags & AV_PKT_FLAG_KEY) != 0;

  if (auto extradataResult = GetExtraData(aPacket); extradataResult.isOk()) {
    data->mExtraData = extradataResult.unwrap();
  }

  data->mTime = media::TimeUnit(aPacket->pts, mConfig.mSampleRate);
  data->mTimecode = data->mTime;
  data->mDuration =
      media::TimeUnit(mCodecContext->frame_size, mConfig.mSampleRate);

  // Handle encoder delay
  // Tracked in https://github.com/w3c/webcodecs/issues/626 because not quite
  // specced yet.
  if (mFirstPacketPts > data->mTime) {
    data->mOriginalPresentationWindow =
        Some(media::TimeInterval{data->mTime, data->GetEndTime()});
    // Duration is likely to be ajusted when the above spec issue is fixed. For
    // now, leave it as-is
    //  data->mDuration -= (mFirstPacketPts - data->mTime);
    // if (data->mDuration.IsNegative()) {
    //   data->mDuration = media::TimeUnit::Zero();
    // }
    data->mTime = mFirstPacketPts;
  }

  if (mPacketsDelivered++ == 0) {
    // Attach the config (including any channel / samplerate modification to fit
    // the encoder requirements), if needed.
    data->mConfig = MakeUnique<EncoderConfig>(mConfig);
  }

  if (data->mExtraData) {
    FFMPEGA_LOG(
        "FFmpegAudioEncoder out: [%s,%s] (%zu bytes, extradata %zu bytes)",
        data->mTime.ToString().get(), data->mDuration.ToString().get(),
        data->Size(), data->mExtraData->Length());
  } else {
    FFMPEGA_LOG("FFmpegAudioEncoder out: [%s,%s] (%zu bytes)",
                data->mTime.ToString().get(), data->mDuration.ToString().get(),
                data->Size());
  }

  return data;
}

Result<already_AddRefed<MediaByteBuffer>, MediaResult>
FFmpegAudioEncoder<LIBAV_VER>::GetExtraData(AVPacket* /* aPacket */) {
  MOZ_ASSERT(mTaskQueue->IsOnCurrentThread());

  if (!mCodecContext->extradata_size) {
    return Err(MediaResult(NS_ERROR_NOT_AVAILABLE, "no extradata"_ns));
  }
  // Create extra data -- they are on the context.
  auto extraData = MakeRefPtr<MediaByteBuffer>();
  extraData->SetLength(mCodecContext->extradata_size);
  MOZ_ASSERT(extraData);
  PodCopy(extraData->Elements(), mCodecContext->extradata,
          mCodecContext->extradata_size);
  return extraData.forget();
}

}  // namespace mozilla
