/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VPXDecoder.h"
#include "gfx2DGlue.h"
#include "nsError.h"
#include "TimeUnits.h"
#include "mozilla/PodOperations.h"
#include "mozilla/SyncRunnable.h"
#include "prsystem.h"

#include <algorithm>

#undef LOG
#define LOG(arg, ...) MOZ_LOG(sPDMLog, mozilla::LogLevel::Debug, ("VPXDecoder(%p)::%s: " arg, this, __func__, ##__VA_ARGS__))

namespace mozilla {

using namespace gfx;
using namespace layers;

static int MimeTypeToCodec(const nsACString& aMimeType)
{
  if (aMimeType.EqualsLiteral("video/webm; codecs=vp8")) {
    return VPXDecoder::Codec::VP8;
  } else if (aMimeType.EqualsLiteral("video/webm; codecs=vp9")) {
    return VPXDecoder::Codec::VP9;
  } else if (aMimeType.EqualsLiteral("video/vp9")) {
    return VPXDecoder::Codec::VP9;
  }
  return -1;
}

VPXDecoder::VPXDecoder(const CreateDecoderParams& aParams)
  : mImageContainer(aParams.mImageContainer)
  , mTaskQueue(aParams.mTaskQueue)
  , mCallback(aParams.mCallback)
  , mIsFlushing(false)
  , mInfo(aParams.VideoConfig())
  , mCodec(MimeTypeToCodec(aParams.VideoConfig().mMimeType))
{
  MOZ_COUNT_CTOR(VPXDecoder);
  PodZero(&mVPX);
}

VPXDecoder::~VPXDecoder()
{
  MOZ_COUNT_DTOR(VPXDecoder);
}

void
VPXDecoder::Shutdown()
{
  vpx_codec_destroy(&mVPX);
}

RefPtr<MediaDataDecoder::InitPromise>
VPXDecoder::Init()
{
  int decode_threads = 2;

  vpx_codec_iface_t* dx = nullptr;
  if (mCodec == Codec::VP8) {
    dx = vpx_codec_vp8_dx();
  } else if (mCodec == Codec::VP9) {
    dx = vpx_codec_vp9_dx();
    if (mInfo.mDisplay.width >= 2048) {
      decode_threads = 8;
    } else if (mInfo.mDisplay.width >= 1024) {
      decode_threads = 4;
    }
  }
  decode_threads = std::min(decode_threads, PR_GetNumberOfProcessors());

  vpx_codec_dec_cfg_t config;
  config.threads = decode_threads;
  config.w = config.h = 0; // set after decode

  if (!dx || vpx_codec_dec_init(&mVPX, dx, &config, 0)) {
    return InitPromise::CreateAndReject(NS_ERROR_DOM_MEDIA_FATAL_ERR, __func__);
  }
  return InitPromise::CreateAndResolve(TrackInfo::kVideoTrack, __func__);
}

void
VPXDecoder::Flush()
{
  MOZ_ASSERT(mCallback->OnReaderTaskQueue());
  mIsFlushing = true;
  nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction([this] () {
    // nothing to do for now.
  });
  SyncRunnable::DispatchToThread(mTaskQueue, r);
  mIsFlushing = false;
}

MediaResult
VPXDecoder::DoDecode(MediaRawData* aSample)
{
  MOZ_ASSERT(mTaskQueue->IsCurrentThreadIn());
#if defined(DEBUG)
  vpx_codec_stream_info_t si;
  PodZero(&si);
  si.sz = sizeof(si);
  if (mCodec == Codec::VP8) {
    vpx_codec_peek_stream_info(vpx_codec_vp8_dx(), aSample->Data(), aSample->Size(), &si);
  } else if (mCodec == Codec::VP9) {
    vpx_codec_peek_stream_info(vpx_codec_vp9_dx(), aSample->Data(), aSample->Size(), &si);
  }
  NS_ASSERTION(bool(si.is_kf) == aSample->mKeyframe,
               "VPX Decode Keyframe error sample->mKeyframe and si.si_kf out of sync");
#endif

  if (vpx_codec_err_t r = vpx_codec_decode(&mVPX, aSample->Data(), aSample->Size(), nullptr, 0)) {
    LOG("VPX Decode error: %s", vpx_codec_err_to_string(r));
    return MediaResult(
      NS_ERROR_DOM_MEDIA_DECODE_ERR,
      RESULT_DETAIL("VPX error: %s", vpx_codec_err_to_string(r)));
  }

  vpx_codec_iter_t  iter = nullptr;
  vpx_image_t      *img;

  while ((img = vpx_codec_get_frame(&mVPX, &iter))) {
    NS_ASSERTION(img->fmt == VPX_IMG_FMT_I420 ||
                 img->fmt == VPX_IMG_FMT_I444,
                 "WebM image format not I420 or I444");

    // Chroma shifts are rounded down as per the decoding examples in the SDK
    VideoData::YCbCrBuffer b;
    b.mPlanes[0].mData = img->planes[0];
    b.mPlanes[0].mStride = img->stride[0];
    b.mPlanes[0].mHeight = img->d_h;
    b.mPlanes[0].mWidth = img->d_w;
    b.mPlanes[0].mOffset = b.mPlanes[0].mSkip = 0;

    b.mPlanes[1].mData = img->planes[1];
    b.mPlanes[1].mStride = img->stride[1];
    b.mPlanes[1].mOffset = b.mPlanes[1].mSkip = 0;

    b.mPlanes[2].mData = img->planes[2];
    b.mPlanes[2].mStride = img->stride[2];
    b.mPlanes[2].mOffset = b.mPlanes[2].mSkip = 0;

    if (img->fmt == VPX_IMG_FMT_I420) {
      b.mPlanes[1].mHeight = (img->d_h + 1) >> img->y_chroma_shift;
      b.mPlanes[1].mWidth = (img->d_w + 1) >> img->x_chroma_shift;

      b.mPlanes[2].mHeight = (img->d_h + 1) >> img->y_chroma_shift;
      b.mPlanes[2].mWidth = (img->d_w + 1) >> img->x_chroma_shift;
    } else if (img->fmt == VPX_IMG_FMT_I444) {
      b.mPlanes[1].mHeight = img->d_h;
      b.mPlanes[1].mWidth = img->d_w;

      b.mPlanes[2].mHeight = img->d_h;
      b.mPlanes[2].mWidth = img->d_w;
    } else {
      LOG("VPX Unknown image format");
      return MediaResult(NS_ERROR_DOM_MEDIA_DECODE_ERR,
                         RESULT_DETAIL("VPX Unknown image format"));
    }

    RefPtr<VideoData> v =
      VideoData::CreateAndCopyData(mInfo,
                                   mImageContainer,
                                   aSample->mOffset,
                                   aSample->mTime,
                                   aSample->mDuration,
                                   b,
                                   aSample->mKeyframe,
                                   aSample->mTimecode,
                                   mInfo.ScaledImageRect(img->d_w,
                                                         img->d_h));

    if (!v) {
      LOG("Image allocation error source %ldx%ld display %ldx%ld picture %ldx%ld",
          img->d_w, img->d_h, mInfo.mDisplay.width, mInfo.mDisplay.height,
          mInfo.mImage.width, mInfo.mImage.height);
      return MediaResult(NS_ERROR_OUT_OF_MEMORY, __func__);
    }
    mCallback->Output(v);
  }
  return NS_OK;
}

void
VPXDecoder::ProcessDecode(MediaRawData* aSample)
{
  MOZ_ASSERT(mTaskQueue->IsCurrentThreadIn());
  if (mIsFlushing) {
    return;
  }
  MediaResult rv = DoDecode(aSample);
  if (NS_FAILED(rv)) {
    mCallback->Error(rv);
  } else {
    mCallback->InputExhausted();
  }
}

void
VPXDecoder::Input(MediaRawData* aSample)
{
  MOZ_ASSERT(mCallback->OnReaderTaskQueue());
  mTaskQueue->Dispatch(NewRunnableMethod<RefPtr<MediaRawData>>(
                       this, &VPXDecoder::ProcessDecode, aSample));
}

void
VPXDecoder::ProcessDrain()
{
  MOZ_ASSERT(mTaskQueue->IsCurrentThreadIn());
  mCallback->DrainComplete();
}

void
VPXDecoder::Drain()
{
  MOZ_ASSERT(mCallback->OnReaderTaskQueue());
  mTaskQueue->Dispatch(NewRunnableMethod(this, &VPXDecoder::ProcessDrain));
}

/* static */
bool
VPXDecoder::IsVPX(const nsACString& aMimeType, uint8_t aCodecMask)
{
  return ((aCodecMask & VPXDecoder::VP8) &&
          aMimeType.EqualsLiteral("video/webm; codecs=vp8")) ||
         ((aCodecMask & VPXDecoder::VP9) &&
          aMimeType.EqualsLiteral("video/webm; codecs=vp9")) ||
         ((aCodecMask & VPXDecoder::VP9) &&
          aMimeType.EqualsLiteral("video/vp9"));
}

/* static */
bool
VPXDecoder::IsVP8(const nsACString& aMimeType)
{
  return IsVPX(aMimeType, VPXDecoder::VP8);
}

/* static */
bool
VPXDecoder::IsVP9(const nsACString& aMimeType)
{
  return IsVPX(aMimeType, VPXDecoder::VP9);
}

} // namespace mozilla
#undef LOG
