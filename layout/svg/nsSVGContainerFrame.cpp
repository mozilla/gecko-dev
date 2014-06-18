/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Main header first:
#include "nsSVGContainerFrame.h"

// Keep others in (case-insensitive) order:
#include "nsCSSFrameConstructor.h"
#include "nsSVGEffects.h"
#include "nsSVGElement.h"
#include "nsSVGUtils.h"
#include "nsSVGAnimatedTransformList.h"
#include "SVGTextFrame.h"
#include "RestyleManager.h"

using namespace mozilla;

NS_QUERYFRAME_HEAD(nsSVGContainerFrame)
  NS_QUERYFRAME_ENTRY(nsSVGContainerFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsSVGContainerFrameBase)

NS_QUERYFRAME_HEAD(nsSVGDisplayContainerFrame)
  NS_QUERYFRAME_ENTRY(nsSVGDisplayContainerFrame)
  NS_QUERYFRAME_ENTRY(nsISVGChildFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsSVGContainerFrame)

nsIFrame*
NS_NewSVGContainerFrame(nsIPresShell* aPresShell,
                        nsStyleContext* aContext)
{
  nsIFrame *frame = new (aPresShell) nsSVGContainerFrame(aContext);
  // If we were called directly, then the frame is for a <defs> or
  // an unknown element type. In both cases we prevent the content
  // from displaying directly.
  frame->AddStateBits(NS_FRAME_IS_NONDISPLAY);
  return frame;
}

NS_IMPL_FRAMEARENA_HELPERS(nsSVGContainerFrame)
NS_IMPL_FRAMEARENA_HELPERS(nsSVGDisplayContainerFrame)

void
nsSVGContainerFrame::AppendFrames(ChildListID  aListID,
                                  nsFrameList& aFrameList)
{
  InsertFrames(aListID, mFrames.LastChild(), aFrameList);  
}

void
nsSVGContainerFrame::InsertFrames(ChildListID aListID,
                                  nsIFrame* aPrevFrame,
                                  nsFrameList& aFrameList)
{
  NS_ASSERTION(aListID == kPrincipalList, "unexpected child list");
  NS_ASSERTION(!aPrevFrame || aPrevFrame->GetParent() == this,
               "inserting after sibling frame with different parent");

  mFrames.InsertFrames(this, aPrevFrame, aFrameList);
}

void
nsSVGContainerFrame::RemoveFrame(ChildListID aListID,
                                 nsIFrame* aOldFrame)
{
  NS_ASSERTION(aListID == kPrincipalList, "unexpected child list");

  mFrames.DestroyFrame(aOldFrame);
}

bool
nsSVGContainerFrame::UpdateOverflow()
{
  if (mState & NS_FRAME_IS_NONDISPLAY) {
    // We don't maintain overflow rects.
    // XXX It would have be better if the restyle request hadn't even happened.
    return false;
  }
  return nsSVGContainerFrameBase::UpdateOverflow();
}

/**
 * Traverses a frame tree, marking any SVGTextFrame frames as dirty
 * and calling InvalidateRenderingObservers() on it.
 *
 * The reason that this helper exists is because SVGTextFrame is special.
 * None of the other SVG frames ever need to be reflowed when they have the
 * NS_FRAME_IS_NONDISPLAY bit set on them because their PaintSVG methods
 * (and those of any containers that they can validly be contained within) do
 * not make use of mRect or overflow rects. "em" lengths, etc., are resolved
 * as those elements are painted.
 *
 * SVGTextFrame is different because its anonymous block and inline frames
 * need to be reflowed in order to get the correct metrics when things like
 * inherited font-size of an ancestor changes, or a delayed webfont loads and
 * applies.
 *
 * We assume that any change that requires the anonymous kid of an
 * SVGTextFrame to reflow will result in an NS_FRAME_IS_DIRTY reflow. When
 * that reflow reaches an NS_FRAME_IS_NONDISPLAY frame it would normally
 * stop, but this helper looks for any SVGTextFrame descendants of such
 * frames and marks them NS_FRAME_IS_DIRTY so that the next time that they are
 * painted their anonymous kid will first get the necessary reflow.
 */
/* static */ void
nsSVGContainerFrame::ReflowSVGNonDisplayText(nsIFrame* aContainer)
{
  NS_ASSERTION(aContainer->GetStateBits() & NS_FRAME_IS_DIRTY,
               "expected aContainer to be NS_FRAME_IS_DIRTY");
  NS_ASSERTION((aContainer->GetStateBits() & NS_FRAME_IS_NONDISPLAY) ||
               !aContainer->IsFrameOfType(nsIFrame::eSVG),
               "it is wasteful to call ReflowSVGNonDisplayText on a container "
               "frame that is not NS_FRAME_IS_NONDISPLAY");
  for (nsIFrame* kid = aContainer->GetFirstPrincipalChild(); kid;
       kid = kid->GetNextSibling()) {
    nsIAtom* type = kid->GetType();
    if (type == nsGkAtoms::svgTextFrame) {
      static_cast<SVGTextFrame*>(kid)->ReflowSVGNonDisplayText();
    } else {
      if (kid->IsFrameOfType(nsIFrame::eSVG | nsIFrame::eSVGContainer) ||
          type == nsGkAtoms::svgForeignObjectFrame ||
          !kid->IsFrameOfType(nsIFrame::eSVG)) {
        ReflowSVGNonDisplayText(kid);
      }
    }
  }
}

void
nsSVGDisplayContainerFrame::Init(nsIContent*       aContent,
                                 nsContainerFrame* aParent,
                                 nsIFrame*         aPrevInFlow)
{
  if (!(GetStateBits() & NS_STATE_IS_OUTER_SVG)) {
    AddStateBits(aParent->GetStateBits() & NS_STATE_SVG_CLIPPATH_CHILD);
  }
  nsSVGContainerFrame::Init(aContent, aParent, aPrevInFlow);
}

void
nsSVGDisplayContainerFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                             const nsRect&           aDirtyRect,
                                             const nsDisplayListSet& aLists)
{
  // mContent could be a XUL element so check for an SVG element before casting
  if (mContent->IsSVG() &&
      !static_cast<const nsSVGElement*>(mContent)->HasValidDimensions()) {
    return;
  }
  return BuildDisplayListForNonBlockChildren(aBuilder, aDirtyRect, aLists);
}

void
nsSVGDisplayContainerFrame::InsertFrames(ChildListID aListID,
                                         nsIFrame* aPrevFrame,
                                         nsFrameList& aFrameList)
{
  // memorize first old frame after insertion point
  // XXXbz once again, this would work a lot better if the nsIFrame
  // methods returned framelist iterators....
  nsIFrame* nextFrame = aPrevFrame ?
    aPrevFrame->GetNextSibling() : GetChildList(aListID).FirstChild();
  nsIFrame* firstNewFrame = aFrameList.FirstChild();
  
  // Insert the new frames
  nsSVGContainerFrame::InsertFrames(aListID, aPrevFrame, aFrameList);

  // If we are not a non-display SVG frame and we do not have a bounds update
  // pending, then we need to schedule one for our new children:
  if (!(GetStateBits() &
        (NS_FRAME_IS_DIRTY | NS_FRAME_HAS_DIRTY_CHILDREN |
         NS_FRAME_IS_NONDISPLAY))) {
    for (nsIFrame* kid = firstNewFrame; kid != nextFrame;
         kid = kid->GetNextSibling()) {
      nsISVGChildFrame* SVGFrame = do_QueryFrame(kid);
      if (SVGFrame) {
        NS_ABORT_IF_FALSE(!(kid->GetStateBits() & NS_FRAME_IS_NONDISPLAY),
                          "Check for this explicitly in the |if|, then");
        bool isFirstReflow = (kid->GetStateBits() & NS_FRAME_FIRST_REFLOW);
        // Remove bits so that ScheduleBoundsUpdate will work:
        kid->RemoveStateBits(NS_FRAME_FIRST_REFLOW | NS_FRAME_IS_DIRTY |
                             NS_FRAME_HAS_DIRTY_CHILDREN);
        // No need to invalidate the new kid's old bounds, so we just use
        // nsSVGUtils::ScheduleBoundsUpdate.
        nsSVGUtils::ScheduleReflowSVG(kid);
        if (isFirstReflow) {
          // Add back the NS_FRAME_FIRST_REFLOW bit:
          kid->AddStateBits(NS_FRAME_FIRST_REFLOW);
        }
      }
    }
  }
}

void
nsSVGDisplayContainerFrame::RemoveFrame(ChildListID aListID,
                                        nsIFrame* aOldFrame)
{
  nsSVGEffects::InvalidateRenderingObservers(aOldFrame);

  // nsSVGContainerFrame::RemoveFrame doesn't call down into
  // nsContainerFrame::RemoveFrame, so it doesn't call FrameNeedsReflow. We
  // need to schedule a repaint and schedule an update to our overflow rects.
  SchedulePaint();
  PresContext()->RestyleManager()->PostRestyleEvent(
    mContent->AsElement(), nsRestyleHint(0), nsChangeHint_UpdateOverflow);

  nsSVGContainerFrame::RemoveFrame(aListID, aOldFrame);

  if (!(GetStateBits() & (NS_FRAME_IS_NONDISPLAY | NS_STATE_IS_OUTER_SVG))) {
    nsSVGUtils::NotifyAncestorsOfFilterRegionChange(this);
  }
}

bool
nsSVGDisplayContainerFrame::IsSVGTransformed(gfx::Matrix *aOwnTransform,
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

  // mContent could be a XUL element so check for an SVG element before casting
  if (mContent->IsSVG()) {
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
  }
  return foundTransform;
}

//----------------------------------------------------------------------
// nsISVGChildFrame methods

nsresult
nsSVGDisplayContainerFrame::PaintSVG(nsRenderingContext* aContext,
                                     const nsIntRect *aDirtyRect,
                                     nsIFrame* aTransformRoot)
{
  NS_ASSERTION(!NS_SVGDisplayListPaintingEnabled() ||
               (mState & NS_FRAME_IS_NONDISPLAY) ||
               PresContext()->IsGlyph(),
               "If display lists are enabled, only painting of non-display "
               "SVG should take this code path");

  const nsStyleDisplay *display = StyleDisplay();
  if (display->mOpacity == 0.0)
    return NS_OK;

  for (nsIFrame* kid = mFrames.FirstChild(); kid;
       kid = kid->GetNextSibling()) {
    nsSVGUtils::PaintFrameWithEffects(aContext, aDirtyRect, kid, aTransformRoot);
  }

  return NS_OK;
}

nsIFrame*
nsSVGDisplayContainerFrame::GetFrameForPoint(const nsPoint &aPoint)
{
  NS_ASSERTION(!NS_SVGDisplayListHitTestingEnabled() ||
               (mState & NS_FRAME_IS_NONDISPLAY),
               "If display lists are enabled, only hit-testing of a "
               "clipPath's contents should take this code path");
  return nsSVGUtils::HitTestChildren(this, aPoint);
}

nsRect
nsSVGDisplayContainerFrame::GetCoveredRegion()
{
  return nsSVGUtils::GetCoveredRegion(mFrames);
}

void
nsSVGDisplayContainerFrame::ReflowSVG()
{
  NS_ASSERTION(nsSVGUtils::OuterSVGIsCallingReflowSVG(this),
               "This call is probably a wasteful mistake");

  NS_ABORT_IF_FALSE(!(GetStateBits() & NS_FRAME_IS_NONDISPLAY),
                    "ReflowSVG mechanism not designed for this");

  NS_ABORT_IF_FALSE(GetType() != nsGkAtoms::svgOuterSVGFrame,
                    "Do not call on outer-<svg>");

  if (!nsSVGUtils::NeedsReflowSVG(this)) {
    return;
  }

  // If the NS_FRAME_FIRST_REFLOW bit has been removed from our parent frame,
  // then our outer-<svg> has previously had its initial reflow. In that case
  // we need to make sure that that bit has been removed from ourself _before_
  // recursing over our children to ensure that they know too. Otherwise, we
  // need to remove it _after_ recursing over our children so that they know
  // the initial reflow is currently underway.

  bool isFirstReflow = (mState & NS_FRAME_FIRST_REFLOW);

  bool outerSVGHasHadFirstReflow =
    (GetParent()->GetStateBits() & NS_FRAME_FIRST_REFLOW) == 0;

  if (outerSVGHasHadFirstReflow) {
    mState &= ~NS_FRAME_FIRST_REFLOW; // tell our children
  }

  nsOverflowAreas overflowRects;

  for (nsIFrame* kid = mFrames.FirstChild(); kid;
       kid = kid->GetNextSibling()) {
    nsISVGChildFrame* SVGFrame = do_QueryFrame(kid);
    if (SVGFrame) {
      NS_ABORT_IF_FALSE(!(kid->GetStateBits() & NS_FRAME_IS_NONDISPLAY),
                        "Check for this explicitly in the |if|, then");
      kid->AddStateBits(mState & NS_FRAME_IS_DIRTY);
      SVGFrame->ReflowSVG();

      // We build up our child frame overflows here instead of using
      // nsLayoutUtils::UnionChildOverflow since SVG frame's all use the same
      // frame list, and we're iterating over that list now anyway.
      ConsiderChildOverflow(overflowRects, kid);
    } else {
      // Inside a non-display container frame, we might have some
      // SVGTextFrames.  We need to cause those to get reflowed in
      // case they are the target of a rendering observer.
      NS_ASSERTION(kid->GetStateBits() & NS_FRAME_IS_NONDISPLAY,
                   "expected kid to be a NS_FRAME_IS_NONDISPLAY frame");
      if (kid->GetStateBits() & NS_FRAME_IS_DIRTY) {
        nsSVGContainerFrame* container = do_QueryFrame(kid);
        if (container && container->GetContent()->IsSVG()) {
          ReflowSVGNonDisplayText(container);
        }
      }
    }
  }

  // <svg> can create an SVG viewport with an offset due to its
  // x/y/width/height attributes, and <use> can introduce an offset with an
  // empty mRect (any width/height is copied to an anonymous <svg> child).
  // Other than that containers should not set mRect since all other offsets
  // come from transforms, which are accounted for by nsDisplayTransform.
  // Note that we rely on |overflow:visible| to allow display list items to be
  // created for our children.
  NS_ABORT_IF_FALSE(mContent->Tag() == nsGkAtoms::svg ||
                    (mContent->Tag() == nsGkAtoms::use &&
                     mRect.Size() == nsSize(0,0)) ||
                    mRect.IsEqualEdges(nsRect()),
                    "Only inner-<svg>/<use> is expected to have mRect set");

  if (isFirstReflow) {
    // Make sure we have our filter property (if any) before calling
    // FinishAndStoreOverflow (subsequent filter changes are handled off
    // nsChangeHint_UpdateEffects):
    nsSVGEffects::UpdateEffects(this);
  }

  FinishAndStoreOverflow(overflowRects, mRect.Size());

  // Remove state bits after FinishAndStoreOverflow so that it doesn't
  // invalidate on first reflow:
  mState &= ~(NS_FRAME_FIRST_REFLOW | NS_FRAME_IS_DIRTY |
              NS_FRAME_HAS_DIRTY_CHILDREN);
}  

void
nsSVGDisplayContainerFrame::NotifySVGChanged(uint32_t aFlags)
{
  NS_ABORT_IF_FALSE(aFlags & (TRANSFORM_CHANGED | COORD_CONTEXT_CHANGED),
                    "Invalidation logic may need adjusting");

  nsSVGUtils::NotifyChildrenOfSVGChange(this, aFlags);
}

SVGBBox
nsSVGDisplayContainerFrame::GetBBoxContribution(
  const Matrix &aToBBoxUserspace,
  uint32_t aFlags)
{
  SVGBBox bboxUnion;

  nsIFrame* kid = mFrames.FirstChild();
  while (kid) {
    nsIContent *content = kid->GetContent();
    nsISVGChildFrame* svgKid = do_QueryFrame(kid);
    // content could be a XUL element so check for an SVG element before casting
    if (svgKid && (!content->IsSVG() ||
                   static_cast<const nsSVGElement*>(content)->HasValidDimensions())) {

      gfxMatrix transform = gfx::ThebesMatrix(aToBBoxUserspace);
      if (content->IsSVG()) {
        transform = static_cast<nsSVGElement*>(content)->
                      PrependLocalTransformsTo(transform);
      }
      // We need to include zero width/height vertical/horizontal lines, so we have
      // to use UnionEdges.
      bboxUnion.UnionEdges(svgKid->GetBBoxContribution(gfx::ToMatrix(transform), aFlags));
    }
    kid = kid->GetNextSibling();
  }

  return bboxUnion;
}
