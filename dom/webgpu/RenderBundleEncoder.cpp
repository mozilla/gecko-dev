/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/WebGPUBinding.h"
#include "RenderBundleEncoder.h"

#include "BindGroup.h"
#include "Buffer.h"
#include "RenderBundle.h"
#include "RenderPipeline.h"
#include "Utility.h"
#include "ipc/WebGPUChild.h"
#include "mozilla/webgpu/ffi/wgpu.h"

namespace mozilla::webgpu {

GPU_IMPL_CYCLE_COLLECTION(RenderBundleEncoder, mParent, mUsedBindGroups,
                          mUsedBuffers, mUsedPipelines)
GPU_IMPL_JS_WRAP(RenderBundleEncoder)

void ffiWGPURenderBundleEncoderDeleter::operator()(
    ffi::WGPURenderBundleEncoder* raw) {
  if (raw) {
    ffi::wgpu_render_bundle_encoder_destroy(raw);
  }
}

ffi::WGPURenderBundleEncoder* CreateRenderBundleEncoder(
    RawId aDeviceId, const dom::GPURenderBundleEncoderDescriptor& aDesc,
    WebGPUChild* const aBridge) {
  if (!aBridge->CanSend()) {
    return nullptr;
  }

  ffi::WGPURenderBundleEncoderDescriptor desc = {};
  desc.sample_count = aDesc.mSampleCount;

  webgpu::StringHelper label(aDesc.mLabel);
  desc.label = label.Get();

  ffi::WGPUTextureFormat depthStencilFormat = {ffi::WGPUTextureFormat_Sentinel};
  if (aDesc.mDepthStencilFormat.WasPassed()) {
    depthStencilFormat =
        ConvertTextureFormat(aDesc.mDepthStencilFormat.Value());
    desc.depth_stencil_format = &depthStencilFormat;
  }

  std::vector<ffi::WGPUTextureFormat> colorFormats = {};
  for (const auto i : IntegerRange(aDesc.mColorFormats.Length())) {
    ffi::WGPUTextureFormat format = {ffi::WGPUTextureFormat_Sentinel};
    format = ConvertTextureFormat(aDesc.mColorFormats[i]);
    colorFormats.push_back(format);
  }

  desc.color_formats = colorFormats.data();
  desc.color_formats_length = colorFormats.size();

  ipc::ByteBuf failureAction;
  auto* bundle = ffi::wgpu_device_create_render_bundle_encoder(
      aDeviceId, &desc, ToFFI(&failureAction));
  // Report an error only if the operation failed.
  if (!bundle) {
    aBridge->SendDeviceAction(aDeviceId, std::move(failureAction));
  }
  return bundle;
}

RenderBundleEncoder::RenderBundleEncoder(
    Device* const aParent, WebGPUChild* const aBridge,
    const dom::GPURenderBundleEncoderDescriptor& aDesc)
    : ChildOf(aParent),
      mEncoder(CreateRenderBundleEncoder(aParent->mId, aDesc, aBridge)) {
  mValid = !!mEncoder;
}

RenderBundleEncoder::~RenderBundleEncoder() { Cleanup(); }

void RenderBundleEncoder::Cleanup() {
  mValid = false;
  mEncoder.release();
  mUsedBindGroups.Clear();
  mUsedBuffers.Clear();
  mUsedPipelines.Clear();
}

void RenderBundleEncoder::SetBindGroup(uint32_t aSlot,
                                       BindGroup* const aBindGroup,
                                       const uint32_t* aDynamicOffsets,
                                       uint64_t aDynamicOffsetsLength) {
  RawId bindGroup = 0;
  if (aBindGroup) {
    mUsedBindGroups.AppendElement(aBindGroup);
    mUsedCanvasContexts.AppendElements(aBindGroup->GetCanvasContexts());
    bindGroup = aBindGroup->mId;
  }
  ffi::wgpu_render_bundle_set_bind_group(
      mEncoder.get(), aSlot, bindGroup, aDynamicOffsets, aDynamicOffsetsLength);
}

void RenderBundleEncoder::SetBindGroup(
    uint32_t aSlot, BindGroup* const aBindGroup,
    const dom::Sequence<uint32_t>& aDynamicOffsets, ErrorResult& aRv) {
  if (!mValid) {
    return;
  }
  this->SetBindGroup(aSlot, aBindGroup, aDynamicOffsets.Elements(),
                     aDynamicOffsets.Length());
}

void RenderBundleEncoder::SetBindGroup(
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

void RenderBundleEncoder::SetPipeline(const RenderPipeline& aPipeline) {
  if (!mValid) {
    return;
  }
  mUsedPipelines.AppendElement(&aPipeline);
  ffi::wgpu_render_bundle_set_pipeline(mEncoder.get(), aPipeline.mId);
}

void RenderBundleEncoder::SetIndexBuffer(
    const Buffer& aBuffer, const dom::GPUIndexFormat& aIndexFormat,
    uint64_t aOffset, const dom::Optional<uint64_t>& aSize) {
  if (!mValid) {
    return;
  }
  mUsedBuffers.AppendElement(&aBuffer);
  const auto iformat = aIndexFormat == dom::GPUIndexFormat::Uint32
                           ? ffi::WGPUIndexFormat_Uint32
                           : ffi::WGPUIndexFormat_Uint16;
  const uint64_t* sizeRef = aSize.WasPassed() ? &aSize.Value() : nullptr;
  ffi::wgpu_render_bundle_set_index_buffer(mEncoder.get(), aBuffer.mId, iformat,
                                           aOffset, sizeRef);
}

void RenderBundleEncoder::SetVertexBuffer(
    uint32_t aSlot, const Buffer& aBuffer, uint64_t aOffset,
    const dom::Optional<uint64_t>& aSize) {
  if (!mValid) {
    return;
  }
  mUsedBuffers.AppendElement(&aBuffer);
  const uint64_t* sizeRef = aSize.WasPassed() ? &aSize.Value() : nullptr;
  ffi::wgpu_render_bundle_set_vertex_buffer(mEncoder.get(), aSlot, aBuffer.mId,
                                            aOffset, sizeRef);
}

void RenderBundleEncoder::Draw(uint32_t aVertexCount, uint32_t aInstanceCount,
                               uint32_t aFirstVertex, uint32_t aFirstInstance) {
  if (!mValid) {
    return;
  }
  ffi::wgpu_render_bundle_draw(mEncoder.get(), aVertexCount, aInstanceCount,
                               aFirstVertex, aFirstInstance);
}

void RenderBundleEncoder::DrawIndexed(uint32_t aIndexCount,
                                      uint32_t aInstanceCount,
                                      uint32_t aFirstIndex, int32_t aBaseVertex,
                                      uint32_t aFirstInstance) {
  if (!mValid) {
    return;
  }
  ffi::wgpu_render_bundle_draw_indexed(mEncoder.get(), aIndexCount,
                                       aInstanceCount, aFirstIndex, aBaseVertex,
                                       aFirstInstance);
}

void RenderBundleEncoder::DrawIndirect(const Buffer& aIndirectBuffer,
                                       uint64_t aIndirectOffset) {
  if (!mValid) {
    return;
  }
  mUsedBuffers.AppendElement(&aIndirectBuffer);
  ffi::wgpu_render_bundle_draw_indirect(mEncoder.get(), aIndirectBuffer.mId,
                                        aIndirectOffset);
}

void RenderBundleEncoder::DrawIndexedIndirect(const Buffer& aIndirectBuffer,
                                              uint64_t aIndirectOffset) {
  if (!mValid) {
    return;
  }
  mUsedBuffers.AppendElement(&aIndirectBuffer);
  ffi::wgpu_render_bundle_draw_indexed_indirect(
      mEncoder.get(), aIndirectBuffer.mId, aIndirectOffset);
}

void RenderBundleEncoder::PushDebugGroup(const nsAString& aString) {
  if (!mValid) {
    return;
  }
  const NS_ConvertUTF16toUTF8 utf8(aString);
  ffi::wgpu_render_bundle_push_debug_group(mEncoder.get(), utf8.get());
}
void RenderBundleEncoder::PopDebugGroup() {
  if (!mValid) {
    return;
  }
  ffi::wgpu_render_bundle_pop_debug_group(mEncoder.get());
}
void RenderBundleEncoder::InsertDebugMarker(const nsAString& aString) {
  if (!mValid) {
    return;
  }
  const NS_ConvertUTF16toUTF8 utf8(aString);
  ffi::wgpu_render_bundle_insert_debug_marker(mEncoder.get(), utf8.get());
}

already_AddRefed<RenderBundle> RenderBundleEncoder::Finish(
    const dom::GPURenderBundleDescriptor& aDesc) {
  RawId deviceId = mParent->mId;
  auto bridge = mParent->GetBridge();
  MOZ_RELEASE_ASSERT(bridge);

  ffi::WGPURenderBundleDescriptor desc = {};
  webgpu::StringHelper label(aDesc.mLabel);
  desc.label = label.Get();

  ipc::ByteBuf bb;
  RawId id;
  if (mValid) {
    id = ffi::wgpu_client_create_render_bundle(
        bridge->GetClient(), mEncoder.get(), &desc, ToFFI(&bb));

  } else {
    id = ffi::wgpu_client_create_render_bundle_error(bridge->GetClient(),
                                                     label.Get(), ToFFI(&bb));
  }

  if (bridge->CanSend()) {
    bridge->SendDeviceAction(deviceId, std::move(bb));
  }

  Cleanup();

  auto canvasContexts = mUsedCanvasContexts.Clone();
  RefPtr<RenderBundle> bundle =
      new RenderBundle(mParent, id, std::move(canvasContexts));
  return bundle.forget();
}

}  // namespace mozilla::webgpu
