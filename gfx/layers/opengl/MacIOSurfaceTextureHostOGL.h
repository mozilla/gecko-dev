/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_MACIOSURFACETEXTUREHOSTOGL_H
#define MOZILLA_GFX_MACIOSURFACETEXTUREHOSTOGL_H

#include "mozilla/layers/CompositorOGL.h"
#include "mozilla/layers/TextureHostOGL.h"
#include "mozilla/gfx/2D.h"
#include "MacIOSurfaceHelpers.h"

class MacIOSurface;

namespace mozilla {
namespace layers {

/**
 * A TextureHost for shared MacIOSurface
 *
 * Most of the logic actually happens in MacIOSurfaceTextureSourceOGL.
 */
class MacIOSurfaceTextureHostOGL : public TextureHost {
 public:
  MacIOSurfaceTextureHostOGL(TextureFlags aFlags,
                             const SurfaceDescriptorMacIOSurface& aDescriptor);
  virtual ~MacIOSurfaceTextureHostOGL();

  // MacIOSurfaceTextureSourceOGL doesn't own any GL texture
  virtual void DeallocateDeviceData() override {}

  virtual void SetTextureSourceProvider(
      TextureSourceProvider* aProvider) override;

  virtual bool Lock() override;

  virtual gfx::SurfaceFormat GetFormat() const override;
  virtual gfx::SurfaceFormat GetReadFormat() const override;

  virtual bool BindTextureSource(
      CompositableTextureSourceRef& aTexture) override {
    aTexture = mTextureSource;
    return !!aTexture;
  }

  virtual already_AddRefed<gfx::DataSourceSurface> GetAsSurface() override {
    RefPtr<gfx::SourceSurface> surf =
        CreateSourceSurfaceFromMacIOSurface(GetMacIOSurface());
    return surf->GetDataSurface();
  }

  gl::GLContext* gl() const;

  virtual gfx::IntSize GetSize() const override;

#ifdef MOZ_LAYERS_HAVE_LOG
  virtual const char* Name() override { return "MacIOSurfaceTextureHostOGL"; }
#endif

  virtual MacIOSurfaceTextureHostOGL* AsMacIOSurfaceTextureHost() override {
    return this;
  }

  virtual MacIOSurface* GetMacIOSurface() override { return mSurface; }

  virtual void CreateRenderTexture(
      const wr::ExternalImageId& aExternalImageId) override;

  virtual uint32_t NumSubTextures() const override;

  virtual void PushResourceUpdates(wr::TransactionBuilder& aResources,
                                   ResourceUpdateOp aOp,
                                   const Range<wr::ImageKey>& aImageKeys,
                                   const wr::ExternalImageId& aExtID) override;

  virtual void PushDisplayItems(wr::DisplayListBuilder& aBuilder,
                                const wr::LayoutRect& aBounds,
                                const wr::LayoutRect& aClip,
                                wr::ImageRendering aFilter,
                                const Range<wr::ImageKey>& aImageKeys) override;

 protected:
  GLTextureSource* CreateTextureSourceForPlane(size_t aPlane);

  RefPtr<GLTextureSource> mTextureSource;
  RefPtr<MacIOSurface> mSurface;
};

}  // namespace layers
}  // namespace mozilla

#endif  // MOZILLA_GFX_MACIOSURFACETEXTUREHOSTOGL_H
