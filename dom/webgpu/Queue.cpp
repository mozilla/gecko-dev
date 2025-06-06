/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/WebGPUBinding.h"
#include "mozilla/dom/UnionTypes.h"
#include "Queue.h"

#include <algorithm>

#include "CommandBuffer.h"
#include "CommandEncoder.h"
#include "ipc/WebGPUChild.h"
#include "mozilla/Casting.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/BufferSourceBinding.h"
#include "mozilla/dom/HTMLImageElement.h"
#include "mozilla/dom/HTMLCanvasElement.h"
#include "mozilla/dom/ImageBitmap.h"
#include "mozilla/dom/OffscreenCanvas.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/WebGLTexelConversions.h"
#include "mozilla/dom/WebGLTypes.h"
#include "mozilla/ipc/SharedMemoryHandle.h"
#include "mozilla/ipc/SharedMemoryMapping.h"
#include "nsLayoutUtils.h"
#include "Utility.h"

namespace mozilla::webgpu {

GPU_IMPL_CYCLE_COLLECTION(Queue, mParent, mBridge)
GPU_IMPL_JS_WRAP(Queue)

Queue::Queue(Device* const aParent, WebGPUChild* aBridge, RawId aId)
    : ChildOf(aParent), mId(aId), mBridge(aBridge) {
  MOZ_RELEASE_ASSERT(aId);
}

Queue::~Queue() { Cleanup(); }

void Queue::Submit(
    const dom::Sequence<OwningNonNull<CommandBuffer>>& aCommandBuffers) {
  nsTArray<RawId> list(aCommandBuffers.Length());
  for (uint32_t i = 0; i < aCommandBuffers.Length(); ++i) {
    auto idMaybe = aCommandBuffers[i]->Commit();
    if (idMaybe) {
      list.AppendElement(*idMaybe);
    }
  }

  mBridge->QueueSubmit(mId, mParent->mId, list);
}

already_AddRefed<dom::Promise> Queue::OnSubmittedWorkDone(ErrorResult& aRv) {
  RefPtr<dom::Promise> promise = dom::Promise::Create(GetParentObject(), aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }
  mBridge->QueueOnSubmittedWorkDone(mId, promise);

  return promise.forget();
}

void Queue::WriteBuffer(
    const Buffer& aBuffer, uint64_t aBufferOffset,
    const dom::MaybeSharedArrayBufferOrMaybeSharedArrayBufferView& aData,
    uint64_t aDataOffset, const dom::Optional<uint64_t>& aSize,
    ErrorResult& aRv) {
  if (!aBuffer.mId) {
    // Invalid buffers are unknown to the parent -- don't try to write
    // to them.
    return;
  }

  size_t elementByteSize = 1;
  if (aData.IsArrayBufferView()) {
    auto type = aData.GetAsArrayBufferView().Type();
    if (type != JS::Scalar::MaxTypedArrayViewType) {
      elementByteSize = byteSize(type);
    }
  }
  dom::ProcessTypedArraysFixed(
      aData, [&, elementByteSize](const Span<const uint8_t>& aData) {
        uint64_t byteLength = aData.Length();

        auto checkedByteOffset =
            CheckedInt<uint64_t>(aDataOffset) * elementByteSize;
        if (!checkedByteOffset.isValid()) {
          aRv.ThrowOperationError("offset x element size overflows");
          return;
        }
        auto offset = checkedByteOffset.value();

        size_t size;
        if (aSize.WasPassed()) {
          const auto checkedByteSize =
              CheckedInt<size_t>(aSize.Value()) * elementByteSize;
          if (!checkedByteSize.isValid()) {
            aRv.ThrowOperationError("write size x element size overflows");
            return;
          }
          size = checkedByteSize.value();
        } else {
          const auto checkedByteSize = CheckedInt<size_t>(byteLength) - offset;
          if (!checkedByteSize.isValid()) {
            aRv.ThrowOperationError("data byte length - offset underflows");
            return;
          }
          size = checkedByteSize.value();
        }

        auto checkedByteEnd = CheckedInt<uint64_t>(offset) + size;
        if (!checkedByteEnd.isValid() || checkedByteEnd.value() > byteLength) {
          aRv.ThrowOperationError(
              nsPrintfCString("Wrong data size %" PRIuPTR, size));
          return;
        }

        if (size % 4 != 0) {
          aRv.ThrowOperationError("Byte size must be a multiple of 4");
          return;
        }

        mozilla::ipc::MutableSharedMemoryHandle handle;
        if (size != 0) {
          handle = mozilla::ipc::shared_memory::Create(size);
          auto mapping = handle.Map();
          if (!handle || !mapping) {
            aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
            return;
          }

          memcpy(mapping.DataAs<uint8_t>(), aData.Elements() + offset, size);
        }
        ipc::ByteBuf bb;
        ffi::wgpu_queue_write_buffer(aBuffer.mId, aBufferOffset, ToFFI(&bb));
        mBridge->SendQueueWriteAction(mId, mParent->mId, std::move(bb),
                                      std::move(handle));
      });
}

static CheckedInt<size_t> ComputeApproxSize(
    const dom::GPUTexelCopyTextureInfo& aDestination,
    const dom::GPUTexelCopyBufferLayout& aDataLayout,
    const ffi::WGPUExtent3d& extent,
    const ffi::WGPUTextureFormatBlockInfo& info) {
  // The spec's algorithm for [validating linear texture data][vltd] computes
  // an exact size for the transfer. wgpu implements the algorithm and will
  // fully validate the operation as described in the spec.
  //
  // Here, we just want to avoid copying excessive amounts of data in the case
  // where the transfer will use only a small portion of the buffer. So we
  // compute an approximation that will be at least the actual transfer size
  // for any valid request. Then we copy the smaller of the approximated size
  // or the remainder of the buffer.
  //
  // [vltd]:
  // https://www.w3.org/TR/webgpu/#abstract-opdef-validating-linear-texture-data

  // VLTD requires that width/height are multiples of the block size.
  auto widthInBlocks = extent.width / info.width;
  auto heightInBlocks = extent.height / info.height;
  auto bytesInLastRow = CheckedInt<size_t>(widthInBlocks) * info.copy_size;

  // VLTD requires bytesPerRow present if heightInBlocks > 1.
  auto bytesPerRow = CheckedInt<size_t>(aDataLayout.mBytesPerRow.WasPassed()
                                            ? aDataLayout.mBytesPerRow.Value()
                                            : bytesInLastRow);

  if (extent.depth_or_array_layers > 1) {
    // VLTD requires rowsPerImage present if layers > 1
    auto rowsPerImage = aDataLayout.mRowsPerImage.WasPassed()
                            ? aDataLayout.mRowsPerImage.Value()
                            : heightInBlocks;
    return bytesPerRow * rowsPerImage * extent.depth_or_array_layers;
  } else {
    return bytesPerRow * heightInBlocks;
  }
}

void Queue::WriteTexture(
    const dom::GPUTexelCopyTextureInfo& aDestination,
    const dom::MaybeSharedArrayBufferOrMaybeSharedArrayBufferView& aData,
    const dom::GPUTexelCopyBufferLayout& aDataLayout,
    const dom::GPUExtent3D& aSize, ErrorResult& aRv) {
  ffi::WGPUTexelCopyTextureInfo copyView = {};
  CommandEncoder::ConvertTextureCopyViewToFFI(aDestination, &copyView);
  ffi::WGPUTexelCopyBufferLayout dataLayout = {};
  CommandEncoder::ConvertTextureDataLayoutToFFI(aDataLayout, &dataLayout);
  dataLayout.offset = 0;  // our Shmem has the contents starting from 0.
  ffi::WGPUExtent3d extent = {};
  ConvertExtent3DToFFI(aSize, &extent);

  auto format = ConvertTextureFormat(aDestination.mTexture->Format());
  auto aspect = ConvertTextureAspect(aDestination.mAspect);
  ffi::WGPUTextureFormatBlockInfo info = {};
  bool valid = ffi::wgpu_texture_format_get_block_info(format, aspect, &info);
  CheckedInt<size_t> approxSize;
  if (valid) {
    approxSize = ComputeApproxSize(aDestination, aDataLayout, extent, info);
  } else {
    // This happens when the caller does not indicate a single aspect to
    // target in a multi-aspect texture. It needs to be validated on the
    // device timeline, so proceed without an estimated size for now
    approxSize = CheckedInt<size_t>(SIZE_MAX) + 1;
  }

  dom::ProcessTypedArraysFixed(aData, [&](const Span<const uint8_t>& aData) {
    const auto checkedSize =
        CheckedInt<size_t>(aData.Length()) - aDataLayout.mOffset;
    size_t size;
    if (checkedSize.isValid() && approxSize.isValid()) {
      size = std::min(checkedSize.value(), approxSize.value());
    } else if (checkedSize.isValid()) {
      size = checkedSize.value();
    } else {
      // CheckedSize is invalid when the caller-provided offset was past the
      // end of their buffer. Maintain that condition, and fail the operation
      // on the device timeline.
      dataLayout.offset = 1;
      size = 0;
    }

    mozilla::ipc::MutableSharedMemoryHandle handle;
    if (size != 0) {
      handle = mozilla::ipc::shared_memory::Create(size);
      auto mapping = handle.Map();
      if (!handle || !mapping) {
        aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
        return;
      }

      memcpy(mapping.DataAs<uint8_t>(), aData.Elements() + aDataLayout.mOffset,
             size);
    } else {
      handle = mozilla::ipc::MutableSharedMemoryHandle();
    }

    ipc::ByteBuf bb;
    ffi::wgpu_queue_write_texture(copyView, dataLayout, extent, ToFFI(&bb));
    mBridge->SendQueueWriteAction(mId, mParent->mId, std::move(bb),
                                  std::move(handle));
  });
}

static WebGLTexelFormat ToWebGLTexelFormat(gfx::SurfaceFormat aFormat) {
  switch (aFormat) {
    case gfx::SurfaceFormat::B8G8R8A8:
    case gfx::SurfaceFormat::B8G8R8X8:
      return WebGLTexelFormat::BGRA8;
    case gfx::SurfaceFormat::R8G8B8A8:
    case gfx::SurfaceFormat::R8G8B8X8:
      return WebGLTexelFormat::RGBA8;
    default:
      return WebGLTexelFormat::FormatNotSupportingAnyConversion;
  }
}

static WebGLTexelFormat ToWebGLTexelFormat(dom::GPUTextureFormat aFormat) {
  // TODO: We need support for Rbg10a2unorm as well.
  switch (aFormat) {
    case dom::GPUTextureFormat::R8unorm:
      return WebGLTexelFormat::R8;
    case dom::GPUTextureFormat::R16float:
      return WebGLTexelFormat::R16F;
    case dom::GPUTextureFormat::R32float:
      return WebGLTexelFormat::R32F;
    case dom::GPUTextureFormat::Rg8unorm:
      return WebGLTexelFormat::RG8;
    case dom::GPUTextureFormat::Rg16float:
      return WebGLTexelFormat::RG16F;
    case dom::GPUTextureFormat::Rg32float:
      return WebGLTexelFormat::RG32F;
    case dom::GPUTextureFormat::Rgba8unorm:
    case dom::GPUTextureFormat::Rgba8unorm_srgb:
      return WebGLTexelFormat::RGBA8;
    case dom::GPUTextureFormat::Bgra8unorm:
    case dom::GPUTextureFormat::Bgra8unorm_srgb:
      return WebGLTexelFormat::BGRA8;
    case dom::GPUTextureFormat::Rgba16float:
      return WebGLTexelFormat::RGBA16F;
    case dom::GPUTextureFormat::Rgba32float:
      return WebGLTexelFormat::RGBA32F;
    default:
      return WebGLTexelFormat::FormatNotSupportingAnyConversion;
  }
}

void Queue::CopyExternalImageToTexture(
    const dom::GPUCopyExternalImageSourceInfo& aSource,
    const dom::GPUCopyExternalImageDestInfo& aDestination,
    const dom::GPUExtent3D& aCopySize, ErrorResult& aRv) {
  const auto dstFormat = ToWebGLTexelFormat(aDestination.mTexture->Format());
  if (dstFormat == WebGLTexelFormat::FormatNotSupportingAnyConversion) {
    aRv.ThrowInvalidStateError("Unsupported destination format");
    return;
  }

  const uint32_t surfaceFlags = nsLayoutUtils::SFE_ALLOW_NON_PREMULT;
  SurfaceFromElementResult sfeResult;
  switch (aSource.mSource.GetType()) {
    case decltype(aSource.mSource)::Type::eImageBitmap: {
      const auto& bitmap = aSource.mSource.GetAsImageBitmap();
      if (bitmap->IsClosed()) {
        aRv.ThrowInvalidStateError("Detached ImageBitmap");
        return;
      }

      sfeResult = nsLayoutUtils::SurfaceFromImageBitmap(bitmap, surfaceFlags);
      break;
    }
    case decltype(aSource.mSource)::Type::eHTMLImageElement: {
      const auto& image = aSource.mSource.GetAsHTMLImageElement();
      if (image->NaturalWidth() == 0 || image->NaturalHeight() == 0) {
        aRv.ThrowInvalidStateError("Zero-sized HTMLImageElement");
        return;
      }

      sfeResult = nsLayoutUtils::SurfaceFromElement(image, surfaceFlags);
      break;
    }
    case decltype(aSource.mSource)::Type::eHTMLCanvasElement: {
      MOZ_ASSERT(NS_IsMainThread());

      const auto& canvas = aSource.mSource.GetAsHTMLCanvasElement();
      if (canvas->Width() == 0 || canvas->Height() == 0) {
        aRv.ThrowInvalidStateError("Zero-sized HTMLCanvasElement");
        return;
      }

      sfeResult = nsLayoutUtils::SurfaceFromElement(canvas, surfaceFlags);
      break;
    }
    case decltype(aSource.mSource)::Type::eOffscreenCanvas: {
      const auto& canvas = aSource.mSource.GetAsOffscreenCanvas();
      if (canvas->Width() == 0 || canvas->Height() == 0) {
        aRv.ThrowInvalidStateError("Zero-sized OffscreenCanvas");
        return;
      }

      sfeResult =
          nsLayoutUtils::SurfaceFromOffscreenCanvas(canvas, surfaceFlags);
      break;
    }
  }

  if (!sfeResult.mCORSUsed) {
    nsIGlobalObject* global = mParent->GetOwnerGlobal();
    nsIPrincipal* dstPrincipal = global ? global->PrincipalOrNull() : nullptr;
    if (!sfeResult.mPrincipal || !dstPrincipal ||
        !dstPrincipal->Subsumes(sfeResult.mPrincipal)) {
      aRv.ThrowSecurityError("Cross-origin elements require CORS!");
      return;
    }
  }

  if (sfeResult.mIsWriteOnly) {
    aRv.ThrowSecurityError("Write only source data not supported!");
    return;
  }

  RefPtr<gfx::SourceSurface> surface = sfeResult.GetSourceSurface();
  if (!surface) {
    aRv.ThrowInvalidStateError("No surface available from source");
    return;
  }

  RefPtr<gfx::DataSourceSurface> dataSurface = surface->GetDataSurface();
  if (!dataSurface) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return;
  }

  bool srcPremultiplied;
  switch (sfeResult.mAlphaType) {
    case gfxAlphaType::Premult:
      srcPremultiplied = true;
      break;
    case gfxAlphaType::NonPremult:
      srcPremultiplied = false;
      break;
    case gfxAlphaType::Opaque:
      // No (un)premultiplication necessary so match the output.
      srcPremultiplied = aDestination.mPremultipliedAlpha;
      break;
  }

  const auto surfaceFormat = dataSurface->GetFormat();
  const auto srcFormat = ToWebGLTexelFormat(surfaceFormat);
  if (srcFormat == WebGLTexelFormat::FormatNotSupportingAnyConversion) {
    gfxCriticalError() << "Unsupported surface format from source "
                       << surfaceFormat;
    MOZ_CRASH();
  }

  gfx::DataSourceSurface::ScopedMap map(dataSurface,
                                        gfx::DataSourceSurface::READ);
  if (!map.IsMapped()) {
    aRv.ThrowInvalidStateError("Cannot map surface from source");
    return;
  }

  if (!aSource.mOrigin.IsGPUOrigin2DDict()) {
    aRv.ThrowInvalidStateError("Cannot get origin from source");
    return;
  }

  ffi::WGPUExtent3d extent = {};
  ConvertExtent3DToFFI(aCopySize, &extent);
  if (extent.depth_or_array_layers > 1) {
    aRv.ThrowOperationError("Depth is greater than 1");
    return;
  }

  uint32_t srcOriginX;
  uint32_t srcOriginY;
  if (aSource.mOrigin.IsRangeEnforcedUnsignedLongSequence()) {
    const auto& seq = aSource.mOrigin.GetAsRangeEnforcedUnsignedLongSequence();
    srcOriginX = seq.Length() > 0 ? seq[0] : 0;
    srcOriginY = seq.Length() > 1 ? seq[1] : 0;
  } else if (aSource.mOrigin.IsGPUOrigin2DDict()) {
    const auto& dict = aSource.mOrigin.GetAsGPUOrigin2DDict();
    srcOriginX = dict.mX;
    srcOriginY = dict.mY;
  } else {
    MOZ_CRASH("Unexpected origin type!");
  }

  const auto checkedMaxWidth = CheckedInt<uint32_t>(srcOriginX) + extent.width;
  const auto checkedMaxHeight =
      CheckedInt<uint32_t>(srcOriginY) + extent.height;
  if (!checkedMaxWidth.isValid() || !checkedMaxHeight.isValid()) {
    aRv.ThrowOperationError("Offset and copy size exceed integer bounds");
    return;
  }

  const gfx::IntSize surfaceSize = dataSurface->GetSize();
  const auto surfaceWidth = AssertedCast<uint32_t>(surfaceSize.width);
  const auto surfaceHeight = AssertedCast<uint32_t>(surfaceSize.height);
  if (surfaceWidth < checkedMaxWidth.value() ||
      surfaceHeight < checkedMaxHeight.value()) {
    aRv.ThrowOperationError("Offset and copy size exceed surface bounds");
    return;
  }

  const auto dstWidth = extent.width;
  const auto dstHeight = extent.height;
  if (dstWidth == 0 || dstHeight == 0) {
    aRv.ThrowOperationError("Destination size is empty");
    return;
  }

  if (!aDestination.mTexture->mBytesPerBlock) {
    // TODO(bug 1781071) This should emmit a GPUValidationError on the device
    // timeline.
    aRv.ThrowInvalidStateError("Invalid destination format");
    return;
  }

  // Note: This assumes bytes per block == bytes per pixel which is the case
  // here because the spec only allows non-compressed texture formats for the
  // destination.
  const auto dstStride = CheckedInt<uint32_t>(extent.width) *
                         aDestination.mTexture->mBytesPerBlock.value();
  const auto dstByteLength = dstStride * extent.height;
  if (!dstStride.isValid() || !dstByteLength.isValid()) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return;
  }

  auto handle = mozilla::ipc::shared_memory::Create(dstByteLength.value());
  auto mapping = handle.Map();
  if (!handle || !mapping) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return;
  }

  const int32_t pixelSize = gfx::BytesPerPixel(surfaceFormat);
  auto* dstBegin = mapping.DataAs<uint8_t>();
  const auto* srcBegin =
      map.GetData() + srcOriginX * pixelSize + srcOriginY * map.GetStride();
  const auto srcOriginPos = gl::OriginPos::TopLeft;
  const auto srcStride = AssertedCast<uint32_t>(map.GetStride());
  const auto dstOriginPos =
      aSource.mFlipY ? gl::OriginPos::BottomLeft : gl::OriginPos::TopLeft;
  bool wasTrivial;

  auto dstStrideVal = dstStride.value();

  if (!ConvertImage(dstWidth, dstHeight, srcBegin, srcStride, srcOriginPos,
                    srcFormat, srcPremultiplied, dstBegin, dstStrideVal,
                    dstOriginPos, dstFormat, aDestination.mPremultipliedAlpha,
                    dom::PredefinedColorSpace::Srgb,
                    dom::PredefinedColorSpace::Srgb, &wasTrivial)) {
    MOZ_ASSERT_UNREACHABLE("ConvertImage failed!");
    aRv.ThrowInvalidStateError(
        nsPrintfCString("Failed to convert source to destination format "
                        "(%i/%i), please file a bug!",
                        (int)srcFormat, (int)dstFormat));
    return;
  }

  ffi::WGPUTexelCopyBufferLayout dataLayout = {0, &dstStrideVal, &dstHeight};
  ffi::WGPUTexelCopyTextureInfo copyView = {};
  CommandEncoder::ConvertTextureCopyViewToFFI(aDestination, &copyView);
  ipc::ByteBuf bb;
  ffi::wgpu_queue_write_texture(copyView, dataLayout, extent, ToFFI(&bb));
  mBridge->SendQueueWriteAction(mId, mParent->mId, std::move(bb),
                                std::move(handle));
}

}  // namespace mozilla::webgpu
