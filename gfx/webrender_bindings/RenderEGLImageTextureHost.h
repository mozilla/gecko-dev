/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_RENDEREGLIMAGETEXTUREHOST_H
#define MOZILLA_GFX_RENDEREGLIMAGETEXTUREHOST_H

#include "mozilla/layers/TextureHostOGL.h"
#include "RenderTextureHostSWGL.h"

namespace mozilla {

namespace wr {

// RenderEGLImageTextureHost is created only for SharedSurface_EGLImage that is
// created in parent process.
class RenderEGLImageTextureHost final : public RenderTextureHostSWGL {
 public:
  RenderEGLImageTextureHost(EGLImage aImage, EGLSync aSync, gfx::IntSize aSize,
                            gfx::SurfaceFormat aFormat);

  wr::WrExternalImage Lock(uint8_t aChannelIndex, gl::GLContext* aGL) override;
  void Unlock() override;
  size_t Bytes() override {
    return mSize.width * mSize.height * BytesPerPixel(mFormat);
  }

  RenderEGLImageTextureHost* AsRenderEGLImageTextureHost() override {
    return this;
  }

  RefPtr<layers::TextureSource> CreateTextureSource(
      layers::TextureSourceProvider* aProvider) override;

  // RenderTextureHostSWGL
  gfx::SurfaceFormat GetFormat() const override;
  gfx::ColorDepth GetColorDepth() const override {
    return gfx::ColorDepth::COLOR_8;
  }
  size_t GetPlaneCount() const override { return 1; };
  bool MapPlane(RenderCompositor* aCompositor, uint8_t aChannelIndex,
                PlaneInfo& aPlaneInfo) override;
  void UnmapPlanes() override;

 private:
  virtual ~RenderEGLImageTextureHost();
  bool CreateTextureHandle();
  void DeleteTextureHandle();
  bool WaitSync();
  already_AddRefed<gfx::DataSourceSurface> ReadTexImage();

  const EGLImage mImage;
  EGLSync mSync;
  const gfx::IntSize mSize;
  const gfx::SurfaceFormat mFormat;

  RefPtr<gl::GLContext> mGL;
  GLenum mTextureTarget;
  GLuint mTextureHandle;
  RefPtr<gfx::DataSourceSurface> mReadback;
};

}  // namespace wr
}  // namespace mozilla

#endif  // MOZILLA_GFX_RENDEREGLIMAGETEXTUREHOST_H
