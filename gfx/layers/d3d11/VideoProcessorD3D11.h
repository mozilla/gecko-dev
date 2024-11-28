/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_VideoProcessorD3D11_H
#define MOZILLA_GFX_VideoProcessorD3D11_H

#include <d3d11.h>

#include "mozilla/gfx/2D.h"
#include "mozilla/RefPtr.h"
#include "nsISupportsImpl.h"

struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11Texture2D;
struct ID3D11VideoDevice;
struct ID3D11VideoContext;
struct ID3D11VideoContext1;
struct ID3D11VideoProcessor;
struct ID3D11VideoProcessorEnumerator;
struct ID3D11VideoProcessorOutputView;

namespace mozilla {
namespace layers {

class DXGITextureHostD3D11;

//
// A class for wrapping ID3D11VideoProcessor.
//
// The class can be used for converting NV12 video frame to RGB
class VideoProcessorD3D11 {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(VideoProcessorD3D11);

  static RefPtr<VideoProcessorD3D11> Create(ID3D11Device* aDevice);

  HRESULT Init(const gfx::IntSize& aSize);

  struct InputTextureInfo {
    InputTextureInfo(gfx::ColorSpace2 aColorSpace, gfx::ColorRange aColorRange,
                     uint32_t aIndex, ID3D11Texture2D* aTexture)
        : mColorSpace(aColorSpace),
          mColorRange(aColorRange),
          mIndex(aIndex),
          mTexture(aTexture) {}
    const gfx::ColorSpace2 mColorSpace;
    const gfx::ColorRange mColorRange;
    const uint32_t mIndex;
    ID3D11Texture2D* mTexture;
  };
  bool CallVideoProcessorBlt(InputTextureInfo& aTextureInfo,
                             ID3D11Texture2D* aOutputTexture);
  gfx::IntSize GetSize() const { return mSize; }

 protected:
  VideoProcessorD3D11(ID3D11Device* aDevice,
                      ID3D11DeviceContext* aDeviceContext,
                      ID3D11VideoDevice* aVideoDevice,
                      ID3D11VideoContext* aVideoContext,
                      ID3D11VideoContext1* aVideoContext1);
  ~VideoProcessorD3D11();

 public:
  const RefPtr<ID3D11Device> mDevice;
  const RefPtr<ID3D11DeviceContext> mDeviceContext;
  const RefPtr<ID3D11VideoDevice> mVideoDevice;
  const RefPtr<ID3D11VideoContext> mVideoContext;
  const RefPtr<ID3D11VideoContext1> mVideoContext1;

 protected:
  gfx::IntSize mSize;
  RefPtr<ID3D11VideoProcessor> mVideoProcessor;
  RefPtr<ID3D11VideoProcessorEnumerator> mVideoProcessorEnumerator;
};

}  // namespace layers
}  // namespace mozilla

#endif  // MOZILLA_GFX_VideoProcessorD3D11_H
