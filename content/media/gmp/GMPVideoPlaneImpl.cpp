/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GMPVideoPlaneImpl.h"

namespace mozilla {
namespace gmp {

GMPPlaneImpl::GMPPlaneImpl()
: mBuffer(nullptr),
  mAllocatedSize(0),
  mSize(0),
  mStride(0),
  mHost(nullptr)
{
}

GMPPlaneImpl::~GMPPlaneImpl()
{
  DestroyBuffer();
  if (mHost) {
    mHost->PlaneDestroyed(this);
  }
}

void
GMPPlaneImpl::SetHost(GMPVideoHostImpl* aHost)
{
  mHost = aHost;
}

void
GMPPlaneImpl::InvalidateShmem()
{
  DestroyBuffer();
  // Do this after destroying the buffer because destruction
  // might involve deallocation, which requires a host.
  mHost = nullptr;
}

void
GMPPlaneImpl::ExtractShmem(ipc::Shmem** aShmem)
{
  if (aShmem) {
    *aShmem = mBuffer;
  }
}

void
GMPPlaneImpl::ReceiveShmem(ipc::Shmem& aShmem)
{
  DestroyBuffer();
  mBuffer = new ipc::Shmem();
  if (!mBuffer) {
    NS_WARNING("GMPPlaneImpl: New buffer allocation failed!");
    return;
  }
  *mBuffer = aShmem;
  mAllocatedSize = aShmem.Size<uint8_t>();
  if (mAllocatedSize < mSize) {
    NS_WARNING("GMPPlaneImpl: Allocated size for buffer is smaller than required!");
  }
}

GMPVideoErr
GMPPlaneImpl::MaybeResize(int32_t aNewSize) {
  if (aNewSize <= mAllocatedSize) {
    return GMPVideoNoErr;
  }

  if (!mHost) {
    return GMPVideoGenericErr;
  }

  ipc::Shmem* new_mem = new ipc::Shmem();
  if (!new_mem) {
    return GMPVideoAllocErr;
  }

  if (!mHost->SharedMemMgr()->MgrAllocShmem(aNewSize, ipc::SharedMemory::TYPE_BASIC, new_mem) ||
      !new_mem->get<uint8_t>()) {
    return GMPVideoAllocErr;
  }

  if (mBuffer) {
    if (mBuffer->IsReadable()) {
      memcpy(new_mem->get<uint8_t>(), mBuffer->get<uint8_t>(), mSize);
    }
    DestroyBuffer();
  }

  mBuffer = new_mem;
  mAllocatedSize = aNewSize;

  return GMPVideoNoErr;
}

void
GMPPlaneImpl::DestroyBuffer()
{
  if (mBuffer) {
    if (mHost && mBuffer->IsWritable()) {
      mHost->SharedMemMgr()->MgrDeallocShmem(*mBuffer);
    }
    delete mBuffer;
    mBuffer = nullptr;
  }
}

GMPVideoErr
GMPPlaneImpl::CreateEmptyPlane(int32_t aAllocatedSize, int32_t aStride, int32_t aPlaneSize)
{
  if (aAllocatedSize < 1 || aStride < 1 || aPlaneSize < 1) {
    return GMPVideoGenericErr;
  }

  GMPVideoErr err = MaybeResize(aAllocatedSize);
  if (err != GMPVideoNoErr) {
    return err;
  }

  mSize = aPlaneSize;
  mStride = aStride;

  return GMPVideoNoErr;
}

GMPVideoErr
GMPPlaneImpl::Copy(const GMPPlane& aPlane)
{
  auto& planeimpl = static_cast<const GMPPlaneImpl&>(aPlane);

  GMPVideoErr err = MaybeResize(planeimpl.mAllocatedSize);
  if (err != GMPVideoNoErr) {
    return err;
  }

  if (planeimpl.mBuffer &&
      planeimpl.mBuffer->get<uint8_t>()) {
    memcpy(mBuffer->get<uint8_t>(), planeimpl.mBuffer->get<uint8_t>(), planeimpl.mSize);
  }

  mSize = planeimpl.mSize;
  mStride = planeimpl.mStride;

  return GMPVideoNoErr;
}

GMPVideoErr
GMPPlaneImpl::Copy(int32_t aSize, int32_t aStride, const uint8_t* aBuffer)
{
  GMPVideoErr err = MaybeResize(aSize);
  if (err != GMPVideoNoErr) {
    return err;
  }

  if (aBuffer && mBuffer) {
    memcpy(mBuffer->get<uint8_t>(), aBuffer, aSize);
  }

  mSize = aSize;
  mStride = aStride;

  return GMPVideoNoErr;
}

void
GMPPlaneImpl::Swap(GMPPlane& aPlane)
{
  auto& planeimpl = static_cast<GMPPlaneImpl&>(aPlane);

  std::swap(mStride, planeimpl.mStride);
  std::swap(mAllocatedSize, planeimpl.mAllocatedSize);
  std::swap(mSize, planeimpl.mSize);
  std::swap(mBuffer, planeimpl.mBuffer);
}

int32_t
GMPPlaneImpl::AllocatedSize() const
{
  return mAllocatedSize;
}

void
GMPPlaneImpl::ResetSize()
{
  mSize = 0;
}

bool
GMPPlaneImpl::IsZeroSize() const
{
  return (mSize == 0);
}

int32_t
GMPPlaneImpl::Stride() const
{
  return mStride;
}

const uint8_t*
GMPPlaneImpl::Buffer() const
{
  if (mBuffer) {
    return mBuffer->get<uint8_t>();
  }
  return nullptr;
}

uint8_t*
GMPPlaneImpl::Buffer()
{
  if (mBuffer) {
    return mBuffer->get<uint8_t>();
  }
  return nullptr;
}

void
GMPPlaneImpl::Destroy()
{
  delete this;
}

} // namespace gmp
} // namespace mozilla
