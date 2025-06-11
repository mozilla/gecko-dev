/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GMPEncoderModule.h"

#include "GMPService.h"
#include "GMPUtils.h"
#include "GMPVideoEncoder.h"
#include "MediaDataEncoderProxy.h"
#include "MP4Decoder.h"

using mozilla::media::EncodeSupport;
using mozilla::media::EncodeSupportSet;

namespace mozilla {

already_AddRefed<MediaDataEncoder> GMPEncoderModule::CreateVideoEncoder(
    const EncoderConfig& aConfig, const RefPtr<TaskQueue>& aTaskQueue) const {
  if (Supports(aConfig).isEmpty()) {
    return nullptr;
  }

  RefPtr<gmp::GeckoMediaPluginService> s(
      gmp::GeckoMediaPluginService::GetGeckoMediaPluginService());
  if (NS_WARN_IF(!s)) {
    return nullptr;
  }

  nsCOMPtr<nsISerialEventTarget> thread(s->GetGMPThread());
  if (NS_WARN_IF(!thread)) {
    return nullptr;
  }

  RefPtr<MediaDataEncoder> encoder(new GMPVideoEncoder(aConfig));
  return do_AddRef(
      new MediaDataEncoderProxy(encoder.forget(), thread.forget()));
}

media::EncodeSupportSet GMPEncoderModule::Supports(
    const EncoderConfig& aConfig) const {
  if (!CanLikelyEncode(aConfig)) {
    return EncodeSupportSet{};
  }
  if (aConfig.mCodec != CodecType::H264) {
    return EncodeSupportSet{};
  }
  if (aConfig.mHardwarePreference == HardwarePreference::RequireHardware) {
    return EncodeSupportSet{};
  }
  if (aConfig.mCodecSpecific && aConfig.mCodecSpecific->is<H264Specific>()) {
    const auto& codecSpecific = aConfig.mCodecSpecific->as<H264Specific>();
    if (codecSpecific.mProfile != H264_PROFILE_UNKNOWN &&
        codecSpecific.mProfile != H264_PROFILE_BASE &&
        !HaveGMPFor("encode-video"_ns, {"moz-h264-advanced"_ns})) {
      return EncodeSupportSet{};
    }
  }
  if (aConfig.mScalabilityMode != ScalabilityMode::None &&
      !HaveGMPFor("encode-video"_ns, {"moz-h264-temporal-svc"_ns})) {
    return EncodeSupportSet{};
  }
  if (!HaveGMPFor("encode-video"_ns, {"h264"_ns})) {
    return EncodeSupportSet{};
  }
  return EncodeSupportSet{EncodeSupport::SoftwareEncode};
}

media::EncodeSupportSet GMPEncoderModule::SupportsCodec(
    CodecType aCodecType) const {
  if (aCodecType != CodecType::H264) {
    return EncodeSupportSet{};
  }
  if (!HaveGMPFor("encode-video"_ns, {"h264"_ns})) {
    return EncodeSupportSet{};
  }
  return EncodeSupportSet{EncodeSupport::SoftwareEncode};
}

}  // namespace mozilla
