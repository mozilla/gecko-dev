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
: mCanSendMessages(true),
  mPlugin(aPlugin),
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
  if (!mCanSendMessages) {
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
GMPVideoDecoderParent::Decode(GMPVideoEncodedFrame* aInputFrame,
                              bool aMissingFrames,
                              const GMPCodecSpecificInfo& aCodecSpecificInfo,
                              int64_t aRenderTimeMs)
{
  if (!mCanSendMessages) {
    NS_WARNING("Trying to use an invalid GMP video decoder!");
    return GMPVideoGenericErr;
  }

  auto inputFrameImpl = static_cast<GMPVideoEncodedFrameImpl*>(aInputFrame);

  ipc::Shmem* encodedFrameShmem = nullptr;
  inputFrameImpl->ExtractShmem(&encodedFrameShmem);
  if (!encodedFrameShmem) {
    return GMPVideoGenericErr;
  }

  if (!SendDecode(*inputFrameImpl,
                  aMissingFrames,
                  aCodecSpecificInfo,
                  aRenderTimeMs,
                  *encodedFrameShmem)) {
    return GMPVideoGenericErr;
  }

  aInputFrame->Destroy();

  // Async IPC, always return no error here. A real failure will
  // terminate subprocess.
  return GMPVideoNoErr;
}

GMPVideoErr
GMPVideoDecoderParent::Reset()
{
  if (!mCanSendMessages) {
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
  if (!mCanSendMessages) {
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
  if (!mCanSendMessages) {
    NS_WARNING("Trying to use an invalid GMP video decoder!");
    return;
  }

  mCanSendMessages = false;

  mObserver = nullptr;

  mVideoHost.InvalidateShmem();

  unused << SendDecodingComplete();
}

bool
GMPVideoDecoderParent::RecvDecoded(const GMPVideoi420FrameImpl& aDecodedFrame,
                                   Shmem& aYShmem,
                                   Shmem& aUShmem,
                                   Shmem& aVShmem)
{
  if (!mObserver) {
    return false;
  }

  // We need a mutable copy of the decoded frame, into which we can
  // inject the shared memory backing.
  auto f = new GMPVideoi420FrameImpl();
  if (!f) {
    return false;
  }

  f->SetHost(&mVideoHost);

  GMPVideoErr err = f->CopyFrame(aDecodedFrame);
  if (err != GMPVideoNoErr) {
    return false;
  }

  f->ReceiveShmem(aYShmem, aUShmem, aVShmem);

  mObserver->Decoded(f);

  return true;
}

bool
GMPVideoDecoderParent::RecvReceivedDecodedReferenceFrame(const uint64_t& aPictureId)
{
  if (!mObserver) {
    return false;
  }

  mObserver->ReceivedDecodedReferenceFrame(aPictureId);

  return true;
}

bool
GMPVideoDecoderParent::RecvReceivedDecodedFrame(const uint64_t& aPictureId)
{
  if (!mObserver) {
    return false;
  }

  mObserver->ReceivedDecodedFrame(aPictureId);

  return true;
}

bool
GMPVideoDecoderParent::RecvInputDataExhausted()
{
  if (!mObserver) {
    return false;
  }

  mObserver->InputDataExhausted();

  return true;
}

bool
GMPVideoDecoderParent::Recv__delete__()
{
  if (mPlugin) {
    mPlugin->VideoDecoderDestroyed(this);
    mPlugin = nullptr;
  }
  return true;
}

} // namespace gmp
} // namespace mozilla
