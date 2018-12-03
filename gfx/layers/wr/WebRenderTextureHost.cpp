/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebRenderTextureHost.h"

#include "mozilla/layers/ImageDataSerializer.h"
#include "mozilla/layers/LayersSurfaces.h"
#include "mozilla/webrender/RenderThread.h"

namespace mozilla {
namespace layers {

WebRenderTextureHost::WebRenderTextureHost(
    const SurfaceDescriptor& aDesc, TextureFlags aFlags, TextureHost* aTexture,
    wr::ExternalImageId& aExternalImageId)
    : TextureHost(aFlags), mExternalImageId(aExternalImageId) {
  // The wrapped textureHost will be used in WebRender, and the WebRender could
  // run at another thread. It's hard to control the life-time when gecko
  // receives PTextureParent destroy message. It's possible that textureHost is
  // still used by WebRender. So, we only accept the textureHost without
  // DEALLOCATE_CLIENT flag here. If the buffer deallocation is controlled by
  // parent, we could do something to make sure the wrapped textureHost is not
  // used by WebRender and then release it.
  MOZ_ASSERT(!(aFlags & TextureFlags::DEALLOCATE_CLIENT));

  MOZ_COUNT_CTOR(WebRenderTextureHost);
  mWrappedTextureHost = aTexture;

  CreateRenderTextureHost(aDesc, aTexture);
}

WebRenderTextureHost::~WebRenderTextureHost() {
  MOZ_COUNT_DTOR(WebRenderTextureHost);
  wr::RenderThread::Get()->UnregisterExternalImage(
      wr::AsUint64(mExternalImageId));
}

void WebRenderTextureHost::CreateRenderTextureHost(
    const layers::SurfaceDescriptor& aDesc, TextureHost* aTexture) {
  MOZ_ASSERT(aTexture);

  aTexture->CreateRenderTexture(mExternalImageId);
}

bool WebRenderTextureHost::Lock() {
  MOZ_ASSERT_UNREACHABLE("unexpected to be called");
  return false;
}

void WebRenderTextureHost::Unlock() {
  MOZ_ASSERT_UNREACHABLE("unexpected to be called");
}

bool WebRenderTextureHost::BindTextureSource(
    CompositableTextureSourceRef& aTexture) {
  MOZ_ASSERT_UNREACHABLE("unexpected to be called");
  return false;
}

already_AddRefed<gfx::DataSourceSurface> WebRenderTextureHost::GetAsSurface() {
  if (!mWrappedTextureHost) {
    return nullptr;
  }
  return mWrappedTextureHost->GetAsSurface();
}

void WebRenderTextureHost::SetTextureSourceProvider(
    TextureSourceProvider* aProvider) {}

YUVColorSpace WebRenderTextureHost::GetYUVColorSpace() const {
  if (mWrappedTextureHost) {
    return mWrappedTextureHost->GetYUVColorSpace();
  }
  return YUVColorSpace::UNKNOWN;
}

gfx::IntSize WebRenderTextureHost::GetSize() const {
  if (!mWrappedTextureHost) {
    return gfx::IntSize();
  }
  return mWrappedTextureHost->GetSize();
}

gfx::SurfaceFormat WebRenderTextureHost::GetFormat() const {
  if (!mWrappedTextureHost) {
    return gfx::SurfaceFormat::UNKNOWN;
  }
  return mWrappedTextureHost->GetFormat();
}

gfx::SurfaceFormat WebRenderTextureHost::GetReadFormat() const {
  if (!mWrappedTextureHost) {
    return gfx::SurfaceFormat::UNKNOWN;
  }
  return mWrappedTextureHost->GetReadFormat();
}

int32_t WebRenderTextureHost::GetRGBStride() {
  if (!mWrappedTextureHost) {
    return 0;
  }
  gfx::SurfaceFormat format = GetFormat();
  if (GetFormat() == gfx::SurfaceFormat::YUV) {
    // XXX this stride is used until yuv image rendering by webrender is used.
    // Software converted RGB buffers strides are aliened to 16
    return gfx::GetAlignedStride<16>(
        GetSize().width, BytesPerPixel(gfx::SurfaceFormat::B8G8R8A8));
  }
  return ImageDataSerializer::ComputeRGBStride(format, GetSize().width);
}

bool WebRenderTextureHost::HasIntermediateBuffer() const {
  MOZ_ASSERT(mWrappedTextureHost);
  return mWrappedTextureHost->HasIntermediateBuffer();
}

uint32_t WebRenderTextureHost::NumSubTextures() const {
  MOZ_ASSERT(mWrappedTextureHost);
  return mWrappedTextureHost->NumSubTextures();
}

void WebRenderTextureHost::PushResourceUpdates(
    wr::TransactionBuilder& aResources, ResourceUpdateOp aOp,
    const Range<wr::ImageKey>& aImageKeys, const wr::ExternalImageId& aExtID) {
  MOZ_ASSERT(mWrappedTextureHost);
  MOZ_ASSERT(mExternalImageId == aExtID || SupportsWrNativeTexture());

  mWrappedTextureHost->PushResourceUpdates(aResources, aOp, aImageKeys, aExtID);
}

void WebRenderTextureHost::PushDisplayItems(
    wr::DisplayListBuilder& aBuilder, const wr::LayoutRect& aBounds,
    const wr::LayoutRect& aClip, wr::ImageRendering aFilter,
    const Range<wr::ImageKey>& aImageKeys) {
  MOZ_ASSERT(mWrappedTextureHost);
  MOZ_ASSERT(aImageKeys.length() > 0);

  mWrappedTextureHost->PushDisplayItems(aBuilder, aBounds, aClip, aFilter,
                                        aImageKeys);
}

bool WebRenderTextureHost::SupportsWrNativeTexture() {
  return mWrappedTextureHost->SupportsWrNativeTexture();
}

}  // namespace layers
}  // namespace mozilla
