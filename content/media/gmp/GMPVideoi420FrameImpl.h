/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GMPVideoi420FrameImpl_h_
#define GMPVideoi420FrameImpl_h_

#include "gmp-video-frame-i420.h"
#include "mozilla/ipc/Shmem.h"

namespace mozilla {
namespace gmp {

class GMPVideoi420FrameImpl : public GMPVideoi420Frame
{
  friend struct IPC::ParamTraits<mozilla::gmp::GMPVideoi420FrameImpl>;
public:
  GMPVideoi420FrameImpl();
  virtual ~GMPVideoi420FrameImpl();

  // A host is required in order to alloc and dealloc shared memory.
  void SetHost(GMPVideoHostImpl* aHost);
  // We have to pass Shmem objects as explicit IPDL params.
  // This method pulls up shared memory backing an object so we can pass it explicitly.
  void ExtractShmem(ipc::Shmem** aYShmem,
                    ipc::Shmem** aUShmem,
                    ipc::Shmem** aVShmem);
  // When we receive an Shmem object via IPDL param, we'll "put it back" via this method.
  void ReceiveShmem(ipc::Shmem& yShmem,
                    ipc::Shmem& uShmem,
                    ipc::Shmem& vShmem);

  // GMPVideoFrame
  virtual GMPVideoFrameFormat GetFrameFormat() MOZ_OVERRIDE;
  virtual void Destroy() MOZ_OVERRIDE;

  // GMPVideoi420Frame
  virtual GMPVideoErr CreateEmptyFrame(int32_t aWidth,
                                       int32_t aHeight,
                                       int32_t aStride_y,
                                       int32_t aStride_u,
                                       int32_t aStride_v) MOZ_OVERRIDE;
  virtual GMPVideoErr CreateFrame(int32_t aSize_y, const uint8_t* aBuffer_y,
                                  int32_t aSize_u, const uint8_t* aBuffer_u,
                                  int32_t aSize_v, const uint8_t* aBuffer_v,
                                  int32_t aWidth,
                                  int32_t aHeight,
                                  int32_t aStride_y,
                                  int32_t aStride_u,
                                  int32_t aStride_v) MOZ_OVERRIDE;
  virtual GMPVideoErr CopyFrame(const GMPVideoi420Frame& aFrame) MOZ_OVERRIDE;
  virtual void SwapFrame(GMPVideoi420Frame* aFrame) MOZ_OVERRIDE;
  virtual uint8_t* Buffer(GMPPlaneType aType) MOZ_OVERRIDE;
  virtual const uint8_t* Buffer(GMPPlaneType aType) const MOZ_OVERRIDE;
  virtual int32_t AllocatedSize(GMPPlaneType aType) const MOZ_OVERRIDE;
  virtual int32_t Stride(GMPPlaneType aType) const MOZ_OVERRIDE;
  virtual GMPVideoErr SetWidth(int32_t aWidth) MOZ_OVERRIDE;
  virtual GMPVideoErr SetHeight(int32_t aHeight) MOZ_OVERRIDE;
  virtual int32_t Width() const MOZ_OVERRIDE;
  virtual int32_t Height() const MOZ_OVERRIDE;
  virtual void SetTimestamp(uint32_t aTimestamp) MOZ_OVERRIDE;
  virtual uint32_t Timestamp() const MOZ_OVERRIDE;
  virtual void SetRenderTime_ms(int64_t aRenderTime_ms) MOZ_OVERRIDE;
  virtual int64_t RenderTime_ms() const MOZ_OVERRIDE;
  virtual bool IsZeroSize() const MOZ_OVERRIDE;
  virtual void ResetSize() MOZ_OVERRIDE;
  virtual void* NativeHandle() const MOZ_OVERRIDE;

private:
  bool CheckDimensions(int32_t aWidth, int32_t aHeight,
                       int32_t aStride_y, int32_t aStride_u, int32_t aStride_v);
  const GMPPlane* GetPlane(GMPPlaneType aType) const;
  GMPPlane* GetPlane(GMPPlaneType aType);

  GMPPlaneImpl mYPlane;
  GMPPlaneImpl mUPlane;
  GMPPlaneImpl mVPlane;
  int32_t mWidth;
  int32_t mHeight;
  uint32_t mTimestamp;
  int64_t mRenderTime_ms;
};

} // namespace gmp
} // namespace mozilla

#endif // GMPVideoi420FrameImpl_h_
