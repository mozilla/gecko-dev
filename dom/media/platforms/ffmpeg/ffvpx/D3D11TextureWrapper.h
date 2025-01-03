/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __D3D11TextureWrapper_h__
#define __D3D11TextureWrapper_h__

#include "mozilla/UniquePtr.h"

struct AVFrame;
struct AVBufferRef;
struct ID3D11Texture2D;

namespace mozilla {

struct FFmpegLibWrapper;

// D3D11TextureWrapper manages the lifecycle of hardware buffers used
// by the FFVPX hardware decoder when zero-copy decoding is enabled. By
// adding a reference to the hardware buffer, it prevents the FFVPX decoder
// from reusing the buffer too early (while it is still being used for display),
// which can help avoid significant playback stutter.
class D3D11TextureWrapper final {
 public:
  D3D11TextureWrapper(AVFrame* aAVFrame, FFmpegLibWrapper* aLib,
                      ID3D11Texture2D* aTexture, unsigned int aArrayIdx);
  D3D11TextureWrapper(D3D11TextureWrapper&& aWrapper) = delete;
  D3D11TextureWrapper(const D3D11TextureWrapper&& aWrapper) = delete;

  ~D3D11TextureWrapper();

  ID3D11Texture2D* GetTexture() const { return mTexture; }
  unsigned int GetArrayIdx() const { return mArrayIdx; }

 private:
  FFmpegLibWrapper* mLib;
  ID3D11Texture2D* mTexture;
  AVBufferRef* mHWAVBuffer;
  const unsigned int mArrayIdx;
};

}  // namespace mozilla

#endif  // __D3D11TextureWrapper_h__
