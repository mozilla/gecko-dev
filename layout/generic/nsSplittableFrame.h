/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * base class for rendering objects that can be split across lines,
 * columns, or pages
 */

#ifndef nsSplittableFrame_h___
#define nsSplittableFrame_h___

#include "mozilla/Attributes.h"
#include "nsFrame.h"

// Derived class that allows splitting
class nsSplittableFrame : public nsFrame
{
public:
  NS_DECL_FRAMEARENA_HELPERS

  virtual void Init(nsIContent*       aContent,
                    nsContainerFrame* aParent,
                    nsIFrame*         aPrevInFlow) override;
  
  virtual nsSplittableType GetSplittableType() const override;

  virtual void DestroyFrom(nsIFrame* aDestructRoot) override;

  /*
   * Frame continuations can be either fluid or not:
   * Fluid continuations ("in-flows") are the result of line breaking, 
   * column breaking, or page breaking.
   * Other (non-fluid) continuations can be the result of BiDi frame splitting.
   * A "flow" is a chain of fluid continuations.
   */
  
  // Get the previous/next continuation, regardless of its type (fluid or non-fluid).
  virtual nsIFrame* GetPrevContinuation() const override;
  virtual nsIFrame* GetNextContinuation() const override;

  // Set a previous/next non-fluid continuation.
  virtual void SetPrevContinuation(nsIFrame*) override;
  virtual void SetNextContinuation(nsIFrame*) override;

  // Get the first/last continuation for this frame.
  virtual nsIFrame* FirstContinuation() const override;
  virtual nsIFrame* LastContinuation() const override;

#ifdef DEBUG
  // Can aFrame2 be reached from aFrame1 by following prev/next continuations?
  static bool IsInPrevContinuationChain(nsIFrame* aFrame1, nsIFrame* aFrame2);
  static bool IsInNextContinuationChain(nsIFrame* aFrame1, nsIFrame* aFrame2);
#endif
  
  // Get the previous/next continuation, only if it is fluid (an "in-flow").
  nsIFrame* GetPrevInFlow() const;
  nsIFrame* GetNextInFlow() const;

  virtual nsIFrame* GetPrevInFlowVirtual() const override { return GetPrevInFlow(); }
  virtual nsIFrame* GetNextInFlowVirtual() const override { return GetNextInFlow(); }
  
  // Set a previous/next fluid continuation.
  virtual void SetPrevInFlow(nsIFrame*) override;
  virtual void SetNextInFlow(nsIFrame*) override;

  // Get the first/last frame in the current flow.
  virtual nsIFrame* FirstInFlow() const override;
  virtual nsIFrame* LastInFlow() const override;

  // Remove the frame from the flow. Connects the frame's prev-in-flow
  // and its next-in-flow. This should only be called in frame Destroy() methods.
  static void RemoveFromFlow(nsIFrame* aFrame);

protected:
  explicit nsSplittableFrame(nsStyleContext* aContext) : nsFrame(aContext) {}

  /**
   * Determine the height consumed by our previous-in-flows.
   *
   * @note (bz) This makes laying out a splittable frame with N in-flows
   *       O(N^2)! So, use this function with caution and minimize the number
   *       of calls to this method.
   */
  nscoord GetConsumedBSize() const;

  /**
   * Retrieve the effective computed block size of this frame, which is the
   * computed block size, minus the block size consumed by any previous in-flows.
   */
  nscoord GetEffectiveComputedBSize(const nsHTMLReflowState& aReflowState,
                                    nscoord aConsumed = NS_INTRINSICSIZE) const;

  /**
   * @see nsIFrame::GetLogicalSkipSides()
   */
  virtual LogicalSides GetLogicalSkipSides(const nsHTMLReflowState* aReflowState = nullptr) const override;

#ifdef DEBUG
  virtual void DumpBaseRegressionData(nsPresContext* aPresContext, FILE* out, int32_t aIndent) override;
#endif

  nsIFrame*   mPrevContinuation;
  nsIFrame*   mNextContinuation;
};

#endif /* nsSplittableFrame_h___ */
