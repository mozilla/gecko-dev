/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_MACIOSURFACETEXTUREHOSTOGL_H
#define MOZILLA_GFX_MACIOSURFACETEXTUREHOSTOGL_H

#include "mozilla/layers/CompositorOGL.h"
#include "mozilla/layers/TextureHostOGL.h"

class MacIOSurface;

namespace mozilla {
namespace layers {

/**
 * A texture source meant for use with MacIOSurfaceTextureHostOGL.
 *
 * It does not own any GL texture, and attaches its shared handle to one of
 * the compositor's temporary textures when binding.
 */
class MacIOSurfaceTextureSourceOGL : public TextureSource
                                   , public TextureSourceOGL
{
public:
  MacIOSurfaceTextureSourceOGL(CompositorOGL* aCompositor,
                               MacIOSurface* aSurface);
  virtual ~MacIOSurfaceTextureSourceOGL();

  virtual TextureSourceOGL* AsSourceOGL() override { return this; }

  virtual void BindTexture(GLenum activetex, gfx::Filter aFilter) override;

  virtual bool IsValid() const override { return !!gl(); }

  virtual gfx::IntSize GetSize() const override;

  virtual gfx::SurfaceFormat GetFormat() const override;

  virtual GLenum GetTextureTarget() const override { return LOCAL_GL_TEXTURE_RECTANGLE_ARB; }

  virtual GLenum GetWrapMode() const override { return LOCAL_GL_CLAMP_TO_EDGE; }

  // MacIOSurfaceTextureSourceOGL doesn't own any gl texture
  virtual void DeallocateDeviceData() override {}

  virtual void SetCompositor(Compositor* aCompositor) override;

  gl::GLContext* gl() const;

protected:
  RefPtr<CompositorOGL> mCompositor;
  RefPtr<MacIOSurface> mSurface;
};

/**
 * A TextureHost for shared MacIOSurface
 *
 * Most of the logic actually happens in MacIOSurfaceTextureSourceOGL.
 */
class MacIOSurfaceTextureHostOGL : public TextureHost
{
public:
  MacIOSurfaceTextureHostOGL(TextureFlags aFlags,
                             const SurfaceDescriptorMacIOSurface& aDescriptor);

  // MacIOSurfaceTextureSourceOGL doesn't own any GL texture
  virtual void DeallocateDeviceData() override {}

  virtual void SetCompositor(Compositor* aCompositor) override;

  virtual bool Lock() override;

  virtual gfx::SurfaceFormat GetFormat() const override;

  virtual bool BindTextureSource(CompositableTextureSourceRef& aTexture) override
  {
    aTexture = mTextureSource;
    return !!aTexture;
  }

  virtual TemporaryRef<gfx::DataSourceSurface> GetAsSurface() override
  {
    return nullptr; // XXX - implement this (for MOZ_DUMP_PAINTING)
  }

  gl::GLContext* gl() const;

  virtual gfx::IntSize GetSize() const override;

#ifdef MOZ_LAYERS_HAVE_LOG
  virtual const char* Name() override { return "MacIOSurfaceTextureHostOGL"; }
#endif

protected:
  RefPtr<CompositorOGL> mCompositor;
  RefPtr<GLTextureSource> mTextureSource;
  RefPtr<MacIOSurface> mSurface;
};

}
}

#endif // MOZILLA_GFX_MACIOSURFACETEXTUREHOSTOGL_H
