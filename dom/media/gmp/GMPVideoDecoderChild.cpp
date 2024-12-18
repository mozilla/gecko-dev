/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GMPVideoDecoderChild.h"
#include "GMPVideoi420FrameImpl.h"
#include "GMPContentChild.h"
#include <stdio.h>
#include "mozilla/Unused.h"
#include "GMPVideoEncodedFrameImpl.h"
#include "runnable_utils.h"

namespace mozilla::gmp {

GMPVideoDecoderChild::GMPVideoDecoderChild(GMPContentChild* aPlugin)
    : mPlugin(aPlugin), mVideoDecoder(nullptr), mVideoHost(this) {
  MOZ_ASSERT(mPlugin);
}

GMPVideoDecoderChild::~GMPVideoDecoderChild() = default;

bool GMPVideoDecoderChild::MgrIsOnOwningThread() const {
  return !mPlugin || mPlugin->GMPMessageLoop() == MessageLoop::current();
}

void GMPVideoDecoderChild::Init(GMPVideoDecoder* aDecoder) {
  MOZ_ASSERT(aDecoder,
             "Cannot initialize video decoder child without a video decoder!");
  mVideoDecoder = aDecoder;
}

GMPVideoHostImpl& GMPVideoDecoderChild::Host() { return mVideoHost; }

void GMPVideoDecoderChild::Decoded(GMPVideoi420Frame* aDecodedFrame) {
  if (!aDecodedFrame) {
    MOZ_CRASH("Not given a decoded frame!");
  }

  if (NS_WARN_IF(!mPlugin)) {
    aDecodedFrame->Destroy();
    return;
  }

  MOZ_ASSERT(mPlugin->GMPMessageLoop() == MessageLoop::current());

  auto df = static_cast<GMPVideoi420FrameImpl*>(aDecodedFrame);

  if (GMPSharedMemManager* memMgr = mVideoHost.SharedMemMgr()) {
    ipc::Shmem inputShmem;
    if (memMgr->MgrTakeShmem(GMPSharedMemClass::Encoded, &inputShmem)) {
      Unused << SendReturnShmem(std::move(inputShmem));
    }
  }

  GMPVideoi420FrameData frameData;
  ipc::Shmem frameShmem;
  nsTArray<uint8_t> frameArray;

  if (df->InitFrameData(frameData, frameShmem)) {
    Unused << SendDecodedShmem(frameData, std::move(frameShmem));
  } else if (df->InitFrameData(frameData, frameArray)) {
    Unused << SendDecodedData(frameData, std::move(frameArray));
  } else {
    MOZ_CRASH("Decoded without any frame data!");
  }

  aDecodedFrame->Destroy();
}

void GMPVideoDecoderChild::ReceivedDecodedReferenceFrame(
    const uint64_t aPictureId) {
  if (NS_WARN_IF(!mPlugin)) {
    return;
  }

  MOZ_ASSERT(mPlugin->GMPMessageLoop() == MessageLoop::current());

  SendReceivedDecodedReferenceFrame(aPictureId);
}

void GMPVideoDecoderChild::ReceivedDecodedFrame(const uint64_t aPictureId) {
  if (NS_WARN_IF(!mPlugin)) {
    return;
  }

  MOZ_ASSERT(mPlugin->GMPMessageLoop() == MessageLoop::current());

  SendReceivedDecodedFrame(aPictureId);
}

void GMPVideoDecoderChild::InputDataExhausted() {
  if (NS_WARN_IF(!mPlugin)) {
    return;
  }

  MOZ_ASSERT(mPlugin->GMPMessageLoop() == MessageLoop::current());

  SendInputDataExhausted();
}

void GMPVideoDecoderChild::DrainComplete() {
  if (NS_WARN_IF(!mPlugin)) {
    return;
  }

  MOZ_ASSERT(mPlugin->GMPMessageLoop() == MessageLoop::current());

  SendDrainComplete();
}

void GMPVideoDecoderChild::ResetComplete() {
  if (NS_WARN_IF(!mPlugin)) {
    return;
  }

  MOZ_ASSERT(mPlugin->GMPMessageLoop() == MessageLoop::current());

  SendResetComplete();
}

void GMPVideoDecoderChild::Error(GMPErr aError) {
  if (NS_WARN_IF(!mPlugin)) {
    return;
  }

  MOZ_ASSERT(mPlugin->GMPMessageLoop() == MessageLoop::current());

  SendError(aError);
}

mozilla::ipc::IPCResult GMPVideoDecoderChild::RecvInitDecode(
    const GMPVideoCodec& aCodecSettings, nsTArray<uint8_t>&& aCodecSpecific,
    const int32_t& aCoreCount) {
  if (!mVideoDecoder) {
    return IPC_FAIL(this, "!mVideoDecoder");
  }

  // Ignore any return code. It is OK for this to fail without killing the
  // process.
  mVideoDecoder->InitDecode(aCodecSettings, aCodecSpecific.Elements(),
                            aCodecSpecific.Length(), this, aCoreCount);
  return IPC_OK();
}

mozilla::ipc::IPCResult GMPVideoDecoderChild::RecvGiveShmem(
    ipc::Shmem&& aOutputShmem) {
  if (GMPSharedMemManager* memMgr = mVideoHost.SharedMemMgr()) {
    memMgr->MgrGiveShmem(GMPSharedMemClass::Decoded, std::move(aOutputShmem));
  } else {
    DeallocShmem(aOutputShmem);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult GMPVideoDecoderChild::RecvDecode(
    const GMPVideoEncodedFrameData& aInputFrame, ipc::Shmem&& aInputShmem,
    const bool& aMissingFrames, nsTArray<uint8_t>&& aCodecSpecificInfo,
    const int64_t& aRenderTimeMs) {
  if (!mVideoDecoder) {
    DeallocShmem(aInputShmem);
    return IPC_FAIL(this, "!mVideoDecoder");
  }

  auto* f = new GMPVideoEncodedFrameImpl(aInputFrame, std::move(aInputShmem),
                                         &mVideoHost);

  // Ignore any return code. It is OK for this to fail without killing the
  // process.
  mVideoDecoder->Decode(f, aMissingFrames, aCodecSpecificInfo.Elements(),
                        aCodecSpecificInfo.Length(), aRenderTimeMs);

  return IPC_OK();
}

mozilla::ipc::IPCResult GMPVideoDecoderChild::RecvReset() {
  if (!mVideoDecoder) {
    return IPC_FAIL(this, "!mVideoDecoder");
  }

  // Ignore any return code. It is OK for this to fail without killing the
  // process.
  mVideoDecoder->Reset();

  return IPC_OK();
}

mozilla::ipc::IPCResult GMPVideoDecoderChild::RecvDrain() {
  if (!mVideoDecoder) {
    return IPC_FAIL(this, "!mVideoDecoder");
  }

  // Ignore any return code. It is OK for this to fail without killing the
  // process.
  mVideoDecoder->Drain();

  return IPC_OK();
}

void GMPVideoDecoderChild::ActorDestroy(ActorDestroyReason why) {
  if (mVideoDecoder) {
    // Ignore any return code. It is OK for this to fail without killing the
    // process.
    mVideoDecoder->DecodingComplete();
    mVideoDecoder = nullptr;
  }

  mVideoHost.DoneWithAPI();

  mPlugin = nullptr;
}

}  // namespace mozilla::gmp
