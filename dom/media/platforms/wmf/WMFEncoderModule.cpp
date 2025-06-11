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

EncodeSupportSet WMFEncoderModule::SupportsCodec(CodecType aCodecType) const {
  if (aCodecType > CodecType::_BeginAudio_ &&
      aCodecType < CodecType::_EndAudio_) {
    return EncodeSupportSet{};
  }
  return CanCreateWMFEncoder(aCodecType);
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
  return SupportsCodec(aConfig.mCodec);
}

already_AddRefed<MediaDataEncoder> WMFEncoderModule::CreateVideoEncoder(
    const EncoderConfig& aConfig, const RefPtr<TaskQueue>& aTaskQueue) const {
  RefPtr<MediaDataEncoder> encoder(
      new WMFMediaDataEncoder(aConfig, aTaskQueue));
  return encoder.forget();
}

}  // namespace mozilla
