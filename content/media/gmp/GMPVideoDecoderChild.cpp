/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GMPVideoDecoderChild.h"
#include <stdio.h>
#include "mozilla/unused.h"

namespace mozilla {
namespace gmp {

GMPVideoDecoderChild::GMPVideoDecoderChild()
: mVideoDecoder(nullptr),
  mVideoHost(this)
{
}

GMPVideoDecoderChild::~GMPVideoDecoderChild()
{
}

void
GMPVideoDecoderChild::Init(GMPVideoDecoder* aDecoder)
{
  MOZ_ASSERT(aDecoder, "Cannot initialize video decoder child without a video decoder!");
  mVideoDecoder = aDecoder;
}

GMPVideoHostImpl&
GMPVideoDecoderChild::Host()
{
  return mVideoHost;
}

void
GMPVideoDecoderChild::Decoded(GMPVideoi420Frame* decodedFrame)
{
  auto df = static_cast<GMPVideoi420FrameImpl*>(decodedFrame);
  ipc::Shmem* yShmem = nullptr;
  ipc::Shmem* uShmem = nullptr;
  ipc::Shmem* vShmem = nullptr;
  df->ExtractShmem(&yShmem, &uShmem, &vShmem);
  SendDecoded(*df, *yShmem, *uShmem, *vShmem);
  decodedFrame->Destroy();
}

void
GMPVideoDecoderChild::ReceivedDecodedReferenceFrame(const uint64_t pictureId)
{
  SendReceivedDecodedReferenceFrame(pictureId);
}

void
GMPVideoDecoderChild::ReceivedDecodedFrame(const uint64_t pictureId)
{
  SendReceivedDecodedFrame(pictureId);
}

void
GMPVideoDecoderChild::InputDataExhausted()
{
  SendInputDataExhausted();
}

bool
GMPVideoDecoderChild::MgrAllocShmem(size_t aSize,
                                    ipc::Shmem::SharedMemory::SharedMemoryType aType,
                                    ipc::Shmem* aMem)
{
  return AllocShmem(aSize, aType, aMem);
}

bool
GMPVideoDecoderChild::MgrDeallocShmem(Shmem& aMem)
{
  return DeallocShmem(aMem);
}

bool
GMPVideoDecoderChild::RecvInitDecode(const GMPVideoCodec& codecSettings,
                                     const int32_t& coreCount)
{
  if (!mVideoDecoder) {
    return false;
  }

  mVideoDecoder->InitDecode(codecSettings, this, coreCount);

  return true;
}

bool
GMPVideoDecoderChild::RecvDecode(const GMPVideoEncodedFrameImpl& inputFrame,
                                 const bool& missingFrames,
                                 const GMPCodecSpecificInfo& codecSpecificInfo,
                                 const int64_t& renderTimeMs,
                                 Shmem& aEncodedFrameShmem)
{
  if (!mVideoDecoder) {
    return false;
  }

  // We need a mutable copy of the decoded frame, into which we can inject
  // the shared memory backing.
  auto frame = new GMPVideoEncodedFrameImpl();
  if (!frame) {
    return false;
  }

  frame->SetHost(&mVideoHost);

  GMPVideoErr err = frame->CopyFrame(inputFrame);
  if (err != GMPVideoNoErr) {
    return false;
  }

  frame->ReceiveShmem(aEncodedFrameShmem);

  mVideoDecoder->Decode(frame, missingFrames, codecSpecificInfo, renderTimeMs);

  return true;
}

bool
GMPVideoDecoderChild::RecvReset()
{
  if (!mVideoDecoder) {
    return false;
  }

  mVideoDecoder->Reset();

  return true;
}

bool
GMPVideoDecoderChild::RecvDrain()
{
  if (!mVideoDecoder) {
    return false;
  }

  mVideoDecoder->Drain();

  return true;
}

bool
GMPVideoDecoderChild::RecvDecodingComplete()
{
  if (mVideoDecoder) {
    mVideoDecoder->DecodingComplete();
    mVideoDecoder = nullptr;
  }

  mVideoHost.InvalidateShmem();

  unused << Send__delete__(this);

  return true;
}

} // namespace gmp
} // namespace mozilla
