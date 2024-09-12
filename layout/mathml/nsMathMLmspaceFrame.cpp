/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsMathMLmspaceFrame.h"

#include "mozilla/dom/MathMLElement.h"
#include "mozilla/PresShell.h"
#include "mozilla/gfx/2D.h"
#include "nsLayoutUtils.h"
#include <algorithm>

using namespace mozilla;

//
// <mspace> -- space - implementation
//

nsIFrame* NS_NewMathMLmspaceFrame(PresShell* aPresShell,
                                  ComputedStyle* aStyle) {
  return new (aPresShell)
      nsMathMLmspaceFrame(aStyle, aPresShell->GetPresContext());
}

NS_IMPL_FRAMEARENA_HELPERS(nsMathMLmspaceFrame)

nsMathMLmspaceFrame::~nsMathMLmspaceFrame() = default;

nsresult nsMathMLmspaceFrame::AttributeChanged(int32_t aNameSpaceID,
                                               nsAtom* aAttribute,
                                               int32_t aModType) {
  if (aNameSpaceID == kNameSpaceID_None) {
    bool hasDirtyAttributes = false;
    if (aAttribute == nsGkAtoms::width) {
      mWidth.mState = Attribute::ParsingState::Dirty;
      hasDirtyAttributes = true;
    } else if (aAttribute == nsGkAtoms::height) {
      mHeight.mState = Attribute::ParsingState::Dirty;
      hasDirtyAttributes = true;
    } else if (aAttribute == nsGkAtoms::depth_) {
      mDepth.mState = Attribute::ParsingState::Dirty;
      hasDirtyAttributes = true;
    }
    if (hasDirtyAttributes) {
      InvalidateFrame();
      // TODO(bug 1918308): This was copied from nsMathMLContainerFrame and
      // seems necessary for some invalidation tests, but we can probably do
      // less.
      PresShell()->FrameNeedsReflow(
          this, IntrinsicDirty::FrameAncestorsAndDescendants,
          NS_FRAME_IS_DIRTY);
    }
    return NS_OK;
  }
  return nsMathMLContainerFrame::AttributeChanged(aNameSpaceID, aAttribute,
                                                  aModType);
}

nscoord nsMathMLmspaceFrame::CalculateAttributeValue(nsAtom* aAtom,
                                                     Attribute& aAttribute,
                                                     uint32_t aFlags,
                                                     float aFontSizeInflation) {
  if (aAttribute.mState == Attribute::ParsingState::Dirty) {
    nsAutoString value;
    aAttribute.mState = Attribute::ParsingState::Invalid;
    mContent->AsElement()->GetAttr(aAtom, value);
    if (!value.IsEmpty()) {
      if (dom::MathMLElement::ParseNumericValue(
              value, aAttribute.mValue, aFlags, PresContext()->Document())) {
        aAttribute.mState = Attribute::ParsingState::Valid;
      } else {
        ReportParseError(aAtom->GetUTF16String(), value.get());
      }
    }
  }
  // Invalid is interpreted as the default which is 0.
  // Percentages are interpreted as a multiple of the default value.
  if (aAttribute.mState == Attribute::ParsingState::Invalid ||
      aAttribute.mValue.GetUnit() == eCSSUnit_Percent) {
    return 0;
  }
  return CalcLength(PresContext(), mComputedStyle, aAttribute.mValue,
                    aFontSizeInflation);
}

nsresult nsMathMLmspaceFrame::Place(DrawTarget* aDrawTarget,
                                    const PlaceFlags& aFlags,
                                    ReflowOutput& aDesiredSize) {
  float fontSizeInflation = nsLayoutUtils::FontSizeInflationFor(this);

  // <mspace/> is listed among MathML elements allowing negative spacing and
  // the MathML test suite contains "Presentation/TokenElements/mspace/mspace2"
  // as an example. Hence we allow negative values.
  nscoord width = CalculateAttributeValue(
      nsGkAtoms::width, mWidth, dom::MathMLElement::PARSE_ALLOW_NEGATIVE,
      fontSizeInflation);

  // We do not allow negative values for height and depth attributes. See bug
  // 716349.
  nscoord height =
      CalculateAttributeValue(nsGkAtoms::height, mHeight, 0, fontSizeInflation);
  nscoord depth =
      CalculateAttributeValue(nsGkAtoms::depth_, mDepth, 0, fontSizeInflation);

  mBoundingMetrics = nsBoundingMetrics();
  mBoundingMetrics.width = width;
  mBoundingMetrics.ascent = height;
  mBoundingMetrics.descent = depth;
  mBoundingMetrics.leftBearing = 0;
  mBoundingMetrics.rightBearing = mBoundingMetrics.width;

  aDesiredSize.SetBlockStartAscent(mBoundingMetrics.ascent);
  aDesiredSize.Width() = std::max(0, mBoundingMetrics.width);
  aDesiredSize.Height() = mBoundingMetrics.ascent + mBoundingMetrics.descent;
  // Also return our bounding metrics
  aDesiredSize.mBoundingMetrics = mBoundingMetrics;

  // Add padding+border.
  auto borderPadding = GetBorderPaddingForPlace(aFlags);
  InflateReflowAndBoundingMetrics(borderPadding, aDesiredSize,
                                  mBoundingMetrics);
  return NS_OK;
}
