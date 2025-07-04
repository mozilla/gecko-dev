/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/WebGPUBinding.h"
#include "CommandEncoder.h"
#include "ComputePassEncoder.h"
#include "BindGroup.h"
#include "ComputePipeline.h"
#include "CommandEncoder.h"
#include "Utility.h"
#include "mozilla/webgpu/ffi/wgpu.h"
#include "ipc/WebGPUChild.h"

namespace mozilla::webgpu {

GPU_IMPL_CYCLE_COLLECTION(ComputePassEncoder, mParent, mUsedBindGroups,
                          mUsedBuffers, mUsedPipelines)
GPU_IMPL_JS_WRAP(ComputePassEncoder)

void ffiWGPUComputePassDeleter::operator()(ffi::WGPURecordedComputePass* raw) {
  if (raw) {
    ffi::wgpu_compute_pass_destroy(raw);
  }
}

ffi::WGPURecordedComputePass* BeginComputePass(
    RawId aEncoderId, const dom::GPUComputePassDescriptor& aDesc) {
  MOZ_RELEASE_ASSERT(aEncoderId);
  ffi::WGPUComputePassDescriptor desc = {};

  webgpu::StringHelper label(aDesc.mLabel);
  desc.label = label.Get();

  ffi::WGPUPassTimestampWrites passTimestampWrites = {};
  if (aDesc.mTimestampWrites.WasPassed()) {
    AssignPassTimestampWrites(aDesc.mTimestampWrites.Value(),
                              passTimestampWrites);
    desc.timestamp_writes = &passTimestampWrites;
  }

  return ffi::wgpu_command_encoder_begin_compute_pass(&desc);
}

ComputePassEncoder::ComputePassEncoder(
    CommandEncoder* const aParent, const dom::GPUComputePassDescriptor& aDesc)
    : ChildOf(aParent), mPass(BeginComputePass(aParent->mId, aDesc)) {}

ComputePassEncoder::~ComputePassEncoder() { Cleanup(); }

void ComputePassEncoder::Cleanup() {
  mValid = false;
  mPass.release();
  mUsedBindGroups.Clear();
  mUsedBuffers.Clear();
  mUsedPipelines.Clear();
}

void ComputePassEncoder::SetBindGroup(uint32_t aSlot,
                                      BindGroup* const aBindGroup,
                                      const uint32_t* aDynamicOffsets,
                                      uint64_t aDynamicOffsetsLength) {
  RawId bindGroup = 0;
  if (aBindGroup) {
    mUsedBindGroups.AppendElement(aBindGroup);
    mUsedCanvasContexts.AppendElements(aBindGroup->GetCanvasContexts());
    bindGroup = aBindGroup->mId;
  }
  ffi::wgpu_recorded_compute_pass_set_bind_group(
      mPass.get(), aSlot, bindGroup, aDynamicOffsets, aDynamicOffsetsLength);
}

void ComputePassEncoder::SetBindGroup(
    uint32_t aSlot, BindGroup* const aBindGroup,
    const dom::Sequence<uint32_t>& aDynamicOffsets, ErrorResult& aRv) {
  if (!mValid) {
    return;
  }
  this->SetBindGroup(aSlot, aBindGroup, aDynamicOffsets.Elements(),
                     aDynamicOffsets.Length());
}

void ComputePassEncoder::SetBindGroup(
    uint32_t aSlot, BindGroup* const aBindGroup,
    const dom::Uint32Array& aDynamicOffsetsData,
    uint64_t aDynamicOffsetsDataStart, uint64_t aDynamicOffsetsDataLength,
    ErrorResult& aRv) {
  if (!mValid) {
    return;
  }

  auto dynamicOffsets =
      GetDynamicOffsetsFromArray(aDynamicOffsetsData, aDynamicOffsetsDataStart,
                                 aDynamicOffsetsDataLength, aRv);

  if (dynamicOffsets.isSome()) {
    this->SetBindGroup(aSlot, aBindGroup, dynamicOffsets->Elements(),
                       dynamicOffsets->Length());
  }
}

void ComputePassEncoder::SetPipeline(const ComputePipeline& aPipeline) {
  if (!mValid) {
    return;
  }
  mUsedPipelines.AppendElement(&aPipeline);
  ffi::wgpu_recorded_compute_pass_set_pipeline(mPass.get(), aPipeline.mId);
}

void ComputePassEncoder::DispatchWorkgroups(uint32_t workgroupCountX,
                                            uint32_t workgroupCountY,
                                            uint32_t workgroupCountZ) {
  if (!mValid) {
    return;
  }
  ffi::wgpu_recorded_compute_pass_dispatch_workgroups(
      mPass.get(), workgroupCountX, workgroupCountY, workgroupCountZ);
}

void ComputePassEncoder::DispatchWorkgroupsIndirect(
    const Buffer& aIndirectBuffer, uint64_t aIndirectOffset) {
  if (!mValid) {
    return;
  }
  mUsedBuffers.AppendElement(&aIndirectBuffer);
  ffi::wgpu_recorded_compute_pass_dispatch_workgroups_indirect(
      mPass.get(), aIndirectBuffer.mId, aIndirectOffset);
}

void ComputePassEncoder::PushDebugGroup(const nsAString& aString) {
  if (!mValid) {
    return;
  }
  const NS_ConvertUTF16toUTF8 utf8(aString);
  ffi::wgpu_recorded_compute_pass_push_debug_group(mPass.get(), utf8.get(), 0);
}
void ComputePassEncoder::PopDebugGroup() {
  if (!mValid) {
    return;
  }
  ffi::wgpu_recorded_compute_pass_pop_debug_group(mPass.get());
}
void ComputePassEncoder::InsertDebugMarker(const nsAString& aString) {
  if (!mValid) {
    return;
  }
  const NS_ConvertUTF16toUTF8 utf8(aString);
  ffi::wgpu_recorded_compute_pass_insert_debug_marker(mPass.get(), utf8.get(),
                                                      0);
}

void ComputePassEncoder::End() {
  if (mParent->GetState() != CommandEncoderState::Locked &&
      mParent->GetBridge()->CanSend()) {
    mParent->GetBridge()->SendReportError(mParent->GetDevice()->mId,
                                          dom::GPUErrorFilter::Validation,
                                          "Encoding must not have ended"_ns);
  }
  if (!mValid) {
    return;
  }
  MOZ_ASSERT(!!mPass);
  mParent->EndComputePass(*mPass, mUsedCanvasContexts);
  Cleanup();
}

}  // namespace mozilla::webgpu
