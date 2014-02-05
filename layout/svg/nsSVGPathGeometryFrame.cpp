/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Main header first:
#include "nsSVGPathGeometryFrame.h"

// Keep others in (case-insensitive) order:
#include "gfxContext.h"
#include "gfxPlatform.h"
#include "gfxSVGGlyphs.h"
#include "nsDisplayList.h"
#include "nsGkAtoms.h"
#include "nsRenderingContext.h"
#include "nsSVGEffects.h"
#include "nsSVGIntegrationUtils.h"
#include "nsSVGMarkerFrame.h"
#include "nsSVGPathGeometryElement.h"
#include "nsSVGUtils.h"
#include "mozilla/ArrayUtils.h"
#include "SVGAnimatedTransformList.h"
#include "SVGGraphicsElement.h"

using namespace mozilla;
using namespace mozilla::gfx;

//----------------------------------------------------------------------
// Implementation

nsIFrame*
NS_NewSVGPathGeometryFrame(nsIPresShell* aPresShell,
                           nsStyleContext* aContext)
{
  return new (aPresShell) nsSVGPathGeometryFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsSVGPathGeometryFrame)

//----------------------------------------------------------------------
// nsQueryFrame methods

NS_QUERYFRAME_HEAD(nsSVGPathGeometryFrame)
  NS_QUERYFRAME_ENTRY(nsISVGChildFrame)
  NS_QUERYFRAME_ENTRY(nsSVGPathGeometryFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsSVGPathGeometryFrameBase)

//----------------------------------------------------------------------
// Display list item:

class nsDisplaySVGPathGeometry : public nsDisplayItem {
public:
  nsDisplaySVGPathGeometry(nsDisplayListBuilder* aBuilder,
                           nsSVGPathGeometryFrame* aFrame)
    : nsDisplayItem(aBuilder, aFrame)
  {
    MOZ_COUNT_CTOR(nsDisplaySVGPathGeometry);
    NS_ABORT_IF_FALSE(aFrame, "Must have a frame!");
  }
#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplaySVGPathGeometry() {
    MOZ_COUNT_DTOR(nsDisplaySVGPathGeometry);
  }
#endif
 
  NS_DISPLAY_DECL_NAME("nsDisplaySVGPathGeometry", TYPE_SVG_PATH_GEOMETRY)

  virtual void HitTest(nsDisplayListBuilder* aBuilder, const nsRect& aRect,
                       HitTestState* aState, nsTArray<nsIFrame*> *aOutFrames);
  virtual void Paint(nsDisplayListBuilder* aBuilder,
                     nsRenderingContext* aCtx);
};

void
nsDisplaySVGPathGeometry::HitTest(nsDisplayListBuilder* aBuilder, const nsRect& aRect,
                                  HitTestState* aState, nsTArray<nsIFrame*> *aOutFrames)
{
  nsSVGPathGeometryFrame *frame = static_cast<nsSVGPathGeometryFrame*>(mFrame);
  nsPoint pointRelativeToReferenceFrame = aRect.Center();
  // ToReferenceFrame() includes frame->GetPosition(), our user space position.
  nsPoint userSpacePt = pointRelativeToReferenceFrame -
                          (ToReferenceFrame() - frame->GetPosition());
  if (frame->GetFrameForPoint(userSpacePt)) {
    aOutFrames->AppendElement(frame);
  }
}

void
nsDisplaySVGPathGeometry::Paint(nsDisplayListBuilder* aBuilder,
                                nsRenderingContext* aCtx)
{
  // ToReferenceFrame includes our mRect offset, but painting takes
  // account of that too. To avoid double counting, we subtract that
  // here.
  nsPoint offset = ToReferenceFrame() - mFrame->GetPosition();

  aCtx->PushState();
  aCtx->Translate(offset);
  static_cast<nsSVGPathGeometryFrame*>(mFrame)->PaintSVG(aCtx, nullptr);
  aCtx->PopState();
}

//----------------------------------------------------------------------
// nsIFrame methods

void
nsSVGPathGeometryFrame::Init(nsIContent* aContent,
                             nsIFrame* aParent,
                             nsIFrame* aPrevInFlow)
{
  AddStateBits(aParent->GetStateBits() & NS_STATE_SVG_CLIPPATH_CHILD);
  nsSVGPathGeometryFrameBase::Init(aContent, aParent, aPrevInFlow);
}

NS_IMETHODIMP
nsSVGPathGeometryFrame::AttributeChanged(int32_t         aNameSpaceID,
                                         nsIAtom*        aAttribute,
                                         int32_t         aModType)
{
  // We don't invalidate for transform changes (the layers code does that).
  // Also note that SVGTransformableElement::GetAttributeChangeHint will
  // return nsChangeHint_UpdateOverflow for "transform" attribute changes
  // and cause DoApplyRenderingChangeToTree to make the SchedulePaint call.

  if (aNameSpaceID == kNameSpaceID_None &&
      (static_cast<nsSVGPathGeometryElement*>
                  (mContent)->AttributeDefinesGeometry(aAttribute))) {
    nsSVGEffects::InvalidateRenderingObservers(this);
    nsSVGUtils::ScheduleReflowSVG(this);
  }
  return NS_OK;
}

/* virtual */ void
nsSVGPathGeometryFrame::DidSetStyleContext(nsStyleContext* aOldStyleContext)
{
  nsSVGPathGeometryFrameBase::DidSetStyleContext(aOldStyleContext);

  if (aOldStyleContext) {
    float oldOpacity = aOldStyleContext->PeekStyleDisplay()->mOpacity;
    float newOpacity = StyleDisplay()->mOpacity;
    if (newOpacity != oldOpacity &&
        nsSVGUtils::CanOptimizeOpacity(this)) {
      // nsIFrame::BuildDisplayListForStackingContext() is not going to create an
      // nsDisplayOpacity display list item, so DLBI won't invalidate for us.
      InvalidateFrame();
    }
  }
}

nsIAtom *
nsSVGPathGeometryFrame::GetType() const
{
  return nsGkAtoms::svgPathGeometryFrame;
}

bool
nsSVGPathGeometryFrame::IsSVGTransformed(gfx::Matrix *aOwnTransform,
                                         gfx::Matrix *aFromParentTransform) const
{
  bool foundTransform = false;

  // Check if our parent has children-only transforms:
  nsIFrame *parent = GetParent();
  if (parent &&
      parent->IsFrameOfType(nsIFrame::eSVG | nsIFrame::eSVGContainer)) {
    foundTransform = static_cast<nsSVGContainerFrame*>(parent)->
                       HasChildrenOnlyTransform(aFromParentTransform);
  }

  nsSVGElement *content = static_cast<nsSVGElement*>(mContent);
  nsSVGAnimatedTransformList* transformList =
    content->GetAnimatedTransformList();
  if ((transformList && transformList->HasTransform()) ||
      content->GetAnimateMotionTransform()) {
    if (aOwnTransform) {
      *aOwnTransform = gfx::ToMatrix(content->PrependLocalTransformsTo(gfxMatrix(),
                                  nsSVGElement::eUserSpaceToParent));
    }
    foundTransform = true;
  }
  return foundTransform;
}

void
nsSVGPathGeometryFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                         const nsRect&           aDirtyRect,
                                         const nsDisplayListSet& aLists)
{
  if (!static_cast<const nsSVGElement*>(mContent)->HasValidDimensions()) {
    return;
  }
  aLists.Content()->AppendNewToTop(
    new (aBuilder) nsDisplaySVGPathGeometry(aBuilder, this));
}

//----------------------------------------------------------------------
// nsISVGChildFrame methods

NS_IMETHODIMP
nsSVGPathGeometryFrame::PaintSVG(nsRenderingContext *aContext,
                                 const nsIntRect *aDirtyRect,
                                 nsIFrame* aTransformRoot)
{
  if (!StyleVisibility()->IsVisible())
    return NS_OK;

  uint32_t paintOrder = StyleSVG()->mPaintOrder;
  if (paintOrder == NS_STYLE_PAINT_ORDER_NORMAL) {
    Render(aContext, eRenderFill | eRenderStroke, aTransformRoot);
    PaintMarkers(aContext);
  } else {
    while (paintOrder) {
      uint32_t component =
        paintOrder & ((1 << NS_STYLE_PAINT_ORDER_BITWIDTH) - 1);
      switch (component) {
        case NS_STYLE_PAINT_ORDER_FILL:
          Render(aContext, eRenderFill, aTransformRoot);
          break;
        case NS_STYLE_PAINT_ORDER_STROKE:
          Render(aContext, eRenderStroke, aTransformRoot);
          break;
        case NS_STYLE_PAINT_ORDER_MARKERS:
          PaintMarkers(aContext);
          break;
      }
      paintOrder >>= NS_STYLE_PAINT_ORDER_BITWIDTH;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP_(nsIFrame*)
nsSVGPathGeometryFrame::GetFrameForPoint(const nsPoint &aPoint)
{
  gfxMatrix canvasTM = GetCanvasTM(FOR_HIT_TESTING);
  if (canvasTM.IsSingular()) {
    return nullptr;
  }
  uint16_t fillRule, hitTestFlags;
  if (GetStateBits() & NS_STATE_SVG_CLIPPATH_CHILD) {
    hitTestFlags = SVG_HIT_TEST_FILL;
    fillRule = StyleSVG()->mClipRule;
  } else {
    hitTestFlags = GetHitTestFlags();
    // XXX once bug 614732 is fixed, aPoint won't need any conversion in order
    // to compare it with mRect.
    nsPoint point =
      nsSVGUtils::TransformOuterSVGPointToChildFrame(aPoint, canvasTM, PresContext());
    if (!hitTestFlags || ((hitTestFlags & SVG_HIT_TEST_CHECK_MRECT) &&
                          !mRect.Contains(point)))
      return nullptr;
    fillRule = StyleSVG()->mFillRule;
  }

  bool isHit = false;

  nsRefPtr<gfxContext> tmpCtx =
    new gfxContext(gfxPlatform::GetPlatform()->ScreenReferenceSurface());

  GeneratePath(tmpCtx, ToMatrix(canvasTM));
  gfxPoint userSpacePoint =
    tmpCtx->DeviceToUser(gfxPoint(PresContext()->AppUnitsToGfxUnits(aPoint.x),
                                  PresContext()->AppUnitsToGfxUnits(aPoint.y)));

  if (fillRule == NS_STYLE_FILL_RULE_EVENODD)
    tmpCtx->SetFillRule(gfxContext::FILL_RULE_EVEN_ODD);
  else
    tmpCtx->SetFillRule(gfxContext::FILL_RULE_WINDING);

  if (hitTestFlags & SVG_HIT_TEST_FILL)
    isHit = tmpCtx->PointInFill(userSpacePoint);
  if (!isHit && (hitTestFlags & SVG_HIT_TEST_STROKE)) {
    nsSVGUtils::SetupCairoStrokeGeometry(this, tmpCtx);
    // tmpCtx's matrix may have transformed by SetupCairoStrokeGeometry
    // if there is a non-scaling stroke. We need to transform userSpacePoint
    // so that everything is using the same co-ordinate system.
    userSpacePoint =
      nsSVGUtils::GetStrokeTransform(this).Invert().Transform(userSpacePoint);
    isHit = tmpCtx->PointInStroke(userSpacePoint);
  }

  if (isHit && nsSVGUtils::HitTestClip(this, aPoint))
    return this;

  return nullptr;
}

NS_IMETHODIMP_(nsRect)
nsSVGPathGeometryFrame::GetCoveredRegion()
{
  return nsSVGUtils::TransformFrameRectToOuterSVG(
           mRect, GetCanvasTM(FOR_OUTERSVG_TM), PresContext());
}

void
nsSVGPathGeometryFrame::ReflowSVG()
{
  NS_ASSERTION(nsSVGUtils::OuterSVGIsCallingReflowSVG(this),
               "This call is probably a wasteful mistake");

  NS_ABORT_IF_FALSE(!(GetStateBits() & NS_FRAME_IS_NONDISPLAY),
                    "ReflowSVG mechanism not designed for this");

  if (!nsSVGUtils::NeedsReflowSVG(this)) {
    return;
  }

  uint32_t flags = nsSVGUtils::eBBoxIncludeFill |
                   nsSVGUtils::eBBoxIncludeStroke |
                   nsSVGUtils::eBBoxIncludeMarkers;
  // Our "visual" overflow rect needs to be valid for building display lists
  // for hit testing, which means that for certain values of 'pointer-events'
  // it needs to include the geometry of the fill or stroke even when the fill/
  // stroke don't actually render (e.g. when stroke="none" or
  // stroke-opacity="0"). GetHitTestFlags() accounts for 'pointer-events'.
  uint16_t hitTestFlags = GetHitTestFlags();
  if ((hitTestFlags & SVG_HIT_TEST_FILL)) {
   flags |= nsSVGUtils::eBBoxIncludeFillGeometry;
  }
  if ((hitTestFlags & SVG_HIT_TEST_STROKE)) {
   flags |= nsSVGUtils::eBBoxIncludeStrokeGeometry;
  }
 
  // We'd like to just pass the identity matrix to GetBBoxContribution, but if
  // this frame's user space size is _very_ large/small then the extents we
  // obtain below might have overflowed or otherwise be broken. This would
  // cause us to end up with a broken mRect and visual overflow rect and break
  // painting of this frame. This is particularly noticeable if the transforms
  // between us and our nsSVGOuterSVGFrame scale this frame to a reasonable
  // size. To avoid this we sadly have to do extra work to account for the
  // transforms between us and our nsSVGOuterSVGFrame, even though the
  // overwhelming number of SVGs will never have this problem.
  // XXX Will Azure eventually save us from having to do this?
  gfxSize scaleFactors = GetCanvasTM(FOR_OUTERSVG_TM).ScaleFactors(true);
  bool applyScaling = fabs(scaleFactors.width) >= 1e-6 &&
                      fabs(scaleFactors.height) >= 1e-6;
  gfx::Matrix scaling;
  if (applyScaling) {
    scaling.Scale(scaleFactors.width, scaleFactors.height);
  }
  gfxRect extent = GetBBoxContribution(scaling, flags).ToThebesRect();
  if (applyScaling) {
    extent.Scale(1 / scaleFactors.width, 1 / scaleFactors.height);
  }
  mRect = nsLayoutUtils::RoundGfxRectToAppRect(extent,
            PresContext()->AppUnitsPerCSSPixel());

  if (mState & NS_FRAME_FIRST_REFLOW) {
    // Make sure we have our filter property (if any) before calling
    // FinishAndStoreOverflow (subsequent filter changes are handled off
    // nsChangeHint_UpdateEffects):
    nsSVGEffects::UpdateEffects(this);
  }

  nsRect overflow = nsRect(nsPoint(0,0), mRect.Size());
  nsOverflowAreas overflowAreas(overflow, overflow);
  FinishAndStoreOverflow(overflowAreas, mRect.Size());

  mState &= ~(NS_FRAME_FIRST_REFLOW | NS_FRAME_IS_DIRTY |
              NS_FRAME_HAS_DIRTY_CHILDREN);

  // Invalidate, but only if this is not our first reflow (since if it is our
  // first reflow then we haven't had our first paint yet).
  if (!(GetParent()->GetStateBits() & NS_FRAME_FIRST_REFLOW)) {
    InvalidateFrame();
  }
}

void
nsSVGPathGeometryFrame::NotifySVGChanged(uint32_t aFlags)
{
  NS_ABORT_IF_FALSE(aFlags & (TRANSFORM_CHANGED | COORD_CONTEXT_CHANGED),
                    "Invalidation logic may need adjusting");

  // Changes to our ancestors may affect how we render when we are rendered as
  // part of our ancestor (specifically, if our coordinate context changes size
  // and we have percentage lengths defining our geometry, then we need to be
  // reflowed). However, ancestor changes cannot affect how we render when we
  // are rendered as part of any rendering observers that we may have.
  // Therefore no need to notify rendering observers here.

  // Don't try to be too smart trying to avoid the ScheduleReflowSVG calls
  // for the stroke properties examined below. Checking HasStroke() is not
  // enough, since what we care about is whether we include the stroke in our
  // overflow rects or not, and we sometimes deliberately include stroke
  // when it's not visible. See the complexities of GetBBoxContribution.

  if (aFlags & COORD_CONTEXT_CHANGED) {
    // Stroke currently contributes to our mRect, which is why we have to take
    // account of stroke-width here. Note that we do not need to take account
    // of stroke-dashoffset since, although that can have a percentage value
    // that is resolved against our coordinate context, it does not affect our
    // mRect.
    if (static_cast<nsSVGPathGeometryElement*>(mContent)->GeometryDependsOnCoordCtx() ||
        StyleSVG()->mStrokeWidth.HasPercent()) {
      nsSVGUtils::ScheduleReflowSVG(this);
    }
  }

  if ((aFlags & TRANSFORM_CHANGED) &&
      StyleSVGReset()->mVectorEffect ==
        NS_STYLE_VECTOR_EFFECT_NON_SCALING_STROKE) {
    // Stroke currently contributes to our mRect, and our stroke depends on
    // the transform to our outer-<svg> if |vector-effect:non-scaling-stroke|.
    nsSVGUtils::ScheduleReflowSVG(this);
  } 
}

SVGBBox
nsSVGPathGeometryFrame::GetBBoxContribution(const Matrix &aToBBoxUserspace,
                                            uint32_t aFlags)
{
  SVGBBox bbox;

  if (aToBBoxUserspace.IsSingular()) {
    // XXX ReportToConsole
    return bbox;
  }

  nsRefPtr<gfxContext> tmpCtx =
    new gfxContext(gfxPlatform::GetPlatform()->ScreenReferenceSurface());

  GeneratePath(tmpCtx, aToBBoxUserspace);
  tmpCtx->IdentityMatrix();

  // Be careful when replacing the following logic to get the fill and stroke
  // extents independently (instead of computing the stroke extents from the
  // path extents). You may think that you can just use the stroke extents if
  // there is both a fill and a stroke. In reality it's necessary to calculate
  // both the fill and stroke extents, and take the union of the two. There are
  // two reasons for this:
  //
  // # Due to stroke dashing, in certain cases the fill extents could actually
  //   extend outside the stroke extents.
  // # If the stroke is very thin, cairo won't paint any stroke, and so the
  //   stroke bounds that it will return will be empty.

  gfxRect pathExtents = tmpCtx->GetUserPathExtent();

  // Account for fill:
  if ((aFlags & nsSVGUtils::eBBoxIncludeFillGeometry) ||
      ((aFlags & nsSVGUtils::eBBoxIncludeFill) &&
       StyleSVG()->mFill.mType != eStyleSVGPaintType_None)) {
    bbox = pathExtents;
  }

  // Account for stroke:
  if ((aFlags & nsSVGUtils::eBBoxIncludeStrokeGeometry) ||
      ((aFlags & nsSVGUtils::eBBoxIncludeStroke) &&
       nsSVGUtils::HasStroke(this))) {
    // We can't use tmpCtx->GetUserStrokeExtent() since it doesn't work for
    // device space extents. Instead we approximate the stroke extents from
    // pathExtents using PathExtentsToMaxStrokeExtents.
    if (pathExtents.Width() <= 0 && pathExtents.Height() <= 0) {
      // We have a zero length path, but it may still have non-empty stroke
      // bounds depending on the value of stroke-linecap. We need to fix up
      // pathExtents before it can be used with PathExtentsToMaxStrokeExtents
      // though, because if pathExtents is empty, its position will not have
      // been set. Happily we can use tmpCtx->GetUserStrokeExtent() to find
      // the center point of the extents even though it gets the extents wrong.
      nsSVGUtils::SetupCairoStrokeBBoxGeometry(this, tmpCtx);
      pathExtents.MoveTo(tmpCtx->GetUserStrokeExtent().Center());
      pathExtents.SizeTo(0, 0);
    }
    bbox.UnionEdges(nsSVGUtils::PathExtentsToMaxStrokeExtents(pathExtents,
                                                              this,
                                                              ThebesMatrix(aToBBoxUserspace)));
  }

  // Account for markers:
  if ((aFlags & nsSVGUtils::eBBoxIncludeMarkers) != 0 &&
      static_cast<nsSVGPathGeometryElement*>(mContent)->IsMarkable()) {

    float strokeWidth = nsSVGUtils::GetStrokeWidth(this);
    MarkerProperties properties = GetMarkerProperties(this);

    if (properties.MarkersExist()) {
      nsTArray<nsSVGMark> marks;
      static_cast<nsSVGPathGeometryElement*>(mContent)->GetMarkPoints(&marks);
      uint32_t num = marks.Length();

      // These are in the same order as the nsSVGMark::Type constants.
      nsSVGMarkerFrame* markerFrames[] = {
        properties.GetMarkerStartFrame(),
        properties.GetMarkerMidFrame(),
        properties.GetMarkerEndFrame(),
      };
      PR_STATIC_ASSERT(MOZ_ARRAY_LENGTH(markerFrames) == nsSVGMark::eTypeCount);

      for (uint32_t i = 0; i < num; i++) {
        nsSVGMark& mark = marks[i];
        nsSVGMarkerFrame* frame = markerFrames[mark.type];
        if (frame) {
          SVGBBox mbbox =
            frame->GetMarkBBoxContribution(aToBBoxUserspace, aFlags, this,
                                           &marks[i], strokeWidth);
          bbox.UnionEdges(mbbox);
        }
      }
    }
  }

  return bbox;
}

//----------------------------------------------------------------------
// nsSVGPathGeometryFrame methods:

gfxMatrix
nsSVGPathGeometryFrame::GetCanvasTM(uint32_t aFor, nsIFrame* aTransformRoot)
{
  if (!(GetStateBits() & NS_FRAME_IS_NONDISPLAY) && !aTransformRoot) {
    if ((aFor == FOR_PAINTING && NS_SVGDisplayListPaintingEnabled()) ||
        (aFor == FOR_HIT_TESTING && NS_SVGDisplayListHitTestingEnabled())) {
      return nsSVGIntegrationUtils::GetCSSPxToDevPxMatrix(this);
    }
  }

  NS_ASSERTION(mParent, "null parent");

  nsSVGContainerFrame *parent = static_cast<nsSVGContainerFrame*>(mParent);
  dom::SVGGraphicsElement *content = static_cast<dom::SVGGraphicsElement*>(mContent);

  return content->PrependLocalTransformsTo(
      this == aTransformRoot ? gfxMatrix() :
                               parent->GetCanvasTM(aFor, aTransformRoot));
}

nsSVGPathGeometryFrame::MarkerProperties
nsSVGPathGeometryFrame::GetMarkerProperties(nsSVGPathGeometryFrame *aFrame)
{
  NS_ASSERTION(!aFrame->GetPrevContinuation(), "aFrame should be first continuation");

  MarkerProperties result;
  const nsStyleSVG *style = aFrame->StyleSVG();
  result.mMarkerStart =
    nsSVGEffects::GetMarkerProperty(style->mMarkerStart, aFrame,
                                    nsSVGEffects::MarkerBeginProperty());
  result.mMarkerMid =
    nsSVGEffects::GetMarkerProperty(style->mMarkerMid, aFrame,
                                    nsSVGEffects::MarkerMiddleProperty());
  result.mMarkerEnd =
    nsSVGEffects::GetMarkerProperty(style->mMarkerEnd, aFrame,
                                    nsSVGEffects::MarkerEndProperty());
  return result;
}

nsSVGMarkerFrame *
nsSVGPathGeometryFrame::MarkerProperties::GetMarkerStartFrame()
{
  if (!mMarkerStart)
    return nullptr;
  return static_cast<nsSVGMarkerFrame *>
    (mMarkerStart->GetReferencedFrame(nsGkAtoms::svgMarkerFrame, nullptr));
}

nsSVGMarkerFrame *
nsSVGPathGeometryFrame::MarkerProperties::GetMarkerMidFrame()
{
  if (!mMarkerMid)
    return nullptr;
  return static_cast<nsSVGMarkerFrame *>
    (mMarkerMid->GetReferencedFrame(nsGkAtoms::svgMarkerFrame, nullptr));
}

nsSVGMarkerFrame *
nsSVGPathGeometryFrame::MarkerProperties::GetMarkerEndFrame()
{
  if (!mMarkerEnd)
    return nullptr;
  return static_cast<nsSVGMarkerFrame *>
    (mMarkerEnd->GetReferencedFrame(nsGkAtoms::svgMarkerFrame, nullptr));
}

void
nsSVGPathGeometryFrame::Render(nsRenderingContext *aContext,
                               uint32_t aRenderComponents,
                               nsIFrame* aTransformRoot)
{
  gfxContext *gfx = aContext->ThebesContext();

  uint16_t renderMode = SVGAutoRenderState::GetRenderMode(aContext);

  switch (StyleSVG()->mShapeRendering) {
  case NS_STYLE_SHAPE_RENDERING_OPTIMIZESPEED:
  case NS_STYLE_SHAPE_RENDERING_CRISPEDGES:
    gfx->SetAntialiasMode(gfxContext::MODE_ALIASED);
    break;
  default:
    gfx->SetAntialiasMode(gfxContext::MODE_COVERAGE);
    break;
  }

  if (renderMode != SVGAutoRenderState::NORMAL) {
    NS_ABORT_IF_FALSE(renderMode == SVGAutoRenderState::CLIP ||
                      renderMode == SVGAutoRenderState::CLIP_MASK,
                      "Unknown render mode");

    // In the case that |renderMode == SVGAutoRenderState::CLIP| then we don't
    // use the path we generate here until further up the call stack when
    // nsSVGClipPathFrame::Clip calls gfxContext::Clip. That's a problem for
    // Moz2D which emits paths in user space (unlike cairo which emits paths in
    // device space). gfxContext has hacks to deal with code changing the
    // transform then using the current path when it is backed by Moz2D, but
    // Moz2D itself does not since that would fundamentally go against its API.
    // Therefore we do not want to Save()/Restore() the gfxContext here in the
    // SVGAutoRenderState::CLIP case since that would block us from killing off
    // gfxContext and using Moz2D directly. Not bothering to Save()/Restore()
    // is actually okay, since we know that doesn't matter in the
    // SVGAutoRenderState::CLIP case (at least for the current implementation).
    gfxContextMatrixAutoSaveRestore autoSaveRestore;
    if (renderMode != SVGAutoRenderState::CLIP) {
      autoSaveRestore.SetContext(gfx);
    }

    GeneratePath(gfx, ToMatrix(GetCanvasTM(FOR_PAINTING, aTransformRoot)));

    // We used to call gfx->Restore() here, since for the
    // SVGAutoRenderState::CLIP case it is important to leave the fill rule
    // that we set below untouched so that the value is still set when return
    // to gfxContext::Clip() further up the call stack. Since we no longer
    // call gfx->Save() in the SVGAutoRenderState::CLIP case we don't need to
    // worry that autoSaveRestore will delay the Restore() call for the
    // CLIP_MASK case until we exit this function.

    gfxContext::FillRule oldFillRull = gfx->CurrentFillRule();

    if (StyleSVG()->mClipRule == NS_STYLE_FILL_RULE_EVENODD)
      gfx->SetFillRule(gfxContext::FILL_RULE_EVEN_ODD);
    else
      gfx->SetFillRule(gfxContext::FILL_RULE_WINDING);

    if (renderMode == SVGAutoRenderState::CLIP_MASK) {
      gfx->SetColor(gfxRGBA(1.0f, 1.0f, 1.0f, 1.0f));
      gfx->Fill();
      gfx->SetFillRule(oldFillRull); // restore, but only for CLIP_MASK
      gfx->NewPath();
    }

    return;
  }

  gfxContextAutoSaveRestore autoSaveRestore(gfx);

  GeneratePath(gfx, ToMatrix(GetCanvasTM(FOR_PAINTING, aTransformRoot)));

  gfxTextContextPaint *contextPaint =
    (gfxTextContextPaint*)aContext->GetUserData(&gfxTextContextPaint::sUserDataKey);

  if ((aRenderComponents & eRenderFill) &&
      nsSVGUtils::SetupCairoFillPaint(this, gfx, contextPaint)) {
    gfx->Fill();
  }

  if ((aRenderComponents & eRenderStroke) &&
       nsSVGUtils::SetupCairoStroke(this, gfx, contextPaint)) {
    gfx->Stroke();
  }

  gfx->NewPath();
}

void
nsSVGPathGeometryFrame::GeneratePath(gfxContext* aContext,
                                     const Matrix &aTransform)
{
  if (aTransform.IsSingular()) {
    aContext->IdentityMatrix();
    aContext->NewPath();
    return;
  }

  aContext->MultiplyAndNudgeToIntegers(ThebesMatrix(aTransform));

  // Hack to let SVGPathData::ConstructPath know if we have square caps:
  const nsStyleSVG* style = StyleSVG();
  if (style->mStrokeLinecap == NS_STYLE_STROKE_LINECAP_SQUARE) {
    aContext->SetLineCap(gfxContext::LINE_CAP_SQUARE);
  }

  aContext->NewPath();
  static_cast<nsSVGPathGeometryElement*>(mContent)->ConstructPath(aContext);
}

void
nsSVGPathGeometryFrame::PaintMarkers(nsRenderingContext* aContext)
{
  gfxTextContextPaint *contextPaint =
    (gfxTextContextPaint*)aContext->GetUserData(&gfxTextContextPaint::sUserDataKey);

  if (static_cast<nsSVGPathGeometryElement*>(mContent)->IsMarkable()) {
    MarkerProperties properties = GetMarkerProperties(this);

    if (properties.MarkersExist()) {
      float strokeWidth = nsSVGUtils::GetStrokeWidth(this, contextPaint);

      nsTArray<nsSVGMark> marks;
      static_cast<nsSVGPathGeometryElement*>
                 (mContent)->GetMarkPoints(&marks);

      uint32_t num = marks.Length();
      if (num) {
        // These are in the same order as the nsSVGMark::Type constants.
        nsSVGMarkerFrame* markerFrames[] = {
          properties.GetMarkerStartFrame(),
          properties.GetMarkerMidFrame(),
          properties.GetMarkerEndFrame(),
        };
        PR_STATIC_ASSERT(MOZ_ARRAY_LENGTH(markerFrames) == nsSVGMark::eTypeCount);

        for (uint32_t i = 0; i < num; i++) {
          nsSVGMark& mark = marks[i];
          nsSVGMarkerFrame* frame = markerFrames[mark.type];
          if (frame) {
            frame->PaintMark(aContext, this, &mark, strokeWidth);
          }
        }
      }
    }
  }
}

uint16_t
nsSVGPathGeometryFrame::GetHitTestFlags()
{
  return nsSVGUtils::GetGeometryHitTestFlags(this);
}
