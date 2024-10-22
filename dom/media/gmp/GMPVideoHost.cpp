/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GMPVideoHost.h"
#include "mozilla/Assertions.h"
#include "GMPSharedMemManager.h"
#include "GMPVideoPlaneImpl.h"
#include "GMPVideoi420FrameImpl.h"
#include "GMPVideoEncodedFrameImpl.h"

namespace mozilla::gmp {

GMPVideoHostImpl::GMPVideoHostImpl(GMPSharedMemManager* aSharedMemMgr)
    : mSharedMemMgr(aSharedMemMgr) {}

GMPVideoHostImpl::~GMPVideoHostImpl() = default;

GMPErr GMPVideoHostImpl::CreateFrame(GMPVideoFrameFormat aFormat,
                                     GMPVideoFrame** aFrame) {
  if (!mSharedMemMgr) {
    return GMPGenericErr;
  }

  if (!aFrame) {
    return GMPGenericErr;
  }
  *aFrame = nullptr;

  switch (aFormat) {
    case kGMPI420VideoFrame:
      *aFrame = new GMPVideoi420FrameImpl(this);
      return GMPNoErr;
    case kGMPEncodedVideoFrame:
      *aFrame = new GMPVideoEncodedFrameImpl(this);
      return GMPNoErr;
    default:
      MOZ_ASSERT_UNREACHABLE("Unknown frame format!");
  }

  return GMPGenericErr;
}

GMPErr GMPVideoHostImpl::CreatePlane(GMPPlane** aPlane) {
  if (!mSharedMemMgr) {
    return GMPGenericErr;
  }

  if (!aPlane) {
    return GMPGenericErr;
  }
  *aPlane = nullptr;

  auto* p = new GMPPlaneImpl();

  *aPlane = p;

  return GMPNoErr;
}

GMPSharedMemManager* GMPVideoHostImpl::SharedMemMgr() { return mSharedMemMgr; }

// XXX This should merge with ActorDestroyed
void GMPVideoHostImpl::DoneWithAPI() { ActorDestroyed(); }

void GMPVideoHostImpl::ActorDestroyed() {
  for (uint32_t i = mEncodedFrames.Length(); i > 0; i--) {
    mEncodedFrames[i - 1]->DoneWithAPI();
    mEncodedFrames.RemoveElementAt(i - 1);
  }
  for (uint32_t i = mDecodedFrames.Length(); i > 0; i--) {
    mDecodedFrames[i - 1]->DoneWithAPI();
    mDecodedFrames.RemoveElementAt(i - 1);
  }
  mSharedMemMgr->MgrPurgeShmems();
  mSharedMemMgr = nullptr;
}

void GMPVideoHostImpl::EncodedFrameCreated(
    GMPVideoEncodedFrameImpl* aEncodedFrame) {
  mEncodedFrames.AppendElement(aEncodedFrame);
}

void GMPVideoHostImpl::EncodedFrameDestroyed(GMPVideoEncodedFrameImpl* aFrame) {
  MOZ_ALWAYS_TRUE(mEncodedFrames.RemoveElement(aFrame));
}

void GMPVideoHostImpl::DecodedFrameCreated(
    GMPVideoi420FrameImpl* aDecodedFrame) {
  mDecodedFrames.AppendElement(aDecodedFrame);
}

void GMPVideoHostImpl::DecodedFrameDestroyed(GMPVideoi420FrameImpl* aFrame) {
  MOZ_ALWAYS_TRUE(mDecodedFrames.RemoveElement(aFrame));
}

}  // namespace mozilla::gmp
