/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#include "nsMathMLmfracFrame.h"
#include "nsPresContext.h"
#include "nsRenderingContext.h"
#include "nsDisplayList.h"
#include "gfxContext.h"
#include "nsMathMLElement.h"
#include <algorithm>

//
// <mfrac> -- form a fraction from two subexpressions - implementation
//

// various fraction line thicknesses (multiplicative values of the default rule thickness)

#define THIN_FRACTION_LINE                   0.5f
#define THIN_FRACTION_LINE_MINIMUM_PIXELS    1  // minimum of 1 pixel

#define THICK_FRACTION_LINE                  2.0f
#define THICK_FRACTION_LINE_MINIMUM_PIXELS   2  // minimum of 2 pixels

nsIFrame*
NS_NewMathMLmfracFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsMathMLmfracFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsMathMLmfracFrame)

nsMathMLmfracFrame::~nsMathMLmfracFrame()
{
}

eMathMLFrameType
nsMathMLmfracFrame::GetMathMLFrameType()
{
  // frac is "inner" in TeXBook, Appendix G, rule 15e. See also page 170.
  return eMathMLFrameType_Inner;
}

uint8_t
nsMathMLmfracFrame::ScriptIncrement(nsIFrame* aFrame)
{
  if (!StyleFont()->mMathDisplay &&
      aFrame && (mFrames.FirstChild() == aFrame ||
                 mFrames.LastChild() == aFrame)) {
    return 1;
  }
  return 0;
}

NS_IMETHODIMP
nsMathMLmfracFrame::TransmitAutomaticData()
{
  // The TeXbook (Ch 17. p.141) says the numerator inherits the compression
  //  while the denominator is compressed
  UpdatePresentationDataFromChildAt(1,  1,
     NS_MATHML_COMPRESSED,
     NS_MATHML_COMPRESSED);

  // If displaystyle is false, then scriptlevel is incremented, so notify the
  // children of this.
  if (!StyleFont()->mMathDisplay) {
    PropagateFrameFlagFor(mFrames.FirstChild(),
                          NS_FRAME_MATHML_SCRIPT_DESCENDANT);
    PropagateFrameFlagFor(mFrames.LastChild(),
                          NS_FRAME_MATHML_SCRIPT_DESCENDANT);
  }

  // if our numerator is an embellished operator, let its state bubble to us
  GetEmbellishDataFrom(mFrames.FirstChild(), mEmbellishData);
  if (NS_MATHML_IS_EMBELLISH_OPERATOR(mEmbellishData.flags)) {
    // even when embellished, we need to record that <mfrac> won't fire
    // Stretch() on its embellished child
    mEmbellishData.direction = NS_STRETCH_DIRECTION_UNSUPPORTED;
  }

  return NS_OK;
}

nscoord 
nsMathMLmfracFrame::CalcLineThickness(nsPresContext*  aPresContext,
                                      nsStyleContext*  aStyleContext,
                                      nsString&        aThicknessAttribute,
                                      nscoord          onePixel,
                                      nscoord          aDefaultRuleThickness)
{
  nscoord defaultThickness = aDefaultRuleThickness;
  nscoord lineThickness = aDefaultRuleThickness;
  nscoord minimumThickness = onePixel;

  // linethickness
  //
  // "Specifies the thickness of the horizontal 'fraction bar', or 'rule'. The
  // default value is 'medium', 'thin' is thinner, but visible, 'thick' is
  // thicker; the exact thickness of these is left up to the rendering agent."
  //
  // values: length | "thin" | "medium" | "thick"
  // default: medium
  //
  if (!aThicknessAttribute.IsEmpty()) {
    if (aThicknessAttribute.EqualsLiteral("thin")) {
      lineThickness = NSToCoordFloor(defaultThickness * THIN_FRACTION_LINE);
      minimumThickness = onePixel * THIN_FRACTION_LINE_MINIMUM_PIXELS;
      // should visually decrease by at least one pixel, if default is not a pixel
      if (defaultThickness > onePixel && lineThickness > defaultThickness - onePixel)
        lineThickness = defaultThickness - onePixel;
    }
    else if (aThicknessAttribute.EqualsLiteral("medium")) {
      // medium is default
    }
    else if (aThicknessAttribute.EqualsLiteral("thick")) {
      lineThickness = NSToCoordCeil(defaultThickness * THICK_FRACTION_LINE);
      minimumThickness = onePixel * THICK_FRACTION_LINE_MINIMUM_PIXELS;
      // should visually increase by at least one pixel
      if (lineThickness < defaultThickness + onePixel)
        lineThickness = defaultThickness + onePixel;
    }
    else {
      // length value
      lineThickness = defaultThickness;
      ParseNumericValue(aThicknessAttribute, &lineThickness,
                        nsMathMLElement::PARSE_ALLOW_UNITLESS,
                        aPresContext, aStyleContext);
    }
  }

  // use minimum if the lineThickness is a non-zero value less than minimun
  if (lineThickness && lineThickness < minimumThickness) 
    lineThickness = minimumThickness;

  return lineThickness;
}

void
nsMathMLmfracFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                     const nsRect&           aDirtyRect,
                                     const nsDisplayListSet& aLists)
{
  /////////////
  // paint the numerator and denominator
  nsMathMLContainerFrame::BuildDisplayList(aBuilder, aDirtyRect, aLists);
  
  /////////////
  // paint the fraction line
  if (mIsBevelled) {
    DisplaySlash(aBuilder, this, mLineRect, mLineThickness, aLists);
  } else {
    DisplayBar(aBuilder, this, mLineRect, aLists);
  }
}

/* virtual */ nsresult
nsMathMLmfracFrame::MeasureForWidth(nsRenderingContext& aRenderingContext,
                                    nsHTMLReflowMetrics& aDesiredSize)
{
  return PlaceInternal(aRenderingContext,
                       false,
                       aDesiredSize,
                       true);
}

nscoord
nsMathMLmfracFrame::FixInterFrameSpacing(nsHTMLReflowMetrics& aDesiredSize)
{
  nscoord gap = nsMathMLContainerFrame::FixInterFrameSpacing(aDesiredSize);
  if (!gap) return 0;

  mLineRect.MoveBy(gap, 0);
  return gap;
}

/* virtual */ nsresult
nsMathMLmfracFrame::Place(nsRenderingContext& aRenderingContext,
                          bool                 aPlaceOrigin,
                          nsHTMLReflowMetrics& aDesiredSize)
{
  return PlaceInternal(aRenderingContext,
                       aPlaceOrigin,
                       aDesiredSize,
                       false);
}

nsresult
nsMathMLmfracFrame::PlaceInternal(nsRenderingContext& aRenderingContext,
                                  bool                 aPlaceOrigin,
                                  nsHTMLReflowMetrics& aDesiredSize,
                                  bool                 aWidthOnly)
{
  ////////////////////////////////////
  // Get the children's desired sizes
  nsBoundingMetrics bmNum, bmDen;
  nsHTMLReflowMetrics sizeNum(aDesiredSize.GetWritingMode());
  nsHTMLReflowMetrics sizeDen(aDesiredSize.GetWritingMode());
  nsIFrame* frameDen = nullptr;
  nsIFrame* frameNum = mFrames.FirstChild();
  if (frameNum) 
    frameDen = frameNum->GetNextSibling();
  if (!frameNum || !frameDen || frameDen->GetNextSibling()) {
    // report an error, encourage people to get their markups in order
    if (aPlaceOrigin) {
      ReportChildCountError();
    }
    return ReflowError(aRenderingContext, aDesiredSize);
  }
  GetReflowAndBoundingMetricsFor(frameNum, sizeNum, bmNum);
  GetReflowAndBoundingMetricsFor(frameDen, sizeDen, bmDen);

  nsPresContext* presContext = PresContext();
  nscoord onePixel = nsPresContext::CSSPixelsToAppUnits(1);

  nsRefPtr<nsFontMetrics> fm;
  nsLayoutUtils::GetFontMetricsForFrame(this, getter_AddRefs(fm));
  aRenderingContext.SetFont(fm);

  nscoord defaultRuleThickness, axisHeight;
  GetRuleThickness(aRenderingContext, fm, defaultRuleThickness);
  GetAxisHeight(aRenderingContext, fm, axisHeight);

  nsEmbellishData coreData;
  GetEmbellishDataFrom(mEmbellishData.coreFrame, coreData);

  // see if the linethickness attribute is there 
  nsAutoString value;
  mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::linethickness_, value);
  mLineThickness = CalcLineThickness(presContext, mStyleContext, value,
                                     onePixel, defaultRuleThickness);

  // bevelled attribute
  mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::bevelled_, value);
  mIsBevelled = value.EqualsLiteral("true");

  if (!mIsBevelled) {
    mLineRect.height = mLineThickness;
    
    // by default, leave at least one-pixel padding at either end, or use
    // lspace & rspace that may come from <mo> if we are an embellished
    // container (we fetch values from the core since they may use units that
    // depend on style data, and style changes could have occurred in the
    // core since our last visit there)
    nscoord leftSpace = std::max(onePixel, StyleVisibility()->mDirection ?
                               coreData.trailingSpace : coreData.leadingSpace);
    nscoord rightSpace = std::max(onePixel, StyleVisibility()->mDirection ?
                                coreData.leadingSpace : coreData.trailingSpace);

    //////////////////
    // Get shifts
    nscoord numShift = 0;
    nscoord denShift = 0;

    // Rule 15b, App. G, TeXbook
    nscoord numShift1, numShift2, numShift3;
    nscoord denShift1, denShift2;

    GetNumeratorShifts(fm, numShift1, numShift2, numShift3);
    GetDenominatorShifts(fm, denShift1, denShift2);
    if (StyleFont()->mMathDisplay == NS_MATHML_DISPLAYSTYLE_BLOCK) {
      // C > T
      numShift = numShift1;
      denShift = denShift1;
    }
    else {
      numShift = (0 < mLineRect.height) ? numShift2 : numShift3;
      denShift = denShift2;
    }

    nscoord minClearance = 0;
    nscoord actualClearance = 0;

    nscoord actualRuleThickness =  mLineThickness;

    if (0 == actualRuleThickness) {
      // Rule 15c, App. G, TeXbook

      // min clearance between numerator and denominator
      minClearance = StyleFont()->mMathDisplay == NS_MATHML_DISPLAYSTYLE_BLOCK ?
        7 * defaultRuleThickness : 3 * defaultRuleThickness;
      actualClearance =
        (numShift - bmNum.descent) - (bmDen.ascent - denShift);
      // actualClearance should be >= minClearance
      if (actualClearance < minClearance) {
        nscoord halfGap = (minClearance - actualClearance)/2;
        numShift += halfGap;
        denShift += halfGap;
      }
    }
    else {
    // Rule 15d, App. G, TeXbook

    // min clearance between numerator or denominator and middle of bar

    // TeX has a different interpretation of the thickness.
    // Try $a \above10pt b$ to see. Here is what TeX does:
    // minClearance = StyleFont()->mMathDisplay == NS_MATHML_DISPLAYSTYLE_BLOCK
    // ? 3 * actualRuleThickness : actualRuleThickness;
 
    // we slightly depart from TeX here. We use the defaultRuleThickness instead
    // of the value coming from the linethickness attribute, i.e., we recover what
    // TeX does if the user hasn't set linethickness. But when the linethickness
    // is set, we avoid the wide gap problem.
     minClearance = StyleFont()->mMathDisplay == NS_MATHML_DISPLAYSTYLE_BLOCK ?
      3 * defaultRuleThickness : defaultRuleThickness + onePixel;

      // adjust numShift to maintain minClearance if needed
      actualClearance =
        (numShift - bmNum.descent) - (axisHeight + actualRuleThickness/2);
      if (actualClearance < minClearance) {
        numShift += (minClearance - actualClearance);
      }
      // adjust denShift to maintain minClearance if needed
      actualClearance =
        (axisHeight - actualRuleThickness/2) - (bmDen.ascent - denShift);
      if (actualClearance < minClearance) {
        denShift += (minClearance - actualClearance);
      }
    }

    //////////////////
    // Place Children

    // XXX Need revisiting the width. TeX uses the exact width
    // e.g. in $$\huge\frac{\displaystyle\int}{i}$$
    nscoord width = std::max(bmNum.width, bmDen.width);
    nscoord dxNum = leftSpace + (width - sizeNum.Width())/2;
    nscoord dxDen = leftSpace + (width - sizeDen.Width())/2;
    width += leftSpace + rightSpace;

    // see if the numalign attribute is there 
    mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::numalign_, value);
    if (value.EqualsLiteral("left"))
      dxNum = leftSpace;
    else if (value.EqualsLiteral("right"))
      dxNum = width - rightSpace - sizeNum.Width();

    // see if the denomalign attribute is there 
    mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::denomalign_, value);
    if (value.EqualsLiteral("left"))
      dxDen = leftSpace;
    else if (value.EqualsLiteral("right"))
      dxDen = width - rightSpace - sizeDen.Width();

    mBoundingMetrics.rightBearing =
      std::max(dxNum + bmNum.rightBearing, dxDen + bmDen.rightBearing);
    if (mBoundingMetrics.rightBearing < width - rightSpace)
      mBoundingMetrics.rightBearing = width - rightSpace;
    mBoundingMetrics.leftBearing =
      std::min(dxNum + bmNum.leftBearing, dxDen + bmDen.leftBearing);
    if (mBoundingMetrics.leftBearing > leftSpace)
      mBoundingMetrics.leftBearing = leftSpace;
    mBoundingMetrics.ascent = bmNum.ascent + numShift;
    mBoundingMetrics.descent = bmDen.descent + denShift;
    mBoundingMetrics.width = width;

    aDesiredSize.SetTopAscent(sizeNum.TopAscent() + numShift);
    aDesiredSize.Height() = aDesiredSize.TopAscent() +
      sizeDen.Height() - sizeDen.TopAscent() + denShift;
    aDesiredSize.Width() = mBoundingMetrics.width;
    aDesiredSize.mBoundingMetrics = mBoundingMetrics;

    mReference.x = 0;
    mReference.y = aDesiredSize.TopAscent();

    if (aPlaceOrigin) {
      nscoord dy;
      // place numerator
      dy = 0;
      FinishReflowChild(frameNum, presContext, sizeNum, nullptr, dxNum, dy, 0);
      // place denominator
      dy = aDesiredSize.Height() - sizeDen.Height();
      FinishReflowChild(frameDen, presContext, sizeDen, nullptr, dxDen, dy, 0);
      // place the fraction bar - dy is top of bar
      dy = aDesiredSize.TopAscent() - (axisHeight + actualRuleThickness/2);
      mLineRect.SetRect(leftSpace, dy, width - (leftSpace + rightSpace),
                        actualRuleThickness);
    }
  } else {
    nscoord numShift = 0.0;
    nscoord denShift = 0.0;
    nscoord padding = 3 * defaultRuleThickness;
    nscoord slashRatio = 3;

    // Define the constant used in the expression of the maximum width
    nscoord em = fm->EmHeight();
    nscoord slashMaxWidthConstant = 2 * em;

    // For large line thicknesses the minimum slash height is limited to the
    // largest expected height of a fraction
    nscoord slashMinHeight = slashRatio *
      std::min(2 * mLineThickness, slashMaxWidthConstant);

    nscoord leadingSpace = std::max(padding, coreData.leadingSpace);
    nscoord trailingSpace = std::max(padding, coreData.trailingSpace);
    nscoord delta;
    
    //           ___________
    //          |           |    /
    //         {|-NUMERATOR-|   /
    //         {|___________|  S
    //         {               L
    // numShift{               A
    // ------------------------------------------------------- baseline
    //                         S   _____________ } denShift
    //                         H  |             |}
    //                        /   |-DENOMINATOR-|}
    //                       /    |_____________| 
    //

    // first, ensure that the top of the numerator is at least as high as the
    // top of the denominator (and the reverse for the bottoms)
    delta = std::max(bmDen.ascent - bmNum.ascent,
                   bmNum.descent - bmDen.descent) / 2;
    if (delta > 0) {
      numShift += delta;
      denShift += delta;
    }

    if (StyleFont()->mMathDisplay == NS_MATHML_DISPLAYSTYLE_BLOCK) {
      delta = std::min(bmDen.ascent + bmDen.descent,
                     bmNum.ascent + bmNum.descent) / 2;
      numShift += delta;
      denShift += delta;
    } else {
      nscoord xHeight = fm->XHeight();
      numShift += xHeight / 2;
      denShift += xHeight / 4;
    }
   
    // Set the ascent/descent of our BoundingMetrics.
    mBoundingMetrics.ascent = bmNum.ascent + numShift;
    mBoundingMetrics.descent = bmDen.descent + denShift;

    // At this point the height of the slash is
    // mBoundingMetrics.ascent + mBoundingMetrics.descent
    // Ensure that it is greater than slashMinHeight
    delta = (slashMinHeight -
             (mBoundingMetrics.ascent + mBoundingMetrics.descent)) / 2;
    if (delta > 0) {
      mBoundingMetrics.ascent += delta;
      mBoundingMetrics.descent += delta;
    }

    // Set the width of the slash
    if (aWidthOnly) {
      mLineRect.width = mLineThickness + slashMaxWidthConstant;
    } else {
      mLineRect.width = mLineThickness +
        std::min(slashMaxWidthConstant,
               (mBoundingMetrics.ascent + mBoundingMetrics.descent) /
               slashRatio);
    }

    // Set horizontal bounding metrics
    if (StyleVisibility()->mDirection) {
      mBoundingMetrics.leftBearing = trailingSpace + bmDen.leftBearing;
      mBoundingMetrics.rightBearing = trailingSpace + bmDen.width + mLineRect.width + bmNum.rightBearing;
    } else {
      mBoundingMetrics.leftBearing = leadingSpace + bmNum.leftBearing;
      mBoundingMetrics.rightBearing = leadingSpace + bmNum.width + mLineRect.width + bmDen.rightBearing;
    }
    mBoundingMetrics.width =
      leadingSpace + bmNum.width + mLineRect.width + bmDen.width +
      trailingSpace;

    // Set aDesiredSize
    aDesiredSize.SetTopAscent(mBoundingMetrics.ascent + padding);
    aDesiredSize.Height() =
      mBoundingMetrics.ascent + mBoundingMetrics.descent + 2 * padding;
    aDesiredSize.Width() = mBoundingMetrics.width;
    aDesiredSize.mBoundingMetrics = mBoundingMetrics;

    mReference.x = 0;
    mReference.y = aDesiredSize.TopAscent();
    
    if (aPlaceOrigin) {
      nscoord dx, dy;

      // place numerator
      dx = MirrorIfRTL(aDesiredSize.Width(), sizeNum.Width(),
                       leadingSpace);
      dy = aDesiredSize.TopAscent() - numShift - sizeNum.TopAscent();
      FinishReflowChild(frameNum, presContext, sizeNum, nullptr, dx, dy, 0);

      // place the fraction bar
      dx = MirrorIfRTL(aDesiredSize.Width(), mLineRect.width,
                       leadingSpace + bmNum.width);
      dy = aDesiredSize.TopAscent() - mBoundingMetrics.ascent;
      mLineRect.SetRect(dx, dy,
                        mLineRect.width, aDesiredSize.Height() - 2 * padding);

      // place denominator
      dx = MirrorIfRTL(aDesiredSize.Width(), sizeDen.Width(),
                       leadingSpace + bmNum.width + mLineRect.width);
      dy = aDesiredSize.TopAscent() + denShift - sizeDen.TopAscent();
      FinishReflowChild(frameDen, presContext, sizeDen, nullptr, dx, dy, 0);
    }

  }

  return NS_OK;
}

class nsDisplayMathMLSlash : public nsDisplayItem {
public:
  nsDisplayMathMLSlash(nsDisplayListBuilder* aBuilder,
                       nsIFrame* aFrame, const nsRect& aRect,
                       nscoord aThickness, bool aRTL)
    : nsDisplayItem(aBuilder, aFrame), mRect(aRect), mThickness(aThickness),
      mRTL(aRTL) {
    MOZ_COUNT_CTOR(nsDisplayMathMLSlash);
  }
#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplayMathMLSlash() {
    MOZ_COUNT_DTOR(nsDisplayMathMLSlash);
  }
#endif

  virtual void Paint(nsDisplayListBuilder* aBuilder, nsRenderingContext* aCtx);
  NS_DISPLAY_DECL_NAME("MathMLSlash", TYPE_MATHML_SLASH)

private:
  nsRect    mRect;
  nscoord   mThickness;
  bool      mRTL;
};

void nsDisplayMathMLSlash::Paint(nsDisplayListBuilder* aBuilder,
                                 nsRenderingContext* aCtx)
{
  // get the gfxRect
  nsPresContext* presContext = mFrame->PresContext();
  gfxRect rect = presContext->AppUnitsToGfxUnits(mRect + ToReferenceFrame());
  
  // paint with the current text color
  aCtx->SetColor(mFrame->GetVisitedDependentColor(eCSSProperty_color));
 
  // draw the slash as a parallelogram 
  gfxContext *gfxCtx = aCtx->ThebesContext();
  gfxPoint delta = gfxPoint(presContext->AppUnitsToGfxUnits(mThickness), 0);
  gfxCtx->NewPath();

  if (mRTL) {
    gfxCtx->MoveTo(rect.TopLeft());
    gfxCtx->LineTo(rect.TopLeft() + delta);
    gfxCtx->LineTo(rect.BottomRight());
    gfxCtx->LineTo(rect.BottomRight() - delta);
  } else {
    gfxCtx->MoveTo(rect.BottomLeft());
    gfxCtx->LineTo(rect.BottomLeft() + delta);
    gfxCtx->LineTo(rect.TopRight());
    gfxCtx->LineTo(rect.TopRight() - delta);
  }

  gfxCtx->ClosePath();
  gfxCtx->Fill();
}

void
nsMathMLmfracFrame::DisplaySlash(nsDisplayListBuilder* aBuilder,
                                 nsIFrame* aFrame, const nsRect& aRect,
                                 nscoord aThickness,
                                 const nsDisplayListSet& aLists) {
  if (!aFrame->StyleVisibility()->IsVisible() || aRect.IsEmpty())
    return;

  aLists.Content()->AppendNewToTop(new (aBuilder)
    nsDisplayMathMLSlash(aBuilder, aFrame, aRect, aThickness,
                         StyleVisibility()->mDirection));
}
