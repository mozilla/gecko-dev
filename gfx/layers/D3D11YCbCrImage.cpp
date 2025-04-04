/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "D3D11YCbCrImage.h"

#include "YCbCrUtils.h"
#include "gfx2DGlue.h"
#include "mozilla/gfx/DeviceManagerDx.h"
#include "mozilla/gfx/gfxVars.h"
#include "mozilla/layers/CompositableClient.h"
#include "mozilla/layers/CompositableForwarder.h"
#include "mozilla/layers/TextureD3D11.h"

using namespace mozilla::gfx;

namespace mozilla {
namespace layers {

DXGIYCbCrTextureAllocationHelper::DXGIYCbCrTextureAllocationHelper(
    const PlanarYCbCrData& aData, TextureFlags aTextureFlags,
    ID3D11Device* aDevice)
    : ITextureClientAllocationHelper(
          gfx::SurfaceFormat::YUV420, aData.mPictureRect.Size(),
          BackendSelector::Content, aTextureFlags, ALLOC_DEFAULT),
      mData(aData),
      mDevice(aDevice) {}

bool DXGIYCbCrTextureAllocationHelper::IsCompatible(
    TextureClient* aTextureClient) {
  MOZ_ASSERT(aTextureClient->GetFormat() == gfx::SurfaceFormat::YUV420);

  DXGIYCbCrTextureData* dxgiData =
      aTextureClient->GetInternalData()->AsDXGIYCbCrTextureData();
  if (!dxgiData || dxgiData->mSize != mData.mPictureRect.Size() ||
      dxgiData->mSizeY != mData.YDataSize() ||
      dxgiData->mSizeCbCr != mData.CbCrDataSize() ||
      dxgiData->mColorDepth != mData.mColorDepth ||
      dxgiData->mYUVColorSpace != mData.mYUVColorSpace) {
    return false;
  }

  ID3D11Texture2D* textureY = dxgiData->GetD3D11Texture(0);

  RefPtr<ID3D11Device> device;
  textureY->GetDevice(getter_AddRefs(device));
  if (!device || device != gfx::DeviceManagerDx::Get()->GetImageDevice()) {
    return false;
  }

  return true;
}

already_AddRefed<TextureClient> DXGIYCbCrTextureAllocationHelper::Allocate(
    KnowsCompositor* aAllocator) {
  auto ySize = mData.YDataSize();
  auto cbcrSize = mData.CbCrDataSize();
  CD3D11_TEXTURE2D_DESC newDesc(mData.mColorDepth == gfx::ColorDepth::COLOR_8
                                    ? DXGI_FORMAT_R8_UNORM
                                    : DXGI_FORMAT_R16_UNORM,
                                ySize.width, ySize.height, 1, 1);
  // Use FenceD3D11 for synchronization.
  newDesc.MiscFlags =
      D3D11_RESOURCE_MISC_SHARED_NTHANDLE | D3D11_RESOURCE_MISC_SHARED;

  RefPtr<ID3D11Texture2D> textureY;
  HRESULT hr =
      mDevice->CreateTexture2D(&newDesc, nullptr, getter_AddRefs(textureY));
  NS_ENSURE_TRUE(SUCCEEDED(hr), nullptr);

  newDesc.Width = cbcrSize.width;
  newDesc.Height = cbcrSize.height;

  RefPtr<ID3D11Texture2D> textureCb;
  hr = mDevice->CreateTexture2D(&newDesc, nullptr, getter_AddRefs(textureCb));
  NS_ENSURE_TRUE(SUCCEEDED(hr), nullptr);

  RefPtr<ID3D11Texture2D> textureCr;
  hr = mDevice->CreateTexture2D(&newDesc, nullptr, getter_AddRefs(textureCr));
  NS_ENSURE_TRUE(SUCCEEDED(hr), nullptr);

  TextureForwarder* forwarder =
      aAllocator ? aAllocator->GetTextureForwarder() : nullptr;

  return TextureClient::CreateWithData(
      DXGIYCbCrTextureData::Create(
          textureY, textureCb, textureCr, mData.mPictureRect.Size(), ySize,
          cbcrSize, mData.mColorDepth, mData.mYUVColorSpace, mData.mColorRange),
      mTextureFlags, forwarder);
}

already_AddRefed<TextureClient> D3D11YCbCrRecycleAllocator::Allocate(
    SurfaceFormat aFormat, IntSize aSize, BackendSelector aSelector,
    TextureFlags aTextureFlags, TextureAllocationFlags aAllocFlags) {
  MOZ_ASSERT_UNREACHABLE("unexpected to be called");
  return nullptr;
}

}  // namespace layers
}  // namespace mozilla
