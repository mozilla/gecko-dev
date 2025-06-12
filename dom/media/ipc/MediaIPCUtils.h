/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_media_MediaIPCUtils_h
#define mozilla_dom_media_MediaIPCUtils_h

#include <type_traits>

#include "DecoderDoctorDiagnostics.h"
#include "EncoderConfig.h"
#include "PerformanceRecorder.h"
#include "PlatformDecoderModule.h"
#include "PlatformEncoderModule.h"
#include "ipc/EnumSerializer.h"
#include "mozilla/EnumSet.h"
#include "mozilla/GfxMessageUtils.h"
#include "mozilla/Maybe.h"
#include "mozilla/ParamTraits_TiedFields.h"
#include "mozilla/gfx/Rect.h"
#include "mozilla/dom/MFCDMSerializers.h"

namespace IPC {
template <>
struct ParamTraits<mozilla::VideoInfo> {
  typedef mozilla::VideoInfo paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    // TrackInfo
    WriteParam(aWriter, aParam.mMimeType);

    // VideoInfo
    WriteParam(aWriter, aParam.mDisplay);
    WriteParam(aWriter, aParam.mStereoMode);
    WriteParam(aWriter, aParam.mImage);
    WriteParam(aWriter, aParam.mImageRect);
    WriteParam(aWriter, *aParam.mCodecSpecificConfig);
    WriteParam(aWriter, *aParam.mExtraData);
    WriteParam(aWriter, aParam.mRotation);
    WriteParam(aWriter, aParam.mColorDepth);
    WriteParam(aWriter, aParam.mColorSpace);
    WriteParam(aWriter, aParam.mColorPrimaries);
    WriteParam(aWriter, aParam.mTransferFunction);
    WriteParam(aWriter, aParam.mColorRange);
    WriteParam(aWriter, aParam.HasAlpha());
    WriteParam(aWriter, aParam.mCrypto);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    mozilla::gfx::IntRect imageRect;
    bool alphaPresent;
    if (ReadParam(aReader, &aResult->mMimeType) &&
        ReadParam(aReader, &aResult->mDisplay) &&
        ReadParam(aReader, &aResult->mStereoMode) &&
        ReadParam(aReader, &aResult->mImage) &&
        ReadParam(aReader, &aResult->mImageRect) &&
        ReadParam(aReader, aResult->mCodecSpecificConfig.get()) &&
        ReadParam(aReader, aResult->mExtraData.get()) &&
        ReadParam(aReader, &aResult->mRotation) &&
        ReadParam(aReader, &aResult->mColorDepth) &&
        ReadParam(aReader, &aResult->mColorSpace) &&
        ReadParam(aReader, &aResult->mColorPrimaries) &&
        ReadParam(aReader, &aResult->mTransferFunction) &&
        ReadParam(aReader, &aResult->mColorRange) &&
        ReadParam(aReader, &alphaPresent) &&
        ReadParam(aReader, &aResult->mCrypto)) {
      aResult->SetAlpha(alphaPresent);
      return true;
    }
    return false;
  }
};

template <>
struct ParamTraits<mozilla::TrackInfo::TrackType>
    : public ContiguousEnumSerializerInclusive<
          mozilla::TrackInfo::TrackType,
          mozilla::TrackInfo::TrackType::kUndefinedTrack,
          mozilla::TrackInfo::TrackType::kTextTrack> {};

template <>
struct ParamTraits<mozilla::VideoRotation>
    : public ContiguousEnumSerializerInclusive<
          mozilla::VideoRotation, mozilla::VideoRotation::kDegree_0,
          mozilla::VideoRotation::kDegree_270> {};

template <>
struct ParamTraits<mozilla::MediaByteBuffer>
    : public ParamTraits<nsTArray<uint8_t>> {
  typedef mozilla::MediaByteBuffer paramType;
};

// Traits for AudioCodecSpecificVariant types.

template <>
struct ParamTraits<mozilla::NoCodecSpecificData>
    : public EmptyStructSerializer<mozilla::NoCodecSpecificData> {};

template <>
struct ParamTraits<mozilla::AudioCodecSpecificBinaryBlob> {
  using paramType = mozilla::AudioCodecSpecificBinaryBlob;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, *aParam.mBinaryBlob);
  }
  static bool Read(MessageReader* aReader, paramType* aResult) {
    return ReadParam(aReader, aResult->mBinaryBlob.get());
  }
};

template <>
struct ParamTraits<mozilla::AacCodecSpecificData> {
  using paramType = mozilla::AacCodecSpecificData;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, *aParam.mEsDescriptorBinaryBlob);
    WriteParam(aWriter, *aParam.mDecoderConfigDescriptorBinaryBlob);
    WriteParam(aWriter, aParam.mEncoderDelayFrames);
    WriteParam(aWriter, aParam.mMediaFrameCount);
  }
  static bool Read(MessageReader* aReader, paramType* aResult) {
    return ReadParam(aReader, aResult->mEsDescriptorBinaryBlob.get()) &&
           ReadParam(aReader,
                     aResult->mDecoderConfigDescriptorBinaryBlob.get()) &&
           ReadParam(aReader, &aResult->mEncoderDelayFrames) &&
           ReadParam(aReader, &aResult->mMediaFrameCount);
  }
};

template <>
struct ParamTraits<mozilla::FlacCodecSpecificData> {
  using paramType = mozilla::FlacCodecSpecificData;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, *aParam.mStreamInfoBinaryBlob);
  }
  static bool Read(MessageReader* aReader, paramType* aResult) {
    return ReadParam(aReader, aResult->mStreamInfoBinaryBlob.get());
  }
};

template <>
struct ParamTraits<mozilla::Mp3CodecSpecificData>
    : public ParamTraits_TiedFields<mozilla::Mp3CodecSpecificData> {};

template <>
struct ParamTraits<mozilla::OpusCodecSpecificData> {
  using paramType = mozilla::OpusCodecSpecificData;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.mContainerCodecDelayFrames);
    WriteParam(aWriter, *aParam.mHeadersBinaryBlob);
  }
  static bool Read(MessageReader* aReader, paramType* aResult) {
    return ReadParam(aReader, &aResult->mContainerCodecDelayFrames) &&
           ReadParam(aReader, aResult->mHeadersBinaryBlob.get());
  }
};

template <>
struct ParamTraits<mozilla::VorbisCodecSpecificData> {
  using paramType = mozilla::VorbisCodecSpecificData;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, *aParam.mHeadersBinaryBlob);
  }
  static bool Read(MessageReader* aReader, paramType* aResult) {
    return ReadParam(aReader, aResult->mHeadersBinaryBlob.get());
  }
};

template <>
struct ParamTraits<mozilla::WaveCodecSpecificData>
    : public EmptyStructSerializer<mozilla::WaveCodecSpecificData> {};

// End traits for AudioCodecSpecificVariant types.

template <>
struct ParamTraits<mozilla::AudioInfo> {
  typedef mozilla::AudioInfo paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    // TrackInfo
    WriteParam(aWriter, aParam.mMimeType);

    // AudioInfo
    WriteParam(aWriter, aParam.mRate);
    WriteParam(aWriter, aParam.mChannels);
    WriteParam(aWriter, aParam.mChannelMap);
    WriteParam(aWriter, aParam.mBitDepth);
    WriteParam(aWriter, aParam.mProfile);
    WriteParam(aWriter, aParam.mExtendedProfile);
    WriteParam(aWriter, aParam.mCodecSpecificConfig);
    WriteParam(aWriter, aParam.mCrypto);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    if (ReadParam(aReader, &aResult->mMimeType) &&
        ReadParam(aReader, &aResult->mRate) &&
        ReadParam(aReader, &aResult->mChannels) &&
        ReadParam(aReader, &aResult->mChannelMap) &&
        ReadParam(aReader, &aResult->mBitDepth) &&
        ReadParam(aReader, &aResult->mProfile) &&
        ReadParam(aReader, &aResult->mExtendedProfile) &&
        ReadParam(aReader, &aResult->mCodecSpecificConfig) &&
        ReadParam(aReader, &aResult->mCrypto)) {
      return true;
    }
    return false;
  }
};

template <>
struct ParamTraits<mozilla::MediaDataDecoder::ConversionRequired>
    : public ContiguousEnumSerializerInclusive<
          mozilla::MediaDataDecoder::ConversionRequired,
          mozilla::MediaDataDecoder::ConversionRequired(0),
          mozilla::MediaDataDecoder::ConversionRequired(
              mozilla::MediaDataDecoder::ConversionRequired::kNeedHVCC)> {};

template <>
struct ParamTraits<mozilla::media::TimeUnit> {
  using paramType = mozilla::media::TimeUnit;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.IsValid());
    WriteParam(aWriter, aParam.IsValid() ? aParam.mTicks.value() : 0);
    WriteParam(aWriter,
               aParam.IsValid() ? aParam.mBase : 1);  // base can't be 0
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    bool valid;
    int64_t ticks;
    int64_t base;
    if (ReadParam(aReader, &valid) && ReadParam(aReader, &ticks) &&
        ReadParam(aReader, &base)) {
      if (valid) {
        *aResult = mozilla::media::TimeUnit(ticks, base);
      } else {
        *aResult = mozilla::media::TimeUnit::Invalid();
      }
      return true;
    }
    return false;
  };
};

template <>
struct ParamTraits<mozilla::media::TimeInterval> {
  typedef mozilla::media::TimeInterval paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.mStart);
    WriteParam(aWriter, aParam.mEnd);
    WriteParam(aWriter, aParam.mFuzz);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    if (ReadParam(aReader, &aResult->mStart) &&
        ReadParam(aReader, &aResult->mEnd) &&
        ReadParam(aReader, &aResult->mFuzz)) {
      return true;
    }
    return false;
  }
};

template <>
struct ParamTraits<mozilla::MediaResult> {
  typedef mozilla::MediaResult paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.Code());
    WriteParam(aWriter, aParam.Message());
    WriteParam(aWriter, aParam.GetPlatformErrorCode());
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    nsresult result;
    nsCString message;
    mozilla::Maybe<int32_t> platformErrorCode;
    if (ReadParam(aReader, &result) && ReadParam(aReader, &message) &&
        ReadParam(aReader, &platformErrorCode)) {
      *aResult = paramType(result, std::move(message), platformErrorCode);
      return true;
    }
    return false;
  };
};

template <>
struct ParamTraits<mozilla::DecoderDoctorDiagnostics> {
  typedef mozilla::DecoderDoctorDiagnostics paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.mDiagnosticsType);
    WriteParam(aWriter, aParam.mFormat);
    WriteParam(aWriter, aParam.mFlags);
    WriteParam(aWriter, aParam.mEvent);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    if (ReadParam(aReader, &aResult->mDiagnosticsType) &&
        ReadParam(aReader, &aResult->mFormat) &&
        ReadParam(aReader, &aResult->mFlags) &&
        ReadParam(aReader, &aResult->mEvent)) {
      return true;
    }
    return false;
  };
};

template <>
struct ParamTraits<mozilla::DecoderDoctorDiagnostics::DiagnosticsType>
    : public ContiguousEnumSerializerInclusive<
          mozilla::DecoderDoctorDiagnostics::DiagnosticsType,
          mozilla::DecoderDoctorDiagnostics::DiagnosticsType::eUnsaved,
          mozilla::DecoderDoctorDiagnostics::DiagnosticsType::eDecodeWarning> {
};

template <>
struct ParamTraits<mozilla::DecoderDoctorEvent> {
  typedef mozilla::DecoderDoctorEvent paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    int domain = aParam.mDomain;
    WriteParam(aWriter, domain);
    WriteParam(aWriter, aParam.mResult);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    int domain = 0;
    if (ReadParam(aReader, &domain) && ReadParam(aReader, &aResult->mResult)) {
      aResult->mDomain = paramType::Domain(domain);
      return true;
    }
    return false;
  };
};

template <>
struct ParamTraits<mozilla::TrackingId::Source>
    : public ContiguousEnumSerializer<
          mozilla::TrackingId::Source,
          mozilla::TrackingId::Source::Unimplemented,
          mozilla::TrackingId::Source::LAST> {};

template <>
struct ParamTraits<mozilla::TrackingId> {
  typedef mozilla::TrackingId paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.mSource);
    WriteParam(aWriter, aParam.mProcId);
    WriteParam(aWriter, aParam.mUniqueInProcId);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    return ReadParam(aReader, &aResult->mSource) &&
           ReadParam(aReader, &aResult->mProcId) &&
           ReadParam(aReader, &aResult->mUniqueInProcId);
  }
};

template <>
struct ParamTraits<mozilla::CryptoTrack> {
  typedef mozilla::CryptoTrack paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.mCryptoScheme);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    return ReadParam(aReader, &aResult->mCryptoScheme);
  }
};

template <>
struct ParamTraits<mozilla::dom::ImageBitmapFormat>
    : public ContiguousEnumSerializerInclusive<
          mozilla::dom::ImageBitmapFormat,
          mozilla::dom::ImageBitmapFormat::RGBA32,
          mozilla::dom::ImageBitmapFormat::DEPTH> {};

template <>
struct ParamTraits<mozilla::CodecType>
    : public ContiguousEnumSerializerInclusive<mozilla::CodecType,
                                               mozilla::CodecType::_BeginVideo_,
                                               mozilla::CodecType::Unknown> {};

template <>
struct ParamTraits<mozilla::BitrateMode>
    : public ContiguousEnumSerializerInclusive<mozilla::BitrateMode,
                                               mozilla::BitrateMode::Constant,
                                               mozilla::BitrateMode::Variable> {
};

template <>
struct ParamTraits<mozilla::ScalabilityMode>
    : public ContiguousEnumSerializerInclusive<mozilla::ScalabilityMode,
                                               mozilla::ScalabilityMode::None,
                                               mozilla::ScalabilityMode::L1T3> {
};

template <>
struct ParamTraits<mozilla::H264BitStreamFormat>
    : public ContiguousEnumSerializerInclusive<
          mozilla::H264BitStreamFormat, mozilla::H264BitStreamFormat::AVC,
          mozilla::H264BitStreamFormat::ANNEXB> {};

template <>
struct ParamTraits<mozilla::HardwarePreference>
    : public ContiguousEnumSerializerInclusive<
          mozilla::HardwarePreference,
          mozilla::HardwarePreference::RequireHardware,
          mozilla::HardwarePreference::None> {};

template <>
struct ParamTraits<mozilla::Usage>
    : public ContiguousEnumSerializerInclusive<
          mozilla::Usage, mozilla::Usage::Realtime, mozilla::Usage::Record> {};

template <>
struct ParamTraits<mozilla::H264_PROFILE>
    : public ContiguousEnumSerializerInclusive<
          mozilla::H264_PROFILE, mozilla::H264_PROFILE::H264_PROFILE_UNKNOWN,
          mozilla::H264_PROFILE::H264_PROFILE_HIGH> {};

template <>
struct ParamTraits<mozilla::H264_LEVEL>
    : public ContiguousEnumSerializerInclusive<
          mozilla::H264_LEVEL, mozilla::H264_LEVEL::H264_LEVEL_1,
          mozilla::H264_LEVEL::H264_LEVEL_6_2> {};

template <>
struct ParamTraits<mozilla::OpusBitstreamFormat>
    : public ContiguousEnumSerializerInclusive<
          mozilla::OpusBitstreamFormat, mozilla::OpusBitstreamFormat::Opus,
          mozilla::OpusBitstreamFormat::OGG> {};

template <>
struct ParamTraits<mozilla::OpusSpecific::Application>
    : public ContiguousEnumSerializerInclusive<
          mozilla::OpusSpecific::Application,
          mozilla::OpusSpecific::Application::Unspecified,
          mozilla::OpusSpecific::Application::RestricedLowDelay> {};

template <>
struct ParamTraits<mozilla::VPXComplexity>
    : public ContiguousEnumSerializerInclusive<mozilla::VPXComplexity,
                                               mozilla::VPXComplexity::Normal,
                                               mozilla::VPXComplexity::Max> {};

template <>
struct ParamTraits<struct mozilla::H264Specific> {
  typedef mozilla::H264Specific paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.mProfile);
    WriteParam(aWriter, aParam.mLevel);
    WriteParam(aWriter, aParam.mFormat);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    return ReadParam(aReader, &aResult->mProfile) &&
           ReadParam(aReader, &aResult->mLevel) &&
           ReadParam(aReader, &aResult->mFormat);
  }
};

template <>
struct ParamTraits<struct mozilla::OpusSpecific> {
  typedef mozilla::OpusSpecific paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.mApplication);
    WriteParam(aWriter, aParam.mFrameDuration);
    WriteParam(aWriter, aParam.mComplexity);
    WriteParam(aWriter, aParam.mFormat);
    WriteParam(aWriter, aParam.mPacketLossPerc);
    WriteParam(aWriter, aParam.mUseInBandFEC);
    WriteParam(aWriter, aParam.mUseDTX);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    return ReadParam(aReader, &aResult->mApplication) &&
           ReadParam(aReader, &aResult->mFrameDuration) &&
           ReadParam(aReader, &aResult->mComplexity) &&
           ReadParam(aReader, &aResult->mFormat) &&
           ReadParam(aReader, &aResult->mPacketLossPerc) &&
           ReadParam(aReader, &aResult->mUseInBandFEC) &&
           ReadParam(aReader, &aResult->mUseDTX);
  }
};

template <>
struct ParamTraits<struct mozilla::VP8Specific> {
  typedef mozilla::VP8Specific paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.mComplexity);
    WriteParam(aWriter, aParam.mResilience);
    WriteParam(aWriter, aParam.mNumTemporalLayers);
    WriteParam(aWriter, aParam.mDenoising);
    WriteParam(aWriter, aParam.mAutoResize);
    WriteParam(aWriter, aParam.mFrameDropping);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    return ReadParam(aReader, &aResult->mComplexity) &&
           ReadParam(aReader, &aResult->mResilience) &&
           ReadParam(aReader, &aResult->mNumTemporalLayers) &&
           ReadParam(aReader, &aResult->mDenoising) &&
           ReadParam(aReader, &aResult->mAutoResize) &&
           ReadParam(aReader, &aResult->mFrameDropping);
  }
};

template <>
struct ParamTraits<struct mozilla::VP9Specific> {
  typedef mozilla::VP9Specific paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    ParamTraits<mozilla::VP8Specific>::Write(aWriter, aParam);
    WriteParam(aWriter, aParam.mAdaptiveQp);
    WriteParam(aWriter, aParam.mNumSpatialLayers);
    WriteParam(aWriter, aParam.mFlexible);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    return ParamTraits<mozilla::VP8Specific>::Read(aReader, aResult) &&
           ReadParam(aReader, &aResult->mAdaptiveQp) &&
           ReadParam(aReader, &aResult->mNumSpatialLayers) &&
           ReadParam(aReader, &aResult->mFlexible);
  }
};

template <>
struct ParamTraits<struct mozilla::EncoderConfig::VideoColorSpace> {
  typedef mozilla::EncoderConfig::VideoColorSpace paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.mRange);
    WriteParam(aWriter, aParam.mMatrix);
    WriteParam(aWriter, aParam.mPrimaries);
    WriteParam(aWriter, aParam.mTransferFunction);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    return ReadParam(aReader, &aResult->mRange) &&
           ReadParam(aReader, &aResult->mMatrix) &&
           ReadParam(aReader, &aResult->mPrimaries) &&
           ReadParam(aReader, &aResult->mTransferFunction);
  }
};

template <>
struct ParamTraits<struct mozilla::EncoderConfig::SampleFormat> {
  typedef mozilla::EncoderConfig::SampleFormat paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.mPixelFormat);
    WriteParam(aWriter, aParam.mColorSpace);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    return ReadParam(aReader, &aResult->mPixelFormat) &&
           ReadParam(aReader, &aResult->mColorSpace);
  }
};

template <>
struct ParamTraits<mozilla::EncoderConfig> {
  typedef mozilla::EncoderConfig paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.mCodec);
    WriteParam(aWriter, aParam.mSize);
    WriteParam(aWriter, aParam.mBitrateMode);
    WriteParam(aWriter, aParam.mBitrate);
    WriteParam(aWriter, aParam.mMinBitrate);
    WriteParam(aWriter, aParam.mMaxBitrate);
    WriteParam(aWriter, aParam.mUsage);
    WriteParam(aWriter, aParam.mHardwarePreference);
    WriteParam(aWriter, aParam.mFormat);
    WriteParam(aWriter, aParam.mScalabilityMode);
    WriteParam(aWriter, aParam.mFramerate);
    WriteParam(aWriter, aParam.mKeyframeInterval);
    WriteParam(aWriter, aParam.mNumberOfChannels);
    WriteParam(aWriter, aParam.mSampleRate);
    WriteParam(aWriter, aParam.mCodecSpecific);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    return ReadParam(aReader, &aResult->mCodec) &&
           ReadParam(aReader, &aResult->mSize) &&
           ReadParam(aReader, &aResult->mBitrateMode) &&
           ReadParam(aReader, &aResult->mBitrate) &&
           ReadParam(aReader, &aResult->mMinBitrate) &&
           ReadParam(aReader, &aResult->mMaxBitrate) &&
           ReadParam(aReader, &aResult->mUsage) &&
           ReadParam(aReader, &aResult->mHardwarePreference) &&
           ReadParam(aReader, &aResult->mFormat) &&
           ReadParam(aReader, &aResult->mScalabilityMode) &&
           ReadParam(aReader, &aResult->mFramerate) &&
           ReadParam(aReader, &aResult->mKeyframeInterval) &&
           ReadParam(aReader, &aResult->mNumberOfChannels) &&
           ReadParam(aReader, &aResult->mSampleRate) &&
           ReadParam(aReader, &aResult->mCodecSpecific);
  }
};

template <typename T, typename Phantom>
struct ParamTraits<struct mozilla::StrongTypedef<T, Phantom>> {
  typedef mozilla::StrongTypedef<T, Phantom> paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.mValue);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    return ReadParam(aReader, &aResult->mValue);
  }
};

// [RefCounted] typed
template <>
struct ParamTraits<mozilla::EncoderConfigurationChangeList*> {
  typedef mozilla::EncoderConfigurationChangeList paramType;

  static void Write(MessageWriter* aWriter, const paramType* aParam) {
    WriteParam(aWriter, aParam->mChanges);
  }

  static bool Read(MessageReader* aReader, RefPtr<paramType>* aResult) {
    auto result = mozilla::MakeRefPtr<paramType>();
    if (!ReadParam(aReader, &result->mChanges)) {
      return false;
    }
    *aResult = std::move(result);
    return true;
  }
};

}  // namespace IPC

#endif  // mozilla_dom_media_MediaIPCUtils_h
