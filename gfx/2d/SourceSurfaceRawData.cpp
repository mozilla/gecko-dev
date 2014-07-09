/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SourceSurfaceRawData.h"

#include "DataSurfaceHelpers.h"
#include "Logging.h"

namespace mozilla {
namespace gfx {

bool
SourceSurfaceRawData::InitWrappingData(uint8_t *aData,
                                       const IntSize &aSize,
                                       int32_t aStride,
                                       SurfaceFormat aFormat,
                                       bool aOwnData)
{
  mRawData = aData;
  mSize = aSize;
  mStride = aStride;
  mFormat = aFormat;
  mOwnData = aOwnData;

  return true;
}

bool
SourceSurfaceAlignedRawData::Init(const IntSize &aSize,
                                  SurfaceFormat aFormat)
{
  mFormat = aFormat;
  mStride = GetAlignedStride<16>(aSize.width * BytesPerPixel(aFormat));

  size_t bufLen = BufferSizeFromStrideAndHeight(mStride, aSize.height);
  if (bufLen > 0) {
    mArray.Realloc(/* actually an object count */ bufLen);
    mSize = aSize;
  } else {
    mArray.Dealloc();
    mSize.SizeTo(0, 0);
  }

  return mArray != nullptr;
}

}
}
