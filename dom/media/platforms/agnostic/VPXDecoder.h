/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#if !defined(VPXDecoder_h_)
#define VPXDecoder_h_

#include "PlatformDecoderModule.h"
#include "mozilla/Span.h"

#include <stdint.h>
#define VPX_DONT_DEFINE_STDINT_TYPES
#include "vpx/vp8dx.h"
#include "vpx/vpx_codec.h"
#include "vpx/vpx_decoder.h"

namespace mozilla {

DDLoggedTypeDeclNameAndBase(VPXDecoder, MediaDataDecoder);

class VPXDecoder
  : public MediaDataDecoder
  , public DecoderDoctorLifeLogger<VPXDecoder>
{
public:
  explicit VPXDecoder(const CreateDecoderParams& aParams);

  RefPtr<InitPromise> Init() override;
  RefPtr<DecodePromise> Decode(MediaRawData* aSample) override;
  RefPtr<DecodePromise> Drain() override;
  RefPtr<FlushPromise> Flush() override;
  RefPtr<ShutdownPromise> Shutdown() override;
  nsCString GetDescriptionName() const override
  {
    return NS_LITERAL_CSTRING("libvpx video decoder");
  }

  enum Codec: uint8_t
  {
    VP8 = 1 << 0,
    VP9 = 1 << 1,
    Unknown = 1 << 7,
  };

  // Return true if aMimeType is a one of the strings used by our demuxers to
  // identify VPX of the specified type. Does not parse general content type
  // strings, i.e. white space matters.
  static bool IsVPX(const nsACString& aMimeType, uint8_t aCodecMask=VP8|VP9);
  static bool IsVP8(const nsACString& aMimeType);
  static bool IsVP9(const nsACString& aMimeType);

  // Return true if a sample is a keyframe for the specified codec.
  static bool IsKeyframe(Span<const uint8_t> aBuffer, Codec aCodec);

  // Return the frame dimensions for a sample for the specified codec.
  static gfx::IntSize GetFrameSize(Span<const uint8_t> aBuffer, Codec aCodec);

  // Return the VP9 profile as per https://www.webmproject.org/vp9/profiles/
  // Return negative value if error.
  static int GetVP9Profile(Span<const uint8_t> aBuffer);

private:
  ~VPXDecoder();
  RefPtr<DecodePromise> ProcessDecode(MediaRawData* aSample);
  MediaResult DecodeAlpha(vpx_image_t** aImgAlpha, const MediaRawData* aSample);

  const RefPtr<layers::ImageContainer> mImageContainer;
  RefPtr<layers::KnowsCompositor> mImageAllocator;
  const RefPtr<TaskQueue> mTaskQueue;

  // VPx decoder state
  vpx_codec_ctx_t mVPX;

  // VPx alpha decoder state
  vpx_codec_ctx_t mVPXAlpha;

  const VideoInfo& mInfo;

  const Codec mCodec;
  const bool mLowLatency;
};

} // namespace mozilla

#endif
