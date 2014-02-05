/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_SOURCESURFACED2D_H_
#define MOZILLA_GFX_SOURCESURFACED2D_H_

#include "2D.h"
#include "HelpersD2D.h"
#include <vector>

namespace mozilla {
namespace gfx {

class DataSourceSurfaceD2D;

class SourceSurfaceD2D : public SourceSurface
{
public:
  SourceSurfaceD2D();
  ~SourceSurfaceD2D();

  virtual SurfaceType GetType() const { return SurfaceType::D2D1_BITMAP; }
  virtual IntSize GetSize() const;
  virtual SurfaceFormat GetFormat() const;
  virtual bool IsValid() const;

  virtual TemporaryRef<DataSourceSurface> GetDataSurface();

  ID2D1Bitmap *GetBitmap() { return mBitmap; }

  bool InitFromData(unsigned char *aData,
                    const IntSize &aSize,
                    int32_t aStride,
                    SurfaceFormat aFormat,
                    ID2D1RenderTarget *aRT);
  bool InitFromTexture(ID3D10Texture2D *aTexture,
                       SurfaceFormat aFormat,
                       ID2D1RenderTarget *aRT);
private:
  friend class DrawTargetD2D;
  friend class DataSourceSurfaceD2D;

  uint32_t GetByteSize() const;

  RefPtr<ID2D1Bitmap> mBitmap;
  // We need to keep this pointer here to check surface validity.
  RefPtr<ID3D10Device> mDevice;
  SurfaceFormat mFormat;
  IntSize mSize;
};


class DataSourceSurfaceD2D : public DataSourceSurface
{
public:
  DataSourceSurfaceD2D(SourceSurfaceD2D* aSourceSurface);
  virtual ~DataSourceSurfaceD2D();

  virtual unsigned char* GetData();
  virtual int32_t Stride();
  virtual IntSize GetSize() const;
  virtual SurfaceFormat GetFormat() const;
  virtual bool Map(MapType, MappedSurface *aMappedSurface);
  virtual void Unmap();

  bool IsValid()
  {
    return mTexture;
  }

private:
  void EnsureMappedTexture();

  RefPtr<ID3D10Texture2D> mTexture;

  D3D10_MAPPED_TEXTURE2D mData;

  SurfaceFormat mFormat;
  IntSize mSize;
  bool mMapped;
};

}
}

#endif /* MOZILLA_GFX_SOURCESURFACED2D_H_ */
