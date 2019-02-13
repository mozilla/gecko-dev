/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_TEXTURECLIENT_SHAREDSURFACE_H
#define MOZILLA_GFX_TEXTURECLIENT_SHAREDSURFACE_H

#include <cstddef>                      // for size_t
#include <stdint.h>                     // for uint32_t, uint8_t, uint64_t
#include "GLContextTypes.h"             // for GLContext (ptr only), etc
#include "TextureClient.h"
#include "mozilla/Assertions.h"         // for MOZ_ASSERT, etc
#include "mozilla/RefPtr.h"             // for RefPtr, RefCounted
#include "mozilla/gfx/Point.h"          // for IntSize
#include "mozilla/gfx/Types.h"          // for SurfaceFormat
#include "mozilla/layers/CompositorTypes.h"  // for TextureFlags, etc
#include "mozilla/layers/LayersSurfaces.h"  // for SurfaceDescriptor

namespace mozilla {
namespace gl {
class GLContext;
class SharedSurface;
class SurfaceFactory;
}

namespace layers {

class SharedSurfaceTextureClient : public TextureClient
{
protected:
  const UniquePtr<gl::SharedSurface> mSurf;

  friend class gl::SurfaceFactory;

  SharedSurfaceTextureClient(ISurfaceAllocator* aAllocator, TextureFlags aFlags,
                             UniquePtr<gl::SharedSurface> surf,
                             gl::SurfaceFactory* factory);

  ~SharedSurfaceTextureClient();

public:
  virtual bool IsAllocated() const override { return true; }
  virtual bool Lock(OpenMode) override { return false; }
  virtual bool IsLocked() const override { return false; }
  virtual bool HasInternalBuffer() const override { return false; }

  virtual gfx::SurfaceFormat GetFormat() const override {
    return gfx::SurfaceFormat::UNKNOWN;
  }

  virtual TemporaryRef<TextureClient>
  CreateSimilar(TextureFlags, TextureAllocationFlags) const override {
    return nullptr;
  }

  virtual bool AllocateForSurface(gfx::IntSize,
                                  TextureAllocationFlags) override {
    MOZ_CRASH("Should never hit this.");
    return false;
  }

  virtual gfx::IntSize GetSize() const override;

  virtual bool ToSurfaceDescriptor(SurfaceDescriptor& aOutDescriptor) override;

  gl::SharedSurface* Surf() const {
    return mSurf.get();
  }
};

} // namespace layers
} // namespace mozilla

#endif // MOZILLA_GFX_TEXTURECLIENT_SHAREDSURFACE_H
