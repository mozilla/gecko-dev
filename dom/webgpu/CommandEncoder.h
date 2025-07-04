/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GPU_CommandEncoder_H_
#define GPU_CommandEncoder_H_

#include "mozilla/dom/TypedArray.h"
#include "mozilla/RefPtr.h"
#include "mozilla/WeakPtr.h"
#include "mozilla/webgpu/ffi/wgpu.h"
#include "mozilla/webgpu/WebGPUTypes.h"
#include "nsWrapperCache.h"
#include "CanvasContext.h"
#include "ObjectModel.h"
#include "QuerySet.h"

namespace mozilla {
class ErrorResult;

namespace dom {
struct GPUComputePassDescriptor;
template <typename T>
class Sequence;
struct GPUCommandBufferDescriptor;
class GPUComputePipelineOrGPURenderPipeline;
class RangeEnforcedUnsignedLongSequenceOrGPUExtent3DDict;
struct GPUTexelCopyBufferInfo;
struct GPUTexelCopyTextureInfo;
struct GPUImageBitmapCopyView;
struct GPUTexelCopyBufferLayout;
struct GPURenderPassDescriptor;
using GPUExtent3D = RangeEnforcedUnsignedLongSequenceOrGPUExtent3DDict;
}  // namespace dom
namespace webgpu {

class BindGroup;
class Buffer;
class CanvasContext;
class CommandBuffer;
class ComputePassEncoder;
class Device;
class RenderPassEncoder;
class WebGPUChild;

enum class CommandEncoderState { Open, Locked, Ended };

class CommandEncoder final : public ObjectBase, public ChildOf<Device> {
 public:
  GPU_DECL_CYCLE_COLLECTION(CommandEncoder)
  GPU_DECL_JS_WRAP(CommandEncoder)

  CommandEncoder(Device* const aParent, WebGPUChild* const aBridge, RawId aId);

  const RawId mId;

  static void ConvertTextureDataLayoutToFFI(
      const dom::GPUTexelCopyBufferLayout& aLayout,
      ffi::WGPUTexelCopyBufferLayout* aLayoutFFI);
  static void ConvertTextureCopyViewToFFI(
      const dom::GPUTexelCopyTextureInfo& aCopy,
      ffi::WGPUTexelCopyTextureInfo_TextureId* aViewFFI);

 private:
  ~CommandEncoder();
  void Cleanup();

  CommandEncoderState mState;

  RefPtr<WebGPUChild> mBridge;
  CanvasContextArray mPresentationContexts;

  void TrackPresentationContext(WeakPtr<CanvasContext> aTargetContext);

 public:
  const auto& GetDevice() const { return mParent; };
  RefPtr<WebGPUChild> GetBridge();

  CommandEncoderState GetState() const { return mState; };

  void EndComputePass(ffi::WGPURecordedComputePass& aPass,
                      CanvasContextArray& aCanvasContexts);
  void EndRenderPass(ffi::WGPURecordedRenderPass& aPass,
                     CanvasContextArray& aCanvasContexts);

  void CopyBufferToBuffer(const Buffer& aSource, const Buffer& aDestination,
                          const dom::Optional<BufferAddress>& aSize) {
    this->CopyBufferToBuffer(aSource, 0, aDestination, 0, aSize);
  }
  void CopyBufferToBuffer(const Buffer& aSource, BufferAddress aSourceOffset,
                          const Buffer& aDestination,
                          BufferAddress aDestinationOffset,
                          const dom::Optional<BufferAddress>& aSize);
  void CopyBufferToTexture(const dom::GPUTexelCopyBufferInfo& aSource,
                           const dom::GPUTexelCopyTextureInfo& aDestination,
                           const dom::GPUExtent3D& aCopySize);
  void CopyTextureToBuffer(const dom::GPUTexelCopyTextureInfo& aSource,
                           const dom::GPUTexelCopyBufferInfo& aDestination,
                           const dom::GPUExtent3D& aCopySize);
  void CopyTextureToTexture(const dom::GPUTexelCopyTextureInfo& aSource,
                            const dom::GPUTexelCopyTextureInfo& aDestination,
                            const dom::GPUExtent3D& aCopySize);
  void ClearBuffer(const Buffer& aBuffer, const uint64_t aOffset,
                   const dom::Optional<uint64_t>& aSize);

  void PushDebugGroup(const nsAString& aString);
  void PopDebugGroup();
  void InsertDebugMarker(const nsAString& aString);

  already_AddRefed<ComputePassEncoder> BeginComputePass(
      const dom::GPUComputePassDescriptor& aDesc);
  already_AddRefed<RenderPassEncoder> BeginRenderPass(
      const dom::GPURenderPassDescriptor& aDesc);
  void ResolveQuerySet(QuerySet& aQuerySet, uint32_t aFirstQuery,
                       uint32_t aQueryCount, webgpu::Buffer& aDestination,
                       uint64_t aDestinationOffset);
  already_AddRefed<CommandBuffer> Finish(
      const dom::GPUCommandBufferDescriptor& aDesc);
};

template <typename T>
void AssignPassTimestampWrites(const T& src,
                               ffi::WGPUPassTimestampWrites& dest) {
  if (src.mBeginningOfPassWriteIndex.WasPassed()) {
    dest.beginning_of_pass_write_index =
        &src.mBeginningOfPassWriteIndex.Value();
  } else {
    dest.beginning_of_pass_write_index = nullptr;
  }

  if (src.mEndOfPassWriteIndex.WasPassed()) {
    dest.end_of_pass_write_index = &src.mEndOfPassWriteIndex.Value();
  } else {
    dest.end_of_pass_write_index = nullptr;
  }

  dest.query_set = src.mQuerySet->mId;
}

}  // namespace webgpu
}  // namespace mozilla

#endif  // GPU_CommandEncoder_H_
