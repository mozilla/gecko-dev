/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <AudioToolbox/AudioToolbox.h>
#include "AppleUtils.h"
#include "MP4Reader.h"
#include "MP4Decoder.h"
#include "mozilla/RefPtr.h"
#include "mozilla/ReentrantMonitor.h"
#include "mp4_demuxer/DecoderData.h"
#include "nsIThread.h"
#include "AppleATDecoder.h"
#include "prlog.h"

#ifdef PR_LOGGING
PRLogModuleInfo* GetDemuxerLog();
#define LOG(...) PR_LOG(GetDemuxerLog(), PR_LOG_DEBUG, (__VA_ARGS__))
#else
#define LOG(...)
#endif

namespace mozilla {

AppleATDecoder::AppleATDecoder(const mp4_demuxer::AudioDecoderConfig& aConfig,
                               MediaTaskQueue* anAudioTaskQueue,
                               MediaDataDecoderCallback* aCallback)
  : mConfig(aConfig)
  , mTaskQueue(anAudioTaskQueue)
  , mCallback(aCallback)
  , mConverter(nullptr)
  , mStream(nullptr)
  , mCurrentAudioFrame(0)
  , mSamplePosition(0)
  , mHaveOutput(false)
{
  MOZ_COUNT_CTOR(AppleATDecoder);
  LOG("Creating Apple AudioToolbox AAC decoder");
  LOG("Audio Decoder configuration: %s %d Hz %d channels %d bits per channel",
      mConfig.mime_type,
      mConfig.samples_per_second,
      mConfig.channel_count,
      mConfig.bits_per_sample);
  // TODO: Verify aConfig.mime_type.
}

AppleATDecoder::~AppleATDecoder()
{
  MOZ_COUNT_DTOR(AppleATDecoer);
  MOZ_ASSERT(!mConverter);
  MOZ_ASSERT(!mStream);
}

static void
_MetadataCallback(void *aDecoder,
                  AudioFileStreamID aStream,
                  AudioFileStreamPropertyID aProperty,
                  UInt32 *aFlags)
{
  LOG("AppleATDecoder metadata callback");
  AppleATDecoder* decoder = static_cast<AppleATDecoder*>(aDecoder);
  decoder->MetadataCallback(aStream, aProperty, aFlags);
}

static void
_SampleCallback(void *aDecoder,
                UInt32 aNumBytes, UInt32 aNumPackets,
                const void *aData,
                AudioStreamPacketDescription *aPackets)
{
  LOG("AppleATDecoder sample callback %u bytes %u packets",
      aNumBytes, aNumPackets);
  AppleATDecoder* decoder = static_cast<AppleATDecoder*>(aDecoder);
  decoder->SampleCallback(aNumBytes, aNumPackets, aData, aPackets);
}

nsresult
AppleATDecoder::Init()
{
  LOG("Initializing Apple AudioToolbox AAC decoder");
  AudioFileTypeID fileType = kAudioFileAAC_ADTSType;
  OSStatus rv = AudioFileStreamOpen(this,
                                    _MetadataCallback,
                                    _SampleCallback,
                                    fileType,
                                    &mStream);
  if (rv) {
    NS_ERROR("Couldn't open AudioFileStream");
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

nsresult
AppleATDecoder::Input(mp4_demuxer::MP4Sample* aSample)
{
  LOG("mp4 input sample %p %lld us %lld pts%s %llu bytes audio",
      aSample,
      aSample->duration,
      aSample->composition_timestamp,
      aSample->is_sync_point ? " keyframe" : "",
      (unsigned long long)aSample->size);

  // Queue a task to perform the actual decoding on a separate thread.
  mTaskQueue->Dispatch(
      NS_NewRunnableMethodWithArg<nsAutoPtr<mp4_demuxer::MP4Sample>>(
        this,
        &AppleATDecoder::SubmitSample,
        nsAutoPtr<mp4_demuxer::MP4Sample>(aSample)));

  return NS_OK;
}

nsresult
AppleATDecoder::Flush()
{
  LOG("Flushing AudioToolbox AAC decoder");
  OSStatus rv = AudioConverterReset(mConverter);
  if (rv) {
    LOG("Error %d resetting AudioConverter", rv);
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

nsresult
AppleATDecoder::Drain()
{
  LOG("Draining AudioToolbox AAC decoder");
  mTaskQueue->AwaitIdle();
  mCallback->DrainComplete();
  return Flush();
}

nsresult
AppleATDecoder::Shutdown()
{
  LOG("Shutdown: Apple AudioToolbox AAC decoder");
  OSStatus rv1 = AudioConverterDispose(mConverter);
  if (rv1) {
    LOG("error %d disposing of AudioConverter", rv1);
  } else {
    mConverter = nullptr;
  }

  OSStatus rv2 = AudioFileStreamClose(mStream);
  if (rv2) {
    LOG("error %d closing AudioFileStream", rv2);
  } else {
    mStream = nullptr;
  }

  return (rv1 && rv2) ? NS_OK : NS_ERROR_FAILURE;
}

void
AppleATDecoder::MetadataCallback(AudioFileStreamID aFileStream,
                                 AudioFileStreamPropertyID aPropertyID,
                                 UInt32* aFlags)
{
  if (aPropertyID == kAudioFileStreamProperty_ReadyToProducePackets) {
    SetupDecoder();
  }
}

struct PassthroughUserData {
  AppleATDecoder* mDecoder;
  UInt32 mNumPackets;
  UInt32 mDataSize;
  const void *mData;
  AudioStreamPacketDescription *mPacketDesc;
  bool mDone;
};

// Error value we pass through the decoder to signal that nothing
// has gone wrong during decoding, but more data is needed.
const uint32_t kNeedMoreData = 'MOAR';

static OSStatus
_PassthroughInputDataCallback(AudioConverterRef aAudioConverter,
                              UInt32 *aNumDataPackets /* in/out */,
                              AudioBufferList *aData /* in/out */,
                              AudioStreamPacketDescription **aPacketDesc,
                              void *aUserData)
{
  PassthroughUserData *userData = (PassthroughUserData *)aUserData;
  if (userData->mDone) {
    // We make sure this callback is run _once_, with all the data we received
    // from |AudioFileStreamParseBytes|. When we return an error, the decoder
    // simply passes the return value on to the calling method,
    // |SampleCallback|; and flushes all of the audio frames it had
    // buffered. It does not change the decoder's state.
    LOG("requested too much data; returning\n");
    *aNumDataPackets = 0;
    return kNeedMoreData;
  }

  userData->mDone = true;

  LOG("AudioConverter wants %u packets of audio data\n", *aNumDataPackets);

  *aNumDataPackets = userData->mNumPackets;
  *aPacketDesc = userData->mPacketDesc;

  aData->mBuffers[0].mNumberChannels = userData->mDecoder->mConfig.channel_count;
  aData->mBuffers[0].mDataByteSize = userData->mDataSize;
  aData->mBuffers[0].mData = const_cast<void *>(userData->mData);

  return noErr;
}

void
AppleATDecoder::SampleCallback(uint32_t aNumBytes,
                               uint32_t aNumPackets,
                               const void* aData,
                               AudioStreamPacketDescription* aPackets)
{
  // Pick a multiple of the frame size close to a power of two
  // for efficient allocation.
  const uint32_t MAX_AUDIO_FRAMES = 128;
  const uint32_t decodedSize = MAX_AUDIO_FRAMES * mConfig.channel_count *
    sizeof(AudioDataValue);

  // Descriptions for _decompressed_ audio packets. ignored.
  nsAutoArrayPtr<AudioStreamPacketDescription>
      packets(new AudioStreamPacketDescription[MAX_AUDIO_FRAMES]);

  // This API insists on having packets spoon-fed to it from a callback.
  // This structure exists only to pass our state and the result of the
  // parser on to the callback above.
  PassthroughUserData userData =
      { this, aNumPackets, aNumBytes, aData, aPackets, false };

  do {
    // Decompressed audio buffer
    nsAutoArrayPtr<uint8_t> decoded(new uint8_t[decodedSize]);

    AudioBufferList decBuffer;
    decBuffer.mNumberBuffers = 1;
    decBuffer.mBuffers[0].mNumberChannels = mConfig.channel_count;
    decBuffer.mBuffers[0].mDataByteSize = decodedSize;
    decBuffer.mBuffers[0].mData = decoded.get();

    // in: the max number of packets we can handle from the decoder.
    // out: the number of packets the decoder is actually returning.
    UInt32 numFrames = MAX_AUDIO_FRAMES;

    OSStatus rv = AudioConverterFillComplexBuffer(mConverter,
                                                  _PassthroughInputDataCallback,
                                                  &userData,
                                                  &numFrames /* in/out */,
                                                  &decBuffer,
                                                  packets.get());

    if (rv && rv != kNeedMoreData) {
      LOG("Error decoding audio stream: %#x\n", rv);
      mCallback->Error();
      break;
    }
    LOG("%d frames decoded", numFrames);

    // If we decoded zero frames then AudioConverterFillComplexBuffer is out
    // of data to provide.  We drained its internal buffer completely on the
    // last pass.
    if (numFrames == 0 && rv == kNeedMoreData) {
      LOG("FillComplexBuffer out of data exactly\n");
      mCallback->InputExhausted();
      break;
    }

    const int rate = mConfig.samples_per_second;
    int64_t time = FramesToUsecs(mCurrentAudioFrame, rate).value();
    int64_t duration = FramesToUsecs(numFrames, rate).value();

    LOG("pushed audio at time %lfs; duration %lfs\n",
        (double)time / USECS_PER_S, (double)duration / USECS_PER_S);

    AudioData *audio = new AudioData(mSamplePosition,
                                     time, duration, numFrames,
                                     reinterpret_cast<AudioDataValue *>(decoded.forget()),
                                     rate);
    mCallback->Output(audio);
    mHaveOutput = true;

    mCurrentAudioFrame += numFrames;

    if (rv == kNeedMoreData) {
      // No error; we just need more data.
      LOG("FillComplexBuffer out of data\n");
      mCallback->InputExhausted();
      break;
    }
  } while (true);
}

void
AppleATDecoder::SetupDecoder()
{
  AudioStreamBasicDescription inputFormat, outputFormat;
  // Fill in the input format description from the stream.
  AppleUtils::GetProperty(mStream,
      kAudioFileStreamProperty_DataFormat, &inputFormat);

  // Fill in the output format manually.
  PodZero(&outputFormat);
  outputFormat.mFormatID = kAudioFormatLinearPCM;
  outputFormat.mSampleRate = inputFormat.mSampleRate;
  outputFormat.mChannelsPerFrame = inputFormat.mChannelsPerFrame;
#if defined(MOZ_SAMPLE_TYPE_FLOAT32)
  outputFormat.mBitsPerChannel = 32;
  outputFormat.mFormatFlags =
    kLinearPCMFormatFlagIsFloat |
    0;
#else
# error Unknown audio sample type
#endif
  // Set up the decoder so it gives us one sample per frame
  outputFormat.mFramesPerPacket = 1;
  outputFormat.mBytesPerPacket = outputFormat.mBytesPerFrame
        = outputFormat.mChannelsPerFrame * outputFormat.mBitsPerChannel / 8;

  OSStatus rv = AudioConverterNew(&inputFormat, &outputFormat, &mConverter);
  if (rv) {
    LOG("Error %d constructing AudioConverter", rv);
    mConverter = nullptr;
    mCallback->Error();
  }
  mHaveOutput = false;
}

void
AppleATDecoder::SubmitSample(nsAutoPtr<mp4_demuxer::MP4Sample> aSample)
{
  mSamplePosition = aSample->byte_offset;
  OSStatus rv = AudioFileStreamParseBytes(mStream,
                                          aSample->size,
                                          aSample->data,
                                          0);
  if (rv != noErr) {
    LOG("Error %d parsing audio data", rv);
    mCallback->Error();
  }

  // Sometimes we need multiple input samples before AudioToolbox
  // starts decoding. If we haven't seen any output yet, ask for
  // more data here.
  if (!mHaveOutput) {
    mCallback->InputExhausted();
  }
}

} // namespace mozilla
