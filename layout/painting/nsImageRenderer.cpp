/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* utility code for drawing images as CSS borders, backgrounds, and shapes. */

#include "nsImageRenderer.h"

#include "mozilla/webrender/WebRenderAPI.h"

#include "gfxContext.h"
#include "gfxDrawable.h"
#include "ImageOps.h"
#include "ImageRegion.h"
#include "mozilla/layers/StackingContextHelper.h"
#include "mozilla/layers/WebRenderLayerManager.h"
#include "nsContentUtils.h"
#include "nsCSSRendering.h"
#include "nsCSSRenderingGradients.h"
#include "nsDeviceContext.h"
#include "nsIFrame.h"
#include "nsStyleStructInlines.h"
#include "nsSVGDisplayableFrame.h"
#include "SVGObserverUtils.h"
#include "nsSVGIntegrationUtils.h"
#include "mozilla/layers/WebRenderLayerManager.h"

using namespace mozilla;
using namespace mozilla::gfx;
using namespace mozilla::image;
using namespace mozilla::layers;

nsSize
CSSSizeOrRatio::ComputeConcreteSize() const
{
  NS_ASSERTION(CanComputeConcreteSize(), "Cannot compute");
  if (mHasWidth && mHasHeight) {
    return nsSize(mWidth, mHeight);
  }
  if (mHasWidth) {
    nscoord height = NSCoordSaturatingNonnegativeMultiply(
      mWidth, double(mRatio.height) / mRatio.width);
    return nsSize(mWidth, height);
  }

  MOZ_ASSERT(mHasHeight);
  nscoord width = NSCoordSaturatingNonnegativeMultiply(
    mHeight, double(mRatio.width) / mRatio.height);
  return nsSize(width, mHeight);
}

nsImageRenderer::nsImageRenderer(nsIFrame* aForFrame,
                                 const nsStyleImage* aImage,
                                 uint32_t aFlags)
  : mForFrame(aForFrame)
  , mImage(aImage)
  , mType(aImage->GetType())
  , mImageContainer(nullptr)
  , mGradientData(nullptr)
  , mPaintServerFrame(nullptr)
  , mPrepareResult(ImgDrawResult::NOT_READY)
  , mSize(0, 0)
  , mFlags(aFlags)
  , mExtendMode(ExtendMode::CLAMP)
  , mMaskOp(NS_STYLE_MASK_MODE_MATCH_SOURCE)
{
}

static bool
ShouldTreatAsCompleteDueToSyncDecode(const nsStyleImage* aImage,
                                     uint32_t aFlags)
{
  if (!(aFlags & nsImageRenderer::FLAG_SYNC_DECODE_IMAGES)) {
    return false;
  }

  if (aImage->GetType() != eStyleImageType_Image) {
    return false;
  }

  imgRequestProxy* req = aImage->GetImageData();
  if (!req) {
    return false;
  }

  uint32_t status = 0;
  if (NS_FAILED(req->GetImageStatus(&status))) {
    return false;
  }

  if (status & imgIRequest::STATUS_ERROR) {
    // The image is "complete" since it's a corrupt image. If we created an
    // imgIContainer at all, return true.
    nsCOMPtr<imgIContainer> image;
    req->GetImage(getter_AddRefs(image));
    return bool(image);
  }

  if (!(status & imgIRequest::STATUS_LOAD_COMPLETE)) {
    // We must have loaded all of the image's data and the size must be
    // available, or else sync decoding won't be able to decode the image.
    return false;
  }

  return true;
}

bool
nsImageRenderer::PrepareImage()
{
  if (mImage->IsEmpty()) {
    mPrepareResult = ImgDrawResult::BAD_IMAGE;
    return false;
  }

  if (!mImage->IsComplete()) {
    // Make sure the image is actually decoding.
    bool frameComplete = mImage->StartDecoding();

    // Check again to see if we finished.
    // We cannot prepare the image for rendering if it is not fully loaded.
    // Special case: If we requested a sync decode and the image has loaded,
    // push on through because the Draw() will do a sync decode then.
    if (!(frameComplete || mImage->IsComplete()) &&
        !ShouldTreatAsCompleteDueToSyncDecode(mImage, mFlags)) {
      mPrepareResult = ImgDrawResult::NOT_READY;
      return false;
    }
  }

  switch (mType) {
    case eStyleImageType_Image: {
      MOZ_ASSERT(mImage->GetImageData(),
                 "must have image data, since we checked IsEmpty above");
      nsCOMPtr<imgIContainer> srcImage;
      DebugOnly<nsresult> rv =
        mImage->GetImageData()->GetImage(getter_AddRefs(srcImage));
      MOZ_ASSERT(NS_SUCCEEDED(rv) && srcImage,
                 "If GetImage() is failing, mImage->IsComplete() "
                 "should have returned false");

      if (!mImage->GetCropRect()) {
        mImageContainer.swap(srcImage);
      } else {
        nsIntRect actualCropRect;
        bool isEntireImage;
        bool success =
          mImage->ComputeActualCropRect(actualCropRect, &isEntireImage);
        if (!success || actualCropRect.IsEmpty()) {
          // The cropped image has zero size
          mPrepareResult = ImgDrawResult::BAD_IMAGE;
          return false;
        }
        if (isEntireImage) {
          // The cropped image is identical to the source image
          mImageContainer.swap(srcImage);
        } else {
          nsCOMPtr<imgIContainer> subImage =
            ImageOps::Clip(srcImage, actualCropRect, Nothing());
          mImageContainer.swap(subImage);
        }
      }
      mPrepareResult = ImgDrawResult::SUCCESS;
      break;
    }
    case eStyleImageType_Gradient:
      mGradientData = mImage->GetGradientData();
      mPrepareResult = ImgDrawResult::SUCCESS;
      break;
    case eStyleImageType_Element: {
      Element* paintElement = // may be null
        SVGObserverUtils::GetAndObserveBackgroundImage(
          mForFrame->FirstContinuation(), mImage->GetElementId());
      // If the referenced element is an <img>, <canvas>, or <video> element,
      // prefer SurfaceFromElement as it's more reliable.
      mImageElementSurface = nsLayoutUtils::SurfaceFromElement(paintElement);

      if (!mImageElementSurface.GetSourceSurface()) {
        nsIFrame* paintServerFrame =
          paintElement ? paintElement->GetPrimaryFrame() : nullptr;
        // If there's no referenced frame, or the referenced frame is
        // non-displayable SVG, then we have nothing valid to paint.
        if (!paintServerFrame ||
            (paintServerFrame->IsFrameOfType(nsIFrame::eSVG) &&
             !paintServerFrame->IsFrameOfType(nsIFrame::eSVGPaintServer) &&
             !static_cast<nsSVGDisplayableFrame*>(
               do_QueryFrame(paintServerFrame)))) {
          mPrepareResult = ImgDrawResult::BAD_IMAGE;
          return false;
        }
        mPaintServerFrame = paintServerFrame;
      }

      mPrepareResult = ImgDrawResult::SUCCESS;
      break;
    }
    case eStyleImageType_Null:
    default:
      break;
  }

  return IsReady();
}

CSSSizeOrRatio
nsImageRenderer::ComputeIntrinsicSize()
{
  NS_ASSERTION(IsReady(),
               "Ensure PrepareImage() has returned true "
               "before calling me");

  CSSSizeOrRatio result;
  switch (mType) {
    case eStyleImageType_Image: {
      bool haveWidth, haveHeight;
      CSSIntSize imageIntSize;
      nsLayoutUtils::ComputeSizeForDrawing(
        mImageContainer, imageIntSize, result.mRatio, haveWidth, haveHeight);
      if (haveWidth) {
        result.SetWidth(nsPresContext::CSSPixelsToAppUnits(imageIntSize.width));
      }
      if (haveHeight) {
        result.SetHeight(
          nsPresContext::CSSPixelsToAppUnits(imageIntSize.height));
      }

      // If we know the aspect ratio and one of the dimensions,
      // we can compute the other missing width or height.
      if (!haveHeight && haveWidth && result.mRatio.width != 0) {
        nscoord intrinsicHeight = NSCoordSaturatingNonnegativeMultiply(
          imageIntSize.width,
          float(result.mRatio.height) / float(result.mRatio.width));
        result.SetHeight(nsPresContext::CSSPixelsToAppUnits(intrinsicHeight));
      } else if (haveHeight && !haveWidth && result.mRatio.height != 0) {
        nscoord intrinsicWidth = NSCoordSaturatingNonnegativeMultiply(
          imageIntSize.height,
          float(result.mRatio.width) / float(result.mRatio.height));
        result.SetWidth(nsPresContext::CSSPixelsToAppUnits(intrinsicWidth));
      }

      break;
    }
    case eStyleImageType_Element: {
      // XXX element() should have the width/height of the referenced element,
      //     and that element's ratio, if it matches.  If it doesn't match, it
      //     should have no width/height or ratio.  See element() in CSS images:
      //     <http://dev.w3.org/csswg/css-images-4/#element-notation>.
      //     Make sure to change nsStyleImageLayers::Size::DependsOnFrameSize
      //     when fixing this!
      if (mPaintServerFrame) {
        // SVG images have no intrinsic size
        if (!mPaintServerFrame->IsFrameOfType(nsIFrame::eSVG)) {
          // The intrinsic image size for a generic nsIFrame paint server is
          // the union of the border-box rects of all of its continuations,
          // rounded to device pixels.
          int32_t appUnitsPerDevPixel =
            mForFrame->PresContext()->AppUnitsPerDevPixel();
          result.SetSize(IntSizeToAppUnits(
            nsSVGIntegrationUtils::GetContinuationUnionSize(mPaintServerFrame)
              .ToNearestPixels(appUnitsPerDevPixel),
            appUnitsPerDevPixel));
        }
      } else {
        NS_ASSERTION(mImageElementSurface.GetSourceSurface(),
                     "Surface should be ready.");
        IntSize surfaceSize = mImageElementSurface.mSize;
        result.SetSize(
          nsSize(nsPresContext::CSSPixelsToAppUnits(surfaceSize.width),
                 nsPresContext::CSSPixelsToAppUnits(surfaceSize.height)));
      }
      break;
    }
    case eStyleImageType_Gradient:
      // Per <http://dev.w3.org/csswg/css3-images/#gradients>, gradients have no
      // intrinsic dimensions.
    case eStyleImageType_Null:
    default:
      break;
  }

  return result;
}

/* static */ nsSize
nsImageRenderer::ComputeConcreteSize(const CSSSizeOrRatio& aSpecifiedSize,
                                     const CSSSizeOrRatio& aIntrinsicSize,
                                     const nsSize& aDefaultSize)
{
  // The specified size is fully specified, just use that
  if (aSpecifiedSize.IsConcrete()) {
    return aSpecifiedSize.ComputeConcreteSize();
  }

  MOZ_ASSERT(!aSpecifiedSize.mHasWidth || !aSpecifiedSize.mHasHeight);

  if (!aSpecifiedSize.mHasWidth && !aSpecifiedSize.mHasHeight) {
    // no specified size, try using the intrinsic size
    if (aIntrinsicSize.CanComputeConcreteSize()) {
      return aIntrinsicSize.ComputeConcreteSize();
    }

    if (aIntrinsicSize.mHasWidth) {
      return nsSize(aIntrinsicSize.mWidth, aDefaultSize.height);
    }
    if (aIntrinsicSize.mHasHeight) {
      return nsSize(aDefaultSize.width, aIntrinsicSize.mHeight);
    }

    // couldn't use the intrinsic size either, revert to using the default size
    return ComputeConstrainedSize(aDefaultSize, aIntrinsicSize.mRatio, CONTAIN);
  }

  MOZ_ASSERT(aSpecifiedSize.mHasWidth || aSpecifiedSize.mHasHeight);

  // The specified height is partial, try to compute the missing part.
  if (aSpecifiedSize.mHasWidth) {
    nscoord height;
    if (aIntrinsicSize.HasRatio()) {
      height = NSCoordSaturatingNonnegativeMultiply(
        aSpecifiedSize.mWidth,
        double(aIntrinsicSize.mRatio.height) / aIntrinsicSize.mRatio.width);
    } else if (aIntrinsicSize.mHasHeight) {
      height = aIntrinsicSize.mHeight;
    } else {
      height = aDefaultSize.height;
    }
    return nsSize(aSpecifiedSize.mWidth, height);
  }

  MOZ_ASSERT(aSpecifiedSize.mHasHeight);
  nscoord width;
  if (aIntrinsicSize.HasRatio()) {
    width = NSCoordSaturatingNonnegativeMultiply(
      aSpecifiedSize.mHeight,
      double(aIntrinsicSize.mRatio.width) / aIntrinsicSize.mRatio.height);
  } else if (aIntrinsicSize.mHasWidth) {
    width = aIntrinsicSize.mWidth;
  } else {
    width = aDefaultSize.width;
  }
  return nsSize(width, aSpecifiedSize.mHeight);
}

/* static */ nsSize
nsImageRenderer::ComputeConstrainedSize(const nsSize& aConstrainingSize,
                                        const nsSize& aIntrinsicRatio,
                                        FitType aFitType)
{
  if (aIntrinsicRatio.width <= 0 && aIntrinsicRatio.height <= 0) {
    return aConstrainingSize;
  }

  float scaleX = double(aConstrainingSize.width) / aIntrinsicRatio.width;
  float scaleY = double(aConstrainingSize.height) / aIntrinsicRatio.height;
  nsSize size;
  if ((aFitType == CONTAIN) == (scaleX < scaleY)) {
    size.width = aConstrainingSize.width;
    size.height =
      NSCoordSaturatingNonnegativeMultiply(aIntrinsicRatio.height, scaleX);
    // If we're reducing the size by less than one css pixel, then just use the
    // constraining size.
    if (aFitType == CONTAIN &&
        aConstrainingSize.height - size.height < AppUnitsPerCSSPixel()) {
      size.height = aConstrainingSize.height;
    }
  } else {
    size.width =
      NSCoordSaturatingNonnegativeMultiply(aIntrinsicRatio.width, scaleY);
    if (aFitType == CONTAIN &&
        aConstrainingSize.width - size.width < AppUnitsPerCSSPixel()) {
      size.width = aConstrainingSize.width;
    }
    size.height = aConstrainingSize.height;
  }
  return size;
}

/**
 * mSize is the image's "preferred" size for this particular rendering, while
 * the drawn (aka concrete) size is the actual rendered size after accounting
 * for background-size etc..  The preferred size is most often the image's
 * intrinsic dimensions.  But for images with incomplete intrinsic dimensions,
 * the preferred size varies, depending on the specified and default sizes, see
 * nsImageRenderer::Compute*Size.
 *
 * This distinction is necessary because the components of a vector image are
 * specified with respect to its preferred size for a rendering situation, not
 * to its actual rendered size.  For example, consider a 4px wide background
 * vector image with no height which contains a left-aligned
 * 2px wide black rectangle with height 100%.  If the background-size width is
 * auto (or 4px), the vector image will render 4px wide, and the black rectangle
 * will be 2px wide.  If the background-size width is 8px, the vector image will
 * render 8px wide, and the black rectangle will be 4px wide -- *not* 2px wide.
 * In both cases mSize.width will be 4px; but in the first case the returned
 * width will be 4px, while in the second case the returned width will be 8px.
 */
void
nsImageRenderer::SetPreferredSize(const CSSSizeOrRatio& aIntrinsicSize,
                                  const nsSize& aDefaultSize)
{
  mSize.width =
    aIntrinsicSize.mHasWidth ? aIntrinsicSize.mWidth : aDefaultSize.width;
  mSize.height =
    aIntrinsicSize.mHasHeight ? aIntrinsicSize.mHeight : aDefaultSize.height;
}

// Convert from nsImageRenderer flags to the flags we want to use for drawing in
// the imgIContainer namespace.
static uint32_t
ConvertImageRendererToDrawFlags(uint32_t aImageRendererFlags)
{
  uint32_t drawFlags = imgIContainer::FLAG_NONE;
  if (aImageRendererFlags & nsImageRenderer::FLAG_SYNC_DECODE_IMAGES) {
    drawFlags |= imgIContainer::FLAG_SYNC_DECODE;
  }
  if (aImageRendererFlags & nsImageRenderer::FLAG_PAINTING_TO_WINDOW) {
    drawFlags |= imgIContainer::FLAG_HIGH_QUALITY_SCALING;
  }
  return drawFlags;
}

ImgDrawResult
nsImageRenderer::Draw(nsPresContext* aPresContext,
                      gfxContext& aRenderingContext,
                      const nsRect& aDirtyRect,
                      const nsRect& aDest,
                      const nsRect& aFill,
                      const nsPoint& aAnchor,
                      const nsSize& aRepeatSize,
                      const CSSIntRect& aSrc,
                      float aOpacity)
{
  if (!IsReady()) {
    MOZ_ASSERT_UNREACHABLE("Ensure PrepareImage() has returned true before "
                           "calling me");
    return ImgDrawResult::TEMPORARY_ERROR;
  }

  if (aDest.IsEmpty() || aFill.IsEmpty() || mSize.width <= 0 ||
      mSize.height <= 0) {
    return ImgDrawResult::SUCCESS;
  }

  SamplingFilter samplingFilter =
    nsLayoutUtils::GetSamplingFilterForFrame(mForFrame);
  ImgDrawResult result = ImgDrawResult::SUCCESS;
  RefPtr<gfxContext> ctx = &aRenderingContext;
  IntRect tmpDTRect;

  if (ctx->CurrentOp() != CompositionOp::OP_OVER ||
      mMaskOp == NS_STYLE_MASK_MODE_LUMINANCE) {
    gfxRect clipRect = ctx->GetClipExtents(gfxContext::eDeviceSpace);
    tmpDTRect = RoundedOut(ToRect(clipRect));
    if (tmpDTRect.IsEmpty()) {
      return ImgDrawResult::SUCCESS;
    }
    RefPtr<DrawTarget> tempDT =
      gfxPlatform::GetPlatform()->CreateSimilarSoftwareDrawTarget(
        ctx->GetDrawTarget(), tmpDTRect.Size(), SurfaceFormat::B8G8R8A8);
    if (!tempDT || !tempDT->IsValid()) {
      gfxDevCrash(LogReason::InvalidContext)
        << "ImageRenderer::Draw problem " << gfx::hexa(tempDT);
      return ImgDrawResult::TEMPORARY_ERROR;
    }
    tempDT->SetTransform(ctx->GetDrawTarget()->GetTransform() *
                         Matrix::Translation(-tmpDTRect.TopLeft()));
    ctx = gfxContext::CreatePreservingTransformOrNull(tempDT);
    if (!ctx) {
      gfxDevCrash(LogReason::InvalidContext)
        << "ImageRenderer::Draw problem " << gfx::hexa(tempDT);
      return ImgDrawResult::TEMPORARY_ERROR;
    }
  }

  switch (mType) {
    case eStyleImageType_Image: {
      CSSIntSize imageSize(nsPresContext::AppUnitsToIntCSSPixels(mSize.width),
                           nsPresContext::AppUnitsToIntCSSPixels(mSize.height));
      result = nsLayoutUtils::DrawBackgroundImage(
        *ctx,
        mForFrame,
        aPresContext,
        mImageContainer,
        imageSize,
        samplingFilter,
        aDest,
        aFill,
        aRepeatSize,
        aAnchor,
        aDirtyRect,
        ConvertImageRendererToDrawFlags(mFlags),
        mExtendMode,
        aOpacity);
      break;
    }
    case eStyleImageType_Gradient: {
      nsCSSGradientRenderer renderer = nsCSSGradientRenderer::Create(
        aPresContext, mForFrame->Style(), mGradientData, mSize);

      renderer.Paint(
        *ctx, aDest, aFill, aRepeatSize, aSrc, aDirtyRect, aOpacity);
      break;
    }
    case eStyleImageType_Element: {
      RefPtr<gfxDrawable> drawable = DrawableForElement(aDest, *ctx);
      if (!drawable) {
        NS_WARNING("Could not create drawable for element");
        return ImgDrawResult::TEMPORARY_ERROR;
      }

      nsCOMPtr<imgIContainer> image(ImageOps::CreateFromDrawable(drawable));
      result = nsLayoutUtils::DrawImage(*ctx,
                                        mForFrame->Style(),
                                        aPresContext,
                                        image,
                                        samplingFilter,
                                        aDest,
                                        aFill,
                                        aAnchor,
                                        aDirtyRect,
                                        ConvertImageRendererToDrawFlags(mFlags),
                                        aOpacity);
      break;
    }
    case eStyleImageType_Null:
    default:
      break;
  }

  if (!tmpDTRect.IsEmpty()) {
    DrawTarget* dt = aRenderingContext.GetDrawTarget();
    Matrix oldTransform = dt->GetTransform();
    dt->SetTransform(Matrix());
    if (mMaskOp == NS_STYLE_MASK_MODE_LUMINANCE) {
      RefPtr<SourceSurface> surf = ctx->GetDrawTarget()->IntoLuminanceSource(
        LuminanceType::LUMINANCE, 1.0f);
      dt->MaskSurface(ColorPattern(Color(0, 0, 0, 1.0f)),
                      surf,
                      tmpDTRect.TopLeft(),
                      DrawOptions(1.0f, aRenderingContext.CurrentOp()));
    } else {
      RefPtr<SourceSurface> surf = ctx->GetDrawTarget()->Snapshot();
      dt->DrawSurface(
        surf,
        Rect(tmpDTRect.x, tmpDTRect.y, tmpDTRect.width, tmpDTRect.height),
        Rect(0, 0, tmpDTRect.width, tmpDTRect.height),
        DrawSurfaceOptions(SamplingFilter::POINT),
        DrawOptions(1.0f, aRenderingContext.CurrentOp()));
    }

    dt->SetTransform(oldTransform);
  }

  if (!mImage->IsComplete()) {
    result &= ImgDrawResult::SUCCESS_NOT_COMPLETE;
  }

  return result;
}

ImgDrawResult
nsImageRenderer::BuildWebRenderDisplayItems(
  nsPresContext* aPresContext,
  mozilla::wr::DisplayListBuilder& aBuilder,
  mozilla::wr::IpcResourceUpdateQueue& aResources,
  const mozilla::layers::StackingContextHelper& aSc,
  mozilla::layers::WebRenderLayerManager* aManager,
  nsDisplayItem* aItem,
  const nsRect& aDirtyRect,
  const nsRect& aDest,
  const nsRect& aFill,
  const nsPoint& aAnchor,
  const nsSize& aRepeatSize,
  const CSSIntRect& aSrc,
  float aOpacity)
{
  if (!IsReady()) {
    MOZ_ASSERT_UNREACHABLE("Ensure PrepareImage() has returned true before "
                           "calling me");
    return ImgDrawResult::NOT_READY;
  }

  if (aDest.IsEmpty() || aFill.IsEmpty() || mSize.width <= 0 ||
      mSize.height <= 0) {
    return ImgDrawResult::SUCCESS;
  }

  ImgDrawResult drawResult = ImgDrawResult::SUCCESS;
  switch (mType) {
    case eStyleImageType_Gradient: {
      nsCSSGradientRenderer renderer = nsCSSGradientRenderer::Create(
        aPresContext, mForFrame->Style(), mGradientData, mSize);

      renderer.BuildWebRenderDisplayItems(aBuilder,
                                          aSc,
                                          aDest,
                                          aFill,
                                          aRepeatSize,
                                          aSrc,
                                          !aItem->BackfaceIsHidden(),
                                          aOpacity);
      break;
    }
    case eStyleImageType_Image: {
      uint32_t containerFlags = imgIContainer::FLAG_ASYNC_NOTIFY;
      if (mFlags & nsImageRenderer::FLAG_PAINTING_TO_WINDOW) {
        containerFlags |= imgIContainer::FLAG_HIGH_QUALITY_SCALING;
      }
      if (mFlags & nsImageRenderer::FLAG_SYNC_DECODE_IMAGES) {
        containerFlags |= imgIContainer::FLAG_SYNC_DECODE;
      }

      CSSIntSize imageSize(nsPresContext::AppUnitsToIntCSSPixels(mSize.width),
                           nsPresContext::AppUnitsToIntCSSPixels(mSize.height));
      Maybe<SVGImageContext> svgContext(Some(SVGImageContext(Some(imageSize))));

      const int32_t appUnitsPerDevPixel =
        mForFrame->PresContext()->AppUnitsPerDevPixel();
      LayoutDeviceRect destRect =
        LayoutDeviceRect::FromAppUnits(aDest, appUnitsPerDevPixel);
      gfx::IntSize decodeSize =
        nsLayoutUtils::ComputeImageContainerDrawingParameters(mImageContainer,
                                                              mForFrame,
                                                              destRect,
                                                              aSc,
                                                              containerFlags,
                                                              svgContext);

      RefPtr<layers::ImageContainer> container;
      drawResult = mImageContainer->GetImageContainerAtSize(aManager, decodeSize, svgContext,
                                                            containerFlags, getter_AddRefs(container));
      if (!container) {
        NS_WARNING("Failed to get image container");
        break;
      }

      mozilla::wr::ImageRendering rendering = wr::ToImageRendering(
        nsLayoutUtils::GetSamplingFilterForFrame(aItem->Frame()));
      gfx::IntSize size;
      Maybe<wr::ImageKey> key =
        aManager->CommandBuilder().CreateImageKey(aItem,
                                                  container,
                                                  aBuilder,
                                                  aResources,
                                                  rendering,
                                                  aSc,
                                                  size,
                                                  Nothing());

      if (key.isNothing()) {
        break;
      }

      nsPoint firstTilePos = nsLayoutUtils::GetBackgroundFirstTilePos(
        aDest.TopLeft(), aFill.TopLeft(), aRepeatSize);
      LayoutDeviceRect fillRect =
        LayoutDeviceRect::FromAppUnits(nsRect(firstTilePos.x,
                                              firstTilePos.y,
                                              aFill.XMost() - firstTilePos.x,
                                              aFill.YMost() - firstTilePos.y),
                                       appUnitsPerDevPixel);
      wr::LayoutRect fill = wr::ToRoundedLayoutRect(fillRect);

      wr::LayoutRect roundedDest = wr::ToRoundedLayoutRect(destRect);
      auto stretchSize = wr::ToLayoutSize(destRect.Size());

      // WebRender special cases situations where stretchSize == fillSize to
      // infer that it shouldn't use repeat sampling. This makes sure
      // we hit those special cases when not repeating.

      switch (mExtendMode) {
        case ExtendMode::CLAMP:
          fill = roundedDest;
          stretchSize = roundedDest.size;
          break;
        case ExtendMode::REPEAT_Y:
          fill.origin.x = roundedDest.origin.x;
          fill.size.width = roundedDest.size.width;
          stretchSize.width = roundedDest.size.width;
          break;
        case ExtendMode::REPEAT_X:
          fill.origin.y = roundedDest.origin.y;
          fill.size.height = roundedDest.size.height;
          stretchSize.height = roundedDest.size.height;
          break;
        default:
          break;
      }

      wr::LayoutRect clip = wr::ToRoundedLayoutRect(
        LayoutDeviceRect::FromAppUnits(aFill, appUnitsPerDevPixel));

      LayoutDeviceSize gapSize = LayoutDeviceSize::FromAppUnits(
        aRepeatSize - aDest.Size(), appUnitsPerDevPixel);

      aBuilder.PushImage(fill,
                         clip,
                         !aItem->BackfaceIsHidden(),
                         stretchSize,
                         wr::ToLayoutSize(gapSize),
                         rendering,
                         key.value());
      break;
    }
    default:
      break;
  }

  if (!mImage->IsComplete() && drawResult == ImgDrawResult::SUCCESS) {
    return ImgDrawResult::SUCCESS_NOT_COMPLETE;
  }
  return drawResult;
}

already_AddRefed<gfxDrawable>
nsImageRenderer::DrawableForElement(const nsRect& aImageRect,
                                    gfxContext& aContext)
{
  NS_ASSERTION(mType == eStyleImageType_Element,
               "DrawableForElement only makes sense if backed by an element");
  if (mPaintServerFrame) {
    // XXX(seth): In order to not pass FLAG_SYNC_DECODE_IMAGES here,
    // DrawableFromPaintServer would have to return a ImgDrawResult indicating
    // whether any images could not be painted because they weren't fully
    // decoded. Even always passing FLAG_SYNC_DECODE_IMAGES won't eliminate all
    // problems, as it won't help if there are image which haven't finished
    // loading, but it's better than nothing.
    int32_t appUnitsPerDevPixel =
      mForFrame->PresContext()->AppUnitsPerDevPixel();
    nsRect destRect = aImageRect - aImageRect.TopLeft();
    nsIntSize roundedOut = destRect.ToOutsidePixels(appUnitsPerDevPixel).Size();
    IntSize imageSize(roundedOut.width, roundedOut.height);
    RefPtr<gfxDrawable> drawable =
      nsSVGIntegrationUtils::DrawableFromPaintServer(
        mPaintServerFrame,
        mForFrame,
        mSize,
        imageSize,
        aContext.GetDrawTarget(),
        aContext.CurrentMatrixDouble(),
        nsSVGIntegrationUtils::FLAG_SYNC_DECODE_IMAGES);

    return drawable.forget();
  }
  NS_ASSERTION(mImageElementSurface.GetSourceSurface(),
               "Surface should be ready.");
  RefPtr<gfxDrawable> drawable = new gfxSurfaceDrawable(
    mImageElementSurface.GetSourceSurface().get(), mImageElementSurface.mSize);
  return drawable.forget();
}

ImgDrawResult
nsImageRenderer::DrawLayer(nsPresContext* aPresContext,
                           gfxContext& aRenderingContext,
                           const nsRect& aDest,
                           const nsRect& aFill,
                           const nsPoint& aAnchor,
                           const nsRect& aDirty,
                           const nsSize& aRepeatSize,
                           float aOpacity)
{
  if (!IsReady()) {
    MOZ_ASSERT_UNREACHABLE("Ensure PrepareImage() has returned true before "
                           "calling me");
    return ImgDrawResult::TEMPORARY_ERROR;
  }

  if (aDest.IsEmpty() || aFill.IsEmpty() || mSize.width <= 0 ||
      mSize.height <= 0) {
    return ImgDrawResult::SUCCESS;
  }

  return Draw(aPresContext,
              aRenderingContext,
              aDirty,
              aDest,
              aFill,
              aAnchor,
              aRepeatSize,
              CSSIntRect(0,
                         0,
                         nsPresContext::AppUnitsToIntCSSPixels(mSize.width),
                         nsPresContext::AppUnitsToIntCSSPixels(mSize.height)),
              aOpacity);
}

ImgDrawResult
nsImageRenderer::BuildWebRenderDisplayItemsForLayer(
  nsPresContext* aPresContext,
  mozilla::wr::DisplayListBuilder& aBuilder,
  mozilla::wr::IpcResourceUpdateQueue& aResources,
  const mozilla::layers::StackingContextHelper& aSc,
  mozilla::layers::WebRenderLayerManager* aManager,
  nsDisplayItem* aItem,
  const nsRect& aDest,
  const nsRect& aFill,
  const nsPoint& aAnchor,
  const nsRect& aDirty,
  const nsSize& aRepeatSize,
  float aOpacity)
{
  if (!IsReady()) {
    MOZ_ASSERT_UNREACHABLE("Ensure PrepareImage() has returned true before "
                           "calling me");
    return mPrepareResult;
  }

  if (aDest.IsEmpty() || aFill.IsEmpty() || mSize.width <= 0 ||
      mSize.height <= 0) {
    return ImgDrawResult::SUCCESS;
  }
  return BuildWebRenderDisplayItems(
    aPresContext,
    aBuilder,
    aResources,
    aSc,
    aManager,
    aItem,
    aDirty,
    aDest,
    aFill,
    aAnchor,
    aRepeatSize,
    CSSIntRect(0,
               0,
               nsPresContext::AppUnitsToIntCSSPixels(mSize.width),
               nsPresContext::AppUnitsToIntCSSPixels(mSize.height)),
    aOpacity);
}

/**
 * Compute the size and position of the master copy of the image. I.e., a single
 * tile used to fill the dest rect.
 * aFill The destination rect to be filled
 * aHFill and aVFill are the repeat patterns for the component -
 * StyleBorderImageRepeat - i.e., how a tiling unit is used to fill aFill
 * aUnitSize The size of the source rect in dest coords.
 */
static nsRect
ComputeTile(nsRect& aFill,
            StyleBorderImageRepeat aHFill,
            StyleBorderImageRepeat aVFill,
            const nsSize& aUnitSize,
            nsSize& aRepeatSize)
{
  nsRect tile;
  switch (aHFill) {
    case StyleBorderImageRepeat::Stretch:
      tile.x = aFill.x;
      tile.width = aFill.width;
      aRepeatSize.width = tile.width;
      break;
    case StyleBorderImageRepeat::Repeat:
      tile.x = aFill.x + aFill.width / 2 - aUnitSize.width / 2;
      tile.width = aUnitSize.width;
      aRepeatSize.width = tile.width;
      break;
    case StyleBorderImageRepeat::Round:
      tile.x = aFill.x;
      tile.width =
        nsCSSRendering::ComputeRoundedSize(aUnitSize.width, aFill.width);
      aRepeatSize.width = tile.width;
      break;
    case StyleBorderImageRepeat::Space: {
      nscoord space;
      aRepeatSize.width = nsCSSRendering::ComputeBorderSpacedRepeatSize(
        aUnitSize.width, aFill.width, space);
      tile.x = aFill.x + space;
      tile.width = aUnitSize.width;
      aFill.x = tile.x;
      aFill.width = aFill.width - space * 2;
    } break;
    default:
      MOZ_ASSERT_UNREACHABLE("unrecognized border-image fill style");
  }

  switch (aVFill) {
    case StyleBorderImageRepeat::Stretch:
      tile.y = aFill.y;
      tile.height = aFill.height;
      aRepeatSize.height = tile.height;
      break;
    case StyleBorderImageRepeat::Repeat:
      tile.y = aFill.y + aFill.height / 2 - aUnitSize.height / 2;
      tile.height = aUnitSize.height;
      aRepeatSize.height = tile.height;
      break;
    case StyleBorderImageRepeat::Round:
      tile.y = aFill.y;
      tile.height =
        nsCSSRendering::ComputeRoundedSize(aUnitSize.height, aFill.height);
      aRepeatSize.height = tile.height;
      break;
    case StyleBorderImageRepeat::Space: {
      nscoord space;
      aRepeatSize.height = nsCSSRendering::ComputeBorderSpacedRepeatSize(
        aUnitSize.height, aFill.height, space);
      tile.y = aFill.y + space;
      tile.height = aUnitSize.height;
      aFill.y = tile.y;
      aFill.height = aFill.height - space * 2;
    } break;
    default:
      MOZ_ASSERT_UNREACHABLE("unrecognized border-image fill style");
  }

  return tile;
}

/**
 * Returns true if the given set of arguments will require the tiles which fill
 * the dest rect to be scaled from the source tile. See comment on ComputeTile
 * for argument descriptions.
 */
static bool
RequiresScaling(const nsRect& aFill,
                StyleBorderImageRepeat aHFill,
                StyleBorderImageRepeat aVFill,
                const nsSize& aUnitSize)
{
  // If we have no tiling in either direction, we can skip the intermediate
  // scaling step.
  return (aHFill != StyleBorderImageRepeat::Stretch ||
          aVFill != StyleBorderImageRepeat::Stretch) &&
         (aUnitSize.width != aFill.width || aUnitSize.height != aFill.height);
}

ImgDrawResult
nsImageRenderer::DrawBorderImageComponent(nsPresContext* aPresContext,
                                          gfxContext& aRenderingContext,
                                          const nsRect& aDirtyRect,
                                          const nsRect& aFill,
                                          const CSSIntRect& aSrc,
                                          StyleBorderImageRepeat aHFill,
                                          StyleBorderImageRepeat aVFill,
                                          const nsSize& aUnitSize,
                                          uint8_t aIndex,
                                          const Maybe<nsSize>& aSVGViewportSize,
                                          const bool aHasIntrinsicRatio)
{
  if (!IsReady()) {
    MOZ_ASSERT_UNREACHABLE("Ensure PrepareImage() has returned true before "
                           "calling me");
    return ImgDrawResult::BAD_ARGS;
  }

  if (aFill.IsEmpty() || aSrc.IsEmpty()) {
    return ImgDrawResult::SUCCESS;
  }

  if (mType == eStyleImageType_Image || mType == eStyleImageType_Element) {
    nsCOMPtr<imgIContainer> subImage;

    // To draw one portion of an image into a border component, we stretch that
    // portion to match the size of that border component and then draw onto.
    // However, preserveAspectRatio attribute of a SVG image may break this
    // rule. To get correct rendering result, we add
    // FLAG_FORCE_PRESERVEASPECTRATIO_NONE flag here, to tell mImage to ignore
    // preserveAspectRatio attribute, and always do non-uniform stretch.
    uint32_t drawFlags = ConvertImageRendererToDrawFlags(mFlags) |
                         imgIContainer::FLAG_FORCE_PRESERVEASPECTRATIO_NONE;
    // For those SVG image sources which don't have fixed aspect ratio (i.e.
    // without viewport size and viewBox), we should scale the source uniformly
    // after the viewport size is decided by "Default Sizing Algorithm".
    if (!aHasIntrinsicRatio) {
      drawFlags = drawFlags | imgIContainer::FLAG_FORCE_UNIFORM_SCALING;
    }
    // Retrieve or create the subimage we'll draw.
    nsIntRect srcRect(aSrc.x, aSrc.y, aSrc.width, aSrc.height);
    if (mType == eStyleImageType_Image) {
      if ((subImage = mImage->GetSubImage(aIndex)) == nullptr) {
        subImage = ImageOps::Clip(mImageContainer, srcRect, aSVGViewportSize);
        mImage->SetSubImage(aIndex, subImage);
      }
    } else {
      // This path, for eStyleImageType_Element, is currently slower than it
      // needs to be because we don't cache anything. (In particular, if we have
      // to draw to a temporary surface inside ClippedImage, we don't cache that
      // temporary surface since we immediately throw the ClippedImage we create
      // here away.) However, if we did cache, we'd need to know when to
      // invalidate that cache, and it's not clear that it's worth the trouble
      // since using border-image with -moz-element is rare.

      RefPtr<gfxDrawable> drawable =
        DrawableForElement(nsRect(nsPoint(), mSize), aRenderingContext);
      if (!drawable) {
        NS_WARNING("Could not create drawable for element");
        return ImgDrawResult::TEMPORARY_ERROR;
      }

      nsCOMPtr<imgIContainer> image(ImageOps::CreateFromDrawable(drawable));
      subImage = ImageOps::Clip(image, srcRect, aSVGViewportSize);
    }

    MOZ_ASSERT(!aSVGViewportSize ||
               subImage->GetType() == imgIContainer::TYPE_VECTOR);

    SamplingFilter samplingFilter =
      nsLayoutUtils::GetSamplingFilterForFrame(mForFrame);

    if (!RequiresScaling(aFill, aHFill, aVFill, aUnitSize)) {
      ImgDrawResult result =
        nsLayoutUtils::DrawSingleImage(aRenderingContext,
                                       aPresContext,
                                       subImage,
                                       samplingFilter,
                                       aFill,
                                       aDirtyRect,
                                       /* no SVGImageContext */ Nothing(),
                                       drawFlags);

      if (!mImage->IsComplete()) {
        result &= ImgDrawResult::SUCCESS_NOT_COMPLETE;
      }

      return result;
    }

    nsSize repeatSize;
    nsRect fillRect(aFill);
    nsRect tile = ComputeTile(fillRect, aHFill, aVFill, aUnitSize, repeatSize);
    CSSIntSize imageSize(srcRect.width, srcRect.height);

    ImgDrawResult result = nsLayoutUtils::DrawBackgroundImage(aRenderingContext,
                                                              mForFrame,
                                                              aPresContext,
                                                              subImage,
                                                              imageSize,
                                                              samplingFilter,
                                                              tile,
                                                              fillRect,
                                                              repeatSize,
                                                              tile.TopLeft(),
                                                              aDirtyRect,
                                                              drawFlags,
                                                              ExtendMode::CLAMP,
                                                              1.0);

    if (!mImage->IsComplete()) {
      result &= ImgDrawResult::SUCCESS_NOT_COMPLETE;
    }

    return result;
  }

  nsSize repeatSize(aFill.Size());
  nsRect fillRect(aFill);
  nsRect destTile =
    RequiresScaling(fillRect, aHFill, aVFill, aUnitSize)
      ? ComputeTile(fillRect, aHFill, aVFill, aUnitSize, repeatSize)
      : fillRect;

  return Draw(aPresContext,
              aRenderingContext,
              aDirtyRect,
              destTile,
              fillRect,
              destTile.TopLeft(),
              repeatSize,
              aSrc);
}

ImgDrawResult
nsImageRenderer::DrawShapeImage(nsPresContext* aPresContext,
                                gfxContext& aRenderingContext)
{
  if (!IsReady()) {
    MOZ_ASSERT_UNREACHABLE("Ensure PrepareImage() has returned true before "
                           "calling me");
    return ImgDrawResult::NOT_READY;
  }

  if (mSize.width <= 0 || mSize.height <= 0) {
    return ImgDrawResult::SUCCESS;
  }

  ImgDrawResult result = ImgDrawResult::SUCCESS;

  switch (mType) {
    case eStyleImageType_Image: {
      uint32_t drawFlags =
        ConvertImageRendererToDrawFlags(mFlags) | imgIContainer::FRAME_FIRST;
      nsRect dest(nsPoint(0, 0), mSize);
      // We have a tricky situation in our choice of SamplingFilter. Shape
      // images define a float area based on the alpha values in the rendered
      // pixels. When multiple device pixels are used for one css pixel, the
      // sampling can change crisp edges into aliased edges. For visual pixels,
      // that's usually the right choice. For defining a float area, it can
      // cause problems. If a style is using a shape-image-threshold value that
      // is less than the alpha of the edge pixels, any filtering may smear the
      // alpha into adjacent pixels and expand the float area in a confusing
      // way. Since the alpha threshold can be set precisely in CSS, and since a
      // web author may be counting on that threshold to define a precise float
      // area from an image, it is least confusing to have the rendered pixels
      // have unfiltered alpha. We use SamplingFilter::POINT to ensure that each
      // rendered pixel has an alpha that precisely matches the alpha of the
      // closest pixel in the image.
      result = nsLayoutUtils::DrawSingleImage(aRenderingContext,
                                              aPresContext,
                                              mImageContainer,
                                              SamplingFilter::POINT,
                                              dest,
                                              dest,
                                              Nothing(),
                                              drawFlags,
                                              nullptr,
                                              nullptr);
      break;
    }

    case eStyleImageType_Gradient: {
      nsCSSGradientRenderer renderer = nsCSSGradientRenderer::Create(
        aPresContext, mForFrame->Style(), mGradientData, mSize);
      nsRect dest(nsPoint(0, 0), mSize);

      renderer.Paint(aRenderingContext,
                     dest,
                     dest,
                     mSize,
                     CSSIntRect::FromAppUnitsRounded(dest),
                     dest,
                     1.0);
      break;
    }

    default:
      // Unsupported image type.
      result = ImgDrawResult::BAD_IMAGE;
      break;
  }

  return result;
}

bool
nsImageRenderer::IsRasterImage()
{
  if (mType != eStyleImageType_Image || !mImageContainer)
    return false;
  return mImageContainer->GetType() == imgIContainer::TYPE_RASTER;
}

bool
nsImageRenderer::IsAnimatedImage()
{
  if (mType != eStyleImageType_Image || !mImageContainer)
    return false;
  bool animated = false;
  if (NS_SUCCEEDED(mImageContainer->GetAnimated(&animated)) && animated)
    return true;

  return false;
}

already_AddRefed<imgIContainer>
nsImageRenderer::GetImage()
{
  if (mType != eStyleImageType_Image || !mImageContainer) {
    return nullptr;
  }

  nsCOMPtr<imgIContainer> image = mImageContainer;
  return image.forget();
}

bool
nsImageRenderer::IsImageContainerAvailable(layers::LayerManager* aManager,
                                           uint32_t aFlags)
{
  if (!mImageContainer) {
    return false;
  }
  return mImageContainer->IsImageContainerAvailable(aManager, aFlags);
}

void
nsImageRenderer::PurgeCacheForViewportChange(
  const Maybe<nsSize>& aSVGViewportSize,
  const bool aHasIntrinsicRatio)
{
  // Check if we should flush the cached data - only vector images need to do
  // the check since they might not have fixed ratio.
  if (mImageContainer &&
      mImageContainer->GetType() == imgIContainer::TYPE_VECTOR) {
    mImage->PurgeCacheForViewportChange(aSVGViewportSize, aHasIntrinsicRatio);
  }
}

already_AddRefed<nsStyleGradient>
nsImageRenderer::GetGradientData()
{
  RefPtr<nsStyleGradient> res = mGradientData;
  return res.forget();
}
