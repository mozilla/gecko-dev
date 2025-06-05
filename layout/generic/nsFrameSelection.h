/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsFrameSelection_h___
#define nsFrameSelection_h___

#include <stdint.h>
#include "mozilla/intl/BidiEmbeddingLevel.h"
#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/CaretAssociationHint.h"
#include "mozilla/CompactPair.h"
#include "mozilla/EnumSet.h"
#include "mozilla/EventForwards.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/Highlight.h"
#include "mozilla/dom/Selection.h"
#include "mozilla/Result.h"
#include "mozilla/TextRange.h"
#include "mozilla/UniquePtr.h"
#include "nsIFrame.h"
#include "nsIContent.h"
#include "nsISelectionController.h"
#include "nsISelectionListener.h"
#include "nsITableCellLayout.h"
#include "WordMovementType.h"
#include "nsBidiPresUtils.h"

class nsRange;

#define BIDI_LEVEL_UNDEFINED mozilla::intl::BidiEmbeddingLevel(0x80)

//----------------------------------------------------------------------

// Selection interface

struct SelectionDetails {
  SelectionDetails()
      : mStart(), mEnd(), mSelectionType(mozilla::SelectionType::eInvalid) {
    MOZ_COUNT_CTOR(SelectionDetails);
  }
  MOZ_COUNTED_DTOR(SelectionDetails)

  int32_t mStart;
  int32_t mEnd;
  mozilla::SelectionType mSelectionType;
  mozilla::dom::HighlightSelectionData mHighlightData;
  mozilla::TextRangeStyle mTextRangeStyle;
  mozilla::UniquePtr<SelectionDetails> mNext;
};

struct SelectionCustomColors {
#ifdef NS_BUILD_REFCNT_LOGGING
  MOZ_COUNTED_DEFAULT_CTOR(SelectionCustomColors)
  MOZ_COUNTED_DTOR(SelectionCustomColors)
#endif
  mozilla::Maybe<nscolor> mForegroundColor;
  mozilla::Maybe<nscolor> mBackgroundColor;
  mozilla::Maybe<nscolor> mAltForegroundColor;
  mozilla::Maybe<nscolor> mAltBackgroundColor;
};

namespace mozilla {
class PresShell;

/** PeekOffsetStruct is used to group various arguments (both input and output)
 *  that are passed to nsIFrame::PeekOffset(). See below for the description of
 *  individual arguments.
 */
enum class PeekOffsetOption : uint16_t {
  // Whether to allow jumping across line boundaries.
  //
  // Used with: eSelectCharacter, eSelectWord.
  JumpLines,

  // Whether we should preserve or trim spaces at begin/end of content
  PreserveSpaces,

  // Whether to stop when reaching a scroll view boundary.
  //
  // Used with: eSelectCharacter, eSelectWord, eSelectLine.
  StopAtScroller,

  // Whether to stop when reaching a placeholder frame.
  StopAtPlaceholder,

  // Whether the peeking is done in response to a keyboard action.
  //
  // Used with: eSelectWord.
  IsKeyboardSelect,

  // Whether bidi caret behavior is visual (set) or logical (unset).
  //
  // Used with: eSelectCharacter, eSelectWord, eSelectBeginLine, eSelectEndLine.
  Visual,

  // Whether the selection is being extended or moved.
  Extend,

  // If true, the offset has to end up in an editable node, otherwise we'll keep
  // searching.
  ForceEditableRegion,
};

using PeekOffsetOptions = EnumSet<PeekOffsetOption>;

struct MOZ_STACK_CLASS PeekOffsetStruct {
  PeekOffsetStruct(nsSelectionAmount aAmount, nsDirection aDirection,
                   int32_t aStartOffset, nsPoint aDesiredCaretPos,
                   // Passing by value here is intentional because EnumSet
                   // is optimized as uint*_t in opt builds.
                   const PeekOffsetOptions aOptions,
                   EWordMovementType aWordMovementType = eDefaultBehavior,
                   const dom::Element* aAncestorLimiter = nullptr);

  /**
   * Return true if the ancestor limiter is not specified or if the content for
   * aFrame is an inclusive descendant of mAncestorLimiter.
   */
  [[nodiscard]] bool FrameContentIsInAncestorLimiter(
      const nsIFrame* aFrame) const {
    return !mAncestorLimiter ||
           (aFrame->GetContent() &&
            aFrame->GetContent()->IsInclusiveDescendantOf(mAncestorLimiter));
  }

  // Note: Most arguments (input and output) are only used with certain values
  // of mAmount. These values are indicated for each argument below.
  // Arguments with no such indication are used with all values of mAmount.

  /*** Input arguments ***/
  // Note: The value of some of the input arguments may be changed upon exit.

  // The type of movement requested (by character, word, line, etc.)
  nsSelectionAmount mAmount;

  // eDirPrevious or eDirNext.
  //
  // Note for visual bidi movement:
  //   * eDirPrevious means 'left-then-up' if the containing block is LTR,
  //     'right-then-up' if it is RTL.
  //   * eDirNext means 'right-then-down' if the containing block is LTR,
  //     'left-then-down' if it is RTL.
  //   * Between paragraphs, eDirPrevious means "go to the visual end of
  //     the previous paragraph", and eDirNext means "go to the visual
  //     beginning of the next paragraph".
  //
  // Used with: eSelectCharacter, eSelectWord, eSelectLine, eSelectParagraph.
  const nsDirection mDirection;

  // Offset into the content of the current frame where the peek starts.
  //
  // Used with: eSelectCharacter, eSelectWord
  int32_t mStartOffset;

  // The desired inline coordinate for the caret (one of .x or .y will be used,
  // depending on line's writing mode)
  //
  // Used with: eSelectLine.
  const nsPoint mDesiredCaretPos;

  // An enum that determines whether to prefer the start or end of a word or to
  // use the default behavior, which is a combination of direction and the
  // platform-based pref "layout.word_select.eat_space_to_next_word"
  EWordMovementType mWordMovementType;

  PeekOffsetOptions mOptions;

  // The ancestor limiter element to peek offset.
  const dom::Element* const mAncestorLimiter;

  /*** Output arguments ***/

  // Content reached as a result of the peek.
  nsCOMPtr<nsIContent> mResultContent;

  // Frame reached as a result of the peek.
  //
  // Used with: eSelectCharacter, eSelectWord.
  nsIFrame* mResultFrame;

  // Offset into content reached as a result of the peek.
  int32_t mContentOffset;

  // When the result position is between two frames, indicates which of the two
  // frames the caret should be painted in. false means "the end of the frame
  // logically before the caret", true means "the beginning of the frame
  // logically after the caret".
  //
  // Used with: eSelectLine, eSelectBeginLine, eSelectEndLine.
  CaretAssociationHint mAttach;
};

struct LimitersAndCaretData;

}  // namespace mozilla

struct nsPrevNextBidiLevels {
  void SetData(nsIFrame* aFrameBefore, nsIFrame* aFrameAfter,
               mozilla::intl::BidiEmbeddingLevel aLevelBefore,
               mozilla::intl::BidiEmbeddingLevel aLevelAfter) {
    mFrameBefore = aFrameBefore;
    mFrameAfter = aFrameAfter;
    mLevelBefore = aLevelBefore;
    mLevelAfter = aLevelAfter;
  }
  nsIFrame* mFrameBefore;
  nsIFrame* mFrameAfter;
  mozilla::intl::BidiEmbeddingLevel mLevelBefore;
  mozilla::intl::BidiEmbeddingLevel mLevelAfter;
};

namespace mozilla {
class SelectionChangeEventDispatcher;
namespace dom {
class Highlight;
class Selection;
enum class ClickSelectionType { NotApplicable, Double, Triple };
}  // namespace dom

/**
 * Constants for places that want to handle table selections.  These
 * indicate what part of a table is being selected.
 */
enum class TableSelectionMode : uint32_t {
  None,     /* Nothing being selected; not valid in all cases. */
  Cell,     /* A cell is being selected. */
  Row,      /* A row is being selected. */
  Column,   /* A column is being selected. */
  Table,    /* A table (including cells and captions) is being selected. */
  AllCells, /* All the cells in a table are being selected. */
};

}  // namespace mozilla

class nsFrameSelection final {
 public:
  friend std::ostream& operator<<(std::ostream&, const nsFrameSelection&);

  using CaretAssociationHint = mozilla::CaretAssociationHint;
  using Element = mozilla::dom::Element;

  /*interfaces for addref and release and queryinterface*/

  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(nsFrameSelection)
  NS_DECL_CYCLE_COLLECTION_NATIVE_CLASS(nsFrameSelection)

  enum class FocusMode {
    kExtendSelection,     /** Keep old anchor point. */
    kCollapseToNewPoint,  /** Collapses the Selection to the new point. */
    kMultiRangeSelection, /** Keeps existing non-collapsed ranges and marks them
                             as generated. */
  };

  /**
   * HandleClick will take the focus to the new frame at the new offset and
   * will either extend the selection from the old anchor, or replace the old
   * anchor. the old anchor and focus position may also be used to deselect
   * things
   *
   * @param aNewfocus is the content that wants the focus
   *
   * @param aContentOffset is the content offset of the parent aNewFocus
   *
   * @param aContentOffsetEnd is the content offset of the parent aNewFocus and
   * is specified different when you need to select to and include both start
   * and end points
   *
   * @param aHint will tell the selection which direction geometrically to
   * actually show the caret on. 1 = end of this line 0 = beginning of this line
   */
  MOZ_CAN_RUN_SCRIPT nsresult HandleClick(nsIContent* aNewFocus,
                                          uint32_t aContentOffset,
                                          uint32_t aContentEndOffset,
                                          FocusMode aFocusMode,
                                          CaretAssociationHint aHint);

 public:
  /**
   * Sets the type of the selection based on whether a selection is created
   * by doubleclick, long tapping a word or tripleclick.
   *
   * @param aClickSelectionType   ClickSelectionType::Double if the selection
   *                              is created by doubleclick,
   *                              ClickSelectionType::Triple if the selection
   *                              is created by tripleclick.
   */
  void SetClickSelectionType(
      mozilla::dom::ClickSelectionType aClickSelectionType) {
    mClickSelectionType = aClickSelectionType;
  }

  /**
   * Return true if this is an instance for an independent selection.
   * Currently, independent selection is created only in the text controls
   * to manage selections in their native anonymous subtree.
   */
  [[nodiscard]] bool IsIndependentSelection() const {
    return !!GetIndependentSelectionRootElement();
  }

  /**
   * Returns true if the selection was created by doubleclick or
   * long tap over a word.
   */
  [[nodiscard]] bool IsDoubleClickSelection() const {
    return mClickSelectionType == mozilla::dom::ClickSelectionType::Double;
  }

  /**
   * Returns true if the selection was created by triple click
   */
  [[nodiscard]] bool IsTripleClickSelection() const {
    return mClickSelectionType == mozilla::dom::ClickSelectionType::Triple;
  }

  /**
   * HandleDrag extends the selection to contain the frame closest to aPoint.
   *
   * @param aPresContext is the context to use when figuring out what frame
   * contains the point.
   *
   * @param aFrame is the parent of all frames to use when searching for the
   * closest frame to the point.
   *
   * @param aPoint is relative to aFrame
   */
  MOZ_CAN_RUN_SCRIPT void HandleDrag(nsIFrame* aFrame, const nsPoint& aPoint);

  /**
   * HandleTableSelection will set selection to a table, cell, etc
   * depending on information contained in aFlags
   *
   * @param aParentContent is the paretent of either a table or cell that user
   * clicked or dragged the mouse in
   *
   * @param aContentOffset is the offset of the table or cell
   *
   * @param aTarget indicates what to select
   *   * TableSelectionMode::Cell
   *       We should select a cell (content points to the cell)
   *   * TableSelectionMode::Row
   *       We should select a row (content points to any cell in row)
   *   * TableSelectionMode::Column
   *       We should select a row (content points to any cell in column)
   *   * TableSelectionMode::Table
   *       We should select a table (content points to the table)
   *   * TableSelectionMode::AllCells
   *       We should select all cells (content points to any cell in table)
   *
   * @param aMouseEvent passed in so we can get where event occurred
   * and what keys are pressed
   */
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT nsresult
  HandleTableSelection(nsINode* aParentContent, int32_t aContentOffset,
                       mozilla::TableSelectionMode aTarget,
                       mozilla::WidgetMouseEvent* aMouseEvent);

  /**
   * Add cell to the selection with `SelectionType::eNormal`.
   *
   * @param  aCell  [in] HTML td element.
   */
  nsresult SelectCellElement(nsIContent* aCell);

 public:
  /**
   * Remove cells from selection inside of the given cell range.
   *
   * @param  aTable             [in] HTML table element
   * @param  aStartRowIndex     [in] row index where the cells range starts
   * @param  aStartColumnIndex  [in] column index where the cells range starts
   * @param  aEndRowIndex       [in] row index where the cells range ends
   * @param  aEndColumnIndex    [in] column index where the cells range ends
   */
  MOZ_CAN_RUN_SCRIPT nsresult RemoveCellsFromSelection(
      nsIContent* aTable, int32_t aStartRowIndex, int32_t aStartColumnIndex,
      int32_t aEndRowIndex, int32_t aEndColumnIndex);

  /**
   * Remove cells from selection outside of the given cell range.
   *
   * @param  aTable             [in] HTML table element
   * @param  aStartRowIndex     [in] row index where the cells range starts
   * @param  aStartColumnIndex  [in] column index where the cells range starts
   * @param  aEndRowIndex       [in] row index where the cells range ends
   * @param  aEndColumnIndex    [in] column index where the cells range ends
   */
  MOZ_CAN_RUN_SCRIPT nsresult RestrictCellsToSelection(
      nsIContent* aTable, int32_t aStartRowIndex, int32_t aStartColumnIndex,
      int32_t aEndRowIndex, int32_t aEndColumnIndex);

  /**
   * StartAutoScrollTimer is responsible for scrolling frames so that
   * aPoint is always visible, and for selecting any frame that contains
   * aPoint. The timer will also reset itself to fire again if we have
   * not scrolled to the end of the document.
   *
   * @param aFrame is the outermost frame to use when searching for
   * the closest frame for the point, i.e. the frame that is capturing
   * the mouse
   *
   * @param aPoint is relative to aFrame.
   *
   * @param aDelay is the timer's interval.
   */
  MOZ_CAN_RUN_SCRIPT nsresult StartAutoScrollTimer(nsIFrame* aFrame,
                                                   const nsPoint& aPoint,
                                                   uint32_t aDelay);

  /**
   * Stops any active auto scroll timer.
   */
  void StopAutoScrollTimer();

  /**
   * Returns in frame coordinates the selection beginning and ending with the
   * type of selection given
   *
   * @param aContent is the content asking
   * @param aContentOffset is the starting content boundary
   * @param aContentLength is the length of the content piece asking
   * @param aSlowCheck will check using slow method with no shortcuts
   */
  mozilla::UniquePtr<SelectionDetails> LookUpSelection(nsIContent* aContent,
                                                       int32_t aContentOffset,
                                                       int32_t aContentLength,
                                                       bool aSlowCheck) const;

  /**
   * Sets the drag state to aState for resons of drag state.
   *
   * @param aState is the new state of drag
   */
  MOZ_CAN_RUN_SCRIPT void SetDragState(bool aState);

  /**
   * Gets the drag state to aState for resons of drag state.
   *
   * @param aState will hold the state of drag
   */
  [[nodiscard]] bool GetDragState() const { return mDragState; }

  /**
   * If we are in table cell selection mode. aka ctrl click in table cell
   */
  [[nodiscard]] bool IsInTableSelectionMode() const {
    return mTableSelection.mMode != mozilla::TableSelectionMode::None;
  }
  void ClearTableCellSelection() {
    mTableSelection.mMode = mozilla::TableSelectionMode::None;
  }

  /**
   * No query interface for selection. must use this method now.
   *
   * @param aSelectionType The selection type what you want.
   */
  [[nodiscard]] mozilla::dom::Selection* GetSelection(
      mozilla::SelectionType aSelectionType) const;

  /**
   * Convenience method to access the `eNormal` Selection.
   */
  [[nodiscard]] mozilla::dom::Selection& NormalSelection() const {
    return *GetSelection(mozilla::SelectionType::eNormal);
  }

  /**
   * Returns the number of highlight selections.
   */
  [[nodiscard]] size_t HighlightSelectionCount() const {
    return mHighlightSelections.Length();
  }

  /**
   * Get a highlight selection by index. The index must be valid.
   */
  [[nodiscard]] RefPtr<mozilla::dom::Selection> HighlightSelection(
      size_t aIndex) const {
    return mHighlightSelections[aIndex].second();
  }

  /**
   * @brief Adds a highlight selection for `aHighlight`.
   */
  MOZ_CAN_RUN_SCRIPT void AddHighlightSelection(
      nsAtom* aHighlightName, mozilla::dom::Highlight& aHighlight);

  void RepaintHighlightSelection(nsAtom* aHighlightName);

  /**
   * @brief Removes the Highlight selection identified by `aHighlightName`.
   */
  MOZ_CAN_RUN_SCRIPT void RemoveHighlightSelection(nsAtom* aHighlightName);

  /**
   * @brief Adds a new range to the highlight selection.
   *
   * If there is no highlight selection for the given highlight yet, it is
   * created using |AddHighlightSelection|.
   */
  MOZ_CAN_RUN_SCRIPT void AddHighlightSelectionRange(
      nsAtom* aHighlightName, mozilla::dom::Highlight& aHighlight,
      mozilla::dom::AbstractRange& aRange);

  /**
   * @brief Removes a range from a highlight selection.
   */
  MOZ_CAN_RUN_SCRIPT void RemoveHighlightSelectionRange(
      nsAtom* aHighlightName, mozilla::dom::AbstractRange& aRange);
  /**
   * ScrollSelectionIntoView scrolls a region of the selection,
   * so that it is visible in the scrolled view.
   *
   * @param aSelectionType the selection to scroll into view.
   *
   * @param aRegion the region inside the selection to scroll into view.
   *
   * @param aFlags the scroll flags.  Valid bits include:
   *   * SCROLL_SYNCHRONOUS: when set, scrolls the selection into view
   *     before returning. If not set, posts a request which is processed
   *     at some point after the method returns.
   *   * SCROLL_FIRST_ANCESTOR_ONLY: if set, only the first ancestor will be
   *     scrolled into view.
   */
  MOZ_CAN_RUN_SCRIPT nsresult
  ScrollSelectionIntoView(mozilla::SelectionType aSelectionType,
                          SelectionRegion aRegion, int16_t aFlags) const;

  /**
   * RepaintSelection repaints the selected frames that are inside the
   * selection specified by aSelectionType.
   *
   * @param aSelectionType The selection type what you want to repaint.
   */
  nsresult RepaintSelection(mozilla::SelectionType aSelectionType);

  /**
   * Return true if aContainerNode is in the selection limiter or the ancestor
   * limiter if one of them is set.
   *
   * Note that this returns true when aContainerNode may be in the scope of
   * an independent selection.  Therefore, even if this returns `true`,
   * aContainerNode may not be valid container node for a selection managed
   * by this instance.
   */
  [[nodiscard]] bool NodeIsInLimiters(const nsINode* aContainerNode) const;

  [[nodiscard]] static bool NodeIsInLimiters(
      const nsINode* aContainerNode,
      const Element* aIndependentSelectionLimiterElement,
      const Element* aSelectionAncestorLimiter);

  /**
   * GetFrameToPageSelect() returns a frame which is ancestor limit of
   * per-page selection.  The frame may not be scrollable.  E.g.,
   * when selection ancestor limit is set to a frame of an editing host of
   * contenteditable element and it's not scrollable.
   */
  [[nodiscard]] nsIFrame* GetFrameToPageSelect() const;

  /**
   * This method moves caret (if aExtend is false) or expands selection (if
   * aExtend is true).  Then, scrolls aFrame one page.  Finally, this may
   * call ScrollSelectionIntoView() for making focus of selection visible
   * but depending on aSelectionIntoView value.
   *
   * @param aForward if true, scroll forward if not scroll backward
   * @param aExtend  if true, extend selection to the new point
   * @param aFrame   the frame to scroll or container of per-page selection.
   *                 if aExtend is true and selection may have ancestor limit,
   *                 should set result of GetFrameToPageSelect().
   * @param aSelectionIntoView
   *                 If IfChanged, this makes selection into view only when
   *                 selection is modified by the call.
   *                 If Yes, this makes selection into view always.
   */
  enum class SelectionIntoView { IfChanged, Yes };
  MOZ_CAN_RUN_SCRIPT nsresult PageMove(bool aForward, bool aExtend,
                                       nsIFrame* aFrame,
                                       SelectionIntoView aSelectionIntoView);

  void SetHint(CaretAssociationHint aHintRight) { mCaret.mHint = aHintRight; }
  [[nodiscard]] CaretAssociationHint GetHint() const { return mCaret.mHint; }

  void SetCaretBidiLevelAndMaybeSchedulePaint(
      mozilla::intl::BidiEmbeddingLevel aLevel);

  /**
   * GetCaretBidiLevel gets the caret bidi level.
   */
  [[nodiscard]] mozilla::intl::BidiEmbeddingLevel GetCaretBidiLevel() const;

  /**
   * UndefineCaretBidiLevel sets the caret bidi level to "undefined".
   */
  void UndefineCaretBidiLevel();

  /**
   * PhysicalMove will generally be called from the nsiselectioncontroller
   * implementations. the effect being the selection will move one unit
   * 'aAmount' in the given aDirection.
   * @param aDirection  the direction to move the selection
   * @param aAmount     amount of movement (char/line; word/page; eol/doc)
   * @param aExtend     continue selection
   */
  MOZ_CAN_RUN_SCRIPT nsresult PhysicalMove(int16_t aDirection, int16_t aAmount,
                                           bool aExtend);

  /**
   * CharacterMove will generally be called from the nsiselectioncontroller
   * implementations. the effect being the selection will move one character
   * left or right.
   * @param aForward move forward in document.
   * @param aExtend continue selection
   */
  MOZ_CAN_RUN_SCRIPT nsresult CharacterMove(bool aForward, bool aExtend);

  /**
   * WordMove will generally be called from the nsiselectioncontroller
   * implementations. the effect being the selection will move one word left or
   * right.
   * @param aForward move forward in document.
   * @param aExtend continue selection
   */
  MOZ_CAN_RUN_SCRIPT nsresult WordMove(bool aForward, bool aExtend);

  /**
   * LineMove will generally be called from the nsiselectioncontroller
   * implementations. the effect being the selection will move one line up or
   * down.
   * @param aForward move forward in document.
   * @param aExtend continue selection
   */
  MOZ_CAN_RUN_SCRIPT nsresult LineMove(bool aForward, bool aExtend);

  /**
   * IntraLineMove will generally be called from the nsiselectioncontroller
   * implementations. the effect being the selection will move to beginning or
   * end of line
   * @param aForward move forward in document.
   * @param aExtend continue selection
   */
  MOZ_CAN_RUN_SCRIPT nsresult IntraLineMove(bool aForward, bool aExtend);

  /**
   * CreateRangeExtendedToNextGraphemeClusterBoundary() returns range which is
   * extended from normal selection range to start of next grapheme cluster
   * boundary.
   *
   * @param aLimitersAndCaretData       The data of limiters and additional
   *                                    caret data.
   * @param aRange                      The range which you want to extend.
   * @param aRangeDirection             eDirNext if the start boundary of
   *                                    aRange is focus.  Otherwise, i.e., if
   *                                    the start boundary is anchor,
   *                                    eDirPrevious.
   */
  template <typename RangeType>
  MOZ_CAN_RUN_SCRIPT static mozilla::Result<RefPtr<RangeType>, nsresult>
  CreateRangeExtendedToNextGraphemeClusterBoundary(
      mozilla::PresShell& aPresShell,
      const mozilla::LimitersAndCaretData& aLimitersAndCaretData,
      const mozilla::dom::AbstractRange& aRange, nsDirection aRangeDirection) {
    return CreateRangeExtendedToSomewhere<RangeType>(
        aPresShell, aLimitersAndCaretData, aRange, aRangeDirection, eDirNext,
        eSelectCluster, eLogical);
  }

  /**
   * CreateRangeExtendedToPreviousCharacterBoundary() returns range which is
   * extended from normal selection range to start of previous character
   * boundary.
   *
   * @param aLimitersAndCaretData       The data of limiters and additional
   *                                    caret data.
   * @param aRange                      The range which you want to extend.
   * @param aRangeDirection             eDirNext if the start boundary of
   *                                    aRange is focus.  Otherwise, i.e., if
   *                                    the start boundary is anchor,
   *                                    eDirPrevious.
   */
  template <typename RangeType>
  MOZ_CAN_RUN_SCRIPT static mozilla::Result<RefPtr<RangeType>, nsresult>
  CreateRangeExtendedToPreviousCharacterBoundary(
      mozilla::PresShell& aPresShell,
      const mozilla::LimitersAndCaretData& aLimitersAndCaretData,
      const mozilla::dom::AbstractRange& aRange, nsDirection aRangeDirection) {
    return CreateRangeExtendedToSomewhere<RangeType>(
        aPresShell, aLimitersAndCaretData, aRange, aRangeDirection,
        eDirPrevious, eSelectCharacter, eLogical);
  }

  /**
   * CreateRangeExtendedToNextWordBoundary() returns range which is
   * extended from normal selection range to start of next word boundary.
   *
   * @param aLimitersAndCaretData       The data of limiters and additional
   *                                    caret data.
   * @param aRange                      The range which you want to extend.
   * @param aRangeDirection             eDirNext if the start boundary of
   *                                    aRange is focus.  Otherwise, i.e., if
   *                                    the start boundary is anchor,
   *                                    eDirPrevious.
   */
  template <typename RangeType>
  MOZ_CAN_RUN_SCRIPT static mozilla::Result<RefPtr<RangeType>, nsresult>
  CreateRangeExtendedToNextWordBoundary(
      mozilla::PresShell& aPresShell,
      const mozilla::LimitersAndCaretData& aLimitersAndCaretData,
      const mozilla::dom::AbstractRange& aRange, nsDirection aRangeDirection) {
    return CreateRangeExtendedToSomewhere<RangeType>(
        aPresShell, aLimitersAndCaretData, aRange, aRangeDirection, eDirNext,
        eSelectWord, eLogical);
  }

  /**
   * CreateRangeExtendedToPreviousWordBoundary() returns range which is
   * extended from normal selection range to start of previous word boundary.
   *
   * @param aLimitersAndCaretData       The data of limiters and additional
   *                                    caret data.
   * @param aRange                      The range which you want to extend.
   * @param aRangeDirection             eDirNext if the start boundary of
   *                                    aRange is focus.  Otherwise, i.e., if
   *                                    the start boundary is anchor,
   *                                    eDirPrevious.
   */
  template <typename RangeType>
  MOZ_CAN_RUN_SCRIPT static mozilla::Result<RefPtr<RangeType>, nsresult>
  CreateRangeExtendedToPreviousWordBoundary(
      mozilla::PresShell& aPresShell,
      const mozilla::LimitersAndCaretData& aLimitersAndCaretData,
      const mozilla::dom::AbstractRange& aRange, nsDirection aRangeDirection) {
    return CreateRangeExtendedToSomewhere<RangeType>(
        aPresShell, aLimitersAndCaretData, aRange, aRangeDirection,
        eDirPrevious, eSelectWord, eLogical);
  }

  /**
   * CreateRangeExtendedToPreviousHardLineBreak() returns range which is
   * extended from normal selection range to previous hard line break.
   *
   * @param aLimitersAndCaretData       The data of limiters and additional
   *                                    caret data.
   * @param aRange                      The range which you want to extend.
   * @param aRangeDirection             eDirNext if the start boundary of
   *                                    aRange is focus.  Otherwise, i.e., if
   *                                    the start boundary is anchor,
   *                                    eDirPrevious.
   */
  template <typename RangeType>
  MOZ_CAN_RUN_SCRIPT static mozilla::Result<RefPtr<RangeType>, nsresult>
  CreateRangeExtendedToPreviousHardLineBreak(
      mozilla::PresShell& aPresShell,
      const mozilla::LimitersAndCaretData& aLimitersAndCaretData,
      const mozilla::dom::AbstractRange& aRange, nsDirection aRangeDirection) {
    return CreateRangeExtendedToSomewhere<RangeType>(
        aPresShell, aLimitersAndCaretData, aRange, aRangeDirection,
        eDirPrevious, eSelectBeginLine, eLogical);
  }

  /**
   * CreateRangeExtendedToNextHardLineBreak() returns range which is extended
   * from normal selection range to next hard line break.
   *
   * @param aLimitersAndCaretData       The data of limiters and additional
   *                                    caret data.
   * @param aRange                      The range which you want to extend.
   * @param aRangeDirection             eDirNext if the start boundary of
   *                                    aRange is focus.  Otherwise, i.e., if
   *                                    the start boundary is anchor,
   *                                    eDirPrevious.
   */
  template <typename RangeType>
  MOZ_CAN_RUN_SCRIPT static mozilla::Result<RefPtr<RangeType>, nsresult>
  CreateRangeExtendedToNextHardLineBreak(
      mozilla::PresShell& aPresShell,
      const mozilla::LimitersAndCaretData& aLimitersAndCaretData,
      const mozilla::dom::AbstractRange& aRange, nsDirection aRangeDirection) {
    return CreateRangeExtendedToSomewhere<RangeType>(
        aPresShell, aLimitersAndCaretData, aRange, aRangeDirection, eDirNext,
        eSelectEndLine, eLogical);
  }

  /** Sets/Gets The display selection enum.
   */
  void SetDisplaySelection(int16_t aState) { mDisplaySelection = aState; }
  [[nodiscard]] int16_t GetDisplaySelection() const {
    return mDisplaySelection;
  }

  /**
   * This method can be used to store the data received during a MouseDown
   * event so that we can place the caret during the MouseUp event.
   *
   * @param aMouseEvent the event received by the selection MouseDown
   * handling method. A nullptr value can be use to tell this method
   * that any data is storing is no longer valid.
   */
  void SetDelayedCaretData(mozilla::WidgetMouseEvent* aMouseEvent);

  /**
   * Get the delayed MouseDown event data necessary to place the
   * caret during MouseUp processing.
   *
   * @return a pointer to the event received
   * by the selection during MouseDown processing. It can be nullptr
   * if the data is no longer valid.
   */
  [[nodiscard]] bool HasDelayedCaretData() const {
    return mDelayedMouseEvent.mIsValid;
  }
  [[nodiscard]] bool IsShiftDownInDelayedCaretData() const {
    NS_ASSERTION(mDelayedMouseEvent.mIsValid, "No valid delayed caret data");
    return mDelayedMouseEvent.mIsShift;
  }
  [[nodiscard]] uint32_t GetClickCountInDelayedCaretData() const {
    NS_ASSERTION(mDelayedMouseEvent.mIsValid, "No valid delayed caret data");
    return mDelayedMouseEvent.mClickCount;
  }

  [[nodiscard]] bool MouseDownRecorded() const {
    return !GetDragState() && HasDelayedCaretData() &&
           GetClickCountInDelayedCaretData() < 2;
  }

  /**
   * Returns the selection root element if and only if the instance is for an
   * independent selection.  Currently, this is a native anonymous `<div>` for
   * a text control.
   */
  [[nodiscard]] Element* GetIndependentSelectionRootElement() const {
    return mLimiters.mIndependentSelectionRootElement;
  }

  /**
   * Get the independent selection root parent which is usually a text control
   * element which hosts the anonymous subtree managed by this frame selection.
   */
  [[nodiscard]] Element* GetIndependentSelectionRootParentElement() const {
    MOZ_DIAGNOSTIC_ASSERT(IsIndependentSelection());
    return Element::FromNodeOrNull(
        mLimiters.mIndependentSelectionRootElement
            ->GetClosestNativeAnonymousSubtreeRootParentOrHost());
  }

  /**
   * GetAncestorLimiter() returns the root of current selection ranges.  This is
   * typically the focused editing host unless it's the root element of the
   * document.
   */
  [[nodiscard]] Element* GetAncestorLimiter() const {
    return mLimiters.mAncestorLimiter;
  }

  [[nodiscard]] Element* GetAncestorLimiterOrIndependentSelectionRootElement()
      const {
    return mLimiters.mAncestorLimiter
               ? mLimiters.mAncestorLimiter
               : mLimiters.mIndependentSelectionRootElement;
  }

  /**
   * Set ancestor limiter.  If aLimiter is not nullptr, this adjusts all
   * selection ranges into the limiter element.  Thus, calling this may run
   * the selection listeners.
   */
  MOZ_CAN_RUN_SCRIPT void SetAncestorLimiter(Element* aLimiter);

  /**
   * GetPrevNextBidiLevels will return the frames and associated Bidi levels of
   * the characters logically before and after a (collapsed) selection.
   *
   * @param aNode is the node containing the selection
   * @param aContentOffset is the offset of the selection in the node
   * @param aJumpLines
   *   If true, look across line boundaries.
   *   If false, behave as if there were base-level frames at line edges.
   *
   * @return A struct holding the before/after frame and the before/after
   * level.
   *
   * At the beginning and end of each line there is assumed to be a frame with
   * Bidi level equal to the paragraph embedding level.
   *
   * In these cases the before frame and after frame respectively will be
   * nullptr.
   */
  [[nodiscard]] nsPrevNextBidiLevels GetPrevNextBidiLevels(
      nsIContent* aNode, uint32_t aContentOffset, bool aJumpLines) const;

  /**
   * MaintainSelection will track the normal selection as being "sticky".
   * Dragging or extending selection will never allow for a subset
   * (or the whole) of the maintained selection to become unselected.
   * Primary use: double click selecting then dragging on second click
   *
   * @param aAmount the initial amount of text selected (word, line or
   * paragraph). For "line", use eSelectBeginLine.
   */
  nsresult MaintainSelection(nsSelectionAmount aAmount = eSelectNoAmount);

  MOZ_CAN_RUN_SCRIPT nsresult ConstrainFrameAndPointToAnchorSubtree(
      nsIFrame* aFrame, const nsPoint& aPoint, nsIFrame** aRetFrame,
      nsPoint& aRetPoint) const;

  /**
   * @param aPresShell is the parameter to be used for most of the other calls
   * for callbacks etc
   *
   * @param aAccessibleCaretEnabled true if we should enable the accessible
   * caret.
   *
   * @param aEditorRootAnonymousDiv if this instance is for an independent
   * selection for a text control, specify this to the anonymous <div> element
   * of the text control which contains only an editable Text and/or a <br>.
   */
  nsFrameSelection(mozilla::PresShell* aPresShell, bool aAccessibleCaretEnabled,
                   Element* aEditorRootAnonymousDiv = nullptr);

  /**
   * @param aRequesterFuncName function name which wants to start the batch.
   * This won't be stored nor exposed to selection listeners etc, used only for
   * logging.
   */
  void StartBatchChanges(const char* aRequesterFuncName);

  /**
   * @param aRequesterFuncName function name which wants to end the batch.
   * This won't be stored nor exposed to selection listeners etc, used only for
   * logging.
   * @param aReasons potentially multiple of the reasons defined in
   * nsISelectionListener.idl
   */
  MOZ_CAN_RUN_SCRIPT void EndBatchChanges(
      const char* aRequesterFuncName,
      int16_t aReasons = nsISelectionListener::NO_REASON);

  [[nodiscard]] mozilla::PresShell* GetPresShell() const { return mPresShell; }

  void DisconnectFromPresShell();
  MOZ_CAN_RUN_SCRIPT nsresult ClearNormalSelection();

  // Table selection support.
  static nsITableCellLayout* GetCellLayout(const nsIContent* aCellContent);

 private:
  ~nsFrameSelection();

  // TODO: in case an error is returned, it sometimes refers to a programming
  // error, in other cases to runtime errors. This deserves to be cleaned up.
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT nsresult
  TakeFocus(nsIContent& aNewFocus, uint32_t aContentOffset,
            uint32_t aContentEndOffset, CaretAssociationHint aHint,
            FocusMode aFocusMode);

  /**
   * After moving the caret, its Bidi level is set according to the following
   * rules:
   *
   * After moving over a character with left/right arrow, set to the Bidi level
   * of the last moved over character. After Home and End, set to the paragraph
   * embedding level. After up/down arrow, PageUp/Down, set to the lower level
   * of the 2 surrounding characters. After mouse click, set to the level of the
   * current frame.
   *
   * The following two methods use GetPrevNextBidiLevels to determine the new
   * Bidi level. BidiLevelFromMove is called when the caret is moved in response
   * to a keyboard event
   *
   * @param aPresShell is the presentation shell
   * @param aNode is the content node
   * @param aContentOffset is the new caret position, as an offset into aNode
   * @param aAmount is the amount of the move that gave the caret its new
   * position
   * @param aHint is the hint indicating in what logical direction the caret
   * moved
   */
  void BidiLevelFromMove(mozilla::PresShell* aPresShell, nsIContent* aNode,
                         uint32_t aContentOffset, nsSelectionAmount aAmount,
                         CaretAssociationHint aHint);
  /**
   * BidiLevelFromClick is called when the caret is repositioned by clicking the
   * mouse
   *
   * @param aNode is the content node
   * @param aContentOffset is the new caret position, as an offset into aNode
   */
  void BidiLevelFromClick(nsIContent* aNewFocus, uint32_t aContentOffset);

  /**
   * @param aReasons potentially multiple of the reasons defined in
   * nsISelectionListener.idl.
   */
  void SetChangeReasons(int16_t aReasons) {
    mSelectionChangeReasons = aReasons;
  }

  /**
   * @param aReasons potentially multiple of the reasons defined in
   * nsISelectionListener.idl.
   */
  void AddChangeReasons(int16_t aReasons) {
    mSelectionChangeReasons |= aReasons;
  }

  /**
   * @return potentially multiple of the reasons defined in
   * nsISelectionListener.idl.
   */
  [[nodiscard]] int16_t PopChangeReasons() {
    int16_t retval = mSelectionChangeReasons;
    mSelectionChangeReasons = nsISelectionListener::NO_REASON;
    return retval;
  }

  [[nodiscard]] nsSelectionAmount GetCaretMoveAmount() {
    return mCaretMoveAmount;
  }

  [[nodiscard]] bool IsUserSelectionReason() const {
    return (mSelectionChangeReasons &
            (nsISelectionListener::DRAG_REASON |
             nsISelectionListener::MOUSEDOWN_REASON |
             nsISelectionListener::MOUSEUP_REASON |
             nsISelectionListener::KEYPRESS_REASON)) !=
           nsISelectionListener::NO_REASON;
  }

  friend class mozilla::dom::Selection;
  friend class mozilla::SelectionChangeEventDispatcher;
  friend struct mozilla::AutoPrepareFocusRange;

  /*HELPER METHODS*/
  // Whether MoveCaret should use logical or visual movement,
  // or follow the bidi.edit.caret_movement_style preference.
  enum CaretMovementStyle { eLogical, eVisual, eUsePrefStyle };
  enum class ExtendSelection : bool { No, Yes };
  MOZ_CAN_RUN_SCRIPT nsresult MoveCaret(nsDirection aDirection,
                                        ExtendSelection aExtendSelection,
                                        nsSelectionAmount aAmount,
                                        CaretMovementStyle aMovementStyle);

  /**
   * @brief Creates `PeekOffsetOptions` for caret move operations.
   *
   * @param aSelection       The selection object. Must be non-null
   * @param aExtendSelection Whether the selection should be extended or not
   * @param aMovementStyle   The `CaretMovementStyle` (logical or visual)
   * @return mozilla::Result<mozilla::PeekOffsetOptions, nsresult>
   */
  [[nodiscard]] mozilla::Result<mozilla::PeekOffsetOptions, nsresult>
  CreatePeekOffsetOptionsForCaretMove(mozilla::dom::Selection* aSelection,
                                      ExtendSelection aExtendSelection,
                                      CaretMovementStyle aMovementStyle) const {
    MOZ_ASSERT(aSelection);
    return CreatePeekOffsetOptionsForCaretMove(
        mLimiters.mIndependentSelectionRootElement,
        static_cast<ForceEditableRegion>(aSelection->IsEditorSelection()),
        aExtendSelection, aMovementStyle);
  }

  enum class ForceEditableRegion : bool { No, Yes };
  [[nodiscard]] static mozilla::Result<mozilla::PeekOffsetOptions, nsresult>
  CreatePeekOffsetOptionsForCaretMove(const Element* aSelectionLimiter,
                                      ForceEditableRegion aForceEditableRegion,
                                      ExtendSelection aExtendSelection,
                                      CaretMovementStyle aMovementStyle);

  /**
   * @brief Get the Ancestor Limiter for caret move operation.
   *
   * If the selection is an editor selection, the correct editing host is
   * identified and chosen as limiting element.
   *
   * @param aSelection The selection object. Must be non-null
   * @return The ancestor limiter, or nullptr.
   */
  [[nodiscard]] mozilla::Result<Element*, nsresult>
  GetAncestorLimiterForCaretMove(mozilla::dom::Selection* aSelection) const;

  /**
   * CreateRangeExtendedToSomewhere() is common method to implement
   * CreateRangeExtendedTo*().  This method creates a range extended from
   * aRange.
   *
   * @param aLimitersAndCaretData       The data of limiters and additional
   *                                    caret data.
   * @param aRange                      The range which you want to extend.
   * @param aRangeDirection             eDirNext if the start boundary of
   *                                    aRange is focus.  Otherwise, i.e., if
   *                                    the start boundary is anchor,
   *                                    eDirPrevious.
   * @param aExtendDirection            Whether you want to extend the range
   *                                    backward or forward.
   * @param aAmount                     The amount which you want to extend.
   * @param aMovementStyle              Whether visual or logical.
   */
  template <typename RangeType>
  MOZ_CAN_RUN_SCRIPT static mozilla::Result<RefPtr<RangeType>, nsresult>
  CreateRangeExtendedToSomewhere(
      mozilla::PresShell& aPresShell,
      const mozilla::LimitersAndCaretData& aLimitersAndCaretData,
      const mozilla::dom::AbstractRange& aRange, nsDirection aRangeDirection,
      nsDirection aExtendDirection, const nsSelectionAmount aAmount,
      CaretMovementStyle aMovementStyle);

  void InvalidateDesiredCaretPos();  // do not listen to mDesiredCaretPos.mValue
                                     // you must get another.

  [[nodiscard]] bool IsBatching() const { return mBatching.mCounter > 0; }

  enum class IsBatchingEnd : bool { No, Yes };

  // nsFrameSelection may get deleted when calling this,
  // so remember to use nsCOMPtr when needed.
  MOZ_CAN_RUN_SCRIPT nsresult
  NotifySelectionListeners(mozilla::SelectionType aSelectionType,
                           IsBatchingEnd aEndBatching = IsBatchingEnd::No);

  static nsresult GetCellIndexes(const nsIContent* aCell, int32_t& aRowIndex,
                                 int32_t& aColIndex);

  [[nodiscard]] static nsIContent* GetFirstCellNodeInRange(
      const nsRange* aRange);
  // Returns non-null table if in same table, null otherwise
  [[nodiscard]] static nsIContent* IsInSameTable(const nsIContent* aContent1,
                                                 const nsIContent* aContent2);
  // Might return null
  [[nodiscard]] static nsIContent* GetParentTable(const nsIContent* aCellNode);

  ////////////BEGIN nsFrameSelection members

  RefPtr<mozilla::dom::Selection>
      mDomSelections[sizeof(mozilla::kPresentSelectionTypes) /
                     sizeof(mozilla::SelectionType)];

  nsTArray<
      mozilla::CompactPair<RefPtr<nsAtom>, RefPtr<mozilla::dom::Selection>>>
      mHighlightSelections;

  struct TableSelection {
    // Get our first range, if its first selected node is a cell.  If this does
    // not return null, then the first node in the returned range is a cell
    // (according to GetFirstCellNodeInRange).
    nsRange* GetFirstCellRange(const mozilla::dom::Selection& aNormalSelection);

    // Get our next range, if its first selected node is a cell.  If this does
    // not return null, then the first node in the returned range is a cell
    // (according to GetFirstCellNodeInRange).
    nsRange* GetNextCellRange(const mozilla::dom::Selection& aNormalSelection);

    [[nodiscard]] MOZ_CAN_RUN_SCRIPT nsresult
    HandleSelection(nsINode* aParentContent, int32_t aContentOffset,
                    mozilla::TableSelectionMode aTarget,
                    mozilla::WidgetMouseEvent* aMouseEvent, bool aDragState,
                    mozilla::dom::Selection& aNormalSelection);

    /**
     * @return the closest inclusive table cell ancestor
     *         (https://dom.spec.whatwg.org/#concept-tree-inclusive-ancestor) of
     *         aContent, if it is actively editable.
     */
    [[nodiscard]] static nsINode* IsContentInActivelyEditableTableCell(
        nsPresContext* aContext, nsIContent* aContent);

    // TODO: annotate this with `MOZ_CAN_RUN_SCRIPT` instead.
    MOZ_CAN_RUN_SCRIPT nsresult
    SelectBlockOfCells(nsIContent* aStartCell, nsIContent* aEndCell,
                       mozilla::dom::Selection& aNormalSelection);

    MOZ_CAN_RUN_SCRIPT nsresult SelectRowOrColumn(
        nsIContent* aCellContent, mozilla::dom::Selection& aNormalSelection);

    MOZ_CAN_RUN_SCRIPT nsresult
    UnselectCells(const nsIContent* aTable, int32_t aStartRowIndex,
                  int32_t aStartColumnIndex, int32_t aEndRowIndex,
                  int32_t aEndColumnIndex, bool aRemoveOutsideOfCellRange,
                  mozilla::dom::Selection& aNormalSelection);

    nsCOMPtr<nsINode>
        mClosestInclusiveTableCellAncestor;  // used to snap to table selection
    nsCOMPtr<nsIContent> mStartSelectedCell;
    nsCOMPtr<nsIContent> mEndSelectedCell;
    nsCOMPtr<nsIContent> mAppendStartSelectedCell;
    nsCOMPtr<nsIContent> mUnselectCellOnMouseUp;
    mozilla::TableSelectionMode mMode = mozilla::TableSelectionMode::None;
    int32_t mSelectedCellIndex = 0;
    bool mDragSelectingCells = false;

   private:
    struct MOZ_STACK_CLASS FirstAndLastCell {
      nsCOMPtr<nsIContent> mFirst;
      nsCOMPtr<nsIContent> mLast;
    };

    [[nodiscard]] mozilla::Result<FirstAndLastCell, nsresult>
    FindFirstAndLastCellOfRowOrColumn(const nsIContent& aCellContent) const;

    [[nodiscard]] MOZ_CAN_RUN_SCRIPT nsresult HandleDragSelecting(
        mozilla::TableSelectionMode aTarget, nsIContent* aChildContent,
        const mozilla::WidgetMouseEvent* aMouseEvent,
        mozilla::dom::Selection& aNormalSelection);

    [[nodiscard]] MOZ_CAN_RUN_SCRIPT nsresult HandleMouseUpOrDown(
        mozilla::TableSelectionMode aTarget, bool aDragState,
        nsIContent* aChildContent, nsINode* aParentContent,
        int32_t aContentOffset, const mozilla::WidgetMouseEvent* aMouseEvent,
        mozilla::dom::Selection& aNormalSelection);

    class MOZ_STACK_CLASS RowAndColumnRelation;
  };

  TableSelection mTableSelection;

  struct MaintainedRange {
    /**
     * Ensure anchor and focus of aNormalSelection are ordered appropriately
     * relative to the maintained range.
     */
    MOZ_CAN_RUN_SCRIPT void AdjustNormalSelection(
        const nsIContent* aContent, int32_t aOffset,
        mozilla::dom::Selection& aNormalSelection) const;

    /**
     * @param aStopAtScroller   If yes, this will
     *                          set `PeekOffsetOption::StopAtScroller`.
     */
    enum class StopAtScroller : bool { No, Yes };
    void AdjustContentOffsets(nsIFrame::ContentOffsets& aOffsets,
                              StopAtScroller aStopAtScroller) const;

    void MaintainAnchorFocusRange(
        const mozilla::dom::Selection& aNormalSelection,
        nsSelectionAmount aAmount);

    RefPtr<nsRange> mRange;
    nsSelectionAmount mAmount = eSelectNoAmount;
  };

  MaintainedRange mMaintainedRange;

  struct Batching {
    uint32_t mCounter = 0;
  };

  Batching mBatching;

  struct Limiters {
    // The independent selection root element if and only if the
    // nsFrameSelection instance is for an independent selection.
    RefPtr<Element> mIndependentSelectionRootElement;
    // Limit selection navigation to a descendant of this element.
    // This is typically the focused editing host if set unless it's the root
    // element of the document.
    RefPtr<Element> mAncestorLimiter;
  };

  Limiters mLimiters;

  mozilla::PresShell* mPresShell = nullptr;
  // Reasons for notifications of selection changing.
  // Can be multiple of the reasons defined in nsISelectionListener.idl.
  int16_t mSelectionChangeReasons = nsISelectionListener::NO_REASON;
  // For visual display purposes.
  int16_t mDisplaySelection = nsISelectionController::SELECTION_OFF;
  nsSelectionAmount mCaretMoveAmount = eSelectNoAmount;

  struct Caret {
    // Hint to tell if the selection is at the end of this line or beginning of
    // next.
    CaretAssociationHint mHint = CaretAssociationHint::Before;
    mozilla::intl::BidiEmbeddingLevel mBidiLevel = BIDI_LEVEL_UNDEFINED;

    [[nodiscard]] static bool IsVisualMovement(
        ExtendSelection aExtendSelection, CaretMovementStyle aMovementStyle);
  };

  Caret mCaret;

  mozilla::intl::BidiEmbeddingLevel mKbdBidiLevel =
      mozilla::intl::BidiEmbeddingLevel::LTR();

  class DesiredCaretPos {
   public:
    // the position requested by the Key Handling for up down
    nsresult FetchPos(nsPoint& aDesiredCaretPos,
                      const mozilla::PresShell& aPresShell,
                      mozilla::dom::Selection& aNormalSelection) const;

    void Set(const nsPoint& aPos);

    void Invalidate();

    bool mIsSet = false;

   private:
    nsPoint mValue;
  };

  DesiredCaretPos mDesiredCaretPos;

  struct DelayedMouseEvent {
    bool mIsValid = false;
    // These values are not used since they are only valid when mIsValid is
    // true, and setting mIsValid  always overrides these values.
    bool mIsShift = false;
    uint32_t mClickCount = 0;
  };

  DelayedMouseEvent mDelayedMouseEvent;

  bool mDragState = false;  // for drag purposes
  bool mAccessibleCaretEnabled = false;

  // Records if a selection was created by doubleclicking or tripleclicking
  // a word. This information is needed later on to determine if a leading
  // or trailing whitespace needs to be removed as well to achieve
  // native behaviour on macOS.
  mozilla::dom::ClickSelectionType mClickSelectionType =
      mozilla::dom::ClickSelectionType::NotApplicable;
};

/**
 * Selection Batcher class that supports multiple FrameSelections.
 */
class MOZ_RAII AutoFrameSelectionBatcher final {
 public:
  MOZ_CAN_RUN_SCRIPT explicit AutoFrameSelectionBatcher(
      const char* aFunctionName, size_t aEstimatedSize = 1)
      : mFunctionName(aFunctionName) {
    mFrameSelections.SetCapacity(aEstimatedSize);
  }
  MOZ_CAN_RUN_SCRIPT ~AutoFrameSelectionBatcher() {
    for (const auto& frameSelection : mFrameSelections) {
      MOZ_KnownLive(frameSelection)->EndBatchChanges(mFunctionName);
    }
  }
  void AddFrameSelection(nsFrameSelection* aFrameSelection) {
    if (!aFrameSelection) {
      return;
    }
    aFrameSelection->StartBatchChanges(mFunctionName);
    mFrameSelections.AppendElement(aFrameSelection);
  }

 private:
  const char* mFunctionName;
  AutoTArray<RefPtr<nsFrameSelection>, 1> mFrameSelections;
};

namespace mozilla {
/**
 * A struct for sharing nsFrameSelection outside of its instance.
 */
struct LimitersAndCaretData {
  using Element = dom::Element;

  LimitersAndCaretData() = default;
  explicit LimitersAndCaretData(const nsFrameSelection& aFrameSelection)
      : mIndependentSelectionRootElement(
            aFrameSelection.GetIndependentSelectionRootElement()),
        mAncestorLimiter(aFrameSelection.GetAncestorLimiter()),
        mCaretAssociationHint(aFrameSelection.GetHint()),
        mCaretBidiLevel(aFrameSelection.GetCaretBidiLevel()) {}

  [[nodiscard]] bool NodeIsInLimiters(const nsINode* aContainerNode) const {
    return nsFrameSelection::NodeIsInLimiters(
        aContainerNode, mIndependentSelectionRootElement, mAncestorLimiter);
  }
  [[nodiscard]] bool RangeInLimiters(const dom::AbstractRange& aRange) const {
    return NodeIsInLimiters(aRange.GetStartContainer()) &&
           (!aRange.IsPositionedAndSameContainer() ||
            NodeIsInLimiters(aRange.GetEndContainer()));
  }

  // nsFrameSelection::GetIndependentSelectionRootElement
  RefPtr<Element> mIndependentSelectionRootElement;
  // nsFrameSelection::GetAncestorLimiter
  RefPtr<Element> mAncestorLimiter;
  // nsFrameSelection::GetHint
  CaretAssociationHint mCaretAssociationHint = CaretAssociationHint::Before;
  // nsFrameSelection::GetCaretBidiLevel
  intl::BidiEmbeddingLevel mCaretBidiLevel;
};

}  // namespace mozilla

#endif /* nsFrameSelection_h___ */
