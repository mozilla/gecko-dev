/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GPU_RenderBundleEncoder_H_
#define GPU_RenderBundleEncoder_H_

#include "mozilla/dom/TypedArray.h"
#include "CanvasContext.h"
#include "ObjectModel.h"

namespace mozilla::webgpu {
namespace ffi {
struct WGPURenderBundleEncoder;
}  // namespace ffi

class Device;
class RenderBundle;

struct ffiWGPURenderBundleEncoderDeleter {
  void operator()(ffi::WGPURenderBundleEncoder*);
};

class RenderBundleEncoder final : public ObjectBase, public ChildOf<Device> {
 public:
  GPU_DECL_CYCLE_COLLECTION(RenderBundleEncoder)
  GPU_DECL_JS_WRAP(RenderBundleEncoder)

  RenderBundleEncoder(Device* const aParent, WebGPUChild* const aBridge,
                      const dom::GPURenderBundleEncoderDescriptor& aDesc);

 private:
  ~RenderBundleEncoder();
  void Cleanup();

  std::unique_ptr<ffi::WGPURenderBundleEncoder,
                  ffiWGPURenderBundleEncoderDeleter>
      mEncoder;
  // keep all the used objects alive while the encoder is finished
  nsTArray<RefPtr<const BindGroup>> mUsedBindGroups;
  nsTArray<RefPtr<const Buffer>> mUsedBuffers;
  nsTArray<RefPtr<const RenderPipeline>> mUsedPipelines;

  // The canvas contexts of any canvas textures used in bind groups of this
  // render bundle.
  CanvasContextArray mUsedCanvasContexts;

  // programmable pass encoder
 private:
  void SetBindGroup(uint32_t aSlot, BindGroup* const aBindGroup,
                    const uint32_t* aDynamicOffsets,
                    uint64_t aDynamicOffsetsLength);

 public:
  void SetBindGroup(uint32_t aSlot, BindGroup* const aBindGroup,
                    const dom::Sequence<uint32_t>& aDynamicOffsets,
                    ErrorResult& aRv);
  void SetBindGroup(uint32_t aSlot, BindGroup* const aBindGroup,
                    const dom::Uint32Array& aDynamicOffsetsData,
                    uint64_t aDynamicOffsetsDataStart,
                    uint64_t aDynamicOffsetsDataLength, ErrorResult& aRv);
  // render encoder base
  void SetPipeline(const RenderPipeline& aPipeline);
  void SetIndexBuffer(const Buffer& aBuffer,
                      const dom::GPUIndexFormat& aIndexFormat, uint64_t aOffset,
                      const dom::Optional<uint64_t>& aSize);
  void SetVertexBuffer(uint32_t aSlot, const Buffer& aBuffer, uint64_t aOffset,
                       const dom::Optional<uint64_t>& aSize);
  void Draw(uint32_t aVertexCount, uint32_t aInstanceCount,
            uint32_t aFirstVertex, uint32_t aFirstInstance);
  void DrawIndexed(uint32_t aIndexCount, uint32_t aInstanceCount,
                   uint32_t aFirstIndex, int32_t aBaseVertex,
                   uint32_t aFirstInstance);
  void DrawIndirect(const Buffer& aIndirectBuffer, uint64_t aIndirectOffset);
  void DrawIndexedIndirect(const Buffer& aIndirectBuffer,
                           uint64_t aIndirectOffset);

  void PushDebugGroup(const nsAString& aString);
  void PopDebugGroup();
  void InsertDebugMarker(const nsAString& aString);

  // self
  already_AddRefed<RenderBundle> Finish(
      const dom::GPURenderBundleDescriptor& aDesc);

  // helpers not defined by WebGPU
  mozilla::Span<const WeakPtr<CanvasContext>> GetCanvasContexts() const {
    return mUsedCanvasContexts;
  }
};

}  // namespace mozilla::webgpu

#endif  // GPU_RenderBundleEncoder_H_
