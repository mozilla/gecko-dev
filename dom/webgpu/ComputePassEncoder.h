/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GPU_ComputePassEncoder_H_
#define GPU_ComputePassEncoder_H_

#include "mozilla/dom/TypedArray.h"
#include "ObjectModel.h"

namespace mozilla {
class ErrorResult;

namespace dom {
struct GPUComputePassDescriptor;
}

namespace webgpu {
namespace ffi {
struct WGPURecordedComputePass;
}  // namespace ffi

class BindGroup;
class Buffer;
class CommandEncoder;
class ComputePipeline;

struct ffiWGPUComputePassDeleter {
  void operator()(ffi::WGPURecordedComputePass*);
};

class ComputePassEncoder final : public ObjectBase,
                                 public ChildOf<CommandEncoder> {
 public:
  GPU_DECL_CYCLE_COLLECTION(ComputePassEncoder)
  GPU_DECL_JS_WRAP(ComputePassEncoder)

  ComputePassEncoder(CommandEncoder* const aParent,
                     const dom::GPUComputePassDescriptor& aDesc);

 private:
  virtual ~ComputePassEncoder();
  void Cleanup();

  std::unique_ptr<ffi::WGPURecordedComputePass, ffiWGPUComputePassDeleter>
      mPass;
  // keep all the used objects alive while the pass is recorded
  nsTArray<RefPtr<const BindGroup>> mUsedBindGroups;
  nsTArray<RefPtr<const Buffer>> mUsedBuffers;
  nsTArray<RefPtr<const ComputePipeline>> mUsedPipelines;

  // The canvas contexts of any canvas textures used in bind groups of this
  // compute pass.
  CanvasContextArray mUsedCanvasContexts;

  // programmable pass encoder
 private:
  void SetBindGroup(uint32_t aSlot, BindGroup* const aBindGroup,
                    const uint32_t* aDynamicOffsets,
                    uint64_t aDynamicOffsetsLength);

 public:
  void Invalidate() { mValid = false; }

  void SetBindGroup(uint32_t aSlot, BindGroup* const aBindGroup,
                    const dom::Sequence<uint32_t>& aDynamicOffsets,
                    ErrorResult& aRv);
  void SetBindGroup(uint32_t aSlot, BindGroup* const aBindGroup,
                    const dom::Uint32Array& aDynamicOffsetsData,
                    uint64_t aDynamicOffsetsDataStart,
                    uint64_t aDynamicOffsetsDataLength, ErrorResult& aRv);
  // self
  void SetPipeline(const ComputePipeline& aPipeline);

  void DispatchWorkgroups(uint32_t workgroupCountX, uint32_t workgroupCountY,
                          uint32_t workgroupCountZ);
  void DispatchWorkgroupsIndirect(const Buffer& aIndirectBuffer,
                                  uint64_t aIndirectOffset);

  void PushDebugGroup(const nsAString& aString);
  void PopDebugGroup();
  void InsertDebugMarker(const nsAString& aString);

  void End();

  // helpers not defined by WebGPU
  mozilla::Span<const WeakPtr<CanvasContext>> GetCanvasContexts() const {
    return mUsedCanvasContexts;
  }
};

}  // namespace webgpu
}  // namespace mozilla

#endif  // GPU_ComputePassEncoder_H_
