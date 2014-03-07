/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GMPVideoEncoderChild.h"
#include <stdio.h>
#include "mozilla/unused.h"

namespace mozilla {
namespace gmp {

GMPVideoEncoderChild::GMPVideoEncoderChild()
: mVideoEncoder(nullptr),
  mVideoHost(this)
{
}

GMPVideoEncoderChild::~GMPVideoEncoderChild()
{
}

void
GMPVideoEncoderChild::Init(GMPVideoEncoder* aEncoder)
{
  MOZ_ASSERT(aEncoder, "Cannot initialize video encoder child without a video encoder!");
  mVideoEncoder = aEncoder;
}

GMPVideoHostImpl&
GMPVideoEncoderChild::Host()
{
  return mVideoHost;
}

void
GMPVideoEncoderChild::Encoded(GMPVideoEncodedFrame& aEncodedFrame,
                              const GMPCodecSpecificInfo& aCodecSpecificInfo)
{
  auto& ef = static_cast<GMPVideoEncodedFrameImpl&>(aEncodedFrame);
  ipc::Shmem* sm = nullptr;
  ef.ExtractShmem(&sm);
  SendEncoded(ef, *sm, aCodecSpecificInfo);
}

bool
GMPVideoEncoderChild::MgrAllocShmem(size_t aSize,
                                    ipc::Shmem::SharedMemory::SharedMemoryType aType,
                                    ipc::Shmem* aMem)
{
  return AllocShmem(aSize, aType, aMem);
}

bool
GMPVideoEncoderChild::MgrDeallocShmem(Shmem& aMem)
{
  return DeallocShmem(aMem);
}

bool
GMPVideoEncoderChild::RecvInitEncode(const GMPVideoCodec& aCodecSettings,
                                     const int32_t& aNumberOfCores,
                                     const uint32_t& aMaxPayloadSize)
{
  if (!mVideoEncoder) {
    return false;
  }

  mVideoEncoder->InitEncode(aCodecSettings, this, aNumberOfCores, aMaxPayloadSize);

  return true;
}

bool
GMPVideoEncoderChild::RecvEncode(const GMPVideoi420FrameImpl& aInputFrame,
                                 Shmem& aYShmem, Shmem& aUShmem, Shmem& aVShmem,
                                 const GMPCodecSpecificInfo& aCodecSpecificInfo,
                                 const InfallibleTArray<int>& aFrameTypes)
{
  if (!mVideoEncoder) {
    return false;
  }

  // We need a mutable copy of the decoded frame, into which we can inject
  // the shared memory backing.
  auto frame = new GMPVideoi420FrameImpl();

  frame->SetHost(&mVideoHost);

  GMPVideoErr err = frame->CopyFrame(aInputFrame);
  if (err != GMPVideoNoErr) {
    return false;
  }

  frame->ReceiveShmem(aYShmem, aUShmem, aVShmem);

  //XXXJOSH convert aFrameTypes to std:: array and pass it through
  mVideoEncoder->Encode(*frame, aCodecSpecificInfo, nullptr);

  return true;
}

bool
GMPVideoEncoderChild::RecvSetChannelParameters(const uint32_t& aPacketLoss,
                                               const uint32_t& aRTT)
{
  if (!mVideoEncoder) {
    return false;
  }

  mVideoEncoder->SetChannelParameters(aPacketLoss, aRTT);

  return true;
}

bool
GMPVideoEncoderChild::RecvSetRates(const uint32_t& aNewBitRate,
                                   const uint32_t& aFrameRate)
{
  if (!mVideoEncoder) {
    return false;
  }

  mVideoEncoder->SetRates(aNewBitRate, aFrameRate);

  return true;
}

bool
GMPVideoEncoderChild::RecvSetPeriodicKeyFrames(const bool& aEnable)
{
  if (!mVideoEncoder) {
    return false;
  }

  mVideoEncoder->SetPeriodicKeyFrames(aEnable);

  return true;
}

bool
GMPVideoEncoderChild::RecvEncodingComplete()
{
  if (!mVideoEncoder) {
    return false;
  }

  mVideoEncoder->EncodingComplete();

  unused << Send__delete__(this);

  return true;
}

} // namespace gmp
} // namespace mozilla
