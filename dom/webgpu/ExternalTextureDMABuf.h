/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GPU_ExternalTextureDMABuf_H_
#define GPU_ExternalTextureDMABuf_H_

#include "mozilla/gfx/FileHandleWrapper.h"
#include "mozilla/webgpu/ExternalTexture.h"

class DMABufSurface;

namespace mozilla {

namespace webgpu {

class VkImageHandle;

class ExternalTextureDMABuf final : public ExternalTexture {
 public:
  static UniquePtr<ExternalTextureDMABuf> Create(
      WebGPUParent* aParent, const ffi::WGPUDeviceId aDeviceId,
      const uint32_t aWidth, const uint32_t aHeight,
      const struct ffi::WGPUTextureFormat aFormat,
      const ffi::WGPUTextureUsages aUsage);

  ExternalTextureDMABuf(
      UniquePtr<VkImageHandle>&& aVkImageHandle, const uint32_t aWidth,
      const uint32_t aHeight, const struct ffi::WGPUTextureFormat aFormat,
      const ffi::WGPUTextureUsages aUsage, RefPtr<DMABufSurface>&& aSurface,
      const layers::SurfaceDescriptorDMABuf& aSurfaceDescriptor);
  virtual ~ExternalTextureDMABuf();

  void* GetExternalTextureHandle() override;

  Maybe<layers::SurfaceDescriptor> ToSurfaceDescriptor(
      Maybe<gfx::FenceInfo>& aFenceInfo) override;

  void GetSnapshot(const ipc::Shmem& aDestShmem,
                   const gfx::IntSize& aSize) override;

  ExternalTextureDMABuf* AsExternalTextureDMABuf() override { return this; }

  UniqueFileHandle CloneDmaBufFd();

  const ffi::WGPUVkImageHandle* GetHandle();

 protected:
  UniquePtr<VkImageHandle> mVkImageHandle;
  RefPtr<DMABufSurface> mSurface;
  const layers::SurfaceDescriptorDMABuf mSurfaceDescriptor;
};

}  // namespace webgpu
}  // namespace mozilla

#endif  // GPU_Texture_H_
