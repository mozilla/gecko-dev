/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GMPVideoi420FrameImpl.h"
#include <algorithm>
#include "mozilla/gmp/GMPTypes.h"
#include "mozilla/CheckedInt.h"
#include "GMPVideoHost.h"
#include "GMPSharedMemManager.h"

namespace mozilla::gmp {

GMPVideoi420FrameImpl::GMPFramePlane::GMPFramePlane(
    const GMPPlaneData& aPlaneData)
    : mOffset(aPlaneData.mOffset()),
      mSize(aPlaneData.mSize()),
      mStride(aPlaneData.mStride()) {}

void GMPVideoi420FrameImpl::GMPFramePlane::InitPlaneData(
    GMPPlaneData& aPlaneData) {
  aPlaneData.mOffset() = mOffset;
  aPlaneData.mSize() = mSize;
  aPlaneData.mStride() = mStride;
}

void GMPVideoi420FrameImpl::GMPFramePlane::Copy(uint8_t* aDst,
                                                int32_t aDstOffset,
                                                const uint8_t* aSrc,
                                                int32_t aSize,
                                                int32_t aStride) {
  mOffset = aDstOffset;
  mSize = aSize;
  mStride = aStride;
  if (aDst && aSrc && aSize > 0) {
    memcpy(aDst + aDstOffset, aSrc, aSize);
  }
}

GMPVideoi420FrameImpl::GMPVideoi420FrameImpl(GMPVideoHostImpl* aHost)
    : mHost(aHost), mWidth(0), mHeight(0), mTimestamp(0ll), mDuration(0ll) {
  MOZ_ASSERT(aHost);
  aHost->DecodedFrameCreated(this);
}

GMPVideoi420FrameImpl::GMPVideoi420FrameImpl(
    const GMPVideoi420FrameData& aFrameData, ipc::Shmem&& aShmemBuffer,
    GMPVideoHostImpl* aHost)
    : mHost(aHost),
      mShmemBuffer(std::move(aShmemBuffer)),
      mYPlane(aFrameData.mYPlane()),
      mUPlane(aFrameData.mUPlane()),
      mVPlane(aFrameData.mVPlane()),
      mWidth(aFrameData.mWidth()),
      mHeight(aFrameData.mHeight()),
      mTimestamp(aFrameData.mTimestamp()),
      mUpdatedTimestamp(aFrameData.mUpdatedTimestamp()),
      mDuration(aFrameData.mDuration()) {
  MOZ_ASSERT(aHost);
  aHost->DecodedFrameCreated(this);
}

GMPVideoi420FrameImpl::GMPVideoi420FrameImpl(
    const GMPVideoi420FrameData& aFrameData, nsTArray<uint8_t>&& aArrayBuffer,
    GMPVideoHostImpl* aHost)
    : mHost(aHost),
      mArrayBuffer(std::move(aArrayBuffer)),
      mYPlane(aFrameData.mYPlane()),
      mUPlane(aFrameData.mUPlane()),
      mVPlane(aFrameData.mVPlane()),
      mWidth(aFrameData.mWidth()),
      mHeight(aFrameData.mHeight()),
      mTimestamp(aFrameData.mTimestamp()),
      mUpdatedTimestamp(aFrameData.mUpdatedTimestamp()),
      mDuration(aFrameData.mDuration()) {
  MOZ_ASSERT(aHost);
  aHost->DecodedFrameCreated(this);
}

GMPVideoi420FrameImpl::~GMPVideoi420FrameImpl() {
  DestroyBuffer();
  if (mHost) {
    mHost->DecodedFrameDestroyed(this);
  }
}

void GMPVideoi420FrameImpl::DoneWithAPI() {
  DestroyBuffer();

  // Do this after destroying the buffer because destruction
  // involves deallocation, which requires a host.
  mHost = nullptr;
}

void GMPVideoi420FrameImpl::InitFrameData(GMPVideoi420FrameData& aFrameData) {
  mYPlane.InitPlaneData(aFrameData.mYPlane());
  mUPlane.InitPlaneData(aFrameData.mUPlane());
  mVPlane.InitPlaneData(aFrameData.mVPlane());
  aFrameData.mWidth() = mWidth;
  aFrameData.mHeight() = mHeight;
  aFrameData.mTimestamp() = mTimestamp;
  aFrameData.mUpdatedTimestamp() = mUpdatedTimestamp;
  aFrameData.mDuration() = mDuration;
}

bool GMPVideoi420FrameImpl::InitFrameData(GMPVideoi420FrameData& aFrameData,
                                          ipc::Shmem& aShmemBuffer) {
  if (!mShmemBuffer.IsReadable()) {
    return false;
  }

  aShmemBuffer = mShmemBuffer;

  // This method is called right before Shmem is sent to another process.
  // We need to effectively zero out our member copy so that we don't
  // try to delete memory we don't own later.
  mShmemBuffer = ipc::Shmem();

  InitFrameData(aFrameData);
  return true;
}

bool GMPVideoi420FrameImpl::InitFrameData(GMPVideoi420FrameData& aFrameData,
                                          nsTArray<uint8_t>& aArrayBuffer) {
  if (mShmemBuffer.IsReadable()) {
    return false;
  }

  aArrayBuffer = std::move(mArrayBuffer);
  InitFrameData(aFrameData);
  return true;
}

GMPVideoFrameFormat GMPVideoi420FrameImpl::GetFrameFormat() {
  return kGMPI420VideoFrame;
}

void GMPVideoi420FrameImpl::Destroy() { delete this; }

/* static */
bool GMPVideoi420FrameImpl::CheckFrameData(
    const GMPVideoi420FrameData& aFrameData, size_t aBufferSize) {
  // We may be passed the "wrong" shmem (one smaller than the actual size).
  // This implies a bug or serious error on the child size.  Ignore this frame
  // if so. Note: Size() greater than expected is also an error, but with no
  // negative consequences
  int32_t half_width = (aFrameData.mWidth() + 1) / 2;
  if ((aFrameData.mYPlane().mStride() <= 0) ||
      (aFrameData.mYPlane().mSize() <= 0) ||
      (aFrameData.mYPlane().mOffset() < 0) ||
      (aFrameData.mUPlane().mStride() <= 0) ||
      (aFrameData.mUPlane().mSize() <= 0) ||
      (aFrameData.mUPlane().mOffset() <
       aFrameData.mYPlane().mOffset() + aFrameData.mYPlane().mSize()) ||
      (aFrameData.mVPlane().mStride() <= 0) ||
      (aFrameData.mVPlane().mSize() <= 0) ||
      (aFrameData.mVPlane().mOffset() <
       aFrameData.mUPlane().mOffset() + aFrameData.mUPlane().mSize()) ||
      (aBufferSize < static_cast<size_t>(aFrameData.mVPlane().mOffset()) +
                         static_cast<size_t>(aFrameData.mVPlane().mSize())) ||
      (aFrameData.mYPlane().mStride() < aFrameData.mWidth()) ||
      (aFrameData.mUPlane().mStride() < half_width) ||
      (aFrameData.mVPlane().mStride() < half_width) ||
      (aFrameData.mYPlane().mSize() <
       aFrameData.mYPlane().mStride() * aFrameData.mHeight()) ||
      (aFrameData.mUPlane().mSize() <
       aFrameData.mUPlane().mStride() * ((aFrameData.mHeight() + 1) / 2)) ||
      (aFrameData.mVPlane().mSize() <
       aFrameData.mVPlane().mStride() * ((aFrameData.mHeight() + 1) / 2))) {
    return false;
  }
  return true;
}

bool GMPVideoi420FrameImpl::CheckDimensions(int32_t aWidth, int32_t aHeight,
                                            int32_t aStride_y,
                                            int32_t aStride_u,
                                            int32_t aStride_v, int32_t aSize_y,
                                            int32_t aSize_u, int32_t aSize_v) {
  if (aWidth < 1 || aHeight < 1 || aStride_y < aWidth || aSize_y < 1 ||
      aSize_u < 1 || aSize_v < 1) {
    return false;
  }
  auto halfWidth = (CheckedInt<int32_t>(aWidth) + 1) / 2;
  if (!halfWidth.isValid() || aStride_u < halfWidth.value() ||
      aStride_v < halfWidth.value()) {
    return false;
  }
  auto height = CheckedInt<int32_t>(aHeight);
  auto halfHeight = (height + 1) / 2;
  auto minSizeY = height * aStride_y;
  auto minSizeU = halfHeight * aStride_u;
  auto minSizeV = halfHeight * aStride_v;
  auto totalSize = minSizeY + minSizeU + minSizeV;
  if (!minSizeY.isValid() || !minSizeU.isValid() || !minSizeV.isValid() ||
      !totalSize.isValid() || minSizeY.value() > aSize_y ||
      minSizeU.value() > aSize_u || minSizeV.value() > aSize_v) {
    return false;
  }
  return true;
}

bool GMPVideoi420FrameImpl::CheckDimensions(int32_t aWidth, int32_t aHeight,
                                            int32_t aStride_y,
                                            int32_t aStride_u,
                                            int32_t aStride_v) {
  int32_t half_width = (aWidth + 1) / 2;
  if (aWidth < 1 || aHeight < 1 || aStride_y < aWidth ||
      aStride_u < half_width || aStride_v < half_width ||
      !(CheckedInt<int32_t>(aHeight) * aStride_y +
        ((CheckedInt<int32_t>(aHeight) + 1) / 2) *
            (CheckedInt<int32_t>(aStride_u) + aStride_v))
           .isValid()) {
    return false;
  }
  return true;
}

const GMPVideoi420FrameImpl::GMPFramePlane* GMPVideoi420FrameImpl::GetPlane(
    GMPPlaneType aType) const {
  switch (aType) {
    case kGMPYPlane:
      return &mYPlane;
    case kGMPUPlane:
      return &mUPlane;
    case kGMPVPlane:
      return &mVPlane;
    default:
      MOZ_CRASH("Unknown plane type!");
  }
  return nullptr;
}

GMPVideoi420FrameImpl::GMPFramePlane* GMPVideoi420FrameImpl::GetPlane(
    GMPPlaneType aType) {
  switch (aType) {
    case kGMPYPlane:
      return &mYPlane;
    case kGMPUPlane:
      return &mUPlane;
    case kGMPVPlane:
      return &mVPlane;
    default:
      MOZ_CRASH("Unknown plane type!");
  }
  return nullptr;
}

GMPErr GMPVideoi420FrameImpl::MaybeResize(int32_t aNewSize) {
  if (aNewSize <= AllocatedSize()) {
    return GMPNoErr;
  }

  if (!mHost) {
    return GMPGenericErr;
  }

  if (!mArrayBuffer.IsEmpty()) {
    if (!mArrayBuffer.SetLength(aNewSize, fallible)) {
      return GMPAllocErr;
    }
    return GMPNoErr;
  }

  ipc::Shmem new_mem;
  if (!mHost->SharedMemMgr()->MgrTakeShmem(GMPSharedMemClass::Decoded, aNewSize,
                                           &new_mem) &&
      !mArrayBuffer.SetLength(aNewSize, fallible)) {
    return GMPAllocErr;
  }

  if (mShmemBuffer.IsReadable()) {
    if (new_mem.IsWritable()) {
      memcpy(new_mem.get<uint8_t>(), mShmemBuffer.get<uint8_t>(), aNewSize);
    }
    mHost->SharedMemMgr()->MgrGiveShmem(GMPSharedMemClass::Decoded,
                                        std::move(mShmemBuffer));
  }

  mShmemBuffer = new_mem;

  return GMPNoErr;
}

void GMPVideoi420FrameImpl::DestroyBuffer() {
  if (mHost && mShmemBuffer.IsWritable()) {
    mHost->SharedMemMgr()->MgrGiveShmem(GMPSharedMemClass::Decoded,
                                        std::move(mShmemBuffer));
  }
  mShmemBuffer = ipc::Shmem();
  mArrayBuffer.Clear();
}

GMPErr GMPVideoi420FrameImpl::CreateEmptyFrame(int32_t aWidth, int32_t aHeight,
                                               int32_t aStride_y,
                                               int32_t aStride_u,
                                               int32_t aStride_v) {
  if (!CheckDimensions(aWidth, aHeight, aStride_y, aStride_u, aStride_v)) {
    return GMPGenericErr;
  }

  int32_t size_y = aStride_y * aHeight;
  int32_t half_height = (aHeight + 1) / 2;
  int32_t size_u = aStride_u * half_height;
  int32_t size_v = aStride_v * half_height;

  int32_t bufferSize = size_y + size_u + size_v;
  GMPErr err = MaybeResize(bufferSize);
  if (err != GMPNoErr) {
    return err;
  }

  mYPlane.mOffset = 0;
  mYPlane.mSize = size_y;
  mYPlane.mStride = aStride_y;

  mUPlane.mOffset = size_y;
  mUPlane.mSize = size_u;
  mUPlane.mStride = aStride_u;

  mVPlane.mOffset = size_y + size_u;
  mVPlane.mSize = size_v;
  mVPlane.mStride = aStride_v;

  mWidth = aWidth;
  mHeight = aHeight;
  mTimestamp = 0ll;
  mUpdatedTimestamp.reset();
  mDuration = 0ll;

  return GMPNoErr;
}

GMPErr GMPVideoi420FrameImpl::CreateFrame(
    int32_t aSize_y, const uint8_t* aBuffer_y, int32_t aSize_u,
    const uint8_t* aBuffer_u, int32_t aSize_v, const uint8_t* aBuffer_v,
    int32_t aWidth, int32_t aHeight, int32_t aStride_y, int32_t aStride_u,
    int32_t aStride_v) {
  MOZ_ASSERT(aBuffer_y);
  MOZ_ASSERT(aBuffer_u);
  MOZ_ASSERT(aBuffer_v);

  if (!CheckDimensions(aWidth, aHeight, aStride_y, aStride_u, aStride_v,
                       aSize_y, aSize_u, aSize_v)) {
    return GMPGenericErr;
  }

  int32_t bufferSize = aSize_y + aSize_u + aSize_v;
  GMPErr err = MaybeResize(bufferSize);
  if (err != GMPNoErr) {
    return err;
  }

  uint8_t* bufferPtr = Buffer();
  mYPlane.Copy(bufferPtr, 0, aBuffer_y, aSize_y, aStride_y);
  mUPlane.Copy(bufferPtr, aSize_y, aBuffer_u, aSize_u, aStride_u);
  mVPlane.Copy(bufferPtr, aSize_y + aSize_u, aBuffer_v, aSize_v, aStride_v);

  mWidth = aWidth;
  mHeight = aHeight;

  return GMPNoErr;
}

GMPErr GMPVideoi420FrameImpl::CopyFrame(const GMPVideoi420Frame& aFrame) {
  auto& f = static_cast<const GMPVideoi420FrameImpl&>(aFrame);

  int32_t bufferSize = f.mYPlane.mSize + f.mUPlane.mSize + f.mVPlane.mSize;
  if (bufferSize != AllocatedSize()) {
    return GMPGenericErr;
  }

  GMPErr err = MaybeResize(bufferSize);
  if (err != GMPNoErr) {
    return err;
  }

  mYPlane = f.mYPlane;
  mUPlane = f.mUPlane;
  mVPlane = f.mVPlane;
  mWidth = f.mWidth;
  mHeight = f.mHeight;
  mTimestamp = f.mTimestamp;
  mUpdatedTimestamp = f.mUpdatedTimestamp;
  mDuration = f.mDuration;

  memcpy(Buffer(), f.Buffer(), bufferSize);

  return GMPNoErr;
}

void GMPVideoi420FrameImpl::SwapFrame(GMPVideoi420Frame* aFrame) {
  auto f = static_cast<GMPVideoi420FrameImpl*>(aFrame);
  mArrayBuffer.SwapElements(f->mArrayBuffer);
  std::swap(mShmemBuffer, f->mShmemBuffer);
  std::swap(mYPlane, f->mYPlane);
  std::swap(mUPlane, f->mUPlane);
  std::swap(mVPlane, f->mVPlane);
  std::swap(mWidth, f->mWidth);
  std::swap(mHeight, f->mHeight);
  std::swap(mTimestamp, f->mTimestamp);
  std::swap(mUpdatedTimestamp, f->mUpdatedTimestamp);
  std::swap(mDuration, f->mDuration);
}

uint8_t* GMPVideoi420FrameImpl::Buffer() {
  if (mShmemBuffer.IsWritable()) {
    return mShmemBuffer.get<uint8_t>();
  }
  if (!mArrayBuffer.IsEmpty()) {
    return mArrayBuffer.Elements();
  }
  return nullptr;
}

const uint8_t* GMPVideoi420FrameImpl::Buffer() const {
  if (mShmemBuffer.IsReadable()) {
    return mShmemBuffer.get<uint8_t>();
  }
  if (!mArrayBuffer.IsEmpty()) {
    return mArrayBuffer.Elements();
  }
  return nullptr;
}

uint8_t* GMPVideoi420FrameImpl::Buffer(GMPPlaneType aType) {
  if (auto* p = GetPlane(aType)) {
    if (auto* buffer = Buffer()) {
      return buffer + p->mOffset;
    }
  }
  return nullptr;
}

const uint8_t* GMPVideoi420FrameImpl::Buffer(GMPPlaneType aType) const {
  if (const auto* p = GetPlane(aType)) {
    if (const auto* buffer = Buffer()) {
      return buffer + p->mOffset;
    }
  }
  return nullptr;
}

int32_t GMPVideoi420FrameImpl::AllocatedSize() const {
  if (mShmemBuffer.IsWritable()) {
    return static_cast<int32_t>(mShmemBuffer.Size<uint8_t>());
  }
  return static_cast<int32_t>(mArrayBuffer.Length());
}

int32_t GMPVideoi420FrameImpl::AllocatedSize(GMPPlaneType aType) const {
  if (const auto* p = GetPlane(aType)) {
    return p->mSize;
  }
  return -1;
}

int32_t GMPVideoi420FrameImpl::Stride(GMPPlaneType aType) const {
  if (const auto* p = GetPlane(aType)) {
    return p->mStride;
  }
  return -1;
}

GMPErr GMPVideoi420FrameImpl::SetWidth(int32_t aWidth) {
  if (!CheckDimensions(aWidth, mHeight, mYPlane.mStride, mUPlane.mStride,
                       mVPlane.mStride)) {
    return GMPGenericErr;
  }
  mWidth = aWidth;
  return GMPNoErr;
}

GMPErr GMPVideoi420FrameImpl::SetHeight(int32_t aHeight) {
  if (!CheckDimensions(mWidth, aHeight, mYPlane.mStride, mUPlane.mStride,
                       mVPlane.mStride)) {
    return GMPGenericErr;
  }
  mHeight = aHeight;
  return GMPNoErr;
}

int32_t GMPVideoi420FrameImpl::Width() const { return mWidth; }

int32_t GMPVideoi420FrameImpl::Height() const { return mHeight; }

void GMPVideoi420FrameImpl::SetTimestamp(uint64_t aTimestamp) {
  mTimestamp = aTimestamp;
}

uint64_t GMPVideoi420FrameImpl::Timestamp() const { return mTimestamp; }

void GMPVideoi420FrameImpl::SetUpdatedTimestamp(uint64_t aTimestamp) {
  mUpdatedTimestamp = Some(aTimestamp);
}

uint64_t GMPVideoi420FrameImpl::UpdatedTimestamp() const {
  return mUpdatedTimestamp ? *mUpdatedTimestamp : mTimestamp;
}

void GMPVideoi420FrameImpl::SetDuration(uint64_t aDuration) {
  mDuration = aDuration;
}

uint64_t GMPVideoi420FrameImpl::Duration() const { return mDuration; }

bool GMPVideoi420FrameImpl::IsZeroSize() const {
  return (mYPlane.mSize == 0 && mUPlane.mSize == 0 && mVPlane.mSize == 0);
}

void GMPVideoi420FrameImpl::ResetSize() {
  mYPlane.mSize = 0;
  mUPlane.mSize = 0;
  mVPlane.mSize = 0;
}

}  // namespace mozilla::gmp
