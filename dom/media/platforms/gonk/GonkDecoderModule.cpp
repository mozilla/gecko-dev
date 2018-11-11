/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "GonkDecoderModule.h"
#include "GonkVideoDecoderManager.h"
#include "GonkAudioDecoderManager.h"
#include "mozilla/DebugOnly.h"
#include "GonkMediaDataDecoder.h"

namespace mozilla {
GonkDecoderModule::GonkDecoderModule()
{
}

GonkDecoderModule::~GonkDecoderModule()
{
}

already_AddRefed<MediaDataDecoder>
GonkDecoderModule::CreateVideoDecoder(const CreateDecoderParams& aParams)
{
  RefPtr<MediaDataDecoder> decoder =
  new GonkMediaDataDecoder(new GonkVideoDecoderManager(aParams.mImageContainer, aParams.VideoConfig()),
                           aParams.mCallback);
  return decoder.forget();
}

already_AddRefed<MediaDataDecoder>
GonkDecoderModule::CreateAudioDecoder(const CreateDecoderParams& aParams)
{
  RefPtr<MediaDataDecoder> decoder =
  new GonkMediaDataDecoder(new GonkAudioDecoderManager(aParams.AudioConfig()),
                           aParams.mCallback);
  return decoder.forget();
}

PlatformDecoderModule::ConversionRequired
GonkDecoderModule::DecoderNeedsConversion(const TrackInfo& aConfig) const
{
  if (aConfig.IsVideo()) {
    return ConversionRequired::kNeedAnnexB;
  } else {
    return ConversionRequired::kNeedNone;
  }
}

bool
GonkDecoderModule::SupportsMimeType(const nsACString& aMimeType,
                                    DecoderDoctorDiagnostics* aDiagnostics) const
{
  return aMimeType.EqualsLiteral("audio/mp4a-latm") ||
    aMimeType.EqualsLiteral("audio/3gpp") ||
    aMimeType.EqualsLiteral("audio/amr-wb") ||
    aMimeType.EqualsLiteral("audio/mpeg") ||
    aMimeType.EqualsLiteral("video/mp4") ||
    aMimeType.EqualsLiteral("video/mp4v-es") ||
    aMimeType.EqualsLiteral("video/avc") ||
    aMimeType.EqualsLiteral("video/3gpp");
}

} // namespace mozilla
