/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_SOURCESURFACESKIA_H_
#define MOZILLA_GFX_SOURCESURFACESKIA_H_

#include "2D.h"
#include <vector>
#include "skia/SkCanvas.h"
#include "skia/SkBitmap.h"

namespace mozilla {

namespace gfx {

class DrawTargetSkia;

class SourceSurfaceSkia : public DataSourceSurface
{
public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(DataSourceSurfaceSkia)
  SourceSurfaceSkia();
  ~SourceSurfaceSkia();

  virtual SurfaceType GetType() const { return SurfaceType::SKIA; }
  virtual IntSize GetSize() const;
  virtual SurfaceFormat GetFormat() const;

  SkBitmap& GetBitmap() { return mBitmap; }

  bool InitFromData(unsigned char* aData,
                    const IntSize &aSize,
                    int32_t aStride,
                    SurfaceFormat aFormat);

  bool InitFromCanvas(SkCanvas* aCanvas,
                      SurfaceFormat aFormat,
                      DrawTargetSkia* aOwner);

  /**
   * NOTE: While wrapping a Texture for SkiaGL, the texture *must* be created
   *       with the same GLcontext of DrawTargetSkia
   */
  bool InitFromTexture(DrawTargetSkia* aOwner,
                       unsigned int aTexture,
                       const IntSize &aSize,
                       SurfaceFormat aFormat);

  virtual unsigned char *GetData();

  virtual int32_t Stride() { return mStride; }

private:
  friend class DrawTargetSkia;

  void DrawTargetWillChange();
  void MaybeUnlock();

  SkBitmap mBitmap;
  SurfaceFormat mFormat;
  IntSize mSize;
  int32_t mStride;
  RefPtr<DrawTargetSkia> mDrawTarget;
  bool mLocked;
};

}
}

#endif /* MOZILLA_GFX_SOURCESURFACESKIA_H_ */
