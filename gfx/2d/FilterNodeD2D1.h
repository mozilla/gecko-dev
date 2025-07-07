/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_FILTERNODED2D1_H_
#define MOZILLA_GFX_FILTERNODED2D1_H_

#include "2D.h"
#include "Filters.h"
#include <vector>
#include <windows.h>
#include <d2d1_1.h>
#include <cguid.h>

namespace mozilla {
namespace gfx {

class FilterNodeD2D1 : public FilterNode {
 public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(FilterNodeD2D1, override)

  static already_AddRefed<FilterNode> Create(ID2D1DeviceContext* aDC,
                                             FilterType aType);

  FilterNodeD2D1(ID2D1Effect* aEffect, FilterType aType)
      : mEffect(aEffect), mType(aType) {
    InitUnmappedProperties();
  }

  virtual FilterBackend GetBackendType() { return FILTER_BACKEND_DIRECT2D1_1; }

  virtual void SetInput(uint32_t aIndex, SourceSurface* aSurface);
  virtual void SetInput(uint32_t aIndex, FilterNode* aFilter);

  virtual void SetAttribute(uint32_t aIndex, uint32_t aValue);
  virtual void SetAttribute(uint32_t aIndex, Float aValue);
  virtual void SetAttribute(uint32_t aIndex, const Point& aValue);
  virtual void SetAttribute(uint32_t aIndex, const Matrix5x4& aValue);
  virtual void SetAttribute(uint32_t aIndex, const Point3D& aValue);
  virtual void SetAttribute(uint32_t aIndex, const Size& aValue);
  virtual void SetAttribute(uint32_t aIndex, const IntSize& aValue);
  virtual void SetAttribute(uint32_t aIndex, const DeviceColor& aValue);
  virtual void SetAttribute(uint32_t aIndex, const Rect& aValue);
  virtual void SetAttribute(uint32_t aIndex, const IntRect& aValue);
  virtual void SetAttribute(uint32_t aIndex, bool aValue);
  virtual void SetAttribute(uint32_t aIndex, const Float* aValues,
                            uint32_t aSize);
  virtual void SetAttribute(uint32_t aIndex, const IntPoint& aValue);
  virtual void SetAttribute(uint32_t aIndex, const Matrix& aValue);

  // Called by DrawTarget before it draws our OutputEffect, and recursively
  // by the filter nodes that have this filter as one of their inputs. This
  // gives us a chance to convert any input surfaces to the target format for
  // the DrawTarget that we will draw to.
  virtual void WillDraw(DrawTarget* aDT);

  virtual ID2D1Effect* MainEffect() { return mEffect.get(); }
  virtual ID2D1Effect* InputEffect() { return mEffect.get(); }
  virtual ID2D1Effect* OutputEffect() { return mEffect.get(); }

 protected:
  friend class DrawTargetD2D1;
  friend class DrawTargetD2D;

  void InitUnmappedProperties();

  RefPtr<ID2D1Effect> mEffect;
  std::vector<RefPtr<FilterNodeD2D1>> mInputFilters;
  std::vector<RefPtr<SourceSurface>> mInputSurfaces;
  FilterType mType;

 private:
  using FilterNode::SetAttribute;
  using FilterNode::SetInput;
};

// Both ConvolveMatrix and Lighting filters have an interaction of edge mode and
// source rect that is a bit tricky with D2D1 effects. We want the edge mode to
// only apply outside of the render rect. So if our input surface or filter is
// smaller than the render rect, we need to add transparency around it until we
// reach the edges of the render rect, and only then do any repeating or edge
// duplicating.  Unfortunately, the border effect does not have a render rect
// attribute - it only looks at the output rect of its input filter or
// surface. So we use our custom ExtendInput effect to adjust the output rect of
// our input.  All of this is only necessary when our edge mode is not
// EDGE_MODE_NONE, so we update the filter chain dynamically in UpdateChain().

class FilterNodeRenderRectD2D1 : public FilterNodeD2D1 {
 public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(FilterNodeRenderRectD2D1, override)
  FilterNodeRenderRectD2D1(ID2D1DeviceContext* aDC, FilterType aType);

  void SetInput(uint32_t aIndex, FilterNode* aFilter) override;

 protected:
  virtual void UpdateChain() = 0;
  void UpdateRenderRect();

  RefPtr<ID2D1Effect> mExtendInputEffect;
  RefPtr<ID2D1Effect> mBorderEffect;
  IntRect mRenderRect;
};

class FilterNodeLightingD2D1 : public FilterNodeRenderRectD2D1 {
 public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(FilterNodeLightingD2D1, override)
  FilterNodeLightingD2D1(ID2D1DeviceContext* aDC, FilterType aType);

  void SetAttribute(uint32_t aIndex, const IntRect& aValue) override;

  ID2D1Effect* InputEffect() override;

 private:
  void UpdateChain() override;
};

class FilterNodeConvolveD2D1 : public FilterNodeRenderRectD2D1 {
 public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(FilterNodeConvolveD2D1, override)
  explicit FilterNodeConvolveD2D1(ID2D1DeviceContext* aDC);

  void SetAttribute(uint32_t aIndex, uint32_t aValue) override;
  void SetAttribute(uint32_t aIndex, const IntSize& aValue) override;
  void SetAttribute(uint32_t aIndex, const IntPoint& aValue) override;
  void SetAttribute(uint32_t aIndex, const IntRect& aValue) override;

  ID2D1Effect* InputEffect() override;

 private:
  using FilterNode::SetAttribute;
  using FilterNode::SetInput;

  void UpdateChain() override;
  void UpdateOffset();

  ConvolveMatrixEdgeMode mEdgeMode;
  IntPoint mTarget;
  IntSize mKernelSize;
};

class FilterNodeOpacityD2D1 : public FilterNodeD2D1 {
 public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(FilterNodeOpacityD2D1, override)
  FilterNodeOpacityD2D1(ID2D1Effect* aEffect, FilterType aType)
      : FilterNodeD2D1(aEffect, aType) {}

  void SetAttribute(uint32_t aIndex, Float aValue) override;
};

class FilterNodeExtendInputAdapterD2D1 : public FilterNodeD2D1 {
 public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(FilterNodeExtendInputAdapterD2D1,
                                          override)
  FilterNodeExtendInputAdapterD2D1(ID2D1DeviceContext* aDC,
                                   FilterNodeD2D1* aFilterNode,
                                   FilterType aType);

  ID2D1Effect* InputEffect() override { return mExtendInputEffect.get(); }
  ID2D1Effect* OutputEffect() override {
    return mWrappedFilterNode->OutputEffect();
  }

 private:
  RefPtr<FilterNodeD2D1> mWrappedFilterNode;
  RefPtr<ID2D1Effect> mExtendInputEffect;
};

class FilterNodePremultiplyAdapterD2D1 : public FilterNodeD2D1 {
 public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(FilterNodePremultiplyAdapterD2D1,
                                          override)
  FilterNodePremultiplyAdapterD2D1(ID2D1DeviceContext* aDC,
                                   FilterNodeD2D1* aFilterNode,
                                   FilterType aType);

  ID2D1Effect* InputEffect() override { return mPrePremultiplyEffect.get(); }
  ID2D1Effect* OutputEffect() override {
    return mPostUnpremultiplyEffect.get();
  }

 private:
  RefPtr<ID2D1Effect> mPrePremultiplyEffect;
  RefPtr<ID2D1Effect> mPostUnpremultiplyEffect;
};

}  // namespace gfx
}  // namespace mozilla

#endif
