/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GMPVideoDecoderParent.h"
#include <stdio.h>
#include "mozilla/unused.h"
#include "GMPMessageUtils.h"

namespace mozilla {
namespace gmp {

GMPVideoDecoderParent::GMPVideoDecoderParent(GMPParent *aPlugin)
: mPlugin(aPlugin),
  mObserver(nullptr),
  mVideoHost(this)
{
  MOZ_ASSERT(mPlugin);
}

GMPVideoDecoderParent::~GMPVideoDecoderParent()
{
}

GMPVideoHostImpl&
GMPVideoDecoderParent::Host()
{
  return mVideoHost;
}

bool
GMPVideoDecoderParent::MgrAllocShmem(size_t aSize,
                                     ipc::Shmem::SharedMemory::SharedMemoryType aType,
                                     ipc::Shmem* aMem)
{
  return AllocShmem(aSize, aType, aMem);
}

bool
GMPVideoDecoderParent::MgrDeallocShmem(Shmem& aMem)
{
  return DeallocShmem(aMem);
}

GMPVideoErr
GMPVideoDecoderParent::InitDecode(const GMPVideoCodec& aCodecSettings,
                                  GMPDecoderCallback* aCallback,
                                  int32_t aCoreCount)
{
  if (!mPlugin) {
    NS_WARNING("Trying to use an invalid GMP video decoder!");
    return GMPVideoGenericErr;
  }

  if (!aCallback) {
    return GMPVideoGenericErr;
  }
  mObserver = aCallback;

  if (!SendInitDecode(aCodecSettings, aCoreCount)) {
    return GMPVideoGenericErr;
  }

  // Async IPC, always return no error here. A real failure will
  // terminate subprocess.
  return GMPVideoNoErr;
}

GMPVideoErr
GMPVideoDecoderParent::Decode(GMPVideoEncodedFrame& aInputFrame,
                              bool aMissingFrames,
                              const GMPCodecSpecificInfo& aCodecSpecificInfo,
                              int64_t aRenderTimeMs)
{
  if (!mPlugin) {
    NS_WARNING("Trying to use an invalid GMP video decoder!");
    return GMPVideoGenericErr;
  }

  auto& inputFrameImpl = static_cast<GMPVideoEncodedFrameImpl&>(aInputFrame);

  ipc::Shmem* encodedFrameShmem = nullptr;
  inputFrameImpl.ExtractShmem(&encodedFrameShmem);
  if (!encodedFrameShmem) {
    return GMPVideoGenericErr;
  }

  if (!SendDecode(inputFrameImpl,
                  aMissingFrames,
                  aCodecSpecificInfo,
                  aRenderTimeMs,
                  *encodedFrameShmem)) {
    return GMPVideoGenericErr;
  }

  // Async IPC, always return no error here. A real failure will
  // terminate subprocess.
  return GMPVideoNoErr;
}

GMPVideoErr
GMPVideoDecoderParent::Reset()
{
  if (!mPlugin) {
    NS_WARNING("Trying to use an invalid GMP video decoder!");
    return GMPVideoGenericErr;
  }

  if (!SendReset()) {
    return GMPVideoGenericErr;
  }

  // Async IPC, always return no error here. A real failure will
  // terminate subprocess.
  return GMPVideoNoErr;
}

GMPVideoErr
GMPVideoDecoderParent::Drain()
{
  if (!mPlugin) {
    NS_WARNING("Trying to use an invalid GMP video decoder!");
    return GMPVideoGenericErr;
  }

  if (!SendDrain()) {
    return GMPVideoGenericErr;
  }

  // Async IPC, always return no error here. A real failure will
  // terminate subprocess.
  return GMPVideoNoErr;
}

void
GMPVideoDecoderParent::DecodingComplete()
{
  if (!mPlugin) {
    NS_WARNING("Trying to use an invalid GMP video decoder!");
    return;
  }

  mObserver = nullptr;

  mVideoHost.InvalidateShmem();

  //XXXJOSH
  // After we send this the process can get shut down from our side,
  // before the child has processed it. We need the child to process it
  // so that the child can release any outstanding shared memory.
  unused << SendDecodingComplete();

  mPlugin->VideoDecoderDestroyed(this);
  mPlugin = nullptr;
}

bool
GMPVideoDecoderParent::RecvDecoded(const GMPVideoi420FrameImpl& aDecodedFrame,
                                   Shmem& aYShmem,
                                   Shmem& aUShmem,
                                   Shmem& aVShmem)
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
  auto f = new GMPVideoi420FrameImpl();
  if (!f) {
    return false;
  }

  f->CopyFrame(aDecodedFrame);
  f->ReceiveShmem(aYShmem, aUShmem, aVShmem);

  mObserver->Decoded(*f);

  f->Destroy();

  return true;
}

bool
GMPVideoDecoderParent::RecvReceivedDecodedReferenceFrame(const uint64_t& aPictureId)
{
  if (!mPlugin) {
    NS_WARNING("Trying to use a destroyed GMP API object!");
    return false;
  }

  if (!mObserver) {
    return false;
  }

  mObserver->ReceivedDecodedReferenceFrame(aPictureId);

  return true;
}

bool
GMPVideoDecoderParent::RecvReceivedDecodedFrame(const uint64_t& aPictureId)
{
  if (!mPlugin) {
    NS_WARNING("Trying to use a destroyed GMP API object!");
    return false;
  }

  if (!mObserver) {
    return false;
  }

  mObserver->ReceivedDecodedFrame(aPictureId);

  return true;
}

bool
GMPVideoDecoderParent::RecvInputDataExhausted()
{
  if (!mPlugin) {
    NS_WARNING("Trying to use a destroyed GMP API object!");
    return false;
  }

  if (!mObserver) {
    return false;
  }

  mObserver->InputDataExhausted();

  return true;
}

} // namespace gmp
} // namespace mozilla
