/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* rendering object that goes directly inside the document's scrollbars */

#ifndef nsCanvasFrame_h___
#define nsCanvasFrame_h___

#include "mozilla/Attributes.h"
#include "mozilla/EventForwards.h"
#include "nsContainerFrame.h"
#include "nsIScrollPositionListener.h"
#include "nsDisplayList.h"
#include "nsIAnonymousContentCreator.h"

class nsPresContext;
class nsRenderingContext;

/**
 * Root frame class.
 *
 * The root frame is the parent frame for the document element's frame.
 * It only supports having a single child frame which must be an area
 * frame.
 * @note nsCanvasFrame keeps overflow container continuations of its child
 * frame in the main child list.
 */
class nsCanvasFrame final : public nsContainerFrame,
                            public nsIScrollPositionListener,
                            public nsIAnonymousContentCreator
{
public:
  explicit nsCanvasFrame(nsStyleContext* aContext)
  : nsContainerFrame(aContext),
    mDoPaintFocus(false),
    mAddedScrollPositionListener(false) {}

  NS_DECL_QUERYFRAME_TARGET(nsCanvasFrame)
  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS


  virtual void DestroyFrom(nsIFrame* aDestructRoot) override;

  virtual mozilla::WritingMode GetWritingMode() const override
  {
    nsIContent* rootElem = GetContent();
    if (rootElem) {
      nsIFrame* rootElemFrame = rootElem->GetPrimaryFrame();
      if (rootElemFrame) {
        return rootElemFrame->GetWritingMode();
      }
    }
    return nsIFrame::GetWritingMode();
  }

#ifdef DEBUG
  virtual void SetInitialChildList(ChildListID     aListID,
                                   nsFrameList&    aChildList) override;
  virtual void AppendFrames(ChildListID     aListID,
                            nsFrameList&    aFrameList) override;
  virtual void InsertFrames(ChildListID     aListID,
                            nsIFrame*       aPrevFrame,
                            nsFrameList&    aFrameList) override;
  virtual void RemoveFrame(ChildListID     aListID,
                           nsIFrame*       aOldFrame) override;
#endif

  virtual nscoord GetMinISize(nsRenderingContext *aRenderingContext) override;
  virtual nscoord GetPrefISize(nsRenderingContext *aRenderingContext) override;
  virtual void Reflow(nsPresContext*           aPresContext,
                      ReflowOutput&     aDesiredSize,
                      const ReflowInput& aReflowInput,
                      nsReflowStatus&          aStatus) override;
  virtual bool IsFrameOfType(uint32_t aFlags) const override
  {
    return nsContainerFrame::IsFrameOfType(aFlags &
             ~(nsIFrame::eCanContainOverflowContainers));
  }

  // nsIAnonymousContentCreator
  virtual nsresult CreateAnonymousContent(nsTArray<ContentInfo>& aElements) override;
  virtual void AppendAnonymousContentTo(nsTArray<nsIContent*>& aElements, uint32_t aFilter) override;

  mozilla::dom::Element* GetCustomContentContainer() const
  {
    return mCustomContentContainer;
  }

  /**
   * Unhide the CustomContentContainer. This call only has an effect if
   * mCustomContentContainer is non-null.
   */
  void ShowCustomContentContainer();

  /**
   * Hide the CustomContentContainer. This call only has an effect if
   * mCustomContentContainer is non-null.
   */
  void HideCustomContentContainer();

  /** SetHasFocus tells the CanvasFrame to draw with focus ring
   *  @param aHasFocus true to show focus ring, false to hide it
   */
  NS_IMETHOD SetHasFocus(bool aHasFocus);

  virtual void BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                const nsRect&           aDirtyRect,
                                const nsDisplayListSet& aLists) override;

  void PaintFocus(mozilla::gfx::DrawTarget* aRenderingContext, nsPoint aPt);

  // nsIScrollPositionListener
  virtual void ScrollPositionWillChange(nscoord aX, nscoord aY) override;
  virtual void ScrollPositionDidChange(nscoord aX, nscoord aY) override {}

  /**
   * Get the "type" of the frame
   *
   * @see nsGkAtoms::canvasFrame
   */
  virtual nsIAtom* GetType() const override;

#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const override;
#endif
  virtual nsresult GetContentForEvent(mozilla::WidgetEvent* aEvent,
                                      nsIContent** aContent) override;

  nsRect CanvasArea() const;

protected:
  // Data members
  bool                      mDoPaintFocus;
  bool                      mAddedScrollPositionListener;

  nsCOMPtr<mozilla::dom::Element> mCustomContentContainer;
};

/**
 * Override nsDisplayBackground methods so that we pass aBGClipRect to
 * PaintBackground, covering the whole overflow area.
 * We can also paint an "extra background color" behind the normal
 * background.
 */
class nsDisplayCanvasBackgroundColor : public nsDisplaySolidColorBase {
public:
  nsDisplayCanvasBackgroundColor(nsDisplayListBuilder* aBuilder, nsIFrame *aFrame)
    : nsDisplaySolidColorBase(aBuilder, aFrame, NS_RGBA(0,0,0,0))
  {
  }

  virtual bool ComputeVisibility(nsDisplayListBuilder* aBuilder,
                                 nsRegion* aVisibleRegion) override
  {
    return NS_GET_A(mColor) > 0;
  }
  virtual nsRect GetBounds(nsDisplayListBuilder* aBuilder, bool* aSnap) override
  {
    nsCanvasFrame* frame = static_cast<nsCanvasFrame*>(mFrame);
    *aSnap = true;
    return frame->CanvasArea() + ToReferenceFrame();
  }
  virtual void HitTest(nsDisplayListBuilder* aBuilder, const nsRect& aRect,
                       HitTestState* aState, nsTArray<nsIFrame*> *aOutFrames) override
  {
    // We need to override so we don't consider border-radius.
    aOutFrames->AppendElement(mFrame);
  }

  virtual void Paint(nsDisplayListBuilder* aBuilder,
                     nsRenderingContext* aCtx) override;

  void SetExtraBackgroundColor(nscolor aColor)
  {
    mColor = aColor;
  }

  NS_DISPLAY_DECL_NAME("CanvasBackgroundColor", TYPE_CANVAS_BACKGROUND_COLOR)
#ifdef MOZ_DUMP_PAINTING
  virtual void WriteDebugInfo(std::stringstream& aStream) override;
#endif
};

class nsDisplayCanvasBackgroundImage : public nsDisplayBackgroundImage {
public:
  nsDisplayCanvasBackgroundImage(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                                 uint32_t aLayer, const nsStyleBackground* aBg)
    : nsDisplayBackgroundImage(aBuilder, aFrame, aLayer,
                               aFrame->GetRectRelativeToSelf() + aBuilder->ToReferenceFrame(aFrame),
                               aBg)
  {
    if (ShouldFixToViewport(aBuilder)) {
      mAnimatedGeometryRoot = aBuilder->FindAnimatedGeometryRootFor(this);
    }
  }

  virtual void Paint(nsDisplayListBuilder* aBuilder, nsRenderingContext* aCtx) override;

  virtual void NotifyRenderingChanged() override
  {
    mFrame->Properties().Delete(nsIFrame::CachedBackgroundImageDT());
  }

  virtual bool ShouldFixToViewport(nsDisplayListBuilder* aBuilder) override
  {
    // Put background-attachment:fixed canvas background images in their own
    // compositing layer. Since we know their background painting area can't
    // change (unless the viewport size itself changes), async scrolling
    // will work well.
    return ShouldTreatAsFixed() &&
           !mBackgroundStyle->mImage.mLayers[mLayer].mImage.IsEmpty();
  }
 
  // We still need to paint a background color as well as an image for this item, 
  // so we can't support this yet.
  virtual bool SupportsOptimizingToImage() override { return false; }

 bool IsSingleFixedPositionImage(nsDisplayListBuilder* aBuilder,
                                  const nsRect& aClipRect,
                                  gfxRect* aDestRect);
  
  
  NS_DISPLAY_DECL_NAME("CanvasBackgroundImage", TYPE_CANVAS_BACKGROUND_IMAGE)
};

class nsDisplayCanvasThemedBackground : public nsDisplayThemedBackground {
public:
  nsDisplayCanvasThemedBackground(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame)
    : nsDisplayThemedBackground(aBuilder, aFrame,
                                aFrame->GetRectRelativeToSelf() + aBuilder->ToReferenceFrame(aFrame))
  {}

  virtual void Paint(nsDisplayListBuilder* aBuilder, nsRenderingContext* aCtx) override;

  NS_DISPLAY_DECL_NAME("CanvasThemedBackground", TYPE_CANVAS_THEMED_BACKGROUND)
};

#endif /* nsCanvasFrame_h___ */
