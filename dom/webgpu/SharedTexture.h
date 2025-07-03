/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SharedTexture_H_
#define SharedTexture_H_

#include "mozilla/gfx/Point.h"
#include "mozilla/layers/LayersSurfaces.h"
#include "mozilla/webgpu/ffi/wgpu.h"
#include "mozilla/webgpu/WebGPUTypes.h"

namespace mozilla {

namespace ipc {
class Shmem;
}

namespace webgpu {

class SharedTextureD3D11;
class SharedTextureDMABuf;
class SharedTextureMacIOSurface;
class WebGPUParent;

// A texture is created and owned by Gecko but is shared with the WebGPU
// implementation.
class SharedTexture {
 public:
  static UniquePtr<SharedTexture> Create(
      WebGPUParent* aParent, const ffi::WGPUDeviceId aDeviceId,
      const uint32_t aWidth, const uint32_t aHeight,
      const struct ffi::WGPUTextureFormat aFormat,
      const ffi::WGPUTextureUsages aUsage);

  SharedTexture(const uint32_t aWidth, const uint32_t aHeight,
                const struct ffi::WGPUTextureFormat aFormat,
                const ffi::WGPUTextureUsages aUsage);
  virtual ~SharedTexture();

  virtual Maybe<layers::SurfaceDescriptor> ToSurfaceDescriptor() = 0;

  virtual void GetSnapshot(const ipc::Shmem& aDestShmem,
                           const gfx::IntSize& aSize) {}

  virtual SharedTextureDMABuf* AsSharedTextureDMABuf() { return nullptr; }

  virtual SharedTextureMacIOSurface* AsSharedTextureMacIOSurface() {
    return nullptr;
  }

  virtual SharedTextureD3D11* AsSharedTextureD3D11() { return nullptr; }

  gfx::IntSize GetSize() { return gfx::IntSize(mWidth, mHeight); }

  void SetSubmissionIndex(uint64_t aSubmissionIndex);
  uint64_t GetSubmissionIndex() const { return mSubmissionIndex; }

  void SetOwnerId(const layers::RemoteTextureOwnerId aOwnerId) {
    mOwnerId = aOwnerId;
  }
  layers::RemoteTextureOwnerId GetOwnerId() const {
    MOZ_ASSERT(mOwnerId.IsValid());
    return mOwnerId;
  }

  virtual void onBeforeQueueSubmit(RawId aQueueId) {}

  virtual void CleanForRecycling() {}

  const uint32_t mWidth;
  const uint32_t mHeight;
  const struct ffi::WGPUTextureFormat mFormat;
  const ffi::WGPUTextureUsages mUsage;

 protected:
  uint64_t mSubmissionIndex = 0;
  layers::RemoteTextureOwnerId mOwnerId;
};

// Dummy class
class SharedTextureReadBackPresent final : public SharedTexture {
 public:
  static UniquePtr<SharedTextureReadBackPresent> Create(
      const uint32_t aWidth, const uint32_t aHeight,
      const struct ffi::WGPUTextureFormat aFormat,
      const ffi::WGPUTextureUsages aUsage);

  SharedTextureReadBackPresent(const uint32_t aWidth, const uint32_t aHeight,
                               const struct ffi::WGPUTextureFormat aFormat,
                               const ffi::WGPUTextureUsages aUsage);
  virtual ~SharedTextureReadBackPresent();

  Maybe<layers::SurfaceDescriptor> ToSurfaceDescriptor() override {
    return Nothing();
  }
};

}  // namespace webgpu
}  // namespace mozilla

#endif  // GPU_SharedTexture_H_
