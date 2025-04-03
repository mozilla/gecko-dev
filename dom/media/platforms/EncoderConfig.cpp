/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EncoderConfig.h"
#include "ImageContainer.h"
#include "MP4Decoder.h"
#include "VPXDecoder.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/ImageUtils.h"

namespace mozilla {

CodecType EncoderConfig::CodecTypeForMime(const nsACString& aMimeType) {
  if (MP4Decoder::IsH264(aMimeType)) {
    return CodecType::H264;
  }
  if (VPXDecoder::IsVPX(aMimeType, VPXDecoder::VP8)) {
    return CodecType::VP8;
  }
  if (VPXDecoder::IsVPX(aMimeType, VPXDecoder::VP9)) {
    return CodecType::VP9;
  }
  MOZ_ASSERT_UNREACHABLE("Unsupported Mimetype");
  return CodecType::Unknown;
}

const char* CodecTypeStrings[] = {
    "BeginVideo", "H264", "VP8", "VP9",  "EndVideo", "Opus",   "Vorbis",
    "Flac",       "AAC",  "PCM", "G722", "EndAudio", "Unknown"};

nsCString EncoderConfig::ToString() const {
  nsCString rv;
  rv.Append(CodecTypeStrings[UnderlyingValue(mCodec)]);
  rv.AppendLiteral(mBitrateMode == BitrateMode::Constant ? " (CBR)" : " (VBR)");
  rv.AppendPrintf("%" PRIu32 "bps", mBitrate);
  if (mUsage == Usage::Realtime) {
    rv.AppendLiteral(", realtime");
  } else {
    rv.AppendLiteral(", record");
  }
  if (mCodec > CodecType::_BeginVideo_ && mCodec < CodecType::_EndVideo_) {
    rv.AppendPrintf(" [%dx%d]", mSize.Width(), mSize.Height());
    if (mHardwarePreference == HardwarePreference::RequireHardware) {
      rv.AppendLiteral(", hw required");
    } else if (mHardwarePreference == HardwarePreference::RequireSoftware) {
      rv.AppendLiteral(", sw required");
    } else {
      rv.AppendLiteral(", hw: no preference");
    }
    rv.AppendPrintf(
        " format: %s (%s-range)", GetEnumString(mFormat.mPixelFormat).get(),
        mFormat.mColorRange == gfx::ColorRange::FULL ? "full" : "limited");
    if (mScalabilityMode == ScalabilityMode::L1T2) {
      rv.AppendLiteral(" (L1T2)");
    } else if (mScalabilityMode == ScalabilityMode::L1T3) {
      rv.AppendLiteral(" (L1T2)");
    }
    rv.AppendPrintf(", fps: %" PRIu8, mFramerate);
    rv.AppendPrintf(", kf interval: %zu", mKeyframeInterval);
  } else {
    rv.AppendPrintf(", ch: %" PRIu32 ", %" PRIu32 "Hz", mNumberOfChannels,
                    mSampleRate);
  }
  rv.AppendPrintf("(w/%s codec specific)", mCodecSpecific ? "" : "o");
  return rv;
};

nsCString EncoderConfig::SampleFormat::ToString() const {
  nsCString str;
  str.AppendPrintf("SampleFormat: %s", dom::GetEnumString(mPixelFormat).get());
  if (mColorRange == gfx::ColorRange::FULL) {
    str.AppendLiteral(" (full-range)");
  } else {
    str.AppendLiteral(" (limited-range)");
  }
  return str;
}

Result<EncoderConfig::SampleFormat, MediaResult>
EncoderConfig::SampleFormat::FromImage(layers::Image* aImage) {
  if (!aImage) {
    return Err(MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR, "No image"));
  }

  const dom::ImageUtils imageUtils(aImage);
  Maybe<dom::ImageBitmapFormat> format = imageUtils.GetFormat();
  if (format.isNothing()) {
    return Err(
        MediaResult(NS_ERROR_NOT_IMPLEMENTED,
                    nsPrintfCString("unsupported image format: %d",
                                    static_cast<int>(aImage->GetFormat()))));
  }

  if (layers::PlanarYCbCrImage* image = aImage->AsPlanarYCbCrImage()) {
    if (const layers::PlanarYCbCrImage::Data* yuv = image->GetData()) {
      return EncoderConfig::SampleFormat{format.ref(), yuv->mColorRange};
    }
    return Err(MediaResult(NS_ERROR_UNEXPECTED,
                           "failed to get YUV data from a YUV image"));
  }

  return EncoderConfig::SampleFormat{format.ref(), gfx::ColorRange::FULL};
}

}  // namespace mozilla
