/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GMPVideoEncoderParent.h"

#include "GMPContentParent.h"
#include "GMPCrashHelper.h"
#include "GMPLog.h"
#include "GMPMessageUtils.h"
#include "GMPVideoEncodedFrameImpl.h"
#include "GMPVideoi420FrameImpl.h"
#include "mozilla/gmp/GMPTypes.h"
#include "mozilla/Unused.h"
#include "nsAutoRef.h"
#include "nsThread.h"
#include "nsThreadUtils.h"
#include "runnable_utils.h"

namespace mozilla::gmp {

#ifdef __CLASS__
#  undef __CLASS__
#endif
#define __CLASS__ "GMPVideoEncoderParent"

// States:
// Initial: mIsOpen == false
//    on InitDecode success -> Open
//    on Shutdown -> Dead
// Open: mIsOpen == true
//    on Close -> Dead
//    on ActorDestroy -> Dead
//    on Shutdown -> Dead
// Dead: mIsOpen == false

GMPVideoEncoderParent::GMPVideoEncoderParent(GMPContentParent* aPlugin)
    : mIsOpen(false),
      mShuttingDown(false),
      mActorDestroyed(false),
      mPlugin(aPlugin),
      mCallback(nullptr),
      mVideoHost(this),
      mPluginId(aPlugin->GetPluginId()) {
  MOZ_ASSERT(mPlugin);
}

bool GMPVideoEncoderParent::MgrIsOnOwningThread() const {
  return !mPlugin || mPlugin->GMPEventTarget()->IsOnCurrentThread();
}

GMPVideoHostImpl& GMPVideoEncoderParent::Host() { return mVideoHost; }

// Note: may be called via Terminated()
void GMPVideoEncoderParent::Close() {
  GMP_LOG_DEBUG("%s::%s: %p", __CLASS__, __FUNCTION__, this);
  MOZ_ASSERT(mPlugin->GMPEventTarget()->IsOnCurrentThread());
  // Consumer is done with us; we can shut down.  No more callbacks should
  // be made to mCallback.  Note: do this before Shutdown()!
  mCallback = nullptr;

  // Let Shutdown mark us as dead so it knows if we had been alive

  // In case this is the last reference
  RefPtr<GMPVideoEncoderParent> kungfudeathgrip(this);
  Release();
  Shutdown();
}

GMPErr GMPVideoEncoderParent::InitEncode(
    const GMPVideoCodec& aCodecSettings,
    const nsTArray<uint8_t>& aCodecSpecific,
    GMPVideoEncoderCallbackProxy* aCallback, int32_t aNumberOfCores,
    uint32_t aMaxPayloadSize) {
  GMP_LOG_DEBUG("%s::%s: %p", __CLASS__, __FUNCTION__, this);
  if (mIsOpen) {
    NS_WARNING("Trying to re-init an in-use GMP video encoder!");
    return GMPGenericErr;
    ;
  }

  MOZ_ASSERT(mPlugin->GMPEventTarget()->IsOnCurrentThread());
  MOZ_ASSERT(!mCallback);

  if (!aCallback) {
    return GMPGenericErr;
  }
  mCallback = aCallback;

  if (!SendInitEncode(aCodecSettings, aCodecSpecific, aNumberOfCores,
                      aMaxPayloadSize)) {
    return GMPGenericErr;
  }
  mIsOpen = true;

  // Async IPC, we don't have access to a return value.
  return GMPNoErr;
}

GMPErr GMPVideoEncoderParent::Encode(
    GMPUniquePtr<GMPVideoi420Frame> aInputFrame,
    const nsTArray<uint8_t>& aCodecSpecificInfo,
    const nsTArray<GMPVideoFrameType>& aFrameTypes) {
  if (!mIsOpen) {
    NS_WARNING("Trying to use an dead GMP video encoder");
    return GMPGenericErr;
  }

  MOZ_ASSERT(mPlugin->GMPEventTarget()->IsOnCurrentThread());

  GMPUniquePtr<GMPVideoi420FrameImpl> inputFrameImpl(
      static_cast<GMPVideoi420FrameImpl*>(aInputFrame.release()));

  GMPVideoi420FrameData frameData;
  ipc::Shmem frameShmem;
  if (!inputFrameImpl->InitFrameData(frameData, frameShmem)) {
    GMP_LOG_ERROR("%s::%s: failed to init frame data", __CLASS__, __FUNCTION__);
    return GMPGenericErr;
  }

  if (mEncodedShmemSize > 0) {
    if (GMPSharedMemManager* memMgr = mVideoHost.SharedMemMgr()) {
      ipc::Shmem outputShmem;
      if (memMgr->MgrTakeShmem(GMPSharedMemClass::Encoded, mEncodedShmemSize,
                               &outputShmem)) {
        Unused << SendGiveShmem(std::move(outputShmem));
      }
    }
  }

  if (!SendEncode(frameData, std::move(frameShmem), aCodecSpecificInfo,
                  aFrameTypes)) {
    GMP_LOG_ERROR("%s::%s: failed to send encode", __CLASS__, __FUNCTION__);
    return GMPGenericErr;
  }

  // Async IPC, we don't have access to a return value.
  return GMPNoErr;
}

GMPErr GMPVideoEncoderParent::SetChannelParameters(uint32_t aPacketLoss,
                                                   uint32_t aRTT) {
  if (!mIsOpen) {
    NS_WARNING("Trying to use an invalid GMP video encoder!");
    return GMPGenericErr;
  }

  MOZ_ASSERT(mPlugin->GMPEventTarget()->IsOnCurrentThread());

  if (!SendSetChannelParameters(aPacketLoss, aRTT)) {
    return GMPGenericErr;
  }

  // Async IPC, we don't have access to a return value.
  return GMPNoErr;
}

GMPErr GMPVideoEncoderParent::SetRates(uint32_t aNewBitRate,
                                       uint32_t aFrameRate) {
  if (!mIsOpen) {
    NS_WARNING("Trying to use an dead GMP video decoder");
    return GMPGenericErr;
  }

  MOZ_ASSERT(mPlugin->GMPEventTarget()->IsOnCurrentThread());

  if (!SendSetRates(aNewBitRate, aFrameRate)) {
    return GMPGenericErr;
  }

  // Async IPC, we don't have access to a return value.
  return GMPNoErr;
}

GMPErr GMPVideoEncoderParent::SetPeriodicKeyFrames(bool aEnable) {
  if (!mIsOpen) {
    NS_WARNING("Trying to use an invalid GMP video encoder!");
    return GMPGenericErr;
  }

  MOZ_ASSERT(mPlugin->GMPEventTarget()->IsOnCurrentThread());

  if (!SendSetPeriodicKeyFrames(aEnable)) {
    return GMPGenericErr;
  }

  // Async IPC, we don't have access to a return value.
  return GMPNoErr;
}

// Note: Consider keeping ActorDestroy sync'd up when making changes here.
void GMPVideoEncoderParent::Shutdown() {
  GMP_LOG_DEBUG("%s::%s: %p", __CLASS__, __FUNCTION__, this);
  MOZ_ASSERT(mPlugin->GMPEventTarget()->IsOnCurrentThread());

  if (mShuttingDown) {
    return;
  }
  mShuttingDown = true;

  // Notify client we're gone!  Won't occur after Close()
  if (mCallback) {
    mCallback->Terminated();
    mCallback = nullptr;
  }

  mIsOpen = false;
  if (!mActorDestroyed) {
    Unused << Send__delete__(this);
  }
}

// Note: Keep this sync'd up with Shutdown
void GMPVideoEncoderParent::ActorDestroy(ActorDestroyReason aWhy) {
  GMP_LOG_DEBUG("%s::%s: %p (%d)", __CLASS__, __FUNCTION__, this, (int)aWhy);
  mIsOpen = false;
  mActorDestroyed = true;
  if (mCallback) {
    // May call Close() (and Shutdown()) immediately or with a delay
    mCallback->Terminated();
    mCallback = nullptr;
  }
  if (mPlugin) {
    // Ignore any return code. It is OK for this to fail without killing the
    // process.
    mPlugin->VideoEncoderDestroyed(this);
    mPlugin = nullptr;
  }
  mVideoHost.ActorDestroyed();  // same as DoneWithAPI
  MaybeDisconnect(aWhy == AbnormalShutdown);
}

mozilla::ipc::IPCResult GMPVideoEncoderParent::RecvReturnShmem(
    ipc::Shmem&& aInputShmem) {
  if (GMPSharedMemManager* memMgr = mVideoHost.SharedMemMgr()) {
    memMgr->MgrGiveShmem(GMPSharedMemClass::Decoded, std::move(aInputShmem));
  } else {
    DeallocShmem(aInputShmem);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult GMPVideoEncoderParent::RecvEncodedShmem(
    const GMPVideoEncodedFrameData& aEncodedFrame, ipc::Shmem&& aEncodedShmem,
    nsTArray<uint8_t>&& aCodecSpecificInfo) {
  if (mCallback && GMPVideoEncodedFrameImpl::CheckFrameData(
                       aEncodedFrame, aEncodedShmem.Size<uint8_t>())) {
    auto* f = new GMPVideoEncodedFrameImpl(
        aEncodedFrame, std::move(aEncodedShmem), &mVideoHost);
    mCallback->Encoded(f, aCodecSpecificInfo);
    f->Destroy();
  } else {
    DeallocShmem(aEncodedShmem);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult GMPVideoEncoderParent::RecvEncodedData(
    const GMPVideoEncodedFrameData& aEncodedFrame,
    nsTArray<uint8_t>&& aEncodedData, nsTArray<uint8_t>&& aCodecSpecificInfo) {
  if (mCallback && GMPVideoEncodedFrameImpl::CheckFrameData(
                       aEncodedFrame, aEncodedData.Length())) {
    mEncodedShmemSize = std::max(mEncodedShmemSize, aEncodedData.Length());
    auto* f = new GMPVideoEncodedFrameImpl(
        aEncodedFrame, std::move(aEncodedData), &mVideoHost);
    mCallback->Encoded(f, aCodecSpecificInfo);
    f->Destroy();
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult GMPVideoEncoderParent::RecvError(const GMPErr& aError) {
  if (mCallback) {
    mCallback->Error(aError);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult GMPVideoEncoderParent::RecvShutdown() {
  Shutdown();
  return IPC_OK();
}

}  // namespace mozilla::gmp

#undef __CLASS__
