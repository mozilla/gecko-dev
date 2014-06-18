/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdint.h>                     // for uint32_t
#include <stdlib.h>                     // for rand, RAND_MAX
#include <sys/types.h>                  // for int32_t
#include "BasicContainerLayer.h"        // for BasicContainerLayer
#include "BasicLayersImpl.h"            // for ToData, BasicReadbackLayer, etc
#include "GeckoProfiler.h"              // for PROFILER_LABEL
#include "ImageContainer.h"             // for ImageFactory
#include "Layers.h"                     // for Layer, ContainerLayer, etc
#include "ReadbackLayer.h"              // for ReadbackLayer
#include "ReadbackProcessor.h"          // for ReadbackProcessor
#include "RenderTrace.h"                // for RenderTraceLayers, etc
#include "basic/BasicImplData.h"        // for BasicImplData
#include "basic/BasicLayers.h"          // for BasicLayerManager, etc
#include "gfx3DMatrix.h"                // for gfx3DMatrix
#include "gfxASurface.h"                // for gfxASurface, etc
#include "gfxCachedTempSurface.h"       // for gfxCachedTempSurface
#include "gfxColor.h"                   // for gfxRGBA
#include "gfxContext.h"                 // for gfxContext, etc
#include "gfxImageSurface.h"            // for gfxImageSurface
#include "gfxMatrix.h"                  // for gfxMatrix
#include "gfxPlatform.h"                // for gfxPlatform
#include "gfxPrefs.h"                   // for gfxPrefs
#include "gfxPoint.h"                   // for gfxIntSize, gfxPoint
#include "gfxRect.h"                    // for gfxRect
#include "gfxUtils.h"                   // for gfxUtils
#include "gfx2DGlue.h"                  // for thebes --> moz2d transition
#include "mozilla/Assertions.h"         // for MOZ_ASSERT, etc
#include "mozilla/WidgetUtils.h"        // for ScreenRotation
#include "mozilla/gfx/2D.h"             // for DrawTarget
#include "mozilla/gfx/BasePoint.h"      // for BasePoint
#include "mozilla/gfx/BaseRect.h"       // for BaseRect
#include "mozilla/gfx/Matrix.h"         // for Matrix
#include "mozilla/gfx/Rect.h"           // for IntRect, Rect
#include "mozilla/layers/LayersTypes.h"  // for BufferMode::BUFFER_NONE, etc
#include "mozilla/mozalloc.h"           // for operator new
#include "nsAutoPtr.h"                  // for nsRefPtr
#include "nsCOMPtr.h"                   // for already_AddRefed
#include "nsDebug.h"                    // for NS_ASSERTION, etc
#include "nsISupportsImpl.h"            // for gfxContext::Release, etc
#include "nsPoint.h"                    // for nsIntPoint
#include "nsRect.h"                     // for nsIntRect
#include "nsRegion.h"                   // for nsIntRegion, etc
#include "nsTArray.h"                   // for nsAutoTArray
#define PIXMAN_DONT_DEFINE_STDINT
#include "pixman.h"                     // for pixman_f_transform, etc

class nsIWidget;

namespace mozilla {
namespace layers {

using namespace mozilla::gfx;

/**
 * Clips to the smallest device-pixel-aligned rectangle containing aRect
 * in user space.
 * Returns true if the clip is "perfect", i.e. we actually clipped exactly to
 * aRect.
 */
static bool
ClipToContain(gfxContext* aContext, const nsIntRect& aRect)
{
  gfxRect userRect(aRect.x, aRect.y, aRect.width, aRect.height);
  gfxRect deviceRect = aContext->UserToDevice(userRect);
  deviceRect.RoundOut();

  gfxMatrix currentMatrix = aContext->CurrentMatrix();
  aContext->IdentityMatrix();
  aContext->NewPath();
  aContext->Rectangle(deviceRect);
  aContext->Clip();
  aContext->SetMatrix(currentMatrix);

  return aContext->DeviceToUser(deviceRect).IsEqualInterior(userRect);
}

already_AddRefed<gfxContext>
BasicLayerManager::PushGroupForLayer(gfxContext* aContext, Layer* aLayer,
                                     const nsIntRegion& aRegion,
                                     bool* aNeedsClipToVisibleRegion)
{
  // If we need to call PushGroup, we should clip to the smallest possible
  // area first to minimize the size of the temporary surface.
  bool didCompleteClip = ClipToContain(aContext, aRegion.GetBounds());

  nsRefPtr<gfxContext> result;
  if (aLayer->CanUseOpaqueSurface() &&
      ((didCompleteClip && aRegion.GetNumRects() == 1) ||
       !aContext->CurrentMatrix().HasNonIntegerTranslation())) {
    // If the layer is opaque in its visible region we can push a gfxContentType::COLOR
    // group. We need to make sure that only pixels inside the layer's visible
    // region are copied back to the destination. Remember if we've already
    // clipped precisely to the visible region.
    *aNeedsClipToVisibleRegion = !didCompleteClip || aRegion.GetNumRects() > 1;
    MOZ_ASSERT(!aContext->IsCairo());
    result = PushGroupWithCachedSurface(aContext, gfxContentType::COLOR);
  } else {
    *aNeedsClipToVisibleRegion = false;
    result = aContext;
    if (aLayer->GetContentFlags() & Layer::CONTENT_COMPONENT_ALPHA) {
      aContext->PushGroupAndCopyBackground(gfxContentType::COLOR_ALPHA);
    } else {
      aContext->PushGroup(gfxContentType::COLOR_ALPHA);
    }
  }
  return result.forget();
}

static nsIntRect
ToOutsideIntRect(const gfxRect &aRect)
{
  gfxRect r = aRect;
  r.RoundOut();
  return nsIntRect(r.X(), r.Y(), r.Width(), r.Height());
}

static nsIntRect
ToInsideIntRect(const gfxRect& aRect)
{
  gfxRect r = aRect;
  r.RoundIn();
  return nsIntRect(r.X(), r.Y(), r.Width(), r.Height());
}

// A context helper for BasicLayerManager::PaintLayer() that holds all the
// painting context together in a data structure so it can be easily passed
// around. It also uses ensures that the Transform and Opaque rect are restored
// to their former state on destruction.

class PaintLayerContext {
public:
  PaintLayerContext(gfxContext* aTarget, Layer* aLayer,
                    LayerManager::DrawThebesLayerCallback aCallback,
                    void* aCallbackData)
   : mTarget(aTarget)
   , mTargetMatrixSR(aTarget)
   , mLayer(aLayer)
   , mCallback(aCallback)
   , mCallbackData(aCallbackData)
   , mPushedOpaqueRect(false)
  {}

  ~PaintLayerContext()
  {
    // Matrix is restored by mTargetMatrixSR
    if (mPushedOpaqueRect)
    {
      ClearOpaqueRect();
    }
  }

  // Gets the effective transform and returns true if it is a 2D
  // transform.
  bool Setup2DTransform()
  {
    // Will return an identity matrix for 3d transforms.
    return mLayer->GetEffectiveTransform().CanDraw2D(&mTransform);
  }

  // Applies the effective transform if it's 2D. If it's a 3D transform then
  // it applies an identity.
  void Apply2DTransform()
  {
    mTarget->SetMatrix(ThebesMatrix(mTransform));
  }

  // Set the opaque rect to match the bounds of the visible region.
  void AnnotateOpaqueRect()
  {
    const nsIntRegion& visibleRegion = mLayer->GetEffectiveVisibleRegion();
    const nsIntRect& bounds = visibleRegion.GetBounds();

    if (mTarget->IsCairo()) {
      nsRefPtr<gfxASurface> currentSurface = mTarget->CurrentSurface();
      const gfxRect& targetOpaqueRect = currentSurface->GetOpaqueRect();

      // Try to annotate currentSurface with a region of pixels that have been
      // (or will be) painted opaque, if no such region is currently set.
      if (targetOpaqueRect.IsEmpty() && visibleRegion.GetNumRects() == 1 &&
          (mLayer->GetContentFlags() & Layer::CONTENT_OPAQUE) &&
          !mTransform.HasNonAxisAlignedTransform()) {
        currentSurface->SetOpaqueRect(
            mTarget->UserToDevice(gfxRect(bounds.x, bounds.y, bounds.width, bounds.height)));
        mPushedOpaqueRect = true;
      }
    } else {
      DrawTarget *dt = mTarget->GetDrawTarget();
      const IntRect& targetOpaqueRect = dt->GetOpaqueRect();

      // Try to annotate currentSurface with a region of pixels that have been
      // (or will be) painted opaque, if no such region is currently set.
      if (targetOpaqueRect.IsEmpty() && visibleRegion.GetNumRects() == 1 &&
          (mLayer->GetContentFlags() & Layer::CONTENT_OPAQUE) &&
          !mTransform.HasNonAxisAlignedTransform()) {

        gfx::Rect opaqueRect = dt->GetTransform().TransformBounds(
          gfx::Rect(bounds.x, bounds.y, bounds.width, bounds.height));
        opaqueRect.RoundIn();
        IntRect intOpaqueRect;
        if (opaqueRect.ToIntRect(&intOpaqueRect)) {
          mTarget->GetDrawTarget()->SetOpaqueRect(intOpaqueRect);
          mPushedOpaqueRect = true;
        }
      }
    }
  }

  // Clear the Opaque rect. Although this doesn't really restore it to it's
  // previous state it will happen on the exit path of the PaintLayer() so when
  // painting is complete the opaque rect qill be clear.
  void ClearOpaqueRect() {
    if (mTarget->IsCairo()) {
      nsRefPtr<gfxASurface> currentSurface = mTarget->CurrentSurface();
      currentSurface->SetOpaqueRect(gfxRect());
    } else {
      mTarget->GetDrawTarget()->SetOpaqueRect(IntRect());
    }
  }

  gfxContext* mTarget;
  gfxContextMatrixAutoSaveRestore mTargetMatrixSR;
  Layer* mLayer;
  LayerManager::DrawThebesLayerCallback mCallback;
  void* mCallbackData;
  Matrix mTransform;
  bool mPushedOpaqueRect;
};

BasicLayerManager::BasicLayerManager(nsIWidget* aWidget) :
  mPhase(PHASE_NONE),
  mWidget(aWidget)
  , mDoubleBuffering(BufferMode::BUFFER_NONE), mUsingDefaultTarget(false)
  , mCachedSurfaceInUse(false)
  , mTransactionIncomplete(false)
  , mCompositorMightResample(false)
{
  MOZ_COUNT_CTOR(BasicLayerManager);
  NS_ASSERTION(aWidget, "Must provide a widget");
}

BasicLayerManager::BasicLayerManager() :
  mPhase(PHASE_NONE),
  mWidget(nullptr)
  , mDoubleBuffering(BufferMode::BUFFER_NONE), mUsingDefaultTarget(false)
  , mCachedSurfaceInUse(false)
  , mTransactionIncomplete(false)
{
  MOZ_COUNT_CTOR(BasicLayerManager);
}

BasicLayerManager::~BasicLayerManager()
{
  NS_ASSERTION(!InTransaction(), "Died during transaction?");

  ClearCachedResources();

  mRoot = nullptr;

  MOZ_COUNT_DTOR(BasicLayerManager);
}

void
BasicLayerManager::SetDefaultTarget(gfxContext* aContext)
{
  NS_ASSERTION(!InTransaction(),
               "Must set default target outside transaction");
  mDefaultTarget = aContext;
}

void
BasicLayerManager::SetDefaultTargetConfiguration(BufferMode aDoubleBuffering, ScreenRotation aRotation)
{
  mDoubleBuffering = aDoubleBuffering;
}

void
BasicLayerManager::BeginTransaction()
{
  mInTransaction = true;
  mUsingDefaultTarget = true;
  BeginTransactionWithTarget(mDefaultTarget);
}

already_AddRefed<gfxContext>
BasicLayerManager::PushGroupWithCachedSurface(gfxContext *aTarget,
                                              gfxContentType aContent)
{
  nsRefPtr<gfxContext> ctx;
  // We can't cache Azure DrawTargets at this point.
  if (!mCachedSurfaceInUse && aTarget->IsCairo()) {
    gfxContextMatrixAutoSaveRestore saveMatrix(aTarget);
    aTarget->IdentityMatrix();

    nsRefPtr<gfxASurface> currentSurf = aTarget->CurrentSurface();
    gfxRect clip = aTarget->GetClipExtents();
    clip.RoundOut();

    ctx = mCachedSurface.Get(aContent, clip, currentSurf);

    if (ctx) {
      mCachedSurfaceInUse = true;
      /* Align our buffer for the original surface */
      ctx->SetMatrix(saveMatrix.Matrix());
      return ctx.forget();
    }
  }

  ctx = aTarget;
  ctx->PushGroup(aContent);
  return ctx.forget();
}

void
BasicLayerManager::PopGroupToSourceWithCachedSurface(gfxContext *aTarget, gfxContext *aPushed)
{
  if (!aTarget)
    return;
  if (aTarget->IsCairo()) {
    nsRefPtr<gfxASurface> current = aPushed->CurrentSurface();
    if (mCachedSurface.IsSurface(current)) {
      gfxContextMatrixAutoSaveRestore saveMatrix(aTarget);
      aTarget->IdentityMatrix();
      aTarget->SetSource(current);
      mCachedSurfaceInUse = false;
      return;
    }
  }
  aTarget->PopGroupToSource();
}

void
BasicLayerManager::BeginTransactionWithTarget(gfxContext* aTarget)
{
  mInTransaction = true;

#ifdef MOZ_LAYERS_HAVE_LOG
  MOZ_LAYERS_LOG(("[----- BeginTransaction"));
  Log();
#endif

  NS_ASSERTION(!InTransaction(), "Nested transactions not allowed");
  mPhase = PHASE_CONSTRUCTION;
  mTarget = aTarget;
}

static void
TransformIntRect(nsIntRect& aRect, const Matrix& aMatrix,
                 nsIntRect (*aRoundMethod)(const gfxRect&))
{
  Rect gr = Rect(aRect.x, aRect.y, aRect.width, aRect.height);
  gr = aMatrix.TransformBounds(gr);
  aRect = (*aRoundMethod)(ThebesRect(gr));
}

/**
 * This function assumes that GetEffectiveTransform transforms
 * all layers to the same coordinate system (the "root coordinate system").
 * It can't be used as is by accelerated layers because of intermediate surfaces.
 * This must set the hidden flag to true or false on *all* layers in the subtree.
 * It also sets the operator for all layers to "OVER", and call
 * SetDrawAtomically(false).
 * It clears mClipToVisibleRegion on all layers.
 * @param aClipRect the cliprect, in the root coordinate system. We assume
 * that any layer drawing is clipped to this rect. It is therefore not
 * allowed to add to the opaque region outside that rect.
 * @param aDirtyRect the dirty rect that will be painted, in the root
 * coordinate system. Layers outside this rect should be hidden.
 * @param aOpaqueRegion the opaque region covering aLayer, in the
 * root coordinate system.
 */
enum {
    ALLOW_OPAQUE = 0x01,
};
static void
MarkLayersHidden(Layer* aLayer, const nsIntRect& aClipRect,
                 const nsIntRect& aDirtyRect,
                 nsIntRegion& aOpaqueRegion,
                 uint32_t aFlags)
{
  nsIntRect newClipRect(aClipRect);
  uint32_t newFlags = aFlags;

  // Allow aLayer or aLayer's descendants to cover underlying layers
  // only if it's opaque.
  if (aLayer->GetOpacity() != 1.0f) {
    newFlags &= ~ALLOW_OPAQUE;
  }

  {
    const nsIntRect* clipRect = aLayer->GetEffectiveClipRect();
    if (clipRect) {
      nsIntRect cr = *clipRect;
      // clipRect is in the container's coordinate system. Get it into the
      // global coordinate system.
      if (aLayer->GetParent()) {
        Matrix tr;
        if (aLayer->GetParent()->GetEffectiveTransform().CanDraw2D(&tr)) {
          // Clip rect is applied after aLayer's transform, i.e., in the coordinate
          // system of aLayer's parent.
          TransformIntRect(cr, tr, ToInsideIntRect);
        } else {
          cr.SetRect(0, 0, 0, 0);
        }
      }
      newClipRect.IntersectRect(newClipRect, cr);
    }
  }

  BasicImplData* data = ToData(aLayer);
  data->SetOperator(CompositionOp::OP_OVER);
  data->SetClipToVisibleRegion(false);
  data->SetDrawAtomically(false);

  if (!aLayer->AsContainerLayer()) {
    Matrix transform;
    if (!aLayer->GetEffectiveTransform().CanDraw2D(&transform)) {
      data->SetHidden(false);
      return;
    }

    nsIntRegion region = aLayer->GetEffectiveVisibleRegion();
    nsIntRect r = region.GetBounds();
    TransformIntRect(r, transform, ToOutsideIntRect);
    r.IntersectRect(r, aDirtyRect);
    data->SetHidden(aOpaqueRegion.Contains(r));

    // Allow aLayer to cover underlying layers only if aLayer's
    // content is opaque
    if ((aLayer->GetContentFlags() & Layer::CONTENT_OPAQUE) &&
        (newFlags & ALLOW_OPAQUE)) {
      nsIntRegionRectIterator it(region);
      while (const nsIntRect* sr = it.Next()) {
        r = *sr;
        TransformIntRect(r, transform, ToInsideIntRect);

        r.IntersectRect(r, newClipRect);
        aOpaqueRegion.Or(aOpaqueRegion, r);
      }
    }
  } else {
    Layer* child = aLayer->GetLastChild();
    bool allHidden = true;
    for (; child; child = child->GetPrevSibling()) {
      MarkLayersHidden(child, newClipRect, aDirtyRect, aOpaqueRegion, newFlags);
      if (!ToData(child)->IsHidden()) {
        allHidden = false;
      }
    }
    data->SetHidden(allHidden);
  }
}

/**
 * This function assumes that GetEffectiveTransform transforms
 * all layers to the same coordinate system (the "root coordinate system").
 * MarkLayersHidden must be called before calling this.
 * @param aVisibleRect the rectangle of aLayer that is visible (i.e. not
 * clipped and in the dirty rect), in the root coordinate system.
 */
static void
ApplyDoubleBuffering(Layer* aLayer, const nsIntRect& aVisibleRect)
{
  BasicImplData* data = ToData(aLayer);
  if (data->IsHidden())
    return;

  nsIntRect newVisibleRect(aVisibleRect);

  {
    const nsIntRect* clipRect = aLayer->GetEffectiveClipRect();
    if (clipRect) {
      nsIntRect cr = *clipRect;
      // clipRect is in the container's coordinate system. Get it into the
      // global coordinate system.
      if (aLayer->GetParent()) {
        Matrix tr;
        if (aLayer->GetParent()->GetEffectiveTransform().CanDraw2D(&tr)) {
          NS_ASSERTION(!ThebesMatrix(tr).HasNonIntegerTranslation(),
                       "Parent can only have an integer translation");
          cr += nsIntPoint(int32_t(tr._31), int32_t(tr._32));
        } else {
          NS_ERROR("Parent can only have an integer translation");
        }
      }
      newVisibleRect.IntersectRect(newVisibleRect, cr);
    }
  }

  BasicContainerLayer* container =
    static_cast<BasicContainerLayer*>(aLayer->AsContainerLayer());
  // Layers that act as their own backbuffers should be drawn to the destination
  // using OPERATOR_SOURCE to ensure that alpha values in a transparent window
  // are cleared. This can also be faster than OPERATOR_OVER.
  if (!container) {
    data->SetOperator(CompositionOp::OP_SOURCE);
    data->SetDrawAtomically(true);
  } else {
    if (container->UseIntermediateSurface() ||
        !container->ChildrenPartitionVisibleRegion(newVisibleRect)) {
      // We need to double-buffer this container.
      data->SetOperator(CompositionOp::OP_SOURCE);
      container->ForceIntermediateSurface();
    } else {
      // Tell the children to clip to their visible regions so our assumption
      // that they don't paint outside their visible regions is valid!
      for (Layer* child = aLayer->GetFirstChild(); child;
           child = child->GetNextSibling()) {
        ToData(child)->SetClipToVisibleRegion(true);
        ApplyDoubleBuffering(child, newVisibleRect);
      }
    }
  }
}

void
BasicLayerManager::EndTransaction(DrawThebesLayerCallback aCallback,
                                  void* aCallbackData,
                                  EndTransactionFlags aFlags)
{
  mInTransaction = false;

  EndTransactionInternal(aCallback, aCallbackData, aFlags);
}

void
BasicLayerManager::AbortTransaction()
{
  NS_ASSERTION(InConstruction(), "Should be in construction phase");
  mPhase = PHASE_NONE;
  mUsingDefaultTarget = false;
  mInTransaction = false;
}

static uint16_t sFrameCount = 0;
void
BasicLayerManager::RenderDebugOverlay()
{
  if (!gfxPrefs::DrawFrameCounter()) {
    return;
  }

  profiler_set_frame_number(sFrameCount);

  uint16_t frameNumber = sFrameCount;
  const uint16_t bitWidth = 3;
  for (size_t i = 0; i < 16; i++) {

    gfxRGBA bitColor;
    if ((frameNumber >> i) & 0x1) {
      bitColor = gfxRGBA(0, 0, 0, 1.0);
    } else {
      bitColor = gfxRGBA(1.0, 1.0, 1.0, 1.0);
    }
    mTarget->NewPath();
    mTarget->SetColor(bitColor);
    mTarget->Rectangle(gfxRect(bitWidth*i, 0, bitWidth, bitWidth));
    mTarget->Fill();
  }
  // We intentionally overflow at 2^16.
  sFrameCount++;
}

bool
BasicLayerManager::EndTransactionInternal(DrawThebesLayerCallback aCallback,
                                          void* aCallbackData,
                                          EndTransactionFlags aFlags)
{
  PROFILER_LABEL("BasicLayerManager", "EndTransactionInternal",
    js::ProfileEntry::Category::GRAPHICS);

#ifdef MOZ_LAYERS_HAVE_LOG
  MOZ_LAYERS_LOG(("  ----- (beginning paint)"));
  Log();
#endif

  NS_ASSERTION(InConstruction(), "Should be in construction phase");
  mPhase = PHASE_DRAWING;

  RenderTraceLayers(mRoot, "FF00");

  mTransactionIncomplete = false;

  if (mRoot) {
    // Need to do this before we call ApplyDoubleBuffering,
    // which depends on correct effective transforms
    mSnapEffectiveTransforms =
      mTarget ? !(mTarget->GetFlags() & gfxContext::FLAG_DISABLE_SNAPPING) : true;
    mRoot->ComputeEffectiveTransforms(mTarget ? Matrix4x4::From2D(ToMatrix(mTarget->CurrentMatrix())) : Matrix4x4());

    ToData(mRoot)->Validate(aCallback, aCallbackData, nullptr);
    if (mRoot->GetMaskLayer()) {
      ToData(mRoot->GetMaskLayer())->Validate(aCallback, aCallbackData, nullptr);
    }

    if (aFlags & END_NO_COMPOSITE) {
      // Apply pending tree updates before recomputing effective
      // properties.
      mRoot->ApplyPendingUpdatesToSubtree();
    }
  }

  if (mTarget && mRoot &&
      !(aFlags & END_NO_IMMEDIATE_REDRAW) &&
      !(aFlags & END_NO_COMPOSITE)) {
    nsIntRect clipRect;

    {
      gfxContextMatrixAutoSaveRestore save(mTarget);
      mTarget->SetMatrix(gfxMatrix());
      clipRect = ToOutsideIntRect(mTarget->GetClipExtents());
    }

    if (IsRetained()) {
      nsIntRegion region;
      MarkLayersHidden(mRoot, clipRect, clipRect, region, ALLOW_OPAQUE);
      if (mUsingDefaultTarget && mDoubleBuffering != BufferMode::BUFFER_NONE) {
        ApplyDoubleBuffering(mRoot, clipRect);
      }
    }

    PaintLayer(mTarget, mRoot, aCallback, aCallbackData);
    if (!mRegionToClear.IsEmpty()) {
      AutoSetOperator op(mTarget, gfxContext::OPERATOR_CLEAR);
      nsIntRegionRectIterator iter(mRegionToClear);
      const nsIntRect *r;
      while ((r = iter.Next())) {
        mTarget->NewPath();
        mTarget->Rectangle(gfxRect(r->x, r->y, r->width, r->height));
        mTarget->Fill();
      }
    }
    if (mWidget) {
      FlashWidgetUpdateArea(mTarget);
    }
    RenderDebugOverlay();
    RecordFrame();
    PostPresent();

    if (!mTransactionIncomplete) {
      // Clear out target if we have a complete transaction.
      mTarget = nullptr;
    }
  }

#ifdef MOZ_LAYERS_HAVE_LOG
  Log();
  MOZ_LAYERS_LOG(("]----- EndTransaction"));
#endif

  // Go back to the construction phase if the transaction isn't complete.
  // Layout will update the layer tree and call EndTransaction().
  mPhase = mTransactionIncomplete ? PHASE_CONSTRUCTION : PHASE_NONE;

  if (!mTransactionIncomplete) {
    // This is still valid if the transaction was incomplete.
    mUsingDefaultTarget = false;
  }

  NS_ASSERTION(!aCallback || !mTransactionIncomplete,
               "If callback is not null, transaction must be complete");

  // XXX - We should probably assert here that for an incomplete transaction
  // out target is the default target.

  return !mTransactionIncomplete;
}

void
BasicLayerManager::FlashWidgetUpdateArea(gfxContext *aContext)
{
  if (gfxPrefs::WidgetUpdateFlashing()) {
    float r = float(rand()) / RAND_MAX;
    float g = float(rand()) / RAND_MAX;
    float b = float(rand()) / RAND_MAX;
    aContext->SetColor(gfxRGBA(r, g, b, 0.2));
    aContext->Paint();
  }
}

bool
BasicLayerManager::EndEmptyTransaction(EndTransactionFlags aFlags)
{
  mInTransaction = false;

  if (!mRoot) {
    return false;
  }

  return EndTransactionInternal(nullptr, nullptr, aFlags);
}

void
BasicLayerManager::SetRoot(Layer* aLayer)
{
  NS_ASSERTION(aLayer, "Root can't be null");
  NS_ASSERTION(aLayer->Manager() == this, "Wrong manager");
  NS_ASSERTION(InConstruction(), "Only allowed in construction phase");
  mRoot = aLayer;
}

static pixman_transform
BasicLayerManager_Matrix3DToPixman(const gfx3DMatrix& aMatrix)
{
  pixman_f_transform transform;

  transform.m[0][0] = aMatrix._11;
  transform.m[0][1] = aMatrix._21;
  transform.m[0][2] = aMatrix._41;
  transform.m[1][0] = aMatrix._12;
  transform.m[1][1] = aMatrix._22;
  transform.m[1][2] = aMatrix._42;
  transform.m[2][0] = aMatrix._14;
  transform.m[2][1] = aMatrix._24;
  transform.m[2][2] = aMatrix._44;

  pixman_transform result;
  pixman_transform_from_pixman_f_transform(&result, &transform);

  return result;
}

static void
PixmanTransform(const gfxImageSurface* aDest,
                RefPtr<DataSourceSurface> aSrc,
                const gfx3DMatrix& aTransform,
                gfxPoint aDestOffset)
{
  IntSize destSize = ToIntSize(aDest->GetSize());
  pixman_image_t* dest = pixman_image_create_bits(aDest->Format() == gfxImageFormat::ARGB32 ? PIXMAN_a8r8g8b8 : PIXMAN_x8r8g8b8,
                                                  destSize.width,
                                                  destSize.height,
                                                  (uint32_t*)aDest->Data(),
                                                  aDest->Stride());

  IntSize srcSize = aSrc->GetSize();
  pixman_image_t* src = pixman_image_create_bits(aSrc->GetFormat() == SurfaceFormat::B8G8R8A8 ? PIXMAN_a8r8g8b8 : PIXMAN_x8r8g8b8,
                                                 srcSize.width,
                                                 srcSize.height,
                                                 (uint32_t*)aSrc->GetData(),
                                                 aSrc->Stride());

  NS_ABORT_IF_FALSE(src && dest, "Failed to create pixman images?");

  pixman_transform pixTransform = BasicLayerManager_Matrix3DToPixman(aTransform);
  pixman_transform pixTransformInverted;

  // If the transform is singular then nothing would be drawn anyway, return here
  if (!pixman_transform_invert(&pixTransformInverted, &pixTransform)) {
    return;
  }
  pixman_image_set_transform(src, &pixTransformInverted);

  pixman_image_composite32(PIXMAN_OP_SRC,
                           src,
                           nullptr,
                           dest,
                           aDestOffset.x,
                           aDestOffset.y,
                           0,
                           0,
                           0,
                           0,
                           destSize.width,
                           destSize.height);

  pixman_image_unref(dest);
  pixman_image_unref(src);
}

/**
 * Transform a surface using a gfx3DMatrix and blit to the destination if
 * it is efficient to do so.
 *
 * @param aSource       Source surface.
 * @param aDest         Desintation context.
 * @param aBounds       Area represented by aSource.
 * @param aTransform    Transformation matrix.
 * @param aDestRect     Output: rectangle in which to draw returned surface on aDest
 *                      (same size as aDest). Only filled in if this returns
 *                      a surface.
 * @return              Transformed surface
 */
static already_AddRefed<gfxASurface>
Transform3D(RefPtr<SourceSurface> aSource,
            gfxContext* aDest,
            const gfxRect& aBounds,
            const gfx3DMatrix& aTransform,
            gfxRect& aDestRect)
{
  // Find the transformed rectangle of our layer.
  gfxRect offsetRect = aTransform.TransformBounds(aBounds);

  // Intersect the transformed layer with the destination rectangle.
  // This is in device space since we have an identity transform set on aTarget.
  aDestRect = aDest->GetClipExtents();
  aDestRect.IntersectRect(aDestRect, offsetRect);
  aDestRect.RoundOut();

  // Create a surface the size of the transformed object.
  nsRefPtr<gfxASurface> dest = aDest->CurrentSurface();
  nsRefPtr<gfxImageSurface> destImage = new gfxImageSurface(gfxIntSize(aDestRect.width,
                                                                       aDestRect.height),
                                                            gfxImageFormat::ARGB32);
  gfxPoint offset = aDestRect.TopLeft();

  // Include a translation to the correct origin.
  gfx3DMatrix translation = gfx3DMatrix::Translation(aBounds.x, aBounds.y, 0);

  // Transform the content and offset it such that the content begins at the origin.
  PixmanTransform(destImage, aSource->GetDataSurface(), translation * aTransform, offset);

  // If we haven't actually drawn to aDest then return our temporary image so
  // that the caller can do this.
  return destImage.forget();
}

void
BasicLayerManager::PaintSelfOrChildren(PaintLayerContext& aPaintContext,
                                       gfxContext* aGroupTarget)
{
  BasicImplData* data = ToData(aPaintContext.mLayer);

  /* Only paint ourself, or our children - This optimization relies on this! */
  Layer* child = aPaintContext.mLayer->GetFirstChild();
  if (!child) {
    if (aPaintContext.mLayer->AsThebesLayer()) {
      data->PaintThebes(aGroupTarget, aPaintContext.mLayer->GetMaskLayer(),
          aPaintContext.mCallback, aPaintContext.mCallbackData);
    } else {
      data->Paint(aGroupTarget->GetDrawTarget(),
                  aGroupTarget->GetDeviceOffset(),
                  aPaintContext.mLayer->GetMaskLayer());
    }
  } else {
    ContainerLayer* container =
        static_cast<ContainerLayer*>(aPaintContext.mLayer);
    nsAutoTArray<Layer*, 12> children;
    container->SortChildrenBy3DZOrder(children);
    for (uint32_t i = 0; i < children.Length(); i++) {
      PaintLayer(aGroupTarget, children.ElementAt(i), aPaintContext.mCallback,
          aPaintContext.mCallbackData);
      if (mTransactionIncomplete)
        break;
    }
  }
}

void
BasicLayerManager::FlushGroup(PaintLayerContext& aPaintContext, bool aNeedsClipToVisibleRegion)
{
  // If we're doing our own double-buffering, we need to avoid drawing
  // the results of an incomplete transaction to the destination surface ---
  // that could cause flicker. Double-buffering is implemented using a
  // temporary surface for one or more container layers, so we need to stop
  // those temporary surfaces from being composited to aTarget.
  // ApplyDoubleBuffering guarantees that this container layer can't
  // intersect any other leaf layers, so if the transaction is not yet marked
  // incomplete, the contents of this container layer are the final contents
  // for the window.
  if (!mTransactionIncomplete) {
    if (aNeedsClipToVisibleRegion) {
      gfxUtils::ClipToRegion(aPaintContext.mTarget,
                             aPaintContext.mLayer->GetEffectiveVisibleRegion());
    }

    CompositionOp op = GetEffectiveOperator(aPaintContext.mLayer);
    AutoSetOperator setOperator(aPaintContext.mTarget, ThebesOp(op));

    PaintWithMask(aPaintContext.mTarget, aPaintContext.mLayer->GetEffectiveOpacity(),
                  aPaintContext.mLayer->GetMaskLayer());
  }
}

void
BasicLayerManager::PaintLayer(gfxContext* aTarget,
                              Layer* aLayer,
                              DrawThebesLayerCallback aCallback,
                              void* aCallbackData)
{
  PROFILER_LABEL("BasicLayerManager", "PaintLayer",
    js::ProfileEntry::Category::GRAPHICS);

  PaintLayerContext paintLayerContext(aTarget, aLayer, aCallback, aCallbackData);

  // Don't attempt to paint layers with a singular transform, cairo will
  // just throw an error.
  if (aLayer->GetEffectiveTransform().IsSingular()) {
    return;
  }

  RenderTraceScope trace("BasicLayerManager::PaintLayer", "707070");

  const nsIntRect* clipRect = aLayer->GetEffectiveClipRect();
  // aLayer might not be a container layer, but if so we take care not to use
  // the container variable
  BasicContainerLayer* container = static_cast<BasicContainerLayer*>(aLayer);
  bool needsGroup = aLayer->GetFirstChild() &&
                    container->UseIntermediateSurface();
  BasicImplData* data = ToData(aLayer);
  bool needsClipToVisibleRegion =
    data->GetClipToVisibleRegion() && !aLayer->AsThebesLayer();
  NS_ASSERTION(needsGroup || !aLayer->GetFirstChild() ||
               container->GetOperator() == CompositionOp::OP_OVER,
               "non-OVER operator should have forced UseIntermediateSurface");
  NS_ASSERTION(!aLayer->GetFirstChild() || !aLayer->GetMaskLayer() ||
               container->UseIntermediateSurface(),
               "ContainerLayer with mask layer should force UseIntermediateSurface");

  gfxContextAutoSaveRestore contextSR;
  gfxMatrix transform;
  // Will return an identity matrix for 3d transforms, and is handled separately below.
  bool is2D = paintLayerContext.Setup2DTransform();
  NS_ABORT_IF_FALSE(is2D || needsGroup || !aLayer->GetFirstChild(), "Must PushGroup for 3d transforms!");

  bool needsSaveRestore =
    needsGroup || clipRect || needsClipToVisibleRegion || !is2D;
  if (needsSaveRestore) {
    contextSR.SetContext(aTarget);

    if (clipRect) {
      aTarget->NewPath();
      aTarget->SnappedRectangle(gfxRect(clipRect->x, clipRect->y, clipRect->width, clipRect->height));
      aTarget->Clip();
    }
  }

  paintLayerContext.Apply2DTransform();

  const nsIntRegion& visibleRegion = aLayer->GetEffectiveVisibleRegion();
  // If needsGroup is true, we'll clip to the visible region after we've popped the group
  if (needsClipToVisibleRegion && !needsGroup) {
    gfxUtils::ClipToRegion(aTarget, visibleRegion);
    // Don't need to clip to visible region again
    needsClipToVisibleRegion = false;
  }
  
  if (is2D) {
    paintLayerContext.AnnotateOpaqueRect();
  }

  bool clipIsEmpty = !aTarget || aTarget->GetClipExtents().IsEmpty();
  if (clipIsEmpty) {
    PaintSelfOrChildren(paintLayerContext, aTarget);
    return;
  }

  if (is2D) {
    if (needsGroup) {
      nsRefPtr<gfxContext> groupTarget = PushGroupForLayer(aTarget, aLayer, aLayer->GetEffectiveVisibleRegion(),
                                      &needsClipToVisibleRegion);
      PaintSelfOrChildren(paintLayerContext, groupTarget);
      PopGroupToSourceWithCachedSurface(aTarget, groupTarget);
      FlushGroup(paintLayerContext, needsClipToVisibleRegion);
    } else {
      PaintSelfOrChildren(paintLayerContext, aTarget);
    }
  } else {
    const nsIntRect& bounds = visibleRegion.GetBounds();
    RefPtr<DrawTarget> untransformedDT =
      gfxPlatform::GetPlatform()->CreateOffscreenContentDrawTarget(IntSize(bounds.width, bounds.height),
                                                                   SurfaceFormat::B8G8R8A8);
    if (!untransformedDT) {
      return;
    }

    nsRefPtr<gfxContext> groupTarget = new gfxContext(untransformedDT,
                                                      Point(bounds.x, bounds.y));

    PaintSelfOrChildren(paintLayerContext, groupTarget);

    // Temporary fast fix for bug 725886
    // Revert these changes when 725886 is ready
    NS_ABORT_IF_FALSE(untransformedDT,
                      "We should always allocate an untransformed surface with 3d transforms!");
    gfxRect destRect;
#ifdef DEBUG
    if (aLayer->GetDebugColorIndex() != 0) {
      gfxRGBA  color((aLayer->GetDebugColorIndex() & 1) ? 1.0 : 0.0,
                     (aLayer->GetDebugColorIndex() & 2) ? 1.0 : 0.0,
                     (aLayer->GetDebugColorIndex() & 4) ? 1.0 : 0.0,
                     1.0);

      nsRefPtr<gfxContext> temp = new gfxContext(untransformedDT, Point(bounds.x, bounds.y));
      temp->SetColor(color);
      temp->Paint();
    }
#endif
    gfx3DMatrix effectiveTransform;
    gfx::To3DMatrix(aLayer->GetEffectiveTransform(), effectiveTransform);
    nsRefPtr<gfxASurface> result =
      Transform3D(untransformedDT->Snapshot(), aTarget, bounds,
                  effectiveTransform, destRect);

    if (result) {
      aTarget->SetSource(result, destRect.TopLeft());
      // Azure doesn't support EXTEND_NONE, so to avoid extending the edges
      // of the source surface out to the current clip region, clip to
      // the rectangle of the result surface now.
      aTarget->NewPath();
      aTarget->SnappedRectangle(destRect);
      aTarget->Clip();
      FlushGroup(paintLayerContext, needsClipToVisibleRegion);
    }
  }
}

void
BasicLayerManager::ClearCachedResources(Layer* aSubtree)
{
  MOZ_ASSERT(!aSubtree || aSubtree->Manager() == this);
  if (aSubtree) {
    ClearLayer(aSubtree);
  } else if (mRoot) {
    ClearLayer(mRoot);
  }
  mCachedSurface.Expire();
}
void
BasicLayerManager::ClearLayer(Layer* aLayer)
{
  ToData(aLayer)->ClearCachedResources();
  for (Layer* child = aLayer->GetFirstChild(); child;
       child = child->GetNextSibling()) {
    ClearLayer(child);
  }
}

already_AddRefed<ReadbackLayer>
BasicLayerManager::CreateReadbackLayer()
{
  NS_ASSERTION(InConstruction(), "Only allowed in construction phase");
  nsRefPtr<ReadbackLayer> layer = new BasicReadbackLayer(this);
  return layer.forget();
}

}
}
