/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOCK_DECODER_MODULE_H_
#define MOCK_DECODER_MODULE_H_

#include "BlankDecoderModule.h"
#include "DummyMediaDataDecoder.h"
#include "PlatformDecoderModule.h"
#include "gmock/gmock.h"

namespace mozilla {

class MockVideoDataDecoder : public DummyMediaDataDecoder {
 public:
  explicit MockVideoDataDecoder(const CreateDecoderParams& aParams)
      : DummyMediaDataDecoder(
            MakeUnique<BlankVideoDataCreator>(
                aParams.VideoConfig().mDisplay.width,
                aParams.VideoConfig().mDisplay.height, aParams.mImageContainer),
            "MockVideoDataDecoder"_ns, aParams) {
    // A non-owning reference is captured to allow deletion of the node.
    // The method is called only when the caller holds a reference.
    ON_CALL(*this, Drain).WillByDefault([self = MOZ_KnownLive(this)]() {
      return self->DummyMediaDataDecoder::Drain();
    });
  }

  MOCK_METHOD(RefPtr<DecodePromise>, Drain, (), (override));

  void SetLatencyFrameCount(uint32_t aLatency) { mMaxRefFrames = aLatency; }

 protected:
  ~MockVideoDataDecoder() override = default;
};

class MockDecoderModule : public PlatformDecoderModule {
 public:
  MockDecoderModule() {
    ON_CALL(*this, SupportsMimeType)
        .WillByDefault(testing::Return(media::DecodeSupport::SoftwareDecode));
  }

  MOCK_METHOD(already_AddRefed<MediaDataDecoder>, CreateVideoDecoder,
              (const CreateDecoderParams& aParams), (override));

  MOCK_METHOD(already_AddRefed<MediaDataDecoder>, CreateAudioDecoder,
              (const CreateDecoderParams& aParams), (override));

  MOCK_METHOD(media::DecodeSupportSet, SupportsMimeType,
              (const nsACString& aMimeType,
               DecoderDoctorDiagnostics* aDiagnostics),
              (const override));

 protected:
  ~MockDecoderModule() override = default;
};

}  // namespace mozilla

#endif /* MOCK_DECODER_MODULE_H_ */
