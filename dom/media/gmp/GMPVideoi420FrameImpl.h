/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GMPVideoi420FrameImpl_h_
#define GMPVideoi420FrameImpl_h_

#include "gmp-video-frame-i420.h"
#include "mozilla/ipc/Shmem.h"
#include "mozilla/Maybe.h"
#include "nsTArray.h"

namespace mozilla::gmp {

class GMPPlaneData;
class GMPVideoi420FrameData;
class GMPVideoHostImpl;

class GMPVideoi420FrameImpl final : public GMPVideoi420Frame {
 public:
  explicit GMPVideoi420FrameImpl(GMPVideoHostImpl* aHost);
  GMPVideoi420FrameImpl(const GMPVideoi420FrameData& aFrameData,
                        ipc::Shmem&& aShmemBuffer, GMPVideoHostImpl* aHost);
  GMPVideoi420FrameImpl(const GMPVideoi420FrameData& aFrameData,
                        nsTArray<uint8_t>&& aArrayBuffer,
                        GMPVideoHostImpl* aHost);
  virtual ~GMPVideoi420FrameImpl();

  // This is called during a normal destroy sequence, which is
  // when a consumer is finished or during XPCOM shutdown.
  void DoneWithAPI();

  static bool CheckFrameData(const GMPVideoi420FrameData& aFrameData,
                             size_t aBufferSize);

  void InitFrameData(GMPVideoi420FrameData& aFrameData);
  bool InitFrameData(GMPVideoi420FrameData& aFrameData,
                     ipc::Shmem& aShmemBuffer);
  bool InitFrameData(GMPVideoi420FrameData& aFrameData,
                     nsTArray<uint8_t>& aArrayBuffer);

  // GMPVideoFrame
  GMPVideoFrameFormat GetFrameFormat() override;
  void Destroy() override;

  // GMPVideoi420Frame
  GMPErr CreateEmptyFrame(int32_t aWidth, int32_t aHeight, int32_t aStride_y,
                          int32_t aStride_u, int32_t aStride_v) override;
  GMPErr CreateFrame(int32_t aSize_y, const uint8_t* aBuffer_y, int32_t aSize_u,
                     const uint8_t* aBuffer_u, int32_t aSize_v,
                     const uint8_t* aBuffer_v, int32_t aWidth, int32_t aHeight,
                     int32_t aStride_y, int32_t aStride_u,
                     int32_t aStride_v) override;
  GMPErr CopyFrame(const GMPVideoi420Frame& aFrame) override;
  void SwapFrame(GMPVideoi420Frame* aFrame) override;
  uint8_t* Buffer(GMPPlaneType aType) override;
  const uint8_t* Buffer(GMPPlaneType aType) const override;
  int32_t AllocatedSize(GMPPlaneType aType) const override;
  int32_t Stride(GMPPlaneType aType) const override;
  GMPErr SetWidth(int32_t aWidth) override;
  GMPErr SetHeight(int32_t aHeight) override;
  int32_t Width() const override;
  int32_t Height() const override;
  void SetTimestamp(uint64_t aTimestamp) override;
  uint64_t Timestamp() const override;
  void SetUpdatedTimestamp(uint64_t aTimestamp) override;
  uint64_t UpdatedTimestamp() const override;
  void SetDuration(uint64_t aDuration) override;
  uint64_t Duration() const override;
  bool IsZeroSize() const override;
  void ResetSize() override;

  uint8_t* Buffer();
  const uint8_t* Buffer() const;
  int32_t AllocatedSize() const;

 private:
  struct GMPFramePlane {
    explicit GMPFramePlane(const GMPPlaneData& aPlaneData);
    GMPFramePlane() = default;
    void InitPlaneData(GMPPlaneData& aPlaneData);
    void Copy(uint8_t* aDst, int32_t aDstOffset, const uint8_t* aSrc,
              int32_t aSize, int32_t aStride);

    int32_t mOffset = 0;
    int32_t mSize = 0;
    int32_t mStride = 0;
  };

  const GMPFramePlane* GetPlane(GMPPlaneType aType) const;
  GMPFramePlane* GetPlane(GMPPlaneType aType);
  bool CheckDimensions(int32_t aWidth, int32_t aHeight, int32_t aStride_y,
                       int32_t aStride_u, int32_t aStride_v, int32_t aSize_y,
                       int32_t aSize_u, int32_t aSize_v);
  bool CheckDimensions(int32_t aWidth, int32_t aHeight, int32_t aStride_y,
                       int32_t aStride_u, int32_t aStride_v);
  GMPErr MaybeResize(int32_t aNewSize);
  void DestroyBuffer();

  GMPVideoHostImpl* mHost;
  nsTArray<uint8_t> mArrayBuffer;
  ipc::Shmem mShmemBuffer;
  GMPFramePlane mYPlane;
  GMPFramePlane mUPlane;
  GMPFramePlane mVPlane;
  int32_t mWidth;
  int32_t mHeight;
  uint64_t mTimestamp;
  Maybe<uint64_t> mUpdatedTimestamp;
  uint64_t mDuration;
};

}  // namespace mozilla::gmp

#endif  // GMPVideoi420FrameImpl_h_
