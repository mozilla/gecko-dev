/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GPU_ExternalTextureD3D11_H_
#define GPU_ExternalTextureD3D11_H_

#include "mozilla/gfx/FileHandleWrapper.h"
#include "mozilla/webgpu/ExternalTexture.h"

struct ID3D11Texture2D;

namespace mozilla {

namespace layers {
class FenceD3D11;
}  // namespace layers

namespace webgpu {

class ExternalTextureD3D11 final : public ExternalTexture {
 public:
  static UniquePtr<ExternalTextureD3D11> Create(
      WebGPUParent* aParent, const ffi::WGPUDeviceId aDeviceId,
      const uint32_t aWidth, const uint32_t aHeight,
      const struct ffi::WGPUTextureFormat aFormat,
      const ffi::WGPUTextureUsages aUsage);

  ExternalTextureD3D11(
      const uint32_t aWidth, const uint32_t aHeight,
      const struct ffi::WGPUTextureFormat aFormat,
      const ffi::WGPUTextureUsages aUsage,
      const RefPtr<ID3D11Texture2D> aTexture,
      RefPtr<gfx::FileHandleWrapper>&& aSharedHandle,
      const layers::CompositeProcessFencesHolderId aFencesHolderId,
      RefPtr<layers::FenceD3D11>&& aWriteFence);
  virtual ~ExternalTextureD3D11();

  void* GetExternalTextureHandle();

  Maybe<layers::SurfaceDescriptor> ToSurfaceDescriptor() override;

  void GetSnapshot(const ipc::Shmem& aDestShmem,
                   const gfx::IntSize& aSize) override;

  ExternalTextureD3D11* AsExternalTextureD3D11() override { return this; }

 protected:
  const RefPtr<ID3D11Texture2D> mTexture;
  const RefPtr<gfx::FileHandleWrapper> mSharedHandle;
  const layers::CompositeProcessFencesHolderId mFencesHolderId;
  const RefPtr<layers::FenceD3D11> mWriteFence;
};

}  // namespace webgpu
}  // namespace mozilla

#endif  // GPU_Texture_H_
