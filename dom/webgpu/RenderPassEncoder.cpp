/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/WebGPUBinding.h"
#include "RenderPassEncoder.h"
#include "BindGroup.h"
#include "CommandEncoder.h"
#include "RenderBundle.h"
#include "RenderPipeline.h"
#include "Utility.h"
#include "mozilla/webgpu/ffi/wgpu.h"
#include "ipc/WebGPUChild.h"

namespace mozilla::webgpu {

GPU_IMPL_CYCLE_COLLECTION(RenderPassEncoder, mParent, mUsedBindGroups,
                          mUsedBuffers, mUsedPipelines, mUsedTextureViews,
                          mUsedRenderBundles)
GPU_IMPL_JS_WRAP(RenderPassEncoder)

void ffiWGPURenderPassDeleter::operator()(ffi::WGPURecordedRenderPass* raw) {
  if (raw) {
    ffi::wgpu_render_pass_destroy(raw);
  }
}

static ffi::WGPUStoreOp ConvertStoreOp(const dom::GPUStoreOp& aOp) {
  switch (aOp) {
    case dom::GPUStoreOp::Store:
      return ffi::WGPUStoreOp_Store;
    case dom::GPUStoreOp::Discard:
      return ffi::WGPUStoreOp_Discard;
  }
  MOZ_CRASH("bad GPUStoreOp");
}

static ffi::WGPUColor ConvertColor(const dom::Sequence<double>& aSeq) {
  ffi::WGPUColor color{
      .r = aSeq.SafeElementAt(0, 0.0),
      .g = aSeq.SafeElementAt(1, 0.0),
      .b = aSeq.SafeElementAt(2, 0.0),
      .a = aSeq.SafeElementAt(3, 1.0),
  };
  return color;
}

static ffi::WGPUColor ConvertColor(const dom::GPUColorDict& aColor) {
  ffi::WGPUColor color = {aColor.mR, aColor.mG, aColor.mB, aColor.mA};
  return color;
}

static ffi::WGPUColor ConvertColor(
    const dom::DoubleSequenceOrGPUColorDict& aColor) {
  if (aColor.IsDoubleSequence()) {
    return ConvertColor(aColor.GetAsDoubleSequence());
  }
  if (aColor.IsGPUColorDict()) {
    return ConvertColor(aColor.GetAsGPUColorDict());
  }
  MOZ_ASSERT_UNREACHABLE(
      "Unexpected dom::DoubleSequenceOrGPUColorDict variant");
  return ffi::WGPUColor();
}
static ffi::WGPUColor ConvertColor(
    const dom::OwningDoubleSequenceOrGPUColorDict& aColor) {
  if (aColor.IsDoubleSequence()) {
    return ConvertColor(aColor.GetAsDoubleSequence());
  }
  if (aColor.IsGPUColorDict()) {
    return ConvertColor(aColor.GetAsGPUColorDict());
  }
  MOZ_ASSERT_UNREACHABLE(
      "Unexpected dom::OwningDoubleSequenceOrGPUColorDict variant");
  return ffi::WGPUColor();
}

ffi::WGPURecordedRenderPass* BeginRenderPass(
    CommandEncoder* const aParent, const dom::GPURenderPassDescriptor& aDesc) {
  ffi::WGPURenderPassDescriptor desc = {};

  webgpu::StringHelper label(aDesc.mLabel);
  desc.label = label.Get();

  ffi::WGPURenderPassDepthStencilAttachment dsDesc = {};
  if (aDesc.mDepthStencilAttachment.WasPassed()) {
    const auto& dsa = aDesc.mDepthStencilAttachment.Value();
    dsDesc.view = dsa.mView->mId;

    // -

    if (dsa.mDepthLoadOp.WasPassed()) {
      dsDesc.depth.load_op.tag =
          ffi::WGPUFfiOption_LoadOp_FfiOption_f32_Some_LoadOp_FfiOption_f32;
      switch (dsa.mDepthLoadOp.Value()) {
        case dom::GPULoadOp::Load:
          dsDesc.depth.load_op.some.tag =
              ffi::WGPULoadOp_FfiOption_f32_Load_FfiOption_f32;
          break;
        case dom::GPULoadOp::Clear:
          dsDesc.depth.load_op.some.clear_tag =
              ffi::WGPULoadOp_FfiOption_f32_Clear_FfiOption_f32;
          if (dsa.mDepthClearValue.WasPassed()) {
            dsDesc.depth.load_op.some.clear.tag =
                ffi::WGPUFfiOption_f32_Some_f32;
            dsDesc.depth.load_op.some.clear.some = dsa.mDepthClearValue.Value();
          } else {
            dsDesc.depth.load_op.some.clear.tag =
                ffi::WGPUFfiOption_f32_None_f32;
          }
          break;
      }
    } else {
      dsDesc.depth.load_op.tag =
          ffi::WGPUFfiOption_LoadOp_FfiOption_f32_None_LoadOp_FfiOption_f32;
    }

    if (dsa.mDepthStoreOp.WasPassed()) {
      dsDesc.depth.store_op.tag = ffi::WGPUFfiOption_StoreOp_Some_StoreOp;
      dsDesc.depth.store_op.some = ConvertStoreOp(dsa.mDepthStoreOp.Value());
    } else {
      dsDesc.depth.store_op.tag = ffi::WGPUFfiOption_StoreOp_None_StoreOp;
    }

    dsDesc.depth.read_only = dsa.mDepthReadOnly;

    // -

    if (dsa.mStencilLoadOp.WasPassed()) {
      dsDesc.stencil.load_op.tag =
          ffi::WGPUFfiOption_LoadOp_FfiOption_u32_Some_LoadOp_FfiOption_u32;
      switch (dsa.mStencilLoadOp.Value()) {
        case dom::GPULoadOp::Load:
          dsDesc.stencil.load_op.some.tag =
              ffi::WGPULoadOp_FfiOption_u32_Load_FfiOption_u32;
          break;
        case dom::GPULoadOp::Clear:
          dsDesc.stencil.load_op.some.clear_tag =
              ffi::WGPULoadOp_FfiOption_u32_Clear_FfiOption_u32;
          dsDesc.stencil.load_op.some.clear.tag =
              ffi::WGPUFfiOption_u32_Some_u32;
          dsDesc.stencil.load_op.some.clear.some = dsa.mStencilClearValue;
          break;
      }
    } else {
      dsDesc.stencil.load_op.tag =
          ffi::WGPUFfiOption_LoadOp_FfiOption_u32_None_LoadOp_FfiOption_u32;
    }

    if (dsa.mStencilStoreOp.WasPassed()) {
      dsDesc.stencil.store_op.tag = ffi::WGPUFfiOption_StoreOp_Some_StoreOp;
      dsDesc.stencil.store_op.some =
          ConvertStoreOp(dsa.mStencilStoreOp.Value());
    } else {
      dsDesc.stencil.store_op.tag = ffi::WGPUFfiOption_StoreOp_None_StoreOp;
    }

    dsDesc.stencil.read_only = dsa.mStencilReadOnly;

    // -

    desc.depth_stencil_attachment = &dsDesc;
  }

  AutoTArray<ffi::WGPUFfiRenderPassColorAttachment, WGPUMAX_COLOR_ATTACHMENTS>
      colorDescs;

  for (const auto& ca : aDesc.mColorAttachments) {
    ffi::WGPUFfiRenderPassColorAttachment cd = {};
    cd.view = ca.mView->mId;
    cd.store_op = ConvertStoreOp(ca.mStoreOp);

    if (ca.mDepthSlice.WasPassed()) {
      cd.depth_slice.tag = ffi::WGPUFfiOption_u32_Some_u32;
      cd.depth_slice.some = ca.mDepthSlice.Value();
    } else {
      cd.depth_slice.tag = ffi::WGPUFfiOption_u32_None_u32;
    }
    if (ca.mResolveTarget.WasPassed()) {
      cd.resolve_target = ca.mResolveTarget.Value().mId;
    }

    switch (ca.mLoadOp) {
      case dom::GPULoadOp::Load:
        cd.load_op.tag = ffi::WGPULoadOp_Color_Load_Color;
        break;
      case dom::GPULoadOp::Clear:
        cd.load_op.clear_tag = ffi::WGPULoadOp_Color_Clear_Color;
        if (ca.mClearValue.WasPassed()) {
          cd.load_op.clear = ConvertColor(ca.mClearValue.Value());
        } else {
          cd.load_op.clear = ffi::WGPUColor{0};
        }
        break;
    }
    colorDescs.AppendElement(cd);
  }

  desc.color_attachments = colorDescs.Elements();
  desc.color_attachments_length = colorDescs.Length();

  if (aDesc.mOcclusionQuerySet.WasPassed()) {
    desc.occlusion_query_set = aDesc.mOcclusionQuerySet.Value().mId;
  }

  ffi::WGPUPassTimestampWrites passTimestampWrites = {};
  if (aDesc.mTimestampWrites.WasPassed()) {
    AssignPassTimestampWrites(aDesc.mTimestampWrites.Value(),
                              passTimestampWrites);
    desc.timestamp_writes = &passTimestampWrites;
  }

  return ffi::wgpu_command_encoder_begin_render_pass(&desc);
}

RenderPassEncoder::RenderPassEncoder(CommandEncoder* const aParent,
                                     const dom::GPURenderPassDescriptor& aDesc)
    : ChildOf(aParent), mPass(BeginRenderPass(aParent, aDesc)) {
  mValid = !!mPass;
  if (!mValid) {
    return;
  }

  for (const auto& at : aDesc.mColorAttachments) {
    mUsedTextureViews.AppendElement(at.mView);
  }
  if (aDesc.mDepthStencilAttachment.WasPassed()) {
    mUsedTextureViews.AppendElement(
        aDesc.mDepthStencilAttachment.Value().mView);
  }
}

RenderPassEncoder::~RenderPassEncoder() { Cleanup(); }

void RenderPassEncoder::Cleanup() {
  mValid = false;
  mPass.release();
  mUsedBindGroups.Clear();
  mUsedBuffers.Clear();
  mUsedPipelines.Clear();
  mUsedTextureViews.Clear();
  mUsedRenderBundles.Clear();
}

void RenderPassEncoder::SetBindGroup(uint32_t aSlot,
                                     BindGroup* const aBindGroup,
                                     const uint32_t* aDynamicOffsets,
                                     uint64_t aDynamicOffsetsLength) {
  RawId bindGroup = 0;
  if (aBindGroup) {
    mUsedBindGroups.AppendElement(aBindGroup);
    mUsedCanvasContexts.AppendElements(aBindGroup->GetCanvasContexts());
    bindGroup = aBindGroup->mId;
  }
  ffi::wgpu_recorded_render_pass_set_bind_group(
      mPass.get(), aSlot, bindGroup, aDynamicOffsets, aDynamicOffsetsLength);
}

void RenderPassEncoder::SetBindGroup(
    uint32_t aSlot, BindGroup* const aBindGroup,
    const dom::Sequence<uint32_t>& aDynamicOffsets, ErrorResult& aRv) {
  if (!mValid) {
    return;
  }
  this->SetBindGroup(aSlot, aBindGroup, aDynamicOffsets.Elements(),
                     aDynamicOffsets.Length());
}

void RenderPassEncoder::SetBindGroup(
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

void RenderPassEncoder::SetPipeline(const RenderPipeline& aPipeline) {
  if (!mValid) {
    return;
  }
  mUsedPipelines.AppendElement(&aPipeline);
  ffi::wgpu_recorded_render_pass_set_pipeline(mPass.get(), aPipeline.mId);
}

void RenderPassEncoder::SetIndexBuffer(const Buffer& aBuffer,
                                       const dom::GPUIndexFormat& aIndexFormat,
                                       uint64_t aOffset,
                                       const dom::Optional<uint64_t>& aSize) {
  if (!mValid) {
    return;
  }
  mUsedBuffers.AppendElement(&aBuffer);
  const auto iformat = aIndexFormat == dom::GPUIndexFormat::Uint32
                           ? ffi::WGPUIndexFormat_Uint32
                           : ffi::WGPUIndexFormat_Uint16;
  const uint64_t* sizeRef = aSize.WasPassed() ? &aSize.Value() : nullptr;
  ffi::wgpu_recorded_render_pass_set_index_buffer(mPass.get(), aBuffer.mId,
                                                  iformat, aOffset, sizeRef);
}

void RenderPassEncoder::SetVertexBuffer(uint32_t aSlot, const Buffer& aBuffer,
                                        uint64_t aOffset,
                                        const dom::Optional<uint64_t>& aSize) {
  if (!mValid) {
    return;
  }
  mUsedBuffers.AppendElement(&aBuffer);

  const uint64_t* sizeRef = aSize.WasPassed() ? &aSize.Value() : nullptr;
  ffi::wgpu_recorded_render_pass_set_vertex_buffer(
      mPass.get(), aSlot, aBuffer.mId, aOffset, sizeRef);
}

void RenderPassEncoder::Draw(uint32_t aVertexCount, uint32_t aInstanceCount,
                             uint32_t aFirstVertex, uint32_t aFirstInstance) {
  if (!mValid) {
    return;
  }
  ffi::wgpu_recorded_render_pass_draw(mPass.get(), aVertexCount, aInstanceCount,
                                      aFirstVertex, aFirstInstance);
}

void RenderPassEncoder::DrawIndexed(uint32_t aIndexCount,
                                    uint32_t aInstanceCount,
                                    uint32_t aFirstIndex, int32_t aBaseVertex,
                                    uint32_t aFirstInstance) {
  if (!mValid) {
    return;
  }
  ffi::wgpu_recorded_render_pass_draw_indexed(mPass.get(), aIndexCount,
                                              aInstanceCount, aFirstIndex,
                                              aBaseVertex, aFirstInstance);
}

void RenderPassEncoder::DrawIndirect(const Buffer& aIndirectBuffer,
                                     uint64_t aIndirectOffset) {
  if (!mValid) {
    return;
  }
  mUsedBuffers.AppendElement(&aIndirectBuffer);
  ffi::wgpu_recorded_render_pass_draw_indirect(mPass.get(), aIndirectBuffer.mId,
                                               aIndirectOffset);
}

void RenderPassEncoder::DrawIndexedIndirect(const Buffer& aIndirectBuffer,
                                            uint64_t aIndirectOffset) {
  if (!mValid) {
    return;
  }
  mUsedBuffers.AppendElement(&aIndirectBuffer);
  ffi::wgpu_recorded_render_pass_draw_indexed_indirect(
      mPass.get(), aIndirectBuffer.mId, aIndirectOffset);
}

void RenderPassEncoder::SetViewport(float x, float y, float width, float height,
                                    float minDepth, float maxDepth) {
  if (!mValid) {
    return;
  }
  ffi::wgpu_recorded_render_pass_set_viewport(mPass.get(), x, y, width, height,
                                              minDepth, maxDepth);
}

void RenderPassEncoder::SetScissorRect(uint32_t x, uint32_t y, uint32_t width,
                                       uint32_t height) {
  if (!mValid) {
    return;
  }
  ffi::wgpu_recorded_render_pass_set_scissor_rect(mPass.get(), x, y, width,
                                                  height);
}

void RenderPassEncoder::SetBlendConstant(
    const dom::DoubleSequenceOrGPUColorDict& color) {
  if (!mValid) {
    return;
  }
  ffi::WGPUColor aColor = ConvertColor(color);
  ffi::wgpu_recorded_render_pass_set_blend_constant(mPass.get(), &aColor);
}

void RenderPassEncoder::SetStencilReference(uint32_t reference) {
  if (!mValid) {
    return;
  }
  ffi::wgpu_recorded_render_pass_set_stencil_reference(mPass.get(), reference);
}

void RenderPassEncoder::BeginOcclusionQuery(uint32_t aQueryIndex) {
  if (!mValid) {
    return;
  }
  ffi::wgpu_recorded_render_pass_begin_occlusion_query(mPass.get(),
                                                       aQueryIndex);
}

void RenderPassEncoder::EndOcclusionQuery() {
  if (!mValid) {
    return;
  }
  ffi::wgpu_recorded_render_pass_end_occlusion_query(mPass.get());
}

void RenderPassEncoder::ExecuteBundles(
    const dom::Sequence<OwningNonNull<RenderBundle>>& aBundles) {
  if (!mValid) {
    return;
  }
  nsTArray<ffi::WGPURenderBundleId> renderBundles(aBundles.Length());
  for (const auto& bundle : aBundles) {
    mUsedRenderBundles.AppendElement(bundle);
    mUsedCanvasContexts.AppendElements(bundle->GetCanvasContexts());
    renderBundles.AppendElement(bundle->mId);
  }
  ffi::wgpu_recorded_render_pass_execute_bundles(
      mPass.get(), renderBundles.Elements(), renderBundles.Length());
}

void RenderPassEncoder::PushDebugGroup(const nsAString& aString) {
  if (!mValid) {
    return;
  }
  const NS_ConvertUTF16toUTF8 utf8(aString);
  ffi::wgpu_recorded_render_pass_push_debug_group(mPass.get(), utf8.get(), 0);
}
void RenderPassEncoder::PopDebugGroup() {
  if (!mValid) {
    return;
  }
  ffi::wgpu_recorded_render_pass_pop_debug_group(mPass.get());
}
void RenderPassEncoder::InsertDebugMarker(const nsAString& aString) {
  if (!mValid) {
    return;
  }
  const NS_ConvertUTF16toUTF8 utf8(aString);
  ffi::wgpu_recorded_render_pass_insert_debug_marker(mPass.get(), utf8.get(),
                                                     0);
}

void RenderPassEncoder::End() {
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
  mParent->EndRenderPass(*mPass, mUsedCanvasContexts);
  Cleanup();
}

}  // namespace mozilla::webgpu
