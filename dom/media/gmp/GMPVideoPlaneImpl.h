/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GMPVideoPlaneImpl_h_
#define GMPVideoPlaneImpl_h_

#include "gmp-video-plane.h"
#include "nsTArray.h"

namespace mozilla::gmp {

class GMPPlaneData;

class GMPPlaneImpl final : public GMPPlane {
 public:
  GMPPlaneImpl() = default;
  GMPPlaneImpl(nsTArray<uint8_t>&& aArrayBuffer,
               const GMPPlaneData& aPlaneData);
  virtual ~GMPPlaneImpl() = default;

  bool InitPlaneData(nsTArray<uint8_t>& aArrayBuffer, GMPPlaneData& aPlaneData);

  // GMPPlane
  GMPErr CreateEmptyPlane(int32_t aAllocatedSize, int32_t aStride,
                          int32_t aPlaneSize) override;
  GMPErr Copy(const GMPPlane& aPlane) override;
  GMPErr Copy(int32_t aSize, int32_t aStride, const uint8_t* aBuffer) override;
  void Swap(GMPPlane& aPlane) override;
  int32_t AllocatedSize() const override;
  void ResetSize() override;
  bool IsZeroSize() const override;
  int32_t Stride() const override;
  const uint8_t* Buffer() const override;
  uint8_t* Buffer() override;
  void Destroy() override;

 private:
  GMPErr MaybeResize(int32_t aNewSize);

  nsTArray<uint8_t> mArrayBuffer;
  int32_t mSize = 0;
  int32_t mStride = 0;
};

}  // namespace mozilla::gmp

#endif  // GMPVideoPlaneImpl_h_
