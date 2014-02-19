/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GMPVideoEncoderParent.h"
#include <stdio.h>
#include "mozilla/unused.h"
#include "GMPMessageUtils.h"

namespace mozilla {
namespace gmp {

GMPVideoEncoderParent::GMPVideoEncoderParent(GMPParent *aPlugin)
: mPlugin(aPlugin),
  mObserver(nullptr),
  mVideoHost(this)
{
  MOZ_ASSERT(mPlugin);
}

GMPVideoEncoderParent::~GMPVideoEncoderParent()
{
}

GMPVideoHostImpl&
GMPVideoEncoderParent::Host()
{
  return mVideoHost;
}

bool
GMPVideoEncoderParent::MgrAllocShmem(size_t aSize,
                                     ipc::Shmem::SharedMemory::SharedMemoryType aType,
                                     ipc::Shmem* aMem)
{
  return AllocShmem(aSize, aType, aMem);
}

bool
GMPVideoEncoderParent::MgrDeallocShmem(Shmem& aMem)
{
  return DeallocShmem(aMem);
}

GMPVideoErr
GMPVideoEncoderParent::InitEncode(const GMPVideoCodec& aCodecSettings,
                                  GMPEncoderCallback* aCallback,
                                  int32_t aNumberOfCores,
                                  uint32_t aMaxPayloadSize)
{
  if (!mPlugin) {
    NS_WARNING("Trying to use an invalid GMP video encoder!");
    return GMPVideoGenericErr;
  }

  if (!aCallback) {
    return GMPVideoGenericErr;
  }
  mObserver = aCallback;

  if (!SendInitEncode(aCodecSettings, aNumberOfCores, aMaxPayloadSize)) {
    return GMPVideoGenericErr;
  }

  // Async IPC, always return no error here. A real failure will
  // terminate subprocess.
  return GMPVideoNoErr;
}

GMPVideoErr
GMPVideoEncoderParent::Encode(GMPVideoi420Frame& aInputFrame,
                              const GMPCodecSpecificInfo& aCodecSpecificInfo,
                              const std::vector<GMPVideoFrameType>* aFrameTypes)
{
  if (!mPlugin) {
    NS_WARNING("Trying to use an invalid GMP video encoder!");
    return GMPVideoGenericErr;
  }

  auto& inputFrameImpl = static_cast<GMPVideoi420FrameImpl&>(aInputFrame);

  ipc::Shmem* yShmem = nullptr;
  ipc::Shmem* uShmem = nullptr;
  ipc::Shmem* vShmem = nullptr;
  inputFrameImpl.ExtractShmem(&yShmem, &uShmem, &vShmem);
  if (!yShmem || !uShmem || !vShmem) {
    return GMPVideoGenericErr;
  }

  //XXXJOSH create proper array here
  InfallibleTArray<int> foo;

  if (!SendEncode(inputFrameImpl,
                  *yShmem, *uShmem, *vShmem,
                  aCodecSpecificInfo,
                  foo)) {
    return GMPVideoGenericErr;
  }

  // Async IPC, always return no error here. A real failure will
  // terminate subprocess.
  return GMPVideoNoErr;
}

GMPVideoErr
GMPVideoEncoderParent::SetChannelParameters(uint32_t aPacketLoss, uint32_t aRTT)
{
  if (!mPlugin) {
    NS_WARNING("Trying to use an invalid GMP video encoder!");
    return GMPVideoGenericErr;
  }

  if (!SendSetChannelParameters(aPacketLoss, aRTT)) {
    return GMPVideoGenericErr;
  }

  // Async IPC, always return no error here. A real failure will
  // terminate subprocess.
  return GMPVideoNoErr;
}

GMPVideoErr
GMPVideoEncoderParent::SetRates(uint32_t aNewBitRate, uint32_t aFrameRate)
{
  if (!mPlugin) {
    NS_WARNING("Trying to use an invalid GMP video encoder!");
    return GMPVideoGenericErr;
  }

  if (!SendSetRates(aNewBitRate, aFrameRate)) {
    return GMPVideoGenericErr;
  }

  // Async IPC, always return no error here. A real failure will
  // terminate subprocess.
  return GMPVideoNoErr;
}

GMPVideoErr
GMPVideoEncoderParent::SetPeriodicKeyFrames(bool aEnable)
{
  if (!mPlugin) {
    NS_WARNING("Trying to use an invalid GMP video encoder!");
    return GMPVideoGenericErr;
  }

  if (!SendSetPeriodicKeyFrames(aEnable)) {
    return GMPVideoGenericErr;
  }

  // Async IPC, always return no error here. A real failure will
  // terminate subprocess.
  return GMPVideoNoErr;
}

void
GMPVideoEncoderParent::EncodingComplete()
{
  if (!mPlugin) {
    NS_WARNING("Trying to use an invalid GMP video encoder!");
    return;
  }

  mObserver = nullptr;

  mVideoHost.InvalidateShmem();

  //XXXJOSH
  // After we send this the process can get shut down from our side,
  // before the child has processed it. We need the child to process it
  // so that the child can release any outstanding shared memory.
  unused << SendEncodingComplete();

  mPlugin->VideoEncoderDestroyed(this);
  mPlugin = nullptr;
}

bool
GMPVideoEncoderParent::RecvEncoded(const GMPVideoEncodedFrameImpl& aEncodedFrame,
                                   Shmem& aEncodedFrameBuffer,
                                   const GMPCodecSpecificInfo& aCodecSpecificInfo)
{
  if (!mPlugin) {
    NS_WARNING("Trying to use a destroyed GMP API object!");
    return false;
  }

  if (!mObserver) {
    return false;
  }

  // We need a mutable copy of the decoded frame, into which we can
  // inject the shared memory backing.
  auto f = new GMPVideoEncodedFrameImpl();
  if (!f) {
    return false;
  }

  f->CopyFrame(aEncodedFrame);
  f->ReceiveShmem(aEncodedFrameBuffer);

  mObserver->Encoded(*f, aCodecSpecificInfo);

  f->Destroy();

  return true;
}

} // namespace gmp
} // namespace mozilla
