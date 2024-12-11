/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ExternalTexture.h"

#include "mozilla/webgpu/WebGPUParent.h"

#ifdef XP_WIN
#  include "mozilla/webgpu/ExternalTextureD3D11.h"
#endif

#ifdef MOZ_WIDGET_GTK
#  include "mozilla/webgpu/ExternalTextureDMABuf.h"
#endif

#ifdef XP_MACOSX
#  include "mozilla/webgpu/ExternalTextureMacIOSurface.h"
#endif

namespace mozilla::webgpu {

// static
UniquePtr<ExternalTexture> ExternalTexture::Create(
    WebGPUParent* aParent, const ffi::WGPUDeviceId aDeviceId,
    const uint32_t aWidth, const uint32_t aHeight,
    const struct ffi::WGPUTextureFormat aFormat,
    const ffi::WGPUTextureUsages aUsage) {
  MOZ_ASSERT(aParent);

  UniquePtr<ExternalTexture> texture;
#ifdef XP_WIN
  texture = ExternalTextureD3D11::Create(aWidth, aHeight, aFormat, aUsage);
#elif defined(MOZ_WIDGET_GTK)
  auto* context = aParent->GetContext();
  texture = ExternalTextureDMABuf::Create(context, aDeviceId, aWidth, aHeight,
                                          aFormat, aUsage);
#elif defined(XP_MACOSX)
  texture = ExternalTextureMacIOSurface::Create(aParent, aDeviceId, aWidth,
                                                aHeight, aFormat, aUsage);
#endif
  return texture;
}

ExternalTexture::ExternalTexture(const uint32_t aWidth, const uint32_t aHeight,
                                 const struct ffi::WGPUTextureFormat aFormat,
                                 const ffi::WGPUTextureUsages aUsage)
    : mWidth(aWidth), mHeight(aHeight), mFormat(aFormat), mUsage(aUsage) {}

ExternalTexture::~ExternalTexture() {}

void ExternalTexture::SetSubmissionIndex(uint64_t aSubmissionIndex) {
  MOZ_ASSERT(aSubmissionIndex != 0);

  mSubmissionIndex = aSubmissionIndex;
}

}  // namespace mozilla::webgpu
