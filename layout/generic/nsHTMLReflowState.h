/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* struct containing the input to nsIFrame::Reflow */

#ifndef nsHTMLReflowState_h___
#define nsHTMLReflowState_h___

#include "nsMargin.h"
#include "nsStyleCoord.h"
#include "nsIFrame.h"
#include "mozilla/Assertions.h"
#include <algorithm>

class nsPresContext;
class nsRenderingContext;
class nsFloatManager;
class nsLineLayout;
class nsIPercentBSizeObserver;
struct nsHypotheticalBox;

/**
 * @return aValue clamped to [aMinValue, aMaxValue].
 *
 * @note This function needs to handle aMinValue > aMaxValue. In that case,
 *       aMinValue is returned.
 * @see http://www.w3.org/TR/CSS21/visudet.html#min-max-widths
 * @see http://www.w3.org/TR/CSS21/visudet.html#min-max-heights
 */
template <class NumericType>
NumericType
NS_CSS_MINMAX(NumericType aValue, NumericType aMinValue, NumericType aMaxValue)
{
  NumericType result = aValue;
  if (aMaxValue < result)
    result = aMaxValue;
  if (aMinValue > result)
    result = aMinValue;
  return result;
}

/**
 * CSS Frame type. Included as part of the reflow state.
 */
typedef uint32_t  nsCSSFrameType;

#define NS_CSS_FRAME_TYPE_UNKNOWN         0
#define NS_CSS_FRAME_TYPE_INLINE          1
#define NS_CSS_FRAME_TYPE_BLOCK           2  /* block-level in normal flow */
#define NS_CSS_FRAME_TYPE_FLOATING        3
#define NS_CSS_FRAME_TYPE_ABSOLUTE        4
#define NS_CSS_FRAME_TYPE_INTERNAL_TABLE  5  /* row group frame, row frame, cell frame, ... */

/**
 * Bit-flag that indicates whether the element is replaced. Applies to inline,
 * block-level, floating, and absolutely positioned elements
 */
#define NS_CSS_FRAME_TYPE_REPLACED                0x08000

/**
 * Bit-flag that indicates that the element is replaced and contains a block
 * (eg some form controls).  Applies to inline, block-level, floating, and
 * absolutely positioned elements.  Mutually exclusive with
 * NS_CSS_FRAME_TYPE_REPLACED.
 */
#define NS_CSS_FRAME_TYPE_REPLACED_CONTAINS_BLOCK 0x10000

/**
 * Helper macros for telling whether items are replaced
 */
#define NS_FRAME_IS_REPLACED_NOBLOCK(_ft) \
  (NS_CSS_FRAME_TYPE_REPLACED == ((_ft) & NS_CSS_FRAME_TYPE_REPLACED))

#define NS_FRAME_IS_REPLACED(_ft)            \
  (NS_FRAME_IS_REPLACED_NOBLOCK(_ft) ||      \
   NS_FRAME_IS_REPLACED_CONTAINS_BLOCK(_ft))

#define NS_FRAME_REPLACED(_ft) \
  (NS_CSS_FRAME_TYPE_REPLACED | (_ft))

#define NS_FRAME_IS_REPLACED_CONTAINS_BLOCK(_ft)         \
  (NS_CSS_FRAME_TYPE_REPLACED_CONTAINS_BLOCK ==         \
   ((_ft) & NS_CSS_FRAME_TYPE_REPLACED_CONTAINS_BLOCK))

#define NS_FRAME_REPLACED_CONTAINS_BLOCK(_ft) \
  (NS_CSS_FRAME_TYPE_REPLACED_CONTAINS_BLOCK | (_ft))

/**
 * A macro to extract the type. Masks off the 'replaced' bit-flag
 */
#define NS_FRAME_GET_TYPE(_ft)                           \
  ((_ft) & ~(NS_CSS_FRAME_TYPE_REPLACED |                \
             NS_CSS_FRAME_TYPE_REPLACED_CONTAINS_BLOCK))

// A base class of nsHTMLReflowState that computes only the padding,
// border, and margin, since those values are needed more often.
struct nsCSSOffsetState {
public:
  typedef mozilla::WritingMode WritingMode;
  typedef mozilla::LogicalMargin LogicalMargin;

  // the frame being reflowed
  nsIFrame*           frame;

  // rendering context to use for measurement
  nsRenderingContext* rendContext;

  const nsMargin& ComputedPhysicalMargin() const { return mComputedMargin; }
  const nsMargin& ComputedPhysicalBorderPadding() const { return mComputedBorderPadding; }
  const nsMargin& ComputedPhysicalPadding() const { return mComputedPadding; }

  // We may need to eliminate the (few) users of these writable-reference accessors
  // as part of migrating to logical coordinates.
  nsMargin& ComputedPhysicalMargin() { return mComputedMargin; }
  nsMargin& ComputedPhysicalBorderPadding() { return mComputedBorderPadding; }
  nsMargin& ComputedPhysicalPadding() { return mComputedPadding; }

  const LogicalMargin ComputedLogicalMargin() const
    { return LogicalMargin(mWritingMode, mComputedMargin); }
  const LogicalMargin ComputedLogicalBorderPadding() const
    { return LogicalMargin(mWritingMode, mComputedBorderPadding); }
  const LogicalMargin ComputedLogicalPadding() const
    { return LogicalMargin(mWritingMode, mComputedPadding); }

  void SetComputedLogicalMargin(mozilla::WritingMode aWM,
                                const LogicalMargin& aMargin)
    { mComputedMargin = aMargin.GetPhysicalMargin(aWM); }
  void SetComputedLogicalMargin(const LogicalMargin& aMargin)
    { SetComputedLogicalMargin(mWritingMode, aMargin); }

  void SetComputedLogicalBorderPadding(mozilla::WritingMode aWM,
                                       const LogicalMargin& aMargin)
    { mComputedBorderPadding = aMargin.GetPhysicalMargin(aWM); }
  void SetComputedLogicalBorderPadding(const LogicalMargin& aMargin)
    { SetComputedLogicalBorderPadding(mWritingMode, aMargin); }

  void SetComputedLogicalPadding(mozilla::WritingMode aWM,
                                 const LogicalMargin& aMargin)
    { mComputedPadding = aMargin.GetPhysicalMargin(aWM); }
  void SetComputedLogicalPadding(const LogicalMargin& aMargin)
    { SetComputedLogicalPadding(mWritingMode, aMargin); }

  WritingMode GetWritingMode() const { return mWritingMode; }

protected:
  // cached copy of the frame's writing-mode, for logical coordinates
  WritingMode      mWritingMode;

  // These are PHYSICAL coordinates (for now).
  // Will probably become logical in due course.

  // Computed margin values
  nsMargin         mComputedMargin;

  // Cached copy of the border + padding values
  nsMargin         mComputedBorderPadding;

  // Computed padding values
  nsMargin         mComputedPadding;

public:
  // Callers using this constructor must call InitOffsets on their own.
  nsCSSOffsetState(nsIFrame *aFrame, nsRenderingContext *aRenderingContext)
    : frame(aFrame)
    , rendContext(aRenderingContext)
    , mWritingMode(aFrame->GetWritingMode())
  {
  }

  nsCSSOffsetState(nsIFrame *aFrame, nsRenderingContext *aRenderingContext,
                   mozilla::WritingMode aContainingBlockWritingMode,
                   nscoord aContainingBlockISize);

#ifdef DEBUG
  // Reflow trace methods.  Defined in nsFrame.cpp so they have access
  // to the display-reflow infrastructure.
  static void* DisplayInitOffsetsEnter(
                                     nsIFrame* aFrame,
                                     nsCSSOffsetState* aState,
                                     const mozilla::LogicalSize& aPercentBasis,
                                     const nsMargin* aBorder,
                                     const nsMargin* aPadding);
  static void DisplayInitOffsetsExit(nsIFrame* aFrame,
                                     nsCSSOffsetState* aState,
                                     void* aValue);
#endif

private:
  /**
   * Computes margin values from the specified margin style information, and
   * fills in the mComputedMargin member.
   *
   * @param aWM Writing mode of the containing block
   * @param aPercentBasis
   *    Logical size in the writing mode of the containing block to use
   *    for resolving percentage margin values in the inline and block
   *    axes.
   *    The inline size is usually the containing block inline-size
   *    (width if writing mode is horizontal, and height if vertical).
   *    The block size is usually the containing block inline-size, per
   *    CSS21 sec 8.3 (read in conjunction with CSS Writing Modes sec
   *    7.2), but may be the containing block block-size, e.g. in CSS3
   *    Flexbox and Grid.
   * @return true if the margin is dependent on the containing block size.
   */
  bool ComputeMargin(mozilla::WritingMode aWM,
                     const mozilla::LogicalSize& aPercentBasis);
  
  /**
   * Computes padding values from the specified padding style information, and
   * fills in the mComputedPadding member.
   *
   * @param aWM Writing mode of the containing block
   * @param aPercentBasis
   *    Logical size in the writing mode of the containing block to use
   *    for resolving percentage padding values in the inline and block
   *    axes.
   *    The inline size is usually the containing block inline-size
   *    (width if writing mode is horizontal, and height if vertical).
   *    The block size is usually the containing block inline-size, per
   *    CSS21 sec 8.3 (read in conjunction with CSS Writing Modes sec
   *    7.2), but may be the containing block block-size, e.g. in CSS3
   *    Flexbox and Grid.
   * @return true if the padding is dependent on the containing block size.
   */
  bool ComputePadding(mozilla::WritingMode aWM,
                      const mozilla::LogicalSize& aPercentBasis,
                      nsIAtom* aFrameType);

protected:

  void InitOffsets(mozilla::WritingMode aWM,
                   const mozilla::LogicalSize& aPercentBasis,
                   nsIAtom* aFrameType,
                   const nsMargin *aBorder = nullptr,
                   const nsMargin *aPadding = nullptr);

  /*
   * Convert nsStyleCoord to nscoord when percentages depend on the
   * inline size of the containing block, and enumerated values are for
   * inline size, min-inline-size, or max-inline-size.  Does not handle
   * auto inline sizes.
   */
  inline nscoord ComputeISizeValue(nscoord aContainingBlockISize,
                                   nscoord aContentEdgeToBoxSizing,
                                   nscoord aBoxSizingToMarginEdge,
                                   const nsStyleCoord& aCoord);
  // same as previous, but using mComputedBorderPadding, mComputedPadding,
  // and mComputedMargin
  nscoord ComputeISizeValue(nscoord aContainingBlockISize,
                            uint8_t aBoxSizing,
                            const nsStyleCoord& aCoord);

  nscoord ComputeBSizeValue(nscoord aContainingBlockBSize,
                            uint8_t aBoxSizing,
                            const nsStyleCoord& aCoord);
};

/**
 * State passed to a frame during reflow or intrinsic size calculation.
 *
 * XXX Refactor so only a base class (nsSizingState?) is used for intrinsic
 * size calculation.
 *
 * @see nsIFrame#Reflow()
 */
struct nsHTMLReflowState : public nsCSSOffsetState {
  // the reflow states are linked together. this is the pointer to the
  // parent's reflow state
  const nsHTMLReflowState* parentReflowState;

  // pointer to the float manager associated with this area
  nsFloatManager* mFloatManager;

  // LineLayout object (only for inline reflow; set to nullptr otherwise)
  nsLineLayout*    mLineLayout;

  // The appropriate reflow state for the containing block (for
  // percentage widths, etc.) of this reflow state's frame.
  const nsHTMLReflowState *mCBReflowState;

  // The type of frame, from css's perspective. This value is
  // initialized by the Init method below.
  nsCSSFrameType   mFrameType;

  // The amount the in-flow position of the block is moving vertically relative
  // to its previous in-flow position (i.e. the amount the line containing the
  // block is moving).
  // This should be zero for anything which is not a block outside, and it
  // should be zero for anything which has a non-block parent.
  // The intended use of this value is to allow the accurate determination
  // of the potential impact of a float
  // This takes on an arbitrary value the first time a block is reflowed
  nscoord mBlockDelta;

  // If an nsHTMLReflowState finds itself initialized with an unconstrained
  // inline-size, it will look up its parentReflowState chain for a state
  // with an orthogonal writing mode and a non-NS_UNCONSTRAINEDSIZE value for
  // orthogonal limit; when it finds such a reflow-state, it will use its
  // orthogonal-limit value to constrain inline-size.
  // This is initialized to NS_UNCONSTRAINEDSIZE (so it will be ignored),
  // but reset to a suitable value for the reflow root by nsPresShell.
  nscoord mOrthogonalLimit;

  // Accessors for the private fields below. Forcing all callers to use these
  // will allow us to introduce logical-coordinate versions and gradually
  // change clients from physical to logical as needed; and potentially switch
  // the internal fields from physical to logical coordinates in due course,
  // while maintaining compatibility with not-yet-updated code.
  nscoord AvailableWidth() const { return mAvailableWidth; }
  nscoord AvailableHeight() const { return mAvailableHeight; }
  nscoord ComputedWidth() const { return mComputedWidth; }
  nscoord ComputedHeight() const { return mComputedHeight; }
  nscoord ComputedMinWidth() const { return mComputedMinWidth; }
  nscoord ComputedMaxWidth() const { return mComputedMaxWidth; }
  nscoord ComputedMinHeight() const { return mComputedMinHeight; }
  nscoord ComputedMaxHeight() const { return mComputedMaxHeight; }

  nscoord& AvailableWidth() { return mAvailableWidth; }
  nscoord& AvailableHeight() { return mAvailableHeight; }
  nscoord& ComputedWidth() { return mComputedWidth; }
  nscoord& ComputedHeight() { return mComputedHeight; }
  nscoord& ComputedMinWidth() { return mComputedMinWidth; }
  nscoord& ComputedMaxWidth() { return mComputedMaxWidth; }
  nscoord& ComputedMinHeight() { return mComputedMinHeight; }
  nscoord& ComputedMaxHeight() { return mComputedMaxHeight; }

  // ISize and BSize are logical-coordinate dimensions:
  // ISize is the size in the writing mode's inline direction (which equates to
  // width in horizontal writing modes, height in vertical ones), and BSize is
  // the size in the block-progression direction.
  nscoord AvailableISize() const
    { return mWritingMode.IsVertical() ? mAvailableHeight : mAvailableWidth; }
  nscoord AvailableBSize() const
    { return mWritingMode.IsVertical() ? mAvailableWidth : mAvailableHeight; }
  nscoord ComputedISize() const
    { return mWritingMode.IsVertical() ? mComputedHeight : mComputedWidth; }
  nscoord ComputedBSize() const
    { return mWritingMode.IsVertical() ? mComputedWidth : mComputedHeight; }
  nscoord ComputedMinISize() const
    { return mWritingMode.IsVertical() ? mComputedMinHeight : mComputedMinWidth; }
  nscoord ComputedMaxISize() const
    { return mWritingMode.IsVertical() ? mComputedMaxHeight : mComputedMaxWidth; }
  nscoord ComputedMinBSize() const
    { return mWritingMode.IsVertical() ? mComputedMinWidth : mComputedMinHeight; }
  nscoord ComputedMaxBSize() const
    { return mWritingMode.IsVertical() ? mComputedMaxWidth : mComputedMaxHeight; }

  nscoord& AvailableISize()
    { return mWritingMode.IsVertical() ? mAvailableHeight : mAvailableWidth; }
  nscoord& AvailableBSize()
    { return mWritingMode.IsVertical() ? mAvailableWidth : mAvailableHeight; }
  nscoord& ComputedISize()
    { return mWritingMode.IsVertical() ? mComputedHeight : mComputedWidth; }
  nscoord& ComputedBSize()
    { return mWritingMode.IsVertical() ? mComputedWidth : mComputedHeight; }
  nscoord& ComputedMinISize()
    { return mWritingMode.IsVertical() ? mComputedMinHeight : mComputedMinWidth; }
  nscoord& ComputedMaxISize()
    { return mWritingMode.IsVertical() ? mComputedMaxHeight : mComputedMaxWidth; }
  nscoord& ComputedMinBSize()
    { return mWritingMode.IsVertical() ? mComputedMinWidth : mComputedMinHeight; }
  nscoord& ComputedMaxBSize()
    { return mWritingMode.IsVertical() ? mComputedMaxWidth : mComputedMaxHeight; }

  mozilla::LogicalSize AvailableSize() const {
    return mozilla::LogicalSize(mWritingMode,
                                AvailableISize(), AvailableBSize());
  }
  mozilla::LogicalSize ComputedSize() const {
    return mozilla::LogicalSize(mWritingMode,
                                ComputedISize(), ComputedBSize());
  }
  mozilla::LogicalSize ComputedMinSize() const {
    return mozilla::LogicalSize(mWritingMode,
                                ComputedMinISize(), ComputedMinBSize());
  }
  mozilla::LogicalSize ComputedMaxSize() const {
    return mozilla::LogicalSize(mWritingMode,
                                ComputedMaxISize(), ComputedMaxBSize());
  }

  mozilla::LogicalSize AvailableSize(mozilla::WritingMode aWM) const
  { return AvailableSize().ConvertTo(aWM, mWritingMode); }
  mozilla::LogicalSize ComputedSize(mozilla::WritingMode aWM) const
    { return ComputedSize().ConvertTo(aWM, mWritingMode); }
  mozilla::LogicalSize ComputedMinSize(mozilla::WritingMode aWM) const
    { return ComputedMinSize().ConvertTo(aWM, mWritingMode); }
  mozilla::LogicalSize ComputedMaxSize(mozilla::WritingMode aWM) const
    { return ComputedMaxSize().ConvertTo(aWM, mWritingMode); }

  mozilla::LogicalSize ComputedSizeWithPadding() const {
    mozilla::WritingMode wm = GetWritingMode();
    return mozilla::LogicalSize(wm,
                                ComputedISize() +
                                ComputedLogicalPadding().IStartEnd(wm),
                                ComputedBSize() +
                                ComputedLogicalPadding().BStartEnd(wm));
  }

  mozilla::LogicalSize ComputedSizeWithPadding(mozilla::WritingMode aWM) const {
    return ComputedSizeWithPadding().ConvertTo(aWM, GetWritingMode());
  }

  mozilla::LogicalSize ComputedSizeWithBorderPadding() const {
    mozilla::WritingMode wm = GetWritingMode();
    return mozilla::LogicalSize(wm,
                                ComputedISize() +
                                ComputedLogicalBorderPadding().IStartEnd(wm),
                                ComputedBSize() +
                                ComputedLogicalBorderPadding().BStartEnd(wm));
  }

  mozilla::LogicalSize
  ComputedSizeWithBorderPadding(mozilla::WritingMode aWM) const {
    return ComputedSizeWithBorderPadding().ConvertTo(aWM, GetWritingMode());
  }

  mozilla::LogicalSize
  ComputedSizeWithMarginBorderPadding() const {
    mozilla::WritingMode wm = GetWritingMode();
    return mozilla::LogicalSize(wm,
                                ComputedISize() +
                                ComputedLogicalMargin().IStartEnd(wm) +
                                ComputedLogicalBorderPadding().IStartEnd(wm),
                                ComputedBSize() +
                                ComputedLogicalMargin().BStartEnd(wm) +
                                ComputedLogicalBorderPadding().BStartEnd(wm));
  }

  mozilla::LogicalSize
  ComputedSizeWithMarginBorderPadding(mozilla::WritingMode aWM) const {
    return ComputedSizeWithMarginBorderPadding().ConvertTo(aWM,
                                                           GetWritingMode());
  }

  // XXX this will need to change when we make mComputedOffsets logical;
  // we won't be able to return a reference for the physical offsets
  const nsMargin& ComputedPhysicalOffsets() const { return mComputedOffsets; }
  nsMargin& ComputedPhysicalOffsets() { return mComputedOffsets; }

  const LogicalMargin ComputedLogicalOffsets() const
    { return LogicalMargin(mWritingMode, mComputedOffsets); }

  void SetComputedLogicalOffsets(const LogicalMargin& aOffsets)
    { mComputedOffsets = aOffsets.GetPhysicalMargin(mWritingMode); }

private:
  // the available width in which to reflow the frame. The space
  // represents the amount of room for the frame's margin, border,
  // padding, and content area. The frame size you choose should fit
  // within the available width.
  nscoord              mAvailableWidth;

  // A value of NS_UNCONSTRAINEDSIZE for the available height means
  // you can choose whatever size you want. In galley mode the
  // available height is always NS_UNCONSTRAINEDSIZE, and only page
  // mode or multi-column layout involves a constrained height. The
  // element's the top border and padding, and content, must fit. If the
  // element is complete after reflow then its bottom border, padding
  // and margin (and similar for its complete ancestors) will need to
  // fit in this height.
  nscoord              mAvailableHeight;

  // The computed width specifies the frame's content area width, and it does
  // not apply to inline non-replaced elements
  //
  // For replaced inline frames, a value of NS_INTRINSICSIZE means you should
  // use your intrinsic width as the computed width
  //
  // For block-level frames, the computed width is based on the width of the
  // containing block, the margin/border/padding areas, and the min/max width.
  nscoord          mComputedWidth; 

  // The computed height specifies the frame's content height, and it does
  // not apply to inline non-replaced elements
  //
  // For replaced inline frames, a value of NS_INTRINSICSIZE means you should
  // use your intrinsic height as the computed height
  //
  // For non-replaced block-level frames in the flow and floated, a value of
  // NS_AUTOHEIGHT means you choose a height to shrink wrap around the normal
  // flow child frames. The height must be within the limit of the min/max
  // height if there is such a limit
  //
  // For replaced block-level frames, a value of NS_INTRINSICSIZE
  // means you use your intrinsic height as the computed height
  nscoord          mComputedHeight;

  // Computed values for 'left/top/right/bottom' offsets. Only applies to
  // 'positioned' elements. These are PHYSICAL coordinates (for now).
  nsMargin         mComputedOffsets;

  // Computed values for 'min-width/max-width' and 'min-height/max-height'
  // XXXldb The width ones here should go; they should be needed only
  // internally.
  nscoord          mComputedMinWidth, mComputedMaxWidth;
  nscoord          mComputedMinHeight, mComputedMaxHeight;

public:
  // Cached pointers to the various style structs used during intialization
  const nsStyleDisplay*    mStyleDisplay;
  const nsStyleVisibility* mStyleVisibility;
  const nsStylePosition*   mStylePosition;
  const nsStyleBorder*     mStyleBorder;
  const nsStyleMargin*     mStyleMargin;
  const nsStylePadding*    mStylePadding;
  const nsStyleText*       mStyleText;

  bool IsFloating() const;

  uint8_t GetDisplay() const;

  // a frame (e.g. nsTableCellFrame) which may need to generate a special 
  // reflow for percent bsize calculations
  nsIPercentBSizeObserver* mPercentBSizeObserver;

  // CSS margin collapsing sometimes requires us to reflow
  // optimistically assuming that margins collapse to see if clearance
  // is required. When we discover that clearance is required, we
  // store the frame in which clearance was discovered to the location
  // requested here.
  nsIFrame** mDiscoveredClearance;

  // This value keeps track of how deeply nested a given reflow state
  // is from the top of the frame tree.
  int16_t mReflowDepth;

  struct ReflowStateFlags {
    uint16_t mSpecialBSizeReflow:1;  // used by tables to communicate special reflow (in process) to handle
                                     // percent bsize frames inside cells which may not have computed bsizes
    uint16_t mNextInFlowUntouched:1; // nothing in the frame's next-in-flow (or its descendants)
                                     // is changing
    uint16_t mIsTopOfPage:1;         // Is the current context at the top of a
                                     // page?  When true, we force something
                                     // that's too tall for a page/column to
                                     // fit anyway to avoid infinite loops.
    uint16_t mHasClearance:1;        // Block has clearance
    uint16_t mAssumingHScrollbar:1;  // parent frame is an nsIScrollableFrame and it
                                     // is assuming a horizontal scrollbar
    uint16_t mAssumingVScrollbar:1;  // parent frame is an nsIScrollableFrame and it
                                     // is assuming a vertical scrollbar

    uint16_t mIsHResize:1;           // Is frame (a) not dirty and (b) a
                                     // different width than before?

    uint16_t mIsVResize:1;           // Is frame (a) not dirty and (b) a
                                     // different height than before or
                                     // (potentially) in a context where
                                     // percent heights have a different
                                     // basis?
    uint16_t mTableIsSplittable:1;   // tables are splittable, this should happen only inside a page
                                     // and never insider a column frame
    uint16_t mHeightDependsOnAncestorCell:1;   // Does frame height depend on
                                               // an ancestor table-cell?
    uint16_t mIsColumnBalancing:1;   // nsColumnSetFrame is balancing columns
    uint16_t mIsFlexContainerMeasuringHeight:1; // nsFlexContainerFrame is
                                                // reflowing this child to
                                                // measure its intrinsic height.
    uint16_t mDummyParentReflowState:1; // a "fake" reflow state made
                                        // in order to be the parent
                                        // of a real one
    uint16_t mMustReflowPlaceholders:1; // Should this frame reflow its place-
                                        // holder children? If the available
                                        // height of this frame didn't change,
                                        // but its in a paginated environment
                                        // (e.g. columns), it should always
                                        // reflow its placeholder children.
  } mFlags;

  // Logical and physical accessors for the resize flags. All users should go
  // via these accessors, so that in due course we can change the storage from
  // physical to logical.
  bool IsHResize() const {
    return mFlags.mIsHResize;
  }
  bool IsVResize() const {
    return mFlags.mIsVResize;
  }
  bool IsIResize() const {
    return mWritingMode.IsVertical() ? mFlags.mIsVResize : mFlags.mIsHResize;
  }
  bool IsBResize() const {
    return mWritingMode.IsVertical() ? mFlags.mIsHResize : mFlags.mIsVResize;
  }
  void SetHResize(bool aValue) {
    mFlags.mIsHResize = aValue;
  }
  void SetVResize(bool aValue) {
    mFlags.mIsVResize = aValue;
  }
  void SetIResize(bool aValue) {
    if (mWritingMode.IsVertical()) {
      mFlags.mIsVResize = aValue;
    } else {
      mFlags.mIsHResize = aValue;
    }
  }
  void SetBResize(bool aValue) {
    if (mWritingMode.IsVertical()) {
      mFlags.mIsHResize = aValue;
    } else {
      mFlags.mIsVResize = aValue;
    }
  }

  // Note: The copy constructor is written by the compiler automatically. You
  // can use that and then override specific values if you want, or you can
  // call Init as desired...

  /**
   * Initialize a ROOT reflow state.
   *
   * @param aPresContext Must be equal to aFrame->PresContext().
   * @param aFrame The frame for whose reflow state is being constructed.
   * @param aRenderingContext The rendering context to be used for measurements.
   * @param aAvailableSpace See comments for availableHeight and availableWidth
   *        members.
   * @param aFlags A set of flags used for additional boolean parameters (see
   *        below).
   */
  nsHTMLReflowState(nsPresContext*              aPresContext,
                    nsIFrame*                   aFrame,
                    nsRenderingContext*         aRenderingContext,
                    const mozilla::LogicalSize& aAvailableSpace,
                    uint32_t                    aFlags = 0);

  /**
   * Initialize a reflow state for a child frame's reflow. Some parts of the
   * state are copied from the parent's reflow state. The remainder is computed.
   *
   * @param aPresContext Must be equal to aFrame->PresContext().
   * @param aParentReflowState A reference to an nsHTMLReflowState object that
   *        is to be the parent of this object.
   * @param aFrame The frame for whose reflow state is being constructed.
   * @param aAvailableSpace See comments for availableHeight and availableWidth
   *        members.
   * @param aContainingBlockSize An optional size, in app units, that
   *        is used by absolute positioning code to override default containing
   *        block sizes.
   * @param aFlags A set of flags used for additional boolean parameters (see
   *        below).
   */
  nsHTMLReflowState(nsPresContext*              aPresContext,
                    const nsHTMLReflowState&    aParentReflowState,
                    nsIFrame*                   aFrame,
                    const mozilla::LogicalSize& aAvailableSpace,
                    const mozilla::LogicalSize* aContainingBlockSize = nullptr,
                    uint32_t                    aFlags = 0);

  // Values for |aFlags| passed to constructor
  enum {
    // Indicates that the parent of this reflow state is "fake" (see
    // mDummyParentReflowState in mFlags).
    DUMMY_PARENT_REFLOW_STATE = (1<<0),

    // Indicates that the calling function will initialize the reflow state, and
    // that the constructor should not call Init().
    CALLER_WILL_INIT = (1<<1)
  };

  // This method initializes various data members. It is automatically
  // called by the various constructors
  void Init(nsPresContext*              aPresContext,
            const mozilla::LogicalSize* aContainingBlockSize = nullptr,
            const nsMargin*             aBorder = nullptr,
            const nsMargin*             aPadding = nullptr);

  /**
   * Find the content isize of our containing block for the given writing mode,
   * which need not be the same as the reflow state's mode.
   */
  nscoord GetContainingBlockContentISize(mozilla::WritingMode aWritingMode) const;

  /**
   * Calculate the used line-height property. The return value will be >= 0.
   */
  nscoord CalcLineHeight() const;

  /**
   * Same as CalcLineHeight() above, but doesn't need a reflow state.
   *
   * @param aBlockBSize The computed block size of the content rect of the block
   *                     that the line should fill.
   *                     Only used with line-height:-moz-block-height.
   *                     NS_AUTOHEIGHT results in a normal line-height for
   *                     line-height:-moz-block-height.
   * @param aFontSizeInflation The result of the appropriate
   *                           nsLayoutUtils::FontSizeInflationFor call,
   *                           or 1.0 if during intrinsic size
   *                           calculation.
   */
  static nscoord CalcLineHeight(nsIContent* aContent,
                                nsStyleContext* aStyleContext,
                                nscoord aBlockBSize,
                                float aFontSizeInflation);


  mozilla::LogicalSize ComputeContainingBlockRectangle(
         nsPresContext*          aPresContext,
         const nsHTMLReflowState* aContainingBlockRS);

  /**
   * Apply the mComputed(Min/Max)Width constraints to the content
   * size computed so far.
   */
  nscoord ApplyMinMaxWidth(nscoord aWidth) const {
    if (NS_UNCONSTRAINEDSIZE != ComputedMaxWidth()) {
      aWidth = std::min(aWidth, ComputedMaxWidth());
    }
    return std::max(aWidth, ComputedMinWidth());
  }

  /**
   * Apply the mComputed(Min/Max)ISize constraints to the content
   * size computed so far.
   */
  nscoord ApplyMinMaxISize(nscoord aISize) const {
    if (NS_UNCONSTRAINEDSIZE != ComputedMaxISize()) {
      aISize = std::min(aISize, ComputedMaxISize());
    }
    return std::max(aISize, ComputedMinISize());
  }

  /**
   * Apply the mComputed(Min/Max)Height constraints to the content
   * size computed so far.
   *
   * @param aHeight The height that we've computed an to which we want to apply
   *        min/max constraints.
   * @param aConsumed The amount of the computed height that was consumed by
   *        our prev-in-flows.
   */
  nscoord ApplyMinMaxHeight(nscoord aHeight, nscoord aConsumed = 0) const {
    aHeight += aConsumed;

    if (NS_UNCONSTRAINEDSIZE != ComputedMaxHeight()) {
      aHeight = std::min(aHeight, ComputedMaxHeight());
    }

    if (NS_UNCONSTRAINEDSIZE != ComputedMinHeight()) {
      aHeight = std::max(aHeight, ComputedMinHeight());
    }

    return aHeight - aConsumed;
  }

  /**
   * Apply the mComputed(Min/Max)BSize constraints to the content
   * size computed so far.
   *
   * @param aBSize The block-size that we've computed an to which we want to apply
   *        min/max constraints.
   * @param aConsumed The amount of the computed block-size that was consumed by
   *        our prev-in-flows.
   */
  nscoord ApplyMinMaxBSize(nscoord aBSize, nscoord aConsumed = 0) const {
    aBSize += aConsumed;

    if (NS_UNCONSTRAINEDSIZE != ComputedMaxBSize()) {
      aBSize = std::min(aBSize, ComputedMaxBSize());
    }

    if (NS_UNCONSTRAINEDSIZE != ComputedMinBSize()) {
      aBSize = std::max(aBSize, ComputedMinBSize());
    }

    return aBSize - aConsumed;
  }

  bool ShouldReflowAllKids() const {
    // Note that we could make a stronger optimization for IsBResize if
    // we use it in a ShouldReflowChild test that replaces the current
    // checks of NS_FRAME_IS_DIRTY | NS_FRAME_HAS_DIRTY_CHILDREN, if it
    // were tested there along with NS_FRAME_CONTAINS_RELATIVE_BSIZE.
    // This would need to be combined with a slight change in which
    // frames NS_FRAME_CONTAINS_RELATIVE_BSIZE is marked on.
    return (frame->GetStateBits() & NS_FRAME_IS_DIRTY) ||
           IsIResize() ||
           (IsBResize() && 
            (frame->GetStateBits() & NS_FRAME_CONTAINS_RELATIVE_BSIZE));
  }

  // This method doesn't apply min/max computed widths to the value passed in.
  void SetComputedWidth(nscoord aComputedWidth);

  // This method doesn't apply min/max computed heights to the value passed in.
  void SetComputedHeight(nscoord aComputedHeight);

  void SetComputedISize(nscoord aComputedISize) {
    if (mWritingMode.IsVertical()) {
      SetComputedHeight(aComputedISize);
    } else {
      SetComputedWidth(aComputedISize);
    }
  }

  void SetComputedBSize(nscoord aComputedBSize) {
    if (mWritingMode.IsVertical()) {
      SetComputedWidth(aComputedBSize);
    } else {
      SetComputedHeight(aComputedBSize);
    }
  }

  void SetComputedHeightWithoutResettingResizeFlags(nscoord aComputedHeight) {
    // Viewport frames reset the computed height on a copy of their reflow
    // state when reflowing fixed-pos kids.  In that case we actually don't
    // want to mess with the resize flags, because comparing the frame's rect
    // to the munged computed width is pointless.
    ComputedHeight() = aComputedHeight;
  }

  void SetTruncated(const nsHTMLReflowMetrics& aMetrics, nsReflowStatus* aStatus) const;

  bool WillReflowAgainForClearance() const {
    return mDiscoveredClearance && *mDiscoveredClearance;
  }

  // Compute the offsets for a relative position element
  static void ComputeRelativeOffsets(mozilla::WritingMode aWM,
                                     nsIFrame* aFrame,
                                     const mozilla::LogicalSize& aCBSize,
                                     nsMargin& aComputedOffsets);

  // If a relatively positioned element, adjust the position appropriately.
  static void ApplyRelativePositioning(nsIFrame* aFrame,
                                       const nsMargin& aComputedOffsets,
                                       nsPoint* aPosition);

  void ApplyRelativePositioning(nsPoint* aPosition) const {
    ApplyRelativePositioning(frame, ComputedPhysicalOffsets(), aPosition);
  }

  static void
  ApplyRelativePositioning(nsIFrame* aFrame,
                           mozilla::WritingMode aWritingMode,
                           const mozilla::LogicalMargin& aComputedOffsets,
                           mozilla::LogicalPoint* aPosition,
                           nscoord aContainerWidth) {
    // Subtract the width of the frame from the container width that we
    // use for converting between the logical and physical origins of
    // the frame. This accounts for the fact that logical origins in RTL
    // coordinate systems are at the top right of the frame instead of
    // the top left.
    nscoord frameWidth = aFrame->GetSize().width;
    nsPoint pos = aPosition->GetPhysicalPoint(aWritingMode,
                                              aContainerWidth - frameWidth);
    ApplyRelativePositioning(aFrame,
                             aComputedOffsets.GetPhysicalMargin(aWritingMode),
                             &pos);
    *aPosition = mozilla::LogicalPoint(aWritingMode, pos,
                                       aContainerWidth - frameWidth);
  }

  void ApplyRelativePositioning(mozilla::LogicalPoint* aPosition,
                                nscoord aContainerWidth) const {
    ApplyRelativePositioning(frame, mWritingMode,
                             ComputedLogicalOffsets(), aPosition,
                             aContainerWidth);
  }

#ifdef DEBUG
  // Reflow trace methods.  Defined in nsFrame.cpp so they have access
  // to the display-reflow infrastructure.
  static void* DisplayInitConstraintsEnter(nsIFrame* aFrame,
                                           nsHTMLReflowState* aState,
                                           nscoord aCBISize,
                                           nscoord aCBBSize,
                                           const nsMargin* aBorder,
                                           const nsMargin* aPadding);
  static void DisplayInitConstraintsExit(nsIFrame* aFrame,
                                         nsHTMLReflowState* aState,
                                         void* aValue);
  static void* DisplayInitFrameTypeEnter(nsIFrame* aFrame,
                                         nsHTMLReflowState* aState);
  static void DisplayInitFrameTypeExit(nsIFrame* aFrame,
                                       nsHTMLReflowState* aState,
                                       void* aValue);
#endif

protected:
  void InitFrameType(nsIAtom* aFrameType);
  void InitCBReflowState();
  void InitResizeFlags(nsPresContext* aPresContext, nsIAtom* aFrameType);

  void InitConstraints(nsPresContext*              aPresContext,
                       const mozilla::LogicalSize& aContainingBlockSize,
                       const nsMargin*             aBorder,
                       const nsMargin*             aPadding,
                       nsIAtom*                    aFrameType);

  // Returns the nearest containing block or block frame (whether or not
  // it is a containing block) for the specified frame.  Also returns
  // the inline-start edge and inline size of the containing block's
  // content area.
  // These are returned in the coordinate space of the containing block.
  nsIFrame* GetHypotheticalBoxContainer(nsIFrame* aFrame,
                                        nscoord& aCBIStartEdge,
                                        nscoord& aCBISize);

  void CalculateHypotheticalBox(nsPresContext*    aPresContext,
                                nsIFrame*         aPlaceholderFrame,
                                const nsHTMLReflowState* cbrs,
                                nsHypotheticalBox& aHypotheticalBox,
                                nsIAtom*          aFrameType);

  void InitAbsoluteConstraints(nsPresContext* aPresContext,
                               const nsHTMLReflowState* cbrs,
                               const mozilla::LogicalSize& aContainingBlockSize,
                               nsIAtom* aFrameType);

  // Calculates the computed values for the 'min-Width', 'max-Width',
  // 'min-Height', and 'max-Height' properties, and stores them in the assorted
  // data members
  void ComputeMinMaxValues(const mozilla::LogicalSize& aContainingBlockSize);

  void CalculateInlineBorderPaddingMargin(nscoord aContainingBlockISize,
                                          nscoord* aInsideBoxSizing,
                                          nscoord* aOutsideBoxSizing);

  void CalculateBlockSideMargins(nsIAtom* aFrameType);
};

#endif /* nsHTMLReflowState_h___ */

