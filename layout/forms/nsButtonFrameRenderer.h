/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsButtonFrameRenderer_h___
#define nsButtonFrameRenderer_h___

#include "imgIContainer.h"
#include "nsAutoPtr.h"
#include "nsMargin.h"

class nsIFrame;
class nsFrame;
class nsDisplayList;
class nsDisplayListBuilder;
class nsPresContext;
class nsRenderingContext;
struct nsRect;
class nsStyleContext;


#define NS_BUTTON_RENDERER_FOCUS_INNER_CONTEXT_INDEX  0
#define NS_BUTTON_RENDERER_FOCUS_OUTER_CONTEXT_INDEX  1
#define NS_BUTTON_RENDERER_LAST_CONTEXT_INDEX   NS_BUTTON_RENDERER_FOCUS_OUTER_CONTEXT_INDEX

class nsButtonFrameRenderer {
  typedef mozilla::image::DrawResult DrawResult;

public:

  nsButtonFrameRenderer();
  ~nsButtonFrameRenderer();

  /**
   * Create display list items for the button
   */
  nsresult DisplayButton(nsDisplayListBuilder* aBuilder,
                         nsDisplayList* aBackground, nsDisplayList* aForeground);


  void PaintOutlineAndFocusBorders(nsPresContext* aPresContext,
                                   nsRenderingContext& aRenderingContext,
                                   const nsRect& aDirtyRect,
                                   const nsRect& aRect);

  DrawResult PaintBorderAndBackground(nsPresContext* aPresContext,
                                      nsRenderingContext& aRenderingContext,
                                      const nsRect& aDirtyRect,
                                      const nsRect& aRect,
                                      uint32_t aBGFlags);

  void SetFrame(nsFrame* aFrame, nsPresContext* aPresContext);
 
  void SetDisabled(bool aDisabled, bool notify);

  bool isActive();
  bool isDisabled();

  void GetButtonOuterFocusRect(const nsRect& aRect, nsRect& aResult);
  void GetButtonRect(const nsRect& aRect, nsRect& aResult);
  void GetButtonInnerFocusRect(const nsRect& aRect, nsRect& aResult);
  nsMargin GetButtonOuterFocusBorderAndPadding();
  nsMargin GetButtonBorderAndPadding();
  nsMargin GetButtonInnerFocusMargin();
  nsMargin GetButtonInnerFocusBorderAndPadding();
  nsMargin GetAddedButtonBorderAndPadding();

  nsStyleContext* GetStyleContext(int32_t aIndex) const;
  void SetStyleContext(int32_t aIndex, nsStyleContext* aStyleContext);
  void ReResolveStyles(nsPresContext* aPresContext);

  nsIFrame* GetFrame();

protected:

private:

  // cached styles for focus and outline.
  nsRefPtr<nsStyleContext> mInnerFocusStyle;
  nsRefPtr<nsStyleContext> mOuterFocusStyle;

  nsFrame* mFrame;
};


#endif

