/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "D3D11TextureWrapper.h"

#include "FFmpegLibWrapper.h"
#include "FFmpegLog.h"
#include "libavutil/frame.h"
#include "mozilla/Assertions.h"
#include "mozilla/Logging.h"
#include "mozilla/gfx/gfxVars.h"

struct ID3D11Texture2D;

extern mozilla::LazyLogModule sFFmpegVideoLog;

#define LOG(...) \
  MOZ_LOG(sFFmpegVideoLog, mozilla::LogLevel::Verbose, (__VA_ARGS__))

namespace mozilla {

D3D11TextureWrapper::D3D11TextureWrapper(AVFrame* aAVFrame,
                                         FFmpegLibWrapper* aLib,
                                         ID3D11Texture2D* aTexture,
                                         unsigned int aArrayIdx,
                                         std::function<void()>&& aReleaseMethod)
    : mLib(aLib),
      mTexture(aTexture),
      mArrayIdx(aArrayIdx),
      mReleaseMethod(std::move(aReleaseMethod)) {
  MOZ_ASSERT(XRE_IsGPUProcess());
  MOZ_ASSERT(gfx::gfxVars::HwDecodedVideoZeroCopy());
  MOZ_ASSERT(mLib);
  MOZ_ASSERT(aAVFrame);
  MOZ_ASSERT(aTexture);
  mHWAVBuffer = aLib->av_buffer_ref(aAVFrame->buf[0]);
  MOZ_ASSERT(mHWAVBuffer);
  LOG("Locked D3D11 texture %p on index %u", mTexture, mArrayIdx);
}

D3D11TextureWrapper::~D3D11TextureWrapper() {
  MOZ_ASSERT(XRE_IsGPUProcess());
  MOZ_ASSERT(mLib);
  MOZ_ASSERT(mHWAVBuffer);
  mLib->av_buffer_unref(&mHWAVBuffer);
  mLib = nullptr;
  mReleaseMethod();
  LOG("Unlocked D3D11 texture %p on index %u", mTexture, mArrayIdx);
}

}  // namespace mozilla

#undef LOG
