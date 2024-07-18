/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ImageConversion.h"

#include "skia/include/core/SkBitmap.h"
#include "skia/include/core/SkColorSpace.h"
#include "skia/include/core/SkImage.h"
#include "skia/include/core/SkImageInfo.h"

#include "ImageContainer.h"
#include "YCbCrUtils.h"
#include "libyuv/convert.h"
#include "libyuv/convert_from_argb.h"
#include "mozilla/PodOperations.h"
#include "mozilla/RefPtr.h"
#include "mozilla/Result.h"
#include "mozilla/dom/ImageBitmapBinding.h"
#include "mozilla/dom/ImageUtils.h"
#include "mozilla/gfx/Point.h"
#include "mozilla/gfx/Swizzle.h"
#include "nsThreadUtils.h"

using mozilla::ImageFormat;
using mozilla::dom::ImageBitmapFormat;
using mozilla::dom::ImageUtils;
using mozilla::gfx::DataSourceSurface;
using mozilla::gfx::IntSize;
using mozilla::gfx::SourceSurface;
using mozilla::gfx::SurfaceFormat;
using mozilla::layers::Image;
using mozilla::layers::PlanarYCbCrData;
using mozilla::layers::PlanarYCbCrImage;

static const PlanarYCbCrData* GetPlanarYCbCrData(Image* aImage) {
  switch (aImage->GetFormat()) {
    case ImageFormat::PLANAR_YCBCR:
      return aImage->AsPlanarYCbCrImage()->GetData();
    case ImageFormat::NV_IMAGE:
      return aImage->AsNVImage()->GetData();
    default:
      return nullptr;
  }
}

static nsresult MapRv(int aRv) {
  // Docs for libyuv::ConvertToI420 say:
  // Returns 0 for successful; -1 for invalid parameter. Non-zero for failure.
  switch (aRv) {
    case 0:
      return NS_OK;
    case -1:
      return NS_ERROR_INVALID_ARG;
    default:
      return NS_ERROR_FAILURE;
  }
}

namespace mozilla {

already_AddRefed<SourceSurface> GetSourceSurface(Image* aImage) {
  if (!aImage->AsGLImage() || NS_IsMainThread()) {
    return aImage->GetAsSourceSurface();
  }

  // GLImage::GetAsSourceSurface() only supports main thread
  RefPtr<SourceSurface> surf;
  NS_DispatchAndSpinEventLoopUntilComplete(
      "ImageToI420::GLImage::GetSourceSurface"_ns,
      mozilla::GetMainThreadSerialEventTarget(),
      NS_NewRunnableFunction(
          "ImageToI420::GLImage::GetSourceSurface",
          [&aImage, &surf]() { surf = aImage->GetAsSourceSurface(); }));

  return surf.forget();
}

nsresult ConvertToI420(Image* aImage, uint8_t* aDestY, int aDestStrideY,
                       uint8_t* aDestU, int aDestStrideU, uint8_t* aDestV,
                       int aDestStrideV) {
  if (!aImage->IsValid()) {
    return NS_ERROR_INVALID_ARG;
  }

  if (const PlanarYCbCrData* data = GetPlanarYCbCrData(aImage)) {
    const ImageUtils imageUtils(aImage);
    Maybe<dom::ImageBitmapFormat> format = imageUtils.GetFormat();
    if (format.isNothing()) {
      MOZ_ASSERT_UNREACHABLE("YUV format conversion not implemented");
      return NS_ERROR_NOT_IMPLEMENTED;
    }
    switch (format.value()) {
      case ImageBitmapFormat::YUV420P:
        return MapRv(libyuv::I420ToI420(
            data->mYChannel, data->mYStride, data->mCbChannel,
            data->mCbCrStride, data->mCrChannel, data->mCbCrStride, aDestY,
            aDestStrideY, aDestU, aDestStrideU, aDestV, aDestStrideV,
            aImage->GetSize().width, aImage->GetSize().height));
      case ImageBitmapFormat::YUV422P:
        return MapRv(libyuv::I422ToI420(
            data->mYChannel, data->mYStride, data->mCbChannel,
            data->mCbCrStride, data->mCrChannel, data->mCbCrStride, aDestY,
            aDestStrideY, aDestU, aDestStrideU, aDestV, aDestStrideV,
            aImage->GetSize().width, aImage->GetSize().height));
      case ImageBitmapFormat::YUV444P:
        return MapRv(libyuv::I444ToI420(
            data->mYChannel, data->mYStride, data->mCbChannel,
            data->mCbCrStride, data->mCrChannel, data->mCbCrStride, aDestY,
            aDestStrideY, aDestU, aDestStrideU, aDestV, aDestStrideV,
            aImage->GetSize().width, aImage->GetSize().height));
      case ImageBitmapFormat::YUV420SP_NV12:
        return MapRv(libyuv::NV12ToI420(
            data->mYChannel, data->mYStride, data->mCbChannel,
            data->mCbCrStride, aDestY, aDestStrideY, aDestU, aDestStrideU,
            aDestV, aDestStrideV, aImage->GetSize().width,
            aImage->GetSize().height));
      case ImageBitmapFormat::YUV420SP_NV21:
        return MapRv(libyuv::NV21ToI420(
            data->mYChannel, data->mYStride, data->mCrChannel,
            data->mCbCrStride, aDestY, aDestStrideY, aDestU, aDestStrideU,
            aDestV, aDestStrideV, aImage->GetSize().width,
            aImage->GetSize().height));
      default:
        MOZ_ASSERT_UNREACHABLE("YUV format conversion not implemented");
        return NS_ERROR_NOT_IMPLEMENTED;
    }
  }

  RefPtr<SourceSurface> surf = GetSourceSurface(aImage);
  if (!surf) {
    return NS_ERROR_FAILURE;
  }

  RefPtr<DataSourceSurface> data = surf->GetDataSurface();
  if (!data) {
    return NS_ERROR_FAILURE;
  }

  DataSourceSurface::ScopedMap map(data, DataSourceSurface::READ);
  if (!map.IsMapped()) {
    return NS_ERROR_FAILURE;
  }

  switch (surf->GetFormat()) {
    case SurfaceFormat::B8G8R8A8:
    case SurfaceFormat::B8G8R8X8:
      return MapRv(libyuv::ARGBToI420(
          static_cast<uint8_t*>(map.GetData()), map.GetStride(), aDestY,
          aDestStrideY, aDestU, aDestStrideU, aDestV, aDestStrideV,
          aImage->GetSize().width, aImage->GetSize().height));
    case SurfaceFormat::R5G6B5_UINT16:
      return MapRv(libyuv::RGB565ToI420(
          static_cast<uint8_t*>(map.GetData()), map.GetStride(), aDestY,
          aDestStrideY, aDestU, aDestStrideU, aDestV, aDestStrideV,
          aImage->GetSize().width, aImage->GetSize().height));
    default:
      MOZ_ASSERT_UNREACHABLE("Surface format conversion not implemented");
      return NS_ERROR_NOT_IMPLEMENTED;
  }
}

nsresult ConvertToNV12(layers::Image* aImage, uint8_t* aDestY, int aDestStrideY,
                       uint8_t* aDestUV, int aDestStrideUV) {
  if (!aImage->IsValid()) {
    return NS_ERROR_INVALID_ARG;
  }

  if (const PlanarYCbCrData* data = GetPlanarYCbCrData(aImage)) {
    const ImageUtils imageUtils(aImage);
    Maybe<dom::ImageBitmapFormat> format = imageUtils.GetFormat();
    if (format.isNothing()) {
      MOZ_ASSERT_UNREACHABLE("YUV format conversion not implemented");
      return NS_ERROR_NOT_IMPLEMENTED;
    }

    if (format.value() != ImageBitmapFormat::YUV420P) {
      NS_WARNING("ConvertToNV12: Convert YUV data in I420 only");
      return NS_ERROR_NOT_IMPLEMENTED;
    }

    return MapRv(libyuv::I420ToNV12(
        data->mYChannel, data->mYStride, data->mCbChannel, data->mCbCrStride,
        data->mCrChannel, data->mCbCrStride, aDestY, aDestStrideY, aDestUV,
        aDestStrideUV, aImage->GetSize().width, aImage->GetSize().height));
  }

  RefPtr<SourceSurface> surf = GetSourceSurface(aImage);
  if (!surf) {
    return NS_ERROR_FAILURE;
  }

  RefPtr<DataSourceSurface> data = surf->GetDataSurface();
  if (!data) {
    return NS_ERROR_FAILURE;
  }

  DataSourceSurface::ScopedMap map(data, DataSourceSurface::READ);
  if (!map.IsMapped()) {
    return NS_ERROR_FAILURE;
  }

  if (surf->GetFormat() != SurfaceFormat::B8G8R8A8 &&
      surf->GetFormat() != SurfaceFormat::B8G8R8X8) {
    NS_WARNING("ConvertToNV12: Convert SurfaceFormat in BGR* only");
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  return MapRv(
      libyuv::ARGBToNV12(static_cast<uint8_t*>(map.GetData()), map.GetStride(),
                         aDestY, aDestStrideY, aDestUV, aDestStrideUV,
                         aImage->GetSize().width, aImage->GetSize().height));
}

static bool IsRGBX(const SurfaceFormat& aFormat) {
  return aFormat == SurfaceFormat::B8G8R8A8 ||
         aFormat == SurfaceFormat::B8G8R8X8 ||
         aFormat == SurfaceFormat::R8G8B8A8 ||
         aFormat == SurfaceFormat::R8G8B8X8 ||
         aFormat == SurfaceFormat::X8R8G8B8 ||
         aFormat == SurfaceFormat::A8R8G8B8;
}

static bool HasAlpha(const SurfaceFormat& aFormat) {
  return aFormat == SurfaceFormat::B8G8R8A8 ||
         aFormat == SurfaceFormat::R8G8B8A8 ||
         aFormat == SurfaceFormat::A8R8G8B8;
}

static nsresult SwapRGBA(DataSourceSurface* aSurface,
                         const SurfaceFormat& aDestFormat) {
  if (!aSurface || !IsRGBX(aSurface->GetFormat()) || !IsRGBX(aDestFormat)) {
    return NS_ERROR_INVALID_ARG;
  }

  if (aSurface->GetFormat() == aDestFormat) {
    return NS_OK;
  }

  DataSourceSurface::ScopedMap map(aSurface, DataSourceSurface::READ_WRITE);
  if (!map.IsMapped()) {
    return NS_ERROR_FAILURE;
  }

  gfx::SwizzleData(map.GetData(), map.GetStride(), aSurface->GetFormat(),
                   map.GetData(), map.GetStride(), aDestFormat,
                   aSurface->GetSize());

  return NS_OK;
}

nsresult ConvertToRGBA(Image* aImage, const SurfaceFormat& aDestFormat,
                       uint8_t* aDestBuffer, int aDestStride) {
  if (!aImage || !aImage->IsValid() || aImage->GetSize().IsEmpty() ||
      !aDestBuffer || !IsRGBX(aDestFormat) || aDestStride <= 0) {
    return NS_ERROR_INVALID_ARG;
  }

  // Read YUV image to the given buffer in required RGBA format.
  if (const PlanarYCbCrData* data = GetPlanarYCbCrData(aImage)) {
    SurfaceFormat convertedFormat = aDestFormat;
    gfx::PremultFunc premultOp = nullptr;
    if (data->mAlpha && HasAlpha(aDestFormat)) {
      if (aDestFormat == SurfaceFormat::A8R8G8B8) {
        convertedFormat = SurfaceFormat::B8G8R8A8;
      }
      if (data->mAlpha->mPremultiplied) {
        premultOp = libyuv::ARGBUnattenuate;
      }
    } else {
      if (aDestFormat == SurfaceFormat::X8R8G8B8 ||
          aDestFormat == SurfaceFormat::A8R8G8B8) {
        convertedFormat = SurfaceFormat::B8G8R8X8;
      }
    }

    nsresult result =
        ConvertYCbCrToRGB32(*data, convertedFormat, aDestBuffer,
                            AssertedCast<int32_t>(aDestStride), premultOp);
    if (NS_FAILED(result)) {
      return result;
    }

    if (convertedFormat == aDestFormat) {
      return NS_OK;
    }

    // Since format of the converted data returned from ConvertYCbCrToRGB or
    // ConvertYCbCrAToARGB is BRG* or RBG*, we need swap the RGBA channels to
    // the required format if needed.

    RefPtr<DataSourceSurface> surf =
        gfx::Factory::CreateWrappingDataSourceSurface(
            aDestBuffer, aDestStride, aImage->GetSize(), convertedFormat);

    if (!surf) {
      return NS_ERROR_FAILURE;
    }

    return SwapRGBA(surf.get(), aDestFormat);
  }

  // Read RGBA image to the given buffer in required RGBA format.

  RefPtr<SourceSurface> surf = GetSourceSurface(aImage);
  if (!surf) {
    return NS_ERROR_FAILURE;
  }

  if (!IsRGBX(surf->GetFormat())) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  RefPtr<DataSourceSurface> src = surf->GetDataSurface();
  if (!src) {
    return NS_ERROR_FAILURE;
  }

  DataSourceSurface::ScopedMap srcMap(src, DataSourceSurface::READ);
  if (!srcMap.IsMapped()) {
    return NS_ERROR_FAILURE;
  }

  RefPtr<DataSourceSurface> dest =
      gfx::Factory::CreateWrappingDataSourceSurface(
          aDestBuffer, aDestStride, aImage->GetSize(), aDestFormat);

  if (!dest) {
    return NS_ERROR_FAILURE;
  }

  DataSourceSurface::ScopedMap destMap(dest, gfx::DataSourceSurface::WRITE);
  if (!destMap.IsMapped()) {
    return NS_ERROR_FAILURE;
  }

  gfx::SwizzleData(srcMap.GetData(), srcMap.GetStride(), src->GetFormat(),
                   destMap.GetData(), destMap.GetStride(), dest->GetFormat(),
                   dest->GetSize());

  return NS_OK;
}

static SkColorType ToSkColorType(const SurfaceFormat& aFormat) {
  switch (aFormat) {
    case SurfaceFormat::B8G8R8A8:
    case SurfaceFormat::B8G8R8X8:
      return kBGRA_8888_SkColorType;
    case SurfaceFormat::R8G8B8A8:
    case SurfaceFormat::R8G8B8X8:
      return kRGBA_8888_SkColorType;
    default:
      break;
  }
  return kUnknown_SkColorType;
}

nsresult ConvertSRGBBufferToDisplayP3(uint8_t* aSrcBuffer,
                                      const SurfaceFormat& aSrcFormat,
                                      uint8_t* aDestBuffer, int aWidth,
                                      int aHeight) {
  if (!aSrcBuffer || !aDestBuffer || aWidth <= 0 || aHeight <= 0 ||
      !IsRGBX(aSrcFormat)) {
    return NS_ERROR_INVALID_ARG;
  }

  SkColorType srcColorType = ToSkColorType(aSrcFormat);
  if (srcColorType == kUnknown_SkColorType) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  // TODO: Provide source's color space info to customize SkColorSpace.
  auto srcColorSpace = SkColorSpace::MakeSRGB();
  SkImageInfo srcInfo = SkImageInfo::Make(aWidth, aHeight, srcColorType,
                                          kUnpremul_SkAlphaType, srcColorSpace);

  constexpr size_t bytesPerPixel = 4;
  CheckedInt<size_t> rowBytes(bytesPerPixel);
  rowBytes *= aWidth;
  if (!rowBytes.isValid()) {
    return NS_ERROR_DOM_MEDIA_OVERFLOW_ERR;
  }

  SkBitmap srcBitmap;
  if (!srcBitmap.installPixels(srcInfo, aSrcBuffer, rowBytes.value())) {
    return NS_ERROR_FAILURE;
  }

  // TODO: Provide destination's color space info to customize SkColorSpace.
  auto destColorSpace =
      SkColorSpace::MakeRGB(SkNamedTransferFn::kSRGB, SkNamedGamut::kDisplayP3);

  SkBitmap destBitmap;
  if (!destBitmap.tryAllocPixels(srcInfo.makeColorSpace(destColorSpace))) {
    return NS_ERROR_FAILURE;
  }

  if (!srcBitmap.readPixels(destBitmap.pixmap())) {
    return NS_ERROR_FAILURE;
  }

  CheckedInt<size_t> size(rowBytes.value());
  size *= aHeight;
  if (!size.isValid()) {
    return NS_ERROR_DOM_MEDIA_OVERFLOW_ERR;
  }

  PodCopy(aDestBuffer, reinterpret_cast<uint8_t*>(destBitmap.getPixels()),
          size.value());
  return NS_OK;
}

}  // namespace mozilla
