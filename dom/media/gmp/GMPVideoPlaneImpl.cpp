/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GMPVideoPlaneImpl.h"
#include <algorithm>
#include "mozilla/gmp/GMPTypes.h"
#include "GMPVideoHost.h"
#include "GMPSharedMemManager.h"

namespace mozilla::gmp {

GMPPlaneImpl::GMPPlaneImpl(nsTArray<uint8_t>&& aArrayBuffer,
                           const GMPPlaneData& aPlaneData)
    : mArrayBuffer(std::move(aArrayBuffer)),
      mSize(aPlaneData.mSize()),
      mStride(aPlaneData.mStride()) {
  MOZ_ASSERT(aPlaneData.mOffset() == 0);
}

bool GMPPlaneImpl::InitPlaneData(nsTArray<uint8_t>& aArrayBuffer,
                                 GMPPlaneData& aPlaneData) {
  aArrayBuffer = std::move(mArrayBuffer);
  aPlaneData.mSize() = mSize;
  aPlaneData.mStride() = mStride;

  return true;
}

GMPErr GMPPlaneImpl::MaybeResize(int32_t aNewSize) {
  if (!mArrayBuffer.SetLength(aNewSize, fallible)) {
    return GMPAllocErr;
  }

  return GMPNoErr;
}

GMPErr GMPPlaneImpl::CreateEmptyPlane(int32_t aAllocatedSize, int32_t aStride,
                                      int32_t aPlaneSize) {
  if (aAllocatedSize < 1 || aStride < 1 || aPlaneSize < 1) {
    return GMPGenericErr;
  }

  GMPErr err = MaybeResize(aAllocatedSize);
  if (err != GMPNoErr) {
    return err;
  }

  mSize = aPlaneSize;
  mStride = aStride;

  return GMPNoErr;
}

GMPErr GMPPlaneImpl::Copy(const GMPPlane& aPlane) {
  auto& planeimpl = static_cast<const GMPPlaneImpl&>(aPlane);

  GMPErr err = MaybeResize(planeimpl.mSize);
  if (err != GMPNoErr) {
    return err;
  }

  if (planeimpl.Buffer() && planeimpl.mSize > 0) {
    memcpy(Buffer(), planeimpl.Buffer(), std::min(mSize, planeimpl.mSize));
  }

  mSize = planeimpl.mSize;
  mStride = planeimpl.mStride;

  return GMPNoErr;
}

GMPErr GMPPlaneImpl::Copy(int32_t aSize, int32_t aStride,
                          const uint8_t* aBuffer) {
  GMPErr err = MaybeResize(aSize);
  if (err != GMPNoErr) {
    return err;
  }

  if (aBuffer && aSize > 0) {
    memcpy(Buffer(), aBuffer, aSize);
  }

  mSize = aSize;
  mStride = aStride;

  return GMPNoErr;
}

void GMPPlaneImpl::Swap(GMPPlane& aPlane) {
  auto& planeimpl = static_cast<GMPPlaneImpl&>(aPlane);

  std::swap(mStride, planeimpl.mStride);
  std::swap(mSize, planeimpl.mSize);
  mArrayBuffer.SwapElements(planeimpl.mArrayBuffer);
}

int32_t GMPPlaneImpl::AllocatedSize() const {
  return static_cast<int32_t>(mArrayBuffer.Length());
}

void GMPPlaneImpl::ResetSize() { mSize = 0; }

bool GMPPlaneImpl::IsZeroSize() const { return (mSize == 0); }

int32_t GMPPlaneImpl::Stride() const { return mStride; }

const uint8_t* GMPPlaneImpl::Buffer() const {
  if (!mArrayBuffer.IsEmpty()) {
    return mArrayBuffer.Elements();
  }
  return nullptr;
}
uint8_t* GMPPlaneImpl::Buffer() {
  if (!mArrayBuffer.IsEmpty()) {
    return mArrayBuffer.Elements();
  }
  return nullptr;
}

void GMPPlaneImpl::Destroy() { delete this; }

}  // namespace mozilla::gmp
