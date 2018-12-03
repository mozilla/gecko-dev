/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* rendering object for CSS "display: flex" and "display: -webkit-box" */

#ifndef nsFlexContainerFrame_h___
#define nsFlexContainerFrame_h___

#include "nsContainerFrame.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/dom/FlexBinding.h"

class nsStyleCoord;

namespace mozilla {
template <class T>
class LinkedList;
class LogicalPoint;
}  // namespace mozilla

nsContainerFrame* NS_NewFlexContainerFrame(nsIPresShell* aPresShell,
                                           mozilla::ComputedStyle* aStyle);

/**
 * These structures are used to capture data during reflow to be
 * extracted by devtools via Chrome APIs. The structures are only
 * created when requested in GetFlexFrameWithComputedInfo(), and
 * the structures are attached to the nsFlexContainerFrame via the
 * FlexContainerInfo property.
 */
struct ComputedFlexItemInfo {
  nsCOMPtr<nsINode> mNode;
  nsRect mFrameRect;
  /**
   * mMainBaseSize is a measure of the size of the item in the main
   * axis before the flex sizing algorithm is applied. In the spec,
   * this is called "flex base size", but we use this name to connect
   * the value to the other main axis sizes.
   */
  nscoord mMainBaseSize;
  /**
   * mMainDeltaSize is the amount that the flex sizing algorithm
   * adds to the mMainBaseSize, before clamping to mMainMinSize and
   * mMainMaxSize. This can be thought of as the amount by which the
   * flex layout algorithm "wants" to shrink or grow the item, and
   * would do, if it was unconstrained. Since the flex sizing
   * algorithm proceeds linearly, the mMainDeltaSize for an item only
   * respects the resolved size of items already frozen.
   */
  nscoord mMainDeltaSize;
  nscoord mMainMinSize;
  nscoord mMainMaxSize;
  nscoord mCrossMinSize;
  nscoord mCrossMaxSize;
  mozilla::dom::FlexItemClampState mClampState;
};

struct ComputedFlexLineInfo {
  nsTArray<ComputedFlexItemInfo> mItems;
  nscoord mCrossStart;
  nscoord mCrossSize;
  nscoord mFirstBaselineOffset;
  nscoord mLastBaselineOffset;
  mozilla::dom::FlexLineGrowthState mGrowthState;
};

struct ComputedFlexContainerInfo {
  nsTArray<ComputedFlexLineInfo> mLines;
  mozilla::dom::FlexPhysicalDirection mMainAxisDirection;
  mozilla::dom::FlexPhysicalDirection mCrossAxisDirection;
};

/**
 * This is the rendering object used for laying out elements with
 * "display: flex" or "display: inline-flex".
 *
 * We also use this class for elements with "display: -webkit-box" or
 * "display: -webkit-inline-box" (but not "-moz-box" / "-moz-inline-box" --
 * those are rendered with old-school XUL frame classes).
 *
 * Note: we represent the -webkit-box family of properties (-webkit-box-orient,
 * -webkit-box-flex, etc.) as aliases for their -moz equivalents.  And for
 * -webkit-{inline-}box containers, nsFlexContainerFrame will honor those
 * "legacy" properties for alignment/flexibility/etc. *instead of* honoring the
 * modern flexbox & alignment properties.  For brevity, many comments in
 * nsFlexContainerFrame.cpp simply refer to these properties using their
 * "-webkit" versions, since we're mostly expecting to encounter them in that
 * form. (Technically, the "-moz" versions of these properties *can* influence
 * layout here as well (since that's what the -webkit versions are aliased to)
 * -- but only inside of a "display:-webkit-{inline-}box" container.)
 */
class nsFlexContainerFrame final : public nsContainerFrame {
 public:
  NS_DECL_FRAMEARENA_HELPERS(nsFlexContainerFrame)
  NS_DECL_QUERYFRAME

  // Factory method:
  friend nsContainerFrame* NS_NewFlexContainerFrame(nsIPresShell* aPresShell,
                                                    ComputedStyle* aStyle);

  // Forward-decls of helper classes
  class FlexItem;
  class FlexLine;
  class FlexboxAxisTracker;
  struct StrutInfo;
  class CachedMeasuringReflowResult;

  // nsIFrame overrides
  void Init(nsIContent* aContent, nsContainerFrame* aParent,
            nsIFrame* aPrevInFlow) override;

  void BuildDisplayList(nsDisplayListBuilder* aBuilder,
                        const nsDisplayListSet& aLists) override;

  void MarkIntrinsicISizesDirty() override;

  void Reflow(nsPresContext* aPresContext, ReflowOutput& aDesiredSize,
              const ReflowInput& aReflowInput,
              nsReflowStatus& aStatus) override;

  void DidReflow(nsPresContext*, const ReflowInput*) final;

  nscoord GetMinISize(gfxContext* aRenderingContext) override;
  nscoord GetPrefISize(gfxContext* aRenderingContext) override;

#ifdef DEBUG_FRAME_DUMP
  nsresult GetFrameName(nsAString& aResult) const override;
#endif

  nscoord GetLogicalBaseline(mozilla::WritingMode aWM) const override;

  bool GetVerticalAlignBaseline(mozilla::WritingMode aWM,
                                nscoord* aBaseline) const override {
    return GetNaturalBaselineBOffset(aWM, BaselineSharingGroup::eFirst,
                                     aBaseline);
  }

  bool GetNaturalBaselineBOffset(mozilla::WritingMode aWM,
                                 BaselineSharingGroup aBaselineGroup,
                                 nscoord* aBaseline) const override {
    if (HasAnyStateBits(NS_STATE_FLEX_SYNTHESIZE_BASELINE)) {
      return false;
    }
    *aBaseline = aBaselineGroup == BaselineSharingGroup::eFirst
                     ? mBaselineFromLastReflow
                     : mLastBaselineFromLastReflow;
    return true;
  }

  // nsContainerFrame overrides
  uint16_t CSSAlignmentForAbsPosChild(
      const ReflowInput& aChildRI,
      mozilla::LogicalAxis aLogicalAxis) const override;

  /**
   * Helper function to calculate packing space and initial offset of alignment
   * subjects in MainAxisPositionTracker() and CrossAxisPositionTracker() for
   * space-between, space-around, and space-evenly.
   *    * @param aNumThingsToPack             Number of alignment subjects.
   * @param aAlignVal                    Value for align-self or justify-self.
   * @param aFirstSubjectOffset          Outparam for first subject offset.
   * @param aNumPackingSpacesRemaining   Outparam for number of equal-sized
   *                                     packing spaces to apply between each
   *                                     alignment subject.
   * @param aPackingSpaceRemaining       Outparam for total amount of packing
   *                                     space to be divided up.
   */
  static void CalculatePackingSpace(uint32_t aNumThingsToPack,
                                    uint8_t aAlignVal,
                                    nscoord* aFirstSubjectOffset,
                                    uint32_t* aNumPackingSpacesRemaining,
                                    nscoord* aPackingSpaceRemaining);

  /**
   * This property is created by a call to
   * nsFlexContainerFrame::GetFlexFrameWithComputedInfo.
   */
  NS_DECLARE_FRAME_PROPERTY_DELETABLE(FlexContainerInfo,
                                      ComputedFlexContainerInfo)
  /**
   * This function should only be called on a nsFlexContainerFrame
   * that has just been returned by a call to
   * GetFlexFrameWithComputedInfo.
   */
  const ComputedFlexContainerInfo* GetFlexContainerInfo() {
    const ComputedFlexContainerInfo* info = GetProperty(FlexContainerInfo());
    MOZ_ASSERT(info, "Property generation wasn't requested.");
    return info;
  }

  /**
   * Return aFrame as a flex frame after ensuring it has computed flex info.
   * @return nullptr if aFrame is null or doesn't have a flex frame
   *         as its content insertion frame.
   * @note this might destroy layout/style data since it may flush layout.
   */
  static nsFlexContainerFrame* GetFlexFrameWithComputedInfo(nsIFrame* aFrame);

  /**
   * Given a frame for a flex item, this method returns true IFF that flex
   * item's inline axis is the same as (i.e. not orthogonal to) its flex
   * container's main axis.
   *
   * (This method is only intended to be used from external
   * callers. Inside of flex reflow code, FlexItem::IsInlineAxisMainAxis() is
   * equivalent & more optimal.)
   *
   * @param aFrame a flex item (must return true from IsFlexItem)
   * @return true iff aFrame's inline axis is the same as (i.e. not orthogonal
   *              to) its flex container's main axis. Otherwise, false.
   */
  static bool IsItemInlineAxisMainAxis(nsIFrame* aFrame);

  /**
   * Returns true iff the given computed 'flex-basis' & main-size property
   * values collectively represent a used flex-basis of 'content'.
   * See https://drafts.csswg.org/css-flexbox-1/#valdef-flex-basis-auto
   *
   * @param aFlexBasis the computed 'flex-basis' for a flex item.
   * @param aMainSize the computed main-size property for a flex item.
   */
  static bool IsUsedFlexBasisContent(const nsStyleCoord* aFlexBasis,
                                     const nsStyleCoord* aMainSize);

  /**
   * Callback for nsFrame::MarkIntrinsicISizesDirty() on a flex item.
   */
  static void MarkCachedFlexMeasurementsDirty(nsIFrame* aItemFrame);

 protected:
  // Protected constructor & destructor
  explicit nsFlexContainerFrame(ComputedStyle* aStyle)
      : nsContainerFrame(aStyle, kClassID),
        mCachedMinISize(NS_INTRINSIC_WIDTH_UNKNOWN),
        mCachedPrefISize(NS_INTRINSIC_WIDTH_UNKNOWN),
        mBaselineFromLastReflow(NS_INTRINSIC_WIDTH_UNKNOWN),
        mLastBaselineFromLastReflow(NS_INTRINSIC_WIDTH_UNKNOWN) {}

  virtual ~nsFlexContainerFrame();

  /*
   * This method does the bulk of the flex layout, implementing the algorithm
   * described at:
   *   http://dev.w3.org/csswg/css-flexbox/#layout-algorithm
   * (with a few initialization pieces happening in the caller, Reflow().
   *
   * Since this is a helper for Reflow(), this takes all the same parameters
   * as Reflow(), plus a few more parameters that Reflow() sets up for us.
   *
   * (The logic behind the division of work between Reflow and DoFlexLayout is
   * as follows: DoFlexLayout() begins at the step that we have to jump back
   * to, if we find any visibility:collapse children, and Reflow() does
   * everything before that point.)
   */
  void DoFlexLayout(nsPresContext* aPresContext, ReflowOutput& aDesiredSize,
                    const ReflowInput& aReflowInput, nsReflowStatus& aStatus,
                    nscoord aContentBoxMainSize,
                    nscoord aAvailableBSizeForContent,
                    nsTArray<StrutInfo>& aStruts,
                    const FlexboxAxisTracker& aAxisTracker,
                    nscoord aMainGapSize, nscoord aCrossGapSize);

  /**
   * Checks whether our child-frame list "mFrames" is sorted, using the given
   * IsLessThanOrEqual function, and sorts it if it's not already sorted.
   *
   * XXXdholbert Once we support pagination, we need to make this function
   * check our continuations as well (or wrap it in a function that does).
   *
   * @return true if we had to sort mFrames, false if it was already sorted.
   */
  template <bool IsLessThanOrEqual(nsIFrame*, nsIFrame*)>
  bool SortChildrenIfNeeded();

  // Protected flex-container-specific methods / member-vars
#ifdef DEBUG
  void SanityCheckAnonymousFlexItems() const;
#endif  // DEBUG

  /*
   * Returns a new FlexItem for the given child frame, allocated on the heap.
   * Guaranteed to return non-null. Caller is responsible for managing the
   * FlexItem's lifetime.
   *
   * Before returning, this method also processes the FlexItem to resolve its
   * flex basis (including e.g. auto-height) as well as to resolve
   * "min-height:auto", via ResolveAutoFlexBasisAndMinSize(). (Basically, the
   * returned FlexItem will be ready to participate in the "Resolve the
   * Flexible Lengths" step of the Flex Layout Algorithm.)
   */
  mozilla::UniquePtr<FlexItem> GenerateFlexItemForChild(
      nsPresContext* aPresContext, nsIFrame* aChildFrame,
      const ReflowInput& aParentReflowInput,
      const FlexboxAxisTracker& aAxisTracker);

  /**
   * This method gets a cached measuring reflow for a flex item, or does it and
   * caches it.
   *
   * This avoids exponential reflows, see the comment on
   * CachedMeasuringReflowResult.
   */
  const CachedMeasuringReflowResult& MeasureAscentAndBSizeForFlexItem(
      FlexItem& aItem, nsPresContext* aPresContext,
      ReflowInput& aChildReflowInput);

  /**
   * This method performs a "measuring" reflow to get the content BSize of
   * aFlexItem.Frame() (treating it as if it had a computed BSize of "auto"),
   * and returns the resulting BSize measurement.
   * (Helper for ResolveAutoFlexBasisAndMinSize().)
   */
  nscoord MeasureFlexItemContentBSize(nsPresContext* aPresContext,
                                      FlexItem& aFlexItem,
                                      bool aForceBResizeForMeasuringReflow,
                                      const ReflowInput& aParentReflowInput);

  /**
   * This method resolves an "auto" flex-basis and/or min-main-size value
   * on aFlexItem, if needed.
   * (Helper for GenerateFlexItemForChild().)
   */
  void ResolveAutoFlexBasisAndMinSize(nsPresContext* aPresContext,
                                      FlexItem& aFlexItem,
                                      const ReflowInput& aItemReflowInput,
                                      const FlexboxAxisTracker& aAxisTracker);

  /**
   * Returns true if "this" is the nsFlexContainerFrame for a -moz-box or
   * a -moz-inline-box -- these boxes have special behavior for flex items with
   * "visibility:collapse".
   *
   * @param aFlexStyleDisp This frame's StyleDisplay(). (Just an optimization to
   *                       avoid repeated lookup; some callers already have it.)
   * @return true if "this" is the nsFlexContainerFrame for a -moz-{inline}box.
   */
  bool ShouldUseMozBoxCollapseBehavior(const nsStyleDisplay* aFlexStyleDisp);

  /**
   * This method:
   *  - Creates FlexItems for all of our child frames (except placeholders).
   *  - Groups those FlexItems into FlexLines.
   *  - Returns those FlexLines in the outparam |aLines|.
   *
   * For any child frames which are placeholders, this method will instead just
   * append that child to the outparam |aPlaceholders| for separate handling.
   * (Absolutely positioned children of a flex container are *not* flex items.)
   */
  void GenerateFlexLines(nsPresContext* aPresContext,
                         const ReflowInput& aReflowInput,
                         nscoord aContentBoxMainSize,
                         nscoord aAvailableBSizeForContent,
                         const nsTArray<StrutInfo>& aStruts,
                         const FlexboxAxisTracker& aAxisTracker,
                         nscoord aMainGapSize,
                         nsTArray<nsIFrame*>& aPlaceholders,
                         mozilla::LinkedList<FlexLine>& aLines);

  nscoord GetMainSizeFromReflowInput(const ReflowInput& aReflowInput,
                                     const FlexboxAxisTracker& aAxisTracker);

  nscoord ComputeCrossSize(const ReflowInput& aReflowInput,
                           const FlexboxAxisTracker& aAxisTracker,
                           nscoord aSumLineCrossSizes,
                           nscoord aAvailableBSizeForContent, bool* aIsDefinite,
                           nsReflowStatus& aStatus);

  void SizeItemInCrossAxis(nsPresContext* aPresContext,
                           const FlexboxAxisTracker& aAxisTracker,
                           ReflowInput& aChildReflowInput, FlexItem& aItem);

  /**
   * Moves the given flex item's frame to the given LogicalPosition (modulo any
   * relative positioning).
   *
   * This can be used in cases where we've already done a "measuring reflow"
   * for the flex item at the correct size, and hence can skip its final reflow
   * (but still need to move it to the right final position).
   *
   * @param aReflowInput    The flex container's reflow state.
   * @param aItem           The flex item whose frame should be moved.
   * @param aFramePos       The position where the flex item's frame should
   *                        be placed. (pre-relative positioning)
   * @param aContainerSize  The flex container's size (required by some methods
   *                        that we call, to interpret aFramePos correctly).
   */
  void MoveFlexItemToFinalPosition(const ReflowInput& aReflowInput,
                                   const FlexItem& aItem,
                                   mozilla::LogicalPoint& aFramePos,
                                   const nsSize& aContainerSize);
  /**
   * Helper-function to reflow a child frame, at its final position determined
   * by flex layout.
   *
   * @param aPresContext    The presentation context being used in reflow.
   * @param aAxisTracker    A FlexboxAxisTracker with the flex container's axes.
   * @param aReflowInput    The flex container's reflow state.
   * @param aItem           The flex item to be reflowed.
   * @param aFramePos       The position where the flex item's frame should
   *                        be placed. (pre-relative positioning)
   * @param aContainerSize  The flex container's size (required by some methods
   *                        that we call, to interpret aFramePos correctly).
   */
  void ReflowFlexItem(nsPresContext* aPresContext,
                      const FlexboxAxisTracker& aAxisTracker,
                      const ReflowInput& aReflowInput, const FlexItem& aItem,
                      mozilla::LogicalPoint& aFramePos,
                      const nsSize& aContainerSize);

  /**
   * Helper-function to perform a "dummy reflow" on all our nsPlaceholderFrame
   * children, at the container's content-box origin.
   *
   * This doesn't actually represent the static position of the placeholders'
   * out-of-flow (OOF) frames -- we can't compute that until we've reflowed the
   * OOF, because (depending on the CSS Align properties) the static position
   * may be influenced by the OOF's size.  So for now, we just co-opt the
   * placeholder to store the flex container's logical content-box origin, and
   * we defer to nsAbsoluteContainingBlock to determine the OOF's actual static
   * position (using this origin, the OOF's size, and the CSS Align
   * properties).
   *
   * @param aPresContext       The presentation context being used in reflow.
   * @param aReflowInput       The flex container's reflow input.
   * @param aPlaceholders      An array of all the flex container's
   *                           nsPlaceholderFrame children.
   * @param aContentBoxOrigin  The flex container's logical content-box
   *                           origin (in its own coordinate space).
   * @param aContainerSize     The flex container's size (required by some
   *                           reflow methods to interpret positions correctly).
   */
  void ReflowPlaceholders(nsPresContext* aPresContext,
                          const ReflowInput& aReflowInput,
                          nsTArray<nsIFrame*>& aPlaceholders,
                          const mozilla::LogicalPoint& aContentBoxOrigin,
                          const nsSize& aContainerSize);

  /**
   * Helper for GetMinISize / GetPrefISize.
   */
  nscoord IntrinsicISize(gfxContext* aRenderingContext,
                         nsLayoutUtils::IntrinsicISizeType aType);

  /**
   * Cached values to optimize GetMinISize/GetPrefISize.
   */
  nscoord mCachedMinISize;
  nscoord mCachedPrefISize;

  nscoord mBaselineFromLastReflow;
  // Note: the last baseline is a distance from our border-box end edge.
  nscoord mLastBaselineFromLastReflow;
};

#endif /* nsFlexContainerFrame_h___ */
