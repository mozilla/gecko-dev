/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AndroidEncoderModule.h"

#include "AndroidDataEncoder.h"

#include "mozilla/Logging.h"
#include "mozilla/java/HardwareCodecCapabilityUtilsWrappers.h"

using mozilla::media::EncodeSupport;
using mozilla::media::EncodeSupportSet;

namespace mozilla {
extern LazyLogModule sPEMLog;
#define AND_PEM_LOG(arg, ...)            \
  MOZ_LOG(                               \
      sPEMLog, mozilla::LogLevel::Debug, \
      ("AndroidEncoderModule(%p)::%s: " arg, this, __func__, ##__VA_ARGS__))

EncodeSupportSet AndroidEncoderModule::SupportsCodec(CodecType aCodec) const {
  EncodeSupportSet supports{};
  switch (aCodec) {
    case CodecType::H264:
      supports += EncodeSupport::SoftwareEncode;
      if (java::HardwareCodecCapabilityUtils::HasHWH264(true /* encoder */)) {
        supports += EncodeSupport::HardwareEncode;
      }
      break;
    case CodecType::VP8:
      if (java::HardwareCodecCapabilityUtils::HasHWVP8(true /* encoder */)) {
        supports += EncodeSupport::HardwareEncode;
      }
      break;
    case CodecType::VP9:
      if (java::HardwareCodecCapabilityUtils::HasHWVP9(true /* encoder */)) {
        supports += EncodeSupport::HardwareEncode;
      }
      break;
    default:
      break;
  }
  return supports;
}

EncodeSupportSet AndroidEncoderModule::Supports(
    const EncoderConfig& aConfig) const {
  if (!CanLikelyEncode(aConfig)) {
    return EncodeSupportSet{};
  }
  if (aConfig.mScalabilityMode != ScalabilityMode::None) {
    return EncodeSupportSet{};
  }
  return SupportsCodec(aConfig.mCodec);
}

already_AddRefed<MediaDataEncoder> AndroidEncoderModule::CreateVideoEncoder(
    const EncoderConfig& aConfig, const RefPtr<TaskQueue>& aTaskQueue) const {
  if (Supports(aConfig).isEmpty()) {
    AND_PEM_LOG("Unsupported codec type: %s",
                GetCodecTypeString(aConfig.mCodec));
    return nullptr;
  }
  return MakeRefPtr<AndroidDataEncoder>(aConfig, aTaskQueue).forget();
}

}  // namespace mozilla

#undef AND_PEM_LOG
