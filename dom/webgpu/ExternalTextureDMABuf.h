/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GPU_ExternalTextureDMABuf_H_
#define GPU_ExternalTextureDMABuf_H_

#include "mozilla/gfx/FileHandleWrapper.h"
#include "mozilla/WeakPtr.h"
#include "mozilla/webgpu/ExternalTexture.h"
#include "nsTArrayForwardDeclare.h"

class DMABufSurface;

namespace mozilla {

namespace webgpu {

class VkImageHandle;
class VkSemaphoreHandle;

class ExternalTextureDMABuf final : public ExternalTexture {
 public:
  static UniquePtr<ExternalTextureDMABuf> Create(
      WebGPUParent* aParent, const ffi::WGPUDeviceId aDeviceId,
      const uint32_t aWidth, const uint32_t aHeight,
      const struct ffi::WGPUTextureFormat aFormat,
      const ffi::WGPUTextureUsages aUsage);

  ExternalTextureDMABuf(
      WebGPUParent* aParent, const ffi::WGPUDeviceId aDeviceId,
      UniquePtr<VkImageHandle>&& aVkImageHandle, const uint32_t aWidth,
      const uint32_t aHeight, const struct ffi::WGPUTextureFormat aFormat,
      const ffi::WGPUTextureUsages aUsage, RefPtr<DMABufSurface>&& aSurface,
      const layers::SurfaceDescriptorDMABuf& aSurfaceDescriptor);
  virtual ~ExternalTextureDMABuf();

  Maybe<layers::SurfaceDescriptor> ToSurfaceDescriptor() override;

  void GetSnapshot(const ipc::Shmem& aDestShmem,
                   const gfx::IntSize& aSize) override;

  ExternalTextureDMABuf* AsExternalTextureDMABuf() override { return this; }

  void onBeforeQueueSubmit(RawId aQueueId) override;

  void CleanForRecycling() override;

  UniqueFileHandle CloneDmaBufFd();

  const ffi::WGPUVkImageHandle* GetHandle();

 protected:
  const WeakPtr<WebGPUParent> mParent;
  const RawId mDeviceId;
  UniquePtr<VkImageHandle> mVkImageHandle;
  nsTArray<UniquePtr<VkSemaphoreHandle>> mVkSemaphoreHandles;
  RefPtr<DMABufSurface> mSurface;
  const layers::SurfaceDescriptorDMABuf mSurfaceDescriptor;
  nsTArray<RefPtr<gfx::FileHandleWrapper>> mSemaphoreFds;
};

}  // namespace webgpu
}  // namespace mozilla

#endif  // GPU_Texture_H_
