/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ExternalTextureMacIOSurface.h"

#include "mozilla/gfx/Logging.h"
#include "mozilla/gfx/MacIOSurface.h"
#include "mozilla/layers/ImageDataSerializer.h"

namespace mozilla::webgpu {

// static
UniquePtr<ExternalTextureMacIOSurface> ExternalTextureMacIOSurface::Create(
    const uint32_t aWidth, const uint32_t aHeight,
    const struct ffi::WGPUTextureFormat aFormat,
    const ffi::WGPUTextureUsages aUsage) {
  if (aFormat.tag != ffi::WGPUTextureFormat_Bgra8Unorm) {
    gfxCriticalNoteOnce << "Non supported format: " << aFormat.tag;
    return nullptr;
  }

  RefPtr<MacIOSurface> surface =
      MacIOSurface::CreateIOSurface(aWidth, aHeight, true);
  if (!surface) {
    return nullptr;
  }

  return MakeUnique<ExternalTextureMacIOSurface>(aWidth, aHeight, aFormat,
                                                 aUsage, std::move(surface));
}

ExternalTextureMacIOSurface::ExternalTextureMacIOSurface(
    const uint32_t aWidth, const uint32_t aHeight,
    const struct ffi::WGPUTextureFormat aFormat,
    const ffi::WGPUTextureUsages aUsage, RefPtr<MacIOSurface>&& aSurface)
    : ExternalTexture(aWidth, aHeight, aFormat, aUsage),
      mSurface(std::move(aSurface)) {}

ExternalTextureMacIOSurface::~ExternalTextureMacIOSurface() {}

void* ExternalTextureMacIOSurface::GetExternalTextureHandle() {
  return nullptr;
}

uint32_t ExternalTextureMacIOSurface::GetIOSurfaceId() {
  return mSurface->GetIOSurfaceID();
}

Maybe<layers::SurfaceDescriptor>
ExternalTextureMacIOSurface::ToSurfaceDescriptor(
    Maybe<gfx::FenceInfo>& aFenceInfo) {
  return Some(layers::SurfaceDescriptorMacIOSurface(
      mSurface->GetIOSurfaceID(), !mSurface->HasAlpha(),
      mSurface->GetYUVColorSpace()));
}

void ExternalTextureMacIOSurface::GetSnapshot(const ipc::Shmem& aDestShmem,
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
}

}  // namespace mozilla::webgpu
