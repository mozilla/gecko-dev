/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#include "nsMathMLmfencedFrame.h"
#include "nsRenderingContext.h"
#include "nsMathMLChar.h"
#include <algorithm>

//
// <mfenced> -- surround content with a pair of fences
//

nsIFrame*
NS_NewMathMLmfencedFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsMathMLmfencedFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsMathMLmfencedFrame)

nsMathMLmfencedFrame::~nsMathMLmfencedFrame()
{
  RemoveFencesAndSeparators();
}

NS_IMETHODIMP
nsMathMLmfencedFrame::InheritAutomaticData(nsIFrame* aParent)
{
  // let the base class get the default from our parent
  nsMathMLContainerFrame::InheritAutomaticData(aParent);

  mPresentationData.flags |= NS_MATHML_STRETCH_ALL_CHILDREN_VERTICALLY;

  RemoveFencesAndSeparators();
  CreateFencesAndSeparators(PresContext());

  return NS_OK;
}

nsresult
nsMathMLmfencedFrame::SetInitialChildList(ChildListID     aListID,
                                          nsFrameList&    aChildList)
{
  // First, let the base class do its work
  nsresult rv = nsMathMLContainerFrame::SetInitialChildList(aListID, aChildList);
  if (NS_FAILED(rv)) return rv;

  // InheritAutomaticData will not get called if our parent is not a mathml
  // frame, so initialize NS_MATHML_STRETCH_ALL_CHILDREN_VERTICALLY for
  // GetPreferredStretchSize() from Reflow().
  mPresentationData.flags |= NS_MATHML_STRETCH_ALL_CHILDREN_VERTICALLY;
  // No need to track the style contexts given to our MathML chars. 
  // The Style System will use Get/SetAdditionalStyleContext() to keep them
  // up-to-date if dynamic changes arise.
  CreateFencesAndSeparators(PresContext());
  return NS_OK;
}

nsresult
nsMathMLmfencedFrame::AttributeChanged(int32_t         aNameSpaceID,
                                       nsIAtom*        aAttribute,
                                       int32_t         aModType)
{
  RemoveFencesAndSeparators();
  CreateFencesAndSeparators(PresContext());

  return nsMathMLContainerFrame::
         AttributeChanged(aNameSpaceID, aAttribute, aModType);
}

nsresult
nsMathMLmfencedFrame::ChildListChanged(int32_t aModType)
{
  RemoveFencesAndSeparators();
  CreateFencesAndSeparators(PresContext());

  return nsMathMLContainerFrame::ChildListChanged(aModType);
}

void
nsMathMLmfencedFrame::RemoveFencesAndSeparators()
{
  delete mOpenChar;
  delete mCloseChar;
  if (mSeparatorsChar) delete[] mSeparatorsChar;

  mOpenChar = nullptr;
  mCloseChar = nullptr;
  mSeparatorsChar = nullptr;
  mSeparatorsCount = 0;
}

void
nsMathMLmfencedFrame::CreateFencesAndSeparators(nsPresContext* aPresContext)
{
  nsAutoString value;
  bool isMutable = false;

  //////////////  
  // see if the opening fence is there ...
  if (!mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::open, value)) {
    value = char16_t('('); // default as per the MathML REC
  } else {
    value.CompressWhitespace();
  }

  if (!value.IsEmpty()) {
    mOpenChar = new nsMathMLChar;
    mOpenChar->SetData(aPresContext, value);
    isMutable = nsMathMLOperators::IsMutableOperator(value);
    ResolveMathMLCharStyle(aPresContext, mContent, mStyleContext, mOpenChar, isMutable);
  }

  //////////////
  // see if the closing fence is there ...
  if(!mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::close, value)) {
    value = char16_t(')'); // default as per the MathML REC
  } else {
    value.CompressWhitespace();
  }

  if (!value.IsEmpty()) {
    mCloseChar = new nsMathMLChar;
    mCloseChar->SetData(aPresContext, value);
    isMutable = nsMathMLOperators::IsMutableOperator(value);
    ResolveMathMLCharStyle(aPresContext, mContent, mStyleContext, mCloseChar, isMutable);
  }

  //////////////
  // see if separators are there ...
  if (!mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::separators_, value)) {
    value = char16_t(','); // default as per the MathML REC
  } else {
    value.StripWhitespace();
  }

  mSeparatorsCount = value.Length();
  if (0 < mSeparatorsCount) {
    int32_t sepCount = mFrames.GetLength() - 1;
    if (0 < sepCount) {
      mSeparatorsChar = new nsMathMLChar[sepCount];
      nsAutoString sepChar;
      for (int32_t i = 0; i < sepCount; i++) {
        if (i < mSeparatorsCount) {
          sepChar = value[i];
          isMutable = nsMathMLOperators::IsMutableOperator(sepChar);
        }
        else {
          sepChar = value[mSeparatorsCount-1];
          // keep the value of isMutable that was set earlier
        }
        mSeparatorsChar[i].SetData(aPresContext, sepChar);
        ResolveMathMLCharStyle(aPresContext, mContent, mStyleContext, &mSeparatorsChar[i], isMutable);
      }
      mSeparatorsCount = sepCount;
    } else {
      // No separators.  Note that sepCount can be -1 here, so don't
      // set mSeparatorsCount to it.
      mSeparatorsCount = 0;
    }
  }
}

void
nsMathMLmfencedFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                       const nsRect&           aDirtyRect,
                                       const nsDisplayListSet& aLists)
{
  /////////////
  // display the content
  nsMathMLContainerFrame::BuildDisplayList(aBuilder, aDirtyRect, aLists);
  
  ////////////
  // display fences and separators
  uint32_t count = 0;
  if (mOpenChar) {
    mOpenChar->Display(aBuilder, this, aLists, count++);
  }
  
  if (mCloseChar) {
    mCloseChar->Display(aBuilder, this, aLists, count++);
  }
  
  for (int32_t i = 0; i < mSeparatorsCount; i++) {
    mSeparatorsChar[i].Display(aBuilder, this, aLists, count++);
  }
}

nsresult
nsMathMLmfencedFrame::Reflow(nsPresContext*          aPresContext,
                             nsHTMLReflowMetrics&     aDesiredSize,
                             const nsHTMLReflowState& aReflowState,
                             nsReflowStatus&          aStatus)
{
  nsresult rv;
  aDesiredSize.Width() = aDesiredSize.Height() = 0;
  aDesiredSize.SetTopAscent(0);
  aDesiredSize.mBoundingMetrics = nsBoundingMetrics();

  int32_t i;
  const nsStyleFont* font = StyleFont();
  nsRefPtr<nsFontMetrics> fm;
  nsLayoutUtils::GetFontMetricsForFrame(this, getter_AddRefs(fm));
  aReflowState.rendContext->SetFont(fm);
  nscoord axisHeight, em;
  GetAxisHeight(*aReflowState.rendContext, fm, axisHeight);
  GetEmHeight(fm, em);
  // leading to be left at the top and the bottom of stretched chars
  nscoord leading = NSToCoordRound(0.2f * em); 

  /////////////
  // Reflow children
  // Asking each child to cache its bounding metrics

  // Note that we don't use the base method nsMathMLContainerFrame::Reflow()
  // because we want to stretch our fences, separators and stretchy frames using
  // the *same* initial aDesiredSize.mBoundingMetrics. If we were to use the base
  // method here, our stretchy frames will be stretched and placed, and we may
  // end up stretching our fences/separators with a different aDesiredSize.
  // XXX The above decision was revisited in bug 121748 and this code can be
  // refactored to use nsMathMLContainerFrame::Reflow() at some stage.

  nsReflowStatus childStatus;
  nsSize availSize(aReflowState.ComputedWidth(), NS_UNCONSTRAINEDSIZE);
  nsIFrame* firstChild = GetFirstPrincipalChild();
  nsIFrame* childFrame = firstChild;
  nscoord ascent = 0, descent = 0;
  if (firstChild || mOpenChar || mCloseChar || mSeparatorsCount > 0) {
    // We use the ASCII metrics to get our minimum height. This way,
    // if we have borders or a background, they will fit better with
    // other elements on the line.
    ascent = fm->MaxAscent();
    descent = fm->MaxDescent();
  }
  while (childFrame) {
    nsHTMLReflowMetrics childDesiredSize(aReflowState,
                                         aDesiredSize.mFlags
                                         | NS_REFLOW_CALC_BOUNDING_METRICS);
    nsHTMLReflowState childReflowState(aPresContext, aReflowState,
                                       childFrame, availSize);
    rv = ReflowChild(childFrame, aPresContext, childDesiredSize,
                     childReflowState, childStatus);
    //NS_ASSERTION(NS_FRAME_IS_COMPLETE(childStatus), "bad status");
    if (NS_FAILED(rv)) {
      // Call DidReflow() for the child frames we successfully did reflow.
      DidReflowChildren(firstChild, childFrame);
      return rv;
    }

    SaveReflowAndBoundingMetricsFor(childFrame, childDesiredSize,
                                    childDesiredSize.mBoundingMetrics);

    nscoord childDescent = childDesiredSize.Height() - childDesiredSize.TopAscent();
    if (descent < childDescent)
      descent = childDescent;
    if (ascent < childDesiredSize.TopAscent())
      ascent = childDesiredSize.TopAscent();

    childFrame = childFrame->GetNextSibling();
  }

  /////////////
  // Ask stretchy children to stretch themselves

  nsBoundingMetrics containerSize;
  nsStretchDirection stretchDir = NS_STRETCH_DIRECTION_VERTICAL;

  GetPreferredStretchSize(*aReflowState.rendContext,
                          0, /* i.e., without embellishments */
                          stretchDir, containerSize);
  childFrame = firstChild;
  while (childFrame) {
    nsIMathMLFrame* mathmlChild = do_QueryFrame(childFrame);
    if (mathmlChild) {
      nsHTMLReflowMetrics childDesiredSize(aReflowState);
      // retrieve the metrics that was stored at the previous pass
      GetReflowAndBoundingMetricsFor(childFrame, childDesiredSize,
                                     childDesiredSize.mBoundingMetrics);
      
      mathmlChild->Stretch(*aReflowState.rendContext, 
                           stretchDir, containerSize, childDesiredSize);
      // store the updated metrics
      SaveReflowAndBoundingMetricsFor(childFrame, childDesiredSize,
                                      childDesiredSize.mBoundingMetrics);
      
      nscoord childDescent = childDesiredSize.Height() - childDesiredSize.TopAscent();
      if (descent < childDescent)
        descent = childDescent;
      if (ascent < childDesiredSize.TopAscent())
        ascent = childDesiredSize.TopAscent();
    }
    childFrame = childFrame->GetNextSibling();
  }

  // bug 121748: for surrounding fences & separators, use a size that covers everything
  GetPreferredStretchSize(*aReflowState.rendContext,
                          STRETCH_CONSIDER_EMBELLISHMENTS,
                          stretchDir, containerSize);

  //////////////////////////////////////////
  // Prepare the opening fence, separators, and closing fence, and
  // adjust the origin of children.

  // we need to center around the axis
  nscoord delta = std::max(containerSize.ascent - axisHeight, 
                         containerSize.descent + axisHeight);
  containerSize.ascent = delta + axisHeight;
  containerSize.descent = delta - axisHeight;

  bool isRTL = StyleVisibility()->mDirection;

  /////////////////
  // opening fence ...
  ReflowChar(aPresContext, *aReflowState.rendContext, mOpenChar,
             NS_MATHML_OPERATOR_FORM_PREFIX, font->mScriptLevel, 
             axisHeight, leading, em, containerSize, ascent, descent, isRTL);
  /////////////////
  // separators ...
  for (i = 0; i < mSeparatorsCount; i++) {
    ReflowChar(aPresContext, *aReflowState.rendContext, &mSeparatorsChar[i],
               NS_MATHML_OPERATOR_FORM_INFIX, font->mScriptLevel,
               axisHeight, leading, em, containerSize, ascent, descent, isRTL);
  }
  /////////////////
  // closing fence ...
  ReflowChar(aPresContext, *aReflowState.rendContext, mCloseChar,
             NS_MATHML_OPERATOR_FORM_POSTFIX, font->mScriptLevel,
             axisHeight, leading, em, containerSize, ascent, descent, isRTL);

  //////////////////
  // Adjust the origins of each child.
  // and update our bounding metrics

  i = 0;
  nscoord dx = 0;
  nsBoundingMetrics bm;
  bool firstTime = true;
  nsMathMLChar *leftChar, *rightChar;
  if (isRTL) {
    leftChar = mCloseChar;
    rightChar = mOpenChar;
  } else {
    leftChar = mOpenChar;
    rightChar = mCloseChar;
  }

  if (leftChar) {
    PlaceChar(leftChar, ascent, bm, dx);
    aDesiredSize.mBoundingMetrics = bm;
    firstTime = false;
  }

  if (isRTL) {
    childFrame = this->GetLastChild(nsIFrame::kPrincipalList);
  } else {
    childFrame = firstChild;
  }
  while (childFrame) {
    nsHTMLReflowMetrics childSize(aReflowState);
    GetReflowAndBoundingMetricsFor(childFrame, childSize, bm);
    if (firstTime) {
      firstTime = false;
      aDesiredSize.mBoundingMetrics  = bm;
    }
    else  
      aDesiredSize.mBoundingMetrics += bm;

    FinishReflowChild(childFrame, aPresContext, childSize, nullptr,
                      dx, ascent - childSize.TopAscent(), 0);
    dx += childSize.Width();

    if (i < mSeparatorsCount) {
      PlaceChar(&mSeparatorsChar[isRTL ? mSeparatorsCount - 1 - i : i],
                ascent, bm, dx);
      aDesiredSize.mBoundingMetrics += bm;
    }
    i++;

    if (isRTL) {
      childFrame = childFrame->GetPrevSibling();
    } else {
      childFrame = childFrame->GetNextSibling();
    }
  }

  if (rightChar) {
    PlaceChar(rightChar, ascent, bm, dx);
    if (firstTime)
      aDesiredSize.mBoundingMetrics  = bm;
    else  
      aDesiredSize.mBoundingMetrics += bm;
  }

  aDesiredSize.Width() = aDesiredSize.mBoundingMetrics.width;
  aDesiredSize.Height() = ascent + descent;
  aDesiredSize.SetTopAscent(ascent);

  SetBoundingMetrics(aDesiredSize.mBoundingMetrics);
  SetReference(nsPoint(0, aDesiredSize.TopAscent()));

  // see if we should fix the spacing
  FixInterFrameSpacing(aDesiredSize);

  // Finished with these:
  ClearSavedChildMetrics();

  // Set our overflow area
  GatherAndStoreOverflow(&aDesiredSize);

  aStatus = NS_FRAME_COMPLETE;
  NS_FRAME_SET_TRUNCATION(aStatus, aReflowState, aDesiredSize);
  return NS_OK;
}

static void
GetCharSpacing(nsMathMLChar*        aMathMLChar,
               nsOperatorFlags      aForm,
               int32_t              aScriptLevel,
               nscoord              em,
               nscoord&             aLeftSpace,
               nscoord&             aRightSpace)
{
  nsAutoString data;
  aMathMLChar->GetData(data);
  nsOperatorFlags flags = 0;
  float lspace = 0.0f;
  float rspace = 0.0f;
  bool found = nsMathMLOperators::LookupOperator(data, aForm,
                                                   &flags, &lspace, &rspace);

  // We don't want extra space when we are a script
  if (found && aScriptLevel > 0) {
    lspace /= 2.0f;
    rspace /= 2.0f;
  }

  aLeftSpace = NSToCoordRound(lspace * em);
  aRightSpace = NSToCoordRound(rspace * em);
}

// helper functions to perform the common task of formatting our chars
/*static*/ nsresult
nsMathMLmfencedFrame::ReflowChar(nsPresContext*      aPresContext,
                                 nsRenderingContext& aRenderingContext,
                                 nsMathMLChar*        aMathMLChar,
                                 nsOperatorFlags      aForm,
                                 int32_t              aScriptLevel,
                                 nscoord              axisHeight,
                                 nscoord              leading,
                                 nscoord              em,
                                 nsBoundingMetrics&   aContainerSize,
                                 nscoord&             aAscent,
                                 nscoord&             aDescent,
                                 bool                 aRTL)
{
  if (aMathMLChar && 0 < aMathMLChar->Length()) {
    nscoord leftSpace;
    nscoord rightSpace;
    GetCharSpacing(aMathMLChar, aForm, aScriptLevel, em, leftSpace, rightSpace);

    // stretch the char to the appropriate height if it is not big enough.
    nsBoundingMetrics charSize;
    nsresult res = aMathMLChar->Stretch(aPresContext, aRenderingContext,
                                        NS_STRETCH_DIRECTION_VERTICAL,
                                        aContainerSize, charSize,
                                        NS_STRETCH_NORMAL, aRTL);

    if (NS_STRETCH_DIRECTION_UNSUPPORTED != aMathMLChar->GetStretchDirection()) {
      // has changed... so center the char around the axis
      nscoord height = charSize.ascent + charSize.descent;
      charSize.ascent = height/2 + axisHeight;
      charSize.descent = height - charSize.ascent;
    }
    else {
      // either it hasn't changed or stretching the char failed (i.e.,
      // GetBoundingMetrics failed)
      leading = 0;
      if (NS_FAILED(res)) {
        nsAutoString data;
        aMathMLChar->GetData(data);
        nsBoundingMetrics metrics =
          aRenderingContext.GetBoundingMetrics(data.get(), data.Length());
        charSize.ascent = metrics.ascent;
        charSize.descent = metrics.descent;
        charSize.width = metrics.width;
        // Set this as the bounding metrics of the MathMLChar to leave
        // the necessary room to paint the char.
        aMathMLChar->SetBoundingMetrics(charSize);
      }
    }

    if (aAscent < charSize.ascent + leading) 
      aAscent = charSize.ascent + leading;
    if (aDescent < charSize.descent + leading) 
      aDescent = charSize.descent + leading;

    // account the spacing
    charSize.width += leftSpace + rightSpace;

    // x-origin is used to store lspace ...
    // y-origin is used to stored the ascent ... 
    aMathMLChar->SetRect(nsRect(leftSpace, 
                                charSize.ascent, charSize.width,
                                charSize.ascent + charSize.descent));
  }
  return NS_OK;
}

/*static*/ void
nsMathMLmfencedFrame::PlaceChar(nsMathMLChar*      aMathMLChar,
                                nscoord            aDesiredAscent,
                                nsBoundingMetrics& bm,
                                nscoord&           dx)
{
  aMathMLChar->GetBoundingMetrics(bm);

  // the char's x-origin was used to store lspace ...
  // the char's y-origin was used to store the ascent ... 
  // the char's width was used to store the advance with (with spacing) ... 
  nsRect rect;
  aMathMLChar->GetRect(rect);

  nscoord dy = aDesiredAscent - rect.y;
  if (aMathMLChar->GetStretchDirection() != NS_STRETCH_DIRECTION_UNSUPPORTED) {
    // the stretchy char will be centered around the axis
    // so we adjust the returned bounding metrics accordingly
    bm.descent = (bm.ascent + bm.descent) - rect.y;
    bm.ascent = rect.y;
  }

  aMathMLChar->SetRect(nsRect(dx + rect.x, dy, bm.width, rect.height));

  bm.leftBearing += rect.x;
  bm.rightBearing += rect.x;

  // return rect.width since it includes lspace and rspace
  bm.width = rect.width;
  dx += rect.width;
}

static nscoord
GetMaxCharWidth(nsPresContext*       aPresContext,
                nsRenderingContext* aRenderingContext,
                nsMathMLChar*        aMathMLChar,
                nsOperatorFlags      aForm,
                int32_t              aScriptLevel,
                nscoord              em)
{
  nscoord width = aMathMLChar->GetMaxWidth(aPresContext, *aRenderingContext);

  if (0 < aMathMLChar->Length()) {
    nscoord leftSpace;
    nscoord rightSpace;
    GetCharSpacing(aMathMLChar, aForm, aScriptLevel, em, leftSpace, rightSpace);

    width += leftSpace + rightSpace;
  }
  
  return width;
}

/* virtual */ void
nsMathMLmfencedFrame::GetIntrinsicWidthMetrics(nsRenderingContext* aRenderingContext, nsHTMLReflowMetrics& aDesiredSize)
{
  nscoord width = 0;

  nsPresContext* presContext = PresContext();
  const nsStyleFont* font = StyleFont();
  nsRefPtr<nsFontMetrics> fm;
  nsLayoutUtils::GetFontMetricsForFrame(this, getter_AddRefs(fm));
  nscoord em;
  GetEmHeight(fm, em);

  if (mOpenChar) {
    width +=
      GetMaxCharWidth(presContext, aRenderingContext, mOpenChar,
                      NS_MATHML_OPERATOR_FORM_PREFIX, font->mScriptLevel, em);
  }

  int32_t i = 0;
  nsIFrame* childFrame = GetFirstPrincipalChild();
  while (childFrame) {
    // XXX This includes margin while Reflow currently doesn't consider
    // margin, so we may end up with too much space, but, with stretchy
    // characters, this is an approximation anyway.
    width += nsLayoutUtils::IntrinsicForContainer(aRenderingContext, childFrame,
                                                  nsLayoutUtils::PREF_WIDTH);

    if (i < mSeparatorsCount) {
      width +=
        GetMaxCharWidth(presContext, aRenderingContext, &mSeparatorsChar[i],
                        NS_MATHML_OPERATOR_FORM_INFIX, font->mScriptLevel, em);
    }
    i++;

    childFrame = childFrame->GetNextSibling();
  }

  if (mCloseChar) {
    width +=
      GetMaxCharWidth(presContext, aRenderingContext, mCloseChar,
                      NS_MATHML_OPERATOR_FORM_POSTFIX, font->mScriptLevel, em);
  }

  aDesiredSize.Width() = width;
  aDesiredSize.mBoundingMetrics.width = width;
  aDesiredSize.mBoundingMetrics.leftBearing = 0;
  aDesiredSize.mBoundingMetrics.rightBearing = width;
}

nscoord
nsMathMLmfencedFrame::FixInterFrameSpacing(nsHTMLReflowMetrics& aDesiredSize)
{
  nscoord gap = nsMathMLContainerFrame::FixInterFrameSpacing(aDesiredSize);
  if (!gap) return 0;

  nsRect rect;
  if (mOpenChar) {
    mOpenChar->GetRect(rect);
    rect.MoveBy(gap, 0);
    mOpenChar->SetRect(rect);
  }
  if (mCloseChar) {
    mCloseChar->GetRect(rect);
    rect.MoveBy(gap, 0);
    mCloseChar->SetRect(rect);
  }
  for (int32_t i = 0; i < mSeparatorsCount; i++) {
    mSeparatorsChar[i].GetRect(rect);
    rect.MoveBy(gap, 0);
    mSeparatorsChar[i].SetRect(rect);
  }
  return gap;
}

// ----------------------
// the Style System will use these to pass the proper style context to our MathMLChar
nsStyleContext*
nsMathMLmfencedFrame::GetAdditionalStyleContext(int32_t aIndex) const
{
  int32_t openIndex = -1;
  int32_t closeIndex = -1;
  int32_t lastIndex = mSeparatorsCount-1;

  if (mOpenChar) { 
    lastIndex++; 
    openIndex = lastIndex; 
  }
  if (mCloseChar) { 
    lastIndex++;
    closeIndex = lastIndex;
  }
  if (aIndex < 0 || aIndex > lastIndex) {
    return nullptr;
  }

  if (aIndex < mSeparatorsCount) {
    return mSeparatorsChar[aIndex].GetStyleContext();
  }
  else if (aIndex == openIndex) {
    return mOpenChar->GetStyleContext();
  }
  else if (aIndex == closeIndex) {
    return mCloseChar->GetStyleContext();
  }
  return nullptr;
}

void
nsMathMLmfencedFrame::SetAdditionalStyleContext(int32_t          aIndex, 
                                                nsStyleContext*  aStyleContext)
{
  int32_t openIndex = -1;
  int32_t closeIndex = -1;
  int32_t lastIndex = mSeparatorsCount-1;

  if (mOpenChar) {
    lastIndex++;
    openIndex = lastIndex;
  }
  if (mCloseChar) {
    lastIndex++;
    closeIndex = lastIndex;
  }
  if (aIndex < 0 || aIndex > lastIndex) {
    return;
  }

  if (aIndex < mSeparatorsCount) {
    mSeparatorsChar[aIndex].SetStyleContext(aStyleContext);
  }
  else if (aIndex == openIndex) {
    mOpenChar->SetStyleContext(aStyleContext);
  }
  else if (aIndex == closeIndex) {
    mCloseChar->SetStyleContext(aStyleContext);
  }
}
