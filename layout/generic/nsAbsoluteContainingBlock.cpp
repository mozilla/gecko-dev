/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * code for managing absolutely positioned children of a rendering
 * object that is a containing block for them
 */

#include "nsAbsoluteContainingBlock.h"

#include "nsContainerFrame.h"
#include "nsGkAtoms.h"
#include "nsIPresShell.h"
#include "nsHTMLReflowState.h"
#include "nsPresContext.h"
#include "nsCSSFrameConstructor.h"

#ifdef DEBUG
#include "nsBlockFrame.h"

static void PrettyUC(nscoord aSize, char* aBuf)
{
  if (NS_UNCONSTRAINEDSIZE == aSize) {
    strcpy(aBuf, "UC");
  } else {
    if((int32_t)0xdeadbeef == aSize) {
      strcpy(aBuf, "deadbeef");
    } else {
      sprintf(aBuf, "%d", aSize);
    }
  }
}
#endif

void
nsAbsoluteContainingBlock::SetInitialChildList(nsIFrame*       aDelegatingFrame,
                                               ChildListID     aListID,
                                               nsFrameList&    aChildList)
{
  NS_PRECONDITION(mChildListID == aListID, "unexpected child list name");
#ifdef DEBUG
  nsFrame::VerifyDirtyBitSet(aChildList);
#endif
  mAbsoluteFrames.SetFrames(aChildList);
}

void
nsAbsoluteContainingBlock::AppendFrames(nsIFrame*      aDelegatingFrame,
                                        ChildListID    aListID,
                                        nsFrameList&   aFrameList)
{
  NS_ASSERTION(mChildListID == aListID, "unexpected child list");

  // Append the frames to our list of absolutely positioned frames
#ifdef DEBUG
  nsFrame::VerifyDirtyBitSet(aFrameList);
#endif
  mAbsoluteFrames.AppendFrames(nullptr, aFrameList);

  // no damage to intrinsic widths, since absolutely positioned frames can't
  // change them
  aDelegatingFrame->PresContext()->PresShell()->
    FrameNeedsReflow(aDelegatingFrame, nsIPresShell::eResize,
                     NS_FRAME_HAS_DIRTY_CHILDREN);
}

void
nsAbsoluteContainingBlock::InsertFrames(nsIFrame*      aDelegatingFrame,
                                        ChildListID    aListID,
                                        nsIFrame*      aPrevFrame,
                                        nsFrameList&   aFrameList)
{
  NS_ASSERTION(mChildListID == aListID, "unexpected child list");
  NS_ASSERTION(!aPrevFrame || aPrevFrame->GetParent() == aDelegatingFrame,
               "inserting after sibling frame with different parent");

#ifdef DEBUG
  nsFrame::VerifyDirtyBitSet(aFrameList);
#endif
  mAbsoluteFrames.InsertFrames(nullptr, aPrevFrame, aFrameList);

  // no damage to intrinsic widths, since absolutely positioned frames can't
  // change them
  aDelegatingFrame->PresContext()->PresShell()->
    FrameNeedsReflow(aDelegatingFrame, nsIPresShell::eResize,
                     NS_FRAME_HAS_DIRTY_CHILDREN);
}

void
nsAbsoluteContainingBlock::RemoveFrame(nsIFrame*       aDelegatingFrame,
                                       ChildListID     aListID,
                                       nsIFrame*       aOldFrame)
{
  NS_ASSERTION(mChildListID == aListID, "unexpected child list");
  nsIFrame* nif = aOldFrame->GetNextInFlow();
  if (nif) {
    nif->GetParent()->DeleteNextInFlowChild(nif, false);
  }

  mAbsoluteFrames.DestroyFrame(aOldFrame);
}

void
nsAbsoluteContainingBlock::Reflow(nsContainerFrame*        aDelegatingFrame,
                                  nsPresContext*           aPresContext,
                                  const nsHTMLReflowState& aReflowState,
                                  nsReflowStatus&          aReflowStatus,
                                  const nsRect&            aContainingBlock,
                                  bool                     aConstrainHeight,
                                  bool                     aCBWidthChanged,
                                  bool                     aCBHeightChanged,
                                  nsOverflowAreas*         aOverflowAreas)
{
  nsReflowStatus reflowStatus = NS_FRAME_COMPLETE;

  bool reflowAll = aReflowState.ShouldReflowAllKids();

  nsIFrame* kidFrame;
  nsOverflowContinuationTracker tracker(aDelegatingFrame, true);
  for (kidFrame = mAbsoluteFrames.FirstChild(); kidFrame; kidFrame = kidFrame->GetNextSibling()) {
    bool kidNeedsReflow = reflowAll || NS_SUBTREE_DIRTY(kidFrame) ||
      FrameDependsOnContainer(kidFrame, aCBWidthChanged, aCBHeightChanged);
    if (kidNeedsReflow && !aPresContext->HasPendingInterrupt()) {
      // Reflow the frame
      nsReflowStatus  kidStatus = NS_FRAME_COMPLETE;
      ReflowAbsoluteFrame(aDelegatingFrame, aPresContext, aReflowState,
                          aContainingBlock,
                          aConstrainHeight, kidFrame, kidStatus,
                          aOverflowAreas);
      nsIFrame* nextFrame = kidFrame->GetNextInFlow();
      if (!NS_FRAME_IS_FULLY_COMPLETE(kidStatus)) {
        // Need a continuation
        if (!nextFrame) {
          nextFrame =
            aPresContext->PresShell()->FrameConstructor()->
              CreateContinuingFrame(aPresContext, kidFrame, aDelegatingFrame);
        }
        // Add it as an overflow container.
        //XXXfr This is a hack to fix some of our printing dataloss.
        // See bug 154892. Not sure how to do it "right" yet; probably want
        // to keep continuations within an nsAbsoluteContainingBlock eventually.
        tracker.Insert(nextFrame, kidStatus);
        NS_MergeReflowStatusInto(&reflowStatus, kidStatus);
      }
      else {
        // Delete any continuations
        if (nextFrame) {
          nsOverflowContinuationTracker::AutoFinish fini(&tracker, kidFrame);
          nextFrame->GetParent()->DeleteNextInFlowChild(nextFrame, true);
        }
      }
    }
    else {
      tracker.Skip(kidFrame, reflowStatus);
      if (aOverflowAreas) {
        aDelegatingFrame->ConsiderChildOverflow(*aOverflowAreas, kidFrame);
      }
    }

    // Make a CheckForInterrupt call, here, not just HasPendingInterrupt.  That
    // will make sure that we end up reflowing aDelegatingFrame in cases when
    // one of our kids interrupted.  Otherwise we'd set the dirty or
    // dirty-children bit on the kid in the condition below, and then when
    // reflow completes and we go to mark dirty bits on all ancestors of that
    // kid we'll immediately bail out, because the kid already has a dirty bit.
    // In particular, we won't set any dirty bits on aDelegatingFrame, so when
    // the following reflow happens we won't reflow the kid in question.  This
    // might be slightly suboptimal in cases where |kidFrame| itself did not
    // interrupt, since we'll trigger a reflow of it too when it's not strictly
    // needed.  But the logic to not do that is enough more complicated, and
    // the case enough of an edge case, that this is probably better.
    if (kidNeedsReflow && aPresContext->CheckForInterrupt(aDelegatingFrame)) {
      if (aDelegatingFrame->GetStateBits() & NS_FRAME_IS_DIRTY) {
        kidFrame->AddStateBits(NS_FRAME_IS_DIRTY);
      } else {
        kidFrame->AddStateBits(NS_FRAME_HAS_DIRTY_CHILDREN);
      }
    }
  }

  // Abspos frames can't cause their parent to be incomplete,
  // only overflow incomplete.
  if (NS_FRAME_IS_NOT_COMPLETE(reflowStatus))
    NS_FRAME_SET_OVERFLOW_INCOMPLETE(reflowStatus);

  NS_MergeReflowStatusInto(&aReflowStatus, reflowStatus);
}

static inline bool IsFixedPaddingSize(const nsStyleCoord& aCoord)
  { return aCoord.ConvertsToLength(); }
static inline bool IsFixedMarginSize(const nsStyleCoord& aCoord)
  { return aCoord.ConvertsToLength(); }
static inline bool IsFixedOffset(const nsStyleCoord& aCoord)
  { return aCoord.ConvertsToLength(); }

bool
nsAbsoluteContainingBlock::FrameDependsOnContainer(nsIFrame* f,
                                                   bool aCBWidthChanged,
                                                   bool aCBHeightChanged)
{
  const nsStylePosition* pos = f->StylePosition();
  // See if f's position might have changed because it depends on a
  // placeholder's position
  // This can happen in the following cases:
  // 1) Vertical positioning.  "top" must be auto and "bottom" must be auto
  //    (otherwise the vertical position is completely determined by
  //    whichever of them is not auto and the height).
  // 2) Horizontal positioning.  "left" must be auto and "right" must be auto
  //    (otherwise the horizontal position is completely determined by
  //    whichever of them is not auto and the width).
  // See nsHTMLReflowState::InitAbsoluteConstraints -- these are the
  // only cases when we call CalculateHypotheticalBox().
  if ((pos->mOffset.GetTopUnit() == eStyleUnit_Auto &&
       pos->mOffset.GetBottomUnit() == eStyleUnit_Auto) ||
      (pos->mOffset.GetLeftUnit() == eStyleUnit_Auto &&
       pos->mOffset.GetRightUnit() == eStyleUnit_Auto)) {
    return true;
  }
  if (!aCBWidthChanged && !aCBHeightChanged) {
    // skip getting style data
    return false;
  }
  const nsStylePadding* padding = f->StylePadding();
  const nsStyleMargin* margin = f->StyleMargin();
  if (aCBWidthChanged) {
    // See if f's width might have changed.
    // If border-left, border-right, padding-left, padding-right,
    // width, min-width, and max-width are all lengths, 'none', or enumerated,
    // then our frame width does not depend on the parent width.
    // Note that borders never depend on the parent width
    // XXX All of the enumerated values except -moz-available are ok too.
    if (pos->WidthDependsOnContainer() ||
        pos->MinWidthDependsOnContainer() ||
        pos->MaxWidthDependsOnContainer() ||
        !IsFixedPaddingSize(padding->mPadding.GetLeft()) ||
        !IsFixedPaddingSize(padding->mPadding.GetRight())) {
      return true;
    }

    // See if f's position might have changed. If we're RTL then the
    // rules are slightly different. We'll assume percentage or auto
    // margins will always induce a dependency on the size
    if (!IsFixedMarginSize(margin->mMargin.GetLeft()) ||
        !IsFixedMarginSize(margin->mMargin.GetRight())) {
      return true;
    }
    if (f->StyleVisibility()->mDirection == NS_STYLE_DIRECTION_RTL) {
      // Note that even if 'left' is a length, our position can
      // still depend on the containing block width, because if
      // 'right' is also a length we will discard 'left' and be
      // positioned relative to the containing block right edge.
      // 'left' length and 'right' auto is the only combination
      // we can be sure of.
      if (!IsFixedOffset(pos->mOffset.GetLeft()) ||
          pos->mOffset.GetRightUnit() != eStyleUnit_Auto) {
        return true;
      }
    } else {
      if (!IsFixedOffset(pos->mOffset.GetLeft())) {
        return true;
      }
    }
  }
  if (aCBHeightChanged) {
    // See if f's height might have changed.
    // If border-top, border-bottom, padding-top, padding-bottom,
    // min-height, and max-height are all lengths or 'none',
    // and height is a length or height and bottom are auto and top is not auto,
    // then our frame height does not depend on the parent height.
    // Note that borders never depend on the parent height
    if ((pos->HeightDependsOnContainer() &&
         !(pos->mHeight.GetUnit() == eStyleUnit_Auto &&
           pos->mOffset.GetBottomUnit() == eStyleUnit_Auto &&
           pos->mOffset.GetTopUnit() != eStyleUnit_Auto)) ||
        pos->MinHeightDependsOnContainer() ||
        pos->MaxHeightDependsOnContainer() ||
        !IsFixedPaddingSize(padding->mPadding.GetTop()) ||
        !IsFixedPaddingSize(padding->mPadding.GetBottom())) { 
      return true;
    }
      
    // See if f's position might have changed.
    if (!IsFixedMarginSize(margin->mMargin.GetTop()) ||
        !IsFixedMarginSize(margin->mMargin.GetBottom())) {
      return true;
    }
    if (!IsFixedOffset(pos->mOffset.GetTop())) {
      return true;
    }
  }
  return false;
}

void
nsAbsoluteContainingBlock::DestroyFrames(nsIFrame* aDelegatingFrame,
                                         nsIFrame* aDestructRoot)
{
  mAbsoluteFrames.DestroyFramesFrom(aDestructRoot);
}

void
nsAbsoluteContainingBlock::MarkSizeDependentFramesDirty()
{
  DoMarkFramesDirty(false);
}

void
nsAbsoluteContainingBlock::MarkAllFramesDirty()
{
  DoMarkFramesDirty(true);
}

void
nsAbsoluteContainingBlock::DoMarkFramesDirty(bool aMarkAllDirty)
{
  for (nsIFrame* kidFrame = mAbsoluteFrames.FirstChild();
       kidFrame;
       kidFrame = kidFrame->GetNextSibling()) {
    if (aMarkAllDirty) {
      kidFrame->AddStateBits(NS_FRAME_IS_DIRTY);
    } else if (FrameDependsOnContainer(kidFrame, true, true)) {
      // Add the weakest flags that will make sure we reflow this frame later
      kidFrame->AddStateBits(NS_FRAME_HAS_DIRTY_CHILDREN);
    }
  }
}

// XXX Optimize the case where it's a resize reflow and the absolutely
// positioned child has the exact same size and position and skip the
// reflow...

// When bug 154892 is checked in, make sure that when 
// mChildListID == kFixedList, the height is unconstrained.
// since we don't allow replicated frames to split.

void
nsAbsoluteContainingBlock::ReflowAbsoluteFrame(nsIFrame*                aDelegatingFrame,
                                               nsPresContext*           aPresContext,
                                               const nsHTMLReflowState& aReflowState,
                                               const nsRect&            aContainingBlock,
                                               bool                     aConstrainHeight,
                                               nsIFrame*                aKidFrame,
                                               nsReflowStatus&          aStatus,
                                               nsOverflowAreas*         aOverflowAreas)
{
#ifdef DEBUG
  if (nsBlockFrame::gNoisyReflow) {
    nsFrame::IndentBy(stdout,nsBlockFrame::gNoiseIndent);
    printf("abs pos ");
    if (aKidFrame) {
      nsAutoString name;
      aKidFrame->GetFrameName(name);
      printf("%s ", NS_LossyConvertUTF16toASCII(name).get());
    }

    char width[16];
    char height[16];
    PrettyUC(aReflowState.AvailableWidth(), width);
    PrettyUC(aReflowState.AvailableHeight(), height);
    printf(" a=%s,%s ", width, height);
    PrettyUC(aReflowState.ComputedWidth(), width);
    PrettyUC(aReflowState.ComputedHeight(), height);
    printf("c=%s,%s \n", width, height);
  }
  AutoNoisyIndenter indent(nsBlockFrame::gNoisy);
#endif // DEBUG

  nscoord availWidth = aContainingBlock.width;
  if (availWidth == -1) {
    NS_ASSERTION(aReflowState.ComputedWidth() != NS_UNCONSTRAINEDSIZE,
                 "Must have a useful width _somewhere_");
    availWidth =
      aReflowState.ComputedWidth() + aReflowState.ComputedPhysicalPadding().LeftRight();
  }

  nsHTMLReflowMetrics kidDesiredSize(aReflowState);
  nsHTMLReflowState kidReflowState(aPresContext, aReflowState, aKidFrame,
                                   nsSize(availWidth, NS_UNCONSTRAINEDSIZE),
                                   aContainingBlock.width,
                                   aContainingBlock.height);

  // Send the WillReflow() notification and position the frame
  aKidFrame->WillReflow(aPresContext);

  // Get the border values
  const nsMargin& border = aReflowState.mStyleBorder->GetComputedBorder();

  bool constrainHeight = (aReflowState.AvailableHeight() != NS_UNCONSTRAINEDSIZE)
    && aConstrainHeight
       // Don't split if told not to (e.g. for fixed frames)
    && (aDelegatingFrame->GetType() != nsGkAtoms::inlineFrame)
       //XXX we don't handle splitting frames for inline absolute containing blocks yet
    && (aKidFrame->GetRect().y <= aReflowState.AvailableHeight());
       // Don't split things below the fold. (Ideally we shouldn't *have*
       // anything totally below the fold, but we can't position frames
       // across next-in-flow breaks yet.
  if (constrainHeight) {
    kidReflowState.AvailableHeight() = aReflowState.AvailableHeight() - border.top
                                     - kidReflowState.ComputedPhysicalMargin().top;
    if (NS_AUTOOFFSET != kidReflowState.ComputedPhysicalOffsets().top)
      kidReflowState.AvailableHeight() -= kidReflowState.ComputedPhysicalOffsets().top;
  }

  // Do the reflow
  aKidFrame->Reflow(aPresContext, kidDesiredSize, kidReflowState, aStatus);

  // If we're solving for 'left' or 'top', then compute it now that we know the
  // width/height
  if ((NS_AUTOOFFSET == kidReflowState.ComputedPhysicalOffsets().left) ||
      (NS_AUTOOFFSET == kidReflowState.ComputedPhysicalOffsets().top)) {
    nscoord aContainingBlockWidth = aContainingBlock.width;
    nscoord aContainingBlockHeight = aContainingBlock.height;

    if (-1 == aContainingBlockWidth) {
      // Get the containing block width/height
      kidReflowState.ComputeContainingBlockRectangle(aPresContext,
                                                     &aReflowState,
                                                     aContainingBlockWidth,
                                                     aContainingBlockHeight);
    }

    if (NS_AUTOOFFSET == kidReflowState.ComputedPhysicalOffsets().left) {
      NS_ASSERTION(NS_AUTOOFFSET != kidReflowState.ComputedPhysicalOffsets().right,
                   "Can't solve for both left and right");
      kidReflowState.ComputedPhysicalOffsets().left = aContainingBlockWidth -
                                             kidReflowState.ComputedPhysicalOffsets().right -
                                             kidReflowState.ComputedPhysicalMargin().right -
                                             kidDesiredSize.Width() -
                                             kidReflowState.ComputedPhysicalMargin().left;
    }
    if (NS_AUTOOFFSET == kidReflowState.ComputedPhysicalOffsets().top) {
      kidReflowState.ComputedPhysicalOffsets().top = aContainingBlockHeight -
                                            kidReflowState.ComputedPhysicalOffsets().bottom -
                                            kidReflowState.ComputedPhysicalMargin().bottom -
                                            kidDesiredSize.Height() -
                                            kidReflowState.ComputedPhysicalMargin().top;
    }
  }

  // Position the child relative to our padding edge
  nsRect  rect(border.left + kidReflowState.ComputedPhysicalOffsets().left + kidReflowState.ComputedPhysicalMargin().left,
               border.top + kidReflowState.ComputedPhysicalOffsets().top + kidReflowState.ComputedPhysicalMargin().top,
               kidDesiredSize.Width(), kidDesiredSize.Height());

  // Offset the frame rect by the given origin of the absolute containing block.
  // If the frame is auto-positioned on both sides of an axis, it will be
  // positioned based on its containing block and we don't need to offset.
  if (aContainingBlock.TopLeft() != nsPoint(0, 0)) {
    if (!(kidReflowState.mStylePosition->mOffset.GetLeftUnit() == eStyleUnit_Auto &&
          kidReflowState.mStylePosition->mOffset.GetRightUnit() == eStyleUnit_Auto)) {
      rect.x += aContainingBlock.x;
    }
    if (!(kidReflowState.mStylePosition->mOffset.GetTopUnit() == eStyleUnit_Auto &&
          kidReflowState.mStylePosition->mOffset.GetBottomUnit() == eStyleUnit_Auto)) {
      rect.y += aContainingBlock.y;
    }
  }

  aKidFrame->SetRect(rect);

  nsView* view = aKidFrame->GetView();
  if (view) {
    // Size and position the view and set its opacity, visibility, content
    // transparency, and clip
    nsContainerFrame::SyncFrameViewAfterReflow(aPresContext, aKidFrame, view,
                                               kidDesiredSize.VisualOverflow());
  } else {
    nsContainerFrame::PositionChildViews(aKidFrame);
  }

  aKidFrame->DidReflow(aPresContext, &kidReflowState, nsDidReflowStatus::FINISHED);

#ifdef DEBUG
  if (nsBlockFrame::gNoisyReflow) {
    nsFrame::IndentBy(stdout,nsBlockFrame::gNoiseIndent - 1);
    printf("abs pos ");
    if (aKidFrame) {
      nsAutoString name;
      aKidFrame->GetFrameName(name);
      printf("%s ", NS_LossyConvertUTF16toASCII(name).get());
    }
    printf("%p rect=%d,%d,%d,%d\n", static_cast<void*>(aKidFrame),
           rect.x, rect.y, rect.width, rect.height);
  }
#endif

  if (aOverflowAreas) {
    aOverflowAreas->UnionWith(kidDesiredSize.mOverflowAreas + rect.TopLeft());
  }
}
