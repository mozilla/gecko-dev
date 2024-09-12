/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsMathMLmrootFrame.h"

#include "mozilla/PresShell.h"
#include "nsLayoutUtils.h"
#include "nsPresContext.h"
#include <algorithm>
#include "gfxContext.h"
#include "gfxMathTable.h"

using namespace mozilla;

//
// <mroot> -- form a radical - implementation
//

static const char16_t kSqrChar = char16_t(0x221A);

nsIFrame* NS_NewMathMLmrootFrame(PresShell* aPresShell, ComputedStyle* aStyle) {
  return new (aPresShell)
      nsMathMLmrootFrame(aStyle, aPresShell->GetPresContext());
}

NS_IMPL_FRAMEARENA_HELPERS(nsMathMLmrootFrame)

nsMathMLmrootFrame::nsMathMLmrootFrame(ComputedStyle* aStyle,
                                       nsPresContext* aPresContext)
    : nsMathMLContainerFrame(aStyle, aPresContext, kClassID) {}

nsMathMLmrootFrame::~nsMathMLmrootFrame() = default;

void nsMathMLmrootFrame::Init(nsIContent* aContent, nsContainerFrame* aParent,
                              nsIFrame* aPrevInFlow) {
  nsMathMLContainerFrame::Init(aContent, aParent, aPrevInFlow);

  nsAutoString sqrChar;
  sqrChar.Assign(kSqrChar);
  mSqrChar.SetData(sqrChar);
  mSqrChar.SetComputedStyle(Style());
}

bool nsMathMLmrootFrame::ShouldUseRowFallback() {
  nsIFrame* baseFrame = mFrames.FirstChild();
  if (!baseFrame) {
    return true;
  }
  nsIFrame* indexFrame = baseFrame->GetNextSibling();
  return !indexFrame || indexFrame->GetNextSibling();
}

NS_IMETHODIMP
nsMathMLmrootFrame::TransmitAutomaticData() {
  // 1. The REC says:
  //    The <mroot> element increments scriptlevel by 2, and sets displaystyle
  //    to "false", within index, but leaves both attributes unchanged within
  //    base.
  // 2. The TeXbook (Ch 17. p.141) says \sqrt is compressed
  UpdatePresentationDataFromChildAt(1, 1, NS_MATHML_COMPRESSED,
                                    NS_MATHML_COMPRESSED);
  UpdatePresentationDataFromChildAt(0, 0, NS_MATHML_COMPRESSED,
                                    NS_MATHML_COMPRESSED);

  PropagateFrameFlagFor(mFrames.LastChild(), NS_FRAME_MATHML_SCRIPT_DESCENDANT);

  return NS_OK;
}

void nsMathMLmrootFrame::BuildDisplayList(nsDisplayListBuilder* aBuilder,
                                          const nsDisplayListSet& aLists) {
  /////////////
  // paint the content we are square-rooting
  nsMathMLContainerFrame::BuildDisplayList(aBuilder, aLists);

  if (ShouldUseRowFallback()) return;

  /////////////
  // paint the sqrt symbol
  mSqrChar.Display(aBuilder, this, aLists, 0);

  DisplayBar(aBuilder, this, mBarRect, aLists);

#if defined(DEBUG) && defined(SHOW_BOUNDING_BOX)
  // for visual debug
  nsRect rect;
  mSqrChar.GetRect(rect);
  nsBoundingMetrics bm;
  mSqrChar.GetBoundingMetrics(bm);
  DisplayBoundingMetrics(aBuilder, this, rect.TopLeft(), bm, aLists);
#endif
}

void nsMathMLmrootFrame::GetRadicalXOffsets(nscoord aIndexWidth,
                                            nscoord aSqrWidth,
                                            nsFontMetrics* aFontMetrics,
                                            nscoord* aIndexOffset,
                                            nscoord* aSqrOffset) {
  // The index is tucked in closer to the radical while making sure
  // that the kern does not make the index and radical collide
  nscoord dxIndex, dxSqr, radicalKernBeforeDegree, radicalKernAfterDegree;
  nscoord oneDevPixel = aFontMetrics->AppUnitsPerDevPixel();
  RefPtr<gfxFont> mathFont =
      aFontMetrics->GetThebesFontGroup()->GetFirstMathFont();

  if (mathFont) {
    radicalKernBeforeDegree = mathFont->MathTable()->Constant(
        gfxMathTable::RadicalKernBeforeDegree, oneDevPixel);
    radicalKernAfterDegree = mathFont->MathTable()->Constant(
        gfxMathTable::RadicalKernAfterDegree, oneDevPixel);
  } else {
    nscoord em;
    GetEmHeight(aFontMetrics, em);
    radicalKernBeforeDegree = NSToCoordRound(5.0f * em / 18);
    radicalKernAfterDegree = NSToCoordRound(-10.0f * em / 18);
  }

  // Clamp radical kern degrees according to spec:
  // https://w3c.github.io/mathml-core/#root-with-index
  radicalKernBeforeDegree = std::max(0, radicalKernBeforeDegree);
  radicalKernAfterDegree = std::max(-aIndexWidth, radicalKernAfterDegree);

  dxIndex = radicalKernBeforeDegree;
  dxSqr = radicalKernBeforeDegree + aIndexWidth + radicalKernAfterDegree;
  if (aIndexOffset) *aIndexOffset = dxIndex;
  if (aSqrOffset) *aSqrOffset = dxSqr;
}

void nsMathMLmrootFrame::Reflow(nsPresContext* aPresContext,
                                ReflowOutput& aDesiredSize,
                                const ReflowInput& aReflowInput,
                                nsReflowStatus& aStatus) {
  if (ShouldUseRowFallback()) {
    ReportChildCountError();
    nsMathMLContainerFrame::Reflow(aPresContext, aDesiredSize, aReflowInput,
                                   aStatus);
    return;
  }

  MarkInReflow();
  MOZ_ASSERT(aStatus.IsEmpty(), "Caller should pass a fresh reflow status!");

  nsReflowStatus childStatus;
  aDesiredSize.ClearSize();
  aDesiredSize.SetBlockStartAscent(0);

  nsBoundingMetrics bmSqr, bmBase, bmIndex;
  nsMargin baseMargin, indexMargin;
  DrawTarget* drawTarget = aReflowInput.mRenderingContext->GetDrawTarget();

  //////////////////
  // Reflow Children

  int32_t count = 0;
  nsIFrame* baseFrame = nullptr;
  nsIFrame* indexFrame = nullptr;
  ReflowOutput baseSize(aReflowInput);
  ReflowOutput indexSize(aReflowInput);
  nsIFrame* childFrame = mFrames.FirstChild();
  while (childFrame) {
    // ask our children to compute their bounding metrics
    ReflowOutput childDesiredSize(aReflowInput);
    WritingMode wm = childFrame->GetWritingMode();
    LogicalSize availSize = aReflowInput.ComputedSize(wm);
    availSize.BSize(wm) = NS_UNCONSTRAINEDSIZE;
    ReflowInput childReflowInput(aPresContext, aReflowInput, childFrame,
                                 availSize);
    ReflowChild(childFrame, aPresContext, childDesiredSize, childReflowInput,
                childStatus);
    // NS_ASSERTION(childStatus.IsComplete(), "bad status");
    if (0 == count) {
      // base
      baseFrame = childFrame;
      baseSize = childDesiredSize;
      bmBase = childDesiredSize.mBoundingMetrics;
      baseMargin = baseFrame->GetUsedMargin();
    } else if (1 == count) {
      // index
      indexFrame = childFrame;
      indexSize = childDesiredSize;
      bmIndex = childDesiredSize.mBoundingMetrics;
      indexMargin = indexFrame->GetUsedMargin();
    }
    count++;
    childFrame = childFrame->GetNextSibling();
  }

  ////////////
  // Prepare the radical symbol and the overline bar

  float fontSizeInflation = nsLayoutUtils::FontSizeInflationFor(this);
  RefPtr<nsFontMetrics> fm =
      nsLayoutUtils::GetFontMetricsForFrame(this, fontSizeInflation);

  nscoord ruleThickness, leading, psi;
  GetRadicalParameters(fm, StyleFont()->mMathStyle == StyleMathStyle::Normal,
                       ruleThickness, leading, psi);

  // built-in: adjust clearance psi to emulate \mathstrut using '1' (TexBook,
  // p.131)
  char16_t one = '1';
  nsBoundingMetrics bmOne =
      nsLayoutUtils::AppUnitBoundsOfString(&one, 1, *fm, drawTarget);
  if (bmOne.ascent > bmBase.ascent + baseMargin.top) {
    psi += bmOne.ascent - bmBase.ascent - baseMargin.top;
  }

  // make sure that the rule appears on on screen
  nscoord onePixel = nsPresContext::CSSPixelsToAppUnits(1);
  if (ruleThickness < onePixel) {
    ruleThickness = onePixel;
  }

  // adjust clearance psi to get an exact number of pixels -- this
  // gives a nicer & uniform look on stacked radicals (bug 130282)
  nscoord delta = psi % onePixel;
  if (delta) psi += onePixel - delta;  // round up

  // Stretch the radical symbol to the appropriate height if it is not big
  // enough.
  nsBoundingMetrics contSize = bmBase;
  contSize.descent =
      bmBase.ascent + bmBase.descent + baseMargin.TopBottom() + psi;
  contSize.ascent = ruleThickness;

  // height(radical) should be >= height(base) + psi + ruleThickness
  nsBoundingMetrics radicalSize;
  mSqrChar.Stretch(this, drawTarget, fontSizeInflation,
                   NS_STRETCH_DIRECTION_VERTICAL, contSize, radicalSize,
                   NS_STRETCH_LARGER,
                   StyleVisibility()->mDirection == StyleDirection::Rtl);
  // radicalSize have changed at this point, and should match with
  // the bounding metrics of the char
  mSqrChar.GetBoundingMetrics(bmSqr);

  // Update the desired size for the container (like msqrt, index is not yet
  // included) the baseline will be that of the base.
  mBoundingMetrics.ascent =
      bmBase.ascent + baseMargin.top + psi + ruleThickness;
  mBoundingMetrics.descent =
      std::max(bmBase.descent + baseMargin.bottom,
               (bmSqr.ascent + bmSqr.descent - mBoundingMetrics.ascent));
  mBoundingMetrics.width = bmSqr.width + bmBase.width + baseMargin.LeftRight();
  mBoundingMetrics.leftBearing = bmSqr.leftBearing;
  mBoundingMetrics.rightBearing =
      bmSqr.width +
      std::max(
          bmBase.width + baseMargin.LeftRight(),
          bmBase.rightBearing + baseMargin.left);  // take also care of the rule

  aDesiredSize.SetBlockStartAscent(mBoundingMetrics.ascent + leading);
  aDesiredSize.Height() =
      aDesiredSize.BlockStartAscent() +
      std::max(baseSize.Height() - baseSize.BlockStartAscent(),
               mBoundingMetrics.descent + ruleThickness);
  aDesiredSize.Width() = mBoundingMetrics.width;

  /////////////
  // Re-adjust the desired size to include the index.

  // the index is raised by some fraction of the height
  // of the radical, see \mroot macro in App. B, TexBook
  float raiseIndexPercent = 0.6f;
  RefPtr<gfxFont> mathFont = fm->GetThebesFontGroup()->GetFirstMathFont();
  if (mathFont) {
    raiseIndexPercent = mathFont->MathTable()->Constant(
        gfxMathTable::RadicalDegreeBottomRaisePercent);
  }
  nscoord raiseIndexDelta =
      NSToCoordRound(raiseIndexPercent * (bmSqr.ascent + bmSqr.descent));
  nscoord indexRaisedAscent =
      mBoundingMetrics.ascent           // top of radical
      - (bmSqr.ascent + bmSqr.descent)  // to bottom of radical
      + raiseIndexDelta + bmIndex.ascent + bmIndex.descent +
      indexMargin.TopBottom();  // to top of raised index

  nscoord indexClearance = 0;
  if (mBoundingMetrics.ascent < indexRaisedAscent) {
    indexClearance =
        indexRaisedAscent -
        mBoundingMetrics.ascent;  // excess gap introduced by a tall index
    mBoundingMetrics.ascent = indexRaisedAscent;
    nscoord descent = aDesiredSize.Height() - aDesiredSize.BlockStartAscent();
    aDesiredSize.SetBlockStartAscent(mBoundingMetrics.ascent + leading);
    aDesiredSize.Height() = aDesiredSize.BlockStartAscent() + descent;
  }

  nscoord dxIndex, dxSqr;
  GetRadicalXOffsets(bmIndex.width + indexMargin.LeftRight(), bmSqr.width, fm,
                     &dxIndex, &dxSqr);

  mBoundingMetrics.width =
      dxSqr + bmSqr.width + bmBase.width + baseMargin.LeftRight();
  mBoundingMetrics.leftBearing =
      std::min(dxIndex + bmIndex.leftBearing, dxSqr + bmSqr.leftBearing);
  mBoundingMetrics.rightBearing =
      dxSqr + bmSqr.width +
      std::max(bmBase.width + baseMargin.LeftRight(),
               bmBase.rightBearing + baseMargin.left);

  aDesiredSize.Width() = mBoundingMetrics.width;
  aDesiredSize.mBoundingMetrics = mBoundingMetrics;

  // Add padding+border around the final layout.
  auto borderPadding = GetUsedBorderAndPadding();
  InflateReflowAndBoundingMetrics(borderPadding, aDesiredSize,
                                  mBoundingMetrics);

  GatherAndStoreOverflow(&aDesiredSize);

  // place the index
  const bool isRTL = StyleVisibility()->mDirection == StyleDirection::Rtl;
  nscoord borderPaddingInlineStart =
      isRTL ? borderPadding.right : borderPadding.left;
  nscoord dx = borderPaddingInlineStart + dxIndex +
               indexMargin.Side(isRTL ? eSideRight : eSideLeft);
  nscoord dy =
      aDesiredSize.BlockStartAscent() -
      (indexRaisedAscent + indexSize.BlockStartAscent() - bmIndex.ascent);
  FinishReflowChild(indexFrame, aPresContext, indexSize, nullptr,
                    MirrorIfRTL(aDesiredSize.Width(), indexSize.Width(), dx),
                    dy + indexMargin.top, ReflowChildFlags::Default);

  // place the radical symbol and the radical bar
  dx = borderPaddingInlineStart + dxSqr;
  dy = borderPadding.top + indexClearance +
       leading;  // leave a leading at the top
  mSqrChar.SetRect(nsRect(MirrorIfRTL(aDesiredSize.Width(), bmSqr.width, dx),
                          dy, bmSqr.width, bmSqr.ascent + bmSqr.descent));
  dx += bmSqr.width;
  mBarRect.SetRect(MirrorIfRTL(aDesiredSize.Width(),
                               bmBase.width + baseMargin.LeftRight(), dx),
                   dy, bmBase.width + baseMargin.LeftRight(), ruleThickness);

  // place the base
  dx += isRTL ? baseMargin.right : baseMargin.left;
  dy = aDesiredSize.BlockStartAscent() - baseSize.BlockStartAscent();
  FinishReflowChild(baseFrame, aPresContext, baseSize, nullptr,
                    MirrorIfRTL(aDesiredSize.Width(), baseSize.Width(), dx), dy,
                    ReflowChildFlags::Default);

  mReference.x = 0;
  mReference.y = aDesiredSize.BlockStartAscent();
}

/* virtual */
nsresult nsMathMLmrootFrame::MeasureForWidth(DrawTarget* aDrawTarget,
                                             ReflowOutput& aDesiredSize) {
  const PlaceFlags flags(PlaceFlag::IntrinsicSize, PlaceFlag::MeasureOnly);
  if (ShouldUseRowFallback()) {
    return PlaceAsMrow(aDrawTarget, flags, aDesiredSize);
  }

  // ShouldUseRowFallback() returned false so there are exactly two children.
  nsIFrame* baseFrame = mFrames.FirstChild();
  MOZ_ASSERT(baseFrame);
  nsIFrame* indexFrame = baseFrame->GetNextSibling();
  MOZ_ASSERT(indexFrame);
  MOZ_ASSERT(!indexFrame->GetNextSibling());

  nsBoundingMetrics bmBase, bmIndex;
  ReflowOutput baseSize(aDesiredSize.GetWritingMode());
  ReflowOutput indexSize(aDesiredSize.GetWritingMode());
  float fontSizeInflation = nsLayoutUtils::FontSizeInflationFor(this);
  GetReflowAndBoundingMetricsFor(baseFrame, baseSize, bmBase);
  GetReflowAndBoundingMetricsFor(indexFrame, indexSize, bmIndex);
  nscoord baseMargin = baseFrame->IntrinsicISizeOffsets().margin,
          indexMargin = indexFrame->IntrinsicISizeOffsets().margin;
  nscoord sqrWidth = mSqrChar.GetMaxWidth(this, aDrawTarget, fontSizeInflation);

  nscoord dxIndex, dxSqr;
  RefPtr<nsFontMetrics> fm =
      nsLayoutUtils::GetFontMetricsForFrame(this, fontSizeInflation);
  GetRadicalXOffsets(bmIndex.width + indexMargin, sqrWidth, fm, &dxIndex,
                     &dxSqr);

  mBoundingMetrics.width = dxSqr + sqrWidth + bmBase.width + baseMargin;
  mBoundingMetrics.leftBearing =
      std::min(dxIndex + bmIndex.leftBearing + indexMargin, dxSqr);
  mBoundingMetrics.rightBearing =
      dxSqr + sqrWidth +
      std::max(bmBase.width + baseMargin, bmBase.rightBearing + baseMargin);

  aDesiredSize.Width() = mBoundingMetrics.width;
  aDesiredSize.mBoundingMetrics = mBoundingMetrics;

  // Add border+padding.
  nsMargin borderPadding = GetBorderPaddingForPlace(flags);
  InflateReflowAndBoundingMetrics(borderPadding, aDesiredSize,
                                  mBoundingMetrics);

  return NS_OK;
}

void nsMathMLmrootFrame::DidSetComputedStyle(ComputedStyle* aOldStyle) {
  nsMathMLContainerFrame::DidSetComputedStyle(aOldStyle);
  mSqrChar.SetComputedStyle(Style());
}
