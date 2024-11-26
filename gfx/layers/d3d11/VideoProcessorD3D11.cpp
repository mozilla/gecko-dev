/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VideoProcessorD3D11.h"

#include <d3d11.h>
#include <d3d11_1.h>

#include "mozilla/gfx/Logging.h"
#include "mozilla/gfx/Types.h"
#include "mozilla/layers/TextureD3D11.h"
#include "mozilla/Maybe.h"

namespace mozilla {
namespace layers {

// TODO: Replace with YUVRangedColorSpace
static Maybe<DXGI_COLOR_SPACE_TYPE> GetSourceDXGIColorSpace(
    const gfx::YUVColorSpace aYUVColorSpace,
    const gfx::ColorRange aColorRange) {
  if (aYUVColorSpace == gfx::YUVColorSpace::BT601) {
    if (aColorRange == gfx::ColorRange::FULL) {
      return Some(DXGI_COLOR_SPACE_YCBCR_FULL_G22_LEFT_P601);
    } else {
      return Some(DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P601);
    }
  } else if (aYUVColorSpace == gfx::YUVColorSpace::BT709) {
    if (aColorRange == gfx::ColorRange::FULL) {
      return Some(DXGI_COLOR_SPACE_YCBCR_FULL_G22_LEFT_P709);
    } else {
      return Some(DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P709);
    }
  } else if (aYUVColorSpace == gfx::YUVColorSpace::BT2020) {
    if (aColorRange == gfx::ColorRange::FULL) {
      // XXX Add SMPTEST2084 handling. HDR content is not handled yet
      return Some(DXGI_COLOR_SPACE_YCBCR_FULL_G22_LEFT_P2020);
    } else {
      return Some(DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P2020);
    }
  }

  return Nothing();
}

static Maybe<DXGI_COLOR_SPACE_TYPE> GetSourceDXGIColorSpace(
    const gfx::YUVRangedColorSpace aYUVColorSpace) {
  const auto info = FromYUVRangedColorSpace(aYUVColorSpace);
  return GetSourceDXGIColorSpace(info.space, info.range);
}

/* static */
RefPtr<VideoProcessorD3D11> VideoProcessorD3D11::Create(ID3D11Device* aDevice) {
  MOZ_ASSERT(aDevice);

  if (!aDevice) {
    return nullptr;
  }

  RefPtr<ID3D11DeviceContext> context;
  aDevice->GetImmediateContext(getter_AddRefs(context));
  if (!context) {
    return nullptr;
  }

  HRESULT hr;
  RefPtr<ID3D11VideoDevice> videoDevice;
  hr =
      aDevice->QueryInterface((ID3D11VideoDevice**)getter_AddRefs(videoDevice));
  if (FAILED(hr)) {
    gfxCriticalNoteOnce << "Failed to get D3D11VideoDevice: " << gfx::hexa(hr);
    return nullptr;
  }

  RefPtr<ID3D11VideoContext> videoContext;
  hr = context->QueryInterface(
      (ID3D11VideoContext**)getter_AddRefs(videoContext));
  if (FAILED(hr)) {
    gfxCriticalNoteOnce << "Failed to get D3D11VideoContext: " << gfx::hexa(hr);
    return nullptr;
  }

  RefPtr<ID3D11VideoContext1> videoContext1;
  hr = videoContext->QueryInterface(
      (ID3D11VideoContext1**)getter_AddRefs(videoContext1));
  if (FAILED(hr)) {
    gfxCriticalNoteOnce << "Failed to get D3D11VideoContext1: "
                        << gfx::hexa(hr);
    return nullptr;
  }

  RefPtr<VideoProcessorD3D11> videoProcessor = new VideoProcessorD3D11(
      aDevice, context, videoDevice, videoContext, videoContext1);

  return videoProcessor;
}

VideoProcessorD3D11::VideoProcessorD3D11(ID3D11Device* aDevice,
                                         ID3D11DeviceContext* aDeviceContext,
                                         ID3D11VideoDevice* aVideoDevice,
                                         ID3D11VideoContext* aVideoContext,
                                         ID3D11VideoContext1* aVideoContext1)
    : mDevice(aDevice),
      mDeviceContext(aDeviceContext),
      mVideoDevice(aVideoDevice),
      mVideoContext(aVideoContext),
      mVideoContext1(aVideoContext1) {}

VideoProcessorD3D11::~VideoProcessorD3D11() {}

HRESULT VideoProcessorD3D11::Init(const gfx::IntSize& aSize) {
  if (mSize == aSize) {
    return S_OK;
  }

  mVideoProcessorEnumerator = nullptr;
  mVideoProcessor = nullptr;
  mSize = gfx::IntSize();

  D3D11_VIDEO_PROCESSOR_CONTENT_DESC desc;
  desc.InputFrameFormat = D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE;
  desc.InputFrameRate.Numerator = 60;
  desc.InputFrameRate.Denominator = 1;
  desc.InputWidth = aSize.width;
  desc.InputHeight = aSize.height;
  desc.OutputFrameRate.Numerator = 60;
  desc.OutputFrameRate.Denominator = 1;
  desc.OutputWidth = aSize.width;
  desc.OutputHeight = aSize.height;
  desc.Usage = D3D11_VIDEO_USAGE_PLAYBACK_NORMAL;

  HRESULT hr = mVideoDevice->CreateVideoProcessorEnumerator(
      &desc, getter_AddRefs(mVideoProcessorEnumerator));
  if (FAILED(hr)) {
    gfxCriticalNoteOnce << "Failed to create VideoProcessorEnumerator: "
                        << gfx::hexa(hr);
    return hr;
  }

  hr = mVideoDevice->CreateVideoProcessor(mVideoProcessorEnumerator, 0,
                                          getter_AddRefs(mVideoProcessor));
  if (FAILED(hr)) {
    gfxCriticalNoteOnce << "Failed to create VideoProcessor: " << gfx::hexa(hr);
    return hr;
  }

  // Turn off auto stream processing (the default) that will hurt power
  // consumption.
  mVideoContext->VideoProcessorSetStreamAutoProcessingMode(mVideoProcessor, 0,
                                                           FALSE);

  mSize = aSize;

  return S_OK;
}

bool VideoProcessorD3D11::CallVideoProcessorBlt(
    InputTextureInfo& aTextureInfo, ID3D11Texture2D* aOutputTexture) {
  MOZ_ASSERT(mVideoProcessorEnumerator);
  MOZ_ASSERT(mVideoProcessor);
  MOZ_ASSERT(aTextureInfo.mTexture);
  MOZ_ASSERT(aOutputTexture);

  HRESULT hr;

  auto yuvRangedColorSpace = gfx::ToYUVRangedColorSpace(
      gfx::ToYUVColorSpace(aTextureInfo.mColorSpace), aTextureInfo.mColorRange);
  auto sourceColorSpace = GetSourceDXGIColorSpace(yuvRangedColorSpace);
  if (sourceColorSpace.isNothing()) {
    gfxCriticalNoteOnce << "Unsupported color space";
    return false;
  }

  DXGI_COLOR_SPACE_TYPE inputColorSpace = sourceColorSpace.ref();
  mVideoContext1->VideoProcessorSetStreamColorSpace1(mVideoProcessor, 0,
                                                     inputColorSpace);

  const DXGI_COLOR_SPACE_TYPE outputColorSpace =
      DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;

  mVideoContext1->VideoProcessorSetOutputColorSpace1(mVideoProcessor,
                                                     outputColorSpace);

  D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC inputDesc = {};
  inputDesc.ViewDimension = D3D11_VPIV_DIMENSION_TEXTURE2D;
  inputDesc.Texture2D.ArraySlice = aTextureInfo.mIndex;

  RefPtr<ID3D11VideoProcessorInputView> inputView;
  hr = mVideoDevice->CreateVideoProcessorInputView(
      aTextureInfo.mTexture, mVideoProcessorEnumerator, &inputDesc,
      getter_AddRefs(inputView));
  if (FAILED(hr)) {
    gfxCriticalNoteOnce << "ID3D11VideoProcessorInputView creation failed: "
                        << gfx::hexa(hr);
    return false;
  }

  D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC outputDesc = {};
  outputDesc.ViewDimension = D3D11_VPOV_DIMENSION_TEXTURE2D;
  outputDesc.Texture2D.MipSlice = 0;

  RefPtr<ID3D11VideoProcessorOutputView> outputView;
  hr = mVideoDevice->CreateVideoProcessorOutputView(
      aOutputTexture, mVideoProcessorEnumerator, &outputDesc,
      getter_AddRefs(outputView));
  if (FAILED(hr)) {
    gfxCriticalNoteOnce << "ID3D11VideoProcessorOutputView creation failed: "
                        << gfx::hexa(hr);
    return false;
  }

  D3D11_VIDEO_PROCESSOR_STREAM stream = {};
  stream.Enable = true;
  stream.pInputSurface = inputView.get();

  hr = mVideoContext->VideoProcessorBlt(mVideoProcessor, outputView, 0, 1,
                                        &stream);
  if (FAILED(hr)) {
    gfxCriticalNoteOnce << "VideoProcessorBlt failed: " << gfx::hexa(hr);
    return false;
  }

  return true;
}

}  // namespace layers
}  // namespace mozilla
