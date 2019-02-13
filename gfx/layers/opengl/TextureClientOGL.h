/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//  * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_TEXTURECLIENTOGL_H
#define MOZILLA_GFX_TEXTURECLIENTOGL_H

#include "GLContextTypes.h"             // for SharedTextureHandle, etc
#include "GLImages.h"
#include "gfxTypes.h"
#include "mozilla/Attributes.h"         // for override
#include "mozilla/gfx/Point.h"          // for IntSize
#include "mozilla/layers/CompositorTypes.h"
#include "mozilla/layers/LayersSurfaces.h"  // for SurfaceDescriptor
#include "mozilla/layers/TextureClient.h"  // for TextureClient, etc
#include "AndroidSurfaceTexture.h"

namespace mozilla {

namespace layers {

class EGLImageTextureClient : public TextureClient
{
public:
  EGLImageTextureClient(ISurfaceAllocator* aAllocator,
                        TextureFlags aFlags,
                        EGLImageImage* aImage,
                        gfx::IntSize aSize);

  virtual bool IsAllocated() const override { return true; }

  virtual bool HasInternalBuffer() const override { return false; }

  virtual gfx::IntSize GetSize() const override { return mSize; }

  virtual bool ToSurfaceDescriptor(SurfaceDescriptor& aOutDescriptor) override;

  // Useless functions.
  virtual bool Lock(OpenMode mode) override;

  virtual void Unlock() override;

  virtual bool IsLocked() const override { return mIsLocked; }

  virtual gfx::SurfaceFormat GetFormat() const override
  {
    return gfx::SurfaceFormat::UNKNOWN;
  }

  virtual TemporaryRef<TextureClient>
  CreateSimilar(TextureFlags aFlags = TextureFlags::DEFAULT,
                TextureAllocationFlags aAllocFlags = ALLOC_DEFAULT) const override
  {
    return nullptr;
  }

  virtual bool AllocateForSurface(gfx::IntSize aSize, TextureAllocationFlags aFlags) override
  {
    return false;
  }

protected:
  RefPtr<EGLImageImage> mImage;
  const gfx::IntSize mSize;
  bool mIsLocked;
};

#ifdef MOZ_WIDGET_ANDROID

class SurfaceTextureClient : public TextureClient
{
public:
  SurfaceTextureClient(ISurfaceAllocator* aAllocator,
                       TextureFlags aFlags,
                       gl::AndroidSurfaceTexture* aSurfTex,
                       gfx::IntSize aSize,
                       gl::OriginPos aOriginPos);

  ~SurfaceTextureClient();

  virtual bool IsAllocated() const override { return true; }

  virtual bool HasInternalBuffer() const override { return false; }

  virtual gfx::IntSize GetSize() const { return mSize; }

  virtual bool ToSurfaceDescriptor(SurfaceDescriptor& aOutDescriptor) override;

  // Useless functions.
  virtual bool Lock(OpenMode mode) override;

  virtual void Unlock() override;

  virtual bool IsLocked() const override { return mIsLocked; }

  virtual gfx::SurfaceFormat GetFormat() const override
  {
    return gfx::SurfaceFormat::UNKNOWN;
  }

  virtual TemporaryRef<TextureClient>
  CreateSimilar(TextureFlags aFlags = TextureFlags::DEFAULT,
                TextureAllocationFlags aAllocFlags = ALLOC_DEFAULT) const override
  {
    return nullptr;
  }

  virtual bool AllocateForSurface(gfx::IntSize aSize, TextureAllocationFlags aFlags) override
  {
    return false;
  }

protected:
  const RefPtr<gl::AndroidSurfaceTexture> mSurfTex;
  const gfx::IntSize mSize;
  bool mIsLocked;
};

#endif // MOZ_WIDGET_ANDROID

} // namespace
} // namespace

#endif
