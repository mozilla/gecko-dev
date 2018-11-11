/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_AUDIOSEGMENT_H_
#define MOZILLA_AUDIOSEGMENT_H_

#include "MediaSegment.h"
#include "AudioSampleFormat.h"
#include "AudioChannelFormat.h"
#include "SharedBuffer.h"
#include "WebAudioUtils.h"
#ifdef MOZILLA_INTERNAL_API
#include "mozilla/TimeStamp.h"
#endif
#include <float.h>

namespace mozilla {

template<typename T>
class SharedChannelArrayBuffer : public ThreadSharedObject {
public:
  explicit SharedChannelArrayBuffer(nsTArray<nsTArray<T> >* aBuffers)
  {
    mBuffers.SwapElements(*aBuffers);
  }

  size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const override
  {
    size_t amount = 0;
    amount += mBuffers.ShallowSizeOfExcludingThis(aMallocSizeOf);
    for (size_t i = 0; i < mBuffers.Length(); i++) {
      amount += mBuffers[i].ShallowSizeOfExcludingThis(aMallocSizeOf);
    }

    return amount;
  }

  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const override
  {
    return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
  }

  nsTArray<nsTArray<T> > mBuffers;
};

class AudioMixer;

/**
 * For auto-arrays etc, guess this as the common number of channels.
 */
const int GUESS_AUDIO_CHANNELS = 2;

// We ensure that the graph advances in steps that are multiples of the Web
// Audio block size
const uint32_t WEBAUDIO_BLOCK_SIZE_BITS = 7;
const uint32_t WEBAUDIO_BLOCK_SIZE = 1 << WEBAUDIO_BLOCK_SIZE_BITS;

template <typename SrcT, typename DestT>
static void
InterleaveAndConvertBuffer(const SrcT* const* aSourceChannels,
                           uint32_t aLength, float aVolume,
                           uint32_t aChannels,
                           DestT* aOutput)
{
  DestT* output = aOutput;
  for (size_t i = 0; i < aLength; ++i) {
    for (size_t channel = 0; channel < aChannels; ++channel) {
      float v = AudioSampleToFloat(aSourceChannels[channel][i])*aVolume;
      *output = FloatToAudioSample<DestT>(v);
      ++output;
    }
  }
}

template <typename SrcT, typename DestT>
static void
DeinterleaveAndConvertBuffer(const SrcT* aSourceBuffer,
                             uint32_t aFrames, uint32_t aChannels,
                             DestT** aOutput)
{
  for (size_t i = 0; i < aChannels; i++) {
    size_t interleavedIndex = i;
    for (size_t j = 0; j < aFrames; j++) {
      ConvertAudioSample(aSourceBuffer[interleavedIndex],
                         aOutput[i][j]);
      interleavedIndex += aChannels;
    }
  }
}

class SilentChannel
{
public:
  static const int AUDIO_PROCESSING_FRAMES = 640; /* > 10ms of 48KHz audio */
  static const uint8_t gZeroChannel[MAX_AUDIO_SAMPLE_SIZE*AUDIO_PROCESSING_FRAMES];
  // We take advantage of the fact that zero in float and zero in int have the
  // same all-zeros bit layout.
  template<typename T>
  static const T* ZeroChannel();
};


/**
 * Given an array of input channels (aChannelData), downmix to aOutputChannels,
 * interleave the channel data. A total of aOutputChannels*aDuration
 * interleaved samples will be copied to a channel buffer in aOutput.
 */
template <typename SrcT, typename DestT>
void
DownmixAndInterleave(const nsTArray<const SrcT*>& aChannelData,
                     int32_t aDuration, float aVolume, uint32_t aOutputChannels,
                     DestT* aOutput)
{

  if (aChannelData.Length() == aOutputChannels) {
    InterleaveAndConvertBuffer(aChannelData.Elements(),
                               aDuration, aVolume, aOutputChannels, aOutput);
  } else {
    AutoTArray<SrcT*,GUESS_AUDIO_CHANNELS> outputChannelData;
    AutoTArray<SrcT, SilentChannel::AUDIO_PROCESSING_FRAMES * GUESS_AUDIO_CHANNELS> outputBuffers;
    outputChannelData.SetLength(aOutputChannels);
    outputBuffers.SetLength(aDuration * aOutputChannels);
    for (uint32_t i = 0; i < aOutputChannels; i++) {
      outputChannelData[i] = outputBuffers.Elements() + aDuration * i;
    }
    AudioChannelsDownMix(aChannelData,
                         outputChannelData.Elements(),
                         aOutputChannels,
                         aDuration);
    InterleaveAndConvertBuffer(outputChannelData.Elements(),
                               aDuration, aVolume, aOutputChannels, aOutput);
  }
}

/**
 * An AudioChunk represents a multi-channel buffer of audio samples.
 * It references an underlying ThreadSharedObject which manages the lifetime
 * of the buffer. An AudioChunk maintains its own duration and channel data
 * pointers so it can represent a subinterval of a buffer without copying.
 * An AudioChunk can store its individual channels anywhere; it maintains
 * separate pointers to each channel's buffer.
 */
struct AudioChunk {
  typedef mozilla::AudioSampleFormat SampleFormat;

  AudioChunk() : mPrincipalHandle(PRINCIPAL_HANDLE_NONE) {}

  // Generic methods
  void SliceTo(StreamTime aStart, StreamTime aEnd)
  {
    MOZ_ASSERT(aStart >= 0 && aStart < aEnd && aEnd <= mDuration,
               "Slice out of bounds");
    if (mBuffer) {
      MOZ_ASSERT(aStart < INT32_MAX, "Can't slice beyond 32-bit sample lengths");
      for (uint32_t channel = 0; channel < mChannelData.Length(); ++channel) {
        mChannelData[channel] = AddAudioSampleOffset(mChannelData[channel],
            mBufferFormat, int32_t(aStart));
      }
    }
    mDuration = aEnd - aStart;
  }
  StreamTime GetDuration() const { return mDuration; }
  bool CanCombineWithFollowing(const AudioChunk& aOther) const
  {
    if (aOther.mBuffer != mBuffer) {
      return false;
    }
    if (mBuffer) {
      NS_ASSERTION(aOther.mBufferFormat == mBufferFormat,
                   "Wrong metadata about buffer");
      NS_ASSERTION(aOther.mChannelData.Length() == mChannelData.Length(),
                   "Mismatched channel count");
      if (mDuration > INT32_MAX) {
        return false;
      }
      for (uint32_t channel = 0; channel < mChannelData.Length(); ++channel) {
        if (aOther.mChannelData[channel] != AddAudioSampleOffset(mChannelData[channel],
            mBufferFormat, int32_t(mDuration))) {
          return false;
        }
      }
    }
    return true;
  }
  bool IsNull() const { return mBuffer == nullptr; }
  void SetNull(StreamTime aDuration)
  {
    mBuffer = nullptr;
    mChannelData.Clear();
    mDuration = aDuration;
    mVolume = 1.0f;
    mBufferFormat = AUDIO_FORMAT_SILENCE;
    mPrincipalHandle = PRINCIPAL_HANDLE_NONE;
  }

  size_t ChannelCount() const { return mChannelData.Length(); }

  bool IsMuted() const { return mVolume == 0.0f; }

  size_t SizeOfExcludingThisIfUnshared(MallocSizeOf aMallocSizeOf) const
  {
    return SizeOfExcludingThis(aMallocSizeOf, true);
  }

  size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf, bool aUnshared) const
  {
    size_t amount = 0;

    // Possibly owned:
    // - mBuffer - Can hold data that is also in the decoded audio queue. If it
    //             is not shared, or unshared == false it gets counted.
    if (mBuffer && (!aUnshared || !mBuffer->IsShared())) {
      amount += mBuffer->SizeOfIncludingThis(aMallocSizeOf);
    }

    // Memory in the array is owned by mBuffer.
    amount += mChannelData.ShallowSizeOfExcludingThis(aMallocSizeOf);
    return amount;
  }

  template<typename T>
  const nsTArray<const T*>& ChannelData()
  {
    MOZ_ASSERT(AudioSampleTypeToFormat<T>::Format == mBufferFormat);
    return *reinterpret_cast<nsTArray<const T*>*>(&mChannelData);
  }

  PrincipalHandle GetPrincipalHandle() const { return mPrincipalHandle; }

  StreamTime mDuration; // in frames within the buffer
  RefPtr<ThreadSharedObject> mBuffer; // the buffer object whose lifetime is managed; null means data is all zeroes
  nsTArray<const void*> mChannelData; // one pointer per channel; empty if and only if mBuffer is null
  float mVolume; // volume multiplier to apply (1.0f if mBuffer is nonnull)
  SampleFormat mBufferFormat; // format of frames in mBuffer (only meaningful if mBuffer is nonnull)
#ifdef MOZILLA_INTERNAL_API
  mozilla::TimeStamp mTimeStamp;           // time at which this has been fetched from the MediaEngine
#endif
  // principalHandle for the data in this chunk.
  // This can be compared to an nsIPrincipal* when back on main thread.
  PrincipalHandle mPrincipalHandle;
};

/**
 * A list of audio samples consisting of a sequence of slices of SharedBuffers.
 * The audio rate is determined by the track, not stored in this class.
 */
class AudioSegment : public MediaSegmentBase<AudioSegment, AudioChunk> {
public:
  typedef mozilla::AudioSampleFormat SampleFormat;

  AudioSegment() : MediaSegmentBase<AudioSegment, AudioChunk>(AUDIO) {}

  // Resample the whole segment in place.
  template<typename T>
  void Resample(SpeexResamplerState* aResampler, uint32_t aInRate, uint32_t aOutRate)
  {
    mDuration = 0;
#ifdef DEBUG
    uint32_t segmentChannelCount = ChannelCount();
#endif

    for (ChunkIterator ci(*this); !ci.IsEnded(); ci.Next()) {
      AutoTArray<nsTArray<T>, GUESS_AUDIO_CHANNELS> output;
      AutoTArray<const T*, GUESS_AUDIO_CHANNELS> bufferPtrs;
      AudioChunk& c = *ci;
      // If this chunk is null, don't bother resampling, just alter its duration
      if (c.IsNull()) {
        c.mDuration = (c.mDuration * aOutRate) / aInRate;
        mDuration += c.mDuration;
        continue;
      }
      uint32_t channels = c.mChannelData.Length();
      MOZ_ASSERT(channels == segmentChannelCount);
      output.SetLength(channels);
      bufferPtrs.SetLength(channels);
      uint32_t inFrames = c.mDuration;
      // Round up to allocate; the last frame may not be used.
      NS_ASSERTION((UINT32_MAX - aInRate + 1) / c.mDuration >= aOutRate,
                   "Dropping samples");
      uint32_t outSize = (c.mDuration * aOutRate + aInRate - 1) / aInRate;
      for (uint32_t i = 0; i < channels; i++) {
        T* out = output[i].AppendElements(outSize);
        uint32_t outFrames = outSize;

        const T* in = static_cast<const T*>(c.mChannelData[i]);
        dom::WebAudioUtils::SpeexResamplerProcess(aResampler, i,
                                                  in, &inFrames,
                                                  out, &outFrames);
        MOZ_ASSERT(inFrames == c.mDuration);

        bufferPtrs[i] = out;
        output[i].SetLength(outFrames);
      }
      MOZ_ASSERT(channels > 0);
      c.mDuration = output[0].Length();
      c.mBuffer = new mozilla::SharedChannelArrayBuffer<T>(&output);
      for (uint32_t i = 0; i < channels; i++) {
        c.mChannelData[i] = bufferPtrs[i];
      }
      mDuration += c.mDuration;
    }
  }

  void ResampleChunks(SpeexResamplerState* aResampler,
                      uint32_t aInRate,
                      uint32_t aOutRate);

  void AppendFrames(already_AddRefed<ThreadSharedObject> aBuffer,
                    const nsTArray<const float*>& aChannelData,
                    int32_t aDuration, const PrincipalHandle& aPrincipalHandle)
  {
    AudioChunk* chunk = AppendChunk(aDuration);
    chunk->mBuffer = aBuffer;
    for (uint32_t channel = 0; channel < aChannelData.Length(); ++channel) {
      chunk->mChannelData.AppendElement(aChannelData[channel]);
    }
    chunk->mVolume = 1.0f;
    chunk->mBufferFormat = AUDIO_FORMAT_FLOAT32;
#ifdef MOZILLA_INTERNAL_API
    chunk->mTimeStamp = TimeStamp::Now();
#endif
    chunk->mPrincipalHandle = aPrincipalHandle;
  }
  void AppendFrames(already_AddRefed<ThreadSharedObject> aBuffer,
                    const nsTArray<const int16_t*>& aChannelData,
                    int32_t aDuration, const PrincipalHandle& aPrincipalHandle)
  {
    AudioChunk* chunk = AppendChunk(aDuration);
    chunk->mBuffer = aBuffer;
    for (uint32_t channel = 0; channel < aChannelData.Length(); ++channel) {
      chunk->mChannelData.AppendElement(aChannelData[channel]);
    }
    chunk->mVolume = 1.0f;
    chunk->mBufferFormat = AUDIO_FORMAT_S16;
#ifdef MOZILLA_INTERNAL_API
    chunk->mTimeStamp = TimeStamp::Now();
#endif
    chunk->mPrincipalHandle = aPrincipalHandle;
  }
  // Consumes aChunk, and returns a pointer to the persistent copy of aChunk
  // in the segment.
  AudioChunk* AppendAndConsumeChunk(AudioChunk* aChunk)
  {
    AudioChunk* chunk = AppendChunk(aChunk->mDuration);
    chunk->mBuffer = aChunk->mBuffer.forget();
    chunk->mChannelData.SwapElements(aChunk->mChannelData);
    chunk->mVolume = aChunk->mVolume;
    chunk->mBufferFormat = aChunk->mBufferFormat;
#ifdef MOZILLA_INTERNAL_API
    chunk->mTimeStamp = TimeStamp::Now();
#endif
    chunk->mPrincipalHandle = aChunk->mPrincipalHandle;
    return chunk;
  }
  void ApplyVolume(float aVolume);
  // Mix the segment into a mixer, interleaved. This is useful to output a
  // segment to a system audio callback. It up or down mixes to aChannelCount
  // channels.
  void WriteTo(uint64_t aID, AudioMixer& aMixer, uint32_t aChannelCount,
               uint32_t aSampleRate);
  // Mix the segment into a mixer, keeping it planar, up or down mixing to
  // aChannelCount channels.
  void Mix(AudioMixer& aMixer, uint32_t aChannelCount, uint32_t aSampleRate);

  int ChannelCount() {
    NS_WARNING_ASSERTION(
      !mChunks.IsEmpty(),
      "Cannot query channel count on a AudioSegment with no chunks.");
    // Find the first chunk that has non-zero channels. A chunk that hs zero
    // channels is just silence and we can simply discard it.
    for (ChunkIterator ci(*this); !ci.IsEnded(); ci.Next()) {
      if (ci->ChannelCount()) {
        return ci->ChannelCount();
      }
    }
    return 0;
  }

  bool IsNull() const {
    for (ChunkIterator ci(*const_cast<AudioSegment*>(this)); !ci.IsEnded();
         ci.Next()) {
      if (!ci->IsNull()) {
        return false;
      }
    }
    return true;
  }

  static Type StaticType() { return AUDIO; }

  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const override
  {
    return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
  }
};

template<typename SrcT>
void WriteChunk(AudioChunk& aChunk,
                uint32_t aOutputChannels,
                AudioDataValue* aOutputBuffer)
{
  AutoTArray<const SrcT*,GUESS_AUDIO_CHANNELS> channelData;

  channelData = aChunk.ChannelData<SrcT>();

  if (channelData.Length() < aOutputChannels) {
    // Up-mix. Note that this might actually make channelData have more
    // than aOutputChannels temporarily.
    AudioChannelsUpMix(&channelData, aOutputChannels, SilentChannel::ZeroChannel<SrcT>());
  }
  if (channelData.Length() > aOutputChannels) {
    // Down-mix.
    DownmixAndInterleave(channelData, aChunk.mDuration,
        aChunk.mVolume, aOutputChannels, aOutputBuffer);
  } else {
    InterleaveAndConvertBuffer(channelData.Elements(),
        aChunk.mDuration, aChunk.mVolume,
        aOutputChannels,
        aOutputBuffer);
  }
}



} // namespace mozilla

#endif /* MOZILLA_AUDIOSEGMENT_H_ */
