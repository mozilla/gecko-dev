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

class AutoCheckLockD3D11Texture final {
 public:
  explicit AutoCheckLockD3D11Texture(ID3D11Texture2D* aTexture)
      : mIsLocked(false) {
    aTexture->QueryInterface((IDXGIKeyedMutex**)getter_AddRefs(mMutex));
    if (!mMutex) {
      // If D3D11Texture does not have keyed mutex, we think that the
      // D3D11Texture could be locked.
      mIsLocked = true;
      return;
    }

    // Test to see if the keyed mutex has been released
    HRESULT hr = mMutex->AcquireSync(0, 0);
    if (hr == S_OK || hr == WAIT_ABANDONED) {
      mIsLocked = true;
      // According to Microsoft documentation:
      // WAIT_ABANDONED - The shared surface and keyed mutex are no longer in a
      // consistent state. If AcquireSync returns this value, you should release
      // and recreate both the keyed mutex and the shared surface
      // So even if we do get WAIT_ABANDONED, the keyed mutex will have to be
      // released.
      mSyncAcquired = true;
    }
  }

  ~AutoCheckLockD3D11Texture() {
    if (!mSyncAcquired) {
      return;
    }
    HRESULT hr = mMutex->ReleaseSync(0);
    if (FAILED(hr)) {
      NS_WARNING("Failed to unlock the texture");
    }
  }

  bool IsLocked() const { return mIsLocked; }

 private:
  bool mIsLocked;
  bool mSyncAcquired = false;
  RefPtr<IDXGIKeyedMutex> mMutex;
};

DXGIYCbCrTextureAllocationHelper::DXGIYCbCrTextureAllocationHelper(
    const PlanarYCbCrData& aData, TextureFlags aTextureFlags,
    ID3D11Device* aDevice)
    : ITextureClientAllocationHelper(
          gfx::SurfaceFormat::YUV, aData.mPictureRect.Size(),
          BackendSelector::Content, aTextureFlags, ALLOC_DEFAULT),
      mData(aData),
      mDevice(aDevice) {}

bool DXGIYCbCrTextureAllocationHelper::IsCompatible(
    TextureClient* aTextureClient) {
  MOZ_ASSERT(aTextureClient->GetFormat() == gfx::SurfaceFormat::YUV);

  DXGIYCbCrTextureData* dxgiData =
      aTextureClient->GetInternalData()->AsDXGIYCbCrTextureData();
  if (!dxgiData || aTextureClient->GetSize() != mData.mPictureRect.Size() ||
      dxgiData->GetYSize() != mData.YDataSize() ||
      dxgiData->GetCbCrSize() != mData.CbCrDataSize() ||
      dxgiData->GetColorDepth() != mData.mColorDepth ||
      dxgiData->GetYUVColorSpace() != mData.mYUVColorSpace) {
    return false;
  }

  ID3D11Texture2D* textureY = dxgiData->GetD3D11Texture(0);
  ID3D11Texture2D* textureCb = dxgiData->GetD3D11Texture(1);
  ID3D11Texture2D* textureCr = dxgiData->GetD3D11Texture(2);

  RefPtr<ID3D11Device> device;
  textureY->GetDevice(getter_AddRefs(device));
  if (!device || device != gfx::DeviceManagerDx::Get()->GetImageDevice()) {
    return false;
  }

  // Test to see if the keyed mutex has been released.
  // If D3D11Texture failed to lock, do not recycle the DXGIYCbCrTextureData.

  AutoCheckLockD3D11Texture lockY(textureY);
  AutoCheckLockD3D11Texture lockCr(textureCr);
  AutoCheckLockD3D11Texture lockCb(textureCb);

  if (!lockY.IsLocked() || !lockCr.IsLocked() || !lockCb.IsLocked()) {
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
  newDesc.MiscFlags = D3D11_RESOURCE_MISC_SHARED_NTHANDLE |
                      D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;

  RefPtr<ID3D10Multithread> mt;
  HRESULT hr = mDevice->QueryInterface((ID3D10Multithread**)getter_AddRefs(mt));

  if (FAILED(hr) || !mt) {
    gfxCriticalError() << "Multithread safety interface not supported. " << hr;
    return nullptr;
  }

  if (!mt->GetMultithreadProtected()) {
    gfxCriticalError() << "Device used not marked as multithread-safe.";
    return nullptr;
  }

  D3D11MTAutoEnter mtAutoEnter(mt.forget());

  RefPtr<ID3D11Texture2D> textureY;
  hr = mDevice->CreateTexture2D(&newDesc, nullptr, getter_AddRefs(textureY));
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
