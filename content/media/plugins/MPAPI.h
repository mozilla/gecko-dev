/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#if !defined(MPAPI_h_)
#define MPAPI_h_

#include <stdint.h>

namespace MPAPI {

enum ColorFormat {
  I420,
  RGB565
};

/*
 * A callback for the plugin to use to request a buffer owned by gecko. This can
 * save us a copy or two down the line.
 */
class BufferCallback {
public:
  virtual void *operator()(size_t aWidth, size_t aHeight,
                           ColorFormat aColorFormat) = 0;
};

struct VideoPlane {
  VideoPlane() :
    mData(0),
    mStride(0),
    mWidth(0),
    mHeight(0),
    mOffset(0),
    mSkip(0)
  {}

  void *mData;
  int32_t mStride;
  int32_t mWidth;
  int32_t mHeight;
  int32_t mOffset;
  int32_t mSkip;
};

struct VideoFrame {
  int64_t mTimeUs;
  bool mKeyFrame;
  void *mData;
  size_t mSize;
  int32_t mStride;
  int32_t mSliceHeight;
  int32_t mRotation;
  VideoPlane Y;
  VideoPlane Cb;
  VideoPlane Cr;

  VideoFrame() :
    mTimeUs(0),
    mKeyFrame(false),
    mData(0),
    mSize(0),
    mStride(0),
    mSliceHeight(0),
    mRotation(0)
  {}

  void Set(int64_t aTimeUs, bool aKeyFrame,
           void *aData, size_t aSize, int32_t aStride, int32_t aSliceHeight, int32_t aRotation,
           void *aYData, int32_t aYStride, int32_t aYWidth, int32_t aYHeight, int32_t aYOffset, int32_t aYSkip,
           void *aCbData, int32_t aCbStride, int32_t aCbWidth, int32_t aCbHeight, int32_t aCbOffset, int32_t aCbSkip,
           void *aCrData, int32_t aCrStride, int32_t aCrWidth, int32_t aCrHeight, int32_t aCrOffset, int32_t aCrSkip)
  {
    mTimeUs = aTimeUs;
    mKeyFrame = aKeyFrame;
    mData = aData;
    mSize = aSize;
    mStride = aStride;
    mSliceHeight = aSliceHeight;
    mRotation = aRotation;
    Y.mData = aYData;
    Y.mStride = aYStride;
    Y.mWidth = aYWidth;
    Y.mHeight = aYHeight;
    Y.mOffset = aYOffset;
    Y.mSkip = aYSkip;
    Cb.mData = aCbData;
    Cb.mStride = aCbStride;
    Cb.mWidth = aCbWidth;
    Cb.mHeight = aCbHeight;
    Cb.mOffset = aCbOffset;
    Cb.mSkip = aCbSkip;
    Cr.mData = aCrData;
    Cr.mStride = aCrStride;
    Cr.mWidth = aCrWidth;
    Cr.mHeight = aCrHeight;
    Cr.mOffset = aCrOffset;
    Cr.mSkip = aCrSkip;
  }
};

struct AudioFrame {
  int64_t mTimeUs;
  void *mData; // 16PCM interleaved
  size_t mSize; // Size of mData in bytes
  int32_t mAudioChannels;
  int32_t mAudioSampleRate;

  AudioFrame() :
    mTimeUs(0),
    mData(0),
    mSize(0),
    mAudioChannels(0),
    mAudioSampleRate(0)
  {
  }

  void Set(int64_t aTimeUs,
           void *aData, size_t aSize,
           int32_t aAudioChannels, int32_t aAudioSampleRate)
  {
    mTimeUs = aTimeUs;
    mData = aData;
    mSize = aSize;
    mAudioChannels = aAudioChannels;
    mAudioSampleRate = aAudioSampleRate;
  }
};

struct Decoder;

struct PluginHost {
  bool (*Read)(Decoder *aDecoder, char *aBuffer, int64_t aOffset, uint32_t aCount, uint32_t* aBytes);
  uint64_t (*GetLength)(Decoder *aDecoder);
  void (*SetMetaDataReadMode)(Decoder *aDecoder);
  void (*SetPlaybackReadMode)(Decoder *aDecoder);
  bool (*GetIntPref)(const char *aPref, int32_t *aResult);
  bool (*GetSystemInfoString)(const char *aKey, char *aResult, uint32_t aResultLen);
};

struct Decoder {
  void *mResource;
  void *mPrivate;

  Decoder();

  void (*GetDuration)(Decoder *aDecoder, int64_t *durationUs);
  void (*GetVideoParameters)(Decoder *aDecoder, int32_t *aWidth, int32_t *aHeight);
  void (*GetAudioParameters)(Decoder *aDecoder, int32_t *aNumChannels, int32_t *aSampleRate);
  bool (*HasVideo)(Decoder *aDecoder);
  bool (*HasAudio)(Decoder *aDecoder);
  bool (*ReadVideo)(Decoder *aDecoder, VideoFrame *aFrame, int64_t aSeekTimeUs, BufferCallback *aBufferCallback);
  bool (*ReadAudio)(Decoder *aDecoder, AudioFrame *aFrame, int64_t aSeekTimeUs);
  void (*DestroyDecoder)(Decoder *);
};

struct Manifest {
  bool (*CanDecode)(const char *aMimeChars, size_t aMimeLen, const char* const**aCodecs);
  bool (*CreateDecoder)(PluginHost *aPluginHost, Decoder *aDecoder,
                        const char *aMimeChars, size_t aMimeLen);
};

}

#endif
