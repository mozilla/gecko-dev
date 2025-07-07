/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_FILTERNODEWEBGL_H_
#define MOZILLA_GFX_FILTERNODEWEBGL_H_

#include "mozilla/gfx/Filters.h"
#include "mozilla/gfx/PatternHelpers.h"

#include <vector>

namespace mozilla::gfx {

class DrawTargetWebgl;
class FilterNodeSoftware;

/**
 * FilterNodeWebgl wraps a FilterNodeSoftware for most operations that
 * are not yet accelerated. To provide acceleration, this must be subclassed
 * to override an optimized implementation for particular operations.
 */
class FilterNodeWebgl : public FilterNode {
 public:
  ~FilterNodeWebgl() override;

  FilterBackend GetBackendType() override { return FILTER_BACKEND_WEBGL; }

  void SetInput(uint32_t aIndex, SourceSurface* aSurface) override;
  void SetInput(uint32_t aIndex, FilterNode* aFilter) override;
  void SetAttribute(uint32_t aIndex, bool) override;
  void SetAttribute(uint32_t aIndex, uint32_t) override;
  void SetAttribute(uint32_t aIndex, Float) override;
  void SetAttribute(uint32_t aIndex, const Size&) override;
  void SetAttribute(uint32_t aIndex, const IntSize&) override;
  void SetAttribute(uint32_t aIndex, const IntPoint&) override;
  void SetAttribute(uint32_t aIndex, const Rect&) override;
  void SetAttribute(uint32_t aIndex, const IntRect&) override;
  void SetAttribute(uint32_t aIndex, const Point&) override;
  void SetAttribute(uint32_t aIndex, const Matrix&) override;
  void SetAttribute(uint32_t aIndex, const Matrix5x4&) override;
  void SetAttribute(uint32_t aIndex, const Point3D&) override;
  void SetAttribute(uint32_t aIndex, const DeviceColor&) override;
  void SetAttribute(uint32_t aIndex, const Float* aFloat,
                    uint32_t aSize) override;

  IntRect MapRectToSource(const IntRect& aRect, const IntRect& aMax,
                          FilterNode* aSourceNode) override;

  virtual void Draw(DrawTargetWebgl* aDT, const Rect& aSourceRect,
                    const Point& aDestPoint, const DrawOptions& aOptions);
  virtual already_AddRefed<SourceSurface> DrawChild(
      DrawTargetWebgl* aDT, const Rect& aSourceRect,
      IntPoint* aSurfaceOffset = nullptr);

  virtual void ResolveInputs(DrawTargetWebgl* aDT, bool aAccel) {}

  void ResolveAllInputs(DrawTargetWebgl* aDT);

 protected:
  std::vector<RefPtr<FilterNodeWebgl>> mInputFilters;
  std::vector<RefPtr<SourceSurface>> mInputSurfaces;
  FilterType mType;
  RefPtr<FilterNodeSoftware> mSoftwareFilter;

  friend class DrawTargetWebgl;

  explicit FilterNodeWebgl(FilterType aType);

  static already_AddRefed<FilterNodeWebgl> Create(FilterType aType);

  size_t NumberOfSetInputs() {
    return std::max(mInputSurfaces.size(), mInputFilters.size());
  }

  virtual int32_t InputIndex(uint32_t aInputEnumIndex) { return -1; }

  IntRect MapInputRectToSource(uint32_t aInputEnumIndex, const IntRect& aRect,
                               const IntRect& aMax, FilterNode* aSourceNode);
};

class FilterNodeCropWebgl : public FilterNodeWebgl {
 public:
  FilterNodeCropWebgl() : FilterNodeWebgl(FilterType::CROP) {}

  void SetAttribute(uint32_t aIndex, const Rect& aValue) override;
  IntRect MapRectToSource(const IntRect& aRect, const IntRect& aMax,
                          FilterNode* aSourceNode) override;

  void Draw(DrawTargetWebgl* aDT, const Rect& aSourceRect,
            const Point& aDestPoint, const DrawOptions& aOptions) override;

 private:
  IntRect mCropRect;

  int32_t InputIndex(uint32_t aInputEnumIndex) override;
};

class FilterNodeTransformWebgl : public FilterNodeWebgl {
 public:
  FilterNodeTransformWebgl() : FilterNodeWebgl(FilterType::TRANSFORM) {}

  void SetAttribute(uint32_t aIndex, uint32_t aValue) override;
  void SetAttribute(uint32_t aIndex, const Matrix& aValue) override;
  IntRect MapRectToSource(const IntRect& aRect, const IntRect& aMax,
                          FilterNode* aSourceNode) override;

  void Draw(DrawTargetWebgl* aDT, const Rect& aSourceRect,
            const Point& aDestPoint, const DrawOptions& aOptions) override;
  already_AddRefed<SourceSurface> DrawChild(DrawTargetWebgl* aDT,
                                            const Rect& aSourceRect,
                                            IntPoint* aSurfaceOffset) override;

 protected:
  Matrix mMatrix;
  SamplingFilter mSamplingFilter = SamplingFilter::GOOD;

  int32_t InputIndex(uint32_t aInputEnumIndex) override;
};

class FilterNodeDeferInputWebgl : public FilterNodeTransformWebgl {
 public:
  FilterNodeDeferInputWebgl(RefPtr<Path> aPath, const Pattern& aPattern,
                            const IntRect& aSourceRect,
                            const Matrix& aDestTransform,
                            const DrawOptions& aOptions,
                            const StrokeOptions* aStrokeOptions);

  void ResolveInputs(DrawTargetWebgl* aDT, bool aAccel) override;

 private:
  RefPtr<Path> mPath;
  GeneralPattern mPattern;
  IntRect mSourceRect;
  Matrix mDestTransform;
  DrawOptions mOptions;
  Maybe<StrokeOptions> mStrokeOptions;
  UniquePtr<Float[]> mDashPatternStorage;
};

class FilterNodeGaussianBlurWebgl : public FilterNodeWebgl {
 public:
  FilterNodeGaussianBlurWebgl() : FilterNodeWebgl(FilterType::GAUSSIAN_BLUR) {}

  void SetAttribute(uint32_t aIndex, float aValue) override;
  IntRect MapRectToSource(const IntRect& aRect, const IntRect& aMax,
                          FilterNode* aSourceNode) override;

  void Draw(DrawTargetWebgl* aDT, const Rect& aSourceRect,
            const Point& aDestPoint, const DrawOptions& aOptions) override;

 private:
  float mStdDeviation = 0;

  int32_t InputIndex(uint32_t aInputEnumIndex) override;
};

}  // namespace mozilla::gfx

#endif /* MOZILLA_GFX_FILTERNODEWEBGL_H_ */
