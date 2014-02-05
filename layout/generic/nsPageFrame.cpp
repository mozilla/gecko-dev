/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPageFrame.h"
#include "nsPresContext.h"
#include "nsRenderingContext.h"
#include "nsGkAtoms.h"
#include "nsIPresShell.h"
#include "nsPageContentFrame.h"
#include "nsDisplayList.h"
#include "nsLayoutUtils.h" // for function BinarySearchForPosition
#include "nsSimplePageSequence.h" // for nsSharedPageData
#include "nsTextFormatter.h" // for page number localization formatting
#ifdef IBMBIDI
#include "nsBidiUtils.h"
#endif
#include "nsIPrintSettings.h"

#include "prlog.h"
#ifdef PR_LOGGING 
extern PRLogModuleInfo *GetLayoutPrintingLog();
#define PR_PL(_p1)  PR_LOG(GetLayoutPrintingLog(), PR_LOG_DEBUG, _p1)
#else
#define PR_PL(_p1)
#endif

using namespace mozilla;

nsIFrame*
NS_NewPageFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsPageFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsPageFrame)

nsPageFrame::nsPageFrame(nsStyleContext* aContext)
: nsContainerFrame(aContext)
{
}

nsPageFrame::~nsPageFrame()
{
}

NS_IMETHODIMP nsPageFrame::Reflow(nsPresContext*           aPresContext,
                                  nsHTMLReflowMetrics&     aDesiredSize,
                                  const nsHTMLReflowState& aReflowState,
                                  nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsPageFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowState, aDesiredSize, aStatus);
  aStatus = NS_FRAME_COMPLETE;  // initialize out parameter

  NS_ASSERTION(mFrames.FirstChild() &&
               nsGkAtoms::pageContentFrame == mFrames.FirstChild()->GetType(),
               "pageFrame must have a pageContentFrame child");

  // Resize our frame allowing it only to be as big as we are
  // XXX Pay attention to the page's border and padding...
  if (mFrames.NotEmpty()) {
    nsIFrame* frame = mFrames.FirstChild();
    // When the reflow size is NS_UNCONSTRAINEDSIZE it means we are reflowing
    // a single page to print selection. So this means we want to use
    // NS_UNCONSTRAINEDSIZE without altering it
    nscoord avHeight;
    if (mPD->mReflowSize.height == NS_UNCONSTRAINEDSIZE) {
      avHeight = NS_UNCONSTRAINEDSIZE;
    } else {
      avHeight = mPD->mReflowSize.height;
    }
    nsSize  maxSize(mPD->mReflowSize.width, avHeight);
    float scale = aPresContext->GetPageScale();
    maxSize.width = NSToCoordCeil(maxSize.width / scale);
    if (maxSize.height != NS_UNCONSTRAINEDSIZE) {
      maxSize.height = NSToCoordCeil(maxSize.height / scale);
    }
    // Get the number of Twips per pixel from the PresContext
    nscoord onePixelInTwips = nsPresContext::CSSPixelsToAppUnits(1);
    // insurance against infinite reflow, when reflowing less than a pixel
    // XXX Shouldn't we do something more friendly when invalid margins
    //     are set?
    if (maxSize.width < onePixelInTwips || maxSize.height < onePixelInTwips) {
      aDesiredSize.Width() = 0;
      aDesiredSize.Height() = 0;
      NS_WARNING("Reflow aborted; no space for content");
      return NS_OK;
    }

    nsHTMLReflowState kidReflowState(aPresContext, aReflowState, frame, maxSize);
    kidReflowState.mFlags.mIsTopOfPage = true;
    kidReflowState.mFlags.mTableIsSplittable = true;

    // Use the margins given in the @page rule.
    // If a margin is 'auto', use the margin from the print settings for that side.
    nsMargin pageContentMargin;
    const nsStyleSides& marginStyle = kidReflowState.mStyleMargin->mMargin;
    NS_FOR_CSS_SIDES(side) {
      if (marginStyle.GetUnit(side) == eStyleUnit_Auto) {
        pageContentMargin.Side(side) = mPD->mReflowMargin.Side(side);
      } else {
        pageContentMargin.Side(side) = kidReflowState.ComputedPhysicalMargin().Side(side);
      }
    }


    nscoord maxWidth = maxSize.width - pageContentMargin.LeftRight() / scale;
    nscoord maxHeight;
    if (maxSize.height == NS_UNCONSTRAINEDSIZE) {
      maxHeight = NS_UNCONSTRAINEDSIZE;
    } else {
      maxHeight = maxSize.height - pageContentMargin.TopBottom() / scale;
    }

    // Check the width and height, if they're too small we reset the margins
    // back to the default.
    if (maxWidth < onePixelInTwips ||
       (maxHeight != NS_UNCONSTRAINEDSIZE && maxHeight < onePixelInTwips)) {
      NS_FOR_CSS_SIDES(side) {
        pageContentMargin.Side(side) = mPD->mReflowMargin.Side(side);
      }
      maxWidth = maxSize.width - pageContentMargin.LeftRight() / scale;
      if (maxHeight != NS_UNCONSTRAINEDSIZE) {
        maxHeight = maxSize.height - pageContentMargin.TopBottom() / scale;
      }
    }

    kidReflowState.SetComputedWidth(maxWidth);
    kidReflowState.SetComputedHeight(maxHeight);

    // calc location of frame
    nscoord xc = pageContentMargin.left;
    nscoord yc = pageContentMargin.top;

    // Get the child's desired size
    ReflowChild(frame, aPresContext, aDesiredSize, kidReflowState, xc, yc, 0, aStatus);

    // Place and size the child
    FinishReflowChild(frame, aPresContext, aDesiredSize, &kidReflowState, xc, yc, 0);

    NS_ASSERTION(!NS_FRAME_IS_FULLY_COMPLETE(aStatus) ||
                 !frame->GetNextInFlow(), "bad child flow list");
  }
  PR_PL(("PageFrame::Reflow %p ", this));
  PR_PL(("[%d,%d][%d,%d]\n", aDesiredSize.Width(), aDesiredSize.Height(), aReflowState.AvailableWidth(), aReflowState.AvailableHeight()));

  // Return our desired size
  aDesiredSize.Width() = aReflowState.AvailableWidth();
  if (aReflowState.AvailableHeight() != NS_UNCONSTRAINEDSIZE) {
    aDesiredSize.Height() = aReflowState.AvailableHeight();
  }

  aDesiredSize.SetOverflowAreasToDesiredBounds();
  FinishAndStoreOverflow(&aDesiredSize);

  PR_PL(("PageFrame::Reflow %p ", this));
  PR_PL(("[%d,%d]\n", aReflowState.AvailableWidth(), aReflowState.AvailableHeight()));

  NS_FRAME_SET_TRUNCATION(aStatus, aReflowState, aDesiredSize);
  return NS_OK;
}

nsIAtom*
nsPageFrame::GetType() const
{
  return nsGkAtoms::pageFrame; 
}

#ifdef DEBUG_FRAME_DUMP
NS_IMETHODIMP
nsPageFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("Page"), aResult);
}
#endif

void 
nsPageFrame::ProcessSpecialCodes(const nsString& aStr, nsString& aNewStr)
{

  aNewStr = aStr;

  // Search to see if the &D code is in the string 
  // then subst in the current date/time
  NS_NAMED_LITERAL_STRING(kDate, "&D");
  if (aStr.Find(kDate) != kNotFound) {
    aNewStr.ReplaceSubstring(kDate.get(), mPD->mDateTimeStr.get());
  }

  // NOTE: Must search for &PT before searching for &P
  //
  // Search to see if the "page number and page" total code are in the string
  // and replace the page number and page total code with the actual
  // values
  NS_NAMED_LITERAL_STRING(kPageAndTotal, "&PT");
  if (aStr.Find(kPageAndTotal) != kNotFound) {
    char16_t * uStr = nsTextFormatter::smprintf(mPD->mPageNumAndTotalsFormat.get(), mPageNum, mTotNumPages);
    aNewStr.ReplaceSubstring(kPageAndTotal.get(), uStr);
    nsMemory::Free(uStr);
  }

  // Search to see if the page number code is in the string
  // and replace the page number code with the actual value
  NS_NAMED_LITERAL_STRING(kPage, "&P");
  if (aStr.Find(kPage) != kNotFound) {
    char16_t * uStr = nsTextFormatter::smprintf(mPD->mPageNumFormat.get(), mPageNum);
    aNewStr.ReplaceSubstring(kPage.get(), uStr);
    nsMemory::Free(uStr);
  }

  NS_NAMED_LITERAL_STRING(kTitle, "&T");
  if (aStr.Find(kTitle) != kNotFound) {
    aNewStr.ReplaceSubstring(kTitle.get(), mPD->mDocTitle.get());
  }

  NS_NAMED_LITERAL_STRING(kDocURL, "&U");
  if (aStr.Find(kDocURL) != kNotFound) {
    aNewStr.ReplaceSubstring(kDocURL.get(), mPD->mDocURL.get());
  }

  NS_NAMED_LITERAL_STRING(kPageTotal, "&L");
  if (aStr.Find(kPageTotal) != kNotFound) {
    char16_t * uStr = nsTextFormatter::smprintf(mPD->mPageNumFormat.get(), mTotNumPages);
    aNewStr.ReplaceSubstring(kPageTotal.get(), uStr);
    nsMemory::Free(uStr);
  }
}


//------------------------------------------------------------------------------
nscoord nsPageFrame::GetXPosition(nsRenderingContext& aRenderingContext, 
                                  const nsRect&        aRect, 
                                  int32_t              aJust,
                                  const nsString&      aStr)
{
  nscoord width = nsLayoutUtils::GetStringWidth(this, &aRenderingContext,
                                                aStr.get(), aStr.Length());

  nscoord x = aRect.x;
  switch (aJust) {
    case nsIPrintSettings::kJustLeft:
      x += mPD->mEdgePaperMargin.left;
      break;

    case nsIPrintSettings::kJustCenter:
      x += (aRect.width - width) / 2;
      break;

    case nsIPrintSettings::kJustRight:
      x += aRect.width - width - mPD->mEdgePaperMargin.right;
      break;
  } // switch

  return x;
}

// Draw a header or footer
// @param aRenderingContext - rendering content ot draw into
// @param aHeaderFooter - indicates whether it is a header or footer
// @param aStrLeft - string for the left header or footer; can be empty
// @param aStrCenter - string for the center header or footer; can be empty
// @param aStrRight - string for the right header or footer; can be empty
// @param aRect - the rect of the page
// @param aAscent - the ascent of the font
// @param aHeight - the height of the font
void
nsPageFrame::DrawHeaderFooter(nsRenderingContext& aRenderingContext,
                              nsHeaderFooterEnum   aHeaderFooter,
                              const nsString&      aStrLeft,
                              const nsString&      aStrCenter,
                              const nsString&      aStrRight,
                              const nsRect&        aRect,
                              nscoord              aAscent,
                              nscoord              aHeight)
{
  int32_t numStrs = 0;
  if (!aStrLeft.IsEmpty()) numStrs++;
  if (!aStrCenter.IsEmpty()) numStrs++;
  if (!aStrRight.IsEmpty()) numStrs++;

  if (numStrs == 0) return;
  nscoord strSpace = aRect.width / numStrs;

  if (!aStrLeft.IsEmpty()) {
    DrawHeaderFooter(aRenderingContext, aHeaderFooter,
                     nsIPrintSettings::kJustLeft, aStrLeft, aRect, aAscent,
                     aHeight, strSpace);
  }
  if (!aStrCenter.IsEmpty()) {
    DrawHeaderFooter(aRenderingContext, aHeaderFooter,
                     nsIPrintSettings::kJustCenter, aStrCenter, aRect, aAscent,
                     aHeight, strSpace);
  }
  if (!aStrRight.IsEmpty()) {
    DrawHeaderFooter(aRenderingContext, aHeaderFooter,
                     nsIPrintSettings::kJustRight, aStrRight, aRect, aAscent,
                     aHeight, strSpace);
  }
}

// Draw a header or footer string
// @param aRenderingContext - rendering context to draw into
// @param aHeaderFooter - indicates whether it is a header or footer
// @param aJust - indicates where the string is located within the header/footer
// @param aStr - the string to be drawn
// @param aRect - the rect of the page
// @param aHeight - the height of the font
// @param aAscent - the ascent of the font
// @param aWidth - available width for the string
void
nsPageFrame::DrawHeaderFooter(nsRenderingContext& aRenderingContext,
                              nsHeaderFooterEnum   aHeaderFooter,
                              int32_t              aJust,
                              const nsString&      aStr,
                              const nsRect&        aRect,
                              nscoord              aAscent,
                              nscoord              aHeight,
                              nscoord              aWidth)
{

  nscoord contentWidth = aWidth - (mPD->mEdgePaperMargin.left + mPD->mEdgePaperMargin.right);

  if ((aHeaderFooter == eHeader && aHeight < mPD->mReflowMargin.top) ||
      (aHeaderFooter == eFooter && aHeight < mPD->mReflowMargin.bottom)) {
    nsAutoString str;
    ProcessSpecialCodes(aStr, str);

    int32_t indx;
    int32_t textWidth = 0;
    const char16_t* text = str.get();

    int32_t len = (int32_t)str.Length();
    if (len == 0) {
      return; // bail is empty string
    }
    // find how much text fits, the "position" is the size of the available area
    if (nsLayoutUtils::BinarySearchForPosition(&aRenderingContext, text, 0, 0, 0, len,
                                int32_t(contentWidth), indx, textWidth)) {
      if (indx < len-1 ) {
        // we can't fit in all the text
        if (indx > 3) {
          // But we can fit in at least 4 chars.  Show all but 3 of them, then
          // an ellipsis.
          // XXXbz for non-plane0 text, this may be cutting things in the
          // middle of a codepoint!  Also, we have no guarantees that the three
          // dots will fit in the space the three chars we removed took up with
          // these font metrics!
          str.Truncate(indx-3);
          str.AppendLiteral("...");
        } else {
          // We can only fit 3 or fewer chars.  Just show nothing
          str.Truncate();
        }
      }
    } else { 
      return; // bail if couldn't find the correct length
    }
    
    if (HasRTLChars(str)) {
      PresContext()->SetBidiEnabled();
    }

    // cacl the x and y positions of the text
    nscoord x = GetXPosition(aRenderingContext, aRect, aJust, str);
    nscoord y;
    if (aHeaderFooter == eHeader) {
      y = aRect.y + mPD->mEdgePaperMargin.top;
    } else {
      y = aRect.YMost() - aHeight - mPD->mEdgePaperMargin.bottom;
    }

    // set up new clip and draw the text
    aRenderingContext.PushState();
    aRenderingContext.SetColor(NS_RGB(0,0,0));
    aRenderingContext.IntersectClip(aRect);
    nsLayoutUtils::DrawString(this, &aRenderingContext, str.get(), str.Length(), nsPoint(x, y + aAscent));
    aRenderingContext.PopState();
  }
}

/**
 * Remove all leaf display items that are not for descendants of
 * aBuilder->GetReferenceFrame() from aList.
 * @param aPage the page we're constructing the display list for
 * @param aExtraPage the page we constructed aList for
 * @param aList the list that is modified in-place
 */
static void
PruneDisplayListForExtraPage(nsDisplayListBuilder* aBuilder,
                             nsPageFrame* aPage, nsIFrame* aExtraPage,
                             nsDisplayList* aList)
{
  nsDisplayList newList;

  while (true) {
    nsDisplayItem* i = aList->RemoveBottom();
    if (!i)
      break;
    nsDisplayList* subList = i->GetSameCoordinateSystemChildren();
    if (subList) {
      PruneDisplayListForExtraPage(aBuilder, aPage, aExtraPage, subList);
      i->UpdateBounds(aBuilder);
    } else {
      nsIFrame* f = i->Frame();
      if (!nsLayoutUtils::IsProperAncestorFrameCrossDoc(aPage, f)) {
        // We're throwing this away so call its destructor now. The memory
        // is owned by aBuilder which destroys all items at once.
        i->~nsDisplayItem();
        continue;
      }
    }
    newList.AppendToTop(i);
  }
  aList->AppendToTop(&newList);
}

static nsresult
BuildDisplayListForExtraPage(nsDisplayListBuilder* aBuilder,
                             nsPageFrame* aPage, nsIFrame* aExtraPage,
                             nsDisplayList* aList)
{
  nsDisplayList list;
  // Pass an empty dirty rect since we're only interested in finding
  // placeholders whose out-of-flows are in the page
  // aBuilder->GetReferenceFrame(), and the paths to those placeholders
  // have already been marked as NS_FRAME_FORCE_DISPLAY_LIST_DESCEND_INTO.
  // Note that we should still do a prune step since we don't want to
  // rely on dirty-rect checking for correctness.
  aExtraPage->BuildDisplayListForStackingContext(aBuilder, nsRect(), &list);
  PruneDisplayListForExtraPage(aBuilder, aPage, aExtraPage, &list);
  aList->AppendToTop(&list);
  return NS_OK;
}

static nsIFrame*
GetNextPage(nsIFrame* aPageContentFrame)
{
  // XXX ugh
  nsIFrame* pageFrame = aPageContentFrame->GetParent();
  NS_ASSERTION(pageFrame->GetType() == nsGkAtoms::pageFrame,
               "pageContentFrame has unexpected parent");
  nsIFrame* nextPageFrame = pageFrame->GetNextSibling();
  if (!nextPageFrame)
    return nullptr;
  NS_ASSERTION(nextPageFrame->GetType() == nsGkAtoms::pageFrame,
               "pageFrame's sibling is not a page frame...");
  nsIFrame* f = nextPageFrame->GetFirstPrincipalChild();
  NS_ASSERTION(f, "pageFrame has no page content frame!");
  NS_ASSERTION(f->GetType() == nsGkAtoms::pageContentFrame,
               "pageFrame's child is not page content!");
  return f;
}

static void PaintHeaderFooter(nsIFrame* aFrame, nsRenderingContext* aCtx,
                              const nsRect& aDirtyRect, nsPoint aPt)
{
  static_cast<nsPageFrame*>(aFrame)->PaintHeaderFooter(*aCtx, aPt);
}

static gfx3DMatrix ComputePageTransform(nsIFrame* aFrame, float aAppUnitsPerPixel)
{
  float scale = aFrame->PresContext()->GetPageScale();
  return gfx3DMatrix::ScalingMatrix(scale, scale, 1);
}

//------------------------------------------------------------------------------
void
nsPageFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                              const nsRect&           aDirtyRect,
                              const nsDisplayListSet& aLists)
{
  nsDisplayListCollection set;

  if (PresContext()->IsScreen()) {
    DisplayBorderBackgroundOutline(aBuilder, aLists);
  }

  nsIFrame *child = mFrames.FirstChild();
  float scale = PresContext()->GetPageScale();
  nsRect clipRect(nsPoint(0, 0), child->GetSize());
  // Note: this computation matches how we compute maxSize.height
  // in nsPageFrame::Reflow
  nscoord expectedPageContentHeight = NSToCoordCeil(GetSize().height / scale);
  if (clipRect.height > expectedPageContentHeight) {
    // We're doing print-selection, with one long page-content frame.
    // Clip to the appropriate page-content slice for the current page.
    NS_ASSERTION(mPageNum > 0, "page num should be positive");
    // Note: The pageContentFrame's y-position has been set such that a zero
    // y-value matches the top edge of the current page.  So, to clip to the
    // current page's content (in coordinates *relative* to the page content
    // frame), we just negate its y-position and add the top margin.
    clipRect.y = NSToCoordCeil((-child->GetRect().y +
                                mPD->mReflowMargin.top) / scale);
    clipRect.height = expectedPageContentHeight;
    NS_ASSERTION(clipRect.y < child->GetSize().height,
                 "Should be clipping to region inside the page content bounds");
  }
  clipRect += aBuilder->ToReferenceFrame(child);

  nsDisplayList content;
  {
    DisplayListClipState::AutoSaveRestore clipState(aBuilder);

    // Overwrite current clip, since we're going to wrap in a transform
    // and the current clip is no longer meaningful.
    clipState.Clear();
    clipState.ClipContainingBlockDescendants(clipRect, nullptr);

    child->BuildDisplayListForStackingContext(aBuilder,
      child->GetVisualOverflowRectRelativeToSelf(), &content);

    // We may need to paint out-of-flow frames whose placeholders are
    // on other pages. Add those pages to our display list. Note that
    // out-of-flow frames can't be placed after their placeholders so
    // we don't have to process earlier pages. The display lists for
    // these extra pages are pruned so that only display items for the
    // page we currently care about (which we would have reached by
    // following placeholders to their out-of-flows) end up on the list.
    nsIFrame* page = child;
    while ((page = GetNextPage(page)) != nullptr) {
      BuildDisplayListForExtraPage(aBuilder, this, page, &content);
    }

    // Add the canvas background color to the bottom of the list. This
    // happens after we've built the list so that AddCanvasBackgroundColorItem
    // can monkey with the contents if necessary.
    nsRect backgroundRect =
      nsRect(aBuilder->ToReferenceFrame(child), child->GetSize());
    PresContext()->GetPresShell()->AddCanvasBackgroundColorItem(
      *aBuilder, content, child, backgroundRect, NS_RGBA(0,0,0,0));
  }

  content.AppendNewToTop(new (aBuilder) nsDisplayTransform(aBuilder, child, &content, ::ComputePageTransform));

  set.Content()->AppendToTop(&content);

  if (PresContext()->IsRootPaginatedDocument()) {
    set.Content()->AppendNewToTop(new (aBuilder)
        nsDisplayGeneric(aBuilder, this, ::PaintHeaderFooter,
                         "HeaderFooter",
                         nsDisplayItem::TYPE_HEADER_FOOTER));
  }

  set.MoveTo(aLists);
}

//------------------------------------------------------------------------------
void
nsPageFrame::SetPageNumInfo(int32_t aPageNumber, int32_t aTotalPages) 
{ 
  mPageNum     = aPageNumber; 
  mTotNumPages = aTotalPages;
}


void
nsPageFrame::PaintHeaderFooter(nsRenderingContext& aRenderingContext,
                               nsPoint aPt)
{
  nsPresContext* pc = PresContext();

  if (!mPD->mPrintSettings) {
    if (pc->Type() == nsPresContext::eContext_PrintPreview || pc->IsDynamic())
      mPD->mPrintSettings = pc->GetPrintSettings();
    if (!mPD->mPrintSettings)
      return;
  }

  nsRect rect(aPt, mRect.Size());
  aRenderingContext.SetColor(NS_RGB(0,0,0));

  // Get the FontMetrics to determine width.height of strings
  nsRefPtr<nsFontMetrics> fontMet;
  pc->DeviceContext()->GetMetricsFor(mPD->mHeadFootFont, nullptr,
                                     pc->GetUserFontSet(),
                                     pc->GetTextPerfMetrics(),
                                     *getter_AddRefs(fontMet));

  aRenderingContext.SetFont(fontMet);

  nscoord ascent = 0;
  nscoord visibleHeight = 0;
  if (fontMet) {
    visibleHeight = fontMet->MaxHeight();
    ascent = fontMet->MaxAscent();
  }

  // print document headers and footers
  nsXPIDLString headerLeft, headerCenter, headerRight;
  mPD->mPrintSettings->GetHeaderStrLeft(getter_Copies(headerLeft));
  mPD->mPrintSettings->GetHeaderStrCenter(getter_Copies(headerCenter));
  mPD->mPrintSettings->GetHeaderStrRight(getter_Copies(headerRight));
  DrawHeaderFooter(aRenderingContext, eHeader,
                   headerLeft, headerCenter, headerRight,
                   rect, ascent, visibleHeight);

  nsXPIDLString footerLeft, footerCenter, footerRight;
  mPD->mPrintSettings->GetFooterStrLeft(getter_Copies(footerLeft));
  mPD->mPrintSettings->GetFooterStrCenter(getter_Copies(footerCenter));
  mPD->mPrintSettings->GetFooterStrRight(getter_Copies(footerRight));
  DrawHeaderFooter(aRenderingContext, eFooter,
                   footerLeft, footerCenter, footerRight,
                   rect, ascent, visibleHeight);
}

void
nsPageFrame::SetSharedPageData(nsSharedPageData* aPD) 
{ 
  mPD = aPD;
  // Set the shared data into the page frame before reflow
  nsPageContentFrame * pcf = static_cast<nsPageContentFrame*>(mFrames.FirstChild());
  if (pcf) {
    pcf->SetSharedPageData(mPD);
  }

}

nsIFrame*
NS_NewPageBreakFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  NS_PRECONDITION(aPresShell, "null PresShell");
  //check that we are only creating page break frames when printing
  NS_ASSERTION(aPresShell->GetPresContext()->IsPaginated(), "created a page break frame while not printing");

  return new (aPresShell) nsPageBreakFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsPageBreakFrame)

nsPageBreakFrame::nsPageBreakFrame(nsStyleContext* aContext) :
  nsLeafFrame(aContext), mHaveReflowed(false)
{
}

nsPageBreakFrame::~nsPageBreakFrame()
{
}

nscoord
nsPageBreakFrame::GetIntrinsicWidth()
{
  return nsPresContext::CSSPixelsToAppUnits(1);
}

nscoord
nsPageBreakFrame::GetIntrinsicHeight()
{
  return 0;
}

nsresult 
nsPageBreakFrame::Reflow(nsPresContext*           aPresContext,
                         nsHTMLReflowMetrics&     aDesiredSize,
                         const nsHTMLReflowState& aReflowState,
                         nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsPageBreakFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowState, aDesiredSize, aStatus);

  // Override reflow, since we don't want to deal with what our
  // computed values are.
  aDesiredSize.Width() = GetIntrinsicWidth();
  aDesiredSize.Height() = (aReflowState.AvailableHeight() == NS_UNCONSTRAINEDSIZE ?
                         0 : aReflowState.AvailableHeight());
  // round the height down to the nearest pixel
  aDesiredSize.Height() -=
    aDesiredSize.Height() % nsPresContext::CSSPixelsToAppUnits(1);

  // Note: not using NS_FRAME_FIRST_REFLOW here, since it's not clear whether
  // DidReflow will always get called before the next Reflow() call.
  mHaveReflowed = true;
  aStatus = NS_FRAME_COMPLETE; 
  return NS_OK;
}

nsIAtom*
nsPageBreakFrame::GetType() const
{
  return nsGkAtoms::pageBreakFrame; 
}

#ifdef DEBUG_FRAME_DUMP
NS_IMETHODIMP
nsPageBreakFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("PageBreak"), aResult);
}
#endif
