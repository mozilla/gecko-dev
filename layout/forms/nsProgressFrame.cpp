/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsProgressFrame.h"

#include "nsIContent.h"
#include "nsPresContext.h"
#include "nsGkAtoms.h"
#include "nsNameSpaceManager.h"
#include "nsIDocument.h"
#include "nsIPresShell.h"
#include "nsNodeInfoManager.h"
#include "nsINodeInfo.h"
#include "nsContentCreatorFunctions.h"
#include "nsContentUtils.h"
#include "nsFormControlFrame.h"
#include "nsFontMetrics.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/HTMLProgressElement.h"
#include "nsContentList.h"
#include "nsStyleSet.h"
#include "nsThemeConstants.h"
#include <algorithm>

using namespace mozilla::dom;

nsIFrame*
NS_NewProgressFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsProgressFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsProgressFrame)

nsProgressFrame::nsProgressFrame(nsStyleContext* aContext)
  : nsContainerFrame(aContext)
  , mBarDiv(nullptr)
{
}

nsProgressFrame::~nsProgressFrame()
{
}

void
nsProgressFrame::DestroyFrom(nsIFrame* aDestructRoot)
{
  NS_ASSERTION(!GetPrevContinuation(),
               "nsProgressFrame should not have continuations; if it does we "
               "need to call RegUnregAccessKey only for the first.");
  nsFormControlFrame::RegUnRegAccessKey(static_cast<nsIFrame*>(this), false);
  nsContentUtils::DestroyAnonymousContent(&mBarDiv);
  nsContainerFrame::DestroyFrom(aDestructRoot);
}

nsresult
nsProgressFrame::CreateAnonymousContent(nsTArray<ContentInfo>& aElements)
{
  // Create the progress bar div.
  nsCOMPtr<nsIDocument> doc = mContent->GetDocument();
  mBarDiv = doc->CreateHTMLElement(nsGkAtoms::div);

  // Associate ::-moz-progress-bar pseudo-element to the anonymous child.
  nsCSSPseudoElements::Type pseudoType = nsCSSPseudoElements::ePseudo_mozProgressBar;
  nsRefPtr<nsStyleContext> newStyleContext = PresContext()->StyleSet()->
    ResolvePseudoElementStyle(mContent->AsElement(), pseudoType,
                              StyleContext(), mBarDiv->AsElement());

  if (!aElements.AppendElement(ContentInfo(mBarDiv, newStyleContext))) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return NS_OK;
}

void
nsProgressFrame::AppendAnonymousContentTo(nsBaseContentList& aElements,
                                          uint32_t aFilter)
{
  aElements.MaybeAppendElement(mBarDiv);
}

NS_QUERYFRAME_HEAD(nsProgressFrame)
  NS_QUERYFRAME_ENTRY(nsProgressFrame)
  NS_QUERYFRAME_ENTRY(nsIAnonymousContentCreator)
NS_QUERYFRAME_TAIL_INHERITING(nsContainerFrame)


void
nsProgressFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                  const nsRect&           aDirtyRect,
                                  const nsDisplayListSet& aLists)
{
  BuildDisplayListForInline(aBuilder, aDirtyRect, aLists);
}

nsresult nsProgressFrame::Reflow(nsPresContext*           aPresContext,
                                      nsHTMLReflowMetrics&     aDesiredSize,
                                      const nsHTMLReflowState& aReflowState,
                                      nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsProgressFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowState, aDesiredSize, aStatus);

  NS_ASSERTION(mBarDiv, "Progress bar div must exist!");
  NS_ASSERTION(!GetPrevContinuation(),
               "nsProgressFrame should not have continuations; if it does we "
               "need to call RegUnregAccessKey only for the first.");

  if (mState & NS_FRAME_FIRST_REFLOW) {
    nsFormControlFrame::RegUnRegAccessKey(this, true);
  }

  nsIFrame* barFrame = mBarDiv->GetPrimaryFrame();
  NS_ASSERTION(barFrame, "The progress frame should have a child with a frame!");

  ReflowBarFrame(barFrame, aPresContext, aReflowState, aStatus);

  aDesiredSize.Width() = aReflowState.ComputedWidth() +
                       aReflowState.ComputedPhysicalBorderPadding().LeftRight();
  aDesiredSize.Height() = aReflowState.ComputedHeight() +
                        aReflowState.ComputedPhysicalBorderPadding().TopBottom();

  aDesiredSize.SetOverflowAreasToDesiredBounds();
  ConsiderChildOverflow(aDesiredSize.mOverflowAreas, barFrame);
  FinishAndStoreOverflow(&aDesiredSize);

  aStatus = NS_FRAME_COMPLETE;

  NS_FRAME_SET_TRUNCATION(aStatus, aReflowState, aDesiredSize);

  return NS_OK;
}

void
nsProgressFrame::ReflowBarFrame(nsIFrame*                aBarFrame,
                                nsPresContext*           aPresContext,
                                const nsHTMLReflowState& aReflowState,
                                nsReflowStatus&          aStatus)
{
  bool vertical = StyleDisplay()->mOrient == NS_STYLE_ORIENT_VERTICAL;
  nsHTMLReflowState reflowState(aPresContext, aReflowState, aBarFrame,
                                nsSize(aReflowState.ComputedWidth(),
                                       NS_UNCONSTRAINEDSIZE));
  nscoord size = vertical ? aReflowState.ComputedHeight()
                          : aReflowState.ComputedWidth();
  nscoord xoffset = aReflowState.ComputedPhysicalBorderPadding().left;
  nscoord yoffset = aReflowState.ComputedPhysicalBorderPadding().top;

  double position = static_cast<HTMLProgressElement*>(mContent)->Position();

  // Force the bar's size to match the current progress.
  // When indeterminate, the progress' size will be 100%.
  if (position >= 0.0) {
    size *= position;
  }

  if (!vertical && StyleVisibility()->mDirection == NS_STYLE_DIRECTION_RTL) {
    xoffset += aReflowState.ComputedWidth() - size;
  }

  // The bar size is fixed in these cases:
  // - the progress position is determined: the bar size is fixed according
  //   to it's value.
  // - the progress position is indeterminate and the bar appearance should be
  //   shown as native: the bar size is forced to 100%.
  // Otherwise (when the progress is indeterminate and the bar appearance isn't
  // native), the bar size isn't fixed and can be set by the author.
  if (position != -1 || ShouldUseNativeStyle()) {
    if (vertical) {
      // We want the bar to begin at the bottom.
      yoffset += aReflowState.ComputedHeight() - size;

      size -= reflowState.ComputedPhysicalMargin().TopBottom() +
              reflowState.ComputedPhysicalBorderPadding().TopBottom();
      size = std::max(size, 0);
      reflowState.SetComputedHeight(size);
    } else {
      size -= reflowState.ComputedPhysicalMargin().LeftRight() +
              reflowState.ComputedPhysicalBorderPadding().LeftRight();
      size = std::max(size, 0);
      reflowState.SetComputedWidth(size);
    }
  } else if (vertical) {
    // For vertical progress bars, we need to position the bar specificly when
    // the width isn't constrained (position == -1 and !ShouldUseNativeStyle())
    // because aReflowState.ComputedHeight() - size == 0.
    yoffset += aReflowState.ComputedHeight() - reflowState.ComputedHeight();
  }

  xoffset += reflowState.ComputedPhysicalMargin().left;
  yoffset += reflowState.ComputedPhysicalMargin().top;

  nsHTMLReflowMetrics barDesiredSize(aReflowState);
  ReflowChild(aBarFrame, aPresContext, barDesiredSize, reflowState, xoffset,
              yoffset, 0, aStatus);
  FinishReflowChild(aBarFrame, aPresContext, barDesiredSize, &reflowState,
                    xoffset, yoffset, 0);
}

nsresult
nsProgressFrame::AttributeChanged(int32_t  aNameSpaceID,
                                  nsIAtom* aAttribute,
                                  int32_t  aModType)
{
  NS_ASSERTION(mBarDiv, "Progress bar div must exist!");

  if (aNameSpaceID == kNameSpaceID_None &&
      (aAttribute == nsGkAtoms::value || aAttribute == nsGkAtoms::max)) {
    nsIFrame* barFrame = mBarDiv->GetPrimaryFrame();
    NS_ASSERTION(barFrame, "The progress frame should have a child with a frame!");
    PresContext()->PresShell()->FrameNeedsReflow(barFrame, nsIPresShell::eResize,
                                                 NS_FRAME_IS_DIRTY);
    InvalidateFrame();
  }

  return nsContainerFrame::AttributeChanged(aNameSpaceID, aAttribute, aModType);
}

nsSize
nsProgressFrame::ComputeAutoSize(nsRenderingContext *aRenderingContext,
                                 nsSize aCBSize, nscoord aAvailableWidth,
                                 nsSize aMargin, nsSize aBorder,
                                 nsSize aPadding, bool aShrinkWrap)
{
  nsSize autoSize;
  autoSize.height = autoSize.width =
    NSToCoordRound(StyleFont()->mFont.size *
                   nsLayoutUtils::FontSizeInflationFor(this)); // 1em

  if (StyleDisplay()->mOrient == NS_STYLE_ORIENT_VERTICAL) {
    autoSize.height *= 10; // 10em
  } else {
    autoSize.width *= 10; // 10em
  }

  return autoSize;
}

nscoord
nsProgressFrame::GetMinWidth(nsRenderingContext *aRenderingContext)
{
  nsRefPtr<nsFontMetrics> fontMet;
  NS_ENSURE_SUCCESS(
      nsLayoutUtils::GetFontMetricsForFrame(this, getter_AddRefs(fontMet)), 0);

  nscoord minWidth = fontMet->Font().size; // 1em

  if (StyleDisplay()->mOrient == NS_STYLE_ORIENT_AUTO ||
      StyleDisplay()->mOrient == NS_STYLE_ORIENT_HORIZONTAL) {
    // The orientation is horizontal
    minWidth *= 10; // 10em
  }

  return minWidth;
}

nscoord
nsProgressFrame::GetPrefWidth(nsRenderingContext *aRenderingContext)
{
  return GetMinWidth(aRenderingContext);
}

bool
nsProgressFrame::ShouldUseNativeStyle() const
{
  // Use the native style if these conditions are satisfied:
  // - both frames use the native appearance;
  // - neither frame has author specified rules setting the border or the
  //   background.
  return StyleDisplay()->mAppearance == NS_THEME_PROGRESSBAR &&
         mBarDiv->GetPrimaryFrame()->StyleDisplay()->mAppearance == NS_THEME_PROGRESSBAR_CHUNK &&
         !PresContext()->HasAuthorSpecifiedRules(const_cast<nsProgressFrame*>(this),
                                                 NS_AUTHOR_SPECIFIED_BORDER | NS_AUTHOR_SPECIFIED_BACKGROUND) &&
         !PresContext()->HasAuthorSpecifiedRules(mBarDiv->GetPrimaryFrame(),
                                                 NS_AUTHOR_SPECIFIED_BORDER | NS_AUTHOR_SPECIFIED_BACKGROUND);
}

Element*
nsProgressFrame::GetPseudoElement(nsCSSPseudoElements::Type aType)
{
  if (aType == nsCSSPseudoElements::ePseudo_mozProgressBar) {
    return mBarDiv;
  }

  return nsContainerFrame::GetPseudoElement(aType);
}
