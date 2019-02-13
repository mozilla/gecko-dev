/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_ContainerLayerComposite_H
#define GFX_ContainerLayerComposite_H

#include "Layers.h"                     // for Layer (ptr only), etc
#include "mozilla/Attributes.h"         // for override
#include "mozilla/UniquePtr.h"          // for UniquePtr
#include "mozilla/layers/LayerManagerComposite.h"
#include "mozilla/gfx/Rect.h"

namespace mozilla {
namespace layers {

class CompositableHost;
class CompositingRenderTarget;
struct PreparedData;

class ContainerLayerComposite : public ContainerLayer,
                                public LayerComposite
{
  template<class ContainerT>
  friend void ContainerPrepare(ContainerT* aContainer,
                               LayerManagerComposite* aManager,
                               const RenderTargetIntRect& aClipRect);
  template<class ContainerT>
  friend void ContainerRender(ContainerT* aContainer,
                              LayerManagerComposite* aManager,
                              const RenderTargetIntRect& aClipRect);
  template<class ContainerT>
  friend void RenderLayers(ContainerT* aContainer,
                           LayerManagerComposite* aManager,
                           const RenderTargetIntRect& aClipRect);
  template<class ContainerT>
  friend void RenderIntermediate(ContainerT* aContainer,
                   LayerManagerComposite* aManager,
                   const gfx::IntRect& aClipRect,
                   RefPtr<CompositingRenderTarget> surface);
  template<class ContainerT>
  friend RefPtr<CompositingRenderTarget>
  CreateTemporaryTargetAndCopyFromBackground(ContainerT* aContainer,
                                             LayerManagerComposite* aManager,
                                             const RenderTargetIntRect& aClipRect);
  template<class ContainerT>
  friend RefPtr<CompositingRenderTarget>
  CreateOrRecycleTarget(ContainerT* aContainer,
                        LayerManagerComposite* aManager,
                        const RenderTargetIntRect& aClipRect);

public:
  explicit ContainerLayerComposite(LayerManagerComposite *aManager);

protected:
  ~ContainerLayerComposite();

public:
  // LayerComposite Implementation
  virtual Layer* GetLayer() override { return this; }

  virtual void SetLayerManager(LayerManagerComposite* aManager) override
  {
    LayerComposite::SetLayerManager(aManager);
    mManager = aManager;

    for (Layer* l = GetFirstChild(); l; l = l->GetNextSibling()) {
      LayerComposite* child = l->AsLayerComposite();
      child->SetLayerManager(aManager);
    }
  }

  virtual void Destroy() override;

  LayerComposite* GetFirstChildComposite() override;

  virtual void RenderLayer(const gfx::IntRect& aClipRect) override;
  virtual void Prepare(const RenderTargetIntRect& aClipRect) override;

  virtual void ComputeEffectiveTransforms(const gfx::Matrix4x4& aTransformToSurface) override
  {
    DefaultComputeEffectiveTransforms(aTransformToSurface);
  }

  virtual void CleanupResources() override;

  virtual LayerComposite* AsLayerComposite() override { return this; }

  // container layers don't use a compositable
  CompositableHost* GetCompositableHost() override { return nullptr; }

  // If the layer is marked as scale-to-resolution, add a post-scale
  // to the layer's transform equal to the pres shell resolution we're
  // scaling to. This cancels out the post scale of '1 / resolution'
  // added by Layout. TODO: It would be nice to get rid of both of these
  // post-scales.
  virtual float GetPostXScale() const override {
    if (mScaleToResolution) {
      return mPostXScale * mPresShellResolution;
    }
    return mPostXScale;
  }
  virtual float GetPostYScale() const override {
    if (mScaleToResolution) {
      return mPostYScale * mPresShellResolution;
    }
    return mPostYScale;
  }

  virtual const char* Name() const override { return "ContainerLayerComposite"; }
  UniquePtr<PreparedData> mPrepared;

  RefPtr<CompositingRenderTarget> mLastIntermediateSurface;
};

class RefLayerComposite : public RefLayer,
                          public LayerComposite
{
  template<class ContainerT>
  friend void ContainerPrepare(ContainerT* aContainer,
                               LayerManagerComposite* aManager,
                               const RenderTargetIntRect& aClipRect);
  template<class ContainerT>
  friend void ContainerRender(ContainerT* aContainer,
                              LayerManagerComposite* aManager,
                              const gfx::IntRect& aClipRect);
  template<class ContainerT>
  friend void RenderLayers(ContainerT* aContainer,
                           LayerManagerComposite* aManager,
                           const gfx::IntRect& aClipRect);
  template<class ContainerT>
  friend void RenderIntermediate(ContainerT* aContainer,
                   LayerManagerComposite* aManager,
                   const gfx::IntRect& aClipRect,
                   RefPtr<CompositingRenderTarget> surface);
  template<class ContainerT>
  friend RefPtr<CompositingRenderTarget>
  CreateTemporaryTargetAndCopyFromBackground(ContainerT* aContainer,
                                             LayerManagerComposite* aManager,
                                             const gfx::IntRect& aClipRect);
  template<class ContainerT>
  friend RefPtr<CompositingRenderTarget>
  CreateTemporaryTarget(ContainerT* aContainer,
                        LayerManagerComposite* aManager,
                        const gfx::IntRect& aClipRect);

public:
  explicit RefLayerComposite(LayerManagerComposite *aManager);

protected:
  ~RefLayerComposite();

public:
  /** LayerOGL implementation */
  Layer* GetLayer() override { return this; }

  void Destroy() override;

  LayerComposite* GetFirstChildComposite() override;

  virtual void RenderLayer(const gfx::IntRect& aClipRect) override;
  virtual void Prepare(const RenderTargetIntRect& aClipRect) override;

  virtual void ComputeEffectiveTransforms(const gfx::Matrix4x4& aTransformToSurface) override
  {
    DefaultComputeEffectiveTransforms(aTransformToSurface);
  }

  virtual void CleanupResources() override;

  virtual LayerComposite* AsLayerComposite() override { return this; }

  // ref layers don't use a compositable
  CompositableHost* GetCompositableHost() override { return nullptr; }

  virtual const char* Name() const override { return "RefLayerComposite"; }
  UniquePtr<PreparedData> mPrepared;
  RefPtr<CompositingRenderTarget> mLastIntermediateSurface;
};

} /* layers */
} /* mozilla */

#endif /* GFX_ContainerLayerComposite_H */
