/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Main header first:
#include "FilterInstance.h"

// MFBT headers next:
#include "mozilla/UniquePtr.h"

// Keep others in (case-insensitive) order:
#include "FilterSupport.h"
#include "ImgDrawResult.h"
#include "SVGContentUtils.h"
#include "gfx2DGlue.h"
#include "gfxContext.h"
#include "gfxPlatform.h"

#include "gfxUtils.h"
#include "mozilla/Unused.h"
#include "mozilla/gfx/Filters.h"
#include "mozilla/gfx/Helpers.h"
#include "mozilla/gfx/Logging.h"
#include "mozilla/gfx/PatternHelpers.h"
#include "mozilla/ISVGDisplayableFrame.h"
#include "mozilla/StaticPrefs_gfx.h"
#include "mozilla/SVGFilterInstance.h"
#include "mozilla/SVGObserverUtils.h"
#include "mozilla/SVGUtils.h"
#include "mozilla/dom/Document.h"
#include "nsLayoutUtils.h"
#include "CSSFilterInstance.h"
#include "SVGIntegrationUtils.h"

using namespace mozilla::dom;
using namespace mozilla::gfx;
using namespace mozilla::image;

namespace mozilla {

FilterDescription FilterInstance::GetFilterDescription(
    nsIContent* aFilteredElement, Span<const StyleFilter> aFilterChain,
    nsISupports* aFiltersObserverList, bool aFilterInputIsTainted,
    const UserSpaceMetrics& aMetrics, const gfxRect& aBBox,
    nsTArray<RefPtr<SourceSurface>>& aOutAdditionalImages) {
  gfxMatrix identity;

  nsTArray<SVGFilterFrame*> filterFrames;
  if (SVGObserverUtils::GetAndObserveFilters(aFiltersObserverList,
                                             &filterFrames) ==
      SVGObserverUtils::eHasRefsSomeInvalid) {
    return FilterDescription();
  }

  FilterInstance instance(nullptr, aFilteredElement, aMetrics, aFilterChain,
                          filterFrames, aFilterInputIsTainted, nullptr,
                          identity, nullptr, nullptr, nullptr, &aBBox);
  if (!instance.IsInitialized()) {
    return FilterDescription();
  }
  return instance.ExtractDescriptionAndAdditionalImages(aOutAdditionalImages);
}

static UniquePtr<UserSpaceMetrics> UserSpaceMetricsForFrame(nsIFrame* aFrame) {
  if (auto* element = SVGElement::FromNodeOrNull(aFrame->GetContent())) {
    return MakeUnique<SVGElementMetrics>(element);
  }
  return MakeUnique<NonSVGFrameUserSpaceMetrics>(aFrame);
}

void FilterInstance::PaintFilteredFrame(
    nsIFrame* aFilteredFrame, Span<const StyleFilter> aFilterChain,
    const nsTArray<SVGFilterFrame*>& aFilterFrames, gfxContext* aCtx,
    const SVGFilterPaintCallback& aPaintCallback, const nsRegion* aDirtyArea,
    imgDrawingParams& aImgParams, float aOpacity,
    const gfxRect* aOverrideBBox) {
  UniquePtr<UserSpaceMetrics> metrics =
      UserSpaceMetricsForFrame(aFilteredFrame);

  gfxContextMatrixAutoSaveRestore autoSR(aCtx);
  auto scaleFactors = aCtx->CurrentMatrixDouble().ScaleFactors();
  if (scaleFactors.xScale == 0 || scaleFactors.yScale == 0) {
    return;
  }

  gfxMatrix scaleMatrix(scaleFactors.xScale, 0.0f, 0.0f, scaleFactors.yScale,
                        0.0f, 0.0f);

  gfxMatrix reverseScaleMatrix = scaleMatrix;
  DebugOnly<bool> invertible = reverseScaleMatrix.Invert();
  MOZ_ASSERT(invertible);

  gfxMatrix scaleMatrixInDevUnits =
      scaleMatrix * SVGUtils::GetCSSPxToDevPxMatrix(aFilteredFrame);

  // Hardcode InputIsTainted to true because we don't want JS to be able to
  // read the rendered contents of aFilteredFrame.
  FilterInstance instance(aFilteredFrame, aFilteredFrame->GetContent(),
                          *metrics, aFilterChain, aFilterFrames,
                          /* InputIsTainted */ true, aPaintCallback,
                          scaleMatrixInDevUnits, aDirtyArea, nullptr, nullptr,
                          aOverrideBBox);
  if (instance.IsInitialized()) {
    // Pull scale vector out of aCtx's transform, put all scale factors, which
    // includes css and css-to-dev-px scale, into scaleMatrixInDevUnits.
    aCtx->SetMatrixDouble(reverseScaleMatrix * aCtx->CurrentMatrixDouble());

    instance.Render(aCtx, aImgParams, aOpacity);
  } else {
    // Render the unfiltered contents.
    aPaintCallback(*aCtx, aImgParams, nullptr, nullptr);
  }
}

static mozilla::wr::ComponentTransferFuncType FuncTypeToWr(uint8_t aFuncType) {
  MOZ_ASSERT(aFuncType != SVG_FECOMPONENTTRANSFER_SAME_AS_R);
  switch (aFuncType) {
    case SVG_FECOMPONENTTRANSFER_TYPE_TABLE:
      return mozilla::wr::ComponentTransferFuncType::Table;
    case SVG_FECOMPONENTTRANSFER_TYPE_DISCRETE:
      return mozilla::wr::ComponentTransferFuncType::Discrete;
    case SVG_FECOMPONENTTRANSFER_TYPE_LINEAR:
      return mozilla::wr::ComponentTransferFuncType::Linear;
    case SVG_FECOMPONENTTRANSFER_TYPE_GAMMA:
      return mozilla::wr::ComponentTransferFuncType::Gamma;
    case SVG_FECOMPONENTTRANSFER_TYPE_IDENTITY:
    default:
      return mozilla::wr::ComponentTransferFuncType::Identity;
  }
  MOZ_ASSERT_UNREACHABLE("all func types not handled?");
  return mozilla::wr::ComponentTransferFuncType::Identity;
}

WrFiltersStatus FilterInstance::BuildWebRenderFilters(
    nsIFrame* aFilteredFrame, Span<const StyleFilter> aFilters,
    StyleFilterType aStyleFilterType, WrFiltersHolder& aWrFilters,
    const nsPoint& aOffsetForSVGFilters) {
  WrFiltersStatus status = WrFiltersStatus::BLOB_FALLBACK;
  if (StaticPrefs::gfx_webrender_svg_filter_effects()) {
    status =
        BuildWebRenderSVGFiltersImpl(aFilteredFrame, aFilters, aStyleFilterType,
                                     aWrFilters, aOffsetForSVGFilters);
  }
  if (status == WrFiltersStatus::BLOB_FALLBACK) {
    status = BuildWebRenderFiltersImpl(aFilteredFrame, aFilters,
                                       aStyleFilterType, aWrFilters);
  }
  if (status == WrFiltersStatus::BLOB_FALLBACK) {
    aFilteredFrame->PresContext()->Document()->SetUseCounter(
        eUseCounter_custom_WrFilterFallback);
  }

  return status;
}

WrFiltersStatus FilterInstance::BuildWebRenderFiltersImpl(
    nsIFrame* aFilteredFrame, Span<const StyleFilter> aFilters,
    StyleFilterType aStyleFilterType, WrFiltersHolder& aWrFilters) {
  aWrFilters.filters.Clear();
  aWrFilters.filter_datas.Clear();
  aWrFilters.values.Clear();
  aWrFilters.post_filters_clip = Nothing();

  nsIFrame* firstFrame =
      nsLayoutUtils::FirstContinuationOrIBSplitSibling(aFilteredFrame);

  nsTArray<SVGFilterFrame*> filterFrames;
  if (SVGObserverUtils::GetAndObserveFilters(firstFrame, &filterFrames,
                                             aStyleFilterType) ==
      SVGObserverUtils::eHasRefsSomeInvalid) {
    return WrFiltersStatus::UNSUPPORTED;
  }

  UniquePtr<UserSpaceMetrics> metrics = UserSpaceMetricsForFrame(firstFrame);

  // TODO: simply using an identity matrix here, was pulling the scale from a
  // gfx context for the non-wr path.
  gfxMatrix scaleMatrix;
  gfxMatrix scaleMatrixInDevUnits =
      scaleMatrix * SVGUtils::GetCSSPxToDevPxMatrix(firstFrame);

  // Hardcode inputIsTainted to true because we don't want JS to be able to
  // read the rendered contents of aFilteredFrame.
  FilterInstance instance(firstFrame, firstFrame->GetContent(), *metrics,
                          aFilters, filterFrames, /* inputIsTainted */ true,
                          nullptr, scaleMatrixInDevUnits, nullptr, nullptr,
                          nullptr, nullptr);

  if (!instance.IsInitialized()) {
    return WrFiltersStatus::UNSUPPORTED;
  }

  // If there are too many filters to render, then just pretend that we
  // succeeded, and don't render any of them.
  if (instance.mFilterDescription.mPrimitives.Length() >
      StaticPrefs::gfx_webrender_max_filter_ops_per_chain()) {
    return WrFiltersStatus::DISABLED_FOR_PERFORMANCE;
  }

  Maybe<IntRect> finalClip;
  bool srgb = true;
  // We currently apply the clip on the stacking context after applying filters,
  // but primitive subregions imply clipping after each filter and not just the
  // end of the chain. For some types of filter it doesn't matter, but for those
  // which sample outside of the location of the destination pixel like blurs,
  // only clipping after could produce incorrect results, so we bail out in this
  // case.
  // We can lift this restriction once we have added support for primitive
  // subregions to WebRender's filters.
  for (uint32_t i = 0; i < instance.mFilterDescription.mPrimitives.Length();
       i++) {
    const auto& primitive = instance.mFilterDescription.mPrimitives[i];

    // WebRender only supports filters with one input.
    if (primitive.NumberOfInputs() != 1) {
      return WrFiltersStatus::BLOB_FALLBACK;
    }
    // The first primitive must have the source graphic as the input, all
    // other primitives must have the prior primitive as the input, otherwise
    // it's not supported by WebRender.
    if (i == 0) {
      if (primitive.InputPrimitiveIndex(0) !=
          FilterPrimitiveDescription::kPrimitiveIndexSourceGraphic) {
        return WrFiltersStatus::BLOB_FALLBACK;
      }
    } else if (primitive.InputPrimitiveIndex(0) != int32_t(i - 1)) {
      return WrFiltersStatus::BLOB_FALLBACK;
    }

    bool previousSrgb = srgb;
    bool primNeedsSrgb = primitive.InputColorSpace(0) == gfx::ColorSpace::SRGB;
    if (srgb && !primNeedsSrgb) {
      aWrFilters.filters.AppendElement(wr::FilterOp::SrgbToLinear());
    } else if (!srgb && primNeedsSrgb) {
      aWrFilters.filters.AppendElement(wr::FilterOp::LinearToSrgb());
    }
    srgb = primitive.OutputColorSpace() == gfx::ColorSpace::SRGB;

    const PrimitiveAttributes& attr = primitive.Attributes();

    bool filterIsNoop = false;

    if (attr.is<OpacityAttributes>()) {
      float opacity = attr.as<OpacityAttributes>().mOpacity;
      aWrFilters.filters.AppendElement(wr::FilterOp::Opacity(
          wr::PropertyBinding<float>::Value(opacity), opacity));
    } else if (attr.is<ColorMatrixAttributes>()) {
      const ColorMatrixAttributes& attributes =
          attr.as<ColorMatrixAttributes>();

      float transposed[20];
      if (gfx::ComputeColorMatrix(attributes, transposed)) {
        float matrix[20] = {
            transposed[0], transposed[5], transposed[10], transposed[15],
            transposed[1], transposed[6], transposed[11], transposed[16],
            transposed[2], transposed[7], transposed[12], transposed[17],
            transposed[3], transposed[8], transposed[13], transposed[18],
            transposed[4], transposed[9], transposed[14], transposed[19]};

        aWrFilters.filters.AppendElement(wr::FilterOp::ColorMatrix(matrix));
      } else {
        filterIsNoop = true;
      }
    } else if (attr.is<GaussianBlurAttributes>()) {
      if (finalClip) {
        // There's a clip that needs to apply before the blur filter, but
        // WebRender only lets us apply the clip at the end of the filter
        // chain. Clipping after a blur is not equivalent to clipping before
        // a blur, so bail out.
        return WrFiltersStatus::BLOB_FALLBACK;
      }

      const GaussianBlurAttributes& blur = attr.as<GaussianBlurAttributes>();

      const Size& stdDev = blur.mStdDeviation;
      if (stdDev.width != 0.0 || stdDev.height != 0.0) {
        aWrFilters.filters.AppendElement(
            wr::FilterOp::Blur(stdDev.width, stdDev.height));
      } else {
        filterIsNoop = true;
      }
    } else if (attr.is<DropShadowAttributes>()) {
      if (finalClip) {
        // We have to bail out for the same reason we would with a blur filter.
        return WrFiltersStatus::BLOB_FALLBACK;
      }

      const DropShadowAttributes& shadow = attr.as<DropShadowAttributes>();

      const Size& stdDev = shadow.mStdDeviation;
      if (stdDev.width != stdDev.height) {
        return WrFiltersStatus::BLOB_FALLBACK;
      }

      sRGBColor color = shadow.mColor;
      if (!primNeedsSrgb) {
        color = sRGBColor(gsRGBToLinearRGBMap[uint8_t(color.r * 255)],
                          gsRGBToLinearRGBMap[uint8_t(color.g * 255)],
                          gsRGBToLinearRGBMap[uint8_t(color.b * 255)], color.a);
      }
      wr::Shadow wrShadow;
      wrShadow.offset = {shadow.mOffset.x, shadow.mOffset.y};
      wrShadow.color = wr::ToColorF(ToDeviceColor(color));
      wrShadow.blur_radius = stdDev.width;
      wr::FilterOp filterOp = wr::FilterOp::DropShadow(wrShadow);

      aWrFilters.filters.AppendElement(filterOp);
    } else if (attr.is<ComponentTransferAttributes>()) {
      const ComponentTransferAttributes& attributes =
          attr.as<ComponentTransferAttributes>();

      size_t numValues =
          attributes.mValues[0].Length() + attributes.mValues[1].Length() +
          attributes.mValues[2].Length() + attributes.mValues[3].Length();
      if (numValues > 1024) {
        // Depending on how the wr shaders are implemented we may need to
        // limit the total number of values.
        return WrFiltersStatus::BLOB_FALLBACK;
      }

      wr::FilterOp filterOp = {wr::FilterOp::Tag::ComponentTransfer};
      wr::WrFilterData filterData;
      aWrFilters.values.AppendElement(nsTArray<float>());
      nsTArray<float>* values =
          &aWrFilters.values[aWrFilters.values.Length() - 1];
      values->SetCapacity(numValues);

      filterData.funcR_type = FuncTypeToWr(attributes.mTypes[0]);
      size_t R_startindex = values->Length();
      values->AppendElements(attributes.mValues[0]);
      filterData.R_values_count = attributes.mValues[0].Length();

      size_t indexToUse =
          attributes.mTypes[1] == SVG_FECOMPONENTTRANSFER_SAME_AS_R ? 0 : 1;
      filterData.funcG_type = FuncTypeToWr(attributes.mTypes[indexToUse]);
      size_t G_startindex = values->Length();
      values->AppendElements(attributes.mValues[indexToUse]);
      filterData.G_values_count = attributes.mValues[indexToUse].Length();

      indexToUse =
          attributes.mTypes[2] == SVG_FECOMPONENTTRANSFER_SAME_AS_R ? 0 : 2;
      filterData.funcB_type = FuncTypeToWr(attributes.mTypes[indexToUse]);
      size_t B_startindex = values->Length();
      values->AppendElements(attributes.mValues[indexToUse]);
      filterData.B_values_count = attributes.mValues[indexToUse].Length();

      filterData.funcA_type = FuncTypeToWr(attributes.mTypes[3]);
      size_t A_startindex = values->Length();
      values->AppendElements(attributes.mValues[3]);
      filterData.A_values_count = attributes.mValues[3].Length();

      filterData.R_values =
          filterData.R_values_count > 0 ? &((*values)[R_startindex]) : nullptr;
      filterData.G_values =
          filterData.G_values_count > 0 ? &((*values)[G_startindex]) : nullptr;
      filterData.B_values =
          filterData.B_values_count > 0 ? &((*values)[B_startindex]) : nullptr;
      filterData.A_values =
          filterData.A_values_count > 0 ? &((*values)[A_startindex]) : nullptr;

      aWrFilters.filters.AppendElement(filterOp);
      aWrFilters.filter_datas.AppendElement(filterData);
    } else {
      return WrFiltersStatus::BLOB_FALLBACK;
    }

    if (filterIsNoop && aWrFilters.filters.Length() > 0 &&
        (aWrFilters.filters.LastElement().tag ==
             wr::FilterOp::Tag::SrgbToLinear ||
         aWrFilters.filters.LastElement().tag ==
             wr::FilterOp::Tag::LinearToSrgb)) {
      // We pushed a color space conversion filter in prevision of applying
      // another filter which turned out to be a no-op, so the conversion is
      // unnecessary. Remove it from the filter list.
      // This is both an optimization and a way to pass the wptest
      // css/filter-effects/filter-scale-001.html for which the needless
      // sRGB->linear->no-op->sRGB roundtrip introduces a slight error and we
      // cannot add fuzziness to the test.
      Unused << aWrFilters.filters.PopLastElement();
      srgb = previousSrgb;
    }

    if (!filterIsNoop) {
      if (finalClip.isNothing()) {
        finalClip = Some(primitive.PrimitiveSubregion());
      } else {
        finalClip =
            Some(primitive.PrimitiveSubregion().Intersect(finalClip.value()));
      }
    }
  }

  if (!srgb) {
    aWrFilters.filters.AppendElement(wr::FilterOp::LinearToSrgb());
  }

  if (finalClip) {
    aWrFilters.post_filters_clip =
        Some(instance.FilterSpaceToFrameSpace(finalClip.value()));
  }
  return WrFiltersStatus::CHAIN;
}

static WrFiltersStatus WrSVGFEInputBuild(wr::FilterOpGraphPictureReference& pic,
                                         int32_t aSource, int16_t aNodeOutput,
                                         int16_t aSourceGraphic,
                                         int16_t aSourceAlpha,
                                         const int16_t aBufferIdMapping[]) {
  switch (aSource) {
    case FilterPrimitiveDescription::kPrimitiveIndexSourceGraphic:
      pic.buffer_id =
          wr::FilterOpGraphPictureBufferId::BufferId(aSourceGraphic);
      break;
    case FilterPrimitiveDescription::kPrimitiveIndexSourceAlpha:
      pic.buffer_id = wr::FilterOpGraphPictureBufferId::BufferId(aSourceAlpha);
      break;
    case FilterPrimitiveDescription::kPrimitiveIndexFillPaint:
    case FilterPrimitiveDescription::kPrimitiveIndexStrokePaint:
      // https://bugzilla.mozilla.org/show_bug.cgi?id=1897878
      // Fill and Stroke paints are not yet supported by WebRender, they may be
      // a color (most common) or pattern fill, so we could implement them with
      // feFlood or feImage + feTile depending on the nature of the fill.
      return WrFiltersStatus::BLOB_FALLBACK;
    default:
      MOZ_RELEASE_ASSERT(
          aSource >= 0,
          "Unrecognized SVG filter primitive enum value - added another?");
      MOZ_RELEASE_ASSERT(aSource < aNodeOutput,
                         "Invalid DAG - nodes can only refer to earlier nodes");
      if (aSource < 0 || aSource >= aNodeOutput) {
        return WrFiltersStatus::UNSUPPORTED;
      }
      // Look up the node we remapped this id to.
      // This can't overflow because aSource < aNodeOutput and the table is the
      // same size.
      pic.buffer_id =
          wr::FilterOpGraphPictureBufferId::BufferId(aBufferIdMapping[aSource]);
      break;
  }
  return WrFiltersStatus::SVGFE;
}

static WrFiltersStatus WrFilterOpSVGFEOpacity(
    WrFiltersHolder& aWrFilters, const wr::FilterOpGraphNode& aGraphNode,
    const OpacityAttributes& aAttributes) {
  // CSS opacity
  // This is the only CSS property that is has no direct analog in SVG, although
  // technically it can be represented as SVGFEComponentTransfer or
  // SVGFEColorMatrix or SVGFECompositeArithmetic, those would be inefficient
  // approaches.
  if (!StaticPrefs::gfx_webrender_svg_filter_effects_opacity()) {
    // Fallback if pref is disabled
    return WrFiltersStatus::BLOB_FALLBACK;
  }
  float opacity = aAttributes.mOpacity;
  if (opacity != 1.0f) {
    aWrFilters.filters.AppendElement(wr::FilterOp::SVGFEOpacity(
        aGraphNode, wr::PropertyBinding<float>::Value(opacity), opacity));
  } else {
    // If it's a no-op, we still have to generate a graph node
    aWrFilters.filters.AppendElement(wr::FilterOp::SVGFEIdentity(aGraphNode));
  }
  return WrFiltersStatus::SVGFE;
}

static WrFiltersStatus WrFilterOpSVGFEToAlpha(
    WrFiltersHolder& aWrFilters, const wr::FilterOpGraphNode& aGraphNode,
    const ToAlphaAttributes& aAttributes) {
  // Convert a color image to an alpha channel - internal use; generated by
  // SVGFilterInstance::GetOrCreateSourceAlphaIndex().
  if (!StaticPrefs::GetPrefName_gfx_webrender_svg_filter_effects_toalpha()) {
    // Fallback if pref is disabled
    return WrFiltersStatus::BLOB_FALLBACK;
  }
  aWrFilters.filters.AppendElement(wr::FilterOp::SVGFEToAlpha(aGraphNode));
  return WrFiltersStatus::SVGFE;
}

static WrFiltersStatus WrFilterOpSVGFEBlend(
    WrFiltersHolder& aWrFilters, const wr::FilterOpGraphNode& aGraphNode,
    const BlendAttributes& aAttributes) {
  // SVGFEBlend - common
  if (!StaticPrefs::gfx_webrender_svg_filter_effects_feblend()) {
    // Fallback if pref is disabled
    return WrFiltersStatus::BLOB_FALLBACK;
  }
  switch (aAttributes.mBlendMode) {
    case SVG_FEBLEND_MODE_COLOR:
      aWrFilters.filters.AppendElement(
          wr::FilterOp::SVGFEBlendColor(aGraphNode));
      return WrFiltersStatus::SVGFE;
    case SVG_FEBLEND_MODE_COLOR_BURN:
      aWrFilters.filters.AppendElement(
          wr::FilterOp::SVGFEBlendColorBurn(aGraphNode));
      return WrFiltersStatus::SVGFE;
    case SVG_FEBLEND_MODE_COLOR_DODGE:
      aWrFilters.filters.AppendElement(
          wr::FilterOp::SVGFEBlendColorDodge(aGraphNode));
      return WrFiltersStatus::SVGFE;
    case SVG_FEBLEND_MODE_DARKEN:
      aWrFilters.filters.AppendElement(
          wr::FilterOp::SVGFEBlendDarken(aGraphNode));
      return WrFiltersStatus::SVGFE;
    case SVG_FEBLEND_MODE_DIFFERENCE:
      aWrFilters.filters.AppendElement(
          wr::FilterOp::SVGFEBlendDifference(aGraphNode));
      return WrFiltersStatus::SVGFE;
    case SVG_FEBLEND_MODE_EXCLUSION:
      aWrFilters.filters.AppendElement(
          wr::FilterOp::SVGFEBlendExclusion(aGraphNode));
      return WrFiltersStatus::SVGFE;
    case SVG_FEBLEND_MODE_HARD_LIGHT:
      aWrFilters.filters.AppendElement(
          wr::FilterOp::SVGFEBlendHardLight(aGraphNode));
      return WrFiltersStatus::SVGFE;
    case SVG_FEBLEND_MODE_HUE:
      aWrFilters.filters.AppendElement(wr::FilterOp::SVGFEBlendHue(aGraphNode));
      return WrFiltersStatus::SVGFE;
    case SVG_FEBLEND_MODE_LIGHTEN:
      aWrFilters.filters.AppendElement(
          wr::FilterOp::SVGFEBlendLighten(aGraphNode));
      return WrFiltersStatus::SVGFE;
    case SVG_FEBLEND_MODE_LUMINOSITY:
      aWrFilters.filters.AppendElement(
          wr::FilterOp::SVGFEBlendLuminosity(aGraphNode));
      return WrFiltersStatus::SVGFE;
    case SVG_FEBLEND_MODE_MULTIPLY:
      aWrFilters.filters.AppendElement(
          wr::FilterOp::SVGFEBlendMultiply(aGraphNode));
      return WrFiltersStatus::SVGFE;
    case SVG_FEBLEND_MODE_NORMAL:
      aWrFilters.filters.AppendElement(
          wr::FilterOp::SVGFEBlendNormal(aGraphNode));
      return WrFiltersStatus::SVGFE;
    case SVG_FEBLEND_MODE_OVERLAY:
      aWrFilters.filters.AppendElement(
          wr::FilterOp::SVGFEBlendOverlay(aGraphNode));
      return WrFiltersStatus::SVGFE;
    case SVG_FEBLEND_MODE_SATURATION:
      aWrFilters.filters.AppendElement(
          wr::FilterOp::SVGFEBlendSaturation(aGraphNode));
      return WrFiltersStatus::SVGFE;
    case SVG_FEBLEND_MODE_SCREEN:
      aWrFilters.filters.AppendElement(
          wr::FilterOp::SVGFEBlendScreen(aGraphNode));
      return WrFiltersStatus::SVGFE;
    case SVG_FEBLEND_MODE_SOFT_LIGHT:
      aWrFilters.filters.AppendElement(
          wr::FilterOp::SVGFEBlendSoftLight(aGraphNode));
      return WrFiltersStatus::SVGFE;
    default:
      break;
  }
  MOZ_CRASH("Unrecognized SVG_FEBLEND_MODE");
  return WrFiltersStatus::BLOB_FALLBACK;
}

static WrFiltersStatus WrFilterOpSVGFEComposite(
    WrFiltersHolder& aWrFilters, const wr::FilterOpGraphNode& aGraphNode,
    const CompositeAttributes& aAttributes) {
  // SVGFEComposite - common
  if (!StaticPrefs::gfx_webrender_svg_filter_effects_fecomposite()) {
    // Fallback if pref is disabled
    return WrFiltersStatus::BLOB_FALLBACK;
  }
  switch (aAttributes.mOperator) {
    case SVG_FECOMPOSITE_OPERATOR_ARITHMETIC:
      aWrFilters.filters.AppendElement(wr::FilterOp::SVGFECompositeArithmetic(
          aGraphNode, aAttributes.mCoefficients[0],
          aAttributes.mCoefficients[1], aAttributes.mCoefficients[2],
          aAttributes.mCoefficients[3]));
      return WrFiltersStatus::SVGFE;
    case SVG_FECOMPOSITE_OPERATOR_ATOP:
      aWrFilters.filters.AppendElement(
          wr::FilterOp::SVGFECompositeATop(aGraphNode));
      return WrFiltersStatus::SVGFE;
    case SVG_FECOMPOSITE_OPERATOR_IN:
      aWrFilters.filters.AppendElement(
          wr::FilterOp::SVGFECompositeIn(aGraphNode));
      return WrFiltersStatus::SVGFE;
    case SVG_FECOMPOSITE_OPERATOR_LIGHTER:
      aWrFilters.filters.AppendElement(
          wr::FilterOp::SVGFECompositeLighter(aGraphNode));
      return WrFiltersStatus::SVGFE;
    case SVG_FECOMPOSITE_OPERATOR_OUT:
      aWrFilters.filters.AppendElement(
          wr::FilterOp::SVGFECompositeOut(aGraphNode));
      return WrFiltersStatus::SVGFE;
    case SVG_FECOMPOSITE_OPERATOR_OVER:
      aWrFilters.filters.AppendElement(
          wr::FilterOp::SVGFECompositeOver(aGraphNode));
      return WrFiltersStatus::SVGFE;
    case SVG_FECOMPOSITE_OPERATOR_XOR:
      aWrFilters.filters.AppendElement(
          wr::FilterOp::SVGFECompositeXOR(aGraphNode));
      return WrFiltersStatus::SVGFE;
    default:
      break;
  }
  MOZ_CRASH("Unrecognized SVG_FECOMPOSITE_OPERATOR");
  return WrFiltersStatus::BLOB_FALLBACK;
}

static WrFiltersStatus WrFilterOpSVGFEColorMatrix(
    WrFiltersHolder& aWrFilters, wr::FilterOpGraphNode& aGraphNode,
    const ColorMatrixAttributes& aAttributes) {
  // SVGFEColorMatrix - common
  if (!StaticPrefs::gfx_webrender_svg_filter_effects_fecolormatrix()) {
    // Fallback if pref is disabled
    return WrFiltersStatus::BLOB_FALLBACK;
  }
  float transposed[20];
  if (gfx::ComputeColorMatrix(aAttributes, transposed)) {
    float matrix[20] = {
        transposed[0], transposed[5], transposed[10], transposed[15],
        transposed[1], transposed[6], transposed[11], transposed[16],
        transposed[2], transposed[7], transposed[12], transposed[17],
        transposed[3], transposed[8], transposed[13], transposed[18],
        transposed[4], transposed[9], transposed[14], transposed[19]};
    aWrFilters.filters.AppendElement(
        wr::FilterOp::SVGFEColorMatrix(aGraphNode, matrix));
  } else {
    // If it's a no-op, we still have to generate a graph node
    aWrFilters.filters.AppendElement(wr::FilterOp::SVGFEIdentity(aGraphNode));
  }
  return WrFiltersStatus::SVGFE;
}

static WrFiltersStatus WrFilterOpSVGFEComponentTransfer(
    WrFiltersHolder& aWrFilters, wr::FilterOpGraphNode& aGraphNode,
    const ComponentTransferAttributes& aAttributes) {
  // SVGFEComponentTransfer - common
  if (!StaticPrefs::gfx_webrender_svg_filter_effects_fecomponenttransfer()) {
    // Fallback if pref is disabled
    return WrFiltersStatus::BLOB_FALLBACK;
  }
  // We ensure that there are at least 256 values for each channel so that
  // the shader can skip interpolation math for simplicity.
  size_t stops = 256;
  for (const auto& v : aAttributes.mValues) {
    if (stops < v.Length()) {
      stops = v.Length();
    }
  }
  aWrFilters.values.AppendElement(nsTArray<float>());
  nsTArray<float>& values = aWrFilters.values[aWrFilters.values.Length() - 1];
  values.SetCapacity(stops * 4);

  // Set the FilterData funcs for whether or not to interpolate the values
  // between stops, although we use enough stops that it may not matter.
  // The only type that doesn't use interpolation is discrete.
  wr::WrFilterData filterData{};
  filterData.funcR_type =
      aAttributes.mTypes[0] != SVG_FECOMPONENTTRANSFER_TYPE_DISCRETE
          ? mozilla::wr::ComponentTransferFuncType::Table
          : mozilla::wr::ComponentTransferFuncType::Discrete;
  filterData.funcG_type =
      aAttributes.mTypes[1] != SVG_FECOMPONENTTRANSFER_TYPE_DISCRETE
          ? mozilla::wr::ComponentTransferFuncType::Table
          : mozilla::wr::ComponentTransferFuncType::Discrete;
  filterData.funcB_type =
      aAttributes.mTypes[2] != SVG_FECOMPONENTTRANSFER_TYPE_DISCRETE
          ? mozilla::wr::ComponentTransferFuncType::Table
          : mozilla::wr::ComponentTransferFuncType::Discrete;
  filterData.funcA_type =
      aAttributes.mTypes[3] != SVG_FECOMPONENTTRANSFER_TYPE_DISCRETE
          ? mozilla::wr::ComponentTransferFuncType::Table
          : mozilla::wr::ComponentTransferFuncType::Discrete;

  // This is a bit of a strange way to store the table, it is an interleaved
  // array of RGBA values that we want to store in a single gpucache array
  // of raw pixels, so it's easiest to send it to WebRender as a single
  // channel, but FilterData requires it to be 4 channels, so we send it as
  // 4 groups of values but the data is interleaved.
  values.SetLength(stops * 4);
  filterData.R_values = &(values[0]);
  filterData.R_values_count = stops;
  filterData.G_values = &(values[stops]);
  filterData.G_values_count = stops;
  filterData.B_values = &(values[stops * 2]);
  filterData.B_values_count = stops;
  filterData.A_values = &(values[stops * 3]);
  filterData.A_values_count = stops;

  // This builds a single interleaved RGBA table as it is well suited to GPU
  // texture fetches without any dynamic component indexing in the shader which
  // can confuse buggy shader compilers.
  for (size_t c = 0; c < 4; c++) {
    auto f = aAttributes.mTypes[c];
    // Check if there's no data (we have crashtests for this).
    if (aAttributes.mValues[c].Length() < 1 &&
        f != SVG_FECOMPONENTTRANSFER_SAME_AS_R) {
      f = SVG_FECOMPONENTTRANSFER_TYPE_IDENTITY;
    }
    // Check for misuse of SVG_FECOMPONENTTRANSFER_SAME_AS_R.
    if (c == 0 && f == SVG_FECOMPONENTTRANSFER_SAME_AS_R) {
      f = SVG_FECOMPONENTTRANSFER_TYPE_IDENTITY;
    }
    switch (f) {
      case SVG_FECOMPONENTTRANSFER_TYPE_DISCRETE: {
        size_t length = (size_t)aAttributes.mValues[c].Length();
        size_t length1 = length - 1;
        float step = (float)length / (float)stops;
        for (size_t i = 0; i < stops; i++) {
          // find the corresponding color in the table
          // this can not overflow due to the length check
          float kf = (float)i * step;
          float floorkf = floor(kf);
          size_t k = (size_t)floorkf;
          k = std::min(k, length1);
          float v = aAttributes.mValues[c][k];
          v = mozilla::clamped(v, 0.0f, 1.0f);
          values[i * 4 + c] = v;
        }
        break;
      }
      case SVG_FECOMPONENTTRANSFER_TYPE_GAMMA: {
        float step = 1.0f / (float)(stops - 1);
        float amplitude = aAttributes.mValues[c][0];
        float exponent = aAttributes.mValues[c][1];
        float offset = aAttributes.mValues[c][2];
        for (size_t i = 0; i < stops; i++) {
          float v = amplitude * pow((float)i * step, exponent) + offset;
          v = mozilla::clamped(v, 0.0f, 1.0f);
          values[i * 4 + c] = v;
        }
        break;
      }
      case SVG_FECOMPONENTTRANSFER_TYPE_IDENTITY: {
        float step = 1.0f / (float)(stops - 1);
        for (size_t i = 0; i < stops; i++) {
          float v = (float)i * step;
          v = mozilla::clamped(v, 0.0f, 1.0f);
          values[i * 4 + c] = v;
        }
        break;
      }
      case SVG_FECOMPONENTTRANSFER_TYPE_LINEAR: {
        float step = aAttributes.mValues[c][0] / (float)(stops - 1);
        float intercept = aAttributes.mValues[c][1];
        for (size_t i = 0; i < stops; i++) {
          float v = (float)i * step + intercept;
          v = mozilla::clamped(v, 0.0f, 1.0f);
          values[i * 4 + c] = v;
        }
        break;
      }
      case SVG_FECOMPONENTTRANSFER_TYPE_TABLE: {
        size_t length1 = (size_t)aAttributes.mValues[c].Length() - 1;
        float step = (float)length1 / (float)(stops - 1);
        for (size_t i = 0; i < stops; i++) {
          // Find the corresponding color in the table and interpolate
          float kf = (float)i * step;
          float floorkf = floor(kf);
          size_t k = (size_t)floorkf;
          float v1 = aAttributes.mValues[c][k];
          float v2 = aAttributes.mValues[c][(k + 1 <= length1) ? k + 1 : k];
          float v = v1 + (v2 - v1) * (kf - floorkf);
          v = mozilla::clamped(v, 0.0f, 1.0f);
          values[i * 4 + c] = v;
        }
        break;
      }
      case SVG_FECOMPONENTTRANSFER_SAME_AS_R: {
        // We already checked c > 0 above.
        for (size_t i = 0; i < stops; i++) {
          values[i * 4 + c] = values[i * 4];
        }
        break;
      }
      default: {
        MOZ_CRASH("Unrecognized feComponentTransfer type");
        return WrFiltersStatus::BLOB_FALLBACK;
      }
    }
  }
  aWrFilters.filters.AppendElement(
      wr::FilterOp::SVGFEComponentTransfer(aGraphNode));
  aWrFilters.filter_datas.AppendElement(filterData);
  return WrFiltersStatus::SVGFE;
}

static WrFiltersStatus WrFilterOpSVGFEConvolveMatrix(
    WrFiltersHolder& aWrFilters, wr::FilterOpGraphNode& aGraphNode,
    const ConvolveMatrixAttributes& aAttributes) {
  // SVGFEConvolveMatrix - extremely rare
  if (!StaticPrefs::gfx_webrender_svg_filter_effects_feconvolvematrix()) {
    // Fallback if pref is disabled
    return WrFiltersStatus::BLOB_FALLBACK;
  }
  // Limited kernel size for performance reasons - spec allows us to drop
  // the whole filter graph if anything is unreasonable, so we only support
  // up to 5x5 kernel as that is pretty fast in hardware
  static constexpr int32_t width = 5;
  static constexpr int32_t height = 5;
  if (aAttributes.mKernelSize.Width() < 1 ||
      aAttributes.mKernelSize.Width() > width ||
      aAttributes.mKernelSize.Height() < 1 ||
      aAttributes.mKernelSize.Height() > height ||
      (size_t)aAttributes.mKernelSize.Width() *
              (size_t)aAttributes.mKernelSize.Height() >
          width * height) {
    return WrFiltersStatus::BLOB_FALLBACK;
  }
  // Reject kernel matrix if it is fewer values than dimensions suggest
  if (aAttributes.mKernelMatrix.Length() <
      (size_t)aAttributes.mKernelSize.Width() *
          (size_t)aAttributes.mKernelSize.Height()) {
    return WrFiltersStatus::UNSUPPORTED;
  }
  // Arrange the values in the order the shader expects
  float matrix[width * height];
  for (size_t y = 0; y < height; y++) {
    for (size_t x = 0; x < width; x++) {
      if (x < (size_t)aAttributes.mKernelSize.Width() &&
          y < (size_t)aAttributes.mKernelSize.Height()) {
        matrix[y * width + x] =
            aAttributes.mKernelMatrix[y * aAttributes.mKernelSize.Width() + x];
      } else {
        matrix[y * width + x] = 0.0f;
      }
    }
  }
  switch (aAttributes.mEdgeMode) {
    case SVG_EDGEMODE_UNKNOWN:
    case SVG_EDGEMODE_DUPLICATE:
      aWrFilters.filters.AppendElement(
          wr::FilterOp::SVGFEConvolveMatrixEdgeModeDuplicate(
              aGraphNode, aAttributes.mKernelSize.Width(),
              aAttributes.mKernelSize.Height(), matrix, aAttributes.mDivisor,
              aAttributes.mBias, aAttributes.mTarget.x.value,
              aAttributes.mTarget.y.value,
              aAttributes.mKernelUnitLength.Width(),
              aAttributes.mKernelUnitLength.Height(),
              aAttributes.mPreserveAlpha));
      return WrFiltersStatus::SVGFE;
    case SVG_EDGEMODE_NONE:
      aWrFilters.filters.AppendElement(
          wr::FilterOp::SVGFEConvolveMatrixEdgeModeNone(
              aGraphNode, aAttributes.mKernelSize.Width(),
              aAttributes.mKernelSize.Height(), matrix, aAttributes.mDivisor,
              aAttributes.mBias, aAttributes.mTarget.x.value,
              aAttributes.mTarget.y.value,
              aAttributes.mKernelUnitLength.Width(),
              aAttributes.mKernelUnitLength.Height(),
              aAttributes.mPreserveAlpha));
      return WrFiltersStatus::SVGFE;
    case SVG_EDGEMODE_WRAP:
      aWrFilters.filters.AppendElement(
          wr::FilterOp::SVGFEConvolveMatrixEdgeModeWrap(
              aGraphNode, aAttributes.mKernelSize.Width(),
              aAttributes.mKernelSize.Height(), matrix, aAttributes.mDivisor,
              aAttributes.mBias, aAttributes.mTarget.x.value,
              aAttributes.mTarget.y.value,
              aAttributes.mKernelUnitLength.Width(),
              aAttributes.mKernelUnitLength.Height(),
              aAttributes.mPreserveAlpha));
      return WrFiltersStatus::SVGFE;
    default:
      break;
  }
  MOZ_CRASH("Unrecognized SVG_EDGEMODE");
  return WrFiltersStatus::BLOB_FALLBACK;
}

static WrFiltersStatus WrFilterOpSVGFEDiffuseLighting(
    WrFiltersHolder& aWrFilters, wr::FilterOpGraphNode& aGraphNode,
    const DiffuseLightingAttributes& aAttributes,
    const LayoutDevicePoint& aUserspaceOffset) {
  // SVGFEDiffuseLighting - extremely rare
  if (!StaticPrefs::gfx_webrender_svg_filter_effects_fediffuselighting()) {
    // Fallback if pref is disabled
    return WrFiltersStatus::BLOB_FALLBACK;
  }
  switch (aAttributes.mLightType) {
    case LightType::Distant:
      aWrFilters.filters.AppendElement(
          wr::FilterOp::SVGFEDiffuseLightingDistant(
              aGraphNode, aAttributes.mSurfaceScale,
              aAttributes.mLightingConstant,
              aAttributes.mKernelUnitLength.width,
              aAttributes.mKernelUnitLength.height, aAttributes.mLightValues[0],
              aAttributes.mLightValues[1]));
      return WrFiltersStatus::SVGFE;
    case LightType::Point:
      aWrFilters.filters.AppendElement(wr::FilterOp::SVGFEDiffuseLightingPoint(
          aGraphNode, aAttributes.mSurfaceScale, aAttributes.mLightingConstant,
          aAttributes.mKernelUnitLength.width,
          aAttributes.mKernelUnitLength.height,
          aAttributes.mLightValues[0] + aUserspaceOffset.x.value,
          aAttributes.mLightValues[1] + aUserspaceOffset.y.value,
          aAttributes.mLightValues[2]));
      return WrFiltersStatus::SVGFE;
    case LightType::Spot:
      aWrFilters.filters.AppendElement(wr::FilterOp::SVGFEDiffuseLightingSpot(
          aGraphNode, aAttributes.mSurfaceScale, aAttributes.mLightingConstant,
          aAttributes.mKernelUnitLength.width,
          aAttributes.mKernelUnitLength.height,
          aAttributes.mLightValues[0] + aUserspaceOffset.x.value,
          aAttributes.mLightValues[1] + aUserspaceOffset.y.value,
          aAttributes.mLightValues[2],
          aAttributes.mLightValues[3] + aUserspaceOffset.x.value,
          aAttributes.mLightValues[4] + aUserspaceOffset.y.value,
          aAttributes.mLightValues[5], aAttributes.mLightValues[6],
          aAttributes.mLightValues[7]));
      return WrFiltersStatus::SVGFE;
    case LightType::None:
    case LightType::Max:
      // No default case, so that the compiler will warn if new enums are added
      break;
  }
  MOZ_CRASH("Unrecognized LightType");
  return WrFiltersStatus::BLOB_FALLBACK;
}

static WrFiltersStatus WrFilterOpSVGFEDisplacementMap(
    WrFiltersHolder& aWrFilters, wr::FilterOpGraphNode& aGraphNode,
    const DisplacementMapAttributes& aAttributes) {
  // SVGFEDisplacementMap - extremely rare
  if (!StaticPrefs::gfx_webrender_svg_filter_effects_fedisplacementmap()) {
    // Fallback if pref is disabled
    return WrFiltersStatus::BLOB_FALLBACK;
  }
  aWrFilters.filters.AppendElement(wr::FilterOp::SVGFEDisplacementMap(
      aGraphNode, aAttributes.mScale, aAttributes.mXChannel,
      aAttributes.mYChannel));
  return WrFiltersStatus::SVGFE;
}

static WrFiltersStatus WrFilterOpSVGFEDropShadow(
    WrFiltersHolder& aWrFilters, wr::FilterOpGraphNode& aGraphNode,
    const DropShadowAttributes& aAttributes) {
  // SVGFEDropShadow - extremely rare
  if (!StaticPrefs::gfx_webrender_svg_filter_effects_fedropshadow()) {
    // Fallback if pref is disabled
    return WrFiltersStatus::BLOB_FALLBACK;
  }
  // This color is used in a shader coefficient that is in sRGB color space,
  // so it needs to go through the regular device color transformation.
  // This does not premultiply the alpha - the shader will do that.
  aWrFilters.filters.AppendElement(wr::FilterOp::SVGFEDropShadow(
      aGraphNode, wr::ToColorF(ToDeviceColor(aAttributes.mColor)),
      aAttributes.mOffset.x, aAttributes.mOffset.y,
      aAttributes.mStdDeviation.width, aAttributes.mStdDeviation.height));
  return WrFiltersStatus::SVGFE;
}

static WrFiltersStatus WrFilterOpSVGFEFlood(
    WrFiltersHolder& aWrFilters, wr::FilterOpGraphNode& aGraphNode,
    const FloodAttributes& aAttributes) {
  // SVGFEFlood - common
  if (!StaticPrefs::gfx_webrender_svg_filter_effects_feflood()) {
    // Fallback if pref is disabled
    return WrFiltersStatus::BLOB_FALLBACK;
  }
  // This color is used in a shader coefficient that is in sRGB color space,
  // so it needs to go through the regular device color transformation.
  // This does not premultiply the alpha - the shader will do that.
  aWrFilters.filters.AppendElement(wr::FilterOp::SVGFEFlood(
      aGraphNode, wr::ToColorF(ToDeviceColor(aAttributes.mColor))));
  return WrFiltersStatus::SVGFE;
}

static WrFiltersStatus WrFilterOpSVGFEGaussianBlur(
    WrFiltersHolder& aWrFilters, wr::FilterOpGraphNode& aGraphNode,
    const GaussianBlurAttributes& aAttributes) {
  // SVGFEGaussianBlur - common
  if (!StaticPrefs::gfx_webrender_svg_filter_effects_fegaussianblur()) {
    // Fallback if pref is disabled
    return WrFiltersStatus::BLOB_FALLBACK;
  }
  aWrFilters.filters.AppendElement(wr::FilterOp::SVGFEGaussianBlur(
      aGraphNode, aAttributes.mStdDeviation.width,
      aAttributes.mStdDeviation.height));
  return WrFiltersStatus::SVGFE;
}

static WrFiltersStatus WrFilterOpSVGFEImage(
    WrFiltersHolder& aWrFilters, wr::FilterOpGraphNode& aGraphNode,
    const ImageAttributes& aAttributes,
    const LayoutDevicePoint& aUserspaceOffset) {
  // SVGFEImage - Extremely rare
  if (!StaticPrefs::gfx_webrender_svg_filter_effects_feimage()) {
    // Fallback if pref is disabled
    return WrFiltersStatus::BLOB_FALLBACK;
  }
  float matrix[6];
  matrix[0] = aAttributes.mTransform.components[0];
  matrix[1] = aAttributes.mTransform.components[1];
  matrix[2] = aAttributes.mTransform.components[2];
  matrix[3] = aAttributes.mTransform.components[3];
  matrix[4] = aAttributes.mTransform.components[4] + aUserspaceOffset.x.value;
  matrix[5] = aAttributes.mTransform.components[5] + aUserspaceOffset.y.value;
  // TODO: We need to resolve aAttributes.mInputIndex to an actual image
  // somehow.
  aWrFilters.filters.AppendElement(
      wr::FilterOp::SVGFEImage(aGraphNode, aAttributes.mFilter, matrix));
  return WrFiltersStatus::SVGFE;
}

static WrFiltersStatus WrFilterOpSVGFEMerge(
    WrFiltersHolder& aWrFilters, wr::FilterOpGraphNode& aGraphNode,
    const MergeAttributes& aAttributes, FilterPrimitiveDescription& aPrimitive,
    int16_t aNodeOutput, int16_t aSourceGraphic, int16_t aSourceAlpha,
    const int16_t aBufferIdMapping[], size_t aMaxFilters) {
  // SVGFEMerge - common
  if (!StaticPrefs::gfx_webrender_svg_filter_effects_femerge()) {
    // Fallback if pref is disabled
    return WrFiltersStatus::BLOB_FALLBACK;
  }
  // There is no SVGFEMerge, so we need to expand the provided inputs to a
  // chain of SVGFECompositeOver ops before handing it to WebRender.
  if (aPrimitive.NumberOfInputs() >= 2) {
    wr::FilterOpGraphPictureReference previous{};
    for (size_t index = 0; index < aPrimitive.NumberOfInputs(); index++) {
      wr::FilterOpGraphPictureReference current{};
      WrFiltersStatus status = WrSVGFEInputBuild(
          current, aPrimitive.InputPrimitiveIndex(index), aNodeOutput,
          aSourceGraphic, aSourceAlpha, aBufferIdMapping);
      if (status != WrFiltersStatus::SVGFE) {
        // If the input is an invalid ref, we have to disable filters on this.
        return status;
      }
      aGraphNode.input = current;
      aGraphNode.input2 = previous;
      if (aWrFilters.filters.Length() >= aMaxFilters) {
        // Reject the graph if it has too many filters to even process
        return WrFiltersStatus::DISABLED_FOR_PERFORMANCE;
      }
      if (index >= 1) {
        // Emit a node that composites this pic over the previous pics.
        aWrFilters.filters.AppendElement(
            wr::FilterOp::SVGFECompositeOver(aGraphNode));
        // Use this graph node as input2 (background) on the next node.
        previous.buffer_id = wr::FilterOpGraphPictureBufferId::BufferId(
            (int16_t)(aWrFilters.filters.Length() - 1));
      } else {
        // Conceptually the first pic is composited over transparent black
        // which is a no-op, so we just use the first pic as a direct input
        // on the first node we actually emit.
        previous.buffer_id = current.buffer_id;
      }
    }
  } else if (aPrimitive.NumberOfInputs() == 1) {
    // If we only got a single feMergeNode pic, we still want to apply
    // the subregion clip, so make an SVGFEIdentity op.
    aWrFilters.filters.AppendElement(wr::FilterOp::SVGFEIdentity(aGraphNode));
  } else {
    // feMerge with no feMergeNodes is just blank.
    wr::ColorF blank = {0.0f, 0.0f, 0.0f, 0.0f};
    aWrFilters.filters.AppendElement(
        wr::FilterOp::SVGFEFlood(aGraphNode, blank));
  }
  return WrFiltersStatus::SVGFE;
}

static WrFiltersStatus WrFilterOpSVGFEMorphology(
    WrFiltersHolder& aWrFilters, wr::FilterOpGraphNode& aGraphNode,
    const MorphologyAttributes& aAttributes) {
  // SVGFEMorphology - Rare
  if (!StaticPrefs::gfx_webrender_svg_filter_effects_femorphology()) {
    // Fallback if pref is disabled
    return WrFiltersStatus::BLOB_FALLBACK;
  }
  switch (aAttributes.mOperator) {
    case SVG_OPERATOR_DILATE:
      aWrFilters.filters.AppendElement(wr::FilterOp::SVGFEMorphologyDilate(
          aGraphNode, aAttributes.mRadii.width, aAttributes.mRadii.height));
      return WrFiltersStatus::SVGFE;
    case SVG_OPERATOR_ERODE:
      aWrFilters.filters.AppendElement(wr::FilterOp::SVGFEMorphologyErode(
          aGraphNode, aAttributes.mRadii.width, aAttributes.mRadii.height));
      return WrFiltersStatus::SVGFE;
    default:
      break;
  }
  MOZ_CRASH("Unrecognized SVG_OPERATOR");
  return WrFiltersStatus::BLOB_FALLBACK;
}

static WrFiltersStatus WrFilterOpSVGFEOffset(
    WrFiltersHolder& aWrFilters, wr::FilterOpGraphNode& aGraphNode,
    const OffsetAttributes& aAttributes) {
  // SVGFEOffset - Common
  if (!StaticPrefs::gfx_webrender_svg_filter_effects_feoffset()) {
    // Fallback if pref is disabled
    return WrFiltersStatus::BLOB_FALLBACK;
  }
  aWrFilters.filters.AppendElement(wr::FilterOp::SVGFEOffset(
      aGraphNode, (float)aAttributes.mValue.x, (float)aAttributes.mValue.y));
  return WrFiltersStatus::SVGFE;
}

static WrFiltersStatus WrFilterOpSVGFETile(WrFiltersHolder& aWrFilters,
                                           wr::FilterOpGraphNode& aGraphNode,
                                           const TileAttributes& aAttributes) {
  // SVGFETile - Extremely rare
  if (!StaticPrefs::gfx_webrender_svg_filter_effects_fetile()) {
    // Fallback if pref is disabled
    return WrFiltersStatus::BLOB_FALLBACK;
  }
  aWrFilters.filters.AppendElement(wr::FilterOp::SVGFETile(aGraphNode));
  return WrFiltersStatus::SVGFE;
}

static WrFiltersStatus WrFilterOpSVGFESpecularLighting(
    WrFiltersHolder& aWrFilters, wr::FilterOpGraphNode& aGraphNode,
    const SpecularLightingAttributes& aAttributes,
    const LayoutDevicePoint& aUserspaceOffset) {
  // SVGFESpecularLighting - extremely rare
  if (!StaticPrefs::gfx_webrender_svg_filter_effects_fespecularlighting()) {
    // Fallback if pref is disabled
    return WrFiltersStatus::BLOB_FALLBACK;
  }
  switch (aAttributes.mLightType) {
    case LightType::Distant:
      aWrFilters.filters.AppendElement(
          wr::FilterOp::SVGFESpecularLightingDistant(
              aGraphNode, aAttributes.mSurfaceScale,
              aAttributes.mLightingConstant, aAttributes.mSpecularExponent,
              aAttributes.mKernelUnitLength.width,
              aAttributes.mKernelUnitLength.height, aAttributes.mLightValues[0],
              aAttributes.mLightValues[1]));
      return WrFiltersStatus::SVGFE;
    case LightType::Point:
      aWrFilters.filters.AppendElement(wr::FilterOp::SVGFESpecularLightingPoint(
          aGraphNode, aAttributes.mSurfaceScale, aAttributes.mLightingConstant,
          aAttributes.mSpecularExponent, aAttributes.mKernelUnitLength.width,
          aAttributes.mKernelUnitLength.height,
          aAttributes.mLightValues[0] + aUserspaceOffset.x.value,
          aAttributes.mLightValues[1] + aUserspaceOffset.y.value,
          aAttributes.mLightValues[2]));
      return WrFiltersStatus::SVGFE;
    case LightType::Spot:
      aWrFilters.filters.AppendElement(wr::FilterOp::SVGFESpecularLightingSpot(
          aGraphNode, aAttributes.mSurfaceScale, aAttributes.mLightingConstant,
          aAttributes.mSpecularExponent, aAttributes.mKernelUnitLength.width,
          aAttributes.mKernelUnitLength.height,
          aAttributes.mLightValues[0] + aUserspaceOffset.x.value,
          aAttributes.mLightValues[1] + aUserspaceOffset.y.value,
          aAttributes.mLightValues[2],
          aAttributes.mLightValues[3] + aUserspaceOffset.x.value,
          aAttributes.mLightValues[4] + aUserspaceOffset.y.value,
          aAttributes.mLightValues[5], aAttributes.mLightValues[6],
          aAttributes.mLightValues[7]));
      return WrFiltersStatus::SVGFE;
    case LightType::None:
    case LightType::Max:
      // No default case, so that the compiler will warn if new enums are added
      break;
  }
  MOZ_CRASH("Unrecognized LightType");
  return WrFiltersStatus::BLOB_FALLBACK;
}

static WrFiltersStatus WrFilterOpSVGFETurbulence(
    WrFiltersHolder& aWrFilters, wr::FilterOpGraphNode& aGraphNode,
    const TurbulenceAttributes& aAttributes,
    const LayoutDevicePoint& aUserspaceOffset) {
  // SVGFETurbulence - Rare
  if (!StaticPrefs::gfx_webrender_svg_filter_effects_feturbulence()) {
    // Fallback if pref is disabled
    return WrFiltersStatus::BLOB_FALLBACK;
  }
  // The software implementation we use converts float to uint32_t and then
  // to int32_t, so we do that here to get identical results to the prior
  // implementation, in contrast to the spec which uses purely signed math
  // for setting up the seed.
  int32_t m1 = 2147483647 - 1;
  int32_t seed = (int32_t)((uint32_t)aAttributes.mSeed);
  if (seed <= 0) seed = -(seed % m1) + 1;
  if (seed > m1) seed = m1;
  switch (aAttributes.mType) {
    case SVG_TURBULENCE_TYPE_FRACTALNOISE:
      if (aAttributes.mStitchable) {
        aWrFilters.filters.AppendElement(
            wr::FilterOp::SVGFETurbulenceWithFractalNoiseWithStitching(
                aGraphNode, aAttributes.mBaseFrequency.width,
                aAttributes.mBaseFrequency.height, aAttributes.mOctaves, seed));
      } else {
        aWrFilters.filters.AppendElement(
            wr::FilterOp::SVGFETurbulenceWithFractalNoiseWithNoStitching(
                aGraphNode, aAttributes.mBaseFrequency.width,
                aAttributes.mBaseFrequency.height, aAttributes.mOctaves, seed));
      }
      return WrFiltersStatus::SVGFE;
    case SVG_TURBULENCE_TYPE_TURBULENCE:
      if (aAttributes.mStitchable) {
        aWrFilters.filters.AppendElement(
            wr::FilterOp::SVGFETurbulenceWithTurbulenceNoiseWithStitching(
                aGraphNode, aAttributes.mBaseFrequency.width,
                aAttributes.mBaseFrequency.height, aAttributes.mOctaves, seed));
      } else {
        aWrFilters.filters.AppendElement(
            wr::FilterOp::SVGFETurbulenceWithTurbulenceNoiseWithNoStitching(
                aGraphNode, aAttributes.mBaseFrequency.width,
                aAttributes.mBaseFrequency.height, aAttributes.mOctaves, seed));
      }
      return WrFiltersStatus::SVGFE;
    default:
      break;
  }
  MOZ_CRASH("Unrecognized SVG_TURBULENCE_TYPE");
  return WrFiltersStatus::BLOB_FALLBACK;
}

/// Builds filter DAG for fully accelerated rendering of SVG filter primitives
/// and CSS filter chains using SVG filter primitives
WrFiltersStatus FilterInstance::BuildWebRenderSVGFiltersImpl(
    nsIFrame* aFilteredFrame, Span<const StyleFilter> aFilters,
    StyleFilterType aStyleFilterType, WrFiltersHolder& aWrFilters,
    const nsPoint& aOffsetForSVGFilters) {
  // If we return without making a valid filter graph, we need to restore
  // aInitialized before the fallback code is run.
  aWrFilters.filters.Clear();
  aWrFilters.filter_datas.Clear();
  aWrFilters.values.Clear();
  aWrFilters.post_filters_clip = Nothing();

  nsIFrame* firstFrame =
      nsLayoutUtils::FirstContinuationOrIBSplitSibling(aFilteredFrame);

  nsTArray<SVGFilterFrame*> filterFrames;
  if (SVGObserverUtils::GetAndObserveFilters(firstFrame, &filterFrames,
                                             aStyleFilterType) ==
      SVGObserverUtils::eHasRefsSomeInvalid) {
    return WrFiltersStatus::UNSUPPORTED;
  }

  UniquePtr<UserSpaceMetrics> metrics = UserSpaceMetricsForFrame(firstFrame);

  gfxRect filterSpaceBoundsNotSnapped;

  // TODO: simply using an identity matrix here, was pulling the scale from a
  // gfx context for the non-wr path.
  gfxMatrix scaleMatrix;
  gfxMatrix scaleMatrixInDevUnits =
      scaleMatrix * SVGUtils::GetCSSPxToDevPxMatrix(firstFrame);

  // Hardcode inputIsTainted to true because we don't want JS to be able to
  // read the rendered contents of aFilteredFrame.
  FilterInstance instance(firstFrame, firstFrame->GetContent(), *metrics,
                          aFilters, filterFrames, /* inputIsTainted */ true,
                          nullptr, scaleMatrixInDevUnits, nullptr, nullptr,
                          nullptr, nullptr, &filterSpaceBoundsNotSnapped);

  if (!instance.IsInitialized()) {
    return WrFiltersStatus::UNSUPPORTED;
  }

  // If there more filters than the limit pref allows, we can drop the entire
  // filter graph and pretend we succeeded, the SVG spec allows us to drop any
  // overly complex graph, very large graphs tend to OOM anyway.
  if (instance.mFilterDescription.mPrimitives.Length() >
      StaticPrefs::gfx_webrender_max_filter_ops_per_chain()) {
    return WrFiltersStatus::DISABLED_FOR_PERFORMANCE;
  }

  // We have to remap the input nodes to a possibly larger number of output
  // nodes due to expanding feMerge.
  static constexpr size_t maxFilters = wr::SVGFE_GRAPH_MAX;
  int16_t bufferIdMapping[maxFilters];
  // Just drop the graph if there are too many filters to process.
  if (instance.mFilterDescription.mPrimitives.Length() > maxFilters) {
    return WrFiltersStatus::DISABLED_FOR_PERFORMANCE;
  }

  // For subregions and filter parameters, we need to transform into the SVG
  // User Space coordinate system, which is the parent stacking context
  // coordinate system, not to be confused with Filter Space (which is this
  // stacking context's child coordinate system) or Frame Space.
  //
  // See nsLayoutUtils::ComputeOffsetToUserSpace for further explanation, and
  // SVGIntegrationUtils.cpp EffectOffsets::ComputeEffectOffset is how this is
  // done in the blob fallback.
  //
  // The display list we are building already puts the child elements' geometry
  // (if any) in SVG User Space, so we want the filter region and primitive
  // subregions to be in SVG User Space, so uerspaceOffset represents the offset
  // from Filter to User Space, which is in LayoutDevice units.
  //
  // As a practical matter, things like regular view zoom change Filter Space
  // scale so we don't have to do anything for that, pinch zoom in apz can be
  // doing its own thing but doesn't affect the coordinate system we use here,
  // as everything is multiplied by subregion and divided by filterRegion, so
  // they only need to be matching scale from WebRender perspective.
  LayoutDevicePoint userspaceOffset = LayoutDevicePoint::FromAppUnits(
      aOffsetForSVGFilters,
      aFilteredFrame->PresContext()->AppUnitsPerDevPixel());

  // The bounds of SourceGraphic are defined in the spec as being equal to the
  // filter region, so we need to compute that, and while subregion bounds are
  // always integer, the bounds of the filter element (and hence filter region)
  // are not actually integer, so we need to account for the non-integer filter
  // region clip by using filterSpaceBoundsNotSnapped, this matters in:
  // ./mach reftest layout/reftests/svg/filter-scaled-01.svg
  wr::LayoutRect filterRegion = {
      {(float)(filterSpaceBoundsNotSnapped.TopLeft().x +
               userspaceOffset.x.value),
       (float)(filterSpaceBoundsNotSnapped.TopLeft().y +
               userspaceOffset.y.value)},
      {(float)(filterSpaceBoundsNotSnapped.BottomRight().x +
               userspaceOffset.x.value),
       (float)(filterSpaceBoundsNotSnapped.BottomRight().y +
               userspaceOffset.y.value)}};

  // To enforce the filterRegion clipping SourceGraphic before it enters the
  // graph we have to create a SourceGraphic node and SourceAlpha node, when we
  // implement StrokePaint and FillPaint they will need to create nodes on
  // demand however as they have custom colors (feFlood) and patterns (feTile).
  auto sourceGraphicNode = (int16_t)aWrFilters.filters.Length();
  auto sourceNode = wr::FilterOpGraphNode{};
  sourceNode.subregion = filterRegion;
  aWrFilters.filters.AppendElement(
      wr::FilterOp::SVGFESourceGraphic(sourceNode));
  auto sourceAlphaNode = (int16_t)aWrFilters.filters.Length();
  aWrFilters.filters.AppendElement(wr::FilterOp::SVGFESourceAlpha(sourceNode));

  // We have some failure modes that can occur when processing the graph.
  WrFiltersStatus status = WrFiltersStatus::SVGFE;

  for (uint32_t i = 0; i < instance.mFilterDescription.mPrimitives.Length();
       i++) {
    const auto& primitive = instance.mFilterDescription.mPrimitives[i];
    const PrimitiveAttributes& attr = primitive.Attributes();
    const bool linear = primitive.OutputColorSpace() == ColorSpace::LinearRGB;
    const size_t inputs = primitive.NumberOfInputs();
    wr::FilterOpGraphNode graphNode = wr::FilterOpGraphNode{};
    // Physical (linear) colorspace is the default in SVG filters, whereas all
    // CSS filters use sRGB (curved / naive) colorspace calculations for math,
    // this is the color-interpolation-filter property in SVG spec.  Note that
    // feFlood cares about the color-interpolation property on the color value
    // provided, rather than the regular color-interpolation-filter property.
    graphNode.linear = linear;
    // Transform the subregion into SVG 'user space' which WebRender expects.
    graphNode.subregion =
        wr::ToLayoutRect(Rect(primitive.PrimitiveSubregion()) +
                         userspaceOffset.ToUnknownPoint());
    // We need to clip the final output node by the filterRegion, as it could
    // be non-integer (whereas the subregions were computed by SVGFilterInstance
    // code as integer only).
    if (i == instance.mFilterDescription.mPrimitives.Length() - 1) {
      if (graphNode.subregion.min.x < filterRegion.min.x) {
        graphNode.subregion.min.x = filterRegion.min.x;
      }
      if (graphNode.subregion.min.y < filterRegion.min.y) {
        graphNode.subregion.min.y = filterRegion.min.y;
      }
      if (graphNode.subregion.max.x > filterRegion.max.x) {
        graphNode.subregion.max.x = filterRegion.max.x;
      }
      if (graphNode.subregion.max.y > filterRegion.max.y) {
        graphNode.subregion.max.y = filterRegion.max.y;
      }
    }

    // Buffer ids are matched up later by WebRender to understand the DAG, we
    // hold the following assumptions (and verify them regularly):
    // * Inputs referencing buffer ids are always < node index
    //   (This means the DAG can be walked sequentially as a flat array and
    //    always evaluate correctly)
    // * node index < maxFilters
    graphNode.input.buffer_id = wr::FilterOpGraphPictureBufferId::None();
    graphNode.input2.buffer_id = wr::FilterOpGraphPictureBufferId::None();
    if (inputs >= 1) {
      status = WrSVGFEInputBuild(
          graphNode.input, primitive.InputPrimitiveIndex(0), (int16_t)i,
          sourceGraphicNode, sourceAlphaNode, bufferIdMapping);
      if (status != WrFiltersStatus::SVGFE) {
        break;
      }
      if (inputs >= 2) {
        status = WrSVGFEInputBuild(
            graphNode.input2, primitive.InputPrimitiveIndex(1), (int16_t)i,
            sourceGraphicNode, sourceAlphaNode, bufferIdMapping);
        if (status != WrFiltersStatus::SVGFE) {
          break;
        }
      }
    }

    // If there are too many filters (after feMerge expansion) to keep track of
    // in bufferIdMapping[] then we can just drop the entire graph, the SVG spec
    // allows us to drop overly complex graphs and maxFilters is not a small
    // quantity.
    if (aWrFilters.filters.Length() >= maxFilters) {
      status = WrFiltersStatus::DISABLED_FOR_PERFORMANCE;
      break;
    }

    if (attr.is<OpacityAttributes>()) {
      status = WrFilterOpSVGFEOpacity(aWrFilters, graphNode,
                                      attr.as<OpacityAttributes>());
    } else if (attr.is<ToAlphaAttributes>()) {
      status = WrFilterOpSVGFEToAlpha(aWrFilters, graphNode,
                                      attr.as<ToAlphaAttributes>());
    } else if (attr.is<BlendAttributes>()) {
      status = WrFilterOpSVGFEBlend(aWrFilters, graphNode,
                                    attr.as<BlendAttributes>());
    } else if (attr.is<ColorMatrixAttributes>()) {
      status = WrFilterOpSVGFEColorMatrix(aWrFilters, graphNode,
                                          attr.as<ColorMatrixAttributes>());
    } else if (attr.is<ComponentTransferAttributes>()) {
      status = WrFilterOpSVGFEComponentTransfer(
          aWrFilters, graphNode, attr.as<ComponentTransferAttributes>());
    } else if (attr.is<CompositeAttributes>()) {
      status = WrFilterOpSVGFEComposite(aWrFilters, graphNode,
                                        attr.as<CompositeAttributes>());
    } else if (attr.is<ConvolveMatrixAttributes>()) {
      status = WrFilterOpSVGFEConvolveMatrix(
          aWrFilters, graphNode, attr.as<ConvolveMatrixAttributes>());
    } else if (attr.is<DiffuseLightingAttributes>()) {
      status = WrFilterOpSVGFEDiffuseLighting(
          aWrFilters, graphNode, attr.as<DiffuseLightingAttributes>(),
          userspaceOffset);
    } else if (attr.is<DisplacementMapAttributes>()) {
      status = WrFilterOpSVGFEDisplacementMap(
          aWrFilters, graphNode, attr.as<DisplacementMapAttributes>());
    } else if (attr.is<DropShadowAttributes>()) {
      status = WrFilterOpSVGFEDropShadow(aWrFilters, graphNode,
                                         attr.as<DropShadowAttributes>());
    } else if (attr.is<FloodAttributes>()) {
      status = WrFilterOpSVGFEFlood(aWrFilters, graphNode,
                                    attr.as<FloodAttributes>());
    } else if (attr.is<GaussianBlurAttributes>()) {
      status = WrFilterOpSVGFEGaussianBlur(aWrFilters, graphNode,
                                           attr.as<GaussianBlurAttributes>());
    } else if (attr.is<ImageAttributes>()) {
      status = WrFilterOpSVGFEImage(
          aWrFilters, graphNode, attr.as<ImageAttributes>(), userspaceOffset);
    } else if (attr.is<MergeAttributes>()) {
      status = WrFilterOpSVGFEMerge(
          aWrFilters, graphNode, attr.as<MergeAttributes>(),
          instance.mFilterDescription.mPrimitives[i], (int16_t)i,
          sourceGraphicNode, sourceAlphaNode, bufferIdMapping, maxFilters);
    } else if (attr.is<MorphologyAttributes>()) {
      status = WrFilterOpSVGFEMorphology(aWrFilters, graphNode,
                                         attr.as<MorphologyAttributes>());
    } else if (attr.is<OffsetAttributes>()) {
      status = WrFilterOpSVGFEOffset(aWrFilters, graphNode,
                                     attr.as<OffsetAttributes>());
    } else if (attr.is<SpecularLightingAttributes>()) {
      status = WrFilterOpSVGFESpecularLighting(
          aWrFilters, graphNode, attr.as<SpecularLightingAttributes>(),
          userspaceOffset);
    } else if (attr.is<TileAttributes>()) {
      status =
          WrFilterOpSVGFETile(aWrFilters, graphNode, attr.as<TileAttributes>());
    } else if (attr.is<TurbulenceAttributes>()) {
      status = WrFilterOpSVGFETurbulence(aWrFilters, graphNode,
                                         attr.as<TurbulenceAttributes>(),
                                         userspaceOffset);
    } else {
      // Unknown attributes type?
      status = WrFiltersStatus::BLOB_FALLBACK;
    }
    if (status != WrFiltersStatus::SVGFE) {
      break;
    }
    // Set the remapping table entry
    bufferIdMapping[i] = (int16_t)(aWrFilters.filters.Length() - 1);
  }
  if (status != WrFiltersStatus::SVGFE) {
    // If we couldn't handle this graph, clear the filters before returning.
    aWrFilters.filters.Clear();
    aWrFilters.filter_datas.Clear();
    aWrFilters.values.Clear();
    aWrFilters.post_filters_clip = Nothing();
  }
  return status;
}

nsRegion FilterInstance::GetPreFilterNeededArea(
    nsIFrame* aFilteredFrame, const nsTArray<SVGFilterFrame*>& aFilterFrames,
    const nsRegion& aPostFilterDirtyRegion) {
  gfxMatrix tm = SVGUtils::GetCanvasTM(aFilteredFrame);
  auto filterChain = aFilteredFrame->StyleEffects()->mFilters.AsSpan();
  UniquePtr<UserSpaceMetrics> metrics =
      UserSpaceMetricsForFrame(aFilteredFrame);
  // Hardcode InputIsTainted to true because we don't want JS to be able to
  // read the rendered contents of aFilteredFrame.
  FilterInstance instance(aFilteredFrame, aFilteredFrame->GetContent(),
                          *metrics, filterChain, aFilterFrames,
                          /* InputIsTainted */ true, nullptr, tm,
                          &aPostFilterDirtyRegion);
  if (!instance.IsInitialized()) {
    return nsRect();
  }

  // Now we can ask the instance to compute the area of the source
  // that's needed.
  return instance.ComputeSourceNeededRect();
}

Maybe<nsRect> FilterInstance::GetPostFilterBounds(
    nsIFrame* aFilteredFrame, const nsTArray<SVGFilterFrame*>& aFilterFrames,
    const gfxRect* aOverrideBBox, const nsRect* aPreFilterBounds) {
  MOZ_ASSERT(!aFilteredFrame->HasAnyStateBits(NS_FRAME_SVG_LAYOUT) ||
                 !aFilteredFrame->HasAnyStateBits(NS_FRAME_IS_NONDISPLAY),
             "Non-display SVG do not maintain ink overflow rects");

  nsRegion preFilterRegion;
  nsRegion* preFilterRegionPtr = nullptr;
  if (aPreFilterBounds) {
    preFilterRegion = *aPreFilterBounds;
    preFilterRegionPtr = &preFilterRegion;
  }

  gfxMatrix tm = SVGUtils::GetCanvasTM(aFilteredFrame);
  auto filterChain = aFilteredFrame->StyleEffects()->mFilters.AsSpan();
  UniquePtr<UserSpaceMetrics> metrics =
      UserSpaceMetricsForFrame(aFilteredFrame);
  // Hardcode InputIsTainted to true because we don't want JS to be able to
  // read the rendered contents of aFilteredFrame.
  FilterInstance instance(aFilteredFrame, aFilteredFrame->GetContent(),
                          *metrics, filterChain, aFilterFrames,
                          /* InputIsTainted */ true, nullptr, tm, nullptr,
                          preFilterRegionPtr, aPreFilterBounds, aOverrideBBox);
  if (!instance.IsInitialized()) {
    return Nothing();
  }

  return Some(instance.ComputePostFilterExtents());
}

FilterInstance::FilterInstance(
    nsIFrame* aTargetFrame, nsIContent* aTargetContent,
    const UserSpaceMetrics& aMetrics, Span<const StyleFilter> aFilterChain,
    const nsTArray<SVGFilterFrame*>& aFilterFrames, bool aFilterInputIsTainted,
    const SVGFilterPaintCallback& aPaintCallback,
    const gfxMatrix& aPaintTransform, const nsRegion* aPostFilterDirtyRegion,
    const nsRegion* aPreFilterDirtyRegion,
    const nsRect* aPreFilterInkOverflowRectOverride,
    const gfxRect* aOverrideBBox, gfxRect* aFilterSpaceBoundsNotSnapped)
    : mTargetFrame(aTargetFrame),
      mTargetContent(aTargetContent),
      mMetrics(aMetrics),
      mPaintCallback(aPaintCallback),
      mPaintTransform(aPaintTransform),
      mInitialized(false) {
  if (aOverrideBBox) {
    mTargetBBox = *aOverrideBBox;
  } else {
    MOZ_ASSERT(mTargetFrame,
               "Need to supply a frame when there's no aOverrideBBox");
    mTargetBBox =
        SVGUtils::GetBBox(mTargetFrame, SVGUtils::eUseFrameBoundsForOuterSVG |
                                            SVGUtils::eBBoxIncludeFillGeometry);
  }

  // Compute user space to filter space transforms.
  if (!ComputeUserSpaceToFilterSpaceScale()) {
    return;
  }

  if (!ComputeTargetBBoxInFilterSpace()) {
    return;
  }

  // Get various transforms:
  gfxMatrix filterToUserSpace(mFilterSpaceToUserSpaceScale.xScale, 0.0f, 0.0f,
                              mFilterSpaceToUserSpaceScale.yScale, 0.0f, 0.0f);

  mFilterSpaceToFrameSpaceInCSSPxTransform =
      filterToUserSpace * GetUserSpaceToFrameSpaceInCSSPxTransform();
  // mFilterSpaceToFrameSpaceInCSSPxTransform is always invertible
  mFrameSpaceInCSSPxToFilterSpaceTransform =
      mFilterSpaceToFrameSpaceInCSSPxTransform;
  mFrameSpaceInCSSPxToFilterSpaceTransform.Invert();

  nsIntRect targetBounds;
  if (aPreFilterInkOverflowRectOverride) {
    targetBounds = FrameSpaceToFilterSpace(aPreFilterInkOverflowRectOverride);
  } else if (mTargetFrame) {
    nsRect preFilterVOR = mTargetFrame->PreEffectsInkOverflowRect();
    targetBounds = FrameSpaceToFilterSpace(&preFilterVOR);
  }
  mTargetBounds.UnionRect(mTargetBBoxInFilterSpace, targetBounds);

  // Build the filter graph.
  if (NS_FAILED(BuildPrimitives(aFilterChain, aFilterFrames,
                                aFilterInputIsTainted))) {
    return;
  }

  // Convert the passed in rects from frame space to filter space:
  mPostFilterDirtyRegion = FrameSpaceToFilterSpace(aPostFilterDirtyRegion);
  mPreFilterDirtyRegion = FrameSpaceToFilterSpace(aPreFilterDirtyRegion);

  if (aFilterSpaceBoundsNotSnapped) {
    *aFilterSpaceBoundsNotSnapped = mFilterSpaceBoundsNotSnapped;
  }

  mInitialized = true;
}

bool FilterInstance::ComputeTargetBBoxInFilterSpace() {
  gfxRect targetBBoxInFilterSpace = UserSpaceToFilterSpace(mTargetBBox);
  targetBBoxInFilterSpace.RoundOut();

  return gfxUtils::GfxRectToIntRect(targetBBoxInFilterSpace,
                                    &mTargetBBoxInFilterSpace);
}

bool FilterInstance::ComputeUserSpaceToFilterSpaceScale() {
  if (mTargetFrame) {
    mUserSpaceToFilterSpaceScale = mPaintTransform.ScaleFactors();
    if (mUserSpaceToFilterSpaceScale.xScale <= 0.0f ||
        mUserSpaceToFilterSpaceScale.yScale <= 0.0f) {
      // Nothing should be rendered.
      return false;
    }
  } else {
    mUserSpaceToFilterSpaceScale = MatrixScalesDouble();
  }

  mFilterSpaceToUserSpaceScale =
      MatrixScalesDouble(1.0f / mUserSpaceToFilterSpaceScale.xScale,
                         1.0f / mUserSpaceToFilterSpaceScale.yScale);

  return true;
}

gfxRect FilterInstance::UserSpaceToFilterSpace(
    const gfxRect& aUserSpaceRect) const {
  gfxRect filterSpaceRect = aUserSpaceRect;
  filterSpaceRect.Scale(mUserSpaceToFilterSpaceScale);
  return filterSpaceRect;
}

gfxRect FilterInstance::FilterSpaceToUserSpace(
    const gfxRect& aFilterSpaceRect) const {
  gfxRect userSpaceRect = aFilterSpaceRect;
  userSpaceRect.Scale(mFilterSpaceToUserSpaceScale);
  return userSpaceRect;
}

nsresult FilterInstance::BuildPrimitives(
    Span<const StyleFilter> aFilterChain,
    const nsTArray<SVGFilterFrame*>& aFilterFrames,
    bool aFilterInputIsTainted) {
  AutoTArray<FilterPrimitiveDescription, 8> primitiveDescriptions;

  uint32_t filterIndex = 0;

  for (uint32_t i = 0; i < aFilterChain.Length(); i++) {
    if (aFilterChain[i].IsUrl() && aFilterFrames.IsEmpty()) {
      return NS_ERROR_FAILURE;
    }
    auto* filterFrame =
        aFilterChain[i].IsUrl() ? aFilterFrames[filterIndex++] : nullptr;
    bool inputIsTainted = primitiveDescriptions.IsEmpty()
                              ? aFilterInputIsTainted
                              : primitiveDescriptions.LastElement().IsTainted();
    nsresult rv = BuildPrimitivesForFilter(
        aFilterChain[i], filterFrame, inputIsTainted, primitiveDescriptions);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  mFilterDescription = FilterDescription(std::move(primitiveDescriptions));

  return NS_OK;
}

nsresult FilterInstance::BuildPrimitivesForFilter(
    const StyleFilter& aFilter, SVGFilterFrame* aFilterFrame,
    bool aInputIsTainted,
    nsTArray<FilterPrimitiveDescription>& aPrimitiveDescriptions) {
  NS_ASSERTION(mUserSpaceToFilterSpaceScale.xScale > 0.0f &&
                   mFilterSpaceToUserSpaceScale.yScale > 0.0f,
               "scale factors between spaces should be positive values");

  if (aFilter.IsUrl()) {
    // Build primitives for an SVG filter.
    SVGFilterInstance svgFilterInstance(
        aFilter, aFilterFrame, mTargetContent, mMetrics, mTargetBBox,
        mUserSpaceToFilterSpaceScale, mFilterSpaceBoundsNotSnapped);
    if (!svgFilterInstance.IsInitialized()) {
      return NS_ERROR_FAILURE;
    }

    return svgFilterInstance.BuildPrimitives(aPrimitiveDescriptions,
                                             mInputImages, aInputIsTainted);
  }

  // Build primitives for a CSS filter.

  // If we don't have a frame, use opaque black for shadows with unspecified
  // shadow colors.
  nscolor shadowFallbackColor =
      mTargetFrame ? mTargetFrame->StyleText()->mColor.ToColor()
                   : NS_RGB(0, 0, 0);

  CSSFilterInstance cssFilterInstance(aFilter, shadowFallbackColor,
                                      mTargetBounds,
                                      mFrameSpaceInCSSPxToFilterSpaceTransform);
  return cssFilterInstance.BuildPrimitives(aPrimitiveDescriptions,
                                           aInputIsTainted);
}

static void UpdateNeededBounds(const nsIntRegion& aRegion, nsIntRect& aBounds) {
  aBounds = aRegion.GetBounds();

  bool overflow;
  IntSize surfaceSize =
      SVGUtils::ConvertToSurfaceSize(SizeDouble(aBounds.Size()), &overflow);
  if (overflow) {
    aBounds.SizeTo(surfaceSize);
  }
}

void FilterInstance::ComputeNeededBoxes() {
  if (mFilterDescription.mPrimitives.IsEmpty()) {
    return;
  }

  nsIntRegion sourceGraphicNeededRegion;
  nsIntRegion fillPaintNeededRegion;
  nsIntRegion strokePaintNeededRegion;

  FilterSupport::ComputeSourceNeededRegions(
      mFilterDescription, mPostFilterDirtyRegion, sourceGraphicNeededRegion,
      fillPaintNeededRegion, strokePaintNeededRegion);

  sourceGraphicNeededRegion.And(sourceGraphicNeededRegion, mTargetBounds);

  UpdateNeededBounds(sourceGraphicNeededRegion, mSourceGraphic.mNeededBounds);
  UpdateNeededBounds(fillPaintNeededRegion, mFillPaint.mNeededBounds);
  UpdateNeededBounds(strokePaintNeededRegion, mStrokePaint.mNeededBounds);
}

void FilterInstance::BuildSourcePaint(SourceInfo* aSource,
                                      imgDrawingParams& aImgParams) {
  MOZ_ASSERT(mTargetFrame);
  nsIntRect neededRect = aSource->mNeededBounds;
  if (neededRect.IsEmpty()) {
    return;
  }

  RefPtr<DrawTarget> offscreenDT =
      gfxPlatform::GetPlatform()->CreateOffscreenContentDrawTarget(
          neededRect.Size(), SurfaceFormat::B8G8R8A8);
  if (!offscreenDT || !offscreenDT->IsValid()) {
    return;
  }

  gfxContext ctx(offscreenDT);
  gfxContextAutoSaveRestore saver(&ctx);

  ctx.SetMatrixDouble(mPaintTransform *
                      gfxMatrix::Translation(-neededRect.TopLeft()));
  GeneralPattern pattern;
  if (aSource == &mFillPaint) {
    SVGUtils::MakeFillPatternFor(mTargetFrame, &ctx, &pattern, aImgParams);
  } else if (aSource == &mStrokePaint) {
    SVGUtils::MakeStrokePatternFor(mTargetFrame, &ctx, &pattern, aImgParams);
  }

  if (pattern.GetPattern()) {
    offscreenDT->FillRect(
        ToRect(FilterSpaceToUserSpace(ThebesRect(neededRect))), pattern);
  }

  aSource->mSourceSurface = offscreenDT->Snapshot();
  aSource->mSurfaceRect = neededRect;
}

void FilterInstance::BuildSourcePaints(imgDrawingParams& aImgParams) {
  if (!mFillPaint.mNeededBounds.IsEmpty()) {
    BuildSourcePaint(&mFillPaint, aImgParams);
  }

  if (!mStrokePaint.mNeededBounds.IsEmpty()) {
    BuildSourcePaint(&mStrokePaint, aImgParams);
  }
}

void FilterInstance::BuildSourceImage(DrawTarget* aDest,
                                      imgDrawingParams& aImgParams,
                                      FilterNode* aFilter, FilterNode* aSource,
                                      const Rect& aSourceRect) {
  MOZ_ASSERT(mTargetFrame);

  nsIntRect neededRect = mSourceGraphic.mNeededBounds;
  if (neededRect.IsEmpty()) {
    return;
  }

  RefPtr<DrawTarget> offscreenDT;
  SurfaceFormat format = SurfaceFormat::B8G8R8A8;
  if (aDest->CanCreateSimilarDrawTarget(neededRect.Size(), format)) {
    offscreenDT = aDest->CreateSimilarDrawTargetForFilter(
        neededRect.Size(), format, aFilter, aSource, aSourceRect, Point(0, 0));
  }
  if (!offscreenDT || !offscreenDT->IsValid()) {
    return;
  }

  gfxRect r = FilterSpaceToUserSpace(ThebesRect(neededRect));
  r.RoundOut();
  nsIntRect dirty;
  if (!gfxUtils::GfxRectToIntRect(r, &dirty)) {
    return;
  }

  // SVG graphics paint to device space, so we need to set an initial device
  // space to filter space transform on the gfxContext that SourceGraphic
  // and SourceAlpha will paint to.
  //
  // (In theory it would be better to minimize error by having filtered SVG
  // graphics temporarily paint to user space when painting the sources and
  // only set a user space to filter space transform on the gfxContext
  // (since that would eliminate the transform multiplications from user
  // space to device space and back again). However, that would make the
  // code more complex while being hard to get right without introducing
  // subtle bugs, and in practice it probably makes no real difference.)
  gfxContext ctx(offscreenDT);
  gfxMatrix devPxToCssPxTM = SVGUtils::GetCSSPxToDevPxMatrix(mTargetFrame);
  DebugOnly<bool> invertible = devPxToCssPxTM.Invert();
  MOZ_ASSERT(invertible);
  ctx.SetMatrixDouble(devPxToCssPxTM * mPaintTransform *
                      gfxMatrix::Translation(-neededRect.TopLeft()));

  auto imageFlags = aImgParams.imageFlags;
  if (mTargetFrame->HasAnyStateBits(NS_FRAME_IS_NONDISPLAY)) {
    // We're coming from a mask or pattern instance. Patterns
    // are painted into a separate surface and it seems we can't
    // handle the differently sized surface that might be returned
    // with FLAG_HIGH_QUALITY_SCALING
    imageFlags &= ~imgIContainer::FLAG_HIGH_QUALITY_SCALING;
  }
  imgDrawingParams imgParams(imageFlags);
  mPaintCallback(ctx, imgParams, &mPaintTransform, &dirty);
  aImgParams.result = imgParams.result;

  mSourceGraphic.mSourceSurface = offscreenDT->Snapshot();
  mSourceGraphic.mSurfaceRect = neededRect;
}

void FilterInstance::Render(gfxContext* aCtx, imgDrawingParams& aImgParams,
                            float aOpacity) {
  MOZ_ASSERT(mTargetFrame, "Need a frame for rendering");

  if (mFilterDescription.mPrimitives.IsEmpty()) {
    // An filter without any primitive. Treat it as success and paint nothing.
    return;
  }

  nsIntRect filterRect =
      mPostFilterDirtyRegion.GetBounds().Intersect(OutputFilterSpaceBounds());
  if (filterRect.IsEmpty() || mPaintTransform.IsSingular()) {
    return;
  }

  gfxContextMatrixAutoSaveRestore autoSR(aCtx);
  aCtx->SetMatrix(
      aCtx->CurrentMatrix().PreTranslate(filterRect.x, filterRect.y));

  ComputeNeededBoxes();

  Rect renderRect = IntRectToRect(filterRect);
  RefPtr<DrawTarget> dt = aCtx->GetDrawTarget();

  MOZ_ASSERT(dt);
  if (!dt->IsValid()) {
    return;
  }

  BuildSourcePaints(aImgParams);
  RefPtr<FilterNode> sourceGraphic, fillPaint, strokePaint;
  if (mFillPaint.mSourceSurface) {
    fillPaint = FilterWrappers::ForSurface(dt, mFillPaint.mSourceSurface,
                                           mFillPaint.mSurfaceRect.TopLeft());
  }
  if (mStrokePaint.mSourceSurface) {
    strokePaint = FilterWrappers::ForSurface(
        dt, mStrokePaint.mSourceSurface, mStrokePaint.mSurfaceRect.TopLeft());
  }

  // We make the sourceGraphic filter but don't set its inputs until after so
  // that we can make the sourceGraphic size depend on the filter chain
  sourceGraphic = dt->CreateFilter(FilterType::TRANSFORM);
  if (sourceGraphic) {
    // Make sure we set the translation before calling BuildSourceImage
    // so that CreateSimilarDrawTargetForFilter works properly
    IntPoint offset = mSourceGraphic.mNeededBounds.TopLeft();
    sourceGraphic->SetAttribute(ATT_TRANSFORM_MATRIX,
                                Matrix::Translation(offset.x, offset.y));
  }

  RefPtr<FilterNode> resultFilter = FilterNodeGraphFromDescription(
      dt, mFilterDescription, renderRect, sourceGraphic,
      mSourceGraphic.mSurfaceRect, fillPaint, strokePaint, mInputImages);

  if (!resultFilter) {
    gfxWarning() << "Filter is NULL.";
    return;
  }

  BuildSourceImage(dt, aImgParams, resultFilter, sourceGraphic, renderRect);
  if (sourceGraphic) {
    if (mSourceGraphic.mSourceSurface) {
      sourceGraphic->SetInput(IN_TRANSFORM_IN, mSourceGraphic.mSourceSurface);
    } else {
      RefPtr<FilterNode> clear = FilterWrappers::Clear(aCtx->GetDrawTarget());
      sourceGraphic->SetInput(IN_TRANSFORM_IN, clear);
    }
  }

  dt->DrawFilter(resultFilter, renderRect, Point(0, 0), DrawOptions(aOpacity));
}

nsRegion FilterInstance::ComputePostFilterDirtyRegion() {
  if (mPreFilterDirtyRegion.IsEmpty() ||
      mFilterDescription.mPrimitives.IsEmpty()) {
    return nsRegion();
  }

  nsIntRegion resultChangeRegion = FilterSupport::ComputeResultChangeRegion(
      mFilterDescription, mPreFilterDirtyRegion, nsIntRegion(), nsIntRegion());
  return FilterSpaceToFrameSpace(resultChangeRegion);
}

nsRect FilterInstance::ComputePostFilterExtents() {
  if (mFilterDescription.mPrimitives.IsEmpty()) {
    return nsRect();
  }

  nsIntRegion postFilterExtents = FilterSupport::ComputePostFilterExtents(
      mFilterDescription, mTargetBounds);
  return FilterSpaceToFrameSpace(postFilterExtents.GetBounds());
}

nsRect FilterInstance::ComputeSourceNeededRect() {
  ComputeNeededBoxes();
  return FilterSpaceToFrameSpace(mSourceGraphic.mNeededBounds);
}

nsIntRect FilterInstance::OutputFilterSpaceBounds() const {
  uint32_t numPrimitives = mFilterDescription.mPrimitives.Length();
  if (numPrimitives <= 0) {
    return nsIntRect();
  }

  return mFilterDescription.mPrimitives[numPrimitives - 1].PrimitiveSubregion();
}

nsIntRect FilterInstance::FrameSpaceToFilterSpace(const nsRect* aRect) const {
  nsIntRect rect = OutputFilterSpaceBounds();
  if (aRect) {
    if (aRect->IsEmpty()) {
      return nsIntRect();
    }
    gfxRect rectInCSSPx =
        nsLayoutUtils::RectToGfxRect(*aRect, AppUnitsPerCSSPixel());
    gfxRect rectInFilterSpace =
        mFrameSpaceInCSSPxToFilterSpaceTransform.TransformBounds(rectInCSSPx);
    rectInFilterSpace.RoundOut();
    nsIntRect intRect;
    if (gfxUtils::GfxRectToIntRect(rectInFilterSpace, &intRect)) {
      rect = intRect;
    }
  }
  return rect;
}

nsRect FilterInstance::FilterSpaceToFrameSpace(const nsIntRect& aRect) const {
  if (aRect.IsEmpty()) {
    return nsRect();
  }
  gfxRect r(aRect.x, aRect.y, aRect.width, aRect.height);
  r = mFilterSpaceToFrameSpaceInCSSPxTransform.TransformBounds(r);
  // nsLayoutUtils::RoundGfxRectToAppRect rounds out.
  return nsLayoutUtils::RoundGfxRectToAppRect(r, AppUnitsPerCSSPixel());
}

nsIntRegion FilterInstance::FrameSpaceToFilterSpace(
    const nsRegion* aRegion) const {
  if (!aRegion) {
    return OutputFilterSpaceBounds();
  }
  nsIntRegion result;
  for (auto iter = aRegion->RectIter(); !iter.Done(); iter.Next()) {
    // FrameSpaceToFilterSpace rounds out, so this works.
    nsRect rect = iter.Get();
    result.Or(result, FrameSpaceToFilterSpace(&rect));
  }
  return result;
}

nsRegion FilterInstance::FilterSpaceToFrameSpace(
    const nsIntRegion& aRegion) const {
  nsRegion result;
  for (auto iter = aRegion.RectIter(); !iter.Done(); iter.Next()) {
    // FilterSpaceToFrameSpace rounds out, so this works.
    result.Or(result, FilterSpaceToFrameSpace(iter.Get()));
  }
  return result;
}

gfxMatrix FilterInstance::GetUserSpaceToFrameSpaceInCSSPxTransform() const {
  if (!mTargetFrame) {
    return gfxMatrix();
  }
  return gfxMatrix::Translation(
      -SVGUtils::FrameSpaceInCSSPxToUserSpaceOffset(mTargetFrame));
}

}  // namespace mozilla
