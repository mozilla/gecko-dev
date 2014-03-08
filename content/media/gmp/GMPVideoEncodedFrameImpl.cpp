/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GMPVideoEncodedFrameImpl.h"

namespace mozilla {
namespace gmp {

GMPVideoEncodedFrameImpl::GMPVideoEncodedFrameImpl()
: mEncodedWidth(0),
  mEncodedHeight(0),
  mTimeStamp(0),
  mCaptureTime_ms(0),
  mFrameType(kGMPDeltaFrame),
  mAllocatedSize(0),
  mSize(0),
  mCompleteFrame(false),
  mHost(nullptr),
  mBuffer(nullptr)
{
}

GMPVideoEncodedFrameImpl::GMPVideoEncodedFrameImpl(uint32_t aAllocatedSize,
                                                   uint32_t aSize)
: mEncodedWidth(0),
  mEncodedHeight(0),
  mTimeStamp(0),
  mCaptureTime_ms(0),
  mFrameType(kGMPDeltaFrame),
  mAllocatedSize(aAllocatedSize),
  mSize(aSize),
  mCompleteFrame(false),
  mHost(nullptr),
  mBuffer(nullptr)
{
}

GMPVideoEncodedFrameImpl::~GMPVideoEncodedFrameImpl()
{
  DestroyBuffer();
  if (mHost) {
    mHost->EncodedFrameDestroyed(this);
  }
}

void
GMPVideoEncodedFrameImpl::SetHost(GMPVideoHostImpl* aHost)
{
  mHost = aHost;
}

void
GMPVideoEncodedFrameImpl::InvalidateShmem()
{
  DestroyBuffer();
  // Do this after destroying the buffer because destruction
  // might involve deallocation, which requires a host.
  mHost = nullptr;
}

void
GMPVideoEncodedFrameImpl::ExtractShmem(ipc::Shmem** aEncodedFrameShmem)
{
  if (aEncodedFrameShmem) {
    *aEncodedFrameShmem = mBuffer;
  }
}

void
GMPVideoEncodedFrameImpl::ReceiveShmem(ipc::Shmem& aShmem)
{
  DestroyBuffer();
  mBuffer = new ipc::Shmem();
  *mBuffer = aShmem;
  mAllocatedSize = aShmem.Size<uint8_t>();
}

void
GMPVideoEncodedFrameImpl::DestroyBuffer()
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
GMPVideoEncodedFrameImpl::CreateEmptyFrame(uint32_t aSize)
{
  if (aSize > 0) {
    ipc::Shmem* new_mem = new ipc::Shmem();
    if (!mHost->SharedMemMgr()->MgrAllocShmem(aSize, ipc::SharedMemory::TYPE_BASIC, new_mem) ||
        !new_mem->get<uint8_t>()) {
      return GMPVideoAllocErr;
    }
    mBuffer = new_mem;
  }
  mAllocatedSize = aSize;
  mSize = aSize;

  return GMPVideoNoErr;
}

GMPVideoErr
GMPVideoEncodedFrameImpl::CopyFrame(const GMPVideoEncodedFrame& aFrame)
{
  auto& f = static_cast<const GMPVideoEncodedFrameImpl&>(aFrame);

  if (f.mBuffer) {
    GMPVideoErr err = CreateEmptyFrame(f.mSize);
    if (err != GMPVideoNoErr) {
      return err;
    }
    if (mBuffer) {
      memcpy(mBuffer->get<uint8_t>(), f.mBuffer->get<uint8_t>(), mSize);
    }
  }
  mEncodedWidth = f.mEncodedWidth;
  mEncodedHeight = f.mEncodedHeight;
  mTimeStamp = f.mTimeStamp;
  mCaptureTime_ms = f.mCaptureTime_ms;
  mFrameType = f.mFrameType;
  mSize = f.mSize;
  mCompleteFrame = f.mCompleteFrame;
  // Don't copy host, that should have been set properly on object creation via host.

  return GMPVideoNoErr;
}

void
GMPVideoEncodedFrameImpl::SetEncodedWidth(uint32_t aEncodedWidth)
{
  mEncodedWidth = aEncodedWidth;
}

uint32_t
GMPVideoEncodedFrameImpl::EncodedWidth()
{
  return mEncodedWidth;
}

void
GMPVideoEncodedFrameImpl::SetEncodedHeight(uint32_t aEncodedHeight)
{
  mEncodedHeight = aEncodedHeight;
}

uint32_t
GMPVideoEncodedFrameImpl::EncodedHeight()
{
  return mEncodedHeight;
}

void
GMPVideoEncodedFrameImpl::SetTimeStamp(uint32_t aTimeStamp)
{
  mTimeStamp = aTimeStamp;
}

uint32_t
GMPVideoEncodedFrameImpl::TimeStamp()
{
  return mTimeStamp;
}

void
GMPVideoEncodedFrameImpl::SetCaptureTime(int64_t aCaptureTime)
{
  mCaptureTime_ms = aCaptureTime;
}

int64_t
GMPVideoEncodedFrameImpl::CaptureTime()
{
  return mCaptureTime_ms;
}

void
GMPVideoEncodedFrameImpl::SetFrameType(GMPVideoFrameType aFrameType)
{
  mFrameType = aFrameType;
}

GMPVideoFrameType
GMPVideoEncodedFrameImpl::FrameType()
{
  return mFrameType;
}

void
GMPVideoEncodedFrameImpl::SetAllocatedSize(uint32_t aAllocatedSize)
{
  mAllocatedSize = aAllocatedSize;
}

uint32_t
GMPVideoEncodedFrameImpl::AllocatedSize()
{
  return mAllocatedSize;
}

void
GMPVideoEncodedFrameImpl::SetSize(uint32_t aSize)
{
  mSize = aSize;
}

uint32_t
GMPVideoEncodedFrameImpl::Size()
{
  return mSize;
}

void
GMPVideoEncodedFrameImpl::SetCompleteFrame(bool aCompleteFrame)
{
  mCompleteFrame = aCompleteFrame;
}

bool
GMPVideoEncodedFrameImpl::CompleteFrame()
{
  return mCompleteFrame;
}

const uint8_t*
GMPVideoEncodedFrameImpl::Buffer() const 
{
  if (mBuffer) {
    return mBuffer->get<uint8_t>();
  }
  return nullptr;
}

uint8_t*
GMPVideoEncodedFrameImpl::Buffer()
{
  if (mBuffer) {
    return mBuffer->get<uint8_t>();
  }
  return nullptr;
}

void
GMPVideoEncodedFrameImpl::Destroy()
{
  delete this;
}

} // namespace gmp
} // namespace mozilla
