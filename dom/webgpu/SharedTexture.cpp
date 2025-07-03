/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SharedTexture.h"

#include "mozilla/webgpu/WebGPUParent.h"

#ifdef XP_WIN
#  include "mozilla/webgpu/SharedTextureD3D11.h"
#endif

#if defined(XP_LINUX) && !defined(MOZ_WIDGET_ANDROID)
#  include "mozilla/webgpu/SharedTextureDMABuf.h"
#endif

#ifdef XP_MACOSX
#  include "mozilla/webgpu/SharedTextureMacIOSurface.h"
#endif

namespace mozilla::webgpu {

// static
UniquePtr<SharedTexture> SharedTexture::Create(
    WebGPUParent* aParent, const ffi::WGPUDeviceId aDeviceId,
    const uint32_t aWidth, const uint32_t aHeight,
    const struct ffi::WGPUTextureFormat aFormat,
    const ffi::WGPUTextureUsages aUsage) {
  MOZ_ASSERT(aParent);

  UniquePtr<SharedTexture> texture;
#ifdef XP_WIN
  texture = SharedTextureD3D11::Create(aParent, aDeviceId, aWidth, aHeight,
                                       aFormat, aUsage);
#elif defined(XP_LINUX) && !defined(MOZ_WIDGET_ANDROID)
  texture = SharedTextureDMABuf::Create(aParent, aDeviceId, aWidth, aHeight,
                                        aFormat, aUsage);
#elif defined(XP_MACOSX)
  texture = SharedTextureMacIOSurface::Create(aParent, aDeviceId, aWidth,
                                              aHeight, aFormat, aUsage);
#endif
  return texture;
}

SharedTexture::SharedTexture(const uint32_t aWidth, const uint32_t aHeight,
                             const struct ffi::WGPUTextureFormat aFormat,
                             const ffi::WGPUTextureUsages aUsage)
    : mWidth(aWidth), mHeight(aHeight), mFormat(aFormat), mUsage(aUsage) {}

SharedTexture::~SharedTexture() {}

void SharedTexture::SetSubmissionIndex(uint64_t aSubmissionIndex) {
  MOZ_ASSERT(aSubmissionIndex != 0);

  mSubmissionIndex = aSubmissionIndex;
}

UniquePtr<SharedTextureReadBackPresent> SharedTextureReadBackPresent::Create(
    const uint32_t aWidth, const uint32_t aHeight,
    const struct ffi::WGPUTextureFormat aFormat,
    const ffi::WGPUTextureUsages aUsage) {
  return MakeUnique<SharedTextureReadBackPresent>(aWidth, aHeight, aFormat,
                                                  aUsage);
}

SharedTextureReadBackPresent::SharedTextureReadBackPresent(
    const uint32_t aWidth, const uint32_t aHeight,
    const struct ffi::WGPUTextureFormat aFormat,
    const ffi::WGPUTextureUsages aUsage)
    : SharedTexture(aWidth, aHeight, aFormat, aUsage) {}

SharedTextureReadBackPresent::~SharedTextureReadBackPresent() {}

}  // namespace mozilla::webgpu
