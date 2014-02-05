/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "OpusTrackEncoder.h"
#include "nsString.h"

#include <opus/opus.h>

#undef LOG
#ifdef MOZ_WIDGET_GONK
#include <android/log.h>
#define LOG(args...) __android_log_print(ANDROID_LOG_INFO, "MediaEncoder", ## args);
#else
#define LOG(args, ...)
#endif

namespace mozilla {

// The Opus format supports up to 8 channels, and supports multitrack audio up
// to 255 channels, but the current implementation supports only mono and
// stereo, and downmixes any more than that.
static const int MAX_SUPPORTED_AUDIO_CHANNELS = 8;

// http://www.opus-codec.org/docs/html_api-1.0.2/group__opus__encoder.html
// In section "opus_encoder_init", channels must be 1 or 2 of input signal.
static const int MAX_CHANNELS = 2;

// A maximum data bytes for Opus to encode.
static const int MAX_DATA_BYTES = 4096;

// http://tools.ietf.org/html/draft-ietf-codec-oggopus-00#section-4
// Second paragraph, " The granule position of an audio data page is in units
// of PCM audio samples at a fixed rate of 48 kHz."
static const int kOpusSamplingRate = 48000;

// The duration of an Opus frame, and it must be 2.5, 5, 10, 20, 40 or 60 ms.
static const int kFrameDurationMs  = 20;

// The supported sampling rate of input signal (Hz),
// must be one of the following. Will resampled to 48kHz otherwise.
static const int kOpusSupportedInputSamplingRates[5] =
                   {8000, 12000, 16000, 24000, 48000};

namespace {

// An endian-neutral serialization of integers. Serializing T in little endian
// format to aOutput, where T is a 16 bits or 32 bits integer.
template<typename T>
static void
SerializeToBuffer(T aValue, nsTArray<uint8_t>* aOutput)
{
  for (uint32_t i = 0; i < sizeof(T); i++) {
    aOutput->AppendElement((uint8_t)(0x000000ff & (aValue >> (i * 8))));
  }
}

static inline void
SerializeToBuffer(const nsCString& aComment, nsTArray<uint8_t>* aOutput)
{
  // Format of serializing a string to buffer is, the length of string (32 bits,
  // little endian), and the string.
  SerializeToBuffer((uint32_t)(aComment.Length()), aOutput);
  aOutput->AppendElements(aComment.get(), aComment.Length());
}


static void
SerializeOpusIdHeader(uint8_t aChannelCount, uint16_t aPreskip,
                      uint32_t aInputSampleRate, nsTArray<uint8_t>* aOutput)
{
  // The magic signature, null terminator has to be stripped off from strings.
  static const uint8_t magic[9] = "OpusHead";
  memcpy(aOutput->AppendElements(sizeof(magic) - 1), magic, sizeof(magic) - 1);

  // The version, must always be 1 (8 bits, unsigned).
  aOutput->AppendElement(1);

  // Number of output channels (8 bits, unsigned).
  aOutput->AppendElement(aChannelCount);

  // Number of samples (at 48 kHz) to discard from the decoder output when
  // starting playback (16 bits, unsigned, little endian).
  SerializeToBuffer(aPreskip, aOutput);

  // The sampling rate of input source (32 bits, unsigned, little endian).
  SerializeToBuffer(aInputSampleRate, aOutput);

  // Output gain, an encoder should set this field to zero (16 bits, signed,
  // little endian).
  SerializeToBuffer((int16_t)0, aOutput);

  // Channel mapping family. Family 0 allows only 1 or 2 channels (8 bits,
  // unsigned).
  aOutput->AppendElement(0);
}

static void
SerializeOpusCommentHeader(const nsCString& aVendor,
                           const nsTArray<nsCString>& aComments,
                           nsTArray<uint8_t>* aOutput)
{
  // The magic signature, null terminator has to be stripped off.
  static const uint8_t magic[9] = "OpusTags";
  memcpy(aOutput->AppendElements(sizeof(magic) - 1), magic, sizeof(magic) - 1);

  // The vendor; Should append in the following order:
  // vendor string length (32 bits, unsigned, little endian)
  // vendor string.
  SerializeToBuffer(aVendor, aOutput);

  // Add comments; Should append in the following order:
  // comment list length (32 bits, unsigned, little endian)
  // comment #0 string length (32 bits, unsigned, little endian)
  // comment #0 string
  // comment #1 string length (32 bits, unsigned, little endian)
  // comment #1 string ...
  SerializeToBuffer((uint32_t)aComments.Length(), aOutput);
  for (uint32_t i = 0; i < aComments.Length(); ++i) {
    SerializeToBuffer(aComments[i], aOutput);
  }
}

}  // Anonymous namespace.

OpusTrackEncoder::OpusTrackEncoder()
  : AudioTrackEncoder()
  , mEncoder(nullptr)
  , mLookahead(0)
  , mResampler(nullptr)
{
}

OpusTrackEncoder::~OpusTrackEncoder()
{
  if (mEncoder) {
    opus_encoder_destroy(mEncoder);
  }
  if (mResampler) {
    speex_resampler_destroy(mResampler);
    mResampler = nullptr;
  }
}

nsresult
OpusTrackEncoder::Init(int aChannels, int aSamplingRate)
{
  // This monitor is used to wake up other methods that are waiting for encoder
  // to be completely initialized.
  ReentrantMonitorAutoEnter mon(mReentrantMonitor);

  NS_ENSURE_TRUE((aChannels <= MAX_SUPPORTED_AUDIO_CHANNELS) && (aChannels > 0),
                 NS_ERROR_FAILURE);

  // This version of encoder API only support 1 or 2 channels,
  // So set the mChannels less or equal 2 and
  // let InterleaveTrackData downmix pcm data.
  mChannels = aChannels > MAX_CHANNELS ? MAX_CHANNELS : aChannels;

  // According to www.opus-codec.org, creating an opus encoder requires the
  // sampling rate of source signal be one of 8000, 12000, 16000, 24000, or
  // 48000. If this constraint is not satisfied, we resample the input to 48kHz.
  nsTArray<int> supportedSamplingRates;
  supportedSamplingRates.AppendElements(kOpusSupportedInputSamplingRates,
                         MOZ_ARRAY_LENGTH(kOpusSupportedInputSamplingRates));
  if (!supportedSamplingRates.Contains(aSamplingRate)) {
    int error;
    mResampler = speex_resampler_init(mChannels,
                                      aSamplingRate,
                                      kOpusSamplingRate,
                                      SPEEX_RESAMPLER_QUALITY_DEFAULT,
                                      &error);

    if (error != RESAMPLER_ERR_SUCCESS) {
      return NS_ERROR_FAILURE;
    }
  }
  mSamplingRate = aSamplingRate;
  NS_ENSURE_TRUE(mSamplingRate > 0, NS_ERROR_FAILURE);

  int error = 0;
  mEncoder = opus_encoder_create(GetOutputSampleRate(), mChannels,
                                 OPUS_APPLICATION_AUDIO, &error);

  mInitialized = (error == OPUS_OK);

  mReentrantMonitor.NotifyAll();

  return error == OPUS_OK ? NS_OK : NS_ERROR_FAILURE;
}

int
OpusTrackEncoder::GetOutputSampleRate()
{
  return mResampler ? kOpusSamplingRate : mSamplingRate;
}

int
OpusTrackEncoder::GetPacketDuration()
{
  return GetOutputSampleRate() * kFrameDurationMs / 1000;
}

already_AddRefed<TrackMetadataBase>
OpusTrackEncoder::GetMetadata()
{
  {
    // Wait if mEncoder is not initialized.
    ReentrantMonitorAutoEnter mon(mReentrantMonitor);
    while (!mCanceled && !mInitialized) {
      mReentrantMonitor.Wait();
    }
  }

  if (mCanceled || mEncodingComplete) {
    return nullptr;
  }

  nsRefPtr<OpusMetadata> meta = new OpusMetadata();

  mLookahead = 0;
  int error = opus_encoder_ctl(mEncoder, OPUS_GET_LOOKAHEAD(&mLookahead));
  if (error != OPUS_OK) {
    mLookahead = 0;
  }

  // The ogg time stamping and pre-skip is always timed at 48000.
  SerializeOpusIdHeader(mChannels, mLookahead * (kOpusSamplingRate /
                        GetOutputSampleRate()), mSamplingRate,
                        &meta->mIdHeader);

  nsCString vendor;
  vendor.AppendASCII(opus_get_version_string());

  nsTArray<nsCString> comments;
  comments.AppendElement(NS_LITERAL_CSTRING("ENCODER=Mozilla" MOZ_APP_UA_VERSION));

  SerializeOpusCommentHeader(vendor, comments,
                             &meta->mCommentHeader);

  return meta.forget();
}

nsresult
OpusTrackEncoder::GetEncodedTrack(EncodedFrameContainer& aData)
{
  {
    // Move all the samples from mRawSegment to mSourceSegment. We only hold
    // the monitor in this block.
    ReentrantMonitorAutoEnter mon(mReentrantMonitor);

    // Wait if mEncoder is not initialized, or when not enough raw data, but is
    // not the end of stream nor is being canceled.
    while (!mCanceled && (!mInitialized || (mRawSegment.GetDuration() +
           mSourceSegment.GetDuration() < GetPacketDuration() &&
           !mEndOfStream))) {
      mReentrantMonitor.Wait();
    }

    if (mCanceled || mEncodingComplete) {
      return NS_ERROR_FAILURE;
    }

    mSourceSegment.AppendFrom(&mRawSegment);

    // Pad |mLookahead| samples to the end of source stream to prevent lost of
    // original data, the pcm duration will be calculated at rate 48K later.
    if (mEndOfStream && !mEosSetInEncoder) {
      mEosSetInEncoder = true;
      mSourceSegment.AppendNullData(mLookahead);
    }
  }

  // Start encoding data.
  nsAutoTArray<AudioDataValue, 9600> pcm;
  pcm.SetLength(GetPacketDuration() * mChannels);
  AudioSegment::ChunkIterator iter(mSourceSegment);
  int frameCopied = 0;
  while (!iter.IsEnded() && frameCopied < GetPacketDuration()) {
    AudioChunk chunk = *iter;

    // Chunk to the required frame size.
    int frameToCopy = chunk.GetDuration();
    if (frameCopied + frameToCopy > GetPacketDuration()) {
      frameToCopy = GetPacketDuration() - frameCopied;
    }

    if (!chunk.IsNull()) {
      // Append the interleaved data to the end of pcm buffer.
      AudioTrackEncoder::InterleaveTrackData(chunk, frameToCopy, mChannels,
        pcm.Elements() + frameCopied * mChannels);
    } else {
      memset(pcm.Elements() + frameCopied * mChannels, 0,
             frameToCopy * mChannels * sizeof(AudioDataValue));
    }

    frameCopied += frameToCopy;
    iter.Next();
  }

  nsRefPtr<EncodedFrame> audiodata = new EncodedFrame();
  audiodata->SetFrameType(EncodedFrame::AUDIO_FRAME);
  if (mResampler) {
    nsAutoTArray<AudioDataValue, 9600> resamplingDest;
    // We want to consume all the input data, so we slightly oversize the
    // resampled data buffer so we can fit the output data in. We cannot really
    // predict the output frame count at each call.
    uint32_t outframes = frameCopied * kOpusSamplingRate / mSamplingRate + 1;
    uint32_t inframes = frameCopied;

    resamplingDest.SetLength(outframes * mChannels);

#if MOZ_SAMPLE_TYPE_S16
    short* in = reinterpret_cast<short*>(pcm.Elements());
    short* out = reinterpret_cast<short*>(resamplingDest.Elements());
    speex_resampler_process_interleaved_int(mResampler, in, &inframes,
                                                        out, &outframes);
#else
    float* in = reinterpret_cast<float*>(pcm.Elements());
    float* out = reinterpret_cast<float*>(resamplingDest.Elements());
    speex_resampler_process_interleaved_float(mResampler, in, &inframes,
                                                          out, &outframes);
#endif

    pcm = resamplingDest;
    // This is always at 48000Hz.
    audiodata->SetDuration(outframes);
  } else {
    // The ogg time stamping and pre-skip is always timed at 48000.
    audiodata->SetDuration(frameCopied * (kOpusSamplingRate / mSamplingRate));
  }

  // Remove the raw data which has been pulled to pcm buffer.
  // The value of frameCopied should equal to (or smaller than, if eos)
  // GetPacketDuration().
  mSourceSegment.RemoveLeading(frameCopied);

  // Has reached the end of input stream and all queued data has pulled for
  // encoding.
  if (mSourceSegment.GetDuration() == 0 && mEndOfStream) {
    mEncodingComplete = true;
    LOG("[Opus] Done encoding.");
  }

  // Append null data to pcm buffer if the leftover data is not enough for
  // opus encoder.
  if (frameCopied < GetPacketDuration() && mEndOfStream) {
    memset(pcm.Elements() + frameCopied * mChannels, 0,
           (GetPacketDuration()-frameCopied)*mChannels*sizeof(AudioDataValue));
  }
  nsTArray<uint8_t> frameData;
  // Encode the data with Opus Encoder.
  frameData.SetLength(MAX_DATA_BYTES);
  // result is returned as opus error code if it is negative.
  int result = 0;
#ifdef MOZ_SAMPLE_TYPE_S16
  const opus_int16* pcmBuf = static_cast<opus_int16*>(pcm.Elements());
  result = opus_encode(mEncoder, pcmBuf, GetPacketDuration(),
                       frameData.Elements(), MAX_DATA_BYTES);
#else
  const float* pcmBuf = static_cast<float*>(pcm.Elements());
  result = opus_encode_float(mEncoder, pcmBuf, GetPacketDuration(),
                             frameData.Elements(), MAX_DATA_BYTES);
#endif
  frameData.SetLength(result >= 0 ? result : 0);

  if (result < 0) {
    LOG("[Opus] Fail to encode data! Result: %s.", opus_strerror(result));
  }
  if (mEncodingComplete) {
    if (mResampler) {
      speex_resampler_destroy(mResampler);
      mResampler = nullptr;
    }
  }

  audiodata->SwapInFrameData(frameData);
  aData.AppendEncodedFrame(audiodata);
  return result >= 0 ? NS_OK : NS_ERROR_FAILURE;
}

}
