/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ExternalTextureDMABuf.h"

#include "mozilla/gfx/Logging.h"
#include "mozilla/layers/ImageDataSerializer.h"
#include "mozilla/widget/DMABufSurface.h"
#include "mozilla/widget/DMABufLibWrapper.h"
#include "mozilla/widget/gbm.h"

namespace mozilla::webgpu {

// static
UniquePtr<ExternalTextureDMABuf> ExternalTextureDMABuf::Create(
    const ffi::WGPUGlobal* aContext, const ffi::WGPUDeviceId aDeviceId,
    const uint32_t aWidth, const uint32_t aHeight,
    const struct ffi::WGPUTextureFormat aFormat,
    const ffi::WGPUTextureUsages aUsage) {
  if (aFormat.tag != ffi::WGPUTextureFormat_Bgra8Unorm) {
    gfxCriticalNoteOnce << "Non supported format: " << aFormat.tag;
    return nullptr;
  }

  uint64_t memorySize = 0;
  UniquePtr<ffi::WGPUVkImageHandle> vkImage(wgpu_vkimage_create_with_dma_buf(
      aContext, aDeviceId, aWidth, aHeight, &memorySize));
  if (!vkImage) {
    gfxCriticalNoteOnce << "Failed to create VkImage";
    return nullptr;
  }

  const auto dmaBufInfo = wgpu_vkimage_get_dma_buf_info(vkImage.get());
  if (!dmaBufInfo.is_valid) {
    gfxCriticalNoteOnce << "Invalid DMABufInfo";
    return nullptr;
  }

  MOZ_ASSERT(dmaBufInfo.plane_count <= 3);

  if (dmaBufInfo.plane_count > 3) {
    gfxCriticalNoteOnce << "Invalid plane count";
    return nullptr;
  }

  auto rawFd =
      wgpu_vkimage_get_file_descriptor(aContext, aDeviceId, vkImage.get());
  if (rawFd < 0) {
    gfxCriticalNoteOnce << "Failed to get fd fom VkDeviceMemory";
    return nullptr;
  }

  RefPtr<gfx::FileHandleWrapper> fd =
      new gfx::FileHandleWrapper(UniqueFileHandle(rawFd));

  RefPtr<DMABufSurface> surface = DMABufSurfaceRGBA::CreateDMABufSurface(
      std::move(fd), dmaBufInfo, aWidth, aHeight);
  if (!surface) {
    MOZ_ASSERT_UNREACHABLE("unexpected to be called");
    return nullptr;
  }

  layers::SurfaceDescriptor desc;
  if (!surface->Serialize(desc)) {
    MOZ_ASSERT_UNREACHABLE("unexpected to be called");
    return nullptr;
  }

  const auto sdType = desc.type();
  if (sdType != layers::SurfaceDescriptor::TSurfaceDescriptorDMABuf) {
    MOZ_ASSERT_UNREACHABLE("unexpected to be called");
    return nullptr;
  }

  return MakeUnique<ExternalTextureDMABuf>(std::move(vkImage), aWidth, aHeight,
                                           aFormat, aUsage, std::move(surface),
                                           desc.get_SurfaceDescriptorDMABuf());
}

ExternalTextureDMABuf::ExternalTextureDMABuf(
    UniquePtr<ffi::WGPUVkImageHandle>&& aVkImageHandle, const uint32_t aWidth,
    const uint32_t aHeight, const struct ffi::WGPUTextureFormat aFormat,
    const ffi::WGPUTextureUsages aUsage, RefPtr<DMABufSurface>&& aSurface,
    const layers::SurfaceDescriptorDMABuf& aSurfaceDescriptor)
    : ExternalTexture(aWidth, aHeight, aFormat, aUsage),
      mVkImageHandle(std::move(aVkImageHandle)),
      mSurface(std::move(aSurface)),
      mSurfaceDescriptor(aSurfaceDescriptor) {}

ExternalTextureDMABuf::~ExternalTextureDMABuf() {}

void* ExternalTextureDMABuf::GetExternalTextureHandle() { return nullptr; }

Maybe<layers::SurfaceDescriptor> ExternalTextureDMABuf::ToSurfaceDescriptor(
    Maybe<gfx::FenceInfo>& aFenceInfo) {
  layers::SurfaceDescriptor sd;
  if (!mSurface->Serialize(sd)) {
    return Nothing();
  }

  if (sd.type() != layers::SurfaceDescriptor::TSurfaceDescriptorDMABuf) {
    return Nothing();
  }

  return Some(sd);
}

void ExternalTextureDMABuf::GetSnapshot(const ipc::Shmem& aDestShmem,
                                        const gfx::IntSize& aSize) {}

UniqueFileHandle ExternalTextureDMABuf::CloneDmaBufFd() {
  return mSurfaceDescriptor.fds()[0]->ClonePlatformHandle();
}

}  // namespace mozilla::webgpu
