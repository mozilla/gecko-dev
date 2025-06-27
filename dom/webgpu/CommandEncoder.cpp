/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/UnionTypes.h"
#include "mozilla/dom/WebGPUBinding.h"
#include "CommandEncoder.h"

#include "CommandBuffer.h"
#include "Buffer.h"
#include "ComputePassEncoder.h"
#include "Device.h"
#include "RenderPassEncoder.h"
#include "Utility.h"
#include "mozilla/webgpu/CanvasContext.h"
#include "mozilla/webgpu/ffi/wgpu.h"
#include "ipc/WebGPUChild.h"

namespace mozilla::webgpu {

GPU_IMPL_CYCLE_COLLECTION(CommandEncoder, mParent, mBridge)
GPU_IMPL_JS_WRAP(CommandEncoder)

void CommandEncoder::ConvertTextureDataLayoutToFFI(
    const dom::GPUTexelCopyBufferLayout& aLayout,
    ffi::WGPUTexelCopyBufferLayout* aLayoutFFI) {
  *aLayoutFFI = {};
  aLayoutFFI->offset = aLayout.mOffset;

  if (aLayout.mBytesPerRow.WasPassed()) {
    aLayoutFFI->bytes_per_row = &aLayout.mBytesPerRow.Value();
  } else {
    aLayoutFFI->bytes_per_row = nullptr;
  }

  if (aLayout.mRowsPerImage.WasPassed()) {
    aLayoutFFI->rows_per_image = &aLayout.mRowsPerImage.Value();
  } else {
    aLayoutFFI->rows_per_image = nullptr;
  }
}

void CommandEncoder::ConvertTextureCopyViewToFFI(
    const dom::GPUTexelCopyTextureInfo& aCopy,
    ffi::WGPUTexelCopyTextureInfo* aViewFFI) {
  *aViewFFI = {};
  aViewFFI->texture = aCopy.mTexture->mId;
  aViewFFI->mip_level = aCopy.mMipLevel;
  const auto& origin = aCopy.mOrigin;
  if (origin.IsRangeEnforcedUnsignedLongSequence()) {
    const auto& seq = origin.GetAsRangeEnforcedUnsignedLongSequence();
    aViewFFI->origin.x = seq.Length() > 0 ? seq[0] : 0;
    aViewFFI->origin.y = seq.Length() > 1 ? seq[1] : 0;
    aViewFFI->origin.z = seq.Length() > 2 ? seq[2] : 0;
  } else if (origin.IsGPUOrigin3DDict()) {
    const auto& dict = origin.GetAsGPUOrigin3DDict();
    aViewFFI->origin.x = dict.mX;
    aViewFFI->origin.y = dict.mY;
    aViewFFI->origin.z = dict.mZ;
  } else {
    MOZ_CRASH("Unexpected origin type");
  }
  aViewFFI->aspect = ConvertTextureAspect(aCopy.mAspect);
}

static ffi::WGPUTexelCopyTextureInfo ConvertTextureCopyView(
    const dom::GPUTexelCopyTextureInfo& aCopy) {
  ffi::WGPUTexelCopyTextureInfo view = {};
  CommandEncoder::ConvertTextureCopyViewToFFI(aCopy, &view);
  return view;
}

CommandEncoder::CommandEncoder(Device* const aParent,
                               WebGPUChild* const aBridge, RawId aId)
    : ChildOf(aParent),
      mId(aId),
      mState(CommandEncoderState::Open),
      mBridge(aBridge) {
  MOZ_RELEASE_ASSERT(aId);
}

CommandEncoder::~CommandEncoder() { Cleanup(); }

void CommandEncoder::Cleanup() {
  if (!mValid) {
    return;
  }
  mValid = false;

  if (!mBridge) {
    return;
  }

  ffi::wgpu_client_drop_command_encoder(mBridge->GetClient(), mId);

  wgpu_client_free_command_encoder_id(mBridge->GetClient(), mId);
}

RefPtr<WebGPUChild> CommandEncoder::GetBridge() { return mBridge; }

void CommandEncoder::TrackPresentationContext(CanvasContext* aTargetContext) {
  if (aTargetContext) {
    mPresentationContexts.AppendElement(aTargetContext);
  }
}

void CommandEncoder::CopyBufferToBuffer(
    const Buffer& aSource, BufferAddress aSourceOffset,
    const Buffer& aDestination, BufferAddress aDestinationOffset,
    const dom::Optional<BufferAddress>& aSize) {
  // In Javascript, `size === undefined` means "copy from source offset to end
  // of buffer". wgpu_command_encoder_copy_buffer_to_buffer uses a value of
  // UINT64_MAX to encode this. If the requested copy size was UINT64_MAX, fudge
  // it to a different value that will still be rejected for misalignment on the
  // device timeline.
  BufferAddress size;
  if (aSize.WasPassed()) {
    if (aSize.Value() == std::numeric_limits<uint64_t>::max()) {
      size = std::numeric_limits<uint64_t>::max() - 4;
    } else {
      size = aSize.Value();
    }
  } else {
    size = std::numeric_limits<uint64_t>::max();
  }

  ffi::wgpu_command_encoder_copy_buffer_to_buffer(
      mBridge->GetClient(), mParent->mId, mId, aSource.mId, aSourceOffset,
      aDestination.mId, aDestinationOffset, size);
}

void CommandEncoder::CopyBufferToTexture(
    const dom::GPUTexelCopyBufferInfo& aSource,
    const dom::GPUTexelCopyTextureInfo& aDestination,
    const dom::GPUExtent3D& aCopySize) {
  ffi::WGPUTexelCopyBufferLayout src_layout = {};
  CommandEncoder::ConvertTextureDataLayoutToFFI(aSource, &src_layout);
  ffi::wgpu_command_encoder_copy_buffer_to_texture(
      mBridge->GetClient(), mParent->mId, mId, aSource.mBuffer->mId,
      &src_layout, ConvertTextureCopyView(aDestination),
      ConvertExtent(aCopySize));

  TrackPresentationContext(aDestination.mTexture->mTargetContext);
}
void CommandEncoder::CopyTextureToBuffer(
    const dom::GPUTexelCopyTextureInfo& aSource,
    const dom::GPUTexelCopyBufferInfo& aDestination,
    const dom::GPUExtent3D& aCopySize) {
  ffi::WGPUTexelCopyBufferLayout dstLayout = {};
  CommandEncoder::ConvertTextureDataLayoutToFFI(aDestination, &dstLayout);
  ffi::wgpu_command_encoder_copy_texture_to_buffer(
      mBridge->GetClient(), mParent->mId, mId, ConvertTextureCopyView(aSource),
      aDestination.mBuffer->mId, &dstLayout, ConvertExtent(aCopySize));
}
void CommandEncoder::CopyTextureToTexture(
    const dom::GPUTexelCopyTextureInfo& aSource,
    const dom::GPUTexelCopyTextureInfo& aDestination,
    const dom::GPUExtent3D& aCopySize) {
  ffi::wgpu_command_encoder_copy_texture_to_texture(
      mBridge->GetClient(), mParent->mId, mId, ConvertTextureCopyView(aSource),
      ConvertTextureCopyView(aDestination), ConvertExtent(aCopySize));

  TrackPresentationContext(aDestination.mTexture->mTargetContext);
}

void CommandEncoder::ClearBuffer(const Buffer& aBuffer, const uint64_t aOffset,
                                 const dom::Optional<uint64_t>& aSize) {
  uint64_t sizeVal = 0xdeaddead;
  uint64_t* size = nullptr;
  if (aSize.WasPassed()) {
    sizeVal = aSize.Value();
    size = &sizeVal;
  }

  ffi::wgpu_command_encoder_clear_buffer(mBridge->GetClient(), mParent->mId,
                                         mId, aBuffer.mId, aOffset, size);
}

void CommandEncoder::PushDebugGroup(const nsAString& aString) {
  NS_ConvertUTF16toUTF8 marker(aString);
  ffi::wgpu_command_encoder_push_debug_group(mBridge->GetClient(), mParent->mId,
                                             mId, &marker);
}
void CommandEncoder::PopDebugGroup() {
  ffi::wgpu_command_encoder_pop_debug_group(mBridge->GetClient(), mParent->mId,
                                            mId);
}
void CommandEncoder::InsertDebugMarker(const nsAString& aString) {
  NS_ConvertUTF16toUTF8 marker(aString);
  ffi::wgpu_command_encoder_insert_debug_marker(mBridge->GetClient(),
                                                mParent->mId, mId, &marker);
}

already_AddRefed<ComputePassEncoder> CommandEncoder::BeginComputePass(
    const dom::GPUComputePassDescriptor& aDesc) {
  RefPtr<ComputePassEncoder> pass = new ComputePassEncoder(this, aDesc);
  pass->SetLabel(aDesc.mLabel);
  if (mState == CommandEncoderState::Ended) {
    // Because we do not call wgpu until the pass is ended, we need to generate
    // this error ourselves in order to report it at the correct time.

    const auto* message = "Encoding must not have ended";
    ffi::wgpu_report_validation_error(mBridge->GetClient(), mParent->mId,
                                      message);

    pass->Invalidate();
  } else if (mState == CommandEncoderState::Locked) {
    // This is not sufficient to handle this case properly. Invalidity
    // needs to be transferred from the pass to the encoder when the pass
    // ends. Bug 1971650.
    pass->Invalidate();
  } else {
    mState = CommandEncoderState::Locked;
  }
  return pass.forget();
}

already_AddRefed<RenderPassEncoder> CommandEncoder::BeginRenderPass(
    const dom::GPURenderPassDescriptor& aDesc) {
  for (const auto& at : aDesc.mColorAttachments) {
    TrackPresentationContext(at.mView->GetTargetContext());
    if (at.mResolveTarget.WasPassed()) {
      TrackPresentationContext(at.mResolveTarget.Value().GetTargetContext());
    }
  }

  RefPtr<RenderPassEncoder> pass = new RenderPassEncoder(this, aDesc);
  pass->SetLabel(aDesc.mLabel);
  if (mState == CommandEncoderState::Ended) {
    // Because we do not call wgpu until the pass is ended, we need to generate
    // this error ourselves in order to report it at the correct time.

    const auto* message = "Encoding must not have ended";
    ffi::wgpu_report_validation_error(mBridge->GetClient(), mParent->mId,
                                      message);

    pass->Invalidate();
  } else if (mState == CommandEncoderState::Locked) {
    // This is not sufficient to handle this case properly. Invalidity
    // needs to be transferred from the pass to the encoder when the pass
    // ends. Bug 1971650.
    pass->Invalidate();
  } else {
    mState = CommandEncoderState::Locked;
  }
  return pass.forget();
}

void CommandEncoder::ResolveQuerySet(QuerySet& aQuerySet, uint32_t aFirstQuery,
                                     uint32_t aQueryCount,
                                     webgpu::Buffer& aDestination,
                                     uint64_t aDestinationOffset) {
  ffi::wgpu_command_encoder_resolve_query_set(
      mBridge->GetClient(), mParent->mId, mId, aQuerySet.mId, aFirstQuery,
      aQueryCount, aDestination.mId, aDestinationOffset);
}

void CommandEncoder::EndComputePass(ffi::WGPURecordedComputePass& aPass) {
  // Because this can be called during child Cleanup, we need to check
  // that the bridge is still alive.
  if (!mBridge) {
    return;
  }

  if (mState != CommandEncoderState::Locked) {
    const auto* message = "Encoder is not currently locked";
    ffi::wgpu_report_validation_error(mBridge->GetClient(), mParent->mId,
                                      message);
    return;
  }
  mState = CommandEncoderState::Open;

  ffi::wgpu_compute_pass_finish(mBridge->GetClient(), mParent->mId, mId,
                                &aPass);
}

void CommandEncoder::EndRenderPass(ffi::WGPURecordedRenderPass& aPass) {
  // Because this can be called during child Cleanup, we need to check
  // that the bridge is still alive.
  if (!mBridge) {
    return;
  }

  if (mState != CommandEncoderState::Locked) {
    const auto* message = "Encoder is not currently locked";
    ffi::wgpu_report_validation_error(mBridge->GetClient(), mParent->mId,
                                      message);
    return;
  }
  mState = CommandEncoderState::Open;

  ffi::wgpu_render_pass_finish(mBridge->GetClient(), mParent->mId, mId, &aPass);
}

already_AddRefed<CommandBuffer> CommandEncoder::Finish(
    const dom::GPUCommandBufferDescriptor& aDesc) {
  ffi::WGPUCommandBufferDescriptor desc = {};

  webgpu::StringHelper label(aDesc.mLabel);
  desc.label = label.Get();

  // We rely on knowledge that `CommandEncoderId` == `CommandBufferId`
  // TODO: refactor this to truly behave as if the encoder is being finished,
  // and a new command buffer ID is being created from it. Resolve the ID
  // type aliasing at the place that introduces it: `wgpu-core`.
  if (mState == CommandEncoderState::Locked) {
    // Most errors that could occur here will be raised by wgpu. But since we
    // don't tell wgpu about passes until they are ended, we need to raise an
    // error if the application left a pass open.
    const auto* message =
        "Encoder is locked by a previously created render/compute pass";
    ffi::wgpu_report_validation_error(mBridge->GetClient(), mParent->mId,
                                      message);
  }
  ffi::wgpu_command_encoder_finish(mBridge->GetClient(), mParent->mId, mId,
                                   &desc);

  mState = CommandEncoderState::Ended;

  RefPtr<CommandEncoder> me(this);
  RefPtr<CommandBuffer> comb = new CommandBuffer(
      mParent, mId, std::move(mPresentationContexts), std::move(me));
  comb->SetLabel(aDesc.mLabel);
  return comb.forget();
}

}  // namespace mozilla::webgpu
