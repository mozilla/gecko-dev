/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* rendering object for CSS "display: ruby" */

#ifndef nsRubyFrame_h___
#define nsRubyFrame_h___

#include "nsInlineFrame.h"
#include "RubyUtils.h"

/**
 * Factory function.
 * @return a newly allocated nsRubyFrame (infallible)
 */
nsContainerFrame* NS_NewRubyFrame(nsIPresShell* aPresShell,
                                  mozilla::ComputedStyle* aStyle);

class nsRubyFrame final : public nsInlineFrame {
 public:
  NS_DECL_FRAMEARENA_HELPERS(nsRubyFrame)
  NS_DECL_QUERYFRAME

  // nsIFrame overrides
  virtual bool IsFrameOfType(uint32_t aFlags) const override;
  virtual void AddInlineMinISize(gfxContext* aRenderingContext,
                                 InlineMinISizeData* aData) override;
  virtual void AddInlinePrefISize(gfxContext* aRenderingContext,
                                  InlinePrefISizeData* aData) override;
  virtual void Reflow(nsPresContext* aPresContext, ReflowOutput& aDesiredSize,
                      const ReflowInput& aReflowInput,
                      nsReflowStatus& aStatus) override;

#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const override;
#endif

  mozilla::RubyBlockLeadings GetBlockLeadings() const { return mLeadings; }

 protected:
  friend nsContainerFrame* NS_NewRubyFrame(nsIPresShell* aPresShell,
                                           ComputedStyle* aStyle);
  explicit nsRubyFrame(ComputedStyle* aStyle)
      : nsInlineFrame(aStyle, kClassID) {}

  void ReflowSegment(nsPresContext* aPresContext,
                     const ReflowInput& aReflowInput,
                     nsRubyBaseContainerFrame* aBaseContainer,
                     nsReflowStatus& aStatus);

  nsRubyBaseContainerFrame* PullOneSegment(const nsLineLayout* aLineLayout,
                                           ContinuationTraversingState& aState);

  // The leadings required to put the annotations. They are dummy-
  // initialized to 0, and get meaningful values at first reflow.
  mozilla::RubyBlockLeadings mLeadings;
};

#endif /* nsRubyFrame_h___ */
