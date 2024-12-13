/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GMPVideoEncodedFrameImpl.h"
#include "GMPVideoHost.h"
#include "mozilla/gmp/GMPTypes.h"
#include "mozilla/Unused.h"
#include "GMPSharedMemManager.h"

namespace mozilla::gmp {

GMPVideoEncodedFrameImpl::GMPVideoEncodedFrameImpl(GMPVideoHostImpl* aHost)
    : mEncodedWidth(0),
      mEncodedHeight(0),
      mTimeStamp(0ll),
      mDuration(0ll),
      mFrameType(kGMPDeltaFrame),
      mSize(0),
      mTemporalLayerId(-1),
      mCompleteFrame(false),
      mHost(aHost),
      mBufferType(GMP_BufferSingle) {
  MOZ_ASSERT(aHost);
  aHost->EncodedFrameCreated(this);
}

GMPVideoEncodedFrameImpl::GMPVideoEncodedFrameImpl(
    const GMPVideoEncodedFrameData& aFrameData, ipc::Shmem&& aShmemBuffer,
    GMPVideoHostImpl* aHost)
    : mEncodedWidth(aFrameData.mEncodedWidth()),
      mEncodedHeight(aFrameData.mEncodedHeight()),
      mTimeStamp(aFrameData.mTimestamp()),
      mDuration(aFrameData.mDuration()),
      mFrameType(static_cast<GMPVideoFrameType>(aFrameData.mFrameType())),
      mSize(aFrameData.mSize()),
      mTemporalLayerId(aFrameData.mTemporalLayerId()),
      mCompleteFrame(aFrameData.mCompleteFrame()),
      mHost(aHost),
      mShmemBuffer(std::move(aShmemBuffer)),
      mBufferType(aFrameData.mBufferType()) {
  MOZ_ASSERT(aHost);
  aHost->EncodedFrameCreated(this);
}

GMPVideoEncodedFrameImpl::GMPVideoEncodedFrameImpl(
    const GMPVideoEncodedFrameData& aFrameData,
    nsTArray<uint8_t>&& aArrayBuffer, GMPVideoHostImpl* aHost)
    : mEncodedWidth(aFrameData.mEncodedWidth()),
      mEncodedHeight(aFrameData.mEncodedHeight()),
      mTimeStamp(aFrameData.mTimestamp()),
      mDuration(aFrameData.mDuration()),
      mFrameType(static_cast<GMPVideoFrameType>(aFrameData.mFrameType())),
      mSize(aFrameData.mSize()),
      mTemporalLayerId(aFrameData.mTemporalLayerId()),
      mCompleteFrame(aFrameData.mCompleteFrame()),
      mHost(aHost),
      mArrayBuffer(std::move(aArrayBuffer)),
      mBufferType(aFrameData.mBufferType()) {
  MOZ_ASSERT(aHost);
  aHost->EncodedFrameCreated(this);
}

GMPVideoEncodedFrameImpl::~GMPVideoEncodedFrameImpl() {
  DestroyBuffer();
  if (mHost) {
    mHost->EncodedFrameDestroyed(this);
  }
}

GMPVideoFrameFormat GMPVideoEncodedFrameImpl::GetFrameFormat() {
  return kGMPEncodedVideoFrame;
}

void GMPVideoEncodedFrameImpl::DoneWithAPI() {
  DestroyBuffer();

  // Do this after destroying the buffer because destruction
  // involves deallocation, which requires a host.
  mHost = nullptr;
}

/* static */
bool GMPVideoEncodedFrameImpl::CheckFrameData(
    const GMPVideoEncodedFrameData& aFrameData, size_t aBufferSize) {
  return aFrameData.mSize() <= aBufferSize;
}

void GMPVideoEncodedFrameImpl::RelinquishFrameData(
    GMPVideoEncodedFrameData& aFrameData) {
  aFrameData.mEncodedWidth() = mEncodedWidth;
  aFrameData.mEncodedHeight() = mEncodedHeight;
  aFrameData.mTimestamp() = mTimeStamp;
  aFrameData.mDuration() = mDuration;
  aFrameData.mFrameType() = mFrameType;
  aFrameData.mSize() = mSize;
  aFrameData.mTemporalLayerId() = mTemporalLayerId;
  aFrameData.mCompleteFrame() = mCompleteFrame;
  aFrameData.mBufferType() = mBufferType;
}

bool GMPVideoEncodedFrameImpl::RelinquishFrameData(
    GMPVideoEncodedFrameData& aFrameData, ipc::Shmem& aShmemBuffer) {
  if (!mShmemBuffer.IsReadable()) {
    return false;
  }

  aShmemBuffer = mShmemBuffer;

  // This method is called right before Shmem is sent to another process.
  // We need to effectively zero out our member copy so that we don't
  // try to delete Shmem we don't own later.
  mShmemBuffer = ipc::Shmem();

  RelinquishFrameData(aFrameData);
  return true;
}

bool GMPVideoEncodedFrameImpl::RelinquishFrameData(
    GMPVideoEncodedFrameData& aFrameData, nsTArray<uint8_t>& aArrayBuffer) {
  if (mShmemBuffer.IsReadable()) {
    return false;
  }

  aArrayBuffer = std::move(mArrayBuffer);
  RelinquishFrameData(aFrameData);
  return true;
}

void GMPVideoEncodedFrameImpl::DestroyBuffer() {
  if (mHost && mShmemBuffer.IsWritable()) {
    mHost->SharedMemMgr()->MgrGiveShmem(GMPSharedMemClass::Encoded,
                                        std::move(mShmemBuffer));
  }
  mShmemBuffer = ipc::Shmem();
  mArrayBuffer.Clear();
}

GMPErr GMPVideoEncodedFrameImpl::CreateEmptyFrame(uint32_t aSize) {
  if (aSize == 0) {
    DestroyBuffer();
  } else if (aSize > AllocatedSize()) {
    DestroyBuffer();
    if (!mHost->SharedMemMgr()->MgrTakeShmem(GMPSharedMemClass::Encoded, aSize,
                                             &mShmemBuffer) &&
        !mArrayBuffer.SetLength(aSize, fallible)) {
      return GMPAllocErr;
    }
  }
  mSize = aSize;

  return GMPNoErr;
}

GMPErr GMPVideoEncodedFrameImpl::CopyFrame(const GMPVideoEncodedFrame& aFrame) {
  auto& f = static_cast<const GMPVideoEncodedFrameImpl&>(aFrame);

  if (f.mSize != 0) {
    GMPErr err = CreateEmptyFrame(f.mSize);
    if (err != GMPNoErr) {
      return err;
    }
    memcpy(Buffer(), f.Buffer(), f.mSize);
  }
  mEncodedWidth = f.mEncodedWidth;
  mEncodedHeight = f.mEncodedHeight;
  mTimeStamp = f.mTimeStamp;
  mDuration = f.mDuration;
  mFrameType = f.mFrameType;
  mSize = f.mSize;  // already set...
  mCompleteFrame = f.mCompleteFrame;
  mBufferType = f.mBufferType;
  // Don't copy host, that should have been set properly on object creation via
  // host.

  return GMPNoErr;
}

void GMPVideoEncodedFrameImpl::SetEncodedWidth(uint32_t aEncodedWidth) {
  mEncodedWidth = aEncodedWidth;
}

uint32_t GMPVideoEncodedFrameImpl::EncodedWidth() { return mEncodedWidth; }

void GMPVideoEncodedFrameImpl::SetEncodedHeight(uint32_t aEncodedHeight) {
  mEncodedHeight = aEncodedHeight;
}

uint32_t GMPVideoEncodedFrameImpl::EncodedHeight() { return mEncodedHeight; }

void GMPVideoEncodedFrameImpl::SetTimeStamp(uint64_t aTimeStamp) {
  mTimeStamp = aTimeStamp;
}

uint64_t GMPVideoEncodedFrameImpl::TimeStamp() { return mTimeStamp; }

void GMPVideoEncodedFrameImpl::SetDuration(uint64_t aDuration) {
  mDuration = aDuration;
}

uint64_t GMPVideoEncodedFrameImpl::Duration() const { return mDuration; }

void GMPVideoEncodedFrameImpl::SetFrameType(GMPVideoFrameType aFrameType) {
  mFrameType = aFrameType;
}

GMPVideoFrameType GMPVideoEncodedFrameImpl::FrameType() { return mFrameType; }

void GMPVideoEncodedFrameImpl::SetAllocatedSize(uint32_t aNewSize) {
  if (aNewSize <= AllocatedSize()) {
    return;
  }

  if (!mHost) {
    return;
  }

  if (!mArrayBuffer.IsEmpty()) {
    Unused << mArrayBuffer.SetLength(aNewSize, fallible);
    return;
  }

  ipc::Shmem new_mem;
  if (!mHost->SharedMemMgr()->MgrTakeShmem(GMPSharedMemClass::Encoded, aNewSize,
                                           &new_mem) &&
      !mArrayBuffer.SetLength(aNewSize, fallible)) {
    return;
  }

  if (mShmemBuffer.IsReadable()) {
    if (new_mem.IsWritable()) {
      memcpy(new_mem.get<uint8_t>(), mShmemBuffer.get<uint8_t>(), mSize);
    }
    mHost->SharedMemMgr()->MgrGiveShmem(GMPSharedMemClass::Encoded,
                                        std::move(mShmemBuffer));
  }

  mShmemBuffer = new_mem;
}

uint32_t GMPVideoEncodedFrameImpl::AllocatedSize() {
  if (mShmemBuffer.IsWritable()) {
    return mShmemBuffer.Size<uint8_t>();
  }
  return mArrayBuffer.Length();
}

void GMPVideoEncodedFrameImpl::SetSize(uint32_t aSize) { mSize = aSize; }

uint32_t GMPVideoEncodedFrameImpl::Size() { return mSize; }

void GMPVideoEncodedFrameImpl::SetCompleteFrame(bool aCompleteFrame) {
  mCompleteFrame = aCompleteFrame;
}

bool GMPVideoEncodedFrameImpl::CompleteFrame() { return mCompleteFrame; }

const uint8_t* GMPVideoEncodedFrameImpl::Buffer() const {
  if (mShmemBuffer.IsReadable()) {
    return mShmemBuffer.get<uint8_t>();
  }
  if (!mArrayBuffer.IsEmpty()) {
    return mArrayBuffer.Elements();
  }
  return nullptr;
}

uint8_t* GMPVideoEncodedFrameImpl::Buffer() {
  if (mShmemBuffer.IsWritable()) {
    return mShmemBuffer.get<uint8_t>();
  }
  if (!mArrayBuffer.IsEmpty()) {
    return mArrayBuffer.Elements();
  }
  return nullptr;
}

void GMPVideoEncodedFrameImpl::Destroy() { delete this; }

GMPBufferType GMPVideoEncodedFrameImpl::BufferType() const {
  return mBufferType;
}

void GMPVideoEncodedFrameImpl::SetBufferType(GMPBufferType aBufferType) {
  mBufferType = aBufferType;
}

void GMPVideoEncodedFrameImpl::SetTemporalLayerId(int32_t aLayerId) {
  mTemporalLayerId = aLayerId;
}

int32_t GMPVideoEncodedFrameImpl::GetTemporalLayerId() {
  return mTemporalLayerId;
}

}  // namespace mozilla::gmp
