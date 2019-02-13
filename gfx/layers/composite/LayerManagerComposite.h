/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_LayerManagerComposite_H
#define GFX_LayerManagerComposite_H

#include <stdint.h>                     // for int32_t, uint32_t
#include "GLDefs.h"                     // for GLenum
#include "Layers.h"
#include "Units.h"                      // for ParentLayerIntRect
#include "mozilla/Assertions.h"         // for MOZ_ASSERT, etc
#include "mozilla/Attributes.h"         // for override
#include "mozilla/RefPtr.h"             // for RefPtr, TemporaryRef
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/Point.h"          // for IntSize
#include "mozilla/gfx/Rect.h"           // for Rect
#include "mozilla/gfx/Types.h"          // for SurfaceFormat
#include "mozilla/layers/CompositorTypes.h"
#include "mozilla/layers/LayersTypes.h"  // for LayersBackend, etc
#include "mozilla/Maybe.h"              // for Maybe
#include "mozilla/RefPtr.h"
#include "mozilla/UniquePtr.h"
#include "nsAString.h"
#include "nsRefPtr.h"                   // for nsRefPtr
#include "nsCOMPtr.h"                   // for already_AddRefed
#include "nsDebug.h"                    // for NS_ASSERTION
#include "nsISupportsImpl.h"            // for Layer::AddRef, etc
#include "nsRect.h"                     // for mozilla::gfx::IntRect
#include "nsRegion.h"                   // for nsIntRegion
#include "nscore.h"                     // for nsAString, etc
#include "LayerTreeInvalidation.h"

class gfxContext;

#ifdef XP_WIN
#include <windows.h>
#endif

namespace mozilla {
namespace gfx {
class DrawTarget;
}

namespace layers {

class CanvasLayerComposite;
class ColorLayerComposite;
class CompositableHost;
class Compositor;
class ContainerLayerComposite;
struct EffectChain;
class ImageLayer;
class ImageLayerComposite;
class LayerComposite;
class RefLayerComposite;
class PaintedLayerComposite;
class TiledLayerComposer;
class TextRenderer;
class CompositingRenderTarget;
struct FPSState;

static const int kVisualWarningDuration = 150; // ms

class LayerManagerComposite final : public LayerManager
{
  typedef mozilla::gfx::DrawTarget DrawTarget;
  typedef mozilla::gfx::IntSize IntSize;
  typedef mozilla::gfx::SurfaceFormat SurfaceFormat;

public:
  explicit LayerManagerComposite(Compositor* aCompositor);
  ~LayerManagerComposite();

  virtual void Destroy() override;

  /**
   * return True if initialization was succesful, false when it was not.
   */
  bool Initialize();

  /**
   * Sets the clipping region for this layer manager. This is important on
   * windows because using OGL we no longer have GDI's native clipping. Therefor
   * widget must tell us what part of the screen is being invalidated,
   * and we should clip to this.
   *
   * \param aClippingRegion Region to clip to. Setting an empty region
   * will disable clipping.
   */
  void SetClippingRegion(const nsIntRegion& aClippingRegion)
  {
    mClippingRegion = aClippingRegion;
  }

  /**
   * LayerManager implementation.
   */
  virtual LayerManagerComposite* AsLayerManagerComposite() override
  {
    return this;
  }

  void UpdateRenderBounds(const gfx::IntRect& aRect);

  virtual void BeginTransaction() override;
  virtual void BeginTransactionWithTarget(gfxContext* aTarget) override
  {
    MOZ_CRASH("Use BeginTransactionWithDrawTarget");
  }
  void BeginTransactionWithDrawTarget(gfx::DrawTarget* aTarget, const gfx::IntRect& aRect);

  virtual bool EndEmptyTransaction(EndTransactionFlags aFlags = END_DEFAULT) override;
  virtual void EndTransaction(DrawPaintedLayerCallback aCallback,
                              void* aCallbackData,
                              EndTransactionFlags aFlags = END_DEFAULT) override;

  virtual void SetRoot(Layer* aLayer) override { mRoot = aLayer; }

  // XXX[nrc]: never called, we should move this logic to ClientLayerManager
  // (bug 946926).
  virtual bool CanUseCanvasLayerForSize(const gfx::IntSize &aSize) override;

  virtual int32_t GetMaxTextureSize() const override
  {
    MOZ_CRASH("Call on compositor, not LayerManagerComposite");
  }

  virtual void ClearCachedResources(Layer* aSubtree = nullptr) override;

  virtual already_AddRefed<PaintedLayer> CreatePaintedLayer() override;
  virtual already_AddRefed<ContainerLayer> CreateContainerLayer() override;
  virtual already_AddRefed<ImageLayer> CreateImageLayer() override;
  virtual already_AddRefed<ColorLayer> CreateColorLayer() override;
  virtual already_AddRefed<CanvasLayer> CreateCanvasLayer() override;
  already_AddRefed<PaintedLayerComposite> CreatePaintedLayerComposite();
  already_AddRefed<ContainerLayerComposite> CreateContainerLayerComposite();
  already_AddRefed<ImageLayerComposite> CreateImageLayerComposite();
  already_AddRefed<ColorLayerComposite> CreateColorLayerComposite();
  already_AddRefed<CanvasLayerComposite> CreateCanvasLayerComposite();
  already_AddRefed<RefLayerComposite> CreateRefLayerComposite();

  virtual LayersBackend GetBackendType() override
  {
    MOZ_CRASH("Shouldn't be called for composited layer manager");
  }
  virtual void GetBackendName(nsAString& name) override
  {
    MOZ_CRASH("Shouldn't be called for composited layer manager");
  }

  virtual bool AreComponentAlphaLayersEnabled() override;

  virtual TemporaryRef<DrawTarget>
    CreateOptimalMaskDrawTarget(const IntSize &aSize) override;

  virtual const char* Name() const override { return ""; }

  /**
   * Restricts the shadow visible region of layers that are covered with
   * opaque content. aOpaqueRegion is the region already known to be covered
   * with opaque content, in the post-transform coordinate space of aLayer.
   */
  void ApplyOcclusionCulling(Layer* aLayer, nsIntRegion& aOpaqueRegion);

  /**
   * RAII helper class to add a mask effect with the compositable from aMaskLayer
   * to the EffectChain aEffect and notify the compositable when we are done.
   */
  class AutoAddMaskEffect
  {
  public:
    AutoAddMaskEffect(Layer* aMaskLayer,
                      EffectChain& aEffect,
                      bool aIs3D = false);
    ~AutoAddMaskEffect();

    bool Failed() const { return mFailed; }
  private:
    CompositableHost* mCompositable;
    bool mFailed;
  };

  /**
   * Creates a DrawTarget which is optimized for inter-operating with this
   * layermanager.
   */
  virtual TemporaryRef<mozilla::gfx::DrawTarget>
    CreateDrawTarget(const mozilla::gfx::IntSize& aSize,
                     mozilla::gfx::SurfaceFormat aFormat) override;

  /**
   * Calculates the 'completeness' of the rendering that intersected with the
   * screen on the last render. This is only useful when progressive tile
   * drawing is enabled, otherwise this will always return 1.0.
   * This function's expense scales with the size of the layer tree and the
   * complexity of individual layers' valid regions.
   */
  float ComputeRenderIntegrity();

  /**
   * returns true if PlatformAllocBuffer will return a buffer that supports
   * direct texturing
   */
  static bool SupportsDirectTexturing();

  static void PlatformSyncBeforeReplyUpdate();

  void AddInvalidRegion(const nsIntRegion& aRegion)
  {
    mInvalidRegion.Or(mInvalidRegion, aRegion);
  }

  Compositor* GetCompositor() const
  {
    return mCompositor;
  }

  /**
   * LayerManagerComposite provides sophisticated debug overlays
   * that can request a next frame.
   */
  bool DebugOverlayWantsNextFrame() { return mDebugOverlayWantsNextFrame; }
  void SetDebugOverlayWantsNextFrame(bool aVal)
  { mDebugOverlayWantsNextFrame = aVal; }

  void NotifyShadowTreeTransaction();

  TextRenderer* GetTextRenderer() { return mTextRenderer; }

  /**
   * Add an on frame warning.
   * @param severity ranges from 0 to 1. It's used to compute the warning color.
   */
  void VisualFrameWarning(float severity) {
    mozilla::TimeStamp now = TimeStamp::Now();
    if (mWarnTime.IsNull() ||
        severity > mWarningLevel ||
        mWarnTime + TimeDuration::FromMilliseconds(kVisualWarningDuration) < now) {
      mWarnTime = now;
      mWarningLevel = severity;
    }
  }

  void UnusedApzTransformWarning() {
    mUnusedApzTransformWarning = true;
  }

  bool LastFrameMissedHWC() { return mLastFrameMissedHWC; }

  bool AsyncPanZoomEnabled() const override;

private:
  /** Region we're clipping our current drawing to. */
  nsIntRegion mClippingRegion;
  gfx::IntRect mRenderBounds;

  /** Current root layer. */
  LayerComposite* RootLayer() const;

  /**
   * Recursive helper method for use by ComputeRenderIntegrity. Subtracts
   * any incomplete rendering on aLayer from aScreenRegion. Any low-precision
   * rendering is included in aLowPrecisionScreenRegion. aTransform is the
   * accumulated transform of intermediate surfaces beneath aLayer.
   */
  static void ComputeRenderIntegrityInternal(Layer* aLayer,
                                             nsIntRegion& aScreenRegion,
                                             nsIntRegion& aLowPrecisionScreenRegion,
                                             const gfx::Matrix4x4& aTransform);

  /**
   * Render the current layer tree to the active target.
   */
  void Render();
#ifdef MOZ_WIDGET_ANDROID
  void RenderToPresentationSurface();
#endif

  /**
   * Render debug overlays such as the FPS/FrameCounter above the frame.
   */
  void RenderDebugOverlay(const gfx::Rect& aBounds);


  RefPtr<CompositingRenderTarget> PushGroupForLayerEffects();
  void PopGroupForLayerEffects(RefPtr<CompositingRenderTarget> aPreviousTarget,
                               gfx::IntRect aClipRect,
                               bool aGrayscaleEffect,
                               bool aInvertEffect,
                               float aContrastEffect);

  float mWarningLevel;
  mozilla::TimeStamp mWarnTime;
  bool mUnusedApzTransformWarning;
  RefPtr<Compositor> mCompositor;
  UniquePtr<LayerProperties> mClonedLayerTreeProperties;

  /**
   * Context target, nullptr when drawing directly to our swap chain.
   */
  RefPtr<gfx::DrawTarget> mTarget;
  gfx::IntRect mTargetBounds;

  nsIntRegion mInvalidRegion;
  UniquePtr<FPSState> mFPS;

  bool mInTransaction;
  bool mIsCompositorReady;
  bool mDebugOverlayWantsNextFrame;

  RefPtr<CompositingRenderTarget> mTwoPassTmpTarget;
  RefPtr<TextRenderer> mTextRenderer;
  bool mGeometryChanged;

  // Testing property. If hardware composer is supported, this will return
  // true if the last frame was deemed 'too complicated' to be rendered.
  bool mLastFrameMissedHWC;
};

/**
 * Composite layers are for use with OMTC on the compositor thread only. There
 * must be corresponding Basic layers on the content thread. For composite
 * layers, the layer manager only maintains the layer tree, all rendering is
 * done by a Compositor (see Compositor.h). As such, composite layers are
 * platform-independent and can be used on any platform for which there is a
 * Compositor implementation.
 *
 * The composite layer tree reflects exactly the basic layer tree. To
 * composite to screen, the layer manager walks the layer tree calling render
 * methods which in turn call into their CompositableHosts' Composite methods.
 * These call Compositor::DrawQuad to do the rendering.
 *
 * Mostly, layers are updated during the layers transaction. This is done from
 * CompositableClient to CompositableHost without interacting with the layer.
 *
 * A reference to the Compositor is stored in LayerManagerComposite.
 */
class LayerComposite
{
public:
  explicit LayerComposite(LayerManagerComposite* aManager);

  virtual ~LayerComposite();

  virtual LayerComposite* GetFirstChildComposite()
  {
    return nullptr;
  }

  /* Do NOT call this from the generic LayerComposite destructor.  Only from the
   * concrete class destructor
   */
  virtual void Destroy();

  virtual Layer* GetLayer() = 0;

  virtual void SetLayerManager(LayerManagerComposite* aManager);

  /**
   * Perform a first pass over the layer tree to render all of the intermediate
   * surfaces that we can. This allows us to avoid framebuffer switches in the
   * middle of our render which is inefficient especially on mobile GPUs. This
   * must be called before RenderLayer.
   */
  virtual void Prepare(const RenderTargetIntRect& aClipRect) {}

  // TODO: This should also take RenderTargetIntRect like Prepare.
  virtual void RenderLayer(const gfx::IntRect& aClipRect) = 0;

  virtual bool SetCompositableHost(CompositableHost*)
  {
    // We must handle this gracefully, see bug 967824
    NS_WARNING("called SetCompositableHost for a layer type not accepting a compositable");
    return false;
  }
  virtual CompositableHost* GetCompositableHost() = 0;

  virtual void CleanupResources() = 0;

  virtual TiledLayerComposer* GetTiledLayerComposer() { return nullptr; }

  virtual void DestroyFrontBuffer() { }

  void AddBlendModeEffect(EffectChain& aEffectChain);

  virtual void GenEffectChain(EffectChain& aEffect) { }

  /**
   * The following methods are
   *
   * CONSTRUCTION PHASE ONLY
   *
   * They are analogous to the Layer interface.
   */
  void SetShadowVisibleRegion(const nsIntRegion& aRegion)
  {
    mShadowVisibleRegion = aRegion;
  }

  void SetShadowOpacity(float aOpacity)
  {
    mShadowOpacity = aOpacity;
  }

  void SetShadowClipRect(const Maybe<ParentLayerIntRect>& aRect)
  {
    mShadowClipRect = aRect;
  }

  void SetShadowTransform(const gfx::Matrix4x4& aMatrix)
  {
    mShadowTransform = aMatrix;
  }
  void SetShadowTransformSetByAnimation(bool aSetByAnimation)
  {
    mShadowTransformSetByAnimation = aSetByAnimation;
  }

  void SetLayerComposited(bool value)
  {
    mLayerComposited = value;
  }

  void SetClearRect(const gfx::IntRect& aRect)
  {
    mClearRect = aRect;
  }

  // These getters can be used anytime.
  float GetShadowOpacity() { return mShadowOpacity; }
  const Maybe<ParentLayerIntRect>& GetShadowClipRect() { return mShadowClipRect; }
  const nsIntRegion& GetShadowVisibleRegion() { return mShadowVisibleRegion; }
  const gfx::Matrix4x4& GetShadowTransform() { return mShadowTransform; }
  bool GetShadowTransformSetByAnimation() { return mShadowTransformSetByAnimation; }
  bool HasLayerBeenComposited() { return mLayerComposited; }
  gfx::IntRect GetClearRect() { return mClearRect; }

  /**
   * Return the part of the visible region that has been fully rendered.
   * While progressive drawing is in progress this region will be
   * a subset of the shadow visible region.
   */
  nsIntRegion GetFullyRenderedRegion();

protected:
  gfx::Matrix4x4 mShadowTransform;
  nsIntRegion mShadowVisibleRegion;
  Maybe<ParentLayerIntRect> mShadowClipRect;
  LayerManagerComposite* mCompositeManager;
  RefPtr<Compositor> mCompositor;
  float mShadowOpacity;
  bool mShadowTransformSetByAnimation;
  bool mDestroyed;
  bool mLayerComposited;
  gfx::IntRect mClearRect;
};


} /* layers */
} /* mozilla */

#endif /* GFX_LayerManagerComposite_H */
