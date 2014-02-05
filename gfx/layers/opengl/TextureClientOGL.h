/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//  * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_TEXTURECLIENTOGL_H
#define MOZILLA_GFX_TEXTURECLIENTOGL_H

#include "GLContextTypes.h"             // for SharedTextureHandle, etc
#include "gfxTypes.h"
#include "mozilla/Attributes.h"         // for MOZ_OVERRIDE
#include "mozilla/gfx/Point.h"          // for IntSize
#include "mozilla/layers/CompositorTypes.h"
#include "mozilla/layers/LayersSurfaces.h"  // for SurfaceDescriptor
#include "mozilla/layers/TextureClient.h"  // for DeprecatedTextureClient, etc

namespace mozilla {
namespace gfx {
class SurfaceStream;
}
}

namespace mozilla {
namespace layers {

class CompositableForwarder;

/**
 * A TextureClient implementation to share TextureMemory that is already
 * on the GPU, for the OpenGL backend.
 */
class SharedTextureClientOGL : public TextureClient
{
public:
  SharedTextureClientOGL(TextureFlags aFlags);

  ~SharedTextureClientOGL();

  virtual bool IsAllocated() const MOZ_OVERRIDE;

  virtual bool ToSurfaceDescriptor(SurfaceDescriptor& aOutDescriptor) MOZ_OVERRIDE;

  virtual bool Lock(OpenMode mode) MOZ_OVERRIDE;

  virtual void Unlock() MOZ_OVERRIDE;

  virtual bool IsLocked() const MOZ_OVERRIDE { return mIsLocked; }

  void InitWith(gl::SharedTextureHandle aHandle,
                gfx::IntSize aSize,
                gl::SharedTextureShareType aShareType,
                bool aInverted = false);

  virtual gfx::IntSize GetSize() const { return mSize; }

  virtual TextureClientData* DropTextureData() MOZ_OVERRIDE
  {
    // XXX - right now the code paths using this are managing the shared texture
    // data, although they should use a TextureClientData for this to ensure that
    // the destruction sequence is race-free.
    MarkInvalid();
    return nullptr;
  }

protected:
  gl::SharedTextureHandle mHandle;
  gfx::IntSize mSize;
  gl::SharedTextureShareType mShareType;
  bool mInverted;
  bool mIsLocked;
};

/**
 * A TextureClient implementation to share SurfaceStream.
 */
class StreamTextureClientOGL : public TextureClient
{
public:
  StreamTextureClientOGL(TextureFlags aFlags);

  ~StreamTextureClientOGL();

  virtual bool IsAllocated() const MOZ_OVERRIDE;

  virtual bool Lock(OpenMode mode) MOZ_OVERRIDE;

  virtual void Unlock() MOZ_OVERRIDE;

  virtual bool IsLocked() const MOZ_OVERRIDE { return mIsLocked; }

  virtual bool ToSurfaceDescriptor(SurfaceDescriptor& aOutDescriptor) MOZ_OVERRIDE;

  virtual TextureClientData* DropTextureData() MOZ_OVERRIDE { return nullptr; }

  void InitWith(gfx::SurfaceStream* aStream);

  virtual gfx::IntSize GetSize() const { return gfx::IntSize(); }

protected:
  gfx::SurfaceStream* mStream;
  bool mIsLocked;
};

} // namespace
} // namespace

#endif
