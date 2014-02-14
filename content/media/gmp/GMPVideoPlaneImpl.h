/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GMPVideoPlaneImpl_h_
#define GMPVideoPlaneImpl_h_

#include "gmp-video-plane.h"
#include "mozilla/ipc/Shmem.h"

namespace mozilla {
namespace gmp {

class GMPVideoHostImpl;

class GMPPlaneImpl : public GMPPlane
{
  friend struct IPC::ParamTraits<mozilla::gmp::GMPPlaneImpl>;
public:
  GMPPlaneImpl();
  virtual ~GMPPlaneImpl();

  // A host is required in order to alloc and dealloc shared memory.
  void SetHost(GMPVideoHostImpl* aHost);
  // Called when managing IPC actor has been destroyed, which means
  // shared memory backing this object is no longer available.
  void InvalidateShmem();
  // We have to pass Shmem objects as explicit IPDL params.
  // This method pulls up shared memory backing an object so we can pass it explicitly.
  void ExtractShmem(ipc::Shmem** aShmem);
  // When we receive an Shmem object via IPDL param, we'll "put it back" via this method.
  void ReceiveShmem(ipc::Shmem& aShmem);

  // GMPPlane
  virtual GMPVideoErr CreateEmptyPlane(int32_t aAllocatedSize,
                                       int32_t aStride,
                                       int32_t aPlaneSize) MOZ_OVERRIDE;
  virtual GMPVideoErr Copy(const GMPPlane& aPlane) MOZ_OVERRIDE;
  virtual GMPVideoErr Copy(int32_t aSize,
                           int32_t aStride,
                           const uint8_t* aBuffer) MOZ_OVERRIDE;
  virtual void Swap(GMPPlane& aPlane) MOZ_OVERRIDE;
  virtual int32_t AllocatedSize() const MOZ_OVERRIDE;
  virtual void ResetSize() MOZ_OVERRIDE;
  virtual bool IsZeroSize() const MOZ_OVERRIDE;
  virtual int32_t Stride() const MOZ_OVERRIDE;
  virtual const uint8_t* Buffer() const MOZ_OVERRIDE;
  virtual uint8_t* Buffer() MOZ_OVERRIDE;
  virtual void Destroy() MOZ_OVERRIDE;

private:
  GMPVideoErr MaybeResize(int32_t aNewSize);
  void DestroyBuffer();

  ipc::Shmem* mBuffer;
  int32_t mAllocatedSize;
  int32_t mSize;
  int32_t mStride;
  GMPVideoHostImpl* mHost;
};

} // namespace gmp
} // namespace mozilla

#endif // GMPVideoPlaneImpl_h_
