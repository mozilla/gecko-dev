/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WMFEncoderModule.h"

#include "WMFMediaDataEncoder.h"

using mozilla::media::EncodeSupportSet;

namespace mozilla {

extern LazyLogModule sPEMLog;

static EncodeSupportSet IsSupported(
    CodecType aCodecType, const gfx::IntSize& aFrameSize,
    const EncoderConfig::CodecSpecific& aCodecSpecific) {
  if (CodecToSubtype(aCodecType) == GUID_NULL) {
    return EncodeSupportSet{};
  }
  return CanCreateWMFEncoder(aCodecType, aFrameSize, aCodecSpecific);
}

EncodeSupportSet WMFEncoderModule::SupportsCodec(CodecType aCodecType) const {
  gfx::IntSize kDefaultSize(640, 480);
  EncoderConfig::CodecSpecific kDefaultCodecSpecific = AsVariant(void_t{});
  return IsSupported(aCodecType, kDefaultSize, kDefaultCodecSpecific);
}

EncodeSupportSet WMFEncoderModule::Supports(
    const EncoderConfig& aConfig) const {
  if (!CanLikelyEncode(aConfig)) {
    return EncodeSupportSet{};
  }
  if (aConfig.IsAudio()) {
    return EncodeSupportSet{};
  }
  if (aConfig.mScalabilityMode != ScalabilityMode::None &&
      aConfig.mCodec != CodecType::H264) {
    return EncodeSupportSet{};
  }
  return IsSupported(aConfig.mCodec, aConfig.mSize, aConfig.mCodecSpecific);
}

already_AddRefed<MediaDataEncoder> WMFEncoderModule::CreateVideoEncoder(
    const EncoderConfig& aConfig, const RefPtr<TaskQueue>& aTaskQueue) const {
  RefPtr<MediaDataEncoder> encoder(
      new WMFMediaDataEncoder(aConfig, aTaskQueue));
  return encoder.forget();
}

}  // namespace mozilla
