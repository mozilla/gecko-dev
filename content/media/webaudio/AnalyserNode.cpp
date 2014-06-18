/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/AnalyserNode.h"
#include "mozilla/dom/AnalyserNodeBinding.h"
#include "AudioNodeEngine.h"
#include "AudioNodeStream.h"
#include "mozilla/Mutex.h"
#include "mozilla/PodOperations.h"

namespace mozilla {
namespace dom {

NS_IMPL_ISUPPORTS_INHERITED0(AnalyserNode, AudioNode)

class AnalyserNodeEngine : public AudioNodeEngine
{
  class TransferBuffer : public nsRunnable
  {
  public:
    TransferBuffer(AudioNodeStream* aStream,
                   const AudioChunk& aChunk)
      : mStream(aStream)
      , mChunk(aChunk)
    {
    }

    NS_IMETHOD Run()
    {
      nsRefPtr<AnalyserNode> node;
      {
        // No need to keep holding the lock for the whole duration of this
        // function, since we're holding a strong reference to it, so if
        // we can obtain the reference, we will hold the node alive in
        // this function.
        MutexAutoLock lock(mStream->Engine()->NodeMutex());
        node = static_cast<AnalyserNode*>(mStream->Engine()->Node());
      }
      if (node) {
        node->AppendChunk(mChunk);
      }
      return NS_OK;
    }

  private:
    nsRefPtr<AudioNodeStream> mStream;
    AudioChunk mChunk;
  };

public:
  explicit AnalyserNodeEngine(AnalyserNode* aNode)
    : AudioNodeEngine(aNode)
  {
    MOZ_ASSERT(NS_IsMainThread());
  }

  virtual void ProcessBlock(AudioNodeStream* aStream,
                            const AudioChunk& aInput,
                            AudioChunk* aOutput,
                            bool* aFinished) MOZ_OVERRIDE
  {
    *aOutput = aInput;

    MutexAutoLock lock(NodeMutex());

    if (Node() &&
        aInput.mChannelData.Length() > 0) {
      nsRefPtr<TransferBuffer> transfer = new TransferBuffer(aStream, aInput);
      NS_DispatchToMainThread(transfer);
    }
  }

  virtual size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const MOZ_OVERRIDE
  {
    return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
  }
};

AnalyserNode::AnalyserNode(AudioContext* aContext)
  : AudioNode(aContext,
              1,
              ChannelCountMode::Explicit,
              ChannelInterpretation::Speakers)
  , mAnalysisBlock(2048)
  , mMinDecibels(-100.)
  , mMaxDecibels(-30.)
  , mSmoothingTimeConstant(.8)
  , mWriteIndex(0)
{
  mStream = aContext->Graph()->CreateAudioNodeStream(new AnalyserNodeEngine(this),
                                                     MediaStreamGraph::INTERNAL_STREAM);
  AllocateBuffer();
}

size_t
AnalyserNode::SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const
{
  size_t amount = AudioNode::SizeOfExcludingThis(aMallocSizeOf);
  amount += mAnalysisBlock.SizeOfExcludingThis(aMallocSizeOf);
  amount += mBuffer.SizeOfExcludingThis(aMallocSizeOf);
  amount += mOutputBuffer.SizeOfExcludingThis(aMallocSizeOf);
  return amount;
}

size_t
AnalyserNode::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
{
  return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
}

JSObject*
AnalyserNode::WrapObject(JSContext* aCx)
{
  return AnalyserNodeBinding::Wrap(aCx, this);
}

void
AnalyserNode::SetFftSize(uint32_t aValue, ErrorResult& aRv)
{
  // Disallow values that are not a power of 2 and outside the [32,2048] range
  if (aValue < 32 ||
      aValue > 2048 ||
      (aValue & (aValue - 1)) != 0) {
    aRv.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return;
  }
  if (FftSize() != aValue) {
    mAnalysisBlock.SetFFTSize(aValue);
    AllocateBuffer();
  }
}

void
AnalyserNode::SetMinDecibels(double aValue, ErrorResult& aRv)
{
  if (aValue >= mMaxDecibels) {
    aRv.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return;
  }
  mMinDecibels = aValue;
}

void
AnalyserNode::SetMaxDecibels(double aValue, ErrorResult& aRv)
{
  if (aValue <= mMinDecibels) {
    aRv.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return;
  }
  mMaxDecibels = aValue;
}

void
AnalyserNode::SetSmoothingTimeConstant(double aValue, ErrorResult& aRv)
{
  if (aValue < 0 || aValue > 1) {
    aRv.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return;
  }
  mSmoothingTimeConstant = aValue;
}

void
AnalyserNode::GetFloatFrequencyData(const Float32Array& aArray)
{
  if (!FFTAnalysis()) {
    // Might fail to allocate memory
    return;
  }

  aArray.ComputeLengthAndData();

  float* buffer = aArray.Data();
  size_t length = std::min(size_t(aArray.Length()), mOutputBuffer.Length());

  for (size_t i = 0; i < length; ++i) {
    buffer[i] = WebAudioUtils::ConvertLinearToDecibels(mOutputBuffer[i], mMinDecibels);
  }
}

void
AnalyserNode::GetByteFrequencyData(const Uint8Array& aArray)
{
  if (!FFTAnalysis()) {
    // Might fail to allocate memory
    return;
  }

  const double rangeScaleFactor = 1.0 / (mMaxDecibels - mMinDecibels);

  aArray.ComputeLengthAndData();

  unsigned char* buffer = aArray.Data();
  size_t length = std::min(size_t(aArray.Length()), mOutputBuffer.Length());

  for (size_t i = 0; i < length; ++i) {
    const double decibels = WebAudioUtils::ConvertLinearToDecibels(mOutputBuffer[i], mMinDecibels);
    // scale down the value to the range of [0, UCHAR_MAX]
    const double scaled = std::max(0.0, std::min(double(UCHAR_MAX),
                                                 UCHAR_MAX * (decibels - mMinDecibels) * rangeScaleFactor));
    buffer[i] = static_cast<unsigned char>(scaled);
  }
}

void
AnalyserNode::GetFloatTimeDomainData(const Float32Array& aArray)
{
  aArray.ComputeLengthAndData();

  float* buffer = aArray.Data();
  size_t length = std::min(size_t(aArray.Length()), mBuffer.Length());

  for (size_t i = 0; i < length; ++i) {
    buffer[i] = mBuffer[(i + mWriteIndex) % mBuffer.Length()];;
  }
}

void
AnalyserNode::GetByteTimeDomainData(const Uint8Array& aArray)
{
  aArray.ComputeLengthAndData();

  unsigned char* buffer = aArray.Data();
  size_t length = std::min(size_t(aArray.Length()), mBuffer.Length());

  for (size_t i = 0; i < length; ++i) {
    const float value = mBuffer[(i + mWriteIndex) % mBuffer.Length()];
    // scale the value to the range of [0, UCHAR_MAX]
    const float scaled = std::max(0.0f, std::min(float(UCHAR_MAX),
                                                 128.0f * (value + 1.0f)));
    buffer[i] = static_cast<unsigned char>(scaled);
  }
}

bool
AnalyserNode::FFTAnalysis()
{
  float* inputBuffer;
  bool allocated = false;
  if (mWriteIndex == 0) {
    inputBuffer = mBuffer.Elements();
  } else {
    inputBuffer = static_cast<float*>(moz_malloc(FftSize() * sizeof(float)));
    if (!inputBuffer) {
      return false;
    }
    memcpy(inputBuffer, mBuffer.Elements() + mWriteIndex, sizeof(float) * (FftSize() - mWriteIndex));
    memcpy(inputBuffer + FftSize() - mWriteIndex, mBuffer.Elements(), sizeof(float) * mWriteIndex);
    allocated = true;
  }

  ApplyBlackmanWindow(inputBuffer, FftSize());

  mAnalysisBlock.PerformFFT(inputBuffer);

  // Normalize so than an input sine wave at 0dBfs registers as 0dBfs (undo FFT scaling factor).
  const double magnitudeScale = 1.0 / FftSize();

  for (uint32_t i = 0; i < mOutputBuffer.Length(); ++i) {
    double scalarMagnitude = NS_hypot(mAnalysisBlock.RealData(i),
                                      mAnalysisBlock.ImagData(i)) *
                             magnitudeScale;
    mOutputBuffer[i] = mSmoothingTimeConstant * mOutputBuffer[i] +
                       (1.0 - mSmoothingTimeConstant) * scalarMagnitude;
  }

  if (allocated) {
    moz_free(inputBuffer);
  }
  return true;
}

void
AnalyserNode::ApplyBlackmanWindow(float* aBuffer, uint32_t aSize)
{
  double alpha = 0.16;
  double a0 = 0.5 * (1.0 - alpha);
  double a1 = 0.5;
  double a2 = 0.5 * alpha;

  for (uint32_t i = 0; i < aSize; ++i) {
    double x = double(i) / aSize;
    double window = a0 - a1 * cos(2 * M_PI * x) + a2 * cos(4 * M_PI * x);
    aBuffer[i] *= window;
  }
}

bool
AnalyserNode::AllocateBuffer()
{
  bool result = true;
  if (mBuffer.Length() != FftSize()) {
    result = mBuffer.SetLength(FftSize());
    if (result) {
      memset(mBuffer.Elements(), 0, sizeof(float) * FftSize());
      mWriteIndex = 0;

      result = mOutputBuffer.SetLength(FrequencyBinCount());
      if (result) {
        memset(mOutputBuffer.Elements(), 0, sizeof(float) * FrequencyBinCount());
      }
    }
  }
  return result;
}

void
AnalyserNode::AppendChunk(const AudioChunk& aChunk)
{
  const uint32_t bufferSize = mBuffer.Length();
  const uint32_t channelCount = aChunk.mChannelData.Length();
  uint32_t chunkDuration = aChunk.mDuration;
  MOZ_ASSERT((bufferSize & (bufferSize - 1)) == 0); // Must be a power of two!
  MOZ_ASSERT(channelCount > 0);
  MOZ_ASSERT(chunkDuration == WEBAUDIO_BLOCK_SIZE);

  if (chunkDuration > bufferSize) {
    // Copy a maximum bufferSize samples.
    chunkDuration = bufferSize;
  }

  PodCopy(mBuffer.Elements() + mWriteIndex, static_cast<const float*>(aChunk.mChannelData[0]), chunkDuration);
  for (uint32_t i = 1; i < channelCount; ++i) {
    AudioBlockAddChannelWithScale(static_cast<const float*>(aChunk.mChannelData[i]), 1.0f,
                                  mBuffer.Elements() + mWriteIndex);
  }
  if (channelCount > 1) {
    AudioBlockInPlaceScale(mBuffer.Elements() + mWriteIndex,
                           1.0f / aChunk.mChannelData.Length());
  }
  mWriteIndex += chunkDuration;
  MOZ_ASSERT(mWriteIndex <= bufferSize);
  if (mWriteIndex >= bufferSize) {
    mWriteIndex = 0;
  }
}

}
}

