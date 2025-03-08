/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WhiteSpaceVisibilityKeeper_h
#define WhiteSpaceVisibilityKeeper_h

#include "EditAction.h"
#include "EditorBase.h"
#include "EditorForwards.h"
#include "EditorDOMPoint.h"  // for EditorDOMPoint
#include "EditorUtils.h"     // for CaretPoint
#include "HTMLEditHelpers.h"
#include "HTMLEditor.h"
#include "HTMLEditUtils.h"
#include "WSRunScanner.h"

#include "mozilla/Assertions.h"
#include "mozilla/Maybe.h"
#include "mozilla/Result.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/HTMLBRElement.h"
#include "mozilla/dom/Text.h"
#include "nsCOMPtr.h"
#include "nsIContent.h"

namespace mozilla {

/**
 * WhiteSpaceVisibilityKeeper class helps `HTMLEditor` modifying the DOM tree
 * with keeps white-space sequence visibility automatically.  E.g., invisible
 * leading/trailing white-spaces becomes visible, this class members delete
 * them.  E.g., when splitting visible-white-space sequence, this class may
 * replace ASCII white-spaces at split edges with NBSPs.
 */
class WhiteSpaceVisibilityKeeper final {
 private:
  using AutoTransactionsConserveSelection =
      EditorBase::AutoTransactionsConserveSelection;
  using EditorType = EditorBase::EditorType;
  using Element = dom::Element;
  using HTMLBRElement = dom::HTMLBRElement;
  using IgnoreNonEditableNodes = WSRunScanner::IgnoreNonEditableNodes;
  using InsertTextTo = EditorBase::InsertTextTo;
  using LineBreakType = HTMLEditor::LineBreakType;
  using PointPosition = WSRunScanner::PointPosition;
  using Scan = WSRunScanner::Scan;
  using TextFragmentData = WSRunScanner::TextFragmentData;
  using VisibleWhiteSpacesData = WSRunScanner::VisibleWhiteSpacesData;

 public:
  WhiteSpaceVisibilityKeeper() = delete;
  explicit WhiteSpaceVisibilityKeeper(
      const WhiteSpaceVisibilityKeeper& aOther) = delete;
  WhiteSpaceVisibilityKeeper(WhiteSpaceVisibilityKeeper&& aOther) = delete;

  /**
   * Remove invisible leading white-spaces and trailing white-spaces if there
   * are around aPoint.
   */
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT static Result<CaretPoint, nsresult>
  DeleteInvisibleASCIIWhiteSpaces(HTMLEditor& aHTMLEditor,
                                  const EditorDOMPoint& aPoint);

  /**
   * Fix up white-spaces before aStartPoint and after aEndPoint in preparation
   * for content to keep the white-spaces visibility after the range is deleted.
   * Note that the nodes and offsets are adjusted in response to any dom changes
   * we make while adjusting white-spaces.
   */
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT static Result<CaretPoint, nsresult>
  PrepareToDeleteRangeAndTrackPoints(HTMLEditor& aHTMLEditor,
                                     EditorDOMPoint* aStartPoint,
                                     EditorDOMPoint* aEndPoint,
                                     const Element& aEditingHost) {
    MOZ_ASSERT(aStartPoint->IsSetAndValid());
    MOZ_ASSERT(aEndPoint->IsSetAndValid());
    AutoTrackDOMPoint trackerStart(aHTMLEditor.RangeUpdaterRef(), aStartPoint);
    AutoTrackDOMPoint trackerEnd(aHTMLEditor.RangeUpdaterRef(), aEndPoint);
    Result<CaretPoint, nsresult> caretPointOrError =
        WhiteSpaceVisibilityKeeper::PrepareToDeleteRange(
            aHTMLEditor, EditorDOMRange(*aStartPoint, *aEndPoint),
            aEditingHost);
    NS_WARNING_ASSERTION(
        caretPointOrError.isOk(),
        "WhiteSpaceVisibilityKeeper::PrepareToDeleteRange() failed");
    return caretPointOrError;
  }
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT static Result<CaretPoint, nsresult>
  PrepareToDeleteRange(HTMLEditor& aHTMLEditor,
                       const EditorDOMPoint& aStartPoint,
                       const EditorDOMPoint& aEndPoint,
                       const Element& aEditingHost) {
    MOZ_ASSERT(aStartPoint.IsSetAndValid());
    MOZ_ASSERT(aEndPoint.IsSetAndValid());
    Result<CaretPoint, nsresult> caretPointOrError =
        WhiteSpaceVisibilityKeeper::PrepareToDeleteRange(
            aHTMLEditor, EditorDOMRange(aStartPoint, aEndPoint), aEditingHost);
    NS_WARNING_ASSERTION(
        caretPointOrError.isOk(),
        "WhiteSpaceVisibilityKeeper::PrepareToDeleteRange() failed");
    return caretPointOrError;
  }
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT static Result<CaretPoint, nsresult>
  PrepareToDeleteRange(HTMLEditor& aHTMLEditor, const EditorDOMRange& aRange,
                       const Element& aEditingHost) {
    MOZ_ASSERT(aRange.IsPositionedAndValid());
    Result<CaretPoint, nsresult> caretPointOrError =
        WhiteSpaceVisibilityKeeper::
            MakeSureToKeepVisibleStateOfWhiteSpacesAroundDeletingRange(
                aHTMLEditor, aRange, aEditingHost);
    NS_WARNING_ASSERTION(
        caretPointOrError.isOk(),
        "WhiteSpaceVisibilityKeeper::"
        "MakeSureToKeepVisibleStateOfWhiteSpacesAroundDeletingRange() failed");
    return caretPointOrError;
  }

  /**
   * PrepareToSplitBlockElement() makes sure that the invisible white-spaces
   * not to become visible and returns splittable point.
   *
   * @param aHTMLEditor         The HTML editor.
   * @param aPointToSplit       The splitting point in aSplittingBlockElement.
   * @param aSplittingBlockElement  A block element which will be split.
   */
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT static Result<EditorDOMPoint, nsresult>
  PrepareToSplitBlockElement(HTMLEditor& aHTMLEditor,
                             const EditorDOMPoint& aPointToSplit,
                             const Element& aSplittingBlockElement);

  /**
   * MergeFirstLineOfRightBlockElementIntoDescendantLeftBlockElement() merges
   * first line in aRightBlockElement into end of aLeftBlockElement which
   * is a descendant of aRightBlockElement.
   *
   * @param aHTMLEditor         The HTML editor.
   * @param aLeftBlockElement   The content will be merged into end of
   *                            this element.
   * @param aRightBlockElement  The first line in this element will be
   *                            moved to aLeftBlockElement.
   * @param aAtRightBlockChild  At a child of aRightBlockElement and inclusive
   *                            ancestor of aLeftBlockElement.
   * @param aListElementTagName Set some if aRightBlockElement is a list
   *                            element and it'll be merged with another
   *                            list element.
   * @param aEditingHost        The editing host.
   */
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT static Result<MoveNodeResult, nsresult>
  MergeFirstLineOfRightBlockElementIntoDescendantLeftBlockElement(
      HTMLEditor& aHTMLEditor, Element& aLeftBlockElement,
      Element& aRightBlockElement, const EditorDOMPoint& aAtRightBlockChild,
      const Maybe<nsAtom*>& aListElementTagName,
      const HTMLBRElement* aPrecedingInvisibleBRElement,
      const Element& aEditingHost);

  /**
   * MergeFirstLineOfRightBlockElementIntoAncestorLeftBlockElement() merges
   * first line in aRightBlockElement into end of aLeftBlockElement which
   * is an ancestor of aRightBlockElement, then, removes aRightBlockElement
   * if it becomes empty.
   *
   * @param aHTMLEditor         The HTML editor.
   * @param aLeftBlockElement   The content will be merged into end of
   *                            this element.
   * @param aRightBlockElement  The first line in this element will be
   *                            moved to aLeftBlockElement and maybe
   *                            removed when this becomes empty.
   * @param aAtLeftBlockChild   At a child of aLeftBlockElement and inclusive
   *                            ancestor of aRightBlockElement.
   * @param aLeftContentInBlock The content whose inclusive ancestor is
   *                            aLeftBlockElement.
   * @param aListElementTagName Set some if aRightBlockElement is a list
   *                            element and it'll be merged with another
   *                            list element.
   * @param aEditingHost        The editing host.
   */
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT static Result<MoveNodeResult, nsresult>
  MergeFirstLineOfRightBlockElementIntoAncestorLeftBlockElement(
      HTMLEditor& aHTMLEditor, Element& aLeftBlockElement,
      Element& aRightBlockElement, const EditorDOMPoint& aAtLeftBlockChild,
      nsIContent& aLeftContentInBlock,
      const Maybe<nsAtom*>& aListElementTagName,
      const HTMLBRElement* aPrecedingInvisibleBRElement,
      const Element& aEditingHost);

  /**
   * MergeFirstLineOfRightBlockElementIntoLeftBlockElement() merges first
   * line in aRightBlockElement into end of aLeftBlockElement and removes
   * aRightBlockElement when it has only one line.
   *
   * @param aHTMLEditor         The HTML editor.
   * @param aLeftBlockElement   The content will be merged into end of
   *                            this element.
   * @param aRightBlockElement  The first line in this element will be
   *                            moved to aLeftBlockElement and maybe
   *                            removed when this becomes empty.
   * @param aListElementTagName Set some if aRightBlockElement is a list
   *                            element and its type needs to be changed.
   * @param aEditingHost        The editing host.
   */
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT static Result<MoveNodeResult, nsresult>
  MergeFirstLineOfRightBlockElementIntoLeftBlockElement(
      HTMLEditor& aHTMLEditor, Element& aLeftBlockElement,
      Element& aRightBlockElement, const Maybe<nsAtom*>& aListElementTagName,
      const HTMLBRElement* aPrecedingInvisibleBRElement,
      const Element& aEditingHost);

  /**
   * InsertLineBreak() inserts a line break at (before) aPointToInsert and
   * delete unnecessary white-spaces around there and/or replaces white-spaces
   * with non-breaking spaces.  Note that if the point is in a text node, the
   * text node will be split and insert new <br> node between the left node
   * and the right node.
   *
   * @param aPointToInsert  The point to insert new line break.  Note that
   *                        it'll be inserted before this point.  I.e., the
   *                        point will be the point of new line break.
   * @return                If succeeded, returns the new line break and
   *                        point to put caret.
   */
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT static Result<CreateLineBreakResult,
                                                 nsresult>
  InsertLineBreak(LineBreakType aLineBreakType, HTMLEditor& aHTMLEditor,
                  const EditorDOMPoint& aPointToInsert);

  /**
   * Insert aStringToInsert to aPointToInsert and makes any needed adjustments
   * to white-spaces around the insertion point.
   *
   * @param aStringToInsert     The string to insert.
   * @param aRangeToBeReplaced  The range to be replaced.
   * @param aInsertTextTo       Whether forcibly creates a new `Text` node in
   *                            specific condition or use existing `Text` if
   *                            available.
   */
  template <typename EditorDOMPointType>
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT static Result<InsertTextResult, nsresult>
  InsertText(HTMLEditor& aHTMLEditor, const nsAString& aStringToInsert,
             const EditorDOMPointType& aPointToInsert,
             InsertTextTo aInsertTextTo) {
    return WhiteSpaceVisibilityKeeper::
        InsertTextOrInsertOrUpdateCompositionString(
            aHTMLEditor, aStringToInsert, EditorDOMRange(aPointToInsert),
            aInsertTextTo, TextIsCompositionString::No);
  }

  /**
   * Insert aCompositionString to the start boundary of aCompositionStringRange
   * or update existing composition string with aCompositionString.
   * If inserting composition string, this may normalize white-spaces around
   * there.  However, if updating composition string, this will skip it to
   * avoid CompositionTransaction work.
   *
   * @param aCompositionString  The new composition string.
   * @param aCompositionStringRange
   *                            If there is old composition string, this should
   *                            cover all of it.  Otherwise, this should be
   *                            collapsed and indicate the insertion point.
   */
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT static Result<InsertTextResult, nsresult>
  InsertOrUpdateCompositionString(
      HTMLEditor& aHTMLEditor, const nsAString& aCompositionString,
      const EditorDOMRange& aCompositionStringRange) {
    return InsertTextOrInsertOrUpdateCompositionString(
        aHTMLEditor, aCompositionString, aCompositionStringRange,
        HTMLEditor::InsertTextTo::ExistingTextNodeIfAvailable,
        TextIsCompositionString::Yes);
  }

  /**
   * Delete previous white-space of aPoint.  This automatically keeps visibility
   * of white-spaces around aPoint. E.g., may remove invisible leading
   * white-spaces.
   */
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT static Result<CaretPoint, nsresult>
  DeletePreviousWhiteSpace(HTMLEditor& aHTMLEditor,
                           const EditorDOMPoint& aPoint,
                           const Element& aEditingHost);

  /**
   * Delete inclusive next white-space of aPoint.  This automatically keeps
   * visiblity of white-spaces around aPoint. E.g., may remove invisible
   * trailing white-spaces.
   */
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT static Result<CaretPoint, nsresult>
  DeleteInclusiveNextWhiteSpace(HTMLEditor& aHTMLEditor,
                                const EditorDOMPoint& aPoint,
                                const Element& aEditingHost);

  /**
   * Delete aContentToDelete and may remove/replace white-spaces around it.
   * Then, if deleting content makes 2 text nodes around it are adjacent
   * siblings, this joins them and put selection at the joined point.
   */
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT static Result<CaretPoint, nsresult>
  DeleteContentNodeAndJoinTextNodesAroundIt(HTMLEditor& aHTMLEditor,
                                            nsIContent& aContentToDelete,
                                            const EditorDOMPoint& aCaretPoint,
                                            const Element& aEditingHost);

  /**
   * Try to normalize visible white-space sequence around aPoint.
   * This may collapse `Selection` after replaced text.  Therefore, the callers
   * of this need to restore `Selection` by themselves (this does not do it for
   * performance reason of multiple calls).
   */
  template <typename EditorDOMPointType>
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT static nsresult
  NormalizeVisibleWhiteSpacesAt(HTMLEditor& aHTMLEditor,
                                const EditorDOMPointType& aPoint,
                                const Element& aEditingHost);

 private:
  /**
   * Maybe delete invisible white-spaces for keeping make them invisible and/or
   * may replace ASCII white-spaces with NBSPs for making visible white-spaces
   * to keep visible.
   */
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT static Result<CaretPoint, nsresult>
  MakeSureToKeepVisibleStateOfWhiteSpacesAroundDeletingRange(
      HTMLEditor& aHTMLEditor, const EditorDOMRange& aRangeToDelete,
      const Element& aEditingHost);

  /**
   * MakeSureToKeepVisibleWhiteSpacesVisibleAfterSplit() replaces ASCII white-
   * spaces which becomes invisible after split with NBSPs.
   */
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT static nsresult
  MakeSureToKeepVisibleWhiteSpacesVisibleAfterSplit(
      HTMLEditor& aHTMLEditor, const EditorDOMPoint& aPointToSplit);

  /**
   * ReplaceTextAndRemoveEmptyTextNodes() replaces the range between
   * aRangeToReplace with aReplaceString simply.  Additionally, removes
   * empty text nodes in the range.
   *
   * @param aRangeToReplace     Range to replace text.
   * @param aReplaceString      The new string.  Empty string is allowed.
   */
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT static nsresult
  ReplaceTextAndRemoveEmptyTextNodes(
      HTMLEditor& aHTMLEditor, const EditorDOMRangeInTexts& aRangeToReplace,
      const nsAString& aReplaceString);

  enum class TextIsCompositionString : bool { No, Yes };

  /**
   * Insert aStringToInsert to aRangeToBeReplaced.StartRef() with normalizing
   * white-spaces around there.
   *
   * @param aStringToInsert     The string to insert.
   * @param aRangeToBeReplaced  If you insert non-composing text, this MUST be
   *                            collapsed to the insertion point.
   *                            If you update composition string, this may be
   *                            not collapsed.  The range is required to
   *                            normalizing the new composition string.
   *                            Therefore, this should match the range of the
   *                            latest composition string.
   * @param aInsertTextTo       Whether forcibly creates a new `Text` node in
   *                            specific condition or use existing `Text` if
   *                            available.
   */
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT static Result<InsertTextResult, nsresult>
  InsertTextOrInsertOrUpdateCompositionString(
      HTMLEditor& aHTMLEditor, const nsAString& aStringToInsert,
      const EditorDOMRange& aRangeToBeReplaced, InsertTextTo aInsertTextTo,
      TextIsCompositionString aTextIsCompositionString);
};

}  // namespace mozilla

#endif  // #ifndef WhiteSpaceVisibilityKeeper_h
