/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* rendering object to wrap rendering objects that should be scrollable */

#ifndef nsGfxScrollFrame_h___
#define nsGfxScrollFrame_h___

#include "mozilla/Attributes.h"
#include "nsContainerFrame.h"
#include "nsIAnonymousContentCreator.h"
#include "nsBoxFrame.h"
#include "nsIScrollableFrame.h"
#include "nsIScrollbarMediator.h"
#include "nsIStatefulFrame.h"
#include "nsThreadUtils.h"
#include "nsIReflowCallback.h"
#include "nsBoxLayoutState.h"
#include "nsQueryFrame.h"
#include "nsExpirationTracker.h"

class nsPresContext;
class nsIPresShell;
class nsIContent;
class nsIAtom;
class nsIScrollFrameInternal;
class nsPresState;
class nsIScrollPositionListener;
struct ScrollReflowState;

namespace mozilla {
namespace layers {
class Layer;
}
namespace layout {
class ScrollbarActivity;
}
}

namespace mozilla {

class ScrollFrameHelper : public nsIReflowCallback {
public:
  typedef nsIFrame::Sides Sides;
  typedef mozilla::CSSIntPoint CSSIntPoint;
  typedef mozilla::layout::ScrollbarActivity ScrollbarActivity;
  typedef mozilla::layers::FrameMetrics FrameMetrics;
  typedef mozilla::layers::Layer Layer;

  class AsyncScroll;
  class AsyncSmoothMSDScroll;

  ScrollFrameHelper(nsContainerFrame* aOuter, bool aIsRoot);
  ~ScrollFrameHelper();

  mozilla::ScrollbarStyles GetScrollbarStylesFromFrame() const;

  // If a child frame was added or removed on the scrollframe,
  // reload our child frame list.
  // We need this if a scrollbar frame is recreated.
  void ReloadChildFrames();

  nsresult CreateAnonymousContent(
    nsTArray<nsIAnonymousContentCreator::ContentInfo>& aElements);
  void AppendAnonymousContentTo(nsTArray<nsIContent*>& aElements, uint32_t aFilter);
  nsresult FireScrollPortEvent();
  void PostOverflowEvent();
  void Destroy();

  void BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                        const nsRect&           aDirtyRect,
                        const nsDisplayListSet& aLists);

  void AppendScrollPartsTo(nsDisplayListBuilder*   aBuilder,
                           const nsRect&           aDirtyRect,
                           const nsDisplayListSet& aLists,
                           bool                    aUsingDisplayPort,
                           bool                    aCreateLayer,
                           bool                    aPositioned);

  bool GetBorderRadii(const nsSize& aFrameSize, const nsSize& aBorderArea,
                      Sides aSkipSides, nscoord aRadii[8]) const;

  // nsIReflowCallback
  virtual bool ReflowFinished() MOZ_OVERRIDE;
  virtual void ReflowCallbackCanceled() MOZ_OVERRIDE;

  /**
   * @note This method might destroy the frame, pres shell and other objects.
   * Called when the 'curpos' attribute on one of the scrollbars changes.
   */
  void CurPosAttributeChanged(nsIContent* aChild);

  void PostScrollEvent();
  void FireScrollEvent();
  void PostScrolledAreaEvent();
  void FireScrolledAreaEvent();

  bool IsSmoothScrollingEnabled();

  class ScrollEvent : public nsRunnable {
  public:
    NS_DECL_NSIRUNNABLE
    explicit ScrollEvent(ScrollFrameHelper *helper) : mHelper(helper) {}
    void Revoke() { mHelper = nullptr; }
  private:
    ScrollFrameHelper *mHelper;
  };

  class AsyncScrollPortEvent : public nsRunnable {
  public:
    NS_DECL_NSIRUNNABLE
    explicit AsyncScrollPortEvent(ScrollFrameHelper *helper) : mHelper(helper) {}
    void Revoke() { mHelper = nullptr; }
  private:
    ScrollFrameHelper *mHelper;
  };

  class ScrolledAreaEvent : public nsRunnable {
  public:
    NS_DECL_NSIRUNNABLE
    explicit ScrolledAreaEvent(ScrollFrameHelper *helper) : mHelper(helper) {}
    void Revoke() { mHelper = nullptr; }
  private:
    ScrollFrameHelper *mHelper;
  };

  class VelocityQueue MOZ_FINAL {
  public:
    VelocityQueue(nsPresContext *aPresContext)
      : mPresContext(aPresContext) {}

    ~VelocityQueue() {}

    // Sample() is to be called periodically when scroll movement occurs.
    // It takes scroll position samples used by GetVelocity()
    //
    // Using the last iteration's scroll position, stored in
    // mScrollVelocityLastPosition, a delta of the scroll position is calculated
    // and accumulated in mScrollVelocityAccumulator until the refresh driver
    // returns a new timestamp for MostRecentRefresh().
    //
    // When there is a new timestamp from the refresh driver, the accumulated
    // change in scroll position is divided by the delta of the timestamp to
    // get an average velocity over that period.  This velocity is pushed into
    // mScrollVelocityQueue as a std::pair associating each velocity with the
    // duration over which it was sampled.
    //
    // Samples are removed from mScrollVelocityQueue, leaving only those necessary
    // to determine the average velocity over the recent relevant period, which
    // has a duration set by the apz.max_velocity_queue_size preference.
    //
    // The velocity of each sample is clamped to a value set by the
    // layout.css.scroll-snap.prediction-max-velocity.
    //
    // As the average velocity will later be integrated over a duration set by
    // the layout.css.scroll-snap.prediction-sensitivity preference and the
    // velocity samples are clamped to a set value, the maximum expected
    // scroll offset can be calculated.  This maximum offset is used to clamp
    // mScrollVelocityAccumulator, eliminating samples that would otherwise
    // result in scroll snap position selection that is not consistent with the
    // user's perception of scroll velocity.
    void Sample(const nsPoint& aScrollPosition);

    // Discards velocity samples, resulting in velocity of 0 returned by
    // GetVelocity until move scroll position updates.
    void Reset();

    // Get scroll velocity averaged from recent movement, in appunits / ms
    nsPoint GetVelocity();
  private:
    // A queue of (timestamp, velocity) pairs; these are the historical
    // velocities at the given timestamps. Timestamps are in milliseconds,
    // velocities are in app units per ms.
    nsTArray<std::pair<uint32_t, nsPoint> > mScrollVelocityQueue;

    // Accumulates the distance and direction travelled by the scroll frame since
    // mScrollVelocitySampleTime.
    nsPoint mScrollVelocityAccumulator;

    // Time that mScrollVelocityAccumulator was last reset and began accumulating.
    TimeStamp mScrollVelocitySampleTime;

    // Scroll offset at the mScrollVelocityAccumulator was last reset and began
    // accumulating.
    nsPoint mScrollVelocityLastPosition;

    // PresContext of the containing frame, used to get timebase
    nsPresContext* mPresContext;
  };

  /**
   * @note This method might destroy the frame, pres shell and other objects.
   */
  void FinishReflowForScrollbar(nsIContent* aContent, nscoord aMinXY,
                                nscoord aMaxXY, nscoord aCurPosXY,
                                nscoord aPageIncrement,
                                nscoord aIncrement);
  /**
   * @note This method might destroy the frame, pres shell and other objects.
   */
  void SetScrollbarEnabled(nsIContent* aContent, nscoord aMaxPos);
  /**
   * @note This method might destroy the frame, pres shell and other objects.
   */
  void SetCoordAttribute(nsIContent* aContent, nsIAtom* aAtom, nscoord aSize);

  nscoord GetCoordAttribute(nsIFrame* aFrame, nsIAtom* aAtom, nscoord aDefaultValue,
                            nscoord* aRangeStart, nscoord* aRangeLength);

  /**
   * @note This method might destroy the frame, pres shell and other objects.
   * Update scrollbar curpos attributes to reflect current scroll position
   */
  void UpdateScrollbarPosition();

  nsRect GetScrollPortRect() const { return mScrollPort; }
  nsPoint GetScrollPosition() const {
    return mScrollPort.TopLeft() - mScrolledFrame->GetPosition();
  }
  /**
   * For LTR frames, the logical scroll position is the offset of the top left
   * corner of the frame from the top left corner of the scroll port (same as
   * GetScrollPosition).
   * For RTL frames, it is the offset of the top right corner of the frame from
   * the top right corner of the scroll port
   */
  nsPoint GetLogicalScrollPosition() const {
    nsPoint pt;
    pt.x = IsLTR() ?
      mScrollPort.x - mScrolledFrame->GetPosition().x :
      mScrollPort.XMost() - mScrolledFrame->GetRect().XMost();
    pt.y = mScrollPort.y - mScrolledFrame->GetPosition().y;
    return pt;
  }
  nsRect GetScrollRange() const;
  // Get the scroll range assuming the scrollport has size (aWidth, aHeight).
  nsRect GetScrollRange(nscoord aWidth, nscoord aHeight) const;
  nsSize GetScrollPositionClampingScrollPortSize() const;
  gfxSize GetResolution() const;
  void SetResolution(const gfxSize& aResolution);
  void SetResolutionAndScaleTo(const gfxSize& aResolution);
  void FlingSnap(const mozilla::CSSPoint& aDestination,
                 uint32_t aFlingSnapGeneration);
  void ScrollSnap();
  void ScrollSnap(const nsPoint &aDestination);

protected:
  nsRect GetScrollRangeForClamping() const;

public:
  static void AsyncScrollCallback(ScrollFrameHelper* aInstance,
                                  mozilla::TimeStamp aTime);
  static void AsyncSmoothMSDScrollCallback(ScrollFrameHelper* aInstance,
                                           mozilla::TimeDuration aDeltaTime);
  /**
   * @note This method might destroy the frame, pres shell and other objects.
   * aRange is the range of allowable scroll positions around the desired
   * aScrollPosition. Null means only aScrollPosition is allowed.
   * This is a closed-ended range --- aRange.XMost()/aRange.YMost() are allowed.
   */
  void ScrollTo(nsPoint aScrollPosition, nsIScrollableFrame::ScrollMode aMode,
                const nsRect* aRange = nullptr,
                nsIScrollableFrame::ScrollSnapMode aSnap = nsIScrollableFrame::DISABLE_SNAP) {
    ScrollToWithOrigin(aScrollPosition, aMode, nsGkAtoms::other, aRange,
                       aSnap);
  }
  /**
   * @note This method might destroy the frame, pres shell and other objects.
   */
  void ScrollToCSSPixels(const CSSIntPoint& aScrollPosition,
                         nsIScrollableFrame::ScrollMode aMode
                           = nsIScrollableFrame::INSTANT);
  /**
   * @note This method might destroy the frame, pres shell and other objects.
   */
  void ScrollToCSSPixelsApproximate(const mozilla::CSSPoint& aScrollPosition,
                                    nsIAtom* aOrigin = nullptr);

  CSSIntPoint GetScrollPositionCSSPixels();
  /**
   * @note This method might destroy the frame, pres shell and other objects.
   */
  void ScrollToImpl(nsPoint aScrollPosition, const nsRect& aRange, nsIAtom* aOrigin = nullptr);
  void ScrollVisual(nsPoint aOldScrolledFramePosition);
  /**
   * @note This method might destroy the frame, pres shell and other objects.
   */
  void ScrollBy(nsIntPoint aDelta, nsIScrollableFrame::ScrollUnit aUnit,
                nsIScrollableFrame::ScrollMode aMode, nsIntPoint* aOverflow,
                nsIAtom* aOrigin = nullptr,
                nsIScrollableFrame::ScrollMomentum aIsMomentum = nsIScrollableFrame::NOT_MOMENTUM,
                nsIScrollableFrame::ScrollSnapMode aSnap = nsIScrollableFrame::DISABLE_SNAP);
  /**
   * @note This method might destroy the frame, pres shell and other objects.
   */
  void ScrollToRestoredPosition();
  /**
   * GetSnapPointForDestination determines which point to snap to after
   * scrolling. aCurrPos gives the position before scrolling and aDestination
   * gives the position after scrolling, with no snapping. Behaviour is
   * dependent on the value of aUnit.
   */
  bool GetSnapPointForDestination(nsIScrollableFrame::ScrollUnit aUnit,
                                  nsPoint aStartPos,
                                  nsPoint &aDestination);

  nsSize GetLineScrollAmount() const;
  nsSize GetPageScrollAmount() const;

  nsPresState* SaveState() const;
  void RestoreState(nsPresState* aState);

  nsIFrame* GetScrolledFrame() const { return mScrolledFrame; }
  nsIFrame* GetScrollbarBox(bool aVertical) const {
    return aVertical ? mVScrollbarBox : mHScrollbarBox;
  }

  void AddScrollPositionListener(nsIScrollPositionListener* aListener) {
    mListeners.AppendElement(aListener);
  }
  void RemoveScrollPositionListener(nsIScrollPositionListener* aListener) {
    mListeners.RemoveElement(aListener);
  }

  static void SetScrollbarVisibility(nsIFrame* aScrollbar, bool aVisible);

  /**
   * GetScrolledRect is designed to encapsulate deciding which
   * directions of overflow should be reachable by scrolling and which
   * should not.  Callers should NOT depend on it having any particular
   * behavior (although nsXULScrollFrame currently does).
   * 
   * This should only be called when the scrolled frame has been
   * reflowed with the scroll port size given in mScrollPort.
   *
   * Currently it allows scrolling down and to the right for
   * nsHTMLScrollFrames with LTR directionality and for all
   * nsXULScrollFrames, and allows scrolling down and to the left for
   * nsHTMLScrollFrames with RTL directionality.
   */
  nsRect GetScrolledRect() const;

  /**
   * GetScrolledRectInternal is designed to encapsulate deciding which
   * directions of overflow should be reachable by scrolling and which
   * should not.  Callers should NOT depend on it having any particular
   * behavior (although nsXULScrollFrame currently does).
   * 
   * Currently it allows scrolling down and to the right for
   * nsHTMLScrollFrames with LTR directionality and for all
   * nsXULScrollFrames, and allows scrolling down and to the left for
   * nsHTMLScrollFrames with RTL directionality.
   */
  nsRect GetScrolledRectInternal(const nsRect& aScrolledOverflowArea,
                                 const nsSize& aScrollPortSize) const;

  uint32_t GetScrollbarVisibility() const {
    return (mHasVerticalScrollbar ? nsIScrollableFrame::VERTICAL : 0) |
           (mHasHorizontalScrollbar ? nsIScrollableFrame::HORIZONTAL : 0);
  }
  nsMargin GetActualScrollbarSizes() const;
  nsMargin GetDesiredScrollbarSizes(nsBoxLayoutState* aState);
  nscoord GetNondisappearingScrollbarWidth(nsBoxLayoutState* aState);
  bool IsLTR() const;
  bool IsScrollbarOnRight() const;
  bool IsScrollingActive(nsDisplayListBuilder* aBuilder) const;
  bool IsMaybeScrollingActive() const;
  bool IsProcessingAsyncScroll() const {
    return mAsyncScroll != nullptr || mAsyncSmoothMSDScroll != nullptr;
  }
  void ResetScrollPositionForLayerPixelAlignment()
  {
    mScrollPosForLayerPixelAlignment = GetScrollPosition();
  }

  bool UpdateOverflow();

  void UpdateSticky();

  bool IsRectNearlyVisible(const nsRect& aRect) const;
  nsRect ExpandRectToNearlyVisible(const nsRect& aRect) const;

  // adjust the scrollbar rectangle aRect to account for any visible resizer.
  // aHasResizer specifies if there is a content resizer, however this method
  // will also check if a widget resizer is present as well.
  void AdjustScrollbarRectForResizer(nsIFrame* aFrame, nsPresContext* aPresContext,
                                     nsRect& aRect, bool aHasResizer, bool aVertical);
  // returns true if a resizer should be visible
  bool HasResizer() { return mResizerBox && !mCollapsedResizer; }
  void LayoutScrollbars(nsBoxLayoutState& aState,
                        const nsRect& aContentArea,
                        const nsRect& aOldScrollArea);

  bool IsIgnoringViewportClipping() const;

  void MarkScrollbarsDirtyForReflow() const;

  bool ShouldClampScrollPosition() const;

  bool IsAlwaysActive() const;
  void MarkRecentlyScrolled();
  void MarkNotRecentlyScrolled();
  nsExpirationState* GetExpirationState() { return &mActivityExpirationState; }

  void ScheduleSyntheticMouseMove();
  static void ScrollActivityCallback(nsITimer *aTimer, void* anInstance);

  void HandleScrollbarStyleSwitching();

  nsIAtom* LastScrollOrigin() const { return mLastScrollOrigin; }
  nsIAtom* LastSmoothScrollOrigin() const { return mLastSmoothScrollOrigin; }
  uint32_t CurrentScrollGeneration() const { return mScrollGeneration; }
  uint32_t CurrentFlingSnapGeneration() const { return mCurrentFlingSnapGeneration; }
  nsPoint LastScrollDestination() const { return mDestination; }
  void ResetScrollInfoIfGeneration(uint32_t aGeneration) {
    if (aGeneration == mScrollGeneration) {
      mLastScrollOrigin = nullptr;
      mLastSmoothScrollOrigin = nullptr;
    }
  }
  bool WantAsyncScroll() const;
  void ComputeFrameMetrics(Layer* aLayer, nsIFrame* aContainerReferenceFrame,
                           const ContainerLayerParameters& aParameters,
                           nsRect* aClipRect,
                           nsTArray<FrameMetrics>* aOutput) const;

  // nsIScrollbarMediator
  void ScrollByPage(nsScrollbarFrame* aScrollbar, int32_t aDirection);
  void ScrollByWhole(nsScrollbarFrame* aScrollbar, int32_t aDirection);
  void ScrollByLine(nsScrollbarFrame* aScrollbar, int32_t aDirection);
  void RepeatButtonScroll(nsScrollbarFrame* aScrollbar);
  void ThumbMoved(nsScrollbarFrame* aScrollbar,
                  nscoord aOldPos,
                  nscoord aNewPos);
  void ScrollByUnit(nsScrollbarFrame* aScrollbar,
                    nsIScrollableFrame::ScrollMode aMode,
                    int32_t aDirection,
                    nsIScrollableFrame::ScrollUnit aUnit);

  // owning references to the nsIAnonymousContentCreator-built content
  nsCOMPtr<nsIContent> mHScrollbarContent;
  nsCOMPtr<nsIContent> mVScrollbarContent;
  nsCOMPtr<nsIContent> mScrollCornerContent;
  nsCOMPtr<nsIContent> mResizerContent;

  nsRevocableEventPtr<ScrollEvent> mScrollEvent;
  nsRevocableEventPtr<AsyncScrollPortEvent> mAsyncScrollPortEvent;
  nsRevocableEventPtr<ScrolledAreaEvent> mScrolledAreaEvent;
  nsIFrame* mHScrollbarBox;
  nsIFrame* mVScrollbarBox;
  nsIFrame* mScrolledFrame;
  nsIFrame* mScrollCornerBox;
  nsIFrame* mResizerBox;
  nsContainerFrame* mOuter;
  nsRefPtr<AsyncScroll> mAsyncScroll;
  nsRefPtr<AsyncSmoothMSDScroll> mAsyncSmoothMSDScroll;
  nsRefPtr<ScrollbarActivity> mScrollbarActivity;
  nsTArray<nsIScrollPositionListener*> mListeners;
  nsIAtom* mLastScrollOrigin;
  nsIAtom* mLastSmoothScrollOrigin;
  uint32_t mScrollGeneration;
  uint32_t mCurrentFlingSnapGeneration;
  nsRect mScrollPort;
  // Where we're currently scrolling to, if we're scrolling asynchronously.
  // If we're not in the middle of an asynchronous scroll then this is
  // just the current scroll position. ScrollBy will choose its
  // destination based on this value.
  nsPoint mDestination;
  nsPoint mScrollPosAtLastPaint;

  // A goal position to try to scroll to as content loads. As long as mLastPos
  // matches the current logical scroll position, we try to scroll to mRestorePos
  // after every reflow --- because after each time content is loaded/added to the
  // scrollable element, there will be a reflow.
  nsPoint mRestorePos;
  // The last logical position we scrolled to while trying to restore mRestorePos, or
  // 0,0 when this is a new frame. Set to -1,-1 once we've scrolled for any reason
  // other than trying to restore mRestorePos.
  nsPoint mLastPos;

  // The current resolution derived from the zoom level and device pixel ratio.
  gfxSize mResolution;

  nsExpirationState mActivityExpirationState;

  nsCOMPtr<nsITimer> mScrollActivityTimer;
  nsPoint mScrollPosForLayerPixelAlignment;

  // The scroll position where we last updated image visibility.
  nsPoint mLastUpdateImagesPos;

  FrameMetrics::ViewID mScrollParentID;

  bool mNeverHasVerticalScrollbar:1;
  bool mNeverHasHorizontalScrollbar:1;
  bool mHasVerticalScrollbar:1;
  bool mHasHorizontalScrollbar:1;
  bool mFrameIsUpdatingScrollbar:1;
  bool mDidHistoryRestore:1;
  // Is this the scrollframe for the document's viewport?
  bool mIsRoot:1;
  // True if we should clip all descendants, false if we should only clip
  // descendants for which we are the containing block.
  bool mClipAllDescendants:1;
  // If true, don't try to layout the scrollbars in Reflow().  This can be
  // useful if multiple passes are involved, because we don't want to place the
  // scrollbars at the wrong size.
  bool mSupppressScrollbarUpdate:1;
  // If true, we skipped a scrollbar layout due to mSupppressScrollbarUpdate
  // being set at some point.  That means we should lay out scrollbars even if
  // it might not strictly be needed next time mSupppressScrollbarUpdate is
  // false.
  bool mSkippedScrollbarLayout:1;

  bool mHadNonInitialReflow:1;
  // State used only by PostScrollEvents so we know
  // which overflow states have changed.
  bool mHorizontalOverflow:1;
  bool mVerticalOverflow:1;
  bool mPostedReflowCallback:1;
  bool mMayHaveDirtyFixedChildren:1;
  // If true, need to actually update our scrollbar attributes in the
  // reflow callback.
  bool mUpdateScrollbarAttributes:1;
  // If true, we should be prepared to scroll using this scrollframe
  // by placing descendant content into its own layer(s)
  bool mHasBeenScrolledRecently:1;
  // If true, the resizer is collapsed and not displayed
  bool mCollapsedResizer:1;

  // If true, the layer should always be active because we always build a
  // scrollable layer. Used for asynchronous scrolling.
  bool mShouldBuildScrollableLayer:1;

  // If true, add clipping in ScrollFrameHelper::ComputeFrameMetrics.
  bool mAddClipRectToLayer:1;

  // True if this frame has been scrolled at least once
  bool mHasBeenScrolled:1;

  // True if the frame's resolution has been set via SetResolution or
  // SetResolutionAndScaleTo or restored via RestoreState.
  bool mIsResolutionSet:1;

  // True if the events synthesized by OSX to produce momentum scrolling should
  // be ignored.  Reset when the next real, non-synthesized scroll event occurs.
  bool mIgnoreMomentumScroll:1;

  // True if the frame's resolution has been set via SetResolutionAndScaleTo.
  // Only meaningful for root scroll frames.
  bool mScaleToResolution:1;

  VelocityQueue mVelocityQueue;

protected:
  /**
   * @note This method might destroy the frame, pres shell and other objects.
   */
  void ScrollToWithOrigin(nsPoint aScrollPosition,
                          nsIScrollableFrame::ScrollMode aMode,
                          nsIAtom *aOrigin, // nullptr indicates "other" origin
                          const nsRect* aRange,
                          nsIScrollableFrame::ScrollSnapMode aSnap = nsIScrollableFrame::DISABLE_SNAP);

  void CompleteAsyncScroll(const nsRect &aRange, nsIAtom* aOrigin = nullptr);

  static void EnsureImageVisPrefsCached();
  static bool sImageVisPrefsCached;
  // The number of scrollports wide/high to expand when looking for images.
  static uint32_t sHorzExpandScrollPort;
  static uint32_t sVertExpandScrollPort;
  // The fraction of the scrollport we allow to scroll by before we schedule
  // an update of image visibility.
  static int32_t sHorzScrollFraction;
  static int32_t sVertScrollFraction;
};

}

/**
 * The scroll frame creates and manages the scrolling view
 *
 * It only supports having a single child frame that typically is an area
 * frame, but doesn't have to be. The child frame must have a view, though
 *
 * Scroll frames don't support incremental changes, i.e. you can't replace
 * or remove the scrolled frame
 */
class nsHTMLScrollFrame : public nsContainerFrame,
                          public nsIScrollableFrame,
                          public nsIAnonymousContentCreator,
                          public nsIStatefulFrame {
public:
  typedef mozilla::ScrollFrameHelper ScrollFrameHelper;
  typedef mozilla::CSSIntPoint CSSIntPoint;
  friend nsHTMLScrollFrame* NS_NewHTMLScrollFrame(nsIPresShell* aPresShell,
                                                  nsStyleContext* aContext,
                                                  bool aIsRoot);

  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS

  virtual mozilla::WritingMode GetWritingMode() const MOZ_OVERRIDE
  {
    if (mHelper.mScrolledFrame) {
      return mHelper.mScrolledFrame->GetWritingMode();
    }
    return nsIFrame::GetWritingMode();
  }

  virtual void BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                const nsRect&           aDirtyRect,
                                const nsDisplayListSet& aLists) MOZ_OVERRIDE {
    mHelper.BuildDisplayList(aBuilder, aDirtyRect, aLists);
  }

  bool TryLayout(ScrollReflowState* aState,
                   nsHTMLReflowMetrics* aKidMetrics,
                   bool aAssumeVScroll, bool aAssumeHScroll,
                   bool aForce);
  bool ScrolledContentDependsOnHeight(ScrollReflowState* aState);
  void ReflowScrolledFrame(ScrollReflowState* aState,
                           bool aAssumeHScroll,
                           bool aAssumeVScroll,
                           nsHTMLReflowMetrics* aMetrics,
                           bool aFirstPass);
  void ReflowContents(ScrollReflowState* aState,
                      const nsHTMLReflowMetrics& aDesiredSize);
  void PlaceScrollArea(const ScrollReflowState& aState,
                       const nsPoint& aScrollPosition);
  nscoord GetIntrinsicVScrollbarWidth(nsRenderingContext *aRenderingContext);

  virtual bool GetBorderRadii(const nsSize& aFrameSize, const nsSize& aBorderArea,
                              Sides aSkipSides, nscoord aRadii[8]) const MOZ_OVERRIDE {
    return mHelper.GetBorderRadii(aFrameSize, aBorderArea, aSkipSides, aRadii);
  }

  virtual nscoord GetMinISize(nsRenderingContext *aRenderingContext) MOZ_OVERRIDE;
  virtual nscoord GetPrefISize(nsRenderingContext *aRenderingContext) MOZ_OVERRIDE;
  virtual nsresult GetPadding(nsMargin& aPadding) MOZ_OVERRIDE;
  virtual bool IsCollapsed() MOZ_OVERRIDE;
  
  virtual void Reflow(nsPresContext*           aPresContext,
                      nsHTMLReflowMetrics&     aDesiredSize,
                      const nsHTMLReflowState& aReflowState,
                      nsReflowStatus&          aStatus) MOZ_OVERRIDE;

  virtual bool UpdateOverflow() MOZ_OVERRIDE {
    return mHelper.UpdateOverflow();
  }

  // Called to set the child frames. We typically have three: the scroll area,
  // the vertical scrollbar, and the horizontal scrollbar.
  virtual void SetInitialChildList(ChildListID     aListID,
                                   nsFrameList&    aChildList) MOZ_OVERRIDE;
  virtual void AppendFrames(ChildListID     aListID,
                            nsFrameList&    aFrameList) MOZ_OVERRIDE;
  virtual void InsertFrames(ChildListID     aListID,
                            nsIFrame*       aPrevFrame,
                            nsFrameList&    aFrameList) MOZ_OVERRIDE;
  virtual void RemoveFrame(ChildListID     aListID,
                           nsIFrame*       aOldFrame) MOZ_OVERRIDE;

  virtual void DestroyFrom(nsIFrame* aDestructRoot) MOZ_OVERRIDE;

  virtual nsIScrollableFrame* GetScrollTargetFrame() MOZ_OVERRIDE {
    return this;
  }

  virtual nsContainerFrame* GetContentInsertionFrame() MOZ_OVERRIDE {
    return mHelper.GetScrolledFrame()->GetContentInsertionFrame();
  }

  virtual bool DoesClipChildren() MOZ_OVERRIDE { return true; }
  virtual nsSplittableType GetSplittableType() const MOZ_OVERRIDE;

  virtual nsPoint GetPositionOfChildIgnoringScrolling(nsIFrame* aChild) MOZ_OVERRIDE
  { nsPoint pt = aChild->GetPosition();
    if (aChild == mHelper.GetScrolledFrame()) pt += GetScrollPosition();
    return pt;
  }

  // nsIAnonymousContentCreator
  virtual nsresult CreateAnonymousContent(nsTArray<ContentInfo>& aElements) MOZ_OVERRIDE;
  virtual void AppendAnonymousContentTo(nsTArray<nsIContent*>& aElements,
                                        uint32_t aFilter) MOZ_OVERRIDE;

  // nsIScrollableFrame
  virtual nsIFrame* GetScrolledFrame() const MOZ_OVERRIDE {
    return mHelper.GetScrolledFrame();
  }
  virtual mozilla::ScrollbarStyles GetScrollbarStyles() const MOZ_OVERRIDE {
    return mHelper.GetScrollbarStylesFromFrame();
  }
  virtual uint32_t GetScrollbarVisibility() const MOZ_OVERRIDE {
    return mHelper.GetScrollbarVisibility();
  }
  virtual nsMargin GetActualScrollbarSizes() const MOZ_OVERRIDE {
    return mHelper.GetActualScrollbarSizes();
  }
  virtual nsMargin GetDesiredScrollbarSizes(nsBoxLayoutState* aState) MOZ_OVERRIDE {
    return mHelper.GetDesiredScrollbarSizes(aState);
  }
  virtual nsMargin GetDesiredScrollbarSizes(nsPresContext* aPresContext,
          nsRenderingContext* aRC) MOZ_OVERRIDE {
    nsBoxLayoutState bls(aPresContext, aRC, 0);
    return GetDesiredScrollbarSizes(&bls);
  }
  virtual nscoord GetNondisappearingScrollbarWidth(nsPresContext* aPresContext,
          nsRenderingContext* aRC) MOZ_OVERRIDE {
    nsBoxLayoutState bls(aPresContext, aRC, 0);
    return mHelper.GetNondisappearingScrollbarWidth(&bls);
  }
  virtual nsRect GetScrolledRect() const MOZ_OVERRIDE {
    return mHelper.GetScrolledRect();
  }
  virtual nsRect GetScrollPortRect() const MOZ_OVERRIDE {
    return mHelper.GetScrollPortRect();
  }
  virtual nsPoint GetScrollPosition() const MOZ_OVERRIDE {
    return mHelper.GetScrollPosition();
  }
  virtual nsPoint GetLogicalScrollPosition() const MOZ_OVERRIDE {
    return mHelper.GetLogicalScrollPosition();
  }
  virtual nsRect GetScrollRange() const MOZ_OVERRIDE {
    return mHelper.GetScrollRange();
  }
  virtual nsSize GetScrollPositionClampingScrollPortSize() const MOZ_OVERRIDE {
    return mHelper.GetScrollPositionClampingScrollPortSize();
  }
  virtual gfxSize GetResolution() const MOZ_OVERRIDE {
    return mHelper.GetResolution();
  }
  virtual void SetResolution(const gfxSize& aResolution) MOZ_OVERRIDE {
    return mHelper.SetResolution(aResolution);
  }
  virtual void SetResolutionAndScaleTo(const gfxSize& aResolution) MOZ_OVERRIDE {
    return mHelper.SetResolutionAndScaleTo(aResolution);
  }
  virtual nsSize GetLineScrollAmount() const MOZ_OVERRIDE {
    return mHelper.GetLineScrollAmount();
  }
  virtual nsSize GetPageScrollAmount() const MOZ_OVERRIDE {
    return mHelper.GetPageScrollAmount();
  }
  /**
   * @note This method might destroy the frame, pres shell and other objects.
   */
  virtual void ScrollTo(nsPoint aScrollPosition, ScrollMode aMode,
                        const nsRect* aRange = nullptr,
                        nsIScrollableFrame::ScrollSnapMode aSnap = nsIScrollableFrame::DISABLE_SNAP)
                        MOZ_OVERRIDE {
    mHelper.ScrollTo(aScrollPosition, aMode, aRange, aSnap);
  }
  /**
   * @note This method might destroy the frame, pres shell and other objects.
   */
  virtual void ScrollToCSSPixels(const CSSIntPoint& aScrollPosition,
                                 nsIScrollableFrame::ScrollMode aMode
                                   = nsIScrollableFrame::INSTANT) MOZ_OVERRIDE {
    mHelper.ScrollToCSSPixels(aScrollPosition, aMode);
  }
  virtual void ScrollToCSSPixelsApproximate(const mozilla::CSSPoint& aScrollPosition,
                                            nsIAtom* aOrigin = nullptr) MOZ_OVERRIDE {
    mHelper.ScrollToCSSPixelsApproximate(aScrollPosition, aOrigin);
  }
  /**
   * @note This method might destroy the frame, pres shell and other objects.
   */
  virtual CSSIntPoint GetScrollPositionCSSPixels() MOZ_OVERRIDE {
    return mHelper.GetScrollPositionCSSPixels();
  }
  /**
   * @note This method might destroy the frame, pres shell and other objects.
   */
  virtual void ScrollBy(nsIntPoint aDelta, ScrollUnit aUnit, ScrollMode aMode,
                        nsIntPoint* aOverflow, nsIAtom* aOrigin = nullptr,
                        nsIScrollableFrame::ScrollMomentum aIsMomentum = nsIScrollableFrame::NOT_MOMENTUM,
                        nsIScrollableFrame::ScrollSnapMode aSnap = nsIScrollableFrame::DISABLE_SNAP)
                        MOZ_OVERRIDE {
    mHelper.ScrollBy(aDelta, aUnit, aMode, aOverflow, aOrigin, aIsMomentum, aSnap);
  }
  virtual void FlingSnap(const mozilla::CSSPoint& aDestination,
                         uint32_t aFlingSnapGeneration) MOZ_OVERRIDE {
    mHelper.FlingSnap(aDestination, aFlingSnapGeneration);
  }
  virtual void ScrollSnap() MOZ_OVERRIDE {
    mHelper.ScrollSnap();
  }
  /**
   * @note This method might destroy the frame, pres shell and other objects.
   */
  virtual void ScrollToRestoredPosition() MOZ_OVERRIDE {
    mHelper.ScrollToRestoredPosition();
  }
  virtual void AddScrollPositionListener(nsIScrollPositionListener* aListener) MOZ_OVERRIDE {
    mHelper.AddScrollPositionListener(aListener);
  }
  virtual void RemoveScrollPositionListener(nsIScrollPositionListener* aListener) MOZ_OVERRIDE {
    mHelper.RemoveScrollPositionListener(aListener);
  }
  /**
   * @note This method might destroy the frame, pres shell and other objects.
   */
  virtual void CurPosAttributeChanged(nsIContent* aChild) MOZ_OVERRIDE {
    mHelper.CurPosAttributeChanged(aChild);
  }
  NS_IMETHOD PostScrolledAreaEventForCurrentArea() MOZ_OVERRIDE {
    mHelper.PostScrolledAreaEvent();
    return NS_OK;
  }
  virtual bool IsScrollingActive(nsDisplayListBuilder* aBuilder) MOZ_OVERRIDE {
    return mHelper.IsScrollingActive(aBuilder);
  }
  virtual bool IsProcessingAsyncScroll() MOZ_OVERRIDE {
    return mHelper.IsProcessingAsyncScroll();
  }
  virtual void ResetScrollPositionForLayerPixelAlignment() MOZ_OVERRIDE {
    mHelper.ResetScrollPositionForLayerPixelAlignment();
  }
  virtual bool IsResolutionSet() const MOZ_OVERRIDE {
    return mHelper.mIsResolutionSet;
  }
  virtual bool DidHistoryRestore() const MOZ_OVERRIDE {
    return mHelper.mDidHistoryRestore;
  }
  virtual void ClearDidHistoryRestore() MOZ_OVERRIDE {
    mHelper.mDidHistoryRestore = false;
  }
  virtual bool IsRectNearlyVisible(const nsRect& aRect) MOZ_OVERRIDE {
    return mHelper.IsRectNearlyVisible(aRect);
  }
  virtual nsRect ExpandRectToNearlyVisible(const nsRect& aRect) const MOZ_OVERRIDE {
    return mHelper.ExpandRectToNearlyVisible(aRect);
  }
  virtual nsIAtom* LastScrollOrigin() MOZ_OVERRIDE {
    return mHelper.LastScrollOrigin();
  }
  virtual nsIAtom* LastSmoothScrollOrigin() MOZ_OVERRIDE {
    return mHelper.LastSmoothScrollOrigin();
  }
  virtual uint32_t CurrentScrollGeneration() MOZ_OVERRIDE {
    return mHelper.CurrentScrollGeneration();
  }
  virtual uint32_t CurrentFlingSnapGeneration() MOZ_OVERRIDE {
    return mHelper.CurrentFlingSnapGeneration();
  }
  virtual nsPoint LastScrollDestination() MOZ_OVERRIDE {
    return mHelper.LastScrollDestination();
  }
  virtual void ResetScrollInfoIfGeneration(uint32_t aGeneration) MOZ_OVERRIDE {
    mHelper.ResetScrollInfoIfGeneration(aGeneration);
  }
  virtual bool WantAsyncScroll() const MOZ_OVERRIDE {
    return mHelper.WantAsyncScroll();
  }
  virtual void ComputeFrameMetrics(Layer* aLayer, nsIFrame* aContainerReferenceFrame,
                                   const ContainerLayerParameters& aParameters,
                                   nsRect* aClipRect,
                                   nsTArray<FrameMetrics>* aOutput) const MOZ_OVERRIDE {
    mHelper.ComputeFrameMetrics(aLayer, aContainerReferenceFrame,
                                aParameters, aClipRect, aOutput);
  }
  virtual bool IsIgnoringViewportClipping() const MOZ_OVERRIDE {
    return mHelper.IsIgnoringViewportClipping();
  }
  virtual void MarkScrollbarsDirtyForReflow() const MOZ_OVERRIDE {
    mHelper.MarkScrollbarsDirtyForReflow();
  }

  // nsIStatefulFrame
  NS_IMETHOD SaveState(nsPresState** aState) MOZ_OVERRIDE {
    NS_ENSURE_ARG_POINTER(aState);
    *aState = mHelper.SaveState();
    return NS_OK;
  }
  NS_IMETHOD RestoreState(nsPresState* aState) MOZ_OVERRIDE {
    NS_ENSURE_ARG_POINTER(aState);
    mHelper.RestoreState(aState);
    return NS_OK;
  }

  /**
   * Get the "type" of the frame
   *
   * @see nsGkAtoms::scrollFrame
   */
  virtual nsIAtom* GetType() const MOZ_OVERRIDE;

  // nsIScrollbarMediator
  virtual void ScrollByPage(nsScrollbarFrame* aScrollbar, int32_t aDirection) MOZ_OVERRIDE {
    mHelper.ScrollByPage(aScrollbar, aDirection);
  }
  virtual void ScrollByWhole(nsScrollbarFrame* aScrollbar, int32_t aDirection) MOZ_OVERRIDE {
    mHelper.ScrollByWhole(aScrollbar, aDirection);
  }
  virtual void ScrollByLine(nsScrollbarFrame* aScrollbar, int32_t aDirection) MOZ_OVERRIDE {
    mHelper.ScrollByLine(aScrollbar, aDirection);
  }
  virtual void RepeatButtonScroll(nsScrollbarFrame* aScrollbar) MOZ_OVERRIDE {
    mHelper.RepeatButtonScroll(aScrollbar);
  }
  virtual void ThumbMoved(nsScrollbarFrame* aScrollbar,
                          nscoord aOldPos,
                          nscoord aNewPos) MOZ_OVERRIDE {
    mHelper.ThumbMoved(aScrollbar, aOldPos, aNewPos);
  }
  virtual void VisibilityChanged(bool aVisible) MOZ_OVERRIDE {}
  virtual nsIFrame* GetScrollbarBox(bool aVertical) MOZ_OVERRIDE {
    return mHelper.GetScrollbarBox(aVertical);
  }
  virtual void ScrollbarActivityStarted() const MOZ_OVERRIDE;
  virtual void ScrollbarActivityStopped() const MOZ_OVERRIDE;
  
#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const MOZ_OVERRIDE;
#endif

#ifdef ACCESSIBILITY
  virtual mozilla::a11y::AccType AccessibleType() MOZ_OVERRIDE;
#endif

protected:
  nsHTMLScrollFrame(nsStyleContext* aContext, bool aIsRoot);
  void SetSuppressScrollbarUpdate(bool aSuppress) {
    mHelper.mSupppressScrollbarUpdate = aSuppress;
  }
  bool GuessHScrollbarNeeded(const ScrollReflowState& aState);
  bool GuessVScrollbarNeeded(const ScrollReflowState& aState);

  bool IsScrollbarUpdateSuppressed() const {
    return mHelper.mSupppressScrollbarUpdate;
  }

  // Return whether we're in an "initial" reflow.  Some reflows with
  // NS_FRAME_FIRST_REFLOW set are NOT "initial" as far as we're concerned.
  bool InInitialReflow() const;
  
  /**
   * Override this to return false if computed height/min-height/max-height
   * should NOT be propagated to child content.
   * nsListControlFrame uses this.
   */
  virtual bool ShouldPropagateComputedHeightToScrolledContent() const { return true; }

private:
  friend class mozilla::ScrollFrameHelper;
  ScrollFrameHelper mHelper;
};

/**
 * The scroll frame creates and manages the scrolling view
 *
 * It only supports having a single child frame that typically is an area
 * frame, but doesn't have to be. The child frame must have a view, though
 *
 * Scroll frames don't support incremental changes, i.e. you can't replace
 * or remove the scrolled frame
 */
class nsXULScrollFrame MOZ_FINAL : public nsBoxFrame,
                                   public nsIScrollableFrame,
                                   public nsIAnonymousContentCreator,
                                   public nsIStatefulFrame {
public:
  typedef mozilla::ScrollFrameHelper ScrollFrameHelper;
  typedef mozilla::CSSIntPoint CSSIntPoint;

  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS

  friend nsXULScrollFrame* NS_NewXULScrollFrame(nsIPresShell* aPresShell,
                                                nsStyleContext* aContext,
                                                bool aIsRoot,
                                                bool aClipAllDescendants);

  virtual void BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                const nsRect&           aDirtyRect,
                                const nsDisplayListSet& aLists) MOZ_OVERRIDE {
    mHelper.BuildDisplayList(aBuilder, aDirtyRect, aLists);
  }

  // XXXldb Is this actually used?
#if 0
  virtual nscoord GetMinISize(nsRenderingContext *aRenderingContext) MOZ_OVERRIDE;
#endif

  virtual bool UpdateOverflow() MOZ_OVERRIDE {
    return mHelper.UpdateOverflow();
  }

  // Called to set the child frames. We typically have three: the scroll area,
  // the vertical scrollbar, and the horizontal scrollbar.
  virtual void SetInitialChildList(ChildListID     aListID,
                                   nsFrameList&    aChildList) MOZ_OVERRIDE;
  virtual void AppendFrames(ChildListID     aListID,
                            nsFrameList&    aFrameList) MOZ_OVERRIDE;
  virtual void InsertFrames(ChildListID     aListID,
                            nsIFrame*       aPrevFrame,
                            nsFrameList&    aFrameList) MOZ_OVERRIDE;
  virtual void RemoveFrame(ChildListID     aListID,
                           nsIFrame*       aOldFrame) MOZ_OVERRIDE;

  virtual void DestroyFrom(nsIFrame* aDestructRoot) MOZ_OVERRIDE;


  virtual nsIScrollableFrame* GetScrollTargetFrame() MOZ_OVERRIDE {
    return this;
  }

  virtual nsContainerFrame* GetContentInsertionFrame() MOZ_OVERRIDE {
    return mHelper.GetScrolledFrame()->GetContentInsertionFrame();
  }

  virtual bool DoesClipChildren() MOZ_OVERRIDE { return true; }
  virtual nsSplittableType GetSplittableType() const MOZ_OVERRIDE;

  virtual nsPoint GetPositionOfChildIgnoringScrolling(nsIFrame* aChild) MOZ_OVERRIDE
  { nsPoint pt = aChild->GetPosition();
    if (aChild == mHelper.GetScrolledFrame())
      pt += mHelper.GetLogicalScrollPosition();
    return pt;
  }

  // nsIAnonymousContentCreator
  virtual nsresult CreateAnonymousContent(nsTArray<ContentInfo>& aElements) MOZ_OVERRIDE;
  virtual void AppendAnonymousContentTo(nsTArray<nsIContent*>& aElements,
                                        uint32_t aFilter) MOZ_OVERRIDE;

  virtual nsSize GetMinSize(nsBoxLayoutState& aBoxLayoutState) MOZ_OVERRIDE;
  virtual nsSize GetPrefSize(nsBoxLayoutState& aBoxLayoutState) MOZ_OVERRIDE;
  virtual nsSize GetMaxSize(nsBoxLayoutState& aBoxLayoutState) MOZ_OVERRIDE;
  virtual nscoord GetBoxAscent(nsBoxLayoutState& aBoxLayoutState) MOZ_OVERRIDE;

  NS_IMETHOD DoLayout(nsBoxLayoutState& aBoxLayoutState) MOZ_OVERRIDE;
  virtual nsresult GetPadding(nsMargin& aPadding) MOZ_OVERRIDE;

  virtual bool GetBorderRadii(const nsSize& aFrameSize, const nsSize& aBorderArea,
                              Sides aSkipSides, nscoord aRadii[8]) const MOZ_OVERRIDE {
    return mHelper.GetBorderRadii(aFrameSize, aBorderArea, aSkipSides, aRadii);
  }

  nsresult Layout(nsBoxLayoutState& aState);
  void LayoutScrollArea(nsBoxLayoutState& aState, const nsPoint& aScrollPosition);

  static bool AddRemoveScrollbar(bool& aHasScrollbar, 
                                   nscoord& aXY, 
                                   nscoord& aSize, 
                                   nscoord aSbSize, 
                                   bool aOnRightOrBottom, 
                                   bool aAdd);
  
  bool AddRemoveScrollbar(nsBoxLayoutState& aState, 
                            bool aOnRightOrBottom, 
                            bool aHorizontal, 
                            bool aAdd);
  
  bool AddHorizontalScrollbar (nsBoxLayoutState& aState, bool aOnBottom);
  bool AddVerticalScrollbar   (nsBoxLayoutState& aState, bool aOnRight);
  void RemoveHorizontalScrollbar(nsBoxLayoutState& aState, bool aOnBottom);
  void RemoveVerticalScrollbar  (nsBoxLayoutState& aState, bool aOnRight);

  static void AdjustReflowStateForPrintPreview(nsBoxLayoutState& aState, bool& aSetBack);
  static void AdjustReflowStateBack(nsBoxLayoutState& aState, bool aSetBack);

  // nsIScrollableFrame
  virtual nsIFrame* GetScrolledFrame() const MOZ_OVERRIDE {
    return mHelper.GetScrolledFrame();
  }
  virtual mozilla::ScrollbarStyles GetScrollbarStyles() const MOZ_OVERRIDE {
    return mHelper.GetScrollbarStylesFromFrame();
  }
  virtual uint32_t GetScrollbarVisibility() const MOZ_OVERRIDE {
    return mHelper.GetScrollbarVisibility();
  }
  virtual nsMargin GetActualScrollbarSizes() const MOZ_OVERRIDE {
    return mHelper.GetActualScrollbarSizes();
  }
  virtual nsMargin GetDesiredScrollbarSizes(nsBoxLayoutState* aState) MOZ_OVERRIDE {
    return mHelper.GetDesiredScrollbarSizes(aState);
  }
  virtual nsMargin GetDesiredScrollbarSizes(nsPresContext* aPresContext,
          nsRenderingContext* aRC) MOZ_OVERRIDE {
    nsBoxLayoutState bls(aPresContext, aRC, 0);
    return GetDesiredScrollbarSizes(&bls);
  }
  virtual nscoord GetNondisappearingScrollbarWidth(nsPresContext* aPresContext,
          nsRenderingContext* aRC) MOZ_OVERRIDE {
    nsBoxLayoutState bls(aPresContext, aRC, 0);
    return mHelper.GetNondisappearingScrollbarWidth(&bls);
  }
  virtual nsRect GetScrolledRect() const MOZ_OVERRIDE {
    return mHelper.GetScrolledRect();
  }
  virtual nsRect GetScrollPortRect() const MOZ_OVERRIDE {
    return mHelper.GetScrollPortRect();
  }
  virtual nsPoint GetScrollPosition() const MOZ_OVERRIDE {
    return mHelper.GetScrollPosition();
  }
  virtual nsPoint GetLogicalScrollPosition() const MOZ_OVERRIDE {
    return mHelper.GetLogicalScrollPosition();
  }
  virtual nsRect GetScrollRange() const MOZ_OVERRIDE {
    return mHelper.GetScrollRange();
  }
  virtual nsSize GetScrollPositionClampingScrollPortSize() const MOZ_OVERRIDE {
    return mHelper.GetScrollPositionClampingScrollPortSize();
  }
  virtual gfxSize GetResolution() const MOZ_OVERRIDE {
    return mHelper.GetResolution();
  }
  virtual void SetResolution(const gfxSize& aResolution) MOZ_OVERRIDE {
    return mHelper.SetResolution(aResolution);
  }
  virtual void SetResolutionAndScaleTo(const gfxSize& aResolution) MOZ_OVERRIDE {
    return mHelper.SetResolutionAndScaleTo(aResolution);
  }
  virtual nsSize GetLineScrollAmount() const MOZ_OVERRIDE {
    return mHelper.GetLineScrollAmount();
  }
  virtual nsSize GetPageScrollAmount() const MOZ_OVERRIDE {
    return mHelper.GetPageScrollAmount();
  }
  /**
   * @note This method might destroy the frame, pres shell and other objects.
   */
  virtual void ScrollTo(nsPoint aScrollPosition, ScrollMode aMode,
                        const nsRect* aRange = nullptr,
                        ScrollSnapMode aSnap = nsIScrollableFrame::DISABLE_SNAP) MOZ_OVERRIDE {
    mHelper.ScrollTo(aScrollPosition, aMode, aRange, aSnap);
  }
  /**
   * @note This method might destroy the frame, pres shell and other objects.
   */
  virtual void ScrollToCSSPixels(const CSSIntPoint& aScrollPosition,
                                 nsIScrollableFrame::ScrollMode aMode
                                   = nsIScrollableFrame::INSTANT) MOZ_OVERRIDE {
    mHelper.ScrollToCSSPixels(aScrollPosition, aMode);
  }
  virtual void ScrollToCSSPixelsApproximate(const mozilla::CSSPoint& aScrollPosition,
                                            nsIAtom* aOrigin = nullptr) MOZ_OVERRIDE {
    mHelper.ScrollToCSSPixelsApproximate(aScrollPosition, aOrigin);
  }
  virtual CSSIntPoint GetScrollPositionCSSPixels() MOZ_OVERRIDE {
    return mHelper.GetScrollPositionCSSPixels();
  }
  /**
   * @note This method might destroy the frame, pres shell and other objects.
   */
  virtual void ScrollBy(nsIntPoint aDelta, ScrollUnit aUnit, ScrollMode aMode,
                        nsIntPoint* aOverflow, nsIAtom* aOrigin = nullptr,
                        nsIScrollableFrame::ScrollMomentum aIsMomentum = nsIScrollableFrame::NOT_MOMENTUM,
                        nsIScrollableFrame::ScrollSnapMode aSnap = nsIScrollableFrame::DISABLE_SNAP)
                        MOZ_OVERRIDE {
    mHelper.ScrollBy(aDelta, aUnit, aMode, aOverflow, aOrigin, aIsMomentum, aSnap);
  }
  virtual void FlingSnap(const mozilla::CSSPoint& aDestination,
                         uint32_t aFlingSnapGeneration) MOZ_OVERRIDE {
    mHelper.FlingSnap(aDestination, aFlingSnapGeneration);
  }
  virtual void ScrollSnap() MOZ_OVERRIDE {
    mHelper.ScrollSnap();
  }
  /**
   * @note This method might destroy the frame, pres shell and other objects.
   */
  virtual void ScrollToRestoredPosition() MOZ_OVERRIDE {
    mHelper.ScrollToRestoredPosition();
  }
  virtual void AddScrollPositionListener(nsIScrollPositionListener* aListener) MOZ_OVERRIDE {
    mHelper.AddScrollPositionListener(aListener);
  }
  virtual void RemoveScrollPositionListener(nsIScrollPositionListener* aListener) MOZ_OVERRIDE {
    mHelper.RemoveScrollPositionListener(aListener);
  }
  /**
   * @note This method might destroy the frame, pres shell and other objects.
   */
  virtual void CurPosAttributeChanged(nsIContent* aChild) MOZ_OVERRIDE {
    mHelper.CurPosAttributeChanged(aChild);
  }
  NS_IMETHOD PostScrolledAreaEventForCurrentArea() MOZ_OVERRIDE {
    mHelper.PostScrolledAreaEvent();
    return NS_OK;
  }
  virtual bool IsScrollingActive(nsDisplayListBuilder* aBuilder) MOZ_OVERRIDE {
    return mHelper.IsScrollingActive(aBuilder);
  }
  virtual bool IsProcessingAsyncScroll() MOZ_OVERRIDE {
    return mHelper.IsProcessingAsyncScroll();
  }
  virtual void ResetScrollPositionForLayerPixelAlignment() MOZ_OVERRIDE {
    mHelper.ResetScrollPositionForLayerPixelAlignment();
  }
  virtual bool IsResolutionSet() const MOZ_OVERRIDE {
    return mHelper.mIsResolutionSet;
  }
  virtual bool DidHistoryRestore() const MOZ_OVERRIDE {
    return mHelper.mDidHistoryRestore;
  }
  virtual void ClearDidHistoryRestore() MOZ_OVERRIDE {
    mHelper.mDidHistoryRestore = false;
  }
  virtual bool IsRectNearlyVisible(const nsRect& aRect) MOZ_OVERRIDE {
    return mHelper.IsRectNearlyVisible(aRect);
  }
  virtual nsRect ExpandRectToNearlyVisible(const nsRect& aRect) const MOZ_OVERRIDE {
    return mHelper.ExpandRectToNearlyVisible(aRect);
  }
  virtual nsIAtom* LastScrollOrigin() MOZ_OVERRIDE {
    return mHelper.LastScrollOrigin();
  }
  virtual nsIAtom* LastSmoothScrollOrigin() MOZ_OVERRIDE {
    return mHelper.LastSmoothScrollOrigin();
  }
  virtual uint32_t CurrentScrollGeneration() MOZ_OVERRIDE {
    return mHelper.CurrentScrollGeneration();
  }
  virtual uint32_t CurrentFlingSnapGeneration() MOZ_OVERRIDE {
    return mHelper.CurrentFlingSnapGeneration();
  }
  virtual nsPoint LastScrollDestination() MOZ_OVERRIDE {
    return mHelper.LastScrollDestination();
  }
  virtual void ResetScrollInfoIfGeneration(uint32_t aGeneration) MOZ_OVERRIDE {
    mHelper.ResetScrollInfoIfGeneration(aGeneration);
  }
  virtual bool WantAsyncScroll() const MOZ_OVERRIDE {
    return mHelper.WantAsyncScroll();
  }
  virtual void ComputeFrameMetrics(Layer* aLayer, nsIFrame* aContainerReferenceFrame,
                                   const ContainerLayerParameters& aParameters,
                                   nsRect* aClipRect,
                                   nsTArray<FrameMetrics>* aOutput) const MOZ_OVERRIDE {
    mHelper.ComputeFrameMetrics(aLayer, aContainerReferenceFrame,
                                aParameters, aClipRect, aOutput);
  }
  virtual bool IsIgnoringViewportClipping() const MOZ_OVERRIDE {
    return mHelper.IsIgnoringViewportClipping();
  }
  virtual void MarkScrollbarsDirtyForReflow() const MOZ_OVERRIDE {
    mHelper.MarkScrollbarsDirtyForReflow();
  }

  // nsIStatefulFrame
  NS_IMETHOD SaveState(nsPresState** aState) MOZ_OVERRIDE {
    NS_ENSURE_ARG_POINTER(aState);
    *aState = mHelper.SaveState();
    return NS_OK;
  }
  NS_IMETHOD RestoreState(nsPresState* aState) MOZ_OVERRIDE {
    NS_ENSURE_ARG_POINTER(aState);
    mHelper.RestoreState(aState);
    return NS_OK;
  }

  /**
   * Get the "type" of the frame
   *
   * @see nsGkAtoms::scrollFrame
   */
  virtual nsIAtom* GetType() const MOZ_OVERRIDE;
  
  virtual bool IsFrameOfType(uint32_t aFlags) const MOZ_OVERRIDE
  {
    // Override bogus IsFrameOfType in nsBoxFrame.
    if (aFlags & (nsIFrame::eReplacedContainsBlock | nsIFrame::eReplaced))
      return false;
    return nsBoxFrame::IsFrameOfType(aFlags);
  }

  virtual void ScrollByPage(nsScrollbarFrame* aScrollbar, int32_t aDirection) MOZ_OVERRIDE {
    mHelper.ScrollByPage(aScrollbar, aDirection);
  }
  virtual void ScrollByWhole(nsScrollbarFrame* aScrollbar, int32_t aDirection) MOZ_OVERRIDE {
    mHelper.ScrollByWhole(aScrollbar, aDirection);
  }
  virtual void ScrollByLine(nsScrollbarFrame* aScrollbar, int32_t aDirection) MOZ_OVERRIDE {
    mHelper.ScrollByLine(aScrollbar, aDirection);
  }
  virtual void RepeatButtonScroll(nsScrollbarFrame* aScrollbar) MOZ_OVERRIDE {
    mHelper.RepeatButtonScroll(aScrollbar);
  }
  virtual void ThumbMoved(nsScrollbarFrame* aScrollbar,
                          nscoord aOldPos,
                          nscoord aNewPos) MOZ_OVERRIDE {
    mHelper.ThumbMoved(aScrollbar, aOldPos, aNewPos);
  }
  virtual void VisibilityChanged(bool aVisible) MOZ_OVERRIDE {}
  virtual nsIFrame* GetScrollbarBox(bool aVertical) MOZ_OVERRIDE {
    return mHelper.GetScrollbarBox(aVertical);
  }

  virtual void ScrollbarActivityStarted() const MOZ_OVERRIDE;
  virtual void ScrollbarActivityStopped() const MOZ_OVERRIDE;

#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const MOZ_OVERRIDE;
#endif

protected:
  nsXULScrollFrame(nsStyleContext* aContext, bool aIsRoot,
                   bool aClipAllDescendants);

  void ClampAndSetBounds(nsBoxLayoutState& aState, 
                         nsRect& aRect,
                         nsPoint aScrollPosition,
                         bool aRemoveOverflowAreas = false) {
    /* 
     * For RTL frames, restore the original scrolled position of the right
     * edge, then subtract the current width to find the physical position.
     */
    if (!mHelper.IsLTR()) {
      aRect.x = mHelper.mScrollPort.XMost() - aScrollPosition.x - aRect.width;
    }
    mHelper.mScrolledFrame->SetBounds(aState, aRect, aRemoveOverflowAreas);
  }

private:
  friend class mozilla::ScrollFrameHelper;
  ScrollFrameHelper mHelper;
};

#endif /* nsGfxScrollFrame_h___ */
