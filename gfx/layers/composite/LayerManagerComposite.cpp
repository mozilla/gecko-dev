/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "LayerManagerComposite.h"
#include <stddef.h>                   // for size_t
#include <stdint.h>                   // for uint16_t, uint32_t
#include "CanvasLayerComposite.h"     // for CanvasLayerComposite
#include "ColorLayerComposite.h"      // for ColorLayerComposite
#include "CompositableHost.h"         // for CompositableHost
#include "ContainerLayerComposite.h"  // for ContainerLayerComposite, etc
#include "Diagnostics.h"
#include "FPSCounter.h"                    // for FPSState, FPSCounter
#include "FrameMetrics.h"                  // for FrameMetrics
#include "GeckoProfiler.h"                 // for profiler_*
#include "ImageLayerComposite.h"           // for ImageLayerComposite
#include "Layers.h"                        // for Layer, ContainerLayer, etc
#include "LayerScope.h"                    // for LayerScope Tool
#include "protobuf/LayerScopePacket.pb.h"  // for protobuf (LayerScope)
#include "PaintedLayerComposite.h"         // for PaintedLayerComposite
#include "TiledContentHost.h"
#include "Units.h"                           // for ScreenIntRect
#include "UnitTransforms.h"                  // for ViewAs
#include "apz/src/AsyncPanZoomController.h"  // for AsyncPanZoomController
#include "gfxEnv.h"                          // for gfxEnv
#include "gfxPrefs.h"                        // for gfxPrefs
#ifdef XP_MACOSX
#include "gfxPlatformMac.h"
#endif
#include "gfxRect.h"                    // for gfxRect
#include "gfxUtils.h"                   // for frame color util
#include "mozilla/Assertions.h"         // for MOZ_ASSERT, etc
#include "mozilla/RefPtr.h"             // for RefPtr, already_AddRefed
#include "mozilla/gfx/2D.h"             // for DrawTarget
#include "mozilla/gfx/Matrix.h"         // for Matrix4x4
#include "mozilla/gfx/Point.h"          // for IntSize, Point
#include "mozilla/gfx/Rect.h"           // for Rect
#include "mozilla/gfx/Types.h"          // for Color, SurfaceFormat
#include "mozilla/layers/Compositor.h"  // for Compositor
#include "mozilla/layers/CompositorTypes.h"
#include "mozilla/layers/Effects.h"              // for Effect, EffectChain, etc
#include "mozilla/layers/LayerMetricsWrapper.h"  // for LayerMetricsWrapper
#include "mozilla/layers/LayersTypes.h"          // for etc
#include "mozilla/widget/CompositorWidget.h"     // for WidgetRenderingContext
#include "ipc/CompositorBench.h"                 // for CompositorBench
#include "ipc/ShadowLayerUtils.h"
#include "mozilla/mozalloc.h"  // for operator new, etc
#include "nsAppRunner.h"
#include "mozilla/RefPtr.h"   // for nsRefPtr
#include "nsCOMPtr.h"         // for already_AddRefed
#include "nsDebug.h"          // for NS_WARNING, etc
#include "nsISupportsImpl.h"  // for Layer::AddRef, etc
#include "nsPoint.h"          // for nsIntPoint
#include "nsRect.h"           // for mozilla::gfx::IntRect
#include "nsRegion.h"         // for nsIntRegion, etc
#if defined(MOZ_WIDGET_ANDROID)
#include <android/log.h>
#include <android/native_window.h>
#include "mozilla/jni/Utils.h"
#include "mozilla/widget/AndroidCompositorWidget.h"
#include "opengl/CompositorOGL.h"
#include "GLConsts.h"
#include "GLContextEGL.h"
#include "GLContextProvider.h"
#include "mozilla/Unused.h"
#include "ScopedGLHelpers.h"
#endif
#include "GeckoProfiler.h"
#include "TextRenderer.h"  // for TextRenderer
#include "mozilla/layers/CompositorBridgeParent.h"
#include "TreeTraversal.h"  // for ForEachNode

#ifdef USE_SKIA
#include "PaintCounter.h"  // For PaintCounter
#endif

class gfxContext;

namespace mozilla {
namespace layers {

class ImageLayer;

using namespace mozilla::gfx;
using namespace mozilla::gl;

static LayerComposite* ToLayerComposite(Layer* aLayer) {
  return static_cast<LayerComposite*>(aLayer->ImplData());
}

static void ClearSubtree(Layer* aLayer) {
  ForEachNode<ForwardIterator>(aLayer, [](Layer* layer) {
    ToLayerComposite(layer)->CleanupResources();
  });
}

void LayerManagerComposite::ClearCachedResources(Layer* aSubtree) {
  MOZ_ASSERT(!aSubtree || aSubtree->Manager() == this);
  Layer* subtree = aSubtree ? aSubtree : mRoot.get();
  if (!subtree) {
    return;
  }

  ClearSubtree(subtree);
  // FIXME [bjacob]
  // XXX the old LayerManagerOGL code had a mMaybeInvalidTree that it set to
  // true here. Do we need that?
}

HostLayerManager::HostLayerManager()
    : mDebugOverlayWantsNextFrame(false),
      mWarningLevel(0.0f),
      mCompositorBridgeID(0),
      mWindowOverlayChanged(false),
      mLastPaintTime(TimeDuration::Forever()),
      mRenderStartTime(TimeStamp::Now()) {}

HostLayerManager::~HostLayerManager() {}

void HostLayerManager::RecordPaintTimes(const PaintTiming& aTiming) {
  mDiagnostics->RecordPaintTimes(aTiming);
}

void HostLayerManager::RecordUpdateTime(float aValue) {
  mDiagnostics->RecordUpdateTime(aValue);
}

/**
 * LayerManagerComposite
 */
LayerManagerComposite::LayerManagerComposite(Compositor* aCompositor)
    : mUnusedApzTransformWarning(false),
      mDisabledApzWarning(false),
      mCompositor(aCompositor),
      mInTransaction(false),
      mIsCompositorReady(false)
#if defined(MOZ_WIDGET_ANDROID)
      ,
      mScreenPixelsTarget(nullptr)
#endif  // defined(MOZ_WIDGET_ANDROID)
{
  mTextRenderer = new TextRenderer();
  mDiagnostics = MakeUnique<Diagnostics>();
  MOZ_ASSERT(aCompositor);

#ifdef USE_SKIA
  mPaintCounter = nullptr;
#endif
}

LayerManagerComposite::~LayerManagerComposite() { Destroy(); }

void LayerManagerComposite::Destroy() {
  if (!mDestroyed) {
    mCompositor->GetWidget()->CleanupWindowEffects();
    if (mRoot) {
      RootLayer()->Destroy();
    }
    mCompositor->CancelFrame();
    mRoot = nullptr;
    mClonedLayerTreeProperties = nullptr;
    mProfilerScreenshotGrabber.Destroy();
    mDestroyed = true;

#ifdef USE_SKIA
    mPaintCounter = nullptr;
#endif
  }
}

void LayerManagerComposite::UpdateRenderBounds(const IntRect& aRect) {
  mRenderBounds = aRect;
}

bool LayerManagerComposite::AreComponentAlphaLayersEnabled() {
  return mCompositor->GetBackendType() != LayersBackend::LAYERS_BASIC &&
         mCompositor->SupportsEffect(EffectTypes::COMPONENT_ALPHA) &&
         LayerManager::AreComponentAlphaLayersEnabled();
}

bool LayerManagerComposite::BeginTransaction(const nsCString& aURL) {
  mInTransaction = true;

  if (!mCompositor->Ready()) {
    return false;
  }

  mIsCompositorReady = true;
  return true;
}

void LayerManagerComposite::BeginTransactionWithDrawTarget(
    DrawTarget* aTarget, const IntRect& aRect) {
  mInTransaction = true;

  if (!mCompositor->Ready()) {
    return;
  }

#ifdef MOZ_LAYERS_HAVE_LOG
  MOZ_LAYERS_LOG(("[----- BeginTransaction"));
  Log();
#endif

  if (mDestroyed) {
    NS_WARNING("Call on destroyed layer manager");
    return;
  }

  mIsCompositorReady = true;
  mCompositor->SetTargetContext(aTarget, aRect);
  mTarget = aTarget;
  mTargetBounds = aRect;
}

void LayerManagerComposite::PostProcessLayers(nsIntRegion& aOpaqueRegion) {
  LayerIntRegion visible;
  LayerComposite* rootComposite =
      static_cast<LayerComposite*>(mRoot->AsHostLayer());
  PostProcessLayers(
      mRoot, aOpaqueRegion, visible,
      ViewAs<RenderTargetPixel>(
          rootComposite->GetShadowClipRect(),
          PixelCastJustification::RenderTargetIsParentLayerForRoot),
      Nothing());
}

// We want to skip directly through ContainerLayers that don't have an
// intermediate surface. We compute occlusions for leaves and intermediate
// surfaces against the layer that they actually composite into so that we can
// use the final (snapped) effective transform.
bool ShouldProcessLayer(Layer* aLayer) {
  if (!aLayer->AsContainerLayer()) {
    return true;
  }

  return aLayer->AsContainerLayer()->UseIntermediateSurface();
}

void LayerManagerComposite::PostProcessLayers(
    Layer* aLayer, nsIntRegion& aOpaqueRegion, LayerIntRegion& aVisibleRegion,
    const Maybe<RenderTargetIntRect>& aRenderTargetClip,
    const Maybe<ParentLayerIntRect>& aClipFromAncestors) {
  // Compute a clip that's the combination of our layer clip with the clip
  // from our ancestors.
  LayerComposite* composite =
      static_cast<LayerComposite*>(aLayer->AsHostLayer());
  Maybe<ParentLayerIntRect> layerClip = composite->GetShadowClipRect();
  MOZ_ASSERT(!layerClip || !aLayer->Combines3DTransformWithAncestors(),
             "The layer with a clip should not participate "
             "a 3D rendering context");
  Maybe<ParentLayerIntRect> outsideClip =
      IntersectMaybeRects(layerClip, aClipFromAncestors);

  Maybe<LayerIntRect> insideClip;
  if (aLayer->Extend3DContext()) {
    // If we're preserve-3d just pass the clip rect down directly, and we'll do
    // the conversion at the preserve-3d leaf Layer.
    if (outsideClip) {
      insideClip = Some(ViewAs<LayerPixel>(
          *outsideClip, PixelCastJustification::MovingDownToChildren));
    }
  } else if (outsideClip) {
    // Convert the combined clip into our pre-transform coordinate space, so
    // that it can later be intersected with our visible region.
    // If our transform is a perspective, there's no meaningful insideClip rect
    // we can compute (it would need to be a cone).
    Matrix4x4 localTransform = aLayer->ComputeTransformToPreserve3DRoot();
    if (!localTransform.HasPerspectiveComponent() && localTransform.Invert()) {
      LayerRect insideClipFloat =
          UntransformBy(ViewAs<ParentLayerToLayerMatrix4x4>(localTransform),
                        ParentLayerRect(*outsideClip), LayerRect::MaxIntRect())
              .valueOr(LayerRect());
      insideClipFloat.RoundOut();
      LayerIntRect insideClipInt;
      if (insideClipFloat.ToIntRect(&insideClipInt)) {
        insideClip = Some(insideClipInt);
      }
    }
  }

  Maybe<ParentLayerIntRect> ancestorClipForChildren;
  if (insideClip) {
    ancestorClipForChildren = Some(ViewAs<ParentLayerPixel>(
        *insideClip, PixelCastJustification::MovingDownToChildren));
  }

  nsIntRegion dummy;
  nsIntRegion& opaqueRegion = aOpaqueRegion;
  if (aLayer->Extend3DContext() || aLayer->Combines3DTransformWithAncestors()) {
    opaqueRegion = dummy;
  }

  if (!ShouldProcessLayer(aLayer)) {
    MOZ_ASSERT(aLayer->AsContainerLayer() &&
               !aLayer->AsContainerLayer()->UseIntermediateSurface());
    // For layers participating 3D rendering context, their visible
    // region should be empty (invisible), so we pass through them
    // without doing anything.
    for (Layer* child = aLayer->GetLastChild(); child;
         child = child->GetPrevSibling()) {
      LayerComposite* childComposite =
          static_cast<LayerComposite*>(child->AsHostLayer());
      Maybe<RenderTargetIntRect> renderTargetClip = aRenderTargetClip;
      if (childComposite->GetShadowClipRect()) {
        RenderTargetIntRect clip = TransformBy(
            ViewAs<ParentLayerToRenderTargetMatrix4x4>(
                aLayer->GetEffectiveTransform(),
                PixelCastJustification::RenderTargetIsParentLayerForRoot),
            *childComposite->GetShadowClipRect());
        renderTargetClip = IntersectMaybeRects(renderTargetClip, Some(clip));
      }

      PostProcessLayers(child, opaqueRegion, aVisibleRegion, renderTargetClip,
                        ancestorClipForChildren);
    }
    return;
  }

  nsIntRegion localOpaque;
  // Treat layers on the path to the root of the 3D rendering context as
  // a giant layer if it is a leaf.
  Matrix4x4 transform = aLayer->GetEffectiveTransform();
  Matrix transform2d;
  Maybe<IntPoint> integerTranslation;
  // If aLayer has a simple transform (only an integer translation) then we
  // can easily convert aOpaqueRegion into pre-transform coordinates and include
  // that region.
  if (transform.Is2D(&transform2d)) {
    if (transform2d.IsIntegerTranslation()) {
      integerTranslation =
          Some(IntPoint::Truncate(transform2d.GetTranslation()));
      localOpaque = opaqueRegion;
      localOpaque.MoveBy(-*integerTranslation);
    }
  }

  // Save the value of localOpaque, which currently stores the region obscured
  // by siblings (and uncles and such), before our descendants contribute to it.
  nsIntRegion obscured = localOpaque;

  // Recurse on our descendants, in front-to-back order. In this process:
  //  - Occlusions are computed for them, and they contribute to localOpaque.
  //  - They recalculate their visible regions, taking ancestorClipForChildren
  //    into account, and accumulate them into descendantsVisibleRegion.
  LayerIntRegion descendantsVisibleRegion;

  bool hasPreserve3DChild = false;
  for (Layer* child = aLayer->GetLastChild(); child;
       child = child->GetPrevSibling()) {
    MOZ_ASSERT(aLayer->AsContainerLayer()->UseIntermediateSurface());
    LayerComposite* childComposite =
        static_cast<LayerComposite*>(child->AsHostLayer());
    PostProcessLayers(
        child, localOpaque, descendantsVisibleRegion,
        ViewAs<RenderTargetPixel>(
            childComposite->GetShadowClipRect(),
            PixelCastJustification::RenderTargetIsParentLayerForRoot),
        ancestorClipForChildren);
    if (child->Extend3DContext()) {
      hasPreserve3DChild = true;
    }
  }

  // Recalculate our visible region.
  LayerIntRegion visible = composite->GetShadowVisibleRegion();

  // If we have descendants, throw away the visible region stored on this
  // layer, and use the region accumulated by our descendants instead.
  if (aLayer->GetFirstChild() && !hasPreserve3DChild) {
    visible = descendantsVisibleRegion;
  }

  // Subtract any areas that we know to be opaque.
  if (!obscured.IsEmpty()) {
    visible.SubOut(LayerIntRegion::FromUnknownRegion(obscured));
  }

  // Clip the visible region using the combined clip.
  if (insideClip) {
    visible.AndWith(*insideClip);
  }
  composite->SetShadowVisibleRegion(visible);

  // Transform the newly calculated visible region into our parent's space,
  // apply our clip to it (if any), and accumulate it into |aVisibleRegion|
  // for the caller to use.
  ParentLayerIntRegion visibleParentSpace =
      TransformBy(ViewAs<LayerToParentLayerMatrix4x4>(transform), visible);
  aVisibleRegion.OrWith(ViewAs<LayerPixel>(
      visibleParentSpace, PixelCastJustification::MovingDownToChildren));

  // If we have a simple transform, then we can add our opaque area into
  // aOpaqueRegion.
  if (integerTranslation && !aLayer->HasMaskLayers() &&
      aLayer->IsOpaqueForVisibility()) {
    if (aLayer->IsOpaque()) {
      localOpaque.OrWith(composite->GetFullyRenderedRegion());
    }
    localOpaque.MoveBy(*integerTranslation);
    if (aRenderTargetClip) {
      localOpaque.AndWith(aRenderTargetClip->ToUnknownRect());
    }
    opaqueRegion.OrWith(localOpaque);
  }
}

void LayerManagerComposite::EndTransaction(const TimeStamp& aTimeStamp,
                                           EndTransactionFlags aFlags) {
  NS_ASSERTION(mInTransaction, "Didn't call BeginTransaction?");
  NS_ASSERTION(!(aFlags & END_NO_COMPOSITE),
               "Shouldn't get END_NO_COMPOSITE here");
  mInTransaction = false;
  mRenderStartTime = TimeStamp::Now();

  if (!mIsCompositorReady) {
    return;
  }
  mIsCompositorReady = false;

#ifdef MOZ_LAYERS_HAVE_LOG
  MOZ_LAYERS_LOG(("  ----- (beginning paint)"));
  Log();
#endif

  if (mDestroyed) {
    NS_WARNING("Call on destroyed layer manager");
    return;
  }

  // Set composition timestamp here because we need it in
  // ComputeEffectiveTransforms (so the correct video frame size is picked) and
  // also to compute invalid regions properly.
  SetCompositionTime(aTimeStamp);

  if (mRoot && !(aFlags & END_NO_IMMEDIATE_REDRAW)) {
    MOZ_ASSERT(!aTimeStamp.IsNull());
    UpdateAndRender();
    mCompositor->FlushPendingNotifyNotUsed();
  }

  mCompositor->ClearTargetContext();
  mTarget = nullptr;

#ifdef MOZ_LAYERS_HAVE_LOG
  Log();
  MOZ_LAYERS_LOG(("]----- EndTransaction"));
#endif
}

void LayerManagerComposite::UpdateAndRender() {
  if (gfxEnv::SkipComposition()) {
    mInvalidRegion.SetEmpty();
    return;
  }

  nsIntRegion invalid;
  // The results of our drawing always go directly into a pixel buffer,
  // so we don't need to pass any global transform here.
  mRoot->ComputeEffectiveTransforms(gfx::Matrix4x4());

  nsIntRegion opaque;
  PostProcessLayers(opaque);

  if (mClonedLayerTreeProperties) {
    // We need to compute layer tree differences even if we're not going to
    // immediately use the resulting damage area, since ComputeDifferences
    // is also responsible for invalidates intermediate surfaces in
    // ContainerLayers.
    nsIntRegion changed;

    const bool overflowed = !mClonedLayerTreeProperties->ComputeDifferences(
        mRoot, changed, nullptr);

    if (overflowed) {
      changed = mTarget ? mTargetBounds : mRenderBounds;
    }

    if (mTarget) {
      // Since we're composing to an external target, we're not going to use
      // the damage region from layers changes - we want to composite
      // everything in the target bounds. Instead we accumulate the layers
      // damage region for the next window composite.
      mInvalidRegion.Or(mInvalidRegion, changed);
    } else {
      invalid = std::move(changed);
    }
  }

  if (mTarget) {
    invalid.Or(invalid, mTargetBounds);
  } else {
    // If we didn't have a previous layer tree, invalidate the entire render
    // area.
    if (!mClonedLayerTreeProperties) {
      invalid.Or(invalid, mRenderBounds);
    }

    // Add any additional invalid rects from the window manager or previous
    // damage computed during ComposeToTarget().
    invalid.Or(invalid, mInvalidRegion);
    mInvalidRegion.SetEmpty();
  }

  if (invalid.IsEmpty() && !mWindowOverlayChanged) {
    // Composition requested, but nothing has changed. Don't do any work.
    mClonedLayerTreeProperties = LayerProperties::CloneFrom(GetRoot());
    return;
  }

  // We don't want our debug overlay to cause more frames to happen
  // so we will invalidate after we've decided if something changed.
  InvalidateDebugOverlay(invalid, mRenderBounds);

  Render(invalid, opaque);
#if defined(MOZ_WIDGET_ANDROID)
  RenderToPresentationSurface();
#endif
  mWindowOverlayChanged = false;

  // Update cached layer tree information.
  mClonedLayerTreeProperties = LayerProperties::CloneFrom(GetRoot());
}

already_AddRefed<DrawTarget> LayerManagerComposite::CreateOptimalMaskDrawTarget(
    const IntSize& aSize) {
  MOZ_CRASH("Should only be called on the drawing side");
  return nullptr;
}

LayerComposite* LayerManagerComposite::RootLayer() const {
  if (mDestroyed) {
    NS_WARNING("Call on destroyed layer manager");
    return nullptr;
  }

  return ToLayerComposite(mRoot);
}

void LayerManagerComposite::InvalidateDebugOverlay(nsIntRegion& aInvalidRegion,
                                                   const IntRect& aBounds) {
  bool drawFps = gfxPrefs::LayersDrawFPS();
  bool drawFrameColorBars = gfxPrefs::CompositorDrawColorBars();

  if (drawFps) {
    aInvalidRegion.Or(aInvalidRegion, nsIntRect(0, 0, 650, 400));
  }
  if (drawFrameColorBars) {
    aInvalidRegion.Or(aInvalidRegion, nsIntRect(0, 0, 10, aBounds.Height()));
  }

#ifdef USE_SKIA
  bool drawPaintTimes = gfxPrefs::AlwaysPaint();
  if (drawPaintTimes) {
    aInvalidRegion.Or(aInvalidRegion, nsIntRect(PaintCounter::GetPaintRect()));
  }
#endif
}

#ifdef USE_SKIA
void LayerManagerComposite::DrawPaintTimes(Compositor* aCompositor) {
  if (!mPaintCounter) {
    mPaintCounter = new PaintCounter();
  }

  TimeDuration compositeTime = TimeStamp::Now() - mRenderStartTime;
  mPaintCounter->Draw(aCompositor, mLastPaintTime, compositeTime);
}
#endif

static uint16_t sFrameCount = 0;
void LayerManagerComposite::RenderDebugOverlay(const IntRect& aBounds) {
  bool drawFps = gfxPrefs::LayersDrawFPS();
  bool drawFrameColorBars = gfxPrefs::CompositorDrawColorBars();

  // Don't draw diagnostic overlays if we want to snapshot the output.
  if (mTarget) {
    return;
  }

  if (drawFps) {
    float alpha = 1;
#ifdef ANDROID
    // Draw a translation delay warning overlay
    int width;
    int border;

    TimeStamp now = TimeStamp::Now();
    if (!mWarnTime.IsNull() &&
        (now - mWarnTime).ToMilliseconds() < kVisualWarningDuration) {
      EffectChain effects;

      // Black blorder
      border = 4;
      width = 6;
      effects.mPrimaryEffect = new EffectSolidColor(gfx::Color(0, 0, 0, 1));
      mCompositor->DrawQuad(
          gfx::Rect(border, border, aBounds.Width() - 2 * border, width),
          aBounds, effects, alpha, gfx::Matrix4x4());
      mCompositor->DrawQuad(gfx::Rect(border, aBounds.Height() - border - width,
                                      aBounds.Width() - 2 * border, width),
                            aBounds, effects, alpha, gfx::Matrix4x4());
      mCompositor->DrawQuad(
          gfx::Rect(border, border + width, width,
                    aBounds.Height() - 2 * border - width * 2),
          aBounds, effects, alpha, gfx::Matrix4x4());
      mCompositor->DrawQuad(
          gfx::Rect(aBounds.Width() - border - width, border + width, width,
                    aBounds.Height() - 2 * border - 2 * width),
          aBounds, effects, alpha, gfx::Matrix4x4());

      // Content
      border = 5;
      width = 4;
      effects.mPrimaryEffect =
          new EffectSolidColor(gfx::Color(1, 1.f - mWarningLevel, 0, 1));
      mCompositor->DrawQuad(
          gfx::Rect(border, border, aBounds.Width() - 2 * border, width),
          aBounds, effects, alpha, gfx::Matrix4x4());
      mCompositor->DrawQuad(gfx::Rect(border, aBounds.height - border - width,
                                      aBounds.Width() - 2 * border, width),
                            aBounds, effects, alpha, gfx::Matrix4x4());
      mCompositor->DrawQuad(
          gfx::Rect(border, border + width, width,
                    aBounds.Height() - 2 * border - width * 2),
          aBounds, effects, alpha, gfx::Matrix4x4());
      mCompositor->DrawQuad(
          gfx::Rect(aBounds.Width() - border - width, border + width, width,
                    aBounds.Height() - 2 * border - 2 * width),
          aBounds, effects, alpha, gfx::Matrix4x4());
      SetDebugOverlayWantsNextFrame(true);
    }
#endif

    GPUStats stats;
    stats.mScreenPixels = mRenderBounds.Width() * mRenderBounds.Height();
    mCompositor->GetFrameStats(&stats);

    std::string text = mDiagnostics->GetFrameOverlayString(stats);
    mTextRenderer->RenderText(mCompositor, text, IntPoint(2, 5), Matrix4x4(),
                              24, 600, TextRenderer::FontType::FixedWidth);

    if (mUnusedApzTransformWarning) {
      // If we have an unused APZ transform on this composite, draw a 20x20 red
      // box in the top-right corner
      EffectChain effects;
      effects.mPrimaryEffect = new EffectSolidColor(gfx::Color(1, 0, 0, 1));
      mCompositor->DrawQuad(gfx::Rect(aBounds.Width() - 20, 0, 20, 20), aBounds,
                            effects, alpha, gfx::Matrix4x4());

      mUnusedApzTransformWarning = false;
      SetDebugOverlayWantsNextFrame(true);
    }
    if (mDisabledApzWarning) {
      // If we have a disabled APZ on this composite, draw a 20x20 yellow box
      // in the top-right corner, to the left of the unused-apz-transform
      // warning box
      EffectChain effects;
      effects.mPrimaryEffect = new EffectSolidColor(gfx::Color(1, 1, 0, 1));
      mCompositor->DrawQuad(gfx::Rect(aBounds.Width() - 40, 0, 20, 20), aBounds,
                            effects, alpha, gfx::Matrix4x4());

      mDisabledApzWarning = false;
      SetDebugOverlayWantsNextFrame(true);
    }
  }

  if (drawFrameColorBars) {
    gfx::IntRect sideRect(0, 0, 10, aBounds.Height());

    EffectChain effects;
    effects.mPrimaryEffect =
        new EffectSolidColor(gfxUtils::GetColorForFrameNumber(sFrameCount));
    mCompositor->DrawQuad(Rect(sideRect), sideRect, effects, 1.0,
                          gfx::Matrix4x4());
  }

  if (drawFrameColorBars) {
    // We intentionally overflow at 2^16.
    sFrameCount++;
  }

#ifdef USE_SKIA
  bool drawPaintTimes = gfxPrefs::AlwaysPaint();
  if (drawPaintTimes) {
    DrawPaintTimes(mCompositor);
  }
#endif
}

RefPtr<CompositingRenderTarget>
LayerManagerComposite::PushGroupForLayerEffects() {
  // This is currently true, so just making sure that any new use of this
  // method is flagged for investigation
  MOZ_ASSERT(gfxPrefs::LayersEffectInvert() ||
             gfxPrefs::LayersEffectGrayscale() ||
             gfxPrefs::LayersEffectContrast() != 0.0);

  RefPtr<CompositingRenderTarget> previousTarget =
      mCompositor->GetCurrentRenderTarget();
  // make our render target the same size as the destination target
  // so that we don't have to change size if the drawing area changes.
  IntRect rect(previousTarget->GetOrigin(), previousTarget->GetSize());
  // XXX: I'm not sure if this is true or not...
  MOZ_ASSERT(rect.IsEqualXY(0, 0));
  if (!mTwoPassTmpTarget ||
      mTwoPassTmpTarget->GetSize() != previousTarget->GetSize() ||
      mTwoPassTmpTarget->GetOrigin() != previousTarget->GetOrigin()) {
    mTwoPassTmpTarget = mCompositor->CreateRenderTarget(rect, INIT_MODE_NONE);
  }
  MOZ_ASSERT(mTwoPassTmpTarget);
  mCompositor->SetRenderTarget(mTwoPassTmpTarget);
  return previousTarget;
}
void LayerManagerComposite::PopGroupForLayerEffects(
    RefPtr<CompositingRenderTarget> aPreviousTarget, IntRect aClipRect,
    bool aGrayscaleEffect, bool aInvertEffect, float aContrastEffect) {
  MOZ_ASSERT(mTwoPassTmpTarget);

  // This is currently true, so just making sure that any new use of this
  // method is flagged for investigation
  MOZ_ASSERT(aInvertEffect || aGrayscaleEffect || aContrastEffect != 0.0);

  mCompositor->SetRenderTarget(aPreviousTarget);

  EffectChain effectChain(RootLayer());
  Matrix5x4 effectMatrix;
  if (aGrayscaleEffect) {
    // R' = G' = B' = luminance
    // R' = 0.2126*R + 0.7152*G + 0.0722*B
    // G' = 0.2126*R + 0.7152*G + 0.0722*B
    // B' = 0.2126*R + 0.7152*G + 0.0722*B
    Matrix5x4 grayscaleMatrix(0.2126f, 0.2126f, 0.2126f, 0, 0.7152f, 0.7152f,
                              0.7152f, 0, 0.0722f, 0.0722f, 0.0722f, 0, 0, 0, 0,
                              1, 0, 0, 0, 0);
    effectMatrix = grayscaleMatrix;
  }

  if (aInvertEffect) {
    // R' = 1 - R
    // G' = 1 - G
    // B' = 1 - B
    Matrix5x4 colorInvertMatrix(-1, 0, 0, 0, 0, -1, 0, 0, 0, 0, -1, 0, 0, 0, 0,
                                1, 1, 1, 1, 0);
    effectMatrix = effectMatrix * colorInvertMatrix;
  }

  if (aContrastEffect != 0.0) {
    // Multiplying with:
    // R' = (1 + c) * (R - 0.5) + 0.5
    // G' = (1 + c) * (G - 0.5) + 0.5
    // B' = (1 + c) * (B - 0.5) + 0.5
    float cP1 = aContrastEffect + 1;
    float hc = 0.5 * aContrastEffect;
    Matrix5x4 contrastMatrix(cP1, 0, 0, 0, 0, cP1, 0, 0, 0, 0, cP1, 0, 0, 0, 0,
                             1, -hc, -hc, -hc, 0);
    effectMatrix = effectMatrix * contrastMatrix;
  }

  effectChain.mPrimaryEffect = new EffectRenderTarget(mTwoPassTmpTarget);
  effectChain.mSecondaryEffects[EffectTypes::COLOR_MATRIX] =
      new EffectColorMatrix(effectMatrix);

  mCompositor->DrawQuad(Rect(Point(0, 0), Size(mTwoPassTmpTarget->GetSize())),
                        aClipRect, effectChain, 1., Matrix4x4());
}

// Used to clear the 'mLayerComposited' flag at the beginning of each Render().
static void ClearLayerFlags(Layer* aLayer) {
  ForEachNode<ForwardIterator>(aLayer, [](Layer* layer) {
    if (layer->AsHostLayer()) {
      static_cast<LayerComposite*>(layer->AsHostLayer())
          ->SetLayerComposited(false);
    }
  });
}

#if defined(MOZ_WIDGET_ANDROID)
class ScopedCompositorRenderOffset {
 public:
  ScopedCompositorRenderOffset(CompositorOGL* aCompositor,
                               const ScreenPoint& aOffset)
      : mCompositor(aCompositor),
        mOriginalOffset(mCompositor->GetScreenRenderOffset()),
        mOriginalProjection(mCompositor->GetProjMatrix()) {
    ScreenPoint offset(mOriginalOffset.x + aOffset.x,
                       mOriginalOffset.y + aOffset.y);
    mCompositor->SetScreenRenderOffset(offset);
    // Calling CompositorOGL::SetScreenRenderOffset does not affect the
    // projection matrix so adjust that as well.
    gfx::Matrix4x4 mat = mOriginalProjection;
    mat.PreTranslate(aOffset.x, aOffset.y, 0.0f);
    mCompositor->SetProjMatrix(mat);
  }
  ~ScopedCompositorRenderOffset() {
    mCompositor->SetScreenRenderOffset(mOriginalOffset);
    mCompositor->SetProjMatrix(mOriginalProjection);
  }

 private:
  CompositorOGL* const mCompositor;
  const ScreenPoint mOriginalOffset;
  const gfx::Matrix4x4 mOriginalProjection;
};
#endif  // defined(MOZ_WIDGET_ANDROID)

void LayerManagerComposite::Render(const nsIntRegion& aInvalidRegion,
                                   const nsIntRegion& aOpaqueRegion) {
  AUTO_PROFILER_LABEL("LayerManagerComposite::Render", GRAPHICS);

  if (mDestroyed || !mCompositor || mCompositor->IsDestroyed()) {
    NS_WARNING("Call on destroyed layer manager");
    return;
  }

  ClearLayerFlags(mRoot);

  // At this time, it doesn't really matter if these preferences change
  // during the execution of the function; we should be safe in all
  // permutations. However, may as well just get the values onces and
  // then use them, just in case the consistency becomes important in
  // the future.
  bool invertVal = gfxPrefs::LayersEffectInvert();
  bool grayscaleVal = gfxPrefs::LayersEffectGrayscale();
  float contrastVal = gfxPrefs::LayersEffectContrast();
  bool haveLayerEffects = (invertVal || grayscaleVal || contrastVal != 0.0);

  // Set LayerScope begin/end frame
  LayerScopeAutoFrame frame(PR_Now());

  // Dump to console
  if (gfxPrefs::LayersDump()) {
    this->Dump(/* aSorted= */ true);
  }

  // Dump to LayerScope Viewer
  if (LayerScope::CheckSendable()) {
    // Create a LayersPacket, dump Layers into it and transfer the
    // packet('s ownership) to LayerScope.
    auto packet = MakeUnique<layerscope::Packet>();
    layerscope::LayersPacket* layersPacket = packet->mutable_layers();
    this->Dump(layersPacket);
    LayerScope::SendLayerDump(std::move(packet));
  }

  mozilla::widget::WidgetRenderingContext widgetContext;
#if defined(XP_MACOSX)
  widgetContext.mLayerManager = this;
#elif defined(MOZ_WIDGET_ANDROID)
  widgetContext.mCompositor = GetCompositor();
#endif

  {
    AUTO_PROFILER_LABEL("LayerManagerComposite::Render:Prerender", GRAPHICS);

    if (!mCompositor->GetWidget()->PreRender(&widgetContext)) {
      return;
    }
  }

  ParentLayerIntRect clipRect;
  IntRect bounds(mRenderBounds.X(), mRenderBounds.Y(), mRenderBounds.Width(),
                 mRenderBounds.Height());
  IntRect actualBounds;

  CompositorBench(mCompositor, bounds);

  MOZ_ASSERT(mRoot->GetOpacity() == 1);
#if defined(MOZ_WIDGET_ANDROID)
  LayerMetricsWrapper wrapper = GetRootContentLayer();
  if (wrapper) {
    mCompositor->SetClearColor(wrapper.Metadata().GetBackgroundColor());
  } else {
    mCompositor->SetClearColorToDefault();
  }
#endif
  if (mRoot->GetClipRect()) {
    clipRect = *mRoot->GetClipRect();
    IntRect rect(clipRect.X(), clipRect.Y(), clipRect.Width(),
                 clipRect.Height());
    mCompositor->BeginFrame(aInvalidRegion, &rect, bounds, aOpaqueRegion,
                            nullptr, &actualBounds);
  } else {
    gfx::IntRect rect;
    mCompositor->BeginFrame(aInvalidRegion, nullptr, bounds, aOpaqueRegion,
                            &rect, &actualBounds);
    clipRect =
        ParentLayerIntRect(rect.X(), rect.Y(), rect.Width(), rect.Height());
  }
#if defined(MOZ_WIDGET_ANDROID)
  ScreenCoord offset = GetContentShiftForToolbar();
  ScopedCompositorRenderOffset scopedOffset(mCompositor->AsCompositorOGL(),
                                            ScreenPoint(0.0f, offset));
#endif

  if (actualBounds.IsEmpty()) {
    mProfilerScreenshotGrabber.NotifyEmptyFrame();
    mCompositor->GetWidget()->PostRender(&widgetContext);
    return;
  }

  // Allow widget to render a custom background.
  mCompositor->GetWidget()->DrawWindowUnderlay(
      &widgetContext, LayoutDeviceIntRect::FromUnknownRect(actualBounds));

  RefPtr<CompositingRenderTarget> previousTarget;
  if (haveLayerEffects) {
    previousTarget = PushGroupForLayerEffects();
  } else {
    mTwoPassTmpTarget = nullptr;
  }

  // Render our layers.
  {
    Diagnostics::Record record(mRenderStartTime);
    RootLayer()->Prepare(ViewAs<RenderTargetPixel>(
        clipRect, PixelCastJustification::RenderTargetIsParentLayerForRoot));
    if (record.Recording()) {
      mDiagnostics->RecordPrepareTime(record.Duration());
    }
  }
  // Execute draw commands.
  {
    Diagnostics::Record record;
    RootLayer()->RenderLayer(clipRect.ToUnknownRect(), Nothing());
    if (record.Recording()) {
      mDiagnostics->RecordCompositeTime(record.Duration());
    }
  }
  RootLayer()->Cleanup();

  if (!mRegionToClear.IsEmpty()) {
    for (auto iter = mRegionToClear.RectIter(); !iter.Done(); iter.Next()) {
      const IntRect& r = iter.Get();
      mCompositor->ClearRect(Rect(r.X(), r.Y(), r.Width(), r.Height()));
    }
  }

  if (mTwoPassTmpTarget) {
    MOZ_ASSERT(haveLayerEffects);
    PopGroupForLayerEffects(previousTarget, clipRect.ToUnknownRect(),
                            grayscaleVal, invertVal, contrastVal);
  }

  // Allow widget to render a custom foreground.
  mCompositor->GetWidget()->DrawWindowOverlay(
      &widgetContext, LayoutDeviceIntRect::FromUnknownRect(actualBounds));

  mProfilerScreenshotGrabber.MaybeGrabScreenshot(mCompositor);

  mCompositor->NormalDrawingDone();

#if defined(MOZ_WIDGET_ANDROID)
  // Depending on the content shift the toolbar may be rendered on top of
  // some of the content so it must be rendered after the content.
  if (jni::IsFennec()) {
    RenderToolbar();
  }
  HandlePixelsTarget();
#endif  // defined(MOZ_WIDGET_ANDROID)

  // Debugging
  RenderDebugOverlay(actualBounds);

  {
    AUTO_PROFILER_LABEL("LayerManagerComposite::Render:EndFrame", GRAPHICS);

    mCompositor->EndFrame();
  }

  mCompositor->GetWidget()->PostRender(&widgetContext);

  mProfilerScreenshotGrabber.MaybeProcessQueue();

  RecordFrame();
}

#if defined(MOZ_WIDGET_ANDROID)
class ScopedCompostitorSurfaceSize {
 public:
  ScopedCompostitorSurfaceSize(CompositorOGL* aCompositor,
                               const gfx::IntSize& aSize)
      : mCompositor(aCompositor),
        mOriginalSize(mCompositor->GetDestinationSurfaceSize()) {
    mCompositor->SetDestinationSurfaceSize(aSize);
  }
  ~ScopedCompostitorSurfaceSize() {
    mCompositor->SetDestinationSurfaceSize(mOriginalSize);
  }

 private:
  CompositorOGL* const mCompositor;
  const gfx::IntSize mOriginalSize;
};

class ScopedContextSurfaceOverride {
 public:
  ScopedContextSurfaceOverride(GLContextEGL* aContext, void* aSurface)
      : mContext(aContext) {
    MOZ_ASSERT(aSurface);
    mContext->SetEGLSurfaceOverride(aSurface);
    mContext->MakeCurrent(true);
  }
  ~ScopedContextSurfaceOverride() {
    mContext->SetEGLSurfaceOverride(EGL_NO_SURFACE);
    mContext->MakeCurrent(true);
  }

 private:
  GLContextEGL* const mContext;
};

void LayerManagerComposite::RenderToPresentationSurface() {
  if (!mCompositor) {
    return;
  }

  widget::CompositorWidget* const widget = mCompositor->GetWidget();

  if (!widget) {
    return;
  }

  ANativeWindow* window = widget->AsAndroid()->GetPresentationANativeWindow();

  if (!window) {
    return;
  }

  CompositorOGL* compositor = mCompositor->AsCompositorOGL();
  GLContext* gl = compositor->gl();
  GLContextEGL* egl = GLContextEGL::Cast(gl);

  if (!egl) {
    return;
  }

  EGLSurface surface = widget->AsAndroid()->GetPresentationEGLSurface();

  if (!surface) {
    // create surface;
    surface = egl->CreateCompatibleSurface(window);
    if (!surface) {
      return;
    }

    widget->AsAndroid()->SetPresentationEGLSurface(surface);
  }

  const IntSize windowSize(ANativeWindow_getWidth(window),
                           ANativeWindow_getHeight(window));

  if ((windowSize.width <= 0) || (windowSize.height <= 0)) {
    return;
  }

  ScreenRotation rotation = compositor->GetScreenRotation();

  const int actualWidth = windowSize.width;
  const int actualHeight = windowSize.height;

  const gfx::IntSize originalSize = compositor->GetDestinationSurfaceSize();
  const nsIntRect originalRect =
      nsIntRect(0, 0, originalSize.width, originalSize.height);

  int pageWidth = originalSize.width;
  int pageHeight = originalSize.height;
  if (rotation == ROTATION_90 || rotation == ROTATION_270) {
    pageWidth = originalSize.height;
    pageHeight = originalSize.width;
  }

  float scale = 1.0;

  if ((pageWidth > actualWidth) || (pageHeight > actualHeight)) {
    const float scaleWidth = (float)actualWidth / (float)pageWidth;
    const float scaleHeight = (float)actualHeight / (float)pageHeight;
    scale = scaleWidth <= scaleHeight ? scaleWidth : scaleHeight;
  }

  const gfx::IntSize actualSize(actualWidth, actualHeight);
  ScopedCompostitorSurfaceSize overrideSurfaceSize(compositor, actualSize);

  const ScreenPoint offset((actualWidth - (int)(scale * pageWidth)) / 2, 0);
  ScopedContextSurfaceOverride overrideSurface(egl, surface);

  Matrix viewMatrix = ComputeTransformForRotation(originalRect, rotation);
  viewMatrix.Invert();  // unrotate
  viewMatrix.PostScale(scale, scale);
  viewMatrix.PostTranslate(offset.x, offset.y);
  Matrix4x4 matrix = Matrix4x4::From2D(viewMatrix);

  mRoot->ComputeEffectiveTransforms(matrix);
  nsIntRegion opaque;
  PostProcessLayers(opaque);

  nsIntRegion invalid;
  IntRect bounds = IntRect::Truncate(0, 0, scale * pageWidth, actualHeight);
  IntRect rect, actualBounds;
  MOZ_ASSERT(mRoot->GetOpacity() == 1);
  mCompositor->BeginFrame(invalid, nullptr, bounds, nsIntRegion(), &rect,
                          &actualBounds);

  // The Java side of Fennec sets a scissor rect that accounts for
  // chrome such as the URL bar. Override that so that the entire frame buffer
  // is cleared.
  ScopedScissorRect scissorRect(egl, 0, 0, actualWidth, actualHeight);
  egl->fClearColor(0.0, 0.0, 0.0, 0.0);
  egl->fClear(LOCAL_GL_COLOR_BUFFER_BIT);

  const IntRect clipRect = IntRect::Truncate(0, 0, actualWidth, actualHeight);

  RootLayer()->Prepare(RenderTargetIntRect::FromUnknownRect(clipRect));
  RootLayer()->RenderLayer(clipRect, Nothing());

  mCompositor->EndFrame();
}

ScreenCoord LayerManagerComposite::GetContentShiftForToolbar() {
  ScreenCoord result(0.0f);
  // If we're not in Fennec, we don't have a dynamic toolbar so there isn't a
  // content offset.
  if (!jni::IsFennec()) {
    return result;
  }
  // If GetTargetContext return is not null we are not drawing to the screen so
  // there will not be any content offset.
  if (mCompositor->GetTargetContext() != nullptr) {
    return result;
  }

  if (CompositorBridgeParent* bridge =
          mCompositor->GetCompositorBridgeParent()) {
    AndroidDynamicToolbarAnimator* animator =
        bridge->GetAndroidDynamicToolbarAnimator();
    MOZ_RELEASE_ASSERT(animator);
    result.value = (float)animator->GetCurrentContentOffset().value;
  }
  return result;
}

void LayerManagerComposite::RenderToolbar() {
  // If GetTargetContext return is not null we are not drawing to the screen so
  // don't draw the toolbar.
  if (mCompositor->GetTargetContext() != nullptr) {
    return;
  }

  if (CompositorBridgeParent* bridge =
          mCompositor->GetCompositorBridgeParent()) {
    AndroidDynamicToolbarAnimator* animator =
        bridge->GetAndroidDynamicToolbarAnimator();
    MOZ_RELEASE_ASSERT(animator);

    animator->UpdateToolbarSnapshotTexture(mCompositor->AsCompositorOGL());

    int32_t toolbarHeight = animator->GetCurrentToolbarHeight();
    if (toolbarHeight == 0) {
      return;
    }

    EffectChain effects;
    effects.mPrimaryEffect = animator->GetToolbarEffect();

    // If GetToolbarEffect returns null, nothing is rendered for the static
    // snapshot of the toolbar. If the real toolbar chrome is not covering this
    // portion of the surface, the clear color of the surface will be visible.
    // On Android the clear color is the background color of the page.
    if (effects.mPrimaryEffect) {
      ScopedCompositorRenderOffset toolbarOffset(
          mCompositor->AsCompositorOGL(),
          ScreenPoint(0.0f, -animator->GetCurrentContentOffset()));
      mCompositor->DrawQuad(gfx::Rect(0, 0, mRenderBounds.width, toolbarHeight),
                            IntRect(0, 0, mRenderBounds.width, toolbarHeight),
                            effects, 1.0, gfx::Matrix4x4());
    }
  }
}

// Used by robocop tests to get a snapshot of the frame buffer.
void LayerManagerComposite::HandlePixelsTarget() {
  if (!mScreenPixelsTarget) {
    return;
  }

  int32_t bufferWidth = mRenderBounds.width;
  int32_t bufferHeight = mRenderBounds.height;
  ipc::Shmem mem;
  if (!mScreenPixelsTarget->AllocPixelBuffer(
          bufferWidth * bufferHeight * sizeof(uint32_t), &mem)) {
    // Failed to alloc shmem, Just bail out.
    return;
  }
  CompositorOGL* compositor = mCompositor->AsCompositorOGL();
  GLContext* gl = compositor->gl();
  MOZ_ASSERT(gl);
  gl->fReadPixels(0, 0, bufferWidth, bufferHeight, LOCAL_GL_RGBA,
                  LOCAL_GL_UNSIGNED_BYTE, mem.get<uint8_t>());
  Unused << mScreenPixelsTarget->SendScreenPixels(
      mem, ScreenIntSize(bufferWidth, bufferHeight));
  mScreenPixelsTarget = nullptr;
}
#endif

already_AddRefed<PaintedLayer> LayerManagerComposite::CreatePaintedLayer() {
  if (mDestroyed) {
    NS_WARNING("Call on destroyed layer manager");
    return nullptr;
  }
  return RefPtr<PaintedLayer>(new PaintedLayerComposite(this)).forget();
}

already_AddRefed<ContainerLayer> LayerManagerComposite::CreateContainerLayer() {
  if (mDestroyed) {
    NS_WARNING("Call on destroyed layer manager");
    return nullptr;
  }
  return RefPtr<ContainerLayer>(new ContainerLayerComposite(this)).forget();
}

already_AddRefed<ImageLayer> LayerManagerComposite::CreateImageLayer() {
  if (mDestroyed) {
    NS_WARNING("Call on destroyed layer manager");
    return nullptr;
  }
  return RefPtr<ImageLayer>(new ImageLayerComposite(this)).forget();
}

already_AddRefed<ColorLayer> LayerManagerComposite::CreateColorLayer() {
  if (LayerManagerComposite::mDestroyed) {
    NS_WARNING("Call on destroyed layer manager");
    return nullptr;
  }
  return RefPtr<ColorLayer>(new ColorLayerComposite(this)).forget();
}

already_AddRefed<CanvasLayer> LayerManagerComposite::CreateCanvasLayer() {
  if (LayerManagerComposite::mDestroyed) {
    NS_WARNING("Call on destroyed layer manager");
    return nullptr;
  }
  return RefPtr<CanvasLayer>(new CanvasLayerComposite(this)).forget();
}

already_AddRefed<RefLayer> LayerManagerComposite::CreateRefLayer() {
  if (LayerManagerComposite::mDestroyed) {
    NS_WARNING("Call on destroyed layer manager");
    return nullptr;
  }
  return RefPtr<RefLayer>(new RefLayerComposite(this)).forget();
}

LayerManagerComposite::AutoAddMaskEffect::AutoAddMaskEffect(
    Layer* aMaskLayer, EffectChain& aEffects)
    : mCompositable(nullptr), mFailed(false) {
  if (!aMaskLayer) {
    return;
  }

  mCompositable = ToLayerComposite(aMaskLayer)->GetCompositableHost();
  if (!mCompositable) {
    NS_WARNING("Mask layer with no compositable host");
    mFailed = true;
    return;
  }

  if (!mCompositable->AddMaskEffect(aEffects,
                                    aMaskLayer->GetEffectiveTransform())) {
    mCompositable = nullptr;
    mFailed = true;
  }
}

LayerManagerComposite::AutoAddMaskEffect::~AutoAddMaskEffect() {
  if (!mCompositable) {
    return;
  }

  mCompositable->RemoveMaskEffect();
}

bool LayerManagerComposite::IsCompositingToScreen() const {
  if (!mCompositor) {
    return true;
  }
  return !mCompositor->GetTargetContext();
}

LayerComposite::LayerComposite(LayerManagerComposite* aManager)
    : HostLayer(aManager),
      mCompositeManager(aManager),
      mCompositor(aManager->GetCompositor()),
      mDestroyed(false),
      mLayerComposited(false) {}

LayerComposite::~LayerComposite() {}

void LayerComposite::Destroy() {
  if (!mDestroyed) {
    mDestroyed = true;
    CleanupResources();
  }
}

void LayerComposite::AddBlendModeEffect(EffectChain& aEffectChain) {
  gfx::CompositionOp blendMode = GetLayer()->GetEffectiveMixBlendMode();
  if (blendMode == gfx::CompositionOp::OP_OVER) {
    return;
  }

  aEffectChain.mSecondaryEffects[EffectTypes::BLEND_MODE] =
      new EffectBlendMode(blendMode);
}

bool LayerManagerComposite::CanUseCanvasLayerForSize(const IntSize& aSize) {
  return mCompositor->CanUseCanvasLayerForSize(
      gfx::IntSize(aSize.width, aSize.height));
}

void LayerManagerComposite::NotifyShadowTreeTransaction() {
  if (gfxPrefs::LayersDrawFPS()) {
    mDiagnostics->AddTxnFrame();
  }
}

void LayerComposite::SetLayerManager(HostLayerManager* aManager) {
  HostLayer::SetLayerManager(aManager);
  mCompositeManager = static_cast<LayerManagerComposite*>(aManager);
  mCompositor = mCompositeManager->GetCompositor();
}

bool LayerManagerComposite::AsyncPanZoomEnabled() const {
  if (CompositorBridgeParent* bridge =
          mCompositor->GetCompositorBridgeParent()) {
    return bridge->GetOptions().UseAPZ();
  }
  return false;
}

bool LayerManagerComposite::AlwaysScheduleComposite() const {
  return !!(mCompositor->GetDiagnosticTypes() & DiagnosticTypes::FLASH_BORDERS);
}

nsIntRegion LayerComposite::GetFullyRenderedRegion() {
  if (TiledContentHost* tiled =
          GetCompositableHost() ? GetCompositableHost()->AsTiledContentHost()
                                : nullptr) {
    nsIntRegion shadowVisibleRegion =
        GetShadowVisibleRegion().ToUnknownRegion();
    // Discard the region which hasn't been drawn yet when doing
    // progressive drawing. Note that if the shadow visible region
    // shrunk the tiled valig region may not have discarded this yet.
    shadowVisibleRegion.And(shadowVisibleRegion, tiled->GetValidRegion());
    return shadowVisibleRegion;
  } else {
    return GetShadowVisibleRegion().ToUnknownRegion();
  }
}

Matrix4x4 HostLayer::GetShadowTransform() {
  Matrix4x4 transform = mShadowTransform;
  Layer* layer = GetLayer();

  transform.PostScale(layer->GetPostXScale(), layer->GetPostYScale(), 1.0f);
  if (const ContainerLayer* c = layer->AsContainerLayer()) {
    transform.PreScale(c->GetPreXScale(), c->GetPreYScale(), 1.0f);
  }

  return transform;
}

static LayerIntRect TransformRect(const LayerIntRect& aRect,
                                  const Matrix4x4& aTransform) {
  if (aRect.IsEmpty()) {
    return LayerIntRect();
  }

  Rect rect(aRect.X(), aRect.Y(), aRect.Width(), aRect.Height());
  rect = aTransform.TransformAndClipBounds(rect, Rect::MaxIntRect());
  rect.RoundOut();

  IntRect intRect;
  if (!rect.ToIntRect(&intRect)) {
    intRect = IntRect::MaxIntRect();
  }

  return ViewAs<LayerPixel>(intRect);
}

static void AddTransformedRegion(LayerIntRegion& aDest,
                                 const LayerIntRegion& aSource,
                                 const Matrix4x4& aTransform) {
  for (auto iter = aSource.RectIter(); !iter.Done(); iter.Next()) {
    aDest.Or(aDest, TransformRect(iter.Get(), aTransform));
  }
  aDest.SimplifyOutward(20);
}

// Async animations can move child layers without updating our visible region.
// PostProcessLayers will recompute visible regions for layers with an
// intermediate surface, but otherwise we need to do it now.
void ComputeVisibleRegionForChildren(ContainerLayer* aContainer,
                                     LayerIntRegion& aResult) {
  for (Layer* l = aContainer->GetFirstChild(); l; l = l->GetNextSibling()) {
    if (l->Extend3DContext()) {
      MOZ_ASSERT(l->AsContainerLayer());
      ComputeVisibleRegionForChildren(l->AsContainerLayer(), aResult);
    } else {
      AddTransformedRegion(aResult, l->GetLocalVisibleRegion(),
                           l->ComputeTransformToPreserve3DRoot());
    }
  }
}

void HostLayer::RecomputeShadowVisibleRegionFromChildren() {
  mShadowVisibleRegion.SetEmpty();
  ContainerLayer* container = GetLayer()->AsContainerLayer();
  MOZ_ASSERT(container);
  if (container) {
    ComputeVisibleRegionForChildren(container, mShadowVisibleRegion);
  }
}

bool LayerComposite::HasStaleCompositor() const {
  return mCompositeManager->GetCompositor() != mCompositor;
}

#ifndef MOZ_HAVE_PLATFORM_SPECIFIC_LAYER_BUFFERS

/*static*/ bool LayerManagerComposite::SupportsDirectTexturing() {
  return false;
}

/*static*/ void LayerManagerComposite::PlatformSyncBeforeReplyUpdate() {}

#endif  // !defined(MOZ_HAVE_PLATFORM_SPECIFIC_LAYER_BUFFERS)

}  // namespace layers
}  // namespace mozilla
