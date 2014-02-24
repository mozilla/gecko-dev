/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GMPMessageUtils_h_
#define GMPMessageUtils_h_

#include "GMPVideoHost.h"
#include "GMPVideoPlaneImpl.h"
#include "GMPVideoi420FrameImpl.h"
#include "gmp-video-codec.h"
#include "mozilla/ipc/Shmem.h"
#include "GMPVideoEncodedFrameImpl.h"

namespace IPC {

template <>
struct ParamTraits<mozilla::gmp::GMPPlaneImpl>
{
  typedef mozilla::gmp::GMPPlaneImpl paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    // Planes are always passed with Shmem object separately.
    // We will get buffer and allocated size from that.
    WriteParam(aMsg, aParam.mSize);
    WriteParam(aMsg, aParam.mStride);
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    if (!ReadParam(aMsg, aIter, &(aResult->mSize)) ||
        !ReadParam(aMsg, aIter, &(aResult->mStride))) {
      return false;
    }

    return true;
  }

  static void Log(const paramType& aParam, std::wstring* aLog)
  {
    aLog->append(StringPrintf(L"[%p, %i, %i, %i]", aParam.mBuffer, aParam.mAllocatedSize, aParam.mSize,
                              aParam.mStride));
  }
};

template <>
struct ParamTraits<GMPVideoCodecVP8>
{
  typedef GMPVideoCodecVP8 paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.mPictureLossIndicationOn);
    WriteParam(aMsg, aParam.mFeedbackModeOn);
    WriteParam(aMsg, static_cast<int>(aParam.mComplexity));
    WriteParam(aMsg, static_cast<int>(aParam.mResilience));
    WriteParam(aMsg, aParam.mNumberOfTemporalLayers);
    WriteParam(aMsg, aParam.mDenoisingOn);
    WriteParam(aMsg, aParam.mErrorConcealmentOn);
    WriteParam(aMsg, aParam.mAutomaticResizeOn);
    WriteParam(aMsg, aParam.mFrameDroppingOn);
    WriteParam(aMsg, aParam.mKeyFrameInterval);
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    int complexity, resilience;
    if (ReadParam(aMsg, aIter, &(aResult->mPictureLossIndicationOn)) &&
        ReadParam(aMsg, aIter, &(aResult->mFeedbackModeOn)) &&
        ReadParam(aMsg, aIter, &complexity) &&
        ReadParam(aMsg, aIter, &resilience) &&
        ReadParam(aMsg, aIter, &(aResult->mNumberOfTemporalLayers)) &&
        ReadParam(aMsg, aIter, &(aResult->mDenoisingOn)) &&
        ReadParam(aMsg, aIter, &(aResult->mErrorConcealmentOn)) &&
        ReadParam(aMsg, aIter, &(aResult->mAutomaticResizeOn)) &&
        ReadParam(aMsg, aIter, &(aResult->mFrameDroppingOn)) &&
        ReadParam(aMsg, aIter, &(aResult->mKeyFrameInterval))) {
      aResult->mComplexity = static_cast<GMPVideoCodecComplexity>(complexity);
      aResult->mResilience = static_cast<GMPVP8ResilienceMode>(resilience);
      return true;
    }

    return false;
  }

  static void Log(const paramType& aParam, std::wstring* aLog)
  {
    aLog->append(StringPrintf(L"[%d, %d, %d, %d, %u, %d, %d, %d, %d, %d]",
                              aParam.mPictureLossIndicationOn,
                              aParam.mFeedbackModeOn,
                              aParam.mComplexity,
                              aParam.mResilience,
                              aParam.mNumberOfTemporalLayers,
                              aParam.mDenoisingOn,
                              aParam.mErrorConcealmentOn,
                              aParam.mAutomaticResizeOn,
                              aParam.mFrameDroppingOn,
                              aParam.mKeyFrameInterval));
  }
};

template <>
struct ParamTraits<mozilla::gmp::GMPVideoi420FrameImpl>
{
  typedef mozilla::gmp::GMPVideoi420FrameImpl paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.mYPlane);
    WriteParam(aMsg, aParam.mUPlane);
    WriteParam(aMsg, aParam.mVPlane);
    WriteParam(aMsg, aParam.mWidth);
    WriteParam(aMsg, aParam.mHeight);
    WriteParam(aMsg, aParam.mTimestamp);
    WriteParam(aMsg, aParam.mRenderTime_ms);
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    if (ReadParam(aMsg, aIter, &(aResult->mYPlane)) &&
        ReadParam(aMsg, aIter, &(aResult->mUPlane)) &&
        ReadParam(aMsg, aIter, &(aResult->mVPlane)) &&
        ReadParam(aMsg, aIter, &(aResult->mWidth)) &&
        ReadParam(aMsg, aIter, &(aResult->mHeight)) &&
        ReadParam(aMsg, aIter, &(aResult->mTimestamp)) &&
        ReadParam(aMsg, aIter, &(aResult->mRenderTime_ms))) {
      return true;
    }

    return false;
  }

  static void Log(const paramType& aParam, std::wstring* aLog)
  {
    aLog->append(StringPrintf(L"[%i, %i, %u, %i]", aParam.mWidth, aParam.mHeight,
                              aParam.mTimestamp, aParam.mRenderTime_ms));
  }
};

template <>
struct ParamTraits<GMPSimulcastStream>
{
  typedef GMPSimulcastStream paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.mWidth);
    WriteParam(aMsg, aParam.mHeight);
    WriteParam(aMsg, aParam.mNumberOfTemporalLayers);
    WriteParam(aMsg, aParam.mMaxBitrate);
    WriteParam(aMsg, aParam.mTargetBitrate);
    WriteParam(aMsg, aParam.mMinBitrate);
    WriteParam(aMsg, aParam.mQPMax);
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    if (ReadParam(aMsg, aIter, &(aResult->mWidth)) &&
        ReadParam(aMsg, aIter, &(aResult->mHeight)) &&
        ReadParam(aMsg, aIter, &(aResult->mNumberOfTemporalLayers)) &&
        ReadParam(aMsg, aIter, &(aResult->mMaxBitrate)) &&
        ReadParam(aMsg, aIter, &(aResult->mTargetBitrate)) &&
        ReadParam(aMsg, aIter, &(aResult->mMinBitrate)) &&
        ReadParam(aMsg, aIter, &(aResult->mQPMax))) {
      return true;
    }
    return false;
  }

  static void Log(const paramType& aParam, std::wstring* aLog)
  {
    aLog->append(StringPrintf(L"[%u, %u, %u, %u, %u, %u, %u]", aParam.mWidth, aParam.mHeight,
                              aParam.mNumberOfTemporalLayers, aParam.mMaxBitrate,
                              aParam.mTargetBitrate, aParam.mMinBitrate, aParam.mQPMax));
  }
};

template <>
struct ParamTraits<GMPVideoCodec>
{
  typedef GMPVideoCodec paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, static_cast<int>(aParam.mCodecType));
    WriteParam(aMsg, nsAutoCString(aParam.mPLName));
    WriteParam(aMsg, aParam.mPLType);
    WriteParam(aMsg, aParam.mWidth);
    WriteParam(aMsg, aParam.mHeight);
    WriteParam(aMsg, aParam.mStartBitrate);
    WriteParam(aMsg, aParam.mMaxBitrate);
    WriteParam(aMsg, aParam.mMinBitrate);
    WriteParam(aMsg, aParam.mMaxFramerate);
    if (aParam.mCodecType == kGMPVideoCodecVP8) {
      WriteParam(aMsg, aParam.mCodecSpecific.mVP8);
    } else {
      MOZ_ASSERT(false, "Serializing unknown codec type!");
    }
    WriteParam(aMsg, aParam.mQPMax);
    WriteParam(aMsg, aParam.mNumberOfSimulcastStreams);
    for (uint32_t i = 0; i < aParam.mNumberOfSimulcastStreams; i++) {
      WriteParam(aMsg, aParam.mSimulcastStream[i]);
    }
    WriteParam(aMsg, static_cast<int>(aParam.mMode));
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    int codecType;
    if (!ReadParam(aMsg, aIter, &codecType)) {
      return false;
    }
    aResult->mCodecType = static_cast<GMPVideoCodecType>(codecType);

    nsAutoCString plName;
    if (!ReadParam(aMsg, aIter, &plName) ||
        plName.Length() > kGMPPayloadNameSize) {
      return false;
    }
    memset(aResult->mPLName, 0, kGMPPayloadNameSize);
    memcpy(aResult->mPLName, plName.get(), plName.Length());

    if (!ReadParam(aMsg, aIter, &(aResult->mPLType)) ||
        !ReadParam(aMsg, aIter, &(aResult->mWidth)) ||
        !ReadParam(aMsg, aIter, &(aResult->mHeight)) ||
        !ReadParam(aMsg, aIter, &(aResult->mStartBitrate)) ||
        !ReadParam(aMsg, aIter, &(aResult->mMaxBitrate)) ||
        !ReadParam(aMsg, aIter, &(aResult->mMinBitrate)) ||
        !ReadParam(aMsg, aIter, &(aResult->mMaxFramerate))) {
      return false;
    }

    if (aResult->mCodecType == kGMPVideoCodecVP8) {
      if (!ReadParam(aMsg, aIter, &(aResult->mCodecSpecific.mVP8))) {
        return false;
      }
    } else {
      MOZ_ASSERT(false, "De-serializing unknown codec type!");
      return false;
    }

    if (!ReadParam(aMsg, aIter, &(aResult->mQPMax)) ||
        !ReadParam(aMsg, aIter, &(aResult->mNumberOfSimulcastStreams))) {
      return false;
    }

    for (uint32_t i = 0; i < aResult->mNumberOfSimulcastStreams; i++) {
      if (!ReadParam(aMsg, aIter, &(aResult->mSimulcastStream[i]))) {
        return false;
      }
    }

    int mode;
    if (!ReadParam(aMsg, aIter, &mode)) {
      return false;
    }
    aResult->mMode = static_cast<GMPVideoCodecMode>(mode);

    return true;
  }

  static void Log(const paramType& aParam, std::wstring* aLog)
  {
    const char* codecName = nullptr;
    if (aParam.mCodecType == kGMPVideoCodecVP8) {
      codecName = "VP8";
    }
    aLog->append(StringPrintf(L"[%s, %u, %u]",
                              codecName,
                              aParam.mWidth,
                              aParam.mHeight));
  }
};

template <>
struct ParamTraits<mozilla::gmp::GMPVideoEncodedFrameImpl>
{
  typedef mozilla::gmp::GMPVideoEncodedFrameImpl paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.mEncodedWidth);
    WriteParam(aMsg, aParam.mEncodedHeight);
    WriteParam(aMsg, aParam.mTimeStamp);
    WriteParam(aMsg, aParam.mCaptureTime_ms);
    WriteParam(aMsg, static_cast<int>(aParam.mFrameType));
    WriteParam(aMsg, aParam.mSize);
    WriteParam(aMsg, aParam.mCompleteFrame);
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    if (!ReadParam(aMsg, aIter, &(aResult->mEncodedWidth)) ||
        !ReadParam(aMsg, aIter, &(aResult->mEncodedHeight)) ||
        !ReadParam(aMsg, aIter, &(aResult->mTimeStamp)) ||
        !ReadParam(aMsg, aIter, &(aResult->mCaptureTime_ms))) {
      return false;
    }

    int frameType;
    if (!ReadParam(aMsg, aIter, &frameType)) {
      return false;
    }
    aResult->mFrameType = static_cast<GMPVideoFrameType>(frameType);

    if (!ReadParam(aMsg, aIter, &(aResult->mSize)) ||
        !ReadParam(aMsg, aIter, &(aResult->mCompleteFrame))) {
      return false;
    }

    return true;
  }

  static void Log(const paramType& aParam, std::wstring* aLog)
  {
    /*
    aLog->append(StringPrintf(L"[%u, %u, %u, %u]", aParam.top, aParam.left,
                              aParam.bottom, aParam.right));
    */
  }
};

template <>
struct ParamTraits<GMPCodecSpecificInfoVP8>
{
  typedef GMPCodecSpecificInfoVP8 paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.mHasReceivedSLI);
    WriteParam(aMsg, aParam.mPictureIdSLI);
    WriteParam(aMsg, aParam.mHasReceivedRPSI);
    WriteParam(aMsg, aParam.mPictureIdRPSI);
    WriteParam(aMsg, aParam.mPictureId);
    WriteParam(aMsg, aParam.mNonReference);
    WriteParam(aMsg, aParam.mSimulcastIdx);
    WriteParam(aMsg, aParam.mTemporalIdx);
    WriteParam(aMsg, aParam.mLayerSync);
    WriteParam(aMsg, aParam.mTL0PicIdx);
    WriteParam(aMsg, aParam.mKeyIdx);
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    if (ReadParam(aMsg, aIter, &(aResult->mHasReceivedSLI)) &&
        ReadParam(aMsg, aIter, &(aResult->mPictureIdSLI)) &&
        ReadParam(aMsg, aIter, &(aResult->mHasReceivedRPSI)) &&
        ReadParam(aMsg, aIter, &(aResult->mPictureIdRPSI)) &&
        ReadParam(aMsg, aIter, &(aResult->mPictureId)) &&
        ReadParam(aMsg, aIter, &(aResult->mNonReference)) &&
        ReadParam(aMsg, aIter, &(aResult->mSimulcastIdx)) &&
        ReadParam(aMsg, aIter, &(aResult->mTemporalIdx)) &&
        ReadParam(aMsg, aIter, &(aResult->mLayerSync)) &&
        ReadParam(aMsg, aIter, &(aResult->mTL0PicIdx)) &&
        ReadParam(aMsg, aIter, &(aResult->mKeyIdx))) {
      return true;
    }
    return false;
  }

  static void Log(const paramType& aParam, std::wstring* aLog)
  {
    aLog->append(StringPrintf(L"[%d, %u, %d, %u, %d, %d, %u, %u, %d, %d, %d]",
                              aParam.mHasReceivedSLI,
                              aParam.mPictureIdSLI,
                              aParam.mHasReceivedRPSI,
                              aParam.mPictureIdRPSI,
                              aParam.mPictureId,
                              aParam.mNonReference,
                              aParam.mSimulcastIdx,
                              aParam.mTemporalIdx,
                              aParam.mLayerSync,
                              aParam.mTL0PicIdx,
                              aParam.mKeyIdx));
  }
};

template <>
struct ParamTraits<GMPCodecSpecificInfo>
{
  typedef GMPCodecSpecificInfo paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, static_cast<int>(aParam.mCodecType));
    if (aParam.mCodecType == kGMPVideoCodecVP8) {
      WriteParam(aMsg, aParam.mCodecSpecific.mVP8);
    } else {
      MOZ_ASSERT(false, "Serializing unknown codec type!");
    }
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    int codecType;
    if (!ReadParam(aMsg, aIter, &codecType)) {
      return false;
    }
    aResult->mCodecType = static_cast<GMPVideoCodecType>(codecType);

    if (aResult->mCodecType == kGMPVideoCodecVP8) {
      if (!ReadParam(aMsg, aIter, &(aResult->mCodecSpecific.mVP8))) {
        return false;
      }
    } else {
      MOZ_ASSERT(false, "De-serializing unknown codec type!");
      return false;
    }

    return true;
  }

  static void Log(const paramType& aParam, std::wstring* aLog)
  {
    const char* codecName = nullptr;
    if (aParam.mCodecType == kGMPVideoCodecVP8) {
      codecName = "VP8";
    }
    aLog->append(StringPrintf(L"[%s]", codecName));
  }
};

} // namespace IPC

#endif // GMPMessageUtils_h_
