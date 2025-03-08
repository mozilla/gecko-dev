/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef HTMLEditorNestedClasses_h
#define HTMLEditorNestedClasses_h

#include "EditorDOMPoint.h"
#include "EditorForwards.h"
#include "HTMLEditor.h"       // for HTMLEditor
#include "HTMLEditHelpers.h"  // for EditorInlineStyleAndValue
#include "HTMLEditUtils.h"    // for HTMLEditUtils::IsContainerNode

#include "mozilla/Attributes.h"
#include "mozilla/OwningNonNull.h"
#include "mozilla/Result.h"
#include "mozilla/dom/Text.h"
#include "nsTextFragment.h"

namespace mozilla {

/*****************************************************************************
 * AutoInlineStyleSetter is a temporary class to set an inline style to
 * specific nodes.
 ****************************************************************************/

class MOZ_STACK_CLASS HTMLEditor::AutoInlineStyleSetter final
    : private EditorInlineStyleAndValue {
  using Element = dom::Element;
  using Text = dom::Text;

 public:
  explicit AutoInlineStyleSetter(
      const EditorInlineStyleAndValue& aStyleAndValue)
      : EditorInlineStyleAndValue(aStyleAndValue) {}

  void Reset() {
    mFirstHandledPoint.Clear();
    mLastHandledPoint.Clear();
  }

  const EditorDOMPoint& FirstHandledPointRef() const {
    return mFirstHandledPoint;
  }
  const EditorDOMPoint& LastHandledPointRef() const {
    return mLastHandledPoint;
  }

  /**
   * Split aText at aStartOffset and aEndOffset (except when they are start or
   * end of its data) and wrap the middle text node in an element to apply the
   * style.
   */
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT Result<SplitRangeOffFromNodeResult, nsresult>
  SplitTextNodeAndApplyStyleToMiddleNode(HTMLEditor& aHTMLEditor, Text& aText,
                                         uint32_t aStartOffset,
                                         uint32_t aEndOffset);

  /**
   * Remove same style from children and apply the style entire (except
   * non-editable nodes) aContent.
   */
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT Result<CaretPoint, nsresult>
  ApplyStyleToNodeOrChildrenAndRemoveNestedSameStyle(HTMLEditor& aHTMLEditor,
                                                     nsIContent& aContent);

  /**
   * Invert the style with creating new element or something.  This should
   * be called only when IsInvertibleWithCSS() returns true.
   */
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT nsresult
  InvertStyleIfApplied(HTMLEditor& aHTMLEditor, Element& aElement);
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT Result<SplitRangeOffFromNodeResult, nsresult>
  InvertStyleIfApplied(HTMLEditor& aHTMLEditor, Text& aTextNode,
                       uint32_t aStartOffset, uint32_t aEndOffset);

  /**
   * Extend or shrink aRange for applying the style to the range.
   * See comments in the definition what this does.
   */
  Result<EditorRawDOMRange, nsresult> ExtendOrShrinkRangeToApplyTheStyle(
      const HTMLEditor& aHTMLEditor, const EditorDOMRange& aRange) const;

  /**
   * Returns next/previous sibling of aContent or an ancestor of it if it's
   * editable and does not cross block boundary.
   */
  [[nodiscard]] static nsIContent* GetNextEditableInlineContent(
      const nsIContent& aContent, const nsINode* aLimiter = nullptr);
  [[nodiscard]] static nsIContent* GetPreviousEditableInlineContent(
      const nsIContent& aContent, const nsINode* aLimiter = nullptr);

  /**
   * GetEmptyTextNodeToApplyNewStyle creates new empty text node to insert
   * a new element which will contain newly inserted text or returns existing
   * empty text node if aCandidatePointToInsert is around it.
   *
   * NOTE: Unfortunately, editor does not want to insert text into empty inline
   * element in some places (e.g., automatically adjusting caret position to
   * nearest text node).  Therefore, we need to create new empty text node to
   * prepare new styles for inserting text.  This method is designed for the
   * preparation.
   *
   * @param aHTMLEditor                 The editor.
   * @param aCandidatePointToInsert     The point where the caller wants to
   *                                    insert new text.
   * @return            If this creates new empty text node returns it.
   *                    If this couldn't create new empty text node due to
   *                    the point or aEditingHost cannot have text node,
   *                    returns nullptr.
   *                    Otherwise, returns error.
   */
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT static Result<RefPtr<Text>, nsresult>
  GetEmptyTextNodeToApplyNewStyle(
      HTMLEditor& aHTMLEditor, const EditorDOMPoint& aCandidatePointToInsert);

 private:
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT Result<CaretPoint, nsresult> ApplyStyle(
      HTMLEditor& aHTMLEditor, nsIContent& aContent);

  [[nodiscard]] MOZ_CAN_RUN_SCRIPT Result<CaretPoint, nsresult>
  ApplyCSSTextDecoration(HTMLEditor& aHTMLEditor, nsIContent& aContent);

  /**
   * Returns true if aStyledElement is a good element to set `style` attribute.
   */
  [[nodiscard]] bool ElementIsGoodContainerToSetStyle(
      nsStyledElement& aStyledElement) const;

  /**
   * ElementIsGoodContainerForTheStyle() returns true if aElement is a
   * good container for applying the style to a node.  I.e., if this returns
   * true, moving nodes into aElement is enough to apply the style to them.
   * Otherwise, you need to create new element for the style.
   */
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT Result<bool, nsresult>
  ElementIsGoodContainerForTheStyle(HTMLEditor& aHTMLEditor,
                                    Element& aElement) const;

  /**
   * Return true if the node is an element node and it represents the style or
   * sets the style (including when setting different value) with `style`
   * attribute.
   */
  [[nodiscard]] bool ContentIsElementSettingTheStyle(
      const HTMLEditor& aHTMLEditor, nsIContent& aContent) const;

  /**
   * Helper methods to shrink range to apply the style.
   */
  [[nodiscard]] EditorRawDOMPoint GetShrunkenRangeStart(
      const HTMLEditor& aHTMLEditor, const EditorDOMRange& aRange,
      const nsINode& aCommonAncestorOfRange,
      const nsIContent* aFirstEntirelySelectedContentNodeInRange) const;
  [[nodiscard]] EditorRawDOMPoint GetShrunkenRangeEnd(
      const HTMLEditor& aHTMLEditor, const EditorDOMRange& aRange,
      const nsINode& aCommonAncestorOfRange,
      const nsIContent* aLastEntirelySelectedContentNodeInRange) const;

  /**
   * Helper methods to extend the range to apply the style.
   */
  [[nodiscard]] EditorRawDOMPoint
  GetExtendedRangeStartToWrapAncestorApplyingSameStyle(
      const HTMLEditor& aHTMLEditor,
      const EditorRawDOMPoint& aStartPoint) const;
  [[nodiscard]] EditorRawDOMPoint
  GetExtendedRangeEndToWrapAncestorApplyingSameStyle(
      const HTMLEditor& aHTMLEditor, const EditorRawDOMPoint& aEndPoint) const;
  [[nodiscard]] EditorRawDOMRange
  GetExtendedRangeToMinimizeTheNumberOfNewElements(
      const HTMLEditor& aHTMLEditor, const nsINode& aCommonAncestor,
      EditorRawDOMPoint&& aStartPoint, EditorRawDOMPoint&& aEndPoint) const;

  /**
   * OnHandled() are called when this class creates new element to apply the
   * style, applies new style to existing element or ignores to apply the style
   * due to already set.
   */
  void OnHandled(const EditorDOMPoint& aStartPoint,
                 const EditorDOMPoint& aEndPoint) {
    if (!mFirstHandledPoint.IsSet()) {
      mFirstHandledPoint = aStartPoint;
    }
    mLastHandledPoint = aEndPoint;
  }
  void OnHandled(nsIContent& aContent) {
    if (aContent.IsElement() && !HTMLEditUtils::IsContainerNode(aContent)) {
      if (!mFirstHandledPoint.IsSet()) {
        mFirstHandledPoint.Set(&aContent);
      }
      mLastHandledPoint.SetAfter(&aContent);
      return;
    }
    if (!mFirstHandledPoint.IsSet()) {
      mFirstHandledPoint.Set(&aContent, 0u);
    }
    mLastHandledPoint = EditorDOMPoint::AtEndOf(aContent);
  }

  // mFirstHandledPoint and mLastHandledPoint store the first and last points
  // which are newly created or apply the new style, or just ignored at trying
  // to split a text node.
  EditorDOMPoint mFirstHandledPoint;
  EditorDOMPoint mLastHandledPoint;
};

/**
 * AutoMoveOneLineHandler moves the content in a line (between line breaks/block
 * boundaries) to specific point or end of a container element.
 */
class MOZ_STACK_CLASS HTMLEditor::AutoMoveOneLineHandler final {
 public:
  /**
   * Use this constructor when you want a line to move specific point.
   */
  explicit AutoMoveOneLineHandler(const EditorDOMPoint& aPointToInsert)
      : mPointToInsert(aPointToInsert),
        mMoveToEndOfContainer(MoveToEndOfContainer::No) {
    MOZ_ASSERT(mPointToInsert.IsSetAndValid());
    MOZ_ASSERT(mPointToInsert.IsInContentNode());
  }
  /**
   * Use this constructor when you want a line to move end of
   * aNewContainerElement.
   */
  explicit AutoMoveOneLineHandler(Element& aNewContainerElement)
      : mPointToInsert(&aNewContainerElement, 0),
        mMoveToEndOfContainer(MoveToEndOfContainer::Yes) {
    MOZ_ASSERT(mPointToInsert.IsSetAndValid());
  }

  /**
   * Must be called before calling Run().
   *
   * @param aHTMLEditor         The HTML editor.
   * @param aPointInHardLine    A point in a line which you want to move.
   * @param aEditingHost        The editing host.
   */
  [[nodiscard]] nsresult Prepare(HTMLEditor& aHTMLEditor,
                                 const EditorDOMPoint& aPointInHardLine,
                                 const Element& aEditingHost);
  /**
   * Must be called if Prepare() returned NS_OK.
   *
   * @param aHTMLEditor         The HTML editor.
   * @param aEditingHost        The editing host.
   */
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT Result<MoveNodeResult, nsresult> Run(
      HTMLEditor& aHTMLEditor, const Element& aEditingHost);

  /**
   * Returns true if there are some content nodes which can be moved to another
   * place or deleted in the line containing aPointInHardLine.  Note that if
   * there is only a padding <br> element in an empty block element, this
   * returns false even though it may be deleted.
   */
  static Result<bool, nsresult> CanMoveOrDeleteSomethingInLine(
      const EditorDOMPoint& aPointInHardLine, const Element& aEditingHost);

  AutoMoveOneLineHandler(const AutoMoveOneLineHandler& aOther) = delete;
  AutoMoveOneLineHandler(AutoMoveOneLineHandler&& aOther) = delete;

 private:
  [[nodiscard]] bool ForceMoveToEndOfContainer() const {
    return mMoveToEndOfContainer == MoveToEndOfContainer::Yes;
  }
  [[nodiscard]] EditorDOMPoint& NextInsertionPointRef() {
    if (ForceMoveToEndOfContainer()) {
      mPointToInsert.SetToEndOf(mPointToInsert.GetContainer());
    }
    return mPointToInsert;
  }

  /**
   * Consider whether Run() should preserve or does not preserve white-space
   * style of moving content.
   *
   * @param aContentInLine      Specify a content node in the moving line.
   *                            Typically, container of aPointInHardLine of
   *                            Prepare().
   * @param aInclusiveAncestorBlockOfInsertionPoint
   *                            Inclusive ancestor block element of insertion
   *                            point.  Typically, computed
   *                            mDestInclusiveAncestorBlock.
   */
  [[nodiscard]] static PreserveWhiteSpaceStyle
  ConsiderWhetherPreserveWhiteSpaceStyle(
      const nsIContent* aContentInLine,
      const Element* aInclusiveAncestorBlockOfInsertionPoint);

  /**
   * Look for inclusive ancestor block element of aBlockElement and a descendant
   * of aAncestorElement.  If aBlockElement and aAncestorElement are same one,
   * this returns nullptr.
   *
   * @param aBlockElement       A block element which is a descendant of
   *                            aAncestorElement.
   * @param aAncestorElement    An inclusive ancestor block element of
   *                            aBlockElement.
   */
  [[nodiscard]] static Element*
  GetMostDistantInclusiveAncestorBlockInSpecificAncestorElement(
      Element& aBlockElement, const Element& aAncestorElement);

  /**
   * Split ancestors at the line range boundaries and collect array of contents
   * in the line to aOutArrayOfContents.  Specify aNewContainer to the container
   * of insertion point to avoid splitting the destination.
   */
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT Result<CaretPoint, nsresult>
  SplitToMakeTheLineIsolated(
      HTMLEditor& aHTMLEditor, const nsIContent& aNewContainer,
      const Element& aEditingHost,
      nsTArray<OwningNonNull<nsIContent>>& aOutArrayOfContents) const;

  /**
   * Delete unnecessary trailing line break in aMovedContentRange if there is.
   */
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT nsresult
  DeleteUnnecessaryTrailingLineBreakInMovedLineEnd(
      HTMLEditor& aHTMLEditor, const EditorDOMRange& aMovedContentRange,
      const Element& aEditingHost) const;

  // Range of selected line.
  EditorDOMRange mLineRange;
  // Next insertion point.  If mMoveToEndOfContainer is `Yes`, this is
  // recomputed with its container in NextInsertionPointRef.  Therefore, this
  // should not be referred directly.
  EditorDOMPoint mPointToInsert;
  // An inclusive ancestor block element of the moving line.
  RefPtr<Element> mSrcInclusiveAncestorBlock;
  // An inclusive ancestor block element of the insertion point.
  RefPtr<Element> mDestInclusiveAncestorBlock;
  // nullptr if mMovingToParentBlock is false.
  // Must be non-nullptr if mMovingToParentBlock is true.  The topmost ancestor
  // block element which contains mSrcInclusiveAncestorBlock and a descendant of
  // mDestInclusiveAncestorBlock.  I.e., this may be same as
  // mSrcInclusiveAncestorBlock, but never same as mDestInclusiveAncestorBlock.
  RefPtr<Element> mTopmostSrcAncestorBlockInDestBlock;
  enum class MoveToEndOfContainer { No, Yes };
  MoveToEndOfContainer mMoveToEndOfContainer;
  PreserveWhiteSpaceStyle mPreserveWhiteSpaceStyle =
      PreserveWhiteSpaceStyle::No;
  // true if mDestInclusiveAncestorBlock is an ancestor of
  // mSrcInclusiveAncestorBlock.
  bool mMovingToParentBlock = false;
};

/**
 * Convert contents around aRanges of Run() to specified list element.  If there
 * are some different type of list elements, this method converts them to
 * specified list items too.  Basically, each line will be wrapped in a list
 * item element.  However, only when <p> element is selected, its child <br>
 * elements won't be treated as line separators.  Perhaps, this is a bug.
 */
class MOZ_STACK_CLASS HTMLEditor::AutoListElementCreator final {
 public:
  /**
   * @param aListElementTagName         The new list element tag name.
   * @param aListItemElementTagName     The new list item element tag name.
   * @param aBulletType                 If this is not empty string, it's set
   *                                    to `type` attribute of new list item
   *                                    elements.  Otherwise, existing `type`
   *                                    attributes will be removed.
   */
  AutoListElementCreator(const nsStaticAtom& aListElementTagName,
                         const nsStaticAtom& aListItemElementTagName,
                         const nsAString& aBulletType)
      // Needs const_cast hack here because the struct users may want
      // non-const nsStaticAtom pointer due to bug 1794954
      : mListTagName(const_cast<nsStaticAtom&>(aListElementTagName)),
        mListItemTagName(const_cast<nsStaticAtom&>(aListItemElementTagName)),
        mBulletType(aBulletType) {
    MOZ_ASSERT(&mListTagName == nsGkAtoms::ul ||
               &mListTagName == nsGkAtoms::ol ||
               &mListTagName == nsGkAtoms::dl);
    MOZ_ASSERT_IF(
        &mListTagName == nsGkAtoms::ul || &mListTagName == nsGkAtoms::ol,
        &mListItemTagName == nsGkAtoms::li);
    MOZ_ASSERT_IF(&mListTagName == nsGkAtoms::dl,
                  &mListItemTagName == nsGkAtoms::dt ||
                      &mListItemTagName == nsGkAtoms::dd);
  }

  /**
   * @param aHTMLEditor The HTML editor.
   * @param aRanges     [in/out] The ranges which will be converted to list.
   *                    The instance must not have saved ranges because it'll
   *                    be used in this method.
   *                    If succeeded, this will have selection ranges which
   *                    should be applied to `Selection`.
   *                    If failed, this keeps storing original selection
   *                    ranges.
   * @param aSelectAllOfCurrentList     Yes if this should treat all of
   *                                    ancestor list element at selection.
   * @param aEditingHost                The editing host.
   */
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT Result<EditActionResult, nsresult> Run(
      HTMLEditor& aHTMLEditor, AutoClonedSelectionRangeArray& aRanges,
      HTMLEditor::SelectAllOfCurrentList aSelectAllOfCurrentList,
      const Element& aEditingHost) const;

 private:
  using ContentNodeArray = nsTArray<OwningNonNull<nsIContent>>;
  using AutoContentNodeArray = AutoTArray<OwningNonNull<nsIContent>, 64>;

  /**
   * If aSelectAllOfCurrentList is "Yes" and aRanges is in a list element,
   * returns the list element.
   * Otherwise, extend aRanges to select start and end lines selected by it and
   * correct all topmost content nodes in the extended ranges with splitting
   * ancestors at range edges.
   */
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT nsresult
  SplitAtRangeEdgesAndCollectContentNodesToMoveIntoList(
      HTMLEditor& aHTMLEditor, AutoClonedRangeArray& aRanges,
      SelectAllOfCurrentList aSelectAllOfCurrentList,
      const Element& aEditingHost, ContentNodeArray& aOutArrayOfContents) const;

  /**
   * Return true if aArrayOfContents has only <br> elements or empty inline
   * container elements.  I.e., it means that aArrayOfContents represents
   * only empty line(s) if this returns true.
   */
  [[nodiscard]] static bool
  IsEmptyOrContainsOnlyBRElementsOrEmptyInlineElements(
      const ContentNodeArray& aArrayOfContents);

  /**
   * Delete all content nodes ina ArrayOfContents, and if we can put new list
   * element at start of the first range of aRanges, insert new list element
   * there.
   *
   * @return            The empty list item element in new list element.
   */
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT Result<RefPtr<Element>, nsresult>
  ReplaceContentNodesWithEmptyNewList(
      HTMLEditor& aHTMLEditor, const AutoClonedRangeArray& aRanges,
      const AutoContentNodeArray& aArrayOfContents,
      const Element& aEditingHost) const;

  /**
   * Creat new list elements or use existing list elements and move
   * aArrayOfContents into list item elements.
   *
   * @return            A list or list item element which should have caret.
   */
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT Result<RefPtr<Element>, nsresult>
  WrapContentNodesIntoNewListElements(HTMLEditor& aHTMLEditor,
                                      AutoClonedRangeArray& aRanges,
                                      AutoContentNodeArray& aArrayOfContents,
                                      const Element& aEditingHost) const;

  struct MOZ_STACK_CLASS AutoHandlingState final {
    // Current list element which is a good container to create new list item
    // element.
    RefPtr<Element> mCurrentListElement;
    // Previously handled list item element.
    RefPtr<Element> mPreviousListItemElement;
    // List or list item element which should have caret after handling all
    // contents.
    RefPtr<Element> mListOrListItemElementToPutCaret;
    // Replacing block element.  This is typically already removed from the DOM
    // tree.
    RefPtr<Element> mReplacingBlockElement;
    // Once id attribute of mReplacingBlockElement copied, the id attribute
    // shouldn't be copied again.
    bool mMaybeCopiedReplacingBlockElementId = false;
  };

  /**
   * Helper methods of WrapContentNodesIntoNewListElements.  They are called for
   * handling one content node of aArrayOfContents.  It's set to aHandling*.
   */
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT nsresult HandleChildContent(
      HTMLEditor& aHTMLEditor, nsIContent& aHandlingContent,
      AutoHandlingState& aState, const Element& aEditingHost) const;
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT nsresult
  HandleChildListElement(HTMLEditor& aHTMLEditor, Element& aHandlingListElement,
                         AutoHandlingState& aState) const;
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT nsresult HandleChildListItemElement(
      HTMLEditor& aHTMLEditor, Element& aHandlingListItemElement,
      AutoHandlingState& aState) const;
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT nsresult
  HandleChildListItemInDifferentTypeList(HTMLEditor& aHTMLEditor,
                                         Element& aHandlingListItemElement,
                                         AutoHandlingState& aState) const;
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT nsresult HandleChildListItemInSameTypeList(
      HTMLEditor& aHTMLEditor, Element& aHandlingListItemElement,
      AutoHandlingState& aState) const;
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT nsresult HandleChildDivOrParagraphElement(
      HTMLEditor& aHTMLEditor, Element& aHandlingDivOrParagraphElement,
      AutoHandlingState& aState, const Element& aEditingHost) const;
  enum class EmptyListItem { NotCreate, Create };
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT nsresult CreateAndUpdateCurrentListElement(
      HTMLEditor& aHTMLEditor, const EditorDOMPoint& aPointToInsert,
      EmptyListItem aEmptyListItem, AutoHandlingState& aState,
      const Element& aEditingHost) const;
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT Result<CreateElementResult, nsresult>
  AppendListItemElement(HTMLEditor& aHTMLEditor, const Element& aListElement,
                        AutoHandlingState& aState) const;
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT static nsresult
  MaybeCloneAttributesToNewListItem(HTMLEditor& aHTMLEditor,
                                    Element& aListItemElement,
                                    AutoHandlingState& aState);
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT nsresult HandleChildInlineContent(
      HTMLEditor& aHTMLEditor, nsIContent& aHandlingInlineContent,
      AutoHandlingState& aState) const;
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT nsresult WrapContentIntoNewListItemElement(
      HTMLEditor& aHTMLEditor, nsIContent& aHandlingContent,
      AutoHandlingState& aState) const;

  /**
   * If aRanges is collapsed outside aListItemOrListToPutCaret, this collapse
   * aRanges in aListItemOrListToPutCaret again.
   */
  nsresult EnsureCollapsedRangeIsInListItemOrListElement(
      Element& aListItemOrListToPutCaret, AutoClonedRangeArray& aRanges) const;

  MOZ_KNOWN_LIVE nsStaticAtom& mListTagName;
  MOZ_KNOWN_LIVE nsStaticAtom& mListItemTagName;
  const nsAutoString mBulletType;
};

/******************************************************************************
 * NormalizedStringToInsertText stores normalized insertion string with
 * normalized surrounding white-spaces if the insertion point is surrounded by
 * collapsible white-spaces.  For deleting invisible (collapsed) white-spaces,
 * this also stores the replace range and new white-space length before and
 * after the inserting text.
 ******************************************************************************/

struct MOZ_STACK_CLASS HTMLEditor::NormalizedStringToInsertText final {
  NormalizedStringToInsertText(
      const nsAString& aStringToInsertWithoutSurroundingWhiteSpaces,
      const EditorDOMPoint& aPointToInsert)
      : mNormalizedString(aStringToInsertWithoutSurroundingWhiteSpaces),
        mReplaceStartOffset(
            aPointToInsert.IsInTextNode() ? aPointToInsert.Offset() : 0u),
        mReplaceEndOffset(mReplaceStartOffset) {
    MOZ_ASSERT(aStringToInsertWithoutSurroundingWhiteSpaces.Length() ==
               InsertingTextLength());
  }

  NormalizedStringToInsertText(
      const nsAString& aStringToInsertWithSurroundingWhiteSpaces,
      uint32_t aInsertOffset, uint32_t aReplaceStartOffset,
      uint32_t aReplaceLength,
      uint32_t aNewPrecedingWhiteSpaceLengthBeforeInsertionString,
      uint32_t aNewFollowingWhiteSpaceLengthAfterInsertionString)
      : mNormalizedString(aStringToInsertWithSurroundingWhiteSpaces),
        mReplaceStartOffset(aReplaceStartOffset),
        mReplaceEndOffset(mReplaceStartOffset + aReplaceLength),
        mReplaceLengthBefore(aInsertOffset - mReplaceStartOffset),
        mReplaceLengthAfter(aReplaceLength - mReplaceLengthBefore),
        mNewLengthBefore(aNewPrecedingWhiteSpaceLengthBeforeInsertionString),
        mNewLengthAfter(aNewFollowingWhiteSpaceLengthAfterInsertionString) {
    MOZ_ASSERT(aReplaceStartOffset <= aInsertOffset);
    MOZ_ASSERT(aReplaceStartOffset + aReplaceLength >= aInsertOffset);
    MOZ_ASSERT(aNewPrecedingWhiteSpaceLengthBeforeInsertionString +
                   aNewFollowingWhiteSpaceLengthAfterInsertionString <
               mNormalizedString.Length());
    MOZ_ASSERT(mReplaceLengthBefore + mReplaceLengthAfter == ReplaceLength());
    MOZ_ASSERT(mReplaceLengthBefore >= mNewLengthBefore);
    MOZ_ASSERT(mReplaceLengthAfter >= mNewLengthAfter);
  }

  NormalizedStringToInsertText GetMinimizedData(const Text& aText) const {
    if (mNormalizedString.IsEmpty() || !ReplaceLength()) {
      return *this;
    }
    const nsTextFragment& textFragment = aText.TextFragment();
    const uint32_t minimizedReplaceStart = [&]() {
      const auto firstDiffCharOffset =
          mNewLengthBefore ? textFragment.FindFirstDifferentCharOffset(
                                 PrecedingWhiteSpaces(), mReplaceStartOffset)
                           : nsTextFragment::kNotFound;
      if (firstDiffCharOffset == nsTextFragment::kNotFound) {
        return
            // We don't need to insert new normalized white-spaces before the
            // inserting string,
            (mReplaceStartOffset + mReplaceLengthBefore)
            // but keep extending the replacing range for deleting invisible
            // white-spaces.
            - DeletingPrecedingInvisibleWhiteSpaces();
      }
      return firstDiffCharOffset;
    }();
    const uint32_t minimizedReplaceEnd = [&]() {
      const auto lastDiffCharOffset =
          mNewLengthAfter ? textFragment.RFindFirstDifferentCharOffset(
                                FollowingWhiteSpaces(), mReplaceEndOffset)
                          : nsTextFragment::kNotFound;
      if (lastDiffCharOffset == nsTextFragment::kNotFound) {
        return
            // We don't need to insert new normalized white-spaces after the
            // inserting string,
            (mReplaceEndOffset - mReplaceLengthAfter)
            // but keep extending the replacing range for deleting invisible
            // white-spaces.
            + DeletingFollowingInvisibleWhiteSpaces();
      }
      return lastDiffCharOffset + 1u;
    }();
    if (minimizedReplaceStart == mReplaceStartOffset &&
        minimizedReplaceEnd == mReplaceEndOffset) {
      return *this;
    }
    const uint32_t newPrecedingWhiteSpaceLength =
        mNewLengthBefore - (minimizedReplaceStart - mReplaceStartOffset);
    const uint32_t newFollowingWhiteSpaceLength =
        mNewLengthAfter - (mReplaceEndOffset - minimizedReplaceEnd);
    return NormalizedStringToInsertText(
        Substring(mNormalizedString,
                  mNewLengthBefore - newPrecedingWhiteSpaceLength,
                  mNormalizedString.Length() -
                      (mNewLengthBefore - newPrecedingWhiteSpaceLength) -
                      (mNewLengthAfter - newFollowingWhiteSpaceLength)),
        OffsetToInsertText(), minimizedReplaceStart,
        minimizedReplaceEnd - minimizedReplaceStart,
        newPrecedingWhiteSpaceLength, newFollowingWhiteSpaceLength);
  }

  /**
   * Return offset to insert the given text.
   */
  [[nodiscard]] uint32_t OffsetToInsertText() const {
    return mReplaceStartOffset + mReplaceLengthBefore;
  }

  /**
   * Return inserting text length not containing the surrounding white-spaces.
   */
  [[nodiscard]] uint32_t InsertingTextLength() const {
    return mNormalizedString.Length() - mNewLengthBefore - mNewLengthAfter;
  }

  /**
   * Return end offset of inserted string after replacing the text with
   * mNormalizedString.
   */
  [[nodiscard]] uint32_t EndOffsetOfInsertedText() const {
    return OffsetToInsertText() + InsertingTextLength();
  }

  /**
   * Return the length to replace with mNormalizedString.  The result means that
   * it's the length of surrounding white-spaces at the insertion point.
   */
  [[nodiscard]] uint32_t ReplaceLength() const {
    return mReplaceEndOffset - mReplaceStartOffset;
  }

  [[nodiscard]] uint32_t DeletingPrecedingInvisibleWhiteSpaces() const {
    return mReplaceLengthBefore - mNewLengthBefore;
  }
  [[nodiscard]] uint32_t DeletingFollowingInvisibleWhiteSpaces() const {
    return mReplaceLengthAfter - mNewLengthAfter;
  }

  [[nodiscard]] nsDependentSubstring PrecedingWhiteSpaces() const {
    return Substring(mNormalizedString, 0u, mNewLengthBefore);
  }
  [[nodiscard]] nsDependentSubstring FollowingWhiteSpaces() const {
    return Substring(mNormalizedString,
                     mNormalizedString.Length() - mNewLengthAfter);
  }

  // Normalizes string which should be inserted.
  nsAutoString mNormalizedString;
  // Start offset in the `Text` to replace.
  const uint32_t mReplaceStartOffset;
  // End offset in the `Text` to replace.
  const uint32_t mReplaceEndOffset;
  // If it needs to replace preceding and/or following white-spaces, these
  // members store the length of white-spaces which should be replaced
  // before/after the insertion point.
  const uint32_t mReplaceLengthBefore = 0u;
  const uint32_t mReplaceLengthAfter = 0u;
  // If it needs to replace preceding and/or following white-spaces, these
  // members store the new length of white-spaces before/after the insertion
  // string.
  const uint32_t mNewLengthBefore = 0u;
  const uint32_t mNewLengthAfter = 0u;
};

/******************************************************************************
 * ReplaceWhiteSpacesData stores normalized string to replace white-spaces in
 * a `Text`.  If ReplaceLength() returns 0, this user needs to do nothing.
 ******************************************************************************/

struct MOZ_STACK_CLASS HTMLEditor::ReplaceWhiteSpacesData final {
  ReplaceWhiteSpacesData() = default;

  /**
   * @param aWhiteSpaces        The new white-spaces which we will replace the
   *                            range with.
   * @param aStartOffset        Replace start offset in the text node.
   * @param aReplaceLength      Replace length in the text node.
   * @param aOffsetAfterReplacing
   *                            [optional] If the caller may want to put caret
   *                            middle of the white-spaces, the offset may be
   *                            changed by deleting some invisible white-spaces.
   *                            Therefore, this may be set for the purpose.
   */
  ReplaceWhiteSpacesData(const nsAString& aWhiteSpaces, uint32_t aStartOffset,
                         uint32_t aReplaceLength,
                         uint32_t aOffsetAfterReplacing = UINT32_MAX)
      : mNormalizedString(aWhiteSpaces),
        mReplaceStartOffset(aStartOffset),
        mReplaceEndOffset(aStartOffset + aReplaceLength),
        mNewOffsetAfterReplace(aOffsetAfterReplacing) {
    MOZ_ASSERT(ReplaceLength() >= mNormalizedString.Length());
    MOZ_ASSERT_IF(mNewOffsetAfterReplace != UINT32_MAX,
                  mNewOffsetAfterReplace <=
                      mReplaceStartOffset + mNormalizedString.Length());
  }

  /**
   * @param aWhiteSpaces        The new white-spaces which we will replace the
   *                            range with.
   * @param aStartOffset        Replace start offset in the text node.
   * @param aReplaceLength      Replace length in the text node.
   * @param aOffsetAfterReplacing
   *                            [optional] If the caller may want to put caret
   *                            middle of the white-spaces, the offset may be
   *                            changed by deleting some invisible white-spaces.
   *                            Therefore, this may be set for the purpose.
   */
  ReplaceWhiteSpacesData(nsAutoString&& aWhiteSpaces, uint32_t aStartOffset,
                         uint32_t aReplaceLength,
                         uint32_t aOffsetAfterReplacing = UINT32_MAX)
      : mNormalizedString(std::forward<nsAutoString>(aWhiteSpaces)),
        mReplaceStartOffset(aStartOffset),
        mReplaceEndOffset(aStartOffset + aReplaceLength),
        mNewOffsetAfterReplace(aOffsetAfterReplacing) {
    MOZ_ASSERT(ReplaceLength() >= mNormalizedString.Length());
    MOZ_ASSERT_IF(mNewOffsetAfterReplace != UINT32_MAX,
                  mNewOffsetAfterReplace <=
                      mReplaceStartOffset + mNormalizedString.Length());
  }

  ReplaceWhiteSpacesData GetMinimizedData(const Text& aText) const {
    if (!ReplaceLength()) {
      return *this;
    }
    const nsTextFragment& textFragment = aText.TextFragment();
    const auto minimizedReplaceStart = [&]() -> uint32_t {
      if (mNormalizedString.IsEmpty()) {
        return mReplaceStartOffset;
      }
      const uint32_t firstDiffCharOffset =
          textFragment.FindFirstDifferentCharOffset(mNormalizedString,
                                                    mReplaceStartOffset);
      if (firstDiffCharOffset == nsTextFragment::kNotFound) {
        // We don't need to insert new white-spaces,
        return mReplaceStartOffset + mNormalizedString.Length();
      }
      return firstDiffCharOffset;
    }();
    const auto minimizedReplaceEnd = [&]() -> uint32_t {
      if (mNormalizedString.IsEmpty()) {
        return mReplaceEndOffset;
      }
      if (minimizedReplaceStart ==
          mReplaceStartOffset + mNormalizedString.Length()) {
        // Note that here may be invisible white-spaces before
        // mReplaceEndOffset.  Then, this value may be larger than
        // minimizedReplaceStart.
        MOZ_ASSERT(mReplaceEndOffset >= minimizedReplaceStart);
        return mReplaceEndOffset;
      }
      if (ReplaceLength() != mNormalizedString.Length()) {
        // If we're deleting some invisible white-spaces, don't shrink the end
        // of the replacing range because it may shrink mNormalizedString too
        // much.
        return mReplaceEndOffset;
      }
      const auto lastDiffCharOffset =
          textFragment.RFindFirstDifferentCharOffset(mNormalizedString,
                                                     mReplaceEndOffset);
      MOZ_ASSERT(lastDiffCharOffset != nsTextFragment::kNotFound);
      return lastDiffCharOffset == nsTextFragment::kNotFound
                 ? mReplaceEndOffset
                 : lastDiffCharOffset + 1u;
    }();
    if (minimizedReplaceStart == mReplaceStartOffset &&
        minimizedReplaceEnd == mReplaceEndOffset) {
      return *this;
    }
    const uint32_t precedingUnnecessaryLength =
        minimizedReplaceStart - mReplaceStartOffset;
    const uint32_t followingUnnecessaryLength =
        mReplaceEndOffset - minimizedReplaceEnd;
    return ReplaceWhiteSpacesData(
        Substring(mNormalizedString, precedingUnnecessaryLength,
                  mNormalizedString.Length() - (precedingUnnecessaryLength +
                                                followingUnnecessaryLength)),
        minimizedReplaceStart, minimizedReplaceEnd - minimizedReplaceStart,
        mNewOffsetAfterReplace);
  }

  /**
   * Return the normalized string before mNewOffsetAfterReplace.  So,
   * mNewOffsetAfterReplace must not be UINT32_MAX and in the replaced range
   * when this is called.
   *
   * @param aReplaceEndOffset   Specify the offset in the Text node of
   *                            mNewOffsetAfterReplace before replacing with the
   *                            data.
   * @return The substring before mNewOffsetAfterReplace which is typically set
   * for new caret position in the Text node or collapsed deleting range
   * surrounded by the white-spaces.
   */
  [[nodiscard]] ReplaceWhiteSpacesData PreviousDataOfNewOffset(
      uint32_t aReplaceEndOffset) const {
    MOZ_ASSERT(mNewOffsetAfterReplace != UINT32_MAX);
    MOZ_ASSERT(mReplaceStartOffset <= mNewOffsetAfterReplace);
    MOZ_ASSERT(mReplaceEndOffset >= mNewOffsetAfterReplace);
    MOZ_ASSERT(mReplaceStartOffset <= aReplaceEndOffset);
    MOZ_ASSERT(mReplaceEndOffset >= aReplaceEndOffset);
    if (!ReplaceLength() || aReplaceEndOffset == mReplaceStartOffset) {
      return ReplaceWhiteSpacesData();
    }
    return ReplaceWhiteSpacesData(
        Substring(mNormalizedString, 0u,
                  mNewOffsetAfterReplace - mReplaceStartOffset),
        mReplaceStartOffset, aReplaceEndOffset - mReplaceStartOffset);
  }

  /**
   * Return the normalized string after mNewOffsetAfterReplace.  So,
   * mNewOffsetAfterReplace must not be UINT32_MAX and in the replaced range
   * when this is called.
   *
   * @param aReplaceStartOffset Specify the replace start offset with the
   *                            normalized white-spaces.
   * @return The substring after mNewOffsetAfterReplace which is typically set
   * for new caret position in the Text node or collapsed deleting range
   * surrounded by the white-spaces.
   */
  [[nodiscard]] ReplaceWhiteSpacesData NextDataOfNewOffset(
      uint32_t aReplaceStartOffset) const {
    MOZ_ASSERT(mNewOffsetAfterReplace != UINT32_MAX);
    MOZ_ASSERT(mReplaceStartOffset <= mNewOffsetAfterReplace);
    MOZ_ASSERT(mReplaceEndOffset >= mNewOffsetAfterReplace);
    MOZ_ASSERT(mReplaceStartOffset <= aReplaceStartOffset);
    MOZ_ASSERT(mReplaceEndOffset >= aReplaceStartOffset);
    if (!ReplaceLength() || aReplaceStartOffset == mReplaceEndOffset) {
      return ReplaceWhiteSpacesData();
    }
    return ReplaceWhiteSpacesData(
        Substring(mNormalizedString,
                  mNewOffsetAfterReplace - mReplaceStartOffset),
        aReplaceStartOffset, mReplaceEndOffset - aReplaceStartOffset);
  }

  [[nodiscard]] uint32_t ReplaceLength() const {
    return mReplaceEndOffset - mReplaceStartOffset;
  }
  [[nodiscard]] uint32_t DeletingInvisibleWhiteSpaces() const {
    return ReplaceLength() - mNormalizedString.Length();
  }

  [[nodiscard]] ReplaceWhiteSpacesData operator+(
      const ReplaceWhiteSpacesData& aOther) const {
    if (!ReplaceLength()) {
      return aOther;
    }
    if (!aOther.ReplaceLength()) {
      return *this;
    }
    MOZ_ASSERT(mReplaceEndOffset == aOther.mReplaceStartOffset);
    MOZ_ASSERT_IF(
        aOther.mNewOffsetAfterReplace != UINT32_MAX,
        aOther.mNewOffsetAfterReplace >= DeletingInvisibleWhiteSpaces());
    return ReplaceWhiteSpacesData(
        nsAutoString(mNormalizedString + aOther.mNormalizedString),
        mReplaceStartOffset, aOther.mReplaceEndOffset,
        aOther.mNewOffsetAfterReplace != UINT32_MAX
            ? aOther.mNewOffsetAfterReplace - DeletingInvisibleWhiteSpaces()
            : mNewOffsetAfterReplace);
  }

  nsAutoString mNormalizedString;
  const uint32_t mReplaceStartOffset = 0u;
  const uint32_t mReplaceEndOffset = 0u;
  // If the caller specifies a point in a white-space sequence, some invisible
  // white-spaces will be deleted with replacing them with normalized string.
  // Then, they may want to keep the position for putting caret or something.
  // So, this may store a specific offset in the text node after replacing.
  const uint32_t mNewOffsetAfterReplace = UINT32_MAX;
};

}  // namespace mozilla

#endif  // #ifndef HTMLEditorNestedClasses_h
