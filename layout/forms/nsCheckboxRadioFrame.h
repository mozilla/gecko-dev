/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsCheckboxRadioFrame_h___
#define nsCheckboxRadioFrame_h___

#include "mozilla/Attributes.h"
#include "nsIFormControlFrame.h"
#include "nsAtomicContainerFrame.h"
#include "nsDisplayList.h"

/**
 * nsCheckboxRadioFrame is used for radio buttons and checkboxes.
 * It also has two static methods (RegUnRegAccessKey and
 * GetScreenHeight) that are used by other form controls.
 */
class nsCheckboxRadioFrame final : public nsAtomicContainerFrame,
                                   public nsIFormControlFrame {
 public:
  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS(nsCheckboxRadioFrame)

  explicit nsCheckboxRadioFrame(ComputedStyle* aStyle);

  // nsIFrame replacements
  virtual bool IsFrameOfType(uint32_t aFlags) const override {
    return nsAtomicContainerFrame::IsFrameOfType(
        aFlags & ~(nsIFrame::eReplaced | nsIFrame::eReplacedContainsBlock));
  }

  virtual void BuildDisplayList(nsDisplayListBuilder* aBuilder,
                                const nsDisplayListSet& aLists) override {
    DO_GLOBAL_REFLOW_COUNT_DSP("nsCheckboxRadioFrame");
    DisplayBorderBackgroundOutline(aBuilder, aLists);
  }

  /**
   * Both GetMinISize and GetPrefISize will return whatever GetIntrinsicISize
   * returns.
   */
  virtual nscoord GetMinISize(gfxContext* aRenderingContext) override;
  virtual nscoord GetPrefISize(gfxContext* aRenderingContext) override;

  /**
   * Our auto size is just intrinsic width and intrinsic height.
   */
  virtual mozilla::LogicalSize ComputeAutoSize(
      gfxContext* aRenderingContext, mozilla::WritingMode aWM,
      const mozilla::LogicalSize& aCBSize, nscoord aAvailableISize,
      const mozilla::LogicalSize& aMargin, const mozilla::LogicalSize& aBorder,
      const mozilla::LogicalSize& aPadding, ComputeSizeFlags aFlags) override;

  /**
   * Respond to a gui event
   * @see nsIFrame::HandleEvent
   */
  virtual nsresult HandleEvent(nsPresContext* aPresContext,
                               mozilla::WidgetGUIEvent* aEvent,
                               nsEventStatus* aEventStatus) override;

  virtual nscoord GetLogicalBaseline(
      mozilla::WritingMode aWritingMode) const override;

  /**
   * Respond to the request to resize and/or reflow
   * @see nsIFrame::Reflow
   */
  virtual void Reflow(nsPresContext* aCX, ReflowOutput& aDesiredSize,
                      const ReflowInput& aReflowInput,
                      nsReflowStatus& aStatus) override;

  virtual void DestroyFrom(nsIFrame* aDestructRoot,
                           PostDestroyData& aPostDestroyData) override;

  // new behavior

  virtual void SetFocus(bool aOn = true, bool aRepaint = false) override;

  // nsIFormControlFrame
  virtual nsresult SetFormProperty(nsAtom* aName,
                                   const nsAString& aValue) override;

  // AccessKey Helper function
  static nsresult RegUnRegAccessKey(nsIFrame* aFrame, bool aDoReg);

  /**
   * Returns the usable screen rect in app units, eg the rect where we can
   * draw dropdowns.
   */
  static nsRect GetUsableScreenRect(nsPresContext* aPresContext);

 protected:
  virtual ~nsCheckboxRadioFrame();

  static nscoord DefaultSize() {
    // XXXmats We have traditionally always returned 9px for GetMin/PrefISize
    // but we might want to factor in what the theme says, something like:
    // GetMinimumWidgetSize - GetWidgetPadding - GetWidgetBorder.
    return nsPresContext::CSSPixelsToAppUnits(9);
  }

  /**
   * Get the state of the checked attribute.
   * @param aState set to true if the checked attribute is set,
   * false if the checked attribute has been removed
   */
  void GetCurrentCheckState(bool* aState);
};

#endif
