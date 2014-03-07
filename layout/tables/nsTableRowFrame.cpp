/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "nsTableRowFrame.h"
#include "nsTableRowGroupFrame.h"
#include "nsIPresShell.h"
#include "nsPresContext.h"
#include "nsStyleContext.h"
#include "nsStyleConsts.h"
#include "nsGkAtoms.h"
#include "nsIContent.h"
#include "nsTableFrame.h"
#include "nsTableCellFrame.h"
#include "nsCSSRendering.h"
#include "nsHTMLParts.h"
#include "nsTableColGroupFrame.h"
#include "nsTableColFrame.h"
#include "nsCOMPtr.h"
#include "nsDisplayList.h"
#include "nsIFrameInlines.h"
#include <algorithm>

using namespace mozilla;

struct nsTableCellReflowState : public nsHTMLReflowState
{
  nsTableCellReflowState(nsPresContext*           aPresContext,
                         const nsHTMLReflowState& aParentReflowState,
                         nsIFrame*                aFrame,
                         const nsSize&            aAvailableSpace,
                         uint32_t                 aFlags = 0)
    : nsHTMLReflowState(aPresContext, aParentReflowState, aFrame,
                        aAvailableSpace, -1, -1, aFlags)
  {
  }

  void FixUp(const nsSize& aAvailSpace);
};

void nsTableCellReflowState::FixUp(const nsSize& aAvailSpace)
{
  // fix the mComputed values during a pass 2 reflow since the cell can be a percentage base
  NS_WARN_IF_FALSE(NS_UNCONSTRAINEDSIZE != aAvailSpace.width,
                   "have unconstrained width; this should only result from "
                   "very large sizes, not attempts at intrinsic width "
                   "calculation");
  if (NS_UNCONSTRAINEDSIZE != ComputedWidth()) {
    nscoord computedWidth =
      aAvailSpace.width - mComputedBorderPadding.LeftRight();
    computedWidth = std::max(0, computedWidth);
    SetComputedWidth(computedWidth);
  }
  if (NS_UNCONSTRAINEDSIZE != ComputedHeight() &&
      NS_UNCONSTRAINEDSIZE != aAvailSpace.height) {
    nscoord computedHeight =
      aAvailSpace.height - mComputedBorderPadding.TopBottom();
    computedHeight = std::max(0, computedHeight);
    SetComputedHeight(computedHeight);
  }
}

void
nsTableRowFrame::InitChildReflowState(nsPresContext&         aPresContext,
                                      const nsSize&           aAvailSize,
                                      bool                    aBorderCollapse,
                                      nsTableCellReflowState& aReflowState)
{
  nsMargin collapseBorder;
  nsMargin* pCollapseBorder = nullptr;
  if (aBorderCollapse) {
    // we only reflow cells, so don't need to check frame type
    nsBCTableCellFrame* bcCellFrame = (nsBCTableCellFrame*)aReflowState.frame;
    if (bcCellFrame) {
      pCollapseBorder = bcCellFrame->GetBorderWidth(collapseBorder);
    }
  }
  aReflowState.Init(&aPresContext, -1, -1, pCollapseBorder);
  aReflowState.FixUp(aAvailSize);
}

void 
nsTableRowFrame::SetFixedHeight(nscoord aValue)
{
  nscoord height = std::max(0, aValue);
  if (HasFixedHeight()) {
    if (height > mStyleFixedHeight) {
      mStyleFixedHeight = height;
    }
  }
  else {
    mStyleFixedHeight = height;
    if (height > 0) {
      SetHasFixedHeight(true);
    }
  }
}

void 
nsTableRowFrame::SetPctHeight(float  aPctValue,
                              bool aForce)
{
  nscoord height = std::max(0, NSToCoordRound(aPctValue * 100.0f));
  if (HasPctHeight()) {
    if ((height > mStylePctHeight) || aForce) {
      mStylePctHeight = height;
    }
  }
  else {
    mStylePctHeight = height;
    if (height > 0) {
      SetHasPctHeight(true);
    }
  }
}

/* ----------- nsTableRowFrame ---------- */

NS_QUERYFRAME_HEAD(nsTableRowFrame)
  NS_QUERYFRAME_ENTRY(nsTableRowFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsContainerFrame)

nsTableRowFrame::nsTableRowFrame(nsStyleContext* aContext)
  : nsContainerFrame(aContext)
{
  mBits.mRowIndex = mBits.mFirstInserted = 0;
  ResetHeight(0);
}

nsTableRowFrame::~nsTableRowFrame()
{
}

void
nsTableRowFrame::Init(nsIContent*      aContent,
                      nsIFrame*        aParent,
                      nsIFrame*        aPrevInFlow)
{
  // Let the base class do its initialization
  nsContainerFrame::Init(aContent, aParent, aPrevInFlow);

  NS_ASSERTION(NS_STYLE_DISPLAY_TABLE_ROW == StyleDisplay()->mDisplay,
               "wrong display on table row frame");

  if (aPrevInFlow) {
    // Set the row index
    nsTableRowFrame* rowFrame = (nsTableRowFrame*)aPrevInFlow;
    
    SetRowIndex(rowFrame->GetRowIndex());
  }
}

/* virtual */ void
nsTableRowFrame::DidSetStyleContext(nsStyleContext* aOldStyleContext)
{
  nsContainerFrame::DidSetStyleContext(aOldStyleContext);

  if (!aOldStyleContext) //avoid this on init
    return;
     
  nsTableFrame* tableFrame = nsTableFrame::GetTableFrame(this);
  if (tableFrame->IsBorderCollapse() &&
      tableFrame->BCRecalcNeeded(aOldStyleContext, StyleContext())) {
    nsIntRect damageArea(0, GetRowIndex(), tableFrame->GetColCount(), 1);
    tableFrame->AddBCDamageArea(damageArea);
  }
}

nsresult
nsTableRowFrame::AppendFrames(ChildListID     aListID,
                              nsFrameList&    aFrameList)
{
  NS_ASSERTION(aListID == kPrincipalList, "unexpected child list");

  const nsFrameList::Slice& newCells = mFrames.AppendFrames(nullptr, aFrameList);

  // Add the new cell frames to the table
  nsTableFrame* tableFrame = nsTableFrame::GetTableFrame(this);
  for (nsFrameList::Enumerator e(newCells) ; !e.AtEnd(); e.Next()) {
    nsIFrame *childFrame = e.get();
    NS_ASSERTION(IS_TABLE_CELL(childFrame->GetType()),"Not a table cell frame/pseudo frame construction failure");
    tableFrame->AppendCell(static_cast<nsTableCellFrame&>(*childFrame), GetRowIndex());
  }

  PresContext()->PresShell()->FrameNeedsReflow(this, nsIPresShell::eTreeChange,
                                               NS_FRAME_HAS_DIRTY_CHILDREN);
  tableFrame->SetGeometryDirty();

  return NS_OK;
}


nsresult
nsTableRowFrame::InsertFrames(ChildListID     aListID,
                              nsIFrame*       aPrevFrame,
                              nsFrameList&    aFrameList)
{
  NS_ASSERTION(aListID == kPrincipalList, "unexpected child list");
  NS_ASSERTION(!aPrevFrame || aPrevFrame->GetParent() == this,
               "inserting after sibling frame with different parent");
  //Insert Frames in the frame list
  const nsFrameList::Slice& newCells = mFrames.InsertFrames(nullptr, aPrevFrame, aFrameList);

  // Get the table frame
  nsTableFrame* tableFrame = nsTableFrame::GetTableFrame(this);
  nsIAtom* cellFrameType = tableFrame->IsBorderCollapse() ? nsGkAtoms::bcTableCellFrame : nsGkAtoms::tableCellFrame;
  nsTableCellFrame* prevCellFrame = (nsTableCellFrame *)nsTableFrame::GetFrameAtOrBefore(this, aPrevFrame, cellFrameType);
  nsTArray<nsTableCellFrame*> cellChildren;
  for (nsFrameList::Enumerator e(newCells); !e.AtEnd(); e.Next()) {
    nsIFrame *childFrame = e.get();
    NS_ASSERTION(IS_TABLE_CELL(childFrame->GetType()),"Not a table cell frame/pseudo frame construction failure");
    cellChildren.AppendElement(static_cast<nsTableCellFrame*>(childFrame));
  }
  // insert the cells into the cell map
  int32_t colIndex = -1;
  if (prevCellFrame) {
    prevCellFrame->GetColIndex(colIndex);
  }
  tableFrame->InsertCells(cellChildren, GetRowIndex(), colIndex);
  
  PresContext()->PresShell()->FrameNeedsReflow(this, nsIPresShell::eTreeChange,
                                               NS_FRAME_HAS_DIRTY_CHILDREN);
  tableFrame->SetGeometryDirty();

  return NS_OK;
}

nsresult
nsTableRowFrame::RemoveFrame(ChildListID     aListID,
                             nsIFrame*       aOldFrame)
{
  NS_ASSERTION(aListID == kPrincipalList, "unexpected child list");

  nsTableFrame* tableFrame = nsTableFrame::GetTableFrame(this);
  nsTableCellFrame *cellFrame = do_QueryFrame(aOldFrame);
  if (cellFrame) {
    int32_t colIndex;
    cellFrame->GetColIndex(colIndex);
    // remove the cell from the cell map
    tableFrame->RemoveCell(cellFrame, GetRowIndex());

    // Remove the frame and destroy it
    mFrames.DestroyFrame(aOldFrame);

    PresContext()->PresShell()->
      FrameNeedsReflow(this, nsIPresShell::eTreeChange,
                       NS_FRAME_HAS_DIRTY_CHILDREN);
    tableFrame->SetGeometryDirty();
  }
  else {
    NS_ERROR("unexpected frame type");
    return NS_ERROR_INVALID_ARG;
  }

  return NS_OK;
}

/* virtual */ nsMargin
nsTableRowFrame::GetUsedMargin() const
{
  return nsMargin(0,0,0,0);
}

/* virtual */ nsMargin
nsTableRowFrame::GetUsedBorder() const
{
  return nsMargin(0,0,0,0);
}

/* virtual */ nsMargin
nsTableRowFrame::GetUsedPadding() const
{
  return nsMargin(0,0,0,0);
}

nscoord 
GetHeightOfRowsSpannedBelowFirst(nsTableCellFrame& aTableCellFrame,
                                 nsTableFrame&     aTableFrame)
{
  nscoord height = 0;
  nscoord cellSpacingY = aTableFrame.GetCellSpacingY();
  int32_t rowSpan = aTableFrame.GetEffectiveRowSpan(aTableCellFrame);
  // add in height of rows spanned beyond the 1st one
  nsIFrame* nextRow = aTableCellFrame.GetParent()->GetNextSibling();
  for (int32_t rowX = 1; ((rowX < rowSpan) && nextRow);) {
    if (nsGkAtoms::tableRowFrame == nextRow->GetType()) {
      height += nextRow->GetSize().height;
      rowX++;
    }
    height += cellSpacingY;
    nextRow = nextRow->GetNextSibling();
  }
  return height;
}

nsTableCellFrame*  
nsTableRowFrame::GetFirstCell() 
{
  nsIFrame* childFrame = mFrames.FirstChild();
  while (childFrame) {
    nsTableCellFrame *cellFrame = do_QueryFrame(childFrame);
    if (cellFrame) {
      return cellFrame;
    }
    childFrame = childFrame->GetNextSibling();
  }
  return nullptr;
}

/**
 * Post-reflow hook. This is where the table row does its post-processing
 */
void
nsTableRowFrame::DidResize()
{
  // Resize and re-align the cell frames based on our row height
  nsTableFrame* tableFrame = nsTableFrame::GetTableFrame(this);
  nsTableIterator iter(*this);
  nsIFrame* childFrame = iter.First();
  
  nsHTMLReflowMetrics desiredSize(GetWritingMode()); // ???
  desiredSize.Width() = mRect.width;
  desiredSize.Height() = mRect.height;
  desiredSize.SetOverflowAreasToDesiredBounds();

  while (childFrame) {
    nsTableCellFrame *cellFrame = do_QueryFrame(childFrame);
    if (cellFrame) {
      nscoord cellHeight = mRect.height + GetHeightOfRowsSpannedBelowFirst(*cellFrame, *tableFrame);

      // resize the cell's height
      nsRect cellRect = cellFrame->GetRect();
      nsRect cellVisualOverflow = cellFrame->GetVisualOverflowRect();
      if (cellRect.height != cellHeight)
      {
        cellFrame->SetSize(nsSize(cellRect.width, cellHeight));
        nsTableFrame::InvalidateTableFrame(cellFrame, cellRect,
                                           cellVisualOverflow,
                                           false);
      }

      // realign cell content based on the new height.  We might be able to
      // skip this if the height didn't change... maybe.  Hard to tell.
      cellFrame->VerticallyAlignChild(mMaxCellAscent);
      
      // Always store the overflow, even if the height didn't change, since
      // we'll lose part of our overflow area otherwise.
      ConsiderChildOverflow(desiredSize.mOverflowAreas, cellFrame);

      // Note that if the cell's *content* needs to change in response
      // to this height, it will get a special height reflow.
    }
    // Get the next child
    childFrame = iter.Next();
  }
  FinishAndStoreOverflow(&desiredSize);
  if (HasView()) {
    nsContainerFrame::SyncFrameViewAfterReflow(PresContext(), this, GetView(),
                                               desiredSize.VisualOverflow(), 0);
  }
  // Let our base class do the usual work
}

// returns max-ascent amongst all cells that have 'vertical-align: baseline'
// *including* cells with rowspans
nscoord nsTableRowFrame::GetMaxCellAscent() const
{
  return mMaxCellAscent;
}

nscoord nsTableRowFrame::GetRowBaseline()
{
  if(mMaxCellAscent)
    return mMaxCellAscent;

  // If we don't have a baseline on any of the cells we go for the lowest 
  // content edge of the inner block frames. 
  // Every table cell has a cell frame with its border and padding. Inside
  // the cell is a block frame. The cell is as high as the tallest cell in
  // the parent row. As a consequence the block frame might not touch both
  // the top and the bottom padding of it parent cell frame at the same time.
  // 
  // bbbbbbbbbbbbbbbbbb             cell border:  b
  // bppppppppppppppppb             cell padding: p
  // bpxxxxxxxxxxxxxxpb             inner block:  x
  // bpx            xpb
  // bpx            xpb
  // bpx            xpb
  // bpxxxxxxxxxxxxxxpb  base line
  // bp              pb
  // bp              pb
  // bppppppppppppppppb
  // bbbbbbbbbbbbbbbbbb

  nsTableIterator iter(*this);
  nsIFrame* childFrame = iter.First();
  nscoord ascent = 0;
   while (childFrame) {
    if (IS_TABLE_CELL(childFrame->GetType())) {
      nsIFrame* firstKid = childFrame->GetFirstPrincipalChild();
      ascent = std::max(ascent, firstKid->GetRect().YMost());
    }
    // Get the next child
    childFrame = iter.Next();
  }
  return ascent;
}
nscoord
nsTableRowFrame::GetHeight(nscoord aPctBasis) const
{
  nscoord height = 0;
  if ((aPctBasis > 0) && HasPctHeight()) {
    height = NSToCoordRound(GetPctHeight() * (float)aPctBasis);
  }
  if (HasFixedHeight()) {
    height = std::max(height, GetFixedHeight());
  }
  return std::max(height, GetContentHeight());
}

void 
nsTableRowFrame::ResetHeight(nscoord aFixedHeight)
{
  SetHasFixedHeight(false);
  SetHasPctHeight(false);
  SetFixedHeight(0);
  SetPctHeight(0);
  SetContentHeight(0);

  if (aFixedHeight > 0) {
    SetFixedHeight(aFixedHeight);
  }

  mMaxCellAscent = 0;
  mMaxCellDescent = 0;
}

void
nsTableRowFrame::UpdateHeight(nscoord           aHeight,
                              nscoord           aAscent,
                              nscoord           aDescent,
                              nsTableFrame*     aTableFrame,
                              nsTableCellFrame* aCellFrame)
{
  if (!aTableFrame || !aCellFrame) {
    NS_ASSERTION(false , "invalid call");
    return;
  }

  if (aHeight != NS_UNCONSTRAINEDSIZE) {
    if (!(aCellFrame->HasVerticalAlignBaseline())) { // only the cell's height matters
      if (GetHeight() < aHeight) {
        int32_t rowSpan = aTableFrame->GetEffectiveRowSpan(*aCellFrame);
        if (rowSpan == 1) {
          SetContentHeight(aHeight);
        }
      }
    }
    else { // the alignment on the baseline can change the height
      NS_ASSERTION((aAscent != NS_UNCONSTRAINEDSIZE) && (aDescent != NS_UNCONSTRAINEDSIZE), "invalid call");
      // see if this is a long ascender
      if (mMaxCellAscent < aAscent) {
        mMaxCellAscent = aAscent;
      }
      // see if this is a long descender and without rowspan
      if (mMaxCellDescent < aDescent) {
        int32_t rowSpan = aTableFrame->GetEffectiveRowSpan(*aCellFrame);
        if (rowSpan == 1) {
          mMaxCellDescent = aDescent;
        }
      }
      // keep the tallest height in sync
      if (GetHeight() < mMaxCellAscent + mMaxCellDescent) {
        SetContentHeight(mMaxCellAscent + mMaxCellDescent);
      }
    }
  }
}

nscoord
nsTableRowFrame::CalcHeight(const nsHTMLReflowState& aReflowState)
{
  nsTableFrame* tableFrame = nsTableFrame::GetTableFrame(this);
  nscoord computedHeight = (NS_UNCONSTRAINEDSIZE == aReflowState.ComputedHeight())
                            ? 0 : aReflowState.ComputedHeight();
  ResetHeight(computedHeight);

  const nsStylePosition* position = StylePosition();
  if (position->mHeight.ConvertsToLength()) {
    SetFixedHeight(nsRuleNode::ComputeCoordPercentCalc(position->mHeight, 0));
  }
  else if (eStyleUnit_Percent == position->mHeight.GetUnit()) {
    SetPctHeight(position->mHeight.GetPercentValue());
  }
  // calc() with percentages is treated like 'auto' on table rows.

  for (nsIFrame* kidFrame = mFrames.FirstChild(); kidFrame;
       kidFrame = kidFrame->GetNextSibling()) {
    nsTableCellFrame *cellFrame = do_QueryFrame(kidFrame);
    if (cellFrame) {
      nsSize desSize = cellFrame->GetDesiredSize();
      if ((NS_UNCONSTRAINEDSIZE == aReflowState.AvailableHeight()) && !GetPrevInFlow()) {
        CalculateCellActualHeight(cellFrame, desSize.height);
      }
      // height may have changed, adjust descent to absorb any excess difference
      nscoord ascent;
       if (!kidFrame->GetFirstPrincipalChild()->GetFirstPrincipalChild())
         ascent = desSize.height;
       else
         ascent = cellFrame->GetCellBaseline();
      nscoord descent = desSize.height - ascent;
      UpdateHeight(desSize.height, ascent, descent, tableFrame, cellFrame);
    }
  }
  return GetHeight();
}

/**
 * We need a custom display item for table row backgrounds. This is only used
 * when the table row is the root of a stacking context (e.g., has 'opacity').
 * Table row backgrounds can extend beyond the row frame bounds, when
 * the row contains row-spanning cells.
 */
class nsDisplayTableRowBackground : public nsDisplayTableItem {
public:
  nsDisplayTableRowBackground(nsDisplayListBuilder* aBuilder,
                              nsTableRowFrame* aFrame) :
    nsDisplayTableItem(aBuilder, aFrame) {
    MOZ_COUNT_CTOR(nsDisplayTableRowBackground);
  }
#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplayTableRowBackground() {
    MOZ_COUNT_DTOR(nsDisplayTableRowBackground);
  }
#endif

  virtual void ComputeInvalidationRegion(nsDisplayListBuilder* aBuilder,
                                         const nsDisplayItemGeometry* aGeometry,
                                         nsRegion *aInvalidRegion) MOZ_OVERRIDE;
  virtual void Paint(nsDisplayListBuilder* aBuilder,
                     nsRenderingContext* aCtx) MOZ_OVERRIDE;
  NS_DISPLAY_DECL_NAME("TableRowBackground", TYPE_TABLE_ROW_BACKGROUND)
};

void
nsDisplayTableRowBackground::ComputeInvalidationRegion(nsDisplayListBuilder* aBuilder,
                                                       const nsDisplayItemGeometry* aGeometry,
                                                       nsRegion *aInvalidRegion)
{
  if (aBuilder->ShouldSyncDecodeImages()) {
    if (nsTableFrame::AnyTablePartHasUndecodedBackgroundImage(mFrame, mFrame->GetNextSibling())) {
      bool snap;
      aInvalidRegion->Or(*aInvalidRegion, GetBounds(aBuilder, &snap));
    }
  }

  nsDisplayTableItem::ComputeInvalidationRegion(aBuilder, aGeometry, aInvalidRegion);
}

void
nsDisplayTableRowBackground::Paint(nsDisplayListBuilder* aBuilder,
                                   nsRenderingContext* aCtx)
{
  nsTableFrame* tableFrame = nsTableFrame::GetTableFrame(mFrame);
  TableBackgroundPainter painter(tableFrame,
                                 TableBackgroundPainter::eOrigin_TableRow,
                                 mFrame->PresContext(), *aCtx,
                                 mVisibleRect, ToReferenceFrame(),
                                 aBuilder->GetBackgroundPaintFlags());
  painter.PaintRow(static_cast<nsTableRowFrame*>(mFrame));
}

void
nsTableRowFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                  const nsRect&           aDirtyRect,
                                  const nsDisplayListSet& aLists)
{
  nsDisplayTableItem* item = nullptr;
  if (IsVisibleInSelection(aBuilder)) {
    bool isRoot = aBuilder->IsAtRootOfPseudoStackingContext();
    if (isRoot) {
      // This background is created regardless of whether this frame is
      // visible or not. Visibility decisions are delegated to the
      // table background painter.
      // We would use nsDisplayGeneric for this rare case except that we
      // need the background to be larger than the row frame in some
      // cases.
      item = new (aBuilder) nsDisplayTableRowBackground(aBuilder, this);
      aLists.BorderBackground()->AppendNewToTop(item);
    }
  }
  nsTableFrame::DisplayGenericTablePart(aBuilder, this, aDirtyRect, aLists, item);
}

int
nsTableRowFrame::GetSkipSides(const nsHTMLReflowState* aReflowState) const
{
  int skip = 0;
  if (nullptr != GetPrevInFlow()) {
    skip |= 1 << NS_SIDE_TOP;
  }
  if (nullptr != GetNextInFlow()) {
    skip |= 1 << NS_SIDE_BOTTOM;
  }
  return skip;
}

// Calculate the cell's actual height given its pass2  height.
// Takes into account the specified height (in the style).
// Modifies the desired height that is passed in.
nsresult
nsTableRowFrame::CalculateCellActualHeight(nsTableCellFrame* aCellFrame,
                                           nscoord&          aDesiredHeight)
{
  nscoord specifiedHeight = 0;
  
  // Get the height specified in the style information
  const nsStylePosition* position = aCellFrame->StylePosition();

  nsTableFrame* tableFrame = nsTableFrame::GetTableFrame(this);
  int32_t rowSpan = tableFrame->GetEffectiveRowSpan(*aCellFrame);
  
  switch (position->mHeight.GetUnit()) {
    case eStyleUnit_Calc: {
      if (position->mHeight.CalcHasPercent()) {
        // Treat this like "auto"
        break;
      }
      // Fall through to the coord case
    }
    case eStyleUnit_Coord: {
      nscoord outsideBoxSizing = 0;
      // In quirks mode, table cell width should be content-box, but height
      // should be border-box.
      // Because of this historic anomaly, we do not use quirk.css
      // (since we can't specify one value of box-sizing for width and another
      // for height)
      if (PresContext()->CompatibilityMode() != eCompatibility_NavQuirks) {
        switch (position->mBoxSizing) {
          case NS_STYLE_BOX_SIZING_CONTENT:
            outsideBoxSizing = aCellFrame->GetUsedBorderAndPadding().TopBottom();
            break;
          case NS_STYLE_BOX_SIZING_PADDING:
            outsideBoxSizing = aCellFrame->GetUsedBorder().TopBottom();
            break;
          default:
            // NS_STYLE_BOX_SIZING_BORDER
            break;
        }
      }

      specifiedHeight =
        nsRuleNode::ComputeCoordPercentCalc(position->mHeight, 0) +
          outsideBoxSizing;

      if (1 == rowSpan) 
        SetFixedHeight(specifiedHeight);
      break;
    }
    case eStyleUnit_Percent: {
      if (1 == rowSpan) 
        SetPctHeight(position->mHeight.GetPercentValue());
      // pct heights are handled when all of the cells are finished, so don't set specifiedHeight 
      break;
    }
    case eStyleUnit_Auto:
    default:
      break;
  }

  // If the specified height is greater than the desired height, then use the specified height
  if (specifiedHeight > aDesiredHeight)
    aDesiredHeight = specifiedHeight;

  return NS_OK;
}

// Calculates the available width for the table cell based on the known
// column widths taking into account column spans and column spacing
static nscoord
CalcAvailWidth(nsTableFrame&     aTableFrame,
               nsTableCellFrame& aCellFrame,
               nscoord           aCellSpacingX)
{
  nscoord cellAvailWidth = 0;
  int32_t colIndex;
  aCellFrame.GetColIndex(colIndex);
  int32_t colspan = aTableFrame.GetEffectiveColSpan(aCellFrame);
  NS_ASSERTION(colspan > 0, "effective colspan should be positive");

  for (int32_t spanX = 0; spanX < colspan; spanX++) {
    cellAvailWidth += aTableFrame.GetColumnWidth(colIndex + spanX);
    if (spanX > 0 &&
        aTableFrame.ColumnHasCellSpacingBefore(colIndex + spanX)) {
      cellAvailWidth += aCellSpacingX;
    }
  }
  return cellAvailWidth;
}

nscoord
GetSpaceBetween(int32_t       aPrevColIndex,
                int32_t       aColIndex,
                int32_t       aColSpan,
                nsTableFrame& aTableFrame,
                nscoord       aCellSpacingX,
                bool          aIsLeftToRight,
                bool          aCheckVisibility)
{
  nscoord space = 0;
  int32_t colX;
  if (aIsLeftToRight) {
    for (colX = aPrevColIndex + 1; aColIndex > colX; colX++) {
      bool isCollapsed = false;
      if (!aCheckVisibility) {
        space += aTableFrame.GetColumnWidth(colX);
      }
      else {
        nsTableColFrame* colFrame = aTableFrame.GetColFrame(colX);
        const nsStyleVisibility* colVis = colFrame->StyleVisibility();
        bool collapseCol = (NS_STYLE_VISIBILITY_COLLAPSE == colVis->mVisible);
        nsIFrame* cgFrame = colFrame->GetParent();
        const nsStyleVisibility* groupVis = cgFrame->StyleVisibility();
        bool collapseGroup = (NS_STYLE_VISIBILITY_COLLAPSE ==
                                groupVis->mVisible);
        isCollapsed = collapseCol || collapseGroup;
        if (!isCollapsed)
          space += aTableFrame.GetColumnWidth(colX);
      }
      if (!isCollapsed && aTableFrame.ColumnHasCellSpacingBefore(colX)) {
        space += aCellSpacingX;
      }
    }
  } 
  else {
    int32_t lastCol = aColIndex + aColSpan - 1;
    for (colX = aPrevColIndex - 1; colX > lastCol; colX--) {
      bool isCollapsed = false;
      if (!aCheckVisibility) {
        space += aTableFrame.GetColumnWidth(colX);
      }
      else {
        nsTableColFrame* colFrame = aTableFrame.GetColFrame(colX);
        const nsStyleVisibility* colVis = colFrame->StyleVisibility();
        bool collapseCol = (NS_STYLE_VISIBILITY_COLLAPSE == colVis->mVisible);
        nsIFrame* cgFrame = colFrame->GetParent();
        const nsStyleVisibility* groupVis = cgFrame->StyleVisibility();
        bool collapseGroup = (NS_STYLE_VISIBILITY_COLLAPSE ==
                                groupVis->mVisible);
        isCollapsed = collapseCol || collapseGroup;
        if (!isCollapsed)
          space += aTableFrame.GetColumnWidth(colX);
      }
      if (!isCollapsed && aTableFrame.ColumnHasCellSpacingBefore(colX)) {
        space += aCellSpacingX;
      }
    }
  }
  return space;
}

// subtract the heights of aRow's prev in flows from the unpaginated height
static
nscoord CalcHeightFromUnpaginatedHeight(nsPresContext*   aPresContext,
                                        nsTableRowFrame& aRow)
{
  nscoord height = 0;
  nsTableRowFrame* firstInFlow =
    static_cast<nsTableRowFrame*>(aRow.FirstInFlow());
  if (firstInFlow->HasUnpaginatedHeight()) {
    height = firstInFlow->GetUnpaginatedHeight(aPresContext);
    for (nsIFrame* prevInFlow = aRow.GetPrevInFlow(); prevInFlow;
         prevInFlow = prevInFlow->GetPrevInFlow()) {
      height -= prevInFlow->GetSize().height;
    }
  }
  return std::max(height, 0);
}

nsresult
nsTableRowFrame::ReflowChildren(nsPresContext*          aPresContext,
                                nsHTMLReflowMetrics&     aDesiredSize,
                                const nsHTMLReflowState& aReflowState,
                                nsTableFrame&            aTableFrame,
                                nsReflowStatus&          aStatus)
{
  aStatus = NS_FRAME_COMPLETE;

  // XXXldb Should we be checking constrained height instead?
  const bool isPaginated = aPresContext->IsPaginated();
  const bool borderCollapse = aTableFrame.IsBorderCollapse();
  nsresult rv = NS_OK;
  nscoord cellSpacingX = aTableFrame.GetCellSpacingX();
  int32_t cellColSpan = 1;  // must be defined here so it's set properly for non-cell kids
  
  nsTableIterator iter(*this);
  // remember the col index of the previous cell to handle rowspans into this row
  int32_t firstPrevColIndex = (iter.IsLeftToRight()) ? -1 : aTableFrame.GetColCount();
  int32_t prevColIndex  = firstPrevColIndex;
  nscoord x = 0; // running total of children x offset

  // This computes the max of all cell heights
  nscoord cellMaxHeight = 0;

  // Reflow each of our existing cell frames
  for (nsIFrame* kidFrame = iter.First(); kidFrame; kidFrame = iter.Next()) {
    nsTableCellFrame *cellFrame = do_QueryFrame(kidFrame);
    if (!cellFrame) {
      // XXXldb nsCSSFrameConstructor needs to enforce this!
      NS_NOTREACHED("yikes, a non-row child");

      // it's an unknown frame type, give it a generic reflow and ignore the results
      nsTableCellReflowState kidReflowState(aPresContext, aReflowState,
                                            kidFrame, nsSize(0,0),
                                            nsHTMLReflowState::CALLER_WILL_INIT);
      InitChildReflowState(*aPresContext, nsSize(0,0), false, kidReflowState);
      nsHTMLReflowMetrics desiredSize(aReflowState);
      nsReflowStatus  status;
      ReflowChild(kidFrame, aPresContext, desiredSize, kidReflowState, 0, 0, 0, status);
      kidFrame->DidReflow(aPresContext, nullptr, nsDidReflowStatus::FINISHED);

      continue;
    }

    // See if we should only reflow the dirty child frames
    bool doReflowChild = true;
    if (!aReflowState.ShouldReflowAllKids() &&
        !aTableFrame.IsGeometryDirty() &&
        !NS_SUBTREE_DIRTY(kidFrame)) {
      if (!aReflowState.mFlags.mSpecialHeightReflow)
        doReflowChild = false;
    }
    else if ((NS_UNCONSTRAINEDSIZE != aReflowState.AvailableHeight())) {
      // We don't reflow a rowspan >1 cell here with a constrained height. 
      // That happens in nsTableRowGroupFrame::SplitSpanningCells.
      if (aTableFrame.GetEffectiveRowSpan(*cellFrame) > 1) {
        doReflowChild = false;
      }
    }
    if (aReflowState.mFlags.mSpecialHeightReflow) {
      if (!isPaginated && !(cellFrame->GetStateBits() &
                            NS_FRAME_CONTAINS_RELATIVE_HEIGHT)) {
        continue;
      }
    }

    int32_t cellColIndex;
    cellFrame->GetColIndex(cellColIndex);
    cellColSpan = aTableFrame.GetEffectiveColSpan(*cellFrame);

    // If the adjacent cell is in a prior row (because of a rowspan) add in the space
    if ((iter.IsLeftToRight() && (prevColIndex != (cellColIndex - 1))) ||
        (!iter.IsLeftToRight() && (prevColIndex != cellColIndex + cellColSpan))) {
      x += GetSpaceBetween(prevColIndex, cellColIndex, cellColSpan, aTableFrame, 
                           cellSpacingX, iter.IsLeftToRight(), false);
    }

    // remember the rightmost (ltr) or leftmost (rtl) column this cell spans into
    prevColIndex = (iter.IsLeftToRight()) ? cellColIndex + (cellColSpan - 1) : cellColIndex;

    // Reflow the child frame
    nsRect kidRect = kidFrame->GetRect();
    nsRect kidVisualOverflow = kidFrame->GetVisualOverflowRect();
    bool firstReflow =
      (kidFrame->GetStateBits() & NS_FRAME_FIRST_REFLOW) != 0;

    if (doReflowChild) {
      // Calculate the available width for the table cell using the known column widths
      nscoord availCellWidth =
        CalcAvailWidth(aTableFrame, *cellFrame, cellSpacingX);

      nsHTMLReflowMetrics desiredSize(aReflowState);

      // If the avail width is not the same as last time we reflowed the cell or
      // the cell wants to be bigger than what was available last time or
      // it is a style change reflow or we are printing, then we must reflow the
      // cell. Otherwise we can skip the reflow.
      // XXXldb Why is this condition distinct from doReflowChild above?
      nsSize cellDesiredSize = cellFrame->GetDesiredSize();
      if ((availCellWidth != cellFrame->GetPriorAvailWidth())       ||
          (cellDesiredSize.width > cellFrame->GetPriorAvailWidth()) ||
          (GetStateBits() & NS_FRAME_IS_DIRTY)                      ||
          isPaginated                                               ||
          NS_SUBTREE_DIRTY(cellFrame)                               ||
          // See if it needs a special reflow, or if it had one that we need to undo.
          (cellFrame->GetStateBits() & NS_FRAME_CONTAINS_RELATIVE_HEIGHT) ||
          HasPctHeight()) {
        // Reflow the cell to fit the available width, height
        // XXX The old IR_ChildIsDirty code used availCellWidth here.
        nsSize  kidAvailSize(availCellWidth, aReflowState.AvailableHeight());

        // Reflow the child
        nsTableCellReflowState kidReflowState(aPresContext, aReflowState, 
                                              kidFrame, kidAvailSize,
                                              nsHTMLReflowState::CALLER_WILL_INIT);
        InitChildReflowState(*aPresContext, kidAvailSize, borderCollapse,
                             kidReflowState);

        nsReflowStatus status;
        rv = ReflowChild(kidFrame, aPresContext, desiredSize, kidReflowState,
                         x, 0, 0, status);

        // allow the table to determine if/how the table needs to be rebalanced
        // If any of the cells are not complete, then we're not complete
        if (NS_FRAME_IS_NOT_COMPLETE(status)) {
          aStatus = NS_FRAME_NOT_COMPLETE;
        }
      }
      else {
        if (x != kidRect.x) {
          kidFrame->InvalidateFrameSubtree();
        }
        
        desiredSize.Width() = cellDesiredSize.width;
        desiredSize.Height() = cellDesiredSize.height;
        desiredSize.mOverflowAreas = cellFrame->GetOverflowAreas();

        // if we are in a floated table, our position is not yet established, so we cannot reposition our views
        // the containing block will do this for us after positioning the table
        if (!aTableFrame.IsFloating()) {
          // Because we may have moved the frame we need to make sure any views are
          // positioned properly. We have to do this, because any one of our parent
          // frames could have moved and we have no way of knowing...
          nsTableFrame::RePositionViews(kidFrame);
        }
      }
      
      if (NS_UNCONSTRAINEDSIZE == aReflowState.AvailableHeight()) {
        if (!GetPrevInFlow()) {
          // Calculate the cell's actual height given its pass2 height. This
          // function takes into account the specified height (in the style)
          CalculateCellActualHeight(cellFrame, desiredSize.Height());
        }
        // height may have changed, adjust descent to absorb any excess difference
        nscoord ascent;
        if (!kidFrame->GetFirstPrincipalChild()->GetFirstPrincipalChild())
          ascent = desiredSize.Height();
        else
          ascent = ((nsTableCellFrame *)kidFrame)->GetCellBaseline();
        nscoord descent = desiredSize.Height() - ascent;
        UpdateHeight(desiredSize.Height(), ascent, descent, &aTableFrame, cellFrame);
      }
      else {
        cellMaxHeight = std::max(cellMaxHeight, desiredSize.Height());
        int32_t rowSpan = aTableFrame.GetEffectiveRowSpan((nsTableCellFrame&)*kidFrame);
        if (1 == rowSpan) {
          SetContentHeight(cellMaxHeight);
        }
      }

      // Place the child
      desiredSize.Width() = availCellWidth;

      FinishReflowChild(kidFrame, aPresContext, desiredSize, nullptr, x, 0, 0);

      nsTableFrame::InvalidateTableFrame(kidFrame, kidRect, kidVisualOverflow,
                                         firstReflow);
      
      x += desiredSize.Width();  
    }
    else {
      if (kidRect.x != x) {
        // Invalidate the old position
        kidFrame->InvalidateFrameSubtree();
        // move to the new position
        kidFrame->SetPosition(nsPoint(x, kidRect.y));
        nsTableFrame::RePositionViews(kidFrame);
        // invalidate the new position
        kidFrame->InvalidateFrameSubtree();
      }
      // we need to account for the cell's width even if it isn't reflowed
      x += kidRect.width;

      if (kidFrame->GetNextInFlow()) {
        aStatus = NS_FRAME_NOT_COMPLETE;
      }
    }
    ConsiderChildOverflow(aDesiredSize.mOverflowAreas, kidFrame);
    x += cellSpacingX;
  }

  // just set our width to what was available. The table will calculate the width and not use our value.
  aDesiredSize.Width() = aReflowState.AvailableWidth();

  if (aReflowState.mFlags.mSpecialHeightReflow) {
    aDesiredSize.Height() = mRect.height;
  }
  else if (NS_UNCONSTRAINEDSIZE == aReflowState.AvailableHeight()) {
    aDesiredSize.Height() = CalcHeight(aReflowState);
    if (GetPrevInFlow()) {
      nscoord height = CalcHeightFromUnpaginatedHeight(aPresContext, *this);
      aDesiredSize.Height() = std::max(aDesiredSize.Height(), height);
    }
    else {
      if (isPaginated && HasStyleHeight()) {
        // set the unpaginated height so next in flows can try to honor it
        SetHasUnpaginatedHeight(true);
        SetUnpaginatedHeight(aPresContext, aDesiredSize.Height());
      }
      if (isPaginated && HasUnpaginatedHeight()) {
        aDesiredSize.Height() = std::max(aDesiredSize.Height(), GetUnpaginatedHeight(aPresContext));
      }
    }
  }
  else { // constrained height, paginated
    // Compute the height we should have from style (subtracting the
    // height from our prev-in-flows from the style height)
    nscoord styleHeight = CalcHeightFromUnpaginatedHeight(aPresContext, *this);
    if (styleHeight > aReflowState.AvailableHeight()) {
      styleHeight = aReflowState.AvailableHeight();
      NS_FRAME_SET_INCOMPLETE(aStatus);
    }
    aDesiredSize.Height() = std::max(cellMaxHeight, styleHeight);
  }
  aDesiredSize.UnionOverflowAreasWithDesiredBounds();
  FinishAndStoreOverflow(&aDesiredSize);
  return rv;
}

/** Layout the entire row.
  * This method stacks cells horizontally according to HTML 4.0 rules.
  */
nsresult
nsTableRowFrame::Reflow(nsPresContext*          aPresContext,
                        nsHTMLReflowMetrics&     aDesiredSize,
                        const nsHTMLReflowState& aReflowState,
                        nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsTableRowFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowState, aDesiredSize, aStatus);
  nsresult rv = NS_OK;

  nsTableFrame* tableFrame = nsTableFrame::GetTableFrame(this);
  const nsStyleVisibility* rowVis = StyleVisibility();
  bool collapseRow = (NS_STYLE_VISIBILITY_COLLAPSE == rowVis->mVisible);
  if (collapseRow) {
    tableFrame->SetNeedToCollapse(true);
  }

  // see if a special height reflow needs to occur due to having a pct height
  nsTableFrame::CheckRequestSpecialHeightReflow(aReflowState);

  // See if we have a cell with specified/pct height
  InitHasCellWithStyleHeight(tableFrame);

  rv = ReflowChildren(aPresContext, aDesiredSize, aReflowState, *tableFrame,
                      aStatus);

  if (aPresContext->IsPaginated() && !NS_FRAME_IS_FULLY_COMPLETE(aStatus) &&
      ShouldAvoidBreakInside(aReflowState)) {
    aStatus = NS_INLINE_LINE_BREAK_BEFORE();
  }

  // just set our width to what was available. The table will calculate the width and not use our value.
  aDesiredSize.Width() = aReflowState.AvailableWidth();

  // If our parent is in initial reflow, it'll handle invalidating our
  // entire overflow rect.
  if (!(GetParent()->GetStateBits() & NS_FRAME_FIRST_REFLOW) &&
      nsSize(aDesiredSize.Width(), aDesiredSize.Height()) != mRect.Size()) {
    InvalidateFrame();
  }

  NS_FRAME_SET_TRUNCATION(aStatus, aReflowState, aDesiredSize);
  return rv;
}

/**
 * This function is called by the row group frame's SplitRowGroup() code when
 * pushing a row frame that has cell frames that span into it. The cell frame
 * should be reflowed with the specified height
 */
nscoord 
nsTableRowFrame::ReflowCellFrame(nsPresContext*          aPresContext,
                                 const nsHTMLReflowState& aReflowState,
                                 bool                     aIsTopOfPage,
                                 nsTableCellFrame*        aCellFrame,
                                 nscoord                  aAvailableHeight,
                                 nsReflowStatus&          aStatus)
{
  // Reflow the cell frame with the specified height. Use the existing width
  nsRect cellRect = aCellFrame->GetRect();
  nsRect cellVisualOverflow = aCellFrame->GetVisualOverflowRect();
  
  nsSize availSize(cellRect.width, aAvailableHeight);
  nsTableFrame* tableFrame = nsTableFrame::GetTableFrame(this);
  bool borderCollapse = tableFrame->IsBorderCollapse();
  nsTableCellReflowState cellReflowState(aPresContext, aReflowState,
                                         aCellFrame, availSize,
                                         nsHTMLReflowState::CALLER_WILL_INIT);
  InitChildReflowState(*aPresContext, availSize, borderCollapse, cellReflowState);
  cellReflowState.mFlags.mIsTopOfPage = aIsTopOfPage;

  nsHTMLReflowMetrics desiredSize(aReflowState);

  ReflowChild(aCellFrame, aPresContext, desiredSize, cellReflowState,
              0, 0, NS_FRAME_NO_MOVE_FRAME, aStatus);
  bool fullyComplete = NS_FRAME_IS_COMPLETE(aStatus) && !NS_FRAME_IS_TRUNCATED(aStatus);
  if (fullyComplete) {
    desiredSize.Height() = aAvailableHeight;
  }
  aCellFrame->SetSize(nsSize(cellRect.width, desiredSize.Height()));

  // Note: VerticallyAlignChild can affect the overflow rect.
  // XXX What happens if this cell has 'vertical-align: baseline' ?
  // XXX Why is it assumed that the cell's ascent hasn't changed ?
  if (fullyComplete) {
    aCellFrame->VerticallyAlignChild(mMaxCellAscent);
  }
  
  nsTableFrame::InvalidateTableFrame(aCellFrame, cellRect,
                                     cellVisualOverflow,
                                     (aCellFrame->GetStateBits() &
                                      NS_FRAME_FIRST_REFLOW) != 0);
  
  aCellFrame->DidReflow(aPresContext, nullptr, nsDidReflowStatus::FINISHED);

  return desiredSize.Height();
}

nscoord
nsTableRowFrame::CollapseRowIfNecessary(nscoord aRowOffset,
                                        nscoord aWidth,
                                        bool    aCollapseGroup,
                                        bool& aDidCollapse)
{
  const nsStyleVisibility* rowVis = StyleVisibility();
  bool collapseRow = (NS_STYLE_VISIBILITY_COLLAPSE == rowVis->mVisible);
  nsTableFrame* tableFrame = static_cast<nsTableFrame*>(
    nsTableFrame::GetTableFrame(this)->FirstInFlow());
  if (collapseRow) {
    tableFrame->SetNeedToCollapse(true);
  }

  if (aRowOffset != 0) {
    // We're moving, so invalidate our old position
    InvalidateFrameSubtree();
  }
  
  nsRect rowRect = GetRect();
  nsRect oldRect = rowRect;
  nsRect oldVisualOverflow = GetVisualOverflowRect();
  
  rowRect.y -= aRowOffset;
  rowRect.width  = aWidth;
  nsOverflowAreas overflow;
  nscoord shift = 0;
  nscoord cellSpacingX = tableFrame->GetCellSpacingX();
  nscoord cellSpacingY = tableFrame->GetCellSpacingY();

  if (aCollapseGroup || collapseRow) {
    nsTableCellFrame* cellFrame = GetFirstCell();
    aDidCollapse = true;
    shift = rowRect.height + cellSpacingY;
    while (cellFrame) {
      nsRect cRect = cellFrame->GetRect();
      // If aRowOffset != 0, there's no point in invalidating the cells, since
      // we've already invalidated our overflow area.  Note that we _do_ still
      // need to invalidate if our row is not moving, because the cell might
      // span out of this row, so invalidating our row rect won't do enough.
      if (aRowOffset == 0) {
        InvalidateFrame();
      }
      cRect.height = 0;
      cellFrame->SetRect(cRect);
      cellFrame = cellFrame->GetNextCell();
    }
    rowRect.height = 0;
  }
  else { // row is not collapsed
    nsTableIterator iter(*this);
    // remember the col index of the previous cell to handle rowspans into this
    // row
    int32_t firstPrevColIndex = (iter.IsLeftToRight()) ? -1 :
                                tableFrame->GetColCount();
    int32_t prevColIndex  = firstPrevColIndex;
    nscoord x = 0; // running total of children x offset

    int32_t colIncrement = iter.IsLeftToRight() ? 1 : -1;

    //nscoord x = cellSpacingX;

    nsIFrame* kidFrame = iter.First();
    while (kidFrame) {
      nsTableCellFrame *cellFrame = do_QueryFrame(kidFrame);
      if (cellFrame) {
        int32_t cellColIndex;
        cellFrame->GetColIndex(cellColIndex);
        int32_t cellColSpan = tableFrame->GetEffectiveColSpan(*cellFrame);

        // If the adjacent cell is in a prior row (because of a rowspan) add in
        // the space
        if ((iter.IsLeftToRight() && (prevColIndex != (cellColIndex - 1))) ||
            (!iter.IsLeftToRight() &&
             (prevColIndex != cellColIndex + cellColSpan))) {
          x += GetSpaceBetween(prevColIndex, cellColIndex, cellColSpan,
                               *tableFrame, cellSpacingX, iter.IsLeftToRight(),
                               true);
        }
        nsRect cRect(x, 0, 0, rowRect.height);

        // remember the rightmost (ltr) or leftmost (rtl) column this cell
        // spans into
        prevColIndex = (iter.IsLeftToRight()) ?
                       cellColIndex + (cellColSpan - 1) : cellColIndex;
        int32_t startIndex = (iter.IsLeftToRight()) ?
                             cellColIndex : cellColIndex + (cellColSpan - 1);
        int32_t actualColSpan = cellColSpan;
        bool isVisible = false;
        for (int32_t colX = startIndex; actualColSpan > 0;
             colX += colIncrement, actualColSpan--) {

          nsTableColFrame* colFrame = tableFrame->GetColFrame(colX);
          const nsStyleVisibility* colVis = colFrame->StyleVisibility();
          bool collapseCol = (NS_STYLE_VISIBILITY_COLLAPSE ==
                                colVis->mVisible);
          nsIFrame* cgFrame = colFrame->GetParent();
          const nsStyleVisibility* groupVis = cgFrame->StyleVisibility();
          bool collapseGroup = (NS_STYLE_VISIBILITY_COLLAPSE ==
                                  groupVis->mVisible);
          bool isCollapsed = collapseCol || collapseGroup;
          if (!isCollapsed) {
            cRect.width += tableFrame->GetColumnWidth(colX);
            isVisible = true;
            if ((actualColSpan > 1)) {
              nsTableColFrame* nextColFrame =
                tableFrame->GetColFrame(colX + colIncrement);
              const nsStyleVisibility* nextColVis =
              nextColFrame->StyleVisibility();
              if ( (NS_STYLE_VISIBILITY_COLLAPSE != nextColVis->mVisible) &&
                  tableFrame->ColumnHasCellSpacingBefore(colX + colIncrement)) {
                cRect.width += cellSpacingX;
              }
            }
          }
        }
        x += cRect.width;
        if (isVisible)
          x += cellSpacingX;
        int32_t actualRowSpan = tableFrame->GetEffectiveRowSpan(*cellFrame);
        nsTableRowFrame* rowFrame = GetNextRow();
        for (actualRowSpan--; actualRowSpan > 0 && rowFrame; actualRowSpan--) {
          const nsStyleVisibility* nextRowVis = rowFrame->StyleVisibility();
          bool collapseNextRow = (NS_STYLE_VISIBILITY_COLLAPSE ==
                                    nextRowVis->mVisible);
          if (!collapseNextRow) {
            nsRect nextRect = rowFrame->GetRect();
            cRect.height += nextRect.height + cellSpacingY;
          }
          rowFrame = rowFrame->GetNextRow();
        }

        nsRect oldCellRect = cellFrame->GetRect();
        nsRect oldCellVisualOverflow = cellFrame->GetVisualOverflowRect();

        if (aRowOffset == 0 && cRect.TopLeft() != oldCellRect.TopLeft()) {
          // We're moving the cell.  Invalidate the old overflow area
          cellFrame->InvalidateFrameSubtree();
        }
        
        cellFrame->SetRect(cRect);

        // XXXbz This looks completely bogus in the cases when we didn't
        // collapse the cell!
        nsRect cellBounds(0, 0, cRect.width, cRect.height);
        nsOverflowAreas cellOverflow(cellBounds, cellBounds);
        cellFrame->FinishAndStoreOverflow(cellOverflow, cRect.Size());
        nsTableFrame::RePositionViews(cellFrame);
        ConsiderChildOverflow(overflow, cellFrame);
                
        if (aRowOffset == 0) {
          nsTableFrame::InvalidateTableFrame(cellFrame, oldCellRect,
                                             oldCellVisualOverflow,
                                             false);
        }
      }
      kidFrame = iter.Next(); // Get the next child
    }
  }

  SetRect(rowRect);
  overflow.UnionAllWith(nsRect(0, 0, rowRect.width, rowRect.height));
  FinishAndStoreOverflow(overflow, rowRect.Size());

  nsTableFrame::RePositionViews(this);
  nsTableFrame::InvalidateTableFrame(this, oldRect, oldVisualOverflow, false);
  return shift;
}

/*
 * The following method is called by the row group frame's SplitRowGroup()
 * when it creates a continuing cell frame and wants to insert it into the
 * row's child list.
 */
void 
nsTableRowFrame::InsertCellFrame(nsTableCellFrame* aFrame,
                                 int32_t           aColIndex)
{
  // Find the cell frame where col index < aColIndex
  nsTableCellFrame* priorCell = nullptr;
  for (nsIFrame* child = mFrames.FirstChild(); child;
       child = child->GetNextSibling()) {
    nsTableCellFrame *cellFrame = do_QueryFrame(child);
    if (cellFrame) {
      int32_t colIndex;
      cellFrame->GetColIndex(colIndex);
      if (colIndex < aColIndex) {
        priorCell = cellFrame;
      }
      else break;
    }
  }
  mFrames.InsertFrame(this, priorCell, aFrame);
}

nsIAtom*
nsTableRowFrame::GetType() const
{
  return nsGkAtoms::tableRowFrame;
}

nsTableRowFrame*  
nsTableRowFrame::GetNextRow() const
{
  nsIFrame* childFrame = GetNextSibling();
  while (childFrame) {
    nsTableRowFrame *rowFrame = do_QueryFrame(childFrame);
    if (rowFrame) {
	  NS_ASSERTION(NS_STYLE_DISPLAY_TABLE_ROW == childFrame->StyleDisplay()->mDisplay, "wrong display type on rowframe");
      return rowFrame;
    }
    childFrame = childFrame->GetNextSibling();
  }
  return nullptr;
}

NS_DECLARE_FRAME_PROPERTY(RowUnpaginatedHeightProperty, nullptr)

void 
nsTableRowFrame::SetUnpaginatedHeight(nsPresContext* aPresContext,
                                      nscoord        aValue)
{
  NS_ASSERTION(!GetPrevInFlow(), "program error");
  // Get the property
  aPresContext->PropertyTable()->
    Set(this, RowUnpaginatedHeightProperty(), NS_INT32_TO_PTR(aValue));
}

nscoord
nsTableRowFrame::GetUnpaginatedHeight(nsPresContext* aPresContext)
{
  FrameProperties props = FirstInFlow()->Properties();
  return NS_PTR_TO_INT32(props.Get(RowUnpaginatedHeightProperty()));
}

void nsTableRowFrame::SetContinuousBCBorderWidth(uint8_t     aForSide,
                                                 BCPixelSize aPixelValue)
{
  switch (aForSide) {
    case NS_SIDE_RIGHT:
      mRightContBorderWidth = aPixelValue;
      return;
    case NS_SIDE_TOP:
      mTopContBorderWidth = aPixelValue;
      return;
    case NS_SIDE_LEFT:
      mLeftContBorderWidth = aPixelValue;
      return;
    default:
      NS_ERROR("invalid NS_SIDE arg");
  }
}
#ifdef ACCESSIBILITY
a11y::AccType
nsTableRowFrame::AccessibleType()
{
  return a11y::eHTMLTableRowType;
}
#endif
/**
 * Sets the NS_ROW_HAS_CELL_WITH_STYLE_HEIGHT bit to indicate whether
 * this row has any cells that have non-auto-height.  (Row-spanning
 * cells are ignored.)
 */
void nsTableRowFrame::InitHasCellWithStyleHeight(nsTableFrame* aTableFrame)
{
  nsTableIterator iter(*this);

  for (nsIFrame* kidFrame = iter.First(); kidFrame; kidFrame = iter.Next()) {
    nsTableCellFrame *cellFrame = do_QueryFrame(kidFrame);
    if (!cellFrame) {
      NS_NOTREACHED("Table row has a non-cell child.");
      continue;
    }
    // Ignore row-spanning cells
    const nsStyleCoord &cellHeight = cellFrame->StylePosition()->mHeight;
    if (aTableFrame->GetEffectiveRowSpan(*cellFrame) == 1 &&
        cellHeight.GetUnit() != eStyleUnit_Auto &&
         /* calc() with percentages treated like 'auto' */
        (!cellHeight.IsCalcUnit() || !cellHeight.HasPercent())) {
      AddStateBits(NS_ROW_HAS_CELL_WITH_STYLE_HEIGHT);
      return;
    }
  }
  RemoveStateBits(NS_ROW_HAS_CELL_WITH_STYLE_HEIGHT);
}
  
void 
nsTableRowFrame::InvalidateFrame(uint32_t aDisplayItemKey)
{
  nsIFrame::InvalidateFrame(aDisplayItemKey);
  GetParent()->InvalidateFrameWithRect(GetVisualOverflowRect() + GetPosition(), aDisplayItemKey);
}

void
nsTableRowFrame::InvalidateFrameWithRect(const nsRect& aRect, uint32_t aDisplayItemKey)
{
  nsIFrame::InvalidateFrameWithRect(aRect, aDisplayItemKey);
  // If we have filters applied that would affects our bounds, then
  // we get an inactive layer created and this is computed
  // within FrameLayerBuilder
  GetParent()->InvalidateFrameWithRect(aRect + GetPosition(), aDisplayItemKey);
}

/* ----- global methods ----- */

nsIFrame* 
NS_NewTableRowFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsTableRowFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsTableRowFrame)

#ifdef DEBUG_FRAME_DUMP
nsresult
nsTableRowFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("TableRow"), aResult);
}
#endif
