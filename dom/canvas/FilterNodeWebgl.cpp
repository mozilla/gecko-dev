/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DrawTargetWebglInternal.h"
#include "FilterNodeWebgl.h"
#include "SourceSurfaceWebgl.h"
#include "mozilla/gfx/Blur.h"
#include "mozilla/gfx/DrawTargetSkia.h"
#include "mozilla/gfx/FilterNodeSoftware.h"
#include "mozilla/gfx/Helpers.h"
#include "mozilla/gfx/Logging.h"

namespace mozilla::gfx {

FilterNodeWebgl::FilterNodeWebgl(FilterType aType)
    : mType(aType),
      mSoftwareFilter(
          FilterNodeSoftware::Create(aType).downcast<FilterNodeSoftware>()) {}

FilterNodeWebgl::~FilterNodeWebgl() = default;

already_AddRefed<FilterNodeWebgl> FilterNodeWebgl::Create(FilterType aType) {
  RefPtr<FilterNodeWebgl> filter;
  switch (aType) {
    case FilterType::CROP:
      filter = new FilterNodeCropWebgl;
      break;
    case FilterType::TRANSFORM:
      filter = new FilterNodeTransformWebgl;
      break;
    case FilterType::GAUSSIAN_BLUR:
      filter = new FilterNodeGaussianBlurWebgl;
      break;
    default:
      filter = new FilterNodeWebgl(aType);
      break;
  }
  return filter.forget();
}

void FilterNodeWebgl::SetInput(uint32_t aIndex, SourceSurface* aSurface) {
  if (mInputSurfaces.size() <= aIndex) {
    mInputSurfaces.resize(aIndex + 1);
  }
  if (mInputFilters.size() <= aIndex) {
    mInputFilters.resize(aIndex + 1);
  }
  mInputSurfaces[aIndex] = aSurface;
  mInputFilters[aIndex] = nullptr;
  if (mSoftwareFilter) {
    mSoftwareFilter->SetInput(aIndex, aSurface);
  }
}

void FilterNodeWebgl::SetInput(uint32_t aIndex, FilterNode* aFilter) {
  if (aFilter && aFilter->GetBackendType() != FILTER_BACKEND_WEBGL) {
    MOZ_ASSERT(false, "FilterNodeWebgl required as input");
    return;
  }

  if (mInputFilters.size() <= aIndex) {
    mInputFilters.resize(aIndex + 1);
  }
  if (mInputSurfaces.size() <= aIndex) {
    mInputSurfaces.resize(aIndex + 1);
  }
  auto* webglFilter = static_cast<FilterNodeWebgl*>(aFilter);
  mInputFilters[aIndex] = webglFilter;
  mInputSurfaces[aIndex] = nullptr;
  if (mSoftwareFilter) {
    MOZ_ASSERT(!webglFilter || webglFilter->mSoftwareFilter);
    mSoftwareFilter->SetInput(
        aIndex, webglFilter ? webglFilter->mSoftwareFilter.get() : nullptr);
  }
}

void FilterNodeWebgl::SetAttribute(uint32_t aIndex, bool aValue) {
  if (mSoftwareFilter) {
    mSoftwareFilter->SetAttribute(aIndex, aValue);
  }
}

void FilterNodeWebgl::SetAttribute(uint32_t aIndex, uint32_t aValue) {
  if (mSoftwareFilter) {
    mSoftwareFilter->SetAttribute(aIndex, aValue);
  }
}

void FilterNodeWebgl::SetAttribute(uint32_t aIndex, Float aValue) {
  if (mSoftwareFilter) {
    mSoftwareFilter->SetAttribute(aIndex, aValue);
  }
}

void FilterNodeWebgl::SetAttribute(uint32_t aIndex, const Size& aValue) {
  if (mSoftwareFilter) {
    mSoftwareFilter->SetAttribute(aIndex, aValue);
  }
}

void FilterNodeWebgl::SetAttribute(uint32_t aIndex, const IntSize& aValue) {
  if (mSoftwareFilter) {
    mSoftwareFilter->SetAttribute(aIndex, aValue);
  }
}

void FilterNodeWebgl::SetAttribute(uint32_t aIndex, const IntPoint& aValue) {
  if (mSoftwareFilter) {
    mSoftwareFilter->SetAttribute(aIndex, aValue);
  }
}

void FilterNodeWebgl::SetAttribute(uint32_t aIndex, const Rect& aValue) {
  if (mSoftwareFilter) {
    mSoftwareFilter->SetAttribute(aIndex, aValue);
  }
}

void FilterNodeWebgl::SetAttribute(uint32_t aIndex, const IntRect& aValue) {
  if (mSoftwareFilter) {
    mSoftwareFilter->SetAttribute(aIndex, aValue);
  }
}

void FilterNodeWebgl::SetAttribute(uint32_t aIndex, const Point& aValue) {
  if (mSoftwareFilter) {
    mSoftwareFilter->SetAttribute(aIndex, aValue);
  }
}

void FilterNodeWebgl::SetAttribute(uint32_t aIndex, const Matrix& aValue) {
  if (mSoftwareFilter) {
    mSoftwareFilter->SetAttribute(aIndex, aValue);
  }
}

void FilterNodeWebgl::SetAttribute(uint32_t aIndex, const Matrix5x4& aValue) {
  if (mSoftwareFilter) {
    mSoftwareFilter->SetAttribute(aIndex, aValue);
  }
}

void FilterNodeWebgl::SetAttribute(uint32_t aIndex, const Point3D& aValue) {
  if (mSoftwareFilter) {
    mSoftwareFilter->SetAttribute(aIndex, aValue);
  }
}

void FilterNodeWebgl::SetAttribute(uint32_t aIndex, const DeviceColor& aValue) {
  if (mSoftwareFilter) {
    mSoftwareFilter->SetAttribute(aIndex, aValue);
  }
}

void FilterNodeWebgl::SetAttribute(uint32_t aIndex, const Float* aValues,
                                   uint32_t aSize) {
  if (mSoftwareFilter) {
    mSoftwareFilter->SetAttribute(aIndex, aValues, aSize);
  }
}

IntRect FilterNodeWebgl::MapRectToSource(const IntRect& aRect,
                                         const IntRect& aMax,
                                         FilterNode* aSourceNode) {
  if (mSoftwareFilter) {
    return mSoftwareFilter->MapRectToSource(aRect, aMax, aSourceNode);
  }
  return aMax;
}

void FilterNodeWebgl::Draw(DrawTargetWebgl* aDT, const Rect& aSourceRect,
                           const Point& aDestPoint,
                           const DrawOptions& aOptions) {
  ResolveAllInputs(aDT);

  MOZ_ASSERT(mSoftwareFilter);
  aDT->DrawFilterFallback(mSoftwareFilter, aSourceRect, aDestPoint, aOptions);
}

already_AddRefed<SourceSurface> FilterNodeWebgl::DrawChild(
    DrawTargetWebgl* aDT, const Rect& aSourceRect, IntPoint* aSurfaceOffset) {
  ResolveAllInputs(aDT);

  MOZ_ASSERT(mSoftwareFilter);
  RefPtr<DrawTarget> swDT = aDT->mSkia->CreateSimilarDrawTarget(
      IntSize::Ceil(aSourceRect.Size()), aDT->GetFormat());
  if (!swDT) {
    return nullptr;
  }
  swDT->DrawFilter(mSoftwareFilter, aSourceRect, Point(0, 0));
  return swDT->Snapshot();
}

IntRect FilterNodeWebgl::MapInputRectToSource(uint32_t aInputEnumIndex,
                                              const IntRect& aRect,
                                              const IntRect& aMax,
                                              FilterNode* aSourceNode) {
  int32_t inputIndex = InputIndex(aInputEnumIndex);
  if (inputIndex < 0) {
    gfxDevCrash(LogReason::FilterInputError)
        << "Invalid input " << inputIndex << " vs. " << NumberOfSetInputs();
    return aMax;
  }
  if ((uint32_t)inputIndex < NumberOfSetInputs()) {
    if (RefPtr<FilterNodeWebgl> filter = mInputFilters[inputIndex]) {
      return filter->MapRectToSource(aRect, aMax, aSourceNode);
    }
  }
  if (this == aSourceNode) {
    return aRect;
  }
  return IntRect();
}

void FilterNodeWebgl::ResolveAllInputs(DrawTargetWebgl* aDT) {
  for (const auto& filter : mInputFilters) {
    if (filter) {
      filter->ResolveInputs(aDT, false);
      filter->ResolveAllInputs(aDT);
    }
  }
}

int32_t FilterNodeCropWebgl::InputIndex(uint32_t aInputEnumIndex) {
  switch (aInputEnumIndex) {
    case IN_CROP_IN:
      return 0;
    default:
      return -1;
  }
}

void FilterNodeCropWebgl::SetAttribute(uint32_t aIndex, const Rect& aValue) {
  MOZ_ASSERT(aIndex == ATT_CROP_RECT);
  Rect srcRect = aValue;
  srcRect.Round();
  if (!srcRect.ToIntRect(&mCropRect)) {
    mCropRect = IntRect();
  }
  FilterNodeWebgl::SetAttribute(aIndex, aValue);
}

IntRect FilterNodeCropWebgl::MapRectToSource(const IntRect& aRect,
                                             const IntRect& aMax,
                                             FilterNode* aSourceNode) {
  return MapInputRectToSource(IN_CROP_IN, aRect.Intersect(mCropRect), aMax,
                              aSourceNode);
}

void FilterNodeCropWebgl::Draw(DrawTargetWebgl* aDT, const Rect& aSourceRect,
                               const Point& aDestPoint,
                               const DrawOptions& aOptions) {
  ResolveInputs(aDT, true);

  uint32_t inputIdx = InputIndex(IN_CROP_IN);
  if (inputIdx < NumberOfSetInputs()) {
    Rect croppedSource = aSourceRect.Intersect(Rect(mCropRect));
    if (RefPtr<FilterNodeWebgl> filter = mInputFilters[inputIdx]) {
      filter->Draw(aDT, croppedSource,
                   aDestPoint + croppedSource.TopLeft() - aSourceRect.TopLeft(),
                   aOptions);
    } else if (RefPtr<SourceSurface> surface = mInputSurfaces[inputIdx]) {
      aDT->DrawSurface(surface,
                       croppedSource - aSourceRect.TopLeft() + aDestPoint,
                       croppedSource, DrawSurfaceOptions(), aOptions);
    }
  }
}

int32_t FilterNodeTransformWebgl::InputIndex(uint32_t aInputEnumIndex) {
  switch (aInputEnumIndex) {
    case IN_TRANSFORM_IN:
      return 0;
    default:
      return -1;
  }
}

void FilterNodeTransformWebgl::SetAttribute(uint32_t aIndex, uint32_t aValue) {
  MOZ_ASSERT(aIndex == ATT_TRANSFORM_FILTER);
  mSamplingFilter = static_cast<SamplingFilter>(aValue);
  FilterNodeWebgl::SetAttribute(aIndex, aValue);
}

void FilterNodeTransformWebgl::SetAttribute(uint32_t aIndex,
                                            const Matrix& aValue) {
  MOZ_ASSERT(aIndex == ATT_TRANSFORM_MATRIX);
  mMatrix = aValue;
  FilterNodeWebgl::SetAttribute(aIndex, aValue);
}

IntRect FilterNodeTransformWebgl::MapRectToSource(const IntRect& aRect,
                                                  const IntRect& aMax,
                                                  FilterNode* aSourceNode) {
  if (aRect.IsEmpty()) {
    return IntRect();
  }
  Matrix inv(mMatrix);
  if (!inv.Invert()) {
    return aMax;
  }
  Rect rect = inv.TransformBounds(Rect(aRect));
  rect.RoundOut();
  IntRect intRect;
  if (!rect.ToIntRect(&intRect)) {
    return aMax;
  }
  return MapInputRectToSource(IN_TRANSFORM_IN, intRect, aMax, aSourceNode);
}

void FilterNodeTransformWebgl::Draw(DrawTargetWebgl* aDT,
                                    const Rect& aSourceRect,
                                    const Point& aDestPoint,
                                    const DrawOptions& aOptions) {
  ResolveInputs(aDT, true);

  uint32_t inputIdx = InputIndex(IN_TRANSFORM_IN);
  if (inputIdx < NumberOfSetInputs()) {
    if (mMatrix.IsTranslation()) {
      if (RefPtr<FilterNodeWebgl> filter = mInputFilters[inputIdx]) {
        filter->Draw(aDT, aSourceRect - mMatrix.GetTranslation(), aDestPoint,
                     aOptions);
      } else if (RefPtr<SourceSurface> surface = mInputSurfaces[inputIdx]) {
        aDT->DrawSurface(surface, Rect(aDestPoint, aSourceRect.Size()),
                         aSourceRect - mMatrix.GetTranslation(),
                         DrawSurfaceOptions(mSamplingFilter), aOptions);
      }
    } else {
      AutoRestoreTransform restore(aDT);
      aDT->PushClipRect(Rect(aDestPoint, aSourceRect.Size()));
      aDT->ConcatTransform(
          mMatrix * Matrix::Translation(aDestPoint - aSourceRect.TopLeft()));
      Matrix inv = mMatrix;
      if (inv.Invert()) {
        Rect invRect = inv.TransformBounds(aSourceRect);
        if (RefPtr<FilterNodeWebgl> filter = mInputFilters[inputIdx]) {
          if (RefPtr<SourceSurface> surface = filter->DrawChild(aDT, invRect)) {
            Rect surfRect(surface->GetRect());
            aDT->DrawSurface(surface, Rect(invRect.TopLeft(), surfRect.Size()),
                             surfRect, DrawSurfaceOptions(mSamplingFilter),
                             aOptions);
          }
        } else if (RefPtr<SourceSurface> surface = mInputSurfaces[inputIdx]) {
          Rect surfRect = Rect(surface->GetRect()).Intersect(invRect);
          aDT->DrawSurface(surface, surfRect, surfRect,
                           DrawSurfaceOptions(mSamplingFilter), aOptions);
        }
      }
      aDT->PopClip();
    }
  }
}

already_AddRefed<SourceSurface> FilterNodeTransformWebgl::DrawChild(
    DrawTargetWebgl* aDT, const Rect& aSourceRect, IntPoint* aSurfaceOffset) {
  ResolveInputs(aDT, true);

  uint32_t inputIdx = InputIndex(IN_TRANSFORM_IN);
  if (inputIdx < NumberOfSetInputs()) {
    if (aSurfaceOffset && mMatrix.IsIntegerTranslation()) {
      if (RefPtr<SourceSurface> surface = mInputSurfaces[inputIdx]) {
        *aSurfaceOffset = RoundedToInt(mMatrix.GetTranslation());
        return surface.forget();
      }
    }
    return FilterNodeWebgl::DrawChild(aDT, aSourceRect, aSurfaceOffset);
  }
  return nullptr;
}

FilterNodeDeferInputWebgl::FilterNodeDeferInputWebgl(
    RefPtr<Path> aPath, const Pattern& aPattern, const IntRect& aSourceRect,
    const Matrix& aDestTransform, const DrawOptions& aOptions,
    const StrokeOptions* aStrokeOptions)
    : mPath(std::move(aPath)),
      mSourceRect(aSourceRect),
      mDestTransform(aDestTransform),
      mOptions(aOptions) {
  mPattern.Init(aPattern);
  if (aStrokeOptions) {
    mStrokeOptions = Some(*aStrokeOptions);
    if (aStrokeOptions->mDashLength > 0) {
      mDashPatternStorage.reset(new Float[aStrokeOptions->mDashLength]);
      PodCopy(mDashPatternStorage.get(), aStrokeOptions->mDashPattern,
              aStrokeOptions->mDashLength);
      mStrokeOptions->mDashPattern = mDashPatternStorage.get();
    }
  }
  SetAttribute(ATT_TRANSFORM_MATRIX,
               Matrix::Translation(mSourceRect.TopLeft()));
}

void FilterNodeDeferInputWebgl::ResolveInputs(DrawTargetWebgl* aDT,
                                              bool aAccel) {
  if (!mInputSurfaces.empty()) {
    return;
  }
  RefPtr<SourceSurface> surface;
  if (aAccel) {
    surface = aDT->ResolveFilterInputAccel(
        mPath, *mPattern.GetPattern(), mSourceRect, mDestTransform, mOptions,
        mStrokeOptions.ptrOr(nullptr));
  }
  if (!surface) {
    surface = aDT->mSkia->ResolveFilterInput(
        mPath, *mPattern.GetPattern(), mSourceRect, mDestTransform, mOptions,
        mStrokeOptions.ptrOr(nullptr));
  }
  SetInput(InputIndex(IN_TRANSFORM_IN), surface);
}

int32_t FilterNodeGaussianBlurWebgl::InputIndex(uint32_t aInputEnumIndex) {
  switch (aInputEnumIndex) {
    case IN_GAUSSIAN_BLUR_IN:
      return 0;
    default:
      return -1;
  }
}

void FilterNodeGaussianBlurWebgl::SetAttribute(uint32_t aIndex, float aValue) {
  MOZ_ASSERT(aIndex == ATT_GAUSSIAN_BLUR_STD_DEVIATION);
  // Match the FilterNodeSoftware blur limit.
  mStdDeviation = std::clamp(aValue, 0.0f, 100.0f);
  FilterNodeWebgl::SetAttribute(aIndex, aValue);
}

IntRect FilterNodeGaussianBlurWebgl::MapRectToSource(const IntRect& aRect,
                                                     const IntRect& aMax,
                                                     FilterNode* aSourceNode) {
  return MapInputRectToSource(IN_GAUSSIAN_BLUR_IN, aRect, aMax, aSourceNode);
}

void FilterNodeGaussianBlurWebgl::Draw(DrawTargetWebgl* aDT,
                                       const Rect& aSourceRect,
                                       const Point& aDestPoint,
                                       const DrawOptions& aOptions) {
  ResolveInputs(aDT, true);

  uint32_t inputIdx = InputIndex(IN_GAUSSIAN_BLUR_IN);
  if (inputIdx < NumberOfSetInputs()) {
    bool success = false;
    IntPoint surfaceOffset;
    if (RefPtr<SourceSurface> surface =
            mInputFilters[inputIdx] ? mInputFilters[inputIdx]->DrawChild(
                                          aDT, aSourceRect, &surfaceOffset)
                                    : mInputSurfaces[inputIdx]) {
      aDT->PushClipRect(Rect(aDestPoint, aSourceRect.Size()));
      IntRect surfRect =
          RoundedOut(Rect(surface->GetRect())
                         .Intersect(aSourceRect - Point(surfaceOffset)));
      Point destOffset =
          Point(surfRect.TopLeft() + surfaceOffset) - aSourceRect.TopLeft();
      success = surfRect.IsEmpty() ||
                aDT->BlurSurface(mStdDeviation, surface, surfRect,
                                 aDestPoint + destOffset, aOptions);
      aDT->PopClip();
    }
    if (!success) {
      FilterNodeWebgl::Draw(aDT, aSourceRect, aDestPoint, aOptions);
    }
  }
}

}  // namespace mozilla::gfx
