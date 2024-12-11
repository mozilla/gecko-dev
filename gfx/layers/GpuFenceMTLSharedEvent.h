/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_GpuFenceMTLSharedEvent_H
#define MOZILLA_GFX_GpuFenceMTLSharedEvent_H

#include "mozilla/layers/GpuFence.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/webgpu/ffi/wgpu.h"
#include "mozilla/webgpu/WebGPUTypes.h"

namespace mozilla {

namespace webgpu {
namespace ffi {
struct WGPUMetalSharedEventHandle;
}
}  // namespace webgpu

namespace layers {

class GpuFenceMTLSharedEvent : public GpuFence {
 public:
  static RefPtr<GpuFenceMTLSharedEvent> Create(
      UniquePtr<webgpu::ffi::WGPUMetalSharedEventHandle>&& aSharedEventHandle,
      const uint64_t aFenceValue);

  bool HasCompleted() override;

 protected:
  GpuFenceMTLSharedEvent(
      UniquePtr<webgpu::ffi::WGPUMetalSharedEventHandle>&& aSharedEventHandle,
      const uint64_t aFenceValue);
  virtual ~GpuFenceMTLSharedEvent() = default;

  UniquePtr<webgpu::ffi::WGPUMetalSharedEventHandle> mSharedEventHandle;
  const uint64_t mFenceValue;
};

}  // namespace layers
}  // namespace mozilla

#endif /* MOZILLA_GFX_GpuFenceMTLSharedEvent_H */
