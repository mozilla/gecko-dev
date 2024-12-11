/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GpuFenceMTLSharedEvent.h"

#include "mozilla/gfx/Logging.h"

namespace mozilla {
namespace layers {

/* static */
RefPtr<GpuFenceMTLSharedEvent> GpuFenceMTLSharedEvent::Create(
    UniquePtr<webgpu::ffi::WGPUMetalSharedEventHandle>&& aSharedEventHandle,
    const uint64_t aFenceValue) {
  if (!aSharedEventHandle) {
    MOZ_ASSERT_UNREACHABLE("unexpected to be called");
    return nullptr;
  }
  return new GpuFenceMTLSharedEvent(std::move(aSharedEventHandle), aFenceValue);
}

GpuFenceMTLSharedEvent::GpuFenceMTLSharedEvent(
    UniquePtr<webgpu::ffi::WGPUMetalSharedEventHandle>&& aSharedEventHandle,
    const uint64_t aFenceValue)
    : mSharedEventHandle(std::move(aSharedEventHandle)),
      mFenceValue(aFenceValue) {}

bool GpuFenceMTLSharedEvent::HasCompleted() {
  auto value =
      wgpu_server_metal_shared_event_signaled_value(mSharedEventHandle.get());
  return value >= mFenceValue;
}

}  // namespace layers
}  // namespace mozilla
