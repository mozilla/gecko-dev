/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsMathMLmpaddedFrame_h___
#define nsMathMLmpaddedFrame_h___

#include "mozilla/Attributes.h"
#include "nsCSSValue.h"
#include "nsMathMLContainerFrame.h"

namespace mozilla {
class PresShell;
}  // namespace mozilla

//
// <mpadded> -- adjust space around content
//

class nsMathMLmpaddedFrame final : public nsMathMLContainerFrame {
 public:
  NS_DECL_FRAMEARENA_HELPERS(nsMathMLmpaddedFrame)

  friend nsIFrame* NS_NewMathMLmpaddedFrame(mozilla::PresShell* aPresShell,
                                            ComputedStyle* aStyle);

  NS_IMETHOD
  InheritAutomaticData(nsIFrame* aParent) override;

  NS_IMETHOD
  TransmitAutomaticData() override {
    return TransmitAutomaticDataForMrowLikeElement();
  }

  virtual void Reflow(nsPresContext* aPresContext, ReflowOutput& aDesiredSize,
                      const ReflowInput& aReflowInput,
                      nsReflowStatus& aStatus) override;

  nsresult Place(DrawTarget* aDrawTarget, const PlaceFlags& aFlags,
                 ReflowOutput& aDesiredSize) override;

  bool IsMrowLike() override {
    return mFrames.FirstChild() != mFrames.LastChild() || !mFrames.FirstChild();
  }

 protected:
  explicit nsMathMLmpaddedFrame(ComputedStyle* aStyle,
                                nsPresContext* aPresContext)
      : nsMathMLContainerFrame(aStyle, aPresContext, kClassID) {}

  virtual ~nsMathMLmpaddedFrame();

  virtual nsresult MeasureForWidth(DrawTarget* aDrawTarget,
                                   ReflowOutput& aDesiredSize) override;

 private:
  struct Attribute {
    enum class Sign : uint8_t {
      Unspecified,
      Minus,
      Plus,
    };
    enum class PseudoUnit : uint8_t {
      Unspecified,
      ItSelf,
      Width,
      Height,
      Depth,
      NamedSpace,
    };
    nsCSSValue mValue;
    Sign mSign = Sign::Unspecified;
    PseudoUnit mPseudoUnit = PseudoUnit::Unspecified;
    bool mIsValid = false;
    void Reset() {
      mValue.Reset();
      mSign = Sign::Unspecified;
      mPseudoUnit = PseudoUnit::Unspecified;
      mIsValid = false;
    }
  };

  Attribute mWidth;
  Attribute mHeight;
  Attribute mDepth;
  Attribute mLeadingSpace;
  Attribute mVerticalOffset;

  // helpers to process the attributes
  void ProcessAttributes();

  void ParseAttribute(nsAtom* aAtom, Attribute& aAttribute);
  bool ParseAttribute(nsString& aString, Attribute& aAttribute);

  void UpdateValue(const Attribute& aAttribute, Attribute::PseudoUnit aSelfUnit,
                   const ReflowOutput& aDesiredSize, nscoord& aValueToUpdate,
                   float aFontSizeInflation) const;
};

#endif /* nsMathMLmpaddedFrame_h___ */
