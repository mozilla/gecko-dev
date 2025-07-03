/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GPU_SharedTextureMacIOSurface_H_
#define GPU_SharedTextureMacIOSurface_H_

#include "mozilla/gfx/FileHandleWrapper.h"
#include "mozilla/WeakPtr.h"
#include "mozilla/webgpu/SharedTexture.h"

class MacIOSurface;

namespace mozilla {

namespace webgpu {

class SharedTextureMacIOSurface final : public SharedTexture {
 public:
  static UniquePtr<SharedTextureMacIOSurface> Create(
      WebGPUParent* aParent, const ffi::WGPUDeviceId aDeviceId,
      const uint32_t aWidth, const uint32_t aHeight,
      const struct ffi::WGPUTextureFormat aFormat,
      const ffi::WGPUTextureUsages aUsage);

  SharedTextureMacIOSurface(WebGPUParent* aParent,
                            const ffi::WGPUDeviceId aDeviceId,
                            const uint32_t aWidth, const uint32_t aHeight,
                            const struct ffi::WGPUTextureFormat aFormat,
                            const ffi::WGPUTextureUsages aUsage,
                            RefPtr<MacIOSurface>&& aSurface);
  virtual ~SharedTextureMacIOSurface();

  Maybe<layers::SurfaceDescriptor> ToSurfaceDescriptor() override;

  void GetSnapshot(const ipc::Shmem& aDestShmem,
                   const gfx::IntSize& aSize) override;

  SharedTextureMacIOSurface* AsSharedTextureMacIOSurface() override {
    return this;
  }

  uint32_t GetIOSurfaceId();

 protected:
  const WeakPtr<WebGPUParent> mParent;
  const RawId mDeviceId;
  const RefPtr<MacIOSurface> mSurface;
};

}  // namespace webgpu
}  // namespace mozilla

#endif  // GPU_Texture_H_
