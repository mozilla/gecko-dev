/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* base class for rendering objects that do not have child lists */

#ifndef nsLeafFrame_h___
#define nsLeafFrame_h___

#include "mozilla/Attributes.h"
#include "nsIFrame.h"
#include "nsDisplayList.h"

/**
 * Abstract class that provides simple fixed-size layout for leaf objects.
 */
class nsLeafFrame : public nsIFrame {
 public:
  NS_DECL_ABSTRACT_FRAME(nsLeafFrame)

  // nsIFrame replacements
  void BuildDisplayList(nsDisplayListBuilder* aBuilder,
                        const nsDisplayListSet& aLists) override;

  nscoord IntrinsicISize(gfxContext* aContext,
                         mozilla::IntrinsicISizeType aType) override;

  /**
   * Our auto size is just the intrinsic size.
   */
  mozilla::LogicalSize ComputeAutoSize(
      gfxContext* aRenderingContext, mozilla::WritingMode aWM,
      const mozilla::LogicalSize& aCBSize, nscoord aAvailableISize,
      const mozilla::LogicalSize& aMargin,
      const mozilla::LogicalSize& aBorderPadding,
      const mozilla::StyleSizeOverrides& aSizeOverrides,
      mozilla::ComputeSizeFlags aFlags) override;

  /**
   * Each of our subclasses should provide its own Reflow impl:
   */
  void Reflow(nsPresContext* aPresContext, ReflowOutput& aDesiredSize,
              const ReflowInput& aReflowInput,
              nsReflowStatus& aStatus) override = 0;

 protected:
  nsLeafFrame(ComputedStyle* aStyle, nsPresContext* aPresContext, ClassID aID)
      : nsIFrame(aStyle, aPresContext, aID) {}

  virtual ~nsLeafFrame();
};

#endif /* nsLeafFrame_h___ */
