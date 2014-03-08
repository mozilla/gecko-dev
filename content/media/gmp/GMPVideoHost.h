/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GMPVideoHost_h_
#define GMPVideoHost_h_

#include "gmp-video-host.h"
#include "gmp-video-plane.h"
#include "gmp-video-frame.h"
#include "gmp-video-host.h"

namespace mozilla {
namespace gmp {

class GMPSharedMemManager;
class GMPPlaneImpl;
class GMPVideoEncodedFrameImpl;

class GMPVideoHostImpl : public GMPVideoHost
{
public:
  GMPVideoHostImpl(GMPSharedMemManager* aSharedMemMgr);
  virtual ~GMPVideoHostImpl();

  // Used for shared memory allocation and deallocation.
  GMPSharedMemManager* SharedMemMgr();
  // Shared memory may have been deleted and cannot be allocated after
  // this is called. Prevent invalid access to existing memory and
  // prevent further allocations.
  void InvalidateShmem();
  void PlaneDestroyed(GMPPlaneImpl* aPlane);
  void EncodedFrameDestroyed(GMPVideoEncodedFrameImpl* aFrame);

  // GMPVideoHost
  virtual GMPVideoErr CreateFrame(GMPVideoFrameFormat aFormat, GMPVideoFrame** aFrame) MOZ_OVERRIDE;
  virtual GMPVideoErr CreatePlane(GMPPlane** aPlane) MOZ_OVERRIDE;
  virtual GMPVideoErr CreateEncodedFrame(GMPVideoEncodedFrame** aFrame) MOZ_OVERRIDE;

private:
  // All shared memory allocations have to be made by an IPDL actor.
  // This is a reference to the owning actor. If this reference is
  // null then the actor has died and all allocations must fail.
  GMPSharedMemManager* mSharedMemMgr;

  // We track all of these things because they need to handle further
  // allocations through us and we need to notify them when they
  // can't use us any more.
  nsTArray<GMPPlaneImpl*> mPlanes;
  nsTArray<GMPVideoEncodedFrameImpl*> mEncodedFrames;
};

} // namespace gmp
} // namespace mozilla

#endif // GMPVideoHost_h_
