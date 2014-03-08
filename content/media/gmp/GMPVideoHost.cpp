/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GMPVideoHost.h"
#include "mozilla/Assertions.h"

namespace mozilla {
namespace gmp {

GMPVideoHostImpl::GMPVideoHostImpl(GMPSharedMemManager* aSharedMemMgr)
: mSharedMemMgr(aSharedMemMgr)
{
}

GMPVideoHostImpl::~GMPVideoHostImpl()
{
}

GMPVideoErr
GMPVideoHostImpl::CreateFrame(GMPVideoFrameFormat aFormat, GMPVideoFrame** aFrame)
{
  if (!mSharedMemMgr) {
    return GMPVideoGenericErr;
  }

  if (!aFrame) {
    return GMPVideoGenericErr;
  }
  *aFrame = nullptr;

  if (aFormat == kGMPI420VideoFrame) {
    auto f = new GMPVideoi420FrameImpl();
    if (f) {
      f->SetHost(this);
      *aFrame = f;
      return GMPVideoNoErr;
    }
  }

  return GMPVideoGenericErr;
}

GMPVideoErr
GMPVideoHostImpl::CreatePlane(GMPPlane** aPlane)
{
  if (!mSharedMemMgr) {
    return GMPVideoGenericErr;
  }

  if (!aPlane) {
    return GMPVideoGenericErr;
  }
  *aPlane = nullptr;

  auto p = new GMPPlaneImpl();
  if (!p) {
    return GMPVideoAllocErr;
  }

  p->SetHost(this);
  *aPlane = p;
  mPlanes.AppendElement(p);

  return GMPVideoNoErr;
}

GMPVideoErr
GMPVideoHostImpl::CreateEncodedFrame(GMPVideoEncodedFrame** aFrame)
{
  if (!mSharedMemMgr) {
    return GMPVideoGenericErr;
  }

  if (!aFrame) {
    return GMPVideoGenericErr;
  }
  *aFrame = nullptr;

  auto f = new GMPVideoEncodedFrameImpl();
  if (!f) {
    return GMPVideoAllocErr;
  }

  f->SetHost(this);
  *aFrame = f;
  mEncodedFrames.AppendElement(f);

  return GMPVideoNoErr;
}

GMPSharedMemManager*
GMPVideoHostImpl::SharedMemMgr()
{
  return mSharedMemMgr;
}

void
GMPVideoHostImpl::InvalidateShmem()
{
  for (uint32_t i = 0; i < mPlanes.Length(); i++) {
    mPlanes[i]->InvalidateShmem();
    mPlanes.RemoveElementAt(i);
  }
  for (uint32_t i = 0; i < mEncodedFrames.Length(); i++) {
    mEncodedFrames[i]->InvalidateShmem();
    mEncodedFrames.RemoveElementAt(i);
  }
  mSharedMemMgr = nullptr;
}

void
GMPVideoHostImpl::PlaneDestroyed(GMPPlaneImpl* aPlane)
{
  mPlanes.RemoveElement(aPlane);
}

void
GMPVideoHostImpl::EncodedFrameDestroyed(GMPVideoEncodedFrameImpl* aFrame)
{
  mEncodedFrames.RemoveElement(aFrame);
}

} // namespace gmp
} // namespace mozilla
