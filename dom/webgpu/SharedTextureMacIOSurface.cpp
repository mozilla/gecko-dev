/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SharedTextureMacIOSurface.h"

#include "mozilla/gfx/Logging.h"
#include "mozilla/gfx/MacIOSurface.h"
#include "mozilla/layers/GpuFenceMTLSharedEvent.h"
#include "mozilla/layers/ImageDataSerializer.h"
#include "mozilla/webgpu/WebGPUParent.h"

namespace mozilla::webgpu {

// static
UniquePtr<SharedTextureMacIOSurface> SharedTextureMacIOSurface::Create(
    WebGPUParent* aParent, const ffi::WGPUDeviceId aDeviceId,
    const uint32_t aWidth, const uint32_t aHeight,
    const struct ffi::WGPUTextureFormat aFormat,
    const ffi::WGPUTextureUsages aUsage) {
  if (aFormat.tag != ffi::WGPUTextureFormat_Bgra8Unorm) {
    gfxCriticalNoteOnce << "Non supported format: " << aFormat.tag;
    return nullptr;
  }

  if (aWidth > MacIOSurface::GetMaxWidth() ||
      aHeight > MacIOSurface::GetMaxHeight()) {
    gfxCriticalNoteOnce << "Requested MacIOSurface is too large: (" << aWidth
                        << ", " << aHeight << ")";
    return nullptr;
  }

  RefPtr<MacIOSurface> surface =
      MacIOSurface::CreateIOSurface(aWidth, aHeight, true);
  if (!surface) {
    gfxCriticalNoteOnce << "Failed to create MacIOSurface: (" << aWidth << ", "
                        << aHeight << ")";
    return nullptr;
  }

  return MakeUnique<SharedTextureMacIOSurface>(
      aParent, aDeviceId, aWidth, aHeight, aFormat, aUsage, std::move(surface));
}

SharedTextureMacIOSurface::SharedTextureMacIOSurface(
    WebGPUParent* aParent, const ffi::WGPUDeviceId aDeviceId,
    const uint32_t aWidth, const uint32_t aHeight,
    const struct ffi::WGPUTextureFormat aFormat,
    const ffi::WGPUTextureUsages aUsage, RefPtr<MacIOSurface>&& aSurface)
    : SharedTexture(aWidth, aHeight, aFormat, aUsage),
      mParent(aParent),
      mDeviceId(aDeviceId),
      mSurface(std::move(aSurface)) {}

SharedTextureMacIOSurface::~SharedTextureMacIOSurface() {}

uint32_t SharedTextureMacIOSurface::GetIOSurfaceId() {
  return mSurface->GetIOSurfaceID();
}

Maybe<layers::SurfaceDescriptor>
SharedTextureMacIOSurface::ToSurfaceDescriptor() {
  MOZ_ASSERT(mSubmissionIndex > 0);

  RefPtr<layers::GpuFence> gpuFence;
  UniquePtr<ffi::WGPUMetalSharedEventHandle> eventHandle(
      wgpu_server_get_device_fence_metal_shared_event(mParent->GetContext(),
                                                      mDeviceId));
  if (eventHandle) {
    gpuFence = layers::GpuFenceMTLSharedEvent::Create(std::move(eventHandle),
                                                      mSubmissionIndex);
  } else {
    gfxCriticalNoteOnce << "Failed to get MetalSharedEventHandle";
  }

  return Some(layers::SurfaceDescriptorMacIOSurface(
      mSurface->GetIOSurfaceID(), !mSurface->HasAlpha(),
      mSurface->GetYUVColorSpace(), std::move(gpuFence)));
}

void SharedTextureMacIOSurface::GetSnapshot(const ipc::Shmem& aDestShmem,
                                            const gfx::IntSize& aSize) {
  if (!mSurface->Lock()) {
    gfxCriticalNoteOnce << "Failed to lock MacIOSurface";
    return;
  }

  const size_t bytesPerRow = mSurface->GetBytesPerRow();
  const uint32_t stride = layers::ImageDataSerializer::ComputeRGBStride(
      gfx::SurfaceFormat::B8G8R8A8, aSize.width);
  uint8_t* src = (uint8_t*)mSurface->GetBaseAddress();
  uint8_t* dst = aDestShmem.get<uint8_t>();

  MOZ_ASSERT(stride * aSize.height <= aDestShmem.Size<uint8_t>());

  for (int y = 0; y < aSize.height; y++) {
    memcpy(dst, src, stride);
    src += bytesPerRow;
    dst += stride;
  }

  mSurface->Unlock();
}

}  // namespace mozilla::webgpu
