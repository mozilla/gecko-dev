/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WhiteSpaceVisibilityKeeper.h"

#include "EditorDOMPoint.h"
#include "EditorUtils.h"
#include "ErrorList.h"
#include "HTMLEditHelpers.h"  // for MoveNodeResult, SplitNodeResult
#include "HTMLEditor.h"
#include "HTMLEditorNestedClasses.h"  // for AutoMoveOneLineHandler
#include "HTMLEditUtils.h"
#include "SelectionState.h"

#include "mozilla/Assertions.h"
#include "mozilla/SelectionState.h"
#include "mozilla/OwningNonNull.h"
#include "mozilla/StaticPrefs_editor.h"  // for StaticPrefs::editor_*
#include "mozilla/InternalMutationEvent.h"
#include "mozilla/dom/AncestorIterator.h"

#include "nsCRT.h"
#include "nsContentUtils.h"
#include "nsDebug.h"
#include "nsError.h"
#include "nsIContent.h"
#include "nsIContentInlines.h"
#include "nsString.h"

namespace mozilla {

using namespace dom;

using LeafNodeType = HTMLEditUtils::LeafNodeType;
using WalkTreeOption = HTMLEditUtils::WalkTreeOption;

template nsresult WhiteSpaceVisibilityKeeper::NormalizeVisibleWhiteSpacesAt(
    HTMLEditor& aHTMLEditor, const EditorDOMPoint& aScanStartPoint,
    const Element& aEditingHost);
template nsresult WhiteSpaceVisibilityKeeper::NormalizeVisibleWhiteSpacesAt(
    HTMLEditor& aHTMLEditor, const EditorDOMPointInText& aScanStartPoint,
    const Element& aEditingHost);

Result<EditorDOMPoint, nsresult>
WhiteSpaceVisibilityKeeper::PrepareToSplitBlockElement(
    HTMLEditor& aHTMLEditor, const EditorDOMPoint& aPointToSplit,
    const Element& aSplittingBlockElement) {
  if (NS_WARN_IF(!aPointToSplit.IsInContentNodeAndValidInComposedDoc()) ||
      NS_WARN_IF(!HTMLEditUtils::IsSplittableNode(aSplittingBlockElement)) ||
      NS_WARN_IF(!EditorUtils::IsEditableContent(
          *aPointToSplit.ContainerAs<nsIContent>(), EditorType::HTML))) {
    return Err(NS_ERROR_FAILURE);
  }

  // The container of aPointToSplit may be not splittable, e.g., selection
  // may be collapsed **in** a `<br>` element or a comment node.  So, look
  // for splittable point with climbing the tree up.
  EditorDOMPoint pointToSplit(aPointToSplit);
  for (nsIContent* content : aPointToSplit.ContainerAs<nsIContent>()
                                 ->InclusiveAncestorsOfType<nsIContent>()) {
    if (content == &aSplittingBlockElement) {
      break;
    }
    if (HTMLEditUtils::IsSplittableNode(*content)) {
      break;
    }
    pointToSplit.Set(content);
  }

  // TODO: Delete this block once we ship the new normalizer.
  if (!StaticPrefs::editor_white_space_normalization_blink_compatible()) {
    AutoTrackDOMPoint tracker(aHTMLEditor.RangeUpdaterRef(), &pointToSplit);

    nsresult rv = WhiteSpaceVisibilityKeeper::
        MakeSureToKeepVisibleWhiteSpacesVisibleAfterSplit(aHTMLEditor,
                                                          pointToSplit);
    if (NS_WARN_IF(aHTMLEditor.Destroyed())) {
      return Err(NS_ERROR_EDITOR_DESTROYED);
    }
    if (NS_FAILED(rv)) {
      NS_WARNING(
          "WhiteSpaceVisibilityKeeper::"
          "MakeSureToKeepVisibleWhiteSpacesVisibleAfterSplit() failed");
      return Err(rv);
    }
  } else {
    // NOTE: Chrome does not normalize white-spaces at splitting `Text` when
    // inserting a paragraph at least when the surrounding white-spaces being or
    // end with an NBSP.
    Result<EditorDOMPoint, nsresult> pointToSplitOrError =
        WhiteSpaceVisibilityKeeper::NormalizeWhiteSpacesToSplitAt(
            aHTMLEditor, pointToSplit,
            {NormalizeOption::StopIfFollowingWhiteSpacesStartsWithNBSP,
             NormalizeOption::StopIfPrecedingWhiteSpacesEndsWithNBP});
    if (MOZ_UNLIKELY(pointToSplitOrError.isErr())) {
      NS_WARNING(
          "WhiteSpaceVisibilityKeeper::NormalizeWhiteSpacesToSplitAt() failed");
      return pointToSplitOrError.propagateErr();
    }
    pointToSplit = pointToSplitOrError.unwrap();
  }

  if (NS_WARN_IF(!pointToSplit.IsInContentNode()) ||
      NS_WARN_IF(
          !pointToSplit.ContainerAs<nsIContent>()->IsInclusiveDescendantOf(
              &aSplittingBlockElement)) ||
      NS_WARN_IF(!HTMLEditUtils::IsSplittableNode(aSplittingBlockElement)) ||
      NS_WARN_IF(!HTMLEditUtils::IsSplittableNode(
          *pointToSplit.ContainerAs<nsIContent>()))) {
    return Err(NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE);
  }

  return pointToSplit;
}

// static
Result<MoveNodeResult, nsresult> WhiteSpaceVisibilityKeeper::
    MergeFirstLineOfRightBlockElementIntoDescendantLeftBlockElement(
        HTMLEditor& aHTMLEditor, Element& aLeftBlockElement,
        Element& aRightBlockElement, const EditorDOMPoint& aAtRightBlockChild,
        const Maybe<nsAtom*>& aListElementTagName,
        const HTMLBRElement* aPrecedingInvisibleBRElement,
        const Element& aEditingHost) {
  MOZ_ASSERT(
      EditorUtils::IsDescendantOf(aLeftBlockElement, aRightBlockElement));
  MOZ_ASSERT(&aRightBlockElement == aAtRightBlockChild.GetContainer());

  OwningNonNull<Element> rightBlockElement = aRightBlockElement;
  EditorDOMPoint afterRightBlockChild = aAtRightBlockChild.NextPoint();
  if (!StaticPrefs::editor_white_space_normalization_blink_compatible()) {
    // NOTE: This method may extend deletion range:
    // - to delete invisible white-spaces at end of aLeftBlockElement
    // - to delete invisible white-spaces at start of
    //   afterRightBlockChild.GetChild()
    // - to delete invisible white-spaces before afterRightBlockChild.GetChild()
    // - to delete invisible `<br>` element at end of aLeftBlockElement

    {
      Result<CaretPoint, nsresult> caretPointOrError =
          WhiteSpaceVisibilityKeeper::DeleteInvisibleASCIIWhiteSpaces(
              aHTMLEditor, EditorDOMPoint::AtEndOf(aLeftBlockElement));
      if (MOZ_UNLIKELY(caretPointOrError.isErr())) {
        NS_WARNING(
            "WhiteSpaceVisibilityKeeper::DeleteInvisibleASCIIWhiteSpaces() "
            "failed");
        return caretPointOrError.propagateErr();
      }
      // Ignore caret suggestion because there was
      // AutoTransactionsConserveSelection.
      caretPointOrError.unwrap().IgnoreCaretPointSuggestion();
    }

    // Check whether aLeftBlockElement is a descendant of aRightBlockElement.
    if (aHTMLEditor.MayHaveMutationEventListeners()) {
      EditorDOMPoint leftBlockContainingPointInRightBlockElement;
      if (aHTMLEditor.MayHaveMutationEventListeners() &&
          MOZ_UNLIKELY(!EditorUtils::IsDescendantOf(
              aLeftBlockElement, aRightBlockElement,
              &leftBlockContainingPointInRightBlockElement))) {
        NS_WARNING(
            "Deleting invisible whitespace at end of left block element caused "
            "moving the left block element outside the right block element");
        return Err(NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE);
      }

      if (MOZ_UNLIKELY(leftBlockContainingPointInRightBlockElement !=
                       aAtRightBlockChild)) {
        NS_WARNING(
            "Deleting invisible whitespace at end of left block element caused "
            "changing the left block element in the right block element");
        return Err(NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE);
      }

      if (MOZ_UNLIKELY(!EditorUtils::IsEditableContent(aRightBlockElement,
                                                       EditorType::HTML))) {
        NS_WARNING(
            "Deleting invisible whitespace at end of left block element caused "
            "making the right block element non-editable");
        return Err(NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE);
      }

      if (MOZ_UNLIKELY(!EditorUtils::IsEditableContent(aLeftBlockElement,
                                                       EditorType::HTML))) {
        NS_WARNING(
            "Deleting invisible whitespace at end of left block element caused "
            "making the left block element non-editable");
        return Err(NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE);
      }
    }

    {
      // We can't just track rightBlockElement because it's an Element.
      AutoTrackDOMPoint tracker(aHTMLEditor.RangeUpdaterRef(),
                                &afterRightBlockChild);
      Result<CaretPoint, nsresult> caretPointOrError =
          WhiteSpaceVisibilityKeeper::DeleteInvisibleASCIIWhiteSpaces(
              aHTMLEditor, afterRightBlockChild);
      if (MOZ_UNLIKELY(caretPointOrError.isErr())) {
        NS_WARNING(
            "WhiteSpaceVisibilityKeeper::DeleteInvisibleASCIIWhiteSpaces() "
            "failed");
        return caretPointOrError.propagateErr();
      }
      // Ignore caret suggestion because there was
      // AutoTransactionsConserveSelection.
      caretPointOrError.unwrap().IgnoreCaretPointSuggestion();
    }
  } else {
    MOZ_ASSERT(
        StaticPrefs::editor_white_space_normalization_blink_compatible());

    {
      AutoTrackDOMPoint trackAfterRightBlockChild(aHTMLEditor.RangeUpdaterRef(),
                                                  &afterRightBlockChild);
      // First, delete invisible white-spaces at start of the right block and
      // normalize the leading visible white-spaces.
      nsresult rv =
          WhiteSpaceVisibilityKeeper::EnsureNoInvisibleWhiteSpacesAfter(
              aHTMLEditor, afterRightBlockChild);
      if (NS_FAILED(rv)) {
        NS_WARNING(
            "WhiteSpaceVisibilityKeeper::EnsureNoInvisibleWhiteSpacesAfter() "
            "failed");
        return Err(rv);
      }
      // Next, delete invisible white-spaces at end of the left block and
      // normalize the trailing visible white-spaces.
      rv = WhiteSpaceVisibilityKeeper::EnsureNoInvisibleWhiteSpacesBefore(
          aHTMLEditor, EditorDOMPoint::AtEndOf(aLeftBlockElement));
      if (NS_FAILED(rv)) {
        NS_WARNING(
            "WhiteSpaceVisibilityKeeper::EnsureNoInvisibleWhiteSpacesBefore() "
            "failed");
        return Err(rv);
      }
      trackAfterRightBlockChild.FlushAndStopTracking();
      if (NS_WARN_IF(afterRightBlockChild.GetContainer() !=
                     &aRightBlockElement)) {
        return Err(NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE);
      }
    }
    // Finally, make sure that we won't create new invisible white-spaces.
    AutoTrackDOMPoint trackAfterRightBlockChild(aHTMLEditor.RangeUpdaterRef(),
                                                &afterRightBlockChild);
    Result<EditorDOMPoint, nsresult> atFirstVisibleThingOrError =
        WhiteSpaceVisibilityKeeper::NormalizeWhiteSpacesAfter(
            aHTMLEditor, afterRightBlockChild,
            {NormalizeOption::StopIfFollowingWhiteSpacesStartsWithNBSP});
    if (MOZ_UNLIKELY(atFirstVisibleThingOrError.isErr())) {
      NS_WARNING(
          "WhiteSpaceVisibilityKeeper::NormalizeWhiteSpacesAfter() failed");
      return atFirstVisibleThingOrError.propagateErr();
    }
    Result<EditorDOMPoint, nsresult> afterLastVisibleThingOrError =
        WhiteSpaceVisibilityKeeper::NormalizeWhiteSpacesBefore(
            aHTMLEditor, EditorDOMPoint::AtEndOf(aLeftBlockElement), {});
    if (MOZ_UNLIKELY(afterLastVisibleThingOrError.isErr())) {
      NS_WARNING(
          "WhiteSpaceVisibilityKeeper::NormalizeWhiteSpacesAfter() failed");
      return afterLastVisibleThingOrError.propagateErr();
    }
  }

  // XXX And afterRightBlockChild.GetContainerAs<Element>() always returns
  //     an element pointer so that probably here should not use
  //     accessors of EditorDOMPoint, should use DOM API directly instead.
  if (afterRightBlockChild.GetContainerAs<Element>()) {
    rightBlockElement = *afterRightBlockChild.ContainerAs<Element>();
  } else if (NS_WARN_IF(
                 !afterRightBlockChild.GetContainerParentAs<Element>())) {
    return Err(NS_ERROR_UNEXPECTED);
  } else {
    rightBlockElement = *afterRightBlockChild.GetContainerParentAs<Element>();
  }

  auto atStartOfRightText = [&]() -> EditorDOMPoint {
    if (!StaticPrefs::editor_white_space_normalization_blink_compatible()) {
      return EditorDOMPoint();  // Don't need to normalize now.
    }
    const EditorRawDOMPointInText atFirstChar =
        WSRunScanner(Scan::All, EditorRawDOMPoint(&aRightBlockElement, 0u),
                     BlockInlineCheck::UseComputedDisplayOutsideStyle)
            .GetInclusiveNextCharPoint<EditorRawDOMPointInText>(
                EditorRawDOMPoint(&aRightBlockElement, 0u));
    if (atFirstChar.IsSet() && atFirstChar.IsCharASCIISpaceOrNBSP() &&
        HTMLEditUtils::IsSimplyEditableNode(*atFirstChar.ContainerAs<Text>())) {
      return atFirstChar.To<EditorDOMPoint>();
    }
    return EditorDOMPoint();
  }();
  AutoTrackDOMPoint trackStartOfRightText(aHTMLEditor.RangeUpdaterRef(),
                                          &atStartOfRightText);

  // Do br adjustment.
  // XXX Why don't we delete the <br> first? If so, we can skip to track the
  // MoveNodeResult at last.
  const RefPtr<HTMLBRElement> invisibleBRElementAtEndOfLeftBlockElement =
      WSRunScanner::GetPrecedingBRElementUnlessVisibleContentFound(
          WSRunScanner::Scan::EditableNodes,
          EditorDOMPoint::AtEndOf(aLeftBlockElement),
          BlockInlineCheck::UseComputedDisplayStyle);
  NS_ASSERTION(
      aPrecedingInvisibleBRElement == invisibleBRElementAtEndOfLeftBlockElement,
      "The preceding invisible BR element computation was different");
  auto moveContentResult = [&]() MOZ_NEVER_INLINE_DEBUG MOZ_CAN_RUN_SCRIPT
      -> Result<MoveNodeResult, nsresult> {
    // NOTE: Keep syncing with CanMergeLeftAndRightBlockElements() of
    //       AutoInclusiveAncestorBlockElementsJoiner.
    if (NS_WARN_IF(aListElementTagName.isSome())) {
      // Since 2002, here was the following comment:
      // > The idea here is to take all children in rightListElement that are
      // > past offset, and pull them into leftlistElement.
      // However, this has never been performed because we are here only when
      // neither left list nor right list is a descendant of the other but
      // in such case, getting a list item in the right list node almost
      // always failed since a variable for offset of
      // rightListElement->GetChildAt() was not initialized.  So, it might be
      // a bug, but we should keep this traditional behavior for now.  If you
      // find when we get here, please remove this comment if we don't need to
      // do it.  Otherwise, please move children of the right list node to the
      // end of the left list node.

      // XXX Although, we do nothing here, but for keeping traditional
      //     behavior, we should mark as handled.
      return MoveNodeResult::HandledResult(
          EditorDOMPoint::AtEndOf(aLeftBlockElement));
    }

    AutoTransactionsConserveSelection dontChangeMySelection(aHTMLEditor);
    // XXX Why do we ignore the result of AutoMoveOneLineHandler::Run()?
    NS_ASSERTION(rightBlockElement == afterRightBlockChild.GetContainer(),
                 "The relation is not guaranteed but assumed");
#ifdef DEBUG
    Result<bool, nsresult> firstLineHasContent =
        HTMLEditor::AutoMoveOneLineHandler::CanMoveOrDeleteSomethingInLine(
            EditorDOMPoint(rightBlockElement, afterRightBlockChild.Offset()),
            aEditingHost);
#endif  // #ifdef DEBUG
    HTMLEditor::AutoMoveOneLineHandler lineMoverToEndOfLeftBlock(
        aLeftBlockElement);
    nsresult rv = lineMoverToEndOfLeftBlock.Prepare(
        aHTMLEditor,
        EditorDOMPoint(rightBlockElement, afterRightBlockChild.Offset()),
        aEditingHost);
    if (NS_FAILED(rv)) {
      NS_WARNING("AutoMoveOneLineHandler::Prepare() failed");
      return Err(rv);
    }
    MoveNodeResult moveResult = MoveNodeResult::IgnoredResult(
        EditorDOMPoint::AtEndOf(aLeftBlockElement));
    AutoTrackDOMMoveNodeResult trackMoveResult(aHTMLEditor.RangeUpdaterRef(),
                                               &moveResult);
    Result<MoveNodeResult, nsresult> moveFirstLineResult =
        lineMoverToEndOfLeftBlock.Run(aHTMLEditor, aEditingHost);
    if (MOZ_UNLIKELY(moveFirstLineResult.isErr())) {
      NS_WARNING("AutoMoveOneLineHandler::Run() failed");
      return moveFirstLineResult.propagateErr();
    }
    trackMoveResult.FlushAndStopTracking();

#ifdef DEBUG
    MOZ_ASSERT(!firstLineHasContent.isErr());
    if (firstLineHasContent.inspect()) {
      NS_ASSERTION(moveFirstLineResult.inspect().Handled(),
                   "Failed to consider whether moving or not something");
    } else {
      NS_ASSERTION(moveFirstLineResult.inspect().Ignored(),
                   "Failed to consider whether moving or not something");
    }
#endif  // #ifdef DEBUG

    moveResult |= moveFirstLineResult.unwrap();
    // Now, all children of rightBlockElement were moved to leftBlockElement.
    // So, afterRightBlockChild is now invalid.
    afterRightBlockChild.Clear();

    return std::move(moveResult);
  }();
  if (MOZ_UNLIKELY(moveContentResult.isErr())) {
    return moveContentResult;
  }

  MoveNodeResult unwrappedMoveContentResult = moveContentResult.unwrap();

  trackStartOfRightText.FlushAndStopTracking();
  if (atStartOfRightText.IsInTextNode() &&
      atStartOfRightText.IsSetAndValidInComposedDoc() &&
      atStartOfRightText.IsMiddleOfContainer()) {
    AutoTrackDOMMoveNodeResult trackMoveContentResult(
        aHTMLEditor.RangeUpdaterRef(), &unwrappedMoveContentResult);
    Result<EditorDOMPoint, nsresult> startOfRightTextOrError =
        WhiteSpaceVisibilityKeeper::NormalizeWhiteSpacesAt(
            aHTMLEditor, atStartOfRightText.AsInText());
    if (MOZ_UNLIKELY(startOfRightTextOrError.isErr())) {
      NS_WARNING("WhiteSpaceVisibilityKeeper::NormalizeWhiteSpacesAt() failed");
      return startOfRightTextOrError.propagateErr();
    }
  }

  if (!invisibleBRElementAtEndOfLeftBlockElement ||
      !invisibleBRElementAtEndOfLeftBlockElement->IsInComposedDoc()) {
    return std::move(unwrappedMoveContentResult);
  }

  {
    AutoTransactionsConserveSelection dontChangeMySelection(aHTMLEditor);
    AutoTrackDOMMoveNodeResult trackMoveContentResult(
        aHTMLEditor.RangeUpdaterRef(), &unwrappedMoveContentResult);
    nsresult rv = aHTMLEditor.DeleteNodeWithTransaction(
        *invisibleBRElementAtEndOfLeftBlockElement);
    if (NS_FAILED(rv)) {
      NS_WARNING("EditorBase::DeleteNodeWithTransaction() failed, but ignored");
      unwrappedMoveContentResult.IgnoreCaretPointSuggestion();
      return Err(rv);
    }
  }
  return std::move(unwrappedMoveContentResult);
}

// static
Result<MoveNodeResult, nsresult> WhiteSpaceVisibilityKeeper::
    MergeFirstLineOfRightBlockElementIntoAncestorLeftBlockElement(
        HTMLEditor& aHTMLEditor, Element& aLeftBlockElement,
        Element& aRightBlockElement, const EditorDOMPoint& aAtLeftBlockChild,
        nsIContent& aLeftContentInBlock,
        const Maybe<nsAtom*>& aListElementTagName,
        const HTMLBRElement* aPrecedingInvisibleBRElement,
        const Element& aEditingHost) {
  MOZ_ASSERT(
      EditorUtils::IsDescendantOf(aRightBlockElement, aLeftBlockElement));
  MOZ_ASSERT(
      &aLeftBlockElement == &aLeftContentInBlock ||
      EditorUtils::IsDescendantOf(aLeftContentInBlock, aLeftBlockElement));
  MOZ_ASSERT(&aLeftBlockElement == aAtLeftBlockChild.GetContainer());

  OwningNonNull<Element> originalLeftBlockElement = aLeftBlockElement;
  OwningNonNull<Element> leftBlockElement = aLeftBlockElement;
  EditorDOMPoint atLeftBlockChild(aAtLeftBlockChild);
  if (!StaticPrefs::editor_white_space_normalization_blink_compatible()) {
    // NOTE: This method may extend deletion range:
    // - to delete invisible white-spaces at start of aRightBlockElement
    // - to delete invisible white-spaces before aRightBlockElement
    // - to delete invisible white-spaces at start of
    // aAtLeftBlockChild.GetChild()
    // - to delete invisible white-spaces before aAtLeftBlockChild.GetChild()
    // - to delete invisible `<br>` element before aAtLeftBlockChild.GetChild()

    {
      Result<CaretPoint, nsresult> caretPointOrError =
          WhiteSpaceVisibilityKeeper::DeleteInvisibleASCIIWhiteSpaces(
              aHTMLEditor, EditorDOMPoint(&aRightBlockElement, 0));
      if (MOZ_UNLIKELY(caretPointOrError.isErr())) {
        NS_WARNING(
            "WhiteSpaceVisibilityKeeper::DeleteInvisibleASCIIWhiteSpaces() "
            "failed");
        return caretPointOrError.propagateErr();
      }
      // Ignore caret suggestion because there was
      // AutoTransactionsConserveSelection.
      caretPointOrError.unwrap().IgnoreCaretPointSuggestion();
    }

    // Check whether aRightBlockElement is a descendant of aLeftBlockElement.
    if (aHTMLEditor.MayHaveMutationEventListeners()) {
      EditorDOMPoint rightBlockContainingPointInLeftBlockElement;
      if (aHTMLEditor.MayHaveMutationEventListeners() &&
          MOZ_UNLIKELY(!EditorUtils::IsDescendantOf(
              aRightBlockElement, aLeftBlockElement,
              &rightBlockContainingPointInLeftBlockElement))) {
        NS_WARNING(
            "Deleting invisible whitespace at start of right block element "
            "caused moving the right block element outside the left block "
            "element");
        return Err(NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE);
      }

      if (MOZ_UNLIKELY(rightBlockContainingPointInLeftBlockElement !=
                       aAtLeftBlockChild)) {
        NS_WARNING(
            "Deleting invisible whitespace at start of right block element "
            "caused changing the right block element position in the left "
            "block element");
        return Err(NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE);
      }

      if (MOZ_UNLIKELY(!EditorUtils::IsEditableContent(aLeftBlockElement,
                                                       EditorType::HTML))) {
        NS_WARNING(
            "Deleting invisible whitespace at start of right block element "
            "caused making the left block element non-editable");
        return Err(NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE);
      }

      if (MOZ_UNLIKELY(!EditorUtils::IsEditableContent(aRightBlockElement,
                                                       EditorType::HTML))) {
        NS_WARNING(
            "Deleting invisible whitespace at start of right block element "
            "caused making the right block element non-editable");
        return Err(NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE);
      }
    }

    {
      // We can't just track leftBlockElement because it's an Element, so track
      // something else.
      AutoTrackDOMPoint tracker(aHTMLEditor.RangeUpdaterRef(),
                                &atLeftBlockChild);
      Result<CaretPoint, nsresult> caretPointOrError =
          WhiteSpaceVisibilityKeeper::DeleteInvisibleASCIIWhiteSpaces(
              aHTMLEditor, EditorDOMPoint(atLeftBlockChild.GetContainer(),
                                          atLeftBlockChild.Offset()));
      if (MOZ_UNLIKELY(caretPointOrError.isErr())) {
        NS_WARNING(
            "WhiteSpaceVisibilityKeeper::DeleteInvisibleASCIIWhiteSpaces() "
            "failed");
        return caretPointOrError.propagateErr();
      }
      // Ignore caret suggestion because there was
      // AutoTransactionsConserveSelection.
      caretPointOrError.unwrap().IgnoreCaretPointSuggestion();
    }
    if (MOZ_UNLIKELY(!atLeftBlockChild.IsSetAndValid())) {
      NS_WARNING(
          "WhiteSpaceVisibilityKeeper::DeleteInvisibleASCIIWhiteSpaces() "
          "caused unexpected DOM tree");
      return Err(NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE);
    }
  } else {
    MOZ_ASSERT(
        StaticPrefs::editor_white_space_normalization_blink_compatible());

    // First, delete invisible white-spaces before the right block.
    {
      AutoTrackDOMPoint tracker(aHTMLEditor.RangeUpdaterRef(),
                                &atLeftBlockChild);
      nsresult rv =
          WhiteSpaceVisibilityKeeper::EnsureNoInvisibleWhiteSpacesBefore(
              aHTMLEditor, EditorDOMPoint(&aRightBlockElement));
      if (NS_FAILED(rv)) {
        NS_WARNING(
            "WhiteSpaceVisibilityKeeper::EnsureNoInvisibleWhiteSpacesBefore() "
            "failed");
        return Err(rv);
      }
      // Next, delete invisible white-spaces at start of the right block.
      rv = WhiteSpaceVisibilityKeeper::EnsureNoInvisibleWhiteSpacesAfter(
          aHTMLEditor, EditorDOMPoint(&aRightBlockElement, 0u));
      if (NS_FAILED(rv)) {
        NS_WARNING(
            "WhiteSpaceVisibilityKeeper::EnsureNoInvisibleWhiteSpacesAfter() "
            "failed");
        return Err(rv);
      }
      tracker.FlushAndStopTracking();
      if (NS_WARN_IF(
              !atLeftBlockChild.IsInContentNodeAndValidInComposedDoc())) {
        return Err(NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE);
      }
    }
    // Finally, make sure that we won't create new invisible white-spaces.
    AutoTrackDOMPoint tracker(aHTMLEditor.RangeUpdaterRef(), &atLeftBlockChild);
    Result<EditorDOMPoint, nsresult> afterLastVisibleThingOrError =
        WhiteSpaceVisibilityKeeper::NormalizeWhiteSpacesAfter(
            aHTMLEditor, EditorDOMPoint(&aRightBlockElement, 0u),
            {NormalizeOption::StopIfPrecedingWhiteSpacesEndsWithNBP});
    if (MOZ_UNLIKELY(afterLastVisibleThingOrError.isErr())) {
      NS_WARNING(
          "WhiteSpaceVisibilityKeeper::NormalizeWhiteSpacesAfter() failed");
      return afterLastVisibleThingOrError.propagateErr();
    }
    Result<EditorDOMPoint, nsresult> atFirstVisibleThingOrError =
        WhiteSpaceVisibilityKeeper::NormalizeWhiteSpacesBefore(
            aHTMLEditor, atLeftBlockChild, {});
    if (MOZ_UNLIKELY(atFirstVisibleThingOrError.isErr())) {
      NS_WARNING(
          "WhiteSpaceVisibilityKeeper::NormalizeWhiteSpacesAfter() failed");
      return atFirstVisibleThingOrError.propagateErr();
    }
    tracker.FlushAndStopTracking();
    if (NS_WARN_IF(!atLeftBlockChild.IsInContentNodeAndValidInComposedDoc())) {
      return Err(NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE);
    }
  }

  // XXX atLeftBlockChild.GetContainerAs<Element>() should always return
  //     an element pointer so that probably here should not use
  //     accessors of EditorDOMPoint, should use DOM API directly instead.
  if (Element* nearestAncestor =
          atLeftBlockChild.GetContainerOrContainerParentElement()) {
    leftBlockElement = *nearestAncestor;
  } else {
    return Err(NS_ERROR_UNEXPECTED);
  }

  auto atStartOfRightText = [&]() -> EditorDOMPoint {
    if (!StaticPrefs::editor_white_space_normalization_blink_compatible()) {
      return EditorDOMPoint();  // Don't need to normalize now.
    }
    const EditorRawDOMPointInText atFirstChar =
        WSRunScanner(Scan::All, EditorRawDOMPoint(&aRightBlockElement, 0u),
                     BlockInlineCheck::UseComputedDisplayOutsideStyle)
            .GetInclusiveNextCharPoint<EditorRawDOMPointInText>(
                EditorRawDOMPoint(&aRightBlockElement, 0u));
    if (atFirstChar.IsSet() && atFirstChar.IsCharASCIISpaceOrNBSP() &&
        HTMLEditUtils::IsSimplyEditableNode(*atFirstChar.ContainerAs<Text>())) {
      return atFirstChar.To<EditorDOMPoint>();
    }
    return EditorDOMPoint();
  }();
  AutoTrackDOMPoint trackStartOfRightText(aHTMLEditor.RangeUpdaterRef(),
                                          &atStartOfRightText);

  // Do br adjustment.
  // XXX Why don't we delete the <br> first? If so, we can skip to track the
  // MoveNodeResult at last.
  const RefPtr<HTMLBRElement> invisibleBRElementBeforeLeftBlockElement =
      WSRunScanner::GetPrecedingBRElementUnlessVisibleContentFound(
          WSRunScanner::Scan::EditableNodes, atLeftBlockChild,
          BlockInlineCheck::UseComputedDisplayStyle);
  NS_ASSERTION(
      aPrecedingInvisibleBRElement == invisibleBRElementBeforeLeftBlockElement,
      "The preceding invisible BR element computation was different");
  auto moveContentResult = [&]() MOZ_NEVER_INLINE_DEBUG MOZ_CAN_RUN_SCRIPT
      -> Result<MoveNodeResult, nsresult> {
    // NOTE: Keep syncing with CanMergeLeftAndRightBlockElements() of
    //       AutoInclusiveAncestorBlockElementsJoiner.
    if (aListElementTagName.isSome()) {
      // XXX Why do we ignore the error from MoveChildrenWithTransaction()?
      MOZ_ASSERT(originalLeftBlockElement == atLeftBlockChild.GetContainer(),
                 "This is not guaranteed, but assumed");
#ifdef DEBUG
      Result<bool, nsresult> rightBlockHasContent =
          aHTMLEditor.CanMoveChildren(aRightBlockElement, aLeftBlockElement);
#endif  // #ifdef DEBUG
      MoveNodeResult moveResult = MoveNodeResult::IgnoredResult(EditorDOMPoint(
          atLeftBlockChild.GetContainer(), atLeftBlockChild.Offset()));
      AutoTrackDOMMoveNodeResult trackMoveResult(aHTMLEditor.RangeUpdaterRef(),
                                                 &moveResult);
      // TODO: Stop using HTMLEditor::PreserveWhiteSpaceStyle::No due to no
      // tests.
      AutoTransactionsConserveSelection dontChangeMySelection(aHTMLEditor);
      Result<MoveNodeResult, nsresult> moveChildrenResult =
          aHTMLEditor.MoveChildrenWithTransaction(
              aRightBlockElement, moveResult.NextInsertionPointRef(),
              HTMLEditor::PreserveWhiteSpaceStyle::No,
              HTMLEditor::RemoveIfCommentNode::Yes);
      if (MOZ_UNLIKELY(moveChildrenResult.isErr())) {
        if (NS_WARN_IF(moveChildrenResult.inspectErr() ==
                       NS_ERROR_EDITOR_DESTROYED)) {
          return moveChildrenResult;
        }
        NS_WARNING(
            "HTMLEditor::MoveChildrenWithTransaction() failed, but ignored");
      } else {
#ifdef DEBUG
        MOZ_ASSERT(!rightBlockHasContent.isErr());
        if (rightBlockHasContent.inspect()) {
          NS_ASSERTION(moveChildrenResult.inspect().Handled(),
                       "Failed to consider whether moving or not children");
        } else {
          NS_ASSERTION(moveChildrenResult.inspect().Ignored(),
                       "Failed to consider whether moving or not children");
        }
#endif  // #ifdef DEBUG
        trackMoveResult.FlushAndStopTracking();
        moveResult |= moveChildrenResult.unwrap();
      }
      // atLeftBlockChild was moved to rightListElement.  So, it's invalid now.
      atLeftBlockChild.Clear();

      return std::move(moveResult);
    }

    // Left block is a parent of right block, and the parent of the previous
    // visible content.  Right block is a child and contains the contents we
    // want to move.
    EditorDOMPoint pointToMoveFirstLineContent;
    if (&aLeftContentInBlock == leftBlockElement) {
      // We are working with valid HTML, aLeftContentInBlock is a block
      // element, and is therefore allowed to contain aRightBlockElement. This
      // is the simple case, we will simply move the content in
      // aRightBlockElement out of its block.
      pointToMoveFirstLineContent = atLeftBlockChild;
      MOZ_ASSERT(pointToMoveFirstLineContent.GetContainer() ==
                 &aLeftBlockElement);
    } else {
      if (NS_WARN_IF(!aLeftContentInBlock.IsInComposedDoc())) {
        return Err(NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE);
      }
      // We try to work as well as possible with HTML that's already invalid.
      // Although "right block" is a block, and a block must not be contained
      // in inline elements, reality is that broken documents do exist.  The
      // DIRECT parent of "left NODE" might be an inline element.  Previous
      // versions of this code skipped inline parents until the first block
      // parent was found (and used "left block" as the destination).
      // However, in some situations this strategy moves the content to an
      // unexpected position.  (see bug 200416) The new idea is to make the
      // moving content a sibling, next to the previous visible content.
      pointToMoveFirstLineContent.SetAfter(&aLeftContentInBlock);
      if (NS_WARN_IF(!pointToMoveFirstLineContent.IsInContentNode())) {
        return Err(NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE);
      }
    }

    MOZ_ASSERT(pointToMoveFirstLineContent.IsSetAndValid());

    // Because we don't want the moving content to receive the style of the
    // previous content, we split the previous content's style.

#ifdef DEBUG
    Result<bool, nsresult> firstLineHasContent =
        HTMLEditor::AutoMoveOneLineHandler::CanMoveOrDeleteSomethingInLine(
            EditorDOMPoint(&aRightBlockElement, 0u), aEditingHost);
#endif  // #ifdef DEBUG

    if (&aLeftContentInBlock != &aEditingHost) {
      Result<SplitNodeResult, nsresult> splitNodeResult =
          aHTMLEditor.SplitAncestorStyledInlineElementsAt(
              pointToMoveFirstLineContent, EditorInlineStyle::RemoveAllStyles(),
              HTMLEditor::SplitAtEdges::eDoNotCreateEmptyContainer);
      if (MOZ_UNLIKELY(splitNodeResult.isErr())) {
        NS_WARNING("HTMLEditor::SplitAncestorStyledInlineElementsAt() failed");
        return splitNodeResult.propagateErr();
      }
      SplitNodeResult unwrappedSplitNodeResult = splitNodeResult.unwrap();
      nsresult rv = unwrappedSplitNodeResult.SuggestCaretPointTo(
          aHTMLEditor, {SuggestCaret::OnlyIfHasSuggestion,
                        SuggestCaret::OnlyIfTransactionsAllowedToDoIt});
      if (NS_FAILED(rv)) {
        NS_WARNING("SplitNodeResult::SuggestCaretPointTo() failed");
        return Err(rv);
      }
      if (!unwrappedSplitNodeResult.DidSplit()) {
        // If nothing was split, we should move the first line content to
        // after the parent inline elements.
        for (EditorDOMPoint parentPoint = pointToMoveFirstLineContent;
             pointToMoveFirstLineContent.IsEndOfContainer() &&
             pointToMoveFirstLineContent.IsInContentNode();
             pointToMoveFirstLineContent = EditorDOMPoint::After(
                 *pointToMoveFirstLineContent.ContainerAs<nsIContent>())) {
          if (pointToMoveFirstLineContent.GetContainer() ==
                  &aLeftBlockElement ||
              NS_WARN_IF(pointToMoveFirstLineContent.GetContainer() ==
                         &aEditingHost)) {
            break;
          }
        }
        if (NS_WARN_IF(!pointToMoveFirstLineContent.IsInContentNode())) {
          return Err(NS_ERROR_FAILURE);
        }
      } else if (unwrappedSplitNodeResult.Handled()) {
        // If se split something, we should move the first line contents
        // before the right elements.
        if (nsIContent* nextContentAtSplitPoint =
                unwrappedSplitNodeResult.GetNextContent()) {
          pointToMoveFirstLineContent.Set(nextContentAtSplitPoint);
          if (NS_WARN_IF(!pointToMoveFirstLineContent.IsInContentNode())) {
            return Err(NS_ERROR_FAILURE);
          }
        } else {
          pointToMoveFirstLineContent =
              unwrappedSplitNodeResult.AtSplitPoint<EditorDOMPoint>();
          if (NS_WARN_IF(!pointToMoveFirstLineContent.IsInContentNode())) {
            return Err(NS_ERROR_FAILURE);
          }
        }
      }
      MOZ_DIAGNOSTIC_ASSERT(pointToMoveFirstLineContent.IsSetAndValid());
    }

    MoveNodeResult moveResult =
        MoveNodeResult::IgnoredResult(pointToMoveFirstLineContent);
    HTMLEditor::AutoMoveOneLineHandler lineMoverToPoint(
        pointToMoveFirstLineContent);
    nsresult rv = lineMoverToPoint.Prepare(
        aHTMLEditor, EditorDOMPoint(&aRightBlockElement, 0u), aEditingHost);
    if (NS_FAILED(rv)) {
      NS_WARNING("AutoMoveOneLineHandler::Prepare() failed");
      return Err(rv);
    }
    AutoTrackDOMMoveNodeResult trackMoveResult(aHTMLEditor.RangeUpdaterRef(),
                                               &moveResult);
    AutoTransactionsConserveSelection dontChangeMySelection(aHTMLEditor);
    Result<MoveNodeResult, nsresult> moveFirstLineResult =
        lineMoverToPoint.Run(aHTMLEditor, aEditingHost);
    if (MOZ_UNLIKELY(moveFirstLineResult.isErr())) {
      NS_WARNING("AutoMoveOneLineHandler::Run() failed");
      return moveFirstLineResult.propagateErr();
    }

#ifdef DEBUG
    MOZ_ASSERT(!firstLineHasContent.isErr());
    if (firstLineHasContent.inspect()) {
      NS_ASSERTION(moveFirstLineResult.inspect().Handled(),
                   "Failed to consider whether moving or not something");
    } else {
      NS_ASSERTION(moveFirstLineResult.inspect().Ignored(),
                   "Failed to consider whether moving or not something");
    }
#endif  // #ifdef DEBUG

    trackMoveResult.FlushAndStopTracking();
    moveResult |= moveFirstLineResult.unwrap();
    return std::move(moveResult);
  }();
  if (MOZ_UNLIKELY(moveContentResult.isErr())) {
    return moveContentResult;
  }

  MoveNodeResult unwrappedMoveContentResult = moveContentResult.unwrap();

  trackStartOfRightText.FlushAndStopTracking();
  if (atStartOfRightText.IsInTextNode() &&
      atStartOfRightText.IsSetAndValidInComposedDoc() &&
      atStartOfRightText.IsMiddleOfContainer()) {
    AutoTrackDOMMoveNodeResult trackMoveContentResult(
        aHTMLEditor.RangeUpdaterRef(), &unwrappedMoveContentResult);
    Result<EditorDOMPoint, nsresult> startOfRightTextOrError =
        WhiteSpaceVisibilityKeeper::NormalizeWhiteSpacesAt(
            aHTMLEditor, atStartOfRightText.AsInText());
    if (MOZ_UNLIKELY(startOfRightTextOrError.isErr())) {
      NS_WARNING("WhiteSpaceVisibilityKeeper::NormalizeWhiteSpacesAt() failed");
      return startOfRightTextOrError.propagateErr();
    }
  }

  if (!invisibleBRElementBeforeLeftBlockElement ||
      !invisibleBRElementBeforeLeftBlockElement->IsInComposedDoc()) {
    return std::move(unwrappedMoveContentResult);
  }

  {
    AutoTrackDOMMoveNodeResult trackMoveContentResult(
        aHTMLEditor.RangeUpdaterRef(), &unwrappedMoveContentResult);
    AutoTransactionsConserveSelection dontChangeMySelection(aHTMLEditor);
    nsresult rv = aHTMLEditor.DeleteNodeWithTransaction(
        *invisibleBRElementBeforeLeftBlockElement);
    if (NS_FAILED(rv)) {
      NS_WARNING("EditorBase::DeleteNodeWithTransaction() failed, but ignored");
      unwrappedMoveContentResult.IgnoreCaretPointSuggestion();
      return Err(rv);
    }
  }
  return std::move(unwrappedMoveContentResult);
}

// static
Result<MoveNodeResult, nsresult> WhiteSpaceVisibilityKeeper::
    MergeFirstLineOfRightBlockElementIntoLeftBlockElement(
        HTMLEditor& aHTMLEditor, Element& aLeftBlockElement,
        Element& aRightBlockElement, const Maybe<nsAtom*>& aListElementTagName,
        const HTMLBRElement* aPrecedingInvisibleBRElement,
        const Element& aEditingHost) {
  MOZ_ASSERT(
      !EditorUtils::IsDescendantOf(aLeftBlockElement, aRightBlockElement));
  MOZ_ASSERT(
      !EditorUtils::IsDescendantOf(aRightBlockElement, aLeftBlockElement));

  if (!StaticPrefs::editor_white_space_normalization_blink_compatible()) {
    // NOTE: This method may extend deletion range:
    // - to delete invisible white-spaces at end of aLeftBlockElement
    // - to delete invisible white-spaces at start of aRightBlockElement
    // - to delete invisible `<br>` element at end of aLeftBlockElement

    // Adjust white-space at block boundaries
    Result<CaretPoint, nsresult> caretPointOrError =
        WhiteSpaceVisibilityKeeper::
            MakeSureToKeepVisibleStateOfWhiteSpacesAroundDeletingRange(
                aHTMLEditor,
                EditorDOMRange(EditorDOMPoint::AtEndOf(aLeftBlockElement),
                               EditorDOMPoint(&aRightBlockElement, 0)),
                aEditingHost);
    if (MOZ_UNLIKELY(caretPointOrError.isErr())) {
      NS_WARNING(
          "WhiteSpaceVisibilityKeeper::"
          "MakeSureToKeepVisibleStateOfWhiteSpacesAroundDeletingRange() "
          "failed");
      return caretPointOrError.propagateErr();
    }
    // Ignore caret point suggestion because there was
    // AutoTransactionsConserveSelection.
    caretPointOrError.unwrap().IgnoreCaretPointSuggestion();
  } else {
    MOZ_ASSERT(
        StaticPrefs::editor_white_space_normalization_blink_compatible());
    // First, delete invisible white-spaces at end of the left block
    nsresult rv =
        WhiteSpaceVisibilityKeeper::EnsureNoInvisibleWhiteSpacesBefore(
            aHTMLEditor, EditorDOMPoint::AtEndOf(aLeftBlockElement));
    if (NS_FAILED(rv)) {
      NS_WARNING(
          "WhiteSpaceVisibilityKeeper::EnsureNoInvisibleWhiteSpacesBefore() "
          "failed");
      return Err(rv);
    }
    // Next, delete invisible white-spaces at start of the right block and
    // normalize the leading visible white-spaces.
    rv = WhiteSpaceVisibilityKeeper::EnsureNoInvisibleWhiteSpacesAfter(
        aHTMLEditor, EditorDOMPoint(&aRightBlockElement, 0u));
    if (NS_FAILED(rv)) {
      NS_WARNING(
          "WhiteSpaceVisibilityKeeper::EnsureNoInvisibleWhiteSpacesAfter() "
          "failed");
      return Err(rv);
    }
    // Finally, make sure to that we won't create new invisible white-spaces.
    Result<EditorDOMPoint, nsresult> atFirstVisibleThingOrError =
        WhiteSpaceVisibilityKeeper::NormalizeWhiteSpacesAfter(
            aHTMLEditor, EditorDOMPoint(&aRightBlockElement, 0u),
            {NormalizeOption::StopIfFollowingWhiteSpacesStartsWithNBSP});
    if (MOZ_UNLIKELY(atFirstVisibleThingOrError.isErr())) {
      NS_WARNING(
          "WhiteSpaceVisibilityKeeper::NormalizeWhiteSpacesAfter() failed");
      return atFirstVisibleThingOrError.propagateErr();
    }
    Result<EditorDOMPoint, nsresult> afterLastVisibleThingOrError =
        WhiteSpaceVisibilityKeeper::NormalizeWhiteSpacesBefore(
            aHTMLEditor, EditorDOMPoint::AtEndOf(aLeftBlockElement), {});
    if (MOZ_UNLIKELY(afterLastVisibleThingOrError.isErr())) {
      NS_WARNING(
          "WhiteSpaceVisibilityKeeper::NormalizeWhiteSpacesBefore() failed");
      return afterLastVisibleThingOrError.propagateErr();
    }
  }
  auto atStartOfRightText = [&]() -> EditorDOMPoint {
    if (!StaticPrefs::editor_white_space_normalization_blink_compatible()) {
      return EditorDOMPoint();  // Don't need to normalize now.
    }
    const EditorRawDOMPointInText atFirstChar =
        WSRunScanner(Scan::All, EditorRawDOMPoint(&aRightBlockElement, 0u),
                     BlockInlineCheck::UseComputedDisplayOutsideStyle)
            .GetInclusiveNextCharPoint<EditorRawDOMPointInText>(
                EditorRawDOMPoint(&aRightBlockElement, 0u));
    if (atFirstChar.IsSet() && atFirstChar.IsCharASCIISpaceOrNBSP() &&
        HTMLEditUtils::IsSimplyEditableNode(*atFirstChar.ContainerAs<Text>())) {
      return atFirstChar.To<EditorDOMPoint>();
    }
    return EditorDOMPoint();
  }();
  AutoTrackDOMPoint trackStartOfRightText(aHTMLEditor.RangeUpdaterRef(),
                                          &atStartOfRightText);
  // Do br adjustment.
  // XXX Why don't we delete the <br> first? If so, we can skip to track the
  // MoveNodeResult at last.
  const RefPtr<HTMLBRElement> invisibleBRElementAtEndOfLeftBlockElement =
      WSRunScanner::GetPrecedingBRElementUnlessVisibleContentFound(
          WSRunScanner::Scan::EditableNodes,
          EditorDOMPoint::AtEndOf(aLeftBlockElement),
          BlockInlineCheck::UseComputedDisplayStyle);
  NS_ASSERTION(
      aPrecedingInvisibleBRElement == invisibleBRElementAtEndOfLeftBlockElement,
      "The preceding invisible BR element computation was different");
  auto moveContentResult = [&]() MOZ_NEVER_INLINE_DEBUG MOZ_CAN_RUN_SCRIPT
      -> Result<MoveNodeResult, nsresult> {
    if (aListElementTagName.isSome() ||
        // TODO: We should stop merging entire blocks even if they have same
        // white-space style because Chrome behave so.  However, it's risky to
        // change our behavior in the major cases so that we should do it in
        // a bug to manage only the change.
        (aLeftBlockElement.NodeInfo()->NameAtom() ==
             aRightBlockElement.NodeInfo()->NameAtom() &&
         EditorUtils::GetComputedWhiteSpaceStyles(aLeftBlockElement) ==
             EditorUtils::GetComputedWhiteSpaceStyles(aRightBlockElement))) {
      MoveNodeResult moveResult = MoveNodeResult::IgnoredResult(
          EditorDOMPoint::AtEndOf(aLeftBlockElement));
      AutoTrackDOMMoveNodeResult trackMoveResult(aHTMLEditor.RangeUpdaterRef(),
                                                 &moveResult);
      AutoTransactionsConserveSelection dontChangeMySelection(aHTMLEditor);
      // Nodes are same type.  merge them.
      EditorDOMPoint atFirstChildOfRightNode;
      nsresult rv = aHTMLEditor.JoinNearestEditableNodesWithTransaction(
          aLeftBlockElement, aRightBlockElement, &atFirstChildOfRightNode);
      if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
        return Err(NS_ERROR_EDITOR_DESTROYED);
      }
      NS_WARNING_ASSERTION(
          NS_SUCCEEDED(rv),
          "HTMLEditor::JoinNearestEditableNodesWithTransaction()"
          " failed, but ignored");
      if (aListElementTagName.isSome() && atFirstChildOfRightNode.IsSet()) {
        Result<CreateElementResult, nsresult> convertListTypeResult =
            aHTMLEditor.ChangeListElementType(
                // XXX Shouldn't be aLeftBlockElement here?
                aRightBlockElement, MOZ_KnownLive(*aListElementTagName.ref()),
                *nsGkAtoms::li);
        if (MOZ_UNLIKELY(convertListTypeResult.isErr())) {
          if (NS_WARN_IF(convertListTypeResult.inspectErr() ==
                         NS_ERROR_EDITOR_DESTROYED)) {
            return Err(NS_ERROR_EDITOR_DESTROYED);
          }
          NS_WARNING("HTMLEditor::ChangeListElementType() failed, but ignored");
        } else {
          // There is AutoTransactionConserveSelection above, therefore, we
          // don't need to update selection here.
          convertListTypeResult.inspect().IgnoreCaretPointSuggestion();
        }
      }
      trackMoveResult.FlushAndStopTracking();
      moveResult |= MoveNodeResult::HandledResult(
          EditorDOMPoint::AtEndOf(aLeftBlockElement));
      return std::move(moveResult);
    }

#ifdef DEBUG
    Result<bool, nsresult> firstLineHasContent =
        HTMLEditor::AutoMoveOneLineHandler::CanMoveOrDeleteSomethingInLine(
            EditorDOMPoint(&aRightBlockElement, 0u), aEditingHost);
#endif  // #ifdef DEBUG

    MoveNodeResult moveResult = MoveNodeResult::IgnoredResult(
        EditorDOMPoint::AtEndOf(aLeftBlockElement));
    // Nodes are dissimilar types.
    HTMLEditor::AutoMoveOneLineHandler lineMoverToEndOfLeftBlock(
        aLeftBlockElement);
    nsresult rv = lineMoverToEndOfLeftBlock.Prepare(
        aHTMLEditor, EditorDOMPoint(&aRightBlockElement, 0u), aEditingHost);
    if (NS_FAILED(rv)) {
      NS_WARNING("AutoMoveOneLineHandler::Prepare() failed");
      return Err(rv);
    }
    AutoTrackDOMMoveNodeResult trackMoveResult(aHTMLEditor.RangeUpdaterRef(),
                                               &moveResult);
    AutoTransactionsConserveSelection dontChangeMySelection(aHTMLEditor);
    Result<MoveNodeResult, nsresult> moveFirstLineResult =
        lineMoverToEndOfLeftBlock.Run(aHTMLEditor, aEditingHost);
    if (MOZ_UNLIKELY(moveFirstLineResult.isErr())) {
      NS_WARNING("AutoMoveOneLineHandler::Run() failed");
      return moveFirstLineResult.propagateErr();
    }

#ifdef DEBUG
    MOZ_ASSERT(!firstLineHasContent.isErr());
    if (firstLineHasContent.inspect()) {
      NS_ASSERTION(moveFirstLineResult.inspect().Handled(),
                   "Failed to consider whether moving or not something");
    } else {
      NS_ASSERTION(moveFirstLineResult.inspect().Ignored(),
                   "Failed to consider whether moving or not something");
    }
#endif  // #ifdef DEBUG

    trackMoveResult.FlushAndStopTracking();
    moveResult |= moveFirstLineResult.unwrap();
    return std::move(moveResult);
  }();
  if (MOZ_UNLIKELY(moveContentResult.isErr())) {
    return moveContentResult;
  }

  MoveNodeResult unwrappedMoveContentResult = moveContentResult.unwrap();

  trackStartOfRightText.FlushAndStopTracking();
  if (atStartOfRightText.IsInTextNode() &&
      atStartOfRightText.IsSetAndValidInComposedDoc() &&
      atStartOfRightText.IsMiddleOfContainer()) {
    AutoTrackDOMMoveNodeResult trackMoveContentResult(
        aHTMLEditor.RangeUpdaterRef(), &unwrappedMoveContentResult);
    Result<EditorDOMPoint, nsresult> startOfRightTextOrError =
        WhiteSpaceVisibilityKeeper::NormalizeWhiteSpacesAt(
            aHTMLEditor, atStartOfRightText.AsInText());
    if (MOZ_UNLIKELY(startOfRightTextOrError.isErr())) {
      NS_WARNING("WhiteSpaceVisibilityKeeper::NormalizeWhiteSpacesAt() failed");
      return startOfRightTextOrError.propagateErr();
    }
  }

  if (!invisibleBRElementAtEndOfLeftBlockElement ||
      !invisibleBRElementAtEndOfLeftBlockElement->IsInComposedDoc()) {
    unwrappedMoveContentResult.ForceToMarkAsHandled();
    return std::move(unwrappedMoveContentResult);
  }

  {
    AutoTrackDOMMoveNodeResult trackMoveContentResult(
        aHTMLEditor.RangeUpdaterRef(), &unwrappedMoveContentResult);
    AutoTransactionsConserveSelection dontChangeMySelection(aHTMLEditor);
    nsresult rv = aHTMLEditor.DeleteNodeWithTransaction(
        *invisibleBRElementAtEndOfLeftBlockElement);
    // XXX In other top level if blocks, the result of
    //     DeleteNodeWithTransaction() is ignored.  Why does only this result
    //     is respected?
    if (NS_FAILED(rv)) {
      NS_WARNING("EditorBase::DeleteNodeWithTransaction() failed");
      unwrappedMoveContentResult.IgnoreCaretPointSuggestion();
      return Err(rv);
    }
  }
  return std::move(unwrappedMoveContentResult);
}

// static
Result<EditorDOMPoint, nsresult>
WhiteSpaceVisibilityKeeper::NormalizeWhiteSpacesAt(
    HTMLEditor& aHTMLEditor, const EditorDOMPointInText& aPoint) {
  MOZ_ASSERT(StaticPrefs::editor_white_space_normalization_blink_compatible());
  MOZ_ASSERT(aPoint.IsSet());
  MOZ_ASSERT(!aPoint.IsEndOfContainer());

  if (!aPoint.IsCharCollapsibleASCIISpaceOrNBSP()) {
    return aPoint.To<EditorDOMPoint>();
  }

  const HTMLEditor::ReplaceWhiteSpacesData normalizedWhiteSpaces =
      aHTMLEditor.GetNormalizedStringAt(aPoint).GetMinimizedData(
          *aPoint.ContainerAs<Text>());
  if (!normalizedWhiteSpaces.ReplaceLength()) {
    return aPoint.To<EditorDOMPoint>();
  }

  const OwningNonNull<Text> textNode = *aPoint.ContainerAs<Text>();
  Result<InsertTextResult, nsresult> insertTextResultOrError =
      aHTMLEditor.ReplaceTextWithTransaction(textNode, normalizedWhiteSpaces);
  if (MOZ_UNLIKELY(insertTextResultOrError.isErr())) {
    NS_WARNING("HTMLEditor::ReplaceTextWithTransaction() failed");
    return insertTextResultOrError.propagateErr();
  }
  return insertTextResultOrError.unwrap().UnwrapCaretPoint();
}

// static
Result<EditorDOMPoint, nsresult>
WhiteSpaceVisibilityKeeper::NormalizeWhiteSpacesBefore(
    HTMLEditor& aHTMLEditor, const EditorDOMPoint& aPoint,
    NormalizeOptions aOptions) {
  MOZ_ASSERT(aPoint.IsSetAndValid());
  MOZ_ASSERT_IF(aPoint.IsInTextNode(), !aPoint.IsMiddleOfContainer());
  MOZ_ASSERT(
      !aOptions.contains(NormalizeOption::HandleOnlyFollowingWhiteSpaces));
  MOZ_ASSERT(StaticPrefs::editor_white_space_normalization_blink_compatible());

  const RefPtr<Element> colsetBlockElement =
      aPoint.IsInContentNode() ? HTMLEditUtils::GetInclusiveAncestorElement(
                                     *aPoint.ContainerAs<nsIContent>(),
                                     HTMLEditUtils::ClosestEditableBlockElement,
                                     BlockInlineCheck::UseComputedDisplayStyle)
                               : nullptr;
  EditorDOMPoint afterLastVisibleThing(aPoint);
  AutoTArray<OwningNonNull<nsIContent>, 32> unnecessaryContents;
  for (nsIContent* previousContent =
           aPoint.IsInTextNode() && aPoint.IsEndOfContainer()
               ? aPoint.ContainerAs<Text>()
               : HTMLEditUtils::GetPreviousLeafContentOrPreviousBlockElement(
                     aPoint,
                     {HTMLEditUtils::LeafNodeType::LeafNodeOrChildBlock},
                     BlockInlineCheck::UseComputedDisplayStyle,
                     colsetBlockElement);
       previousContent;
       previousContent =
           HTMLEditUtils::GetPreviousLeafContentOrPreviousBlockElement(
               EditorRawDOMPoint(previousContent),
               {HTMLEditUtils::LeafNodeType::LeafNodeOrChildBlock},
               BlockInlineCheck::UseComputedDisplayStyle, colsetBlockElement)) {
    if (!HTMLEditUtils::IsSimplyEditableNode(*previousContent)) {
      // XXX Assume non-editable nodes are visible.
      break;
    }
    const RefPtr<Text> precedingTextNode = Text::FromNode(previousContent);
    if (!precedingTextNode &&
        HTMLEditUtils::IsVisibleElementEvenIfLeafNode(*previousContent)) {
      afterLastVisibleThing.SetAfter(previousContent);
      break;
    }
    if (!precedingTextNode || !precedingTextNode->TextDataLength()) {
      // If it's an empty inline element like `<b></b>` or an empty `Text`,
      // delete it.
      nsIContent* emptyInlineContent =
          HTMLEditUtils::GetMostDistantAncestorEditableEmptyInlineElement(
              *previousContent, BlockInlineCheck::UseComputedDisplayStyle);
      if (!emptyInlineContent) {
        emptyInlineContent = previousContent;
      }
      unnecessaryContents.AppendElement(*emptyInlineContent);
      continue;
    }
    const auto atLastChar =
        EditorRawDOMPointInText::AtLastContentOf(*precedingTextNode);
    if (!atLastChar.IsCharCollapsibleASCIISpaceOrNBSP()) {
      afterLastVisibleThing.SetAfter(precedingTextNode);
      break;
    }
    if (aOptions.contains(
            NormalizeOption::StopIfPrecedingWhiteSpacesEndsWithNBP) &&
        atLastChar.IsCharNBSP()) {
      afterLastVisibleThing.SetAfter(precedingTextNode);
      break;
    }
    const HTMLEditor::ReplaceWhiteSpacesData replaceData =
        aHTMLEditor.GetNormalizedStringAt(atLastChar.AsInText())
            .GetMinimizedData(*precedingTextNode);
    if (!replaceData.ReplaceLength()) {
      afterLastVisibleThing.SetAfter(precedingTextNode);
      break;
    }
    // If the Text node has only invisible white-spaces, delete the node itself.
    if (replaceData.ReplaceLength() == precedingTextNode->TextDataLength() &&
        replaceData.mNormalizedString.IsEmpty()) {
      nsIContent* emptyInlineContent =
          HTMLEditUtils::GetMostDistantAncestorEditableEmptyInlineElement(
              *precedingTextNode, BlockInlineCheck::UseComputedDisplayStyle);
      if (!emptyInlineContent) {
        emptyInlineContent = precedingTextNode;
      }
      unnecessaryContents.AppendElement(*emptyInlineContent);
      continue;
    }
    Result<InsertTextResult, nsresult> replaceWhiteSpacesResultOrError =
        aHTMLEditor.ReplaceTextWithTransaction(*precedingTextNode, replaceData);
    if (MOZ_UNLIKELY(replaceWhiteSpacesResultOrError.isErr())) {
      NS_WARNING("HTMLEditor::ReplaceTextWithTransaction() failed");
      return replaceWhiteSpacesResultOrError.propagateErr();
    }
    InsertTextResult replaceWhiteSpacesResult =
        replaceWhiteSpacesResultOrError.unwrap();
    replaceWhiteSpacesResult.IgnoreCaretPointSuggestion();
    afterLastVisibleThing = replaceWhiteSpacesResult.EndOfInsertedTextRef();
  }

  AutoTrackDOMPoint trackAfterLastVisibleThing(aHTMLEditor.RangeUpdaterRef(),
                                               &afterLastVisibleThing);
  for (const auto& contentToDelete : unnecessaryContents) {
    if (MOZ_UNLIKELY(!contentToDelete->IsInComposedDoc())) {
      continue;
    }
    nsresult rv =
        aHTMLEditor.DeleteNodeWithTransaction(MOZ_KnownLive(contentToDelete));
    if (NS_FAILED(rv)) {
      NS_WARNING("EditorBase::DeleteNodeWithTransaction() failed");
      return Err(rv);
    }
  }
  trackAfterLastVisibleThing.FlushAndStopTracking();
  if (NS_WARN_IF(
          !afterLastVisibleThing.IsInContentNodeAndValidInComposedDoc())) {
    return Err(NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE);
  }
  return std::move(afterLastVisibleThing);
}

// static
Result<EditorDOMPoint, nsresult>
WhiteSpaceVisibilityKeeper::NormalizeWhiteSpacesAfter(
    HTMLEditor& aHTMLEditor, const EditorDOMPoint& aPoint,
    NormalizeOptions aOptions) {
  MOZ_ASSERT(aPoint.IsSetAndValid());
  MOZ_ASSERT_IF(aPoint.IsInTextNode(), !aPoint.IsMiddleOfContainer());
  MOZ_ASSERT(
      !aOptions.contains(NormalizeOption::HandleOnlyPrecedingWhiteSpaces));
  MOZ_ASSERT(StaticPrefs::editor_white_space_normalization_blink_compatible());

  const RefPtr<Element> colsetBlockElement =
      aPoint.IsInContentNode() ? HTMLEditUtils::GetInclusiveAncestorElement(
                                     *aPoint.ContainerAs<nsIContent>(),
                                     HTMLEditUtils::ClosestEditableBlockElement,
                                     BlockInlineCheck::UseComputedDisplayStyle)
                               : nullptr;
  EditorDOMPoint atFirstVisibleThing(aPoint);
  AutoTArray<OwningNonNull<nsIContent>, 32> unnecessaryContents;
  for (nsIContent* nextContent =
           aPoint.IsInTextNode() && aPoint.IsStartOfContainer()
               ? aPoint.ContainerAs<Text>()
               : HTMLEditUtils::GetNextLeafContentOrNextBlockElement(
                     aPoint,
                     {HTMLEditUtils::LeafNodeType::LeafNodeOrChildBlock},
                     BlockInlineCheck::UseComputedDisplayStyle,
                     colsetBlockElement);
       nextContent;
       nextContent = HTMLEditUtils::GetNextLeafContentOrNextBlockElement(
           EditorRawDOMPoint::After(*nextContent),
           {HTMLEditUtils::LeafNodeType::LeafNodeOrChildBlock},
           BlockInlineCheck::UseComputedDisplayStyle, colsetBlockElement)) {
    if (!HTMLEditUtils::IsSimplyEditableNode(*nextContent)) {
      // XXX Assume non-editable nodes are visible.
      break;
    }
    const RefPtr<Text> followingTextNode = Text::FromNode(nextContent);
    if (!followingTextNode &&
        HTMLEditUtils::IsVisibleElementEvenIfLeafNode(*nextContent)) {
      atFirstVisibleThing.Set(nextContent);
      break;
    }
    if (!followingTextNode || !followingTextNode->TextDataLength()) {
      // If it's an empty inline element like `<b></b>` or an empty `Text`,
      // delete it.
      nsIContent* emptyInlineContent =
          HTMLEditUtils::GetMostDistantAncestorEditableEmptyInlineElement(
              *nextContent, BlockInlineCheck::UseComputedDisplayStyle);
      if (!emptyInlineContent) {
        emptyInlineContent = nextContent;
      }
      unnecessaryContents.AppendElement(*emptyInlineContent);
      continue;
    }
    const auto atFirstChar = EditorRawDOMPointInText(followingTextNode, 0u);
    if (!atFirstChar.IsCharCollapsibleASCIISpaceOrNBSP()) {
      atFirstVisibleThing.Set(followingTextNode);
      break;
    }
    if (aOptions.contains(
            NormalizeOption::StopIfPrecedingWhiteSpacesEndsWithNBP) &&
        atFirstChar.IsCharNBSP()) {
      atFirstVisibleThing.Set(followingTextNode);
      break;
    }
    const HTMLEditor::ReplaceWhiteSpacesData replaceData =
        aHTMLEditor.GetNormalizedStringAt(atFirstChar.AsInText())
            .GetMinimizedData(*followingTextNode);
    if (!replaceData.ReplaceLength()) {
      atFirstVisibleThing.Set(followingTextNode);
      break;
    }
    // If the Text node has only invisible white-spaces, delete the node itself.
    if (replaceData.ReplaceLength() == followingTextNode->TextDataLength() &&
        replaceData.mNormalizedString.IsEmpty()) {
      nsIContent* emptyInlineContent =
          HTMLEditUtils::GetMostDistantAncestorEditableEmptyInlineElement(
              *followingTextNode, BlockInlineCheck::UseComputedDisplayStyle);
      if (!emptyInlineContent) {
        emptyInlineContent = followingTextNode;
      }
      unnecessaryContents.AppendElement(*emptyInlineContent);
      continue;
    }
    Result<InsertTextResult, nsresult> replaceWhiteSpacesResultOrError =
        aHTMLEditor.ReplaceTextWithTransaction(*followingTextNode, replaceData);
    if (MOZ_UNLIKELY(replaceWhiteSpacesResultOrError.isErr())) {
      NS_WARNING("HTMLEditor::ReplaceTextWithTransaction() failed");
      return replaceWhiteSpacesResultOrError.propagateErr();
    }
    replaceWhiteSpacesResultOrError.unwrap().IgnoreCaretPointSuggestion();
    atFirstVisibleThing.Set(followingTextNode, 0u);
    break;
  }

  AutoTrackDOMPoint trackAtFirstVisibleThing(aHTMLEditor.RangeUpdaterRef(),
                                             &atFirstVisibleThing);
  for (const auto& contentToDelete : unnecessaryContents) {
    if (MOZ_UNLIKELY(!contentToDelete->IsInComposedDoc())) {
      continue;
    }
    nsresult rv =
        aHTMLEditor.DeleteNodeWithTransaction(MOZ_KnownLive(contentToDelete));
    if (NS_FAILED(rv)) {
      NS_WARNING("EditorBase::DeleteNodeWithTransaction() failed");
      return Err(rv);
    }
  }
  trackAtFirstVisibleThing.FlushAndStopTracking();
  if (NS_WARN_IF(!atFirstVisibleThing.IsInContentNodeAndValidInComposedDoc())) {
    return Err(NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE);
  }
  return std::move(atFirstVisibleThing);
}

// static
Result<EditorDOMPoint, nsresult>
WhiteSpaceVisibilityKeeper::NormalizeWhiteSpacesToSplitTextNodeAt(
    HTMLEditor& aHTMLEditor, const EditorDOMPointInText& aPointToSplit,
    NormalizeOptions aOptions) {
  MOZ_ASSERT(aPointToSplit.IsSetAndValid());
  MOZ_ASSERT(StaticPrefs::editor_white_space_normalization_blink_compatible());

  if (EditorUtils::IsWhiteSpacePreformatted(
          *aPointToSplit.ContainerAs<Text>())) {
    return aPointToSplit.To<EditorDOMPoint>();
  }

  const OwningNonNull<Text> textNode = *aPointToSplit.ContainerAs<Text>();
  if (!textNode->TextDataLength()) {
    // Delete if it's an empty `Text` node and removable.
    if (!HTMLEditUtils::IsRemovableNode(*textNode)) {
      // It's logically odd to call this for non-editable `Text`, but it may
      // happen if surrounding white-space sequence contains empty non-editable
      // `Text`.  In that case, the caller needs to normalize its preceding
      // `Text` nodes too.
      return EditorDOMPoint();
    }
    const nsCOMPtr<nsINode> parentNode = textNode->GetParentNode();
    const nsCOMPtr<nsIContent> nextSibling = textNode->GetNextSibling();
    nsresult rv = aHTMLEditor.DeleteNodeWithTransaction(textNode);
    if (NS_FAILED(rv)) {
      NS_WARNING("EditorBase::DeleteNodeWithTransaction() failed");
      return Err(rv);
    }
    if (NS_WARN_IF(nextSibling && nextSibling->GetParentNode() != parentNode)) {
      return Err(NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE);
    }
    return EditorDOMPoint(nextSibling);
  }

  const HTMLEditor::ReplaceWhiteSpacesData replacePrecedingWhiteSpacesData =
      aPointToSplit.IsStartOfContainer() ||
              aOptions.contains(
                  NormalizeOption::HandleOnlyFollowingWhiteSpaces) ||
              (aOptions.contains(
                   NormalizeOption::StopIfPrecedingWhiteSpacesEndsWithNBP) &&
               aPointToSplit.IsPreviousCharNBSP())
          ? HTMLEditor::ReplaceWhiteSpacesData()
          : aHTMLEditor.GetPrecedingNormalizedStringToSplitAt(aPointToSplit);
  const HTMLEditor::ReplaceWhiteSpacesData replaceFollowingWhiteSpaceData =
      aPointToSplit.IsEndOfContainer() ||
              aOptions.contains(
                  NormalizeOption::HandleOnlyPrecedingWhiteSpaces) ||
              (aOptions.contains(
                   NormalizeOption::StopIfFollowingWhiteSpacesStartsWithNBSP) &&
               aPointToSplit.IsCharNBSP())
          ? HTMLEditor::ReplaceWhiteSpacesData()
          : aHTMLEditor.GetFollowingNormalizedStringToSplitAt(aPointToSplit);
  const HTMLEditor::ReplaceWhiteSpacesData replaceWhiteSpacesData =
      (replacePrecedingWhiteSpacesData + replaceFollowingWhiteSpaceData)
          .GetMinimizedData(*textNode);
  if (!replaceWhiteSpacesData.ReplaceLength()) {
    return aPointToSplit.To<EditorDOMPoint>();
  }
  if (replaceWhiteSpacesData.mNormalizedString.IsEmpty() &&
      replaceWhiteSpacesData.ReplaceLength() == textNode->TextDataLength()) {
    // If there is only invisible white-spaces, mNormalizedString is empty
    // string but replace length is same the the `Text` length. In this case, we
    // should delete the `Text` to avoid empty `Text` to stay in the DOM tree.
    const nsCOMPtr<nsINode> parentNode = textNode->GetParentNode();
    const nsCOMPtr<nsIContent> nextSibling = textNode->GetNextSibling();
    nsresult rv = aHTMLEditor.DeleteNodeWithTransaction(textNode);
    if (NS_FAILED(rv)) {
      NS_WARNING("EditorBase::DeleteNodeWithTransaction() failed");
      return Err(rv);
    }
    if (NS_WARN_IF(nextSibling && nextSibling->GetParentNode() != parentNode)) {
      return Err(NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE);
    }
    return EditorDOMPoint(nextSibling);
  }
  Result<InsertTextResult, nsresult> replaceWhiteSpacesResultOrError =
      aHTMLEditor.ReplaceTextWithTransaction(textNode, replaceWhiteSpacesData);
  if (MOZ_UNLIKELY(replaceWhiteSpacesResultOrError.isErr())) {
    NS_WARNING("HTMLEditor::ReplaceTextWithTransaction() failed");
    return replaceWhiteSpacesResultOrError.propagateErr();
  }
  replaceWhiteSpacesResultOrError.unwrap().IgnoreCaretPointSuggestion();
  const uint32_t offsetToSplit =
      aPointToSplit.Offset() - replacePrecedingWhiteSpacesData.ReplaceLength() +
      replacePrecedingWhiteSpacesData.mNormalizedString.Length();
  if (NS_WARN_IF(textNode->TextDataLength() < offsetToSplit)) {
    return Err(NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE);
  }
  return EditorDOMPoint(textNode, offsetToSplit);
}

// static
Result<EditorDOMPoint, nsresult>
WhiteSpaceVisibilityKeeper::NormalizeWhiteSpacesToSplitAt(
    HTMLEditor& aHTMLEditor, const EditorDOMPoint& aPointToSplit,
    NormalizeOptions aOptions) {
  MOZ_ASSERT(aPointToSplit.IsSet());
  MOZ_ASSERT(StaticPrefs::editor_white_space_normalization_blink_compatible());

  // If the insertion point is not in composed doc, we're probably initializing
  // an element which will be inserted.  In such case, the caller should own the
  // responsibility for normalizing the white-spaces.
  if (!aPointToSplit.IsInComposedDoc()) {
    return aPointToSplit;
  }

  EditorDOMPoint pointToSplit(aPointToSplit);
  {
    AutoTrackDOMPoint trackPointToSplit(aHTMLEditor.RangeUpdaterRef(),
                                        &pointToSplit);
    Result<EditorDOMPoint, nsresult> pointToSplitOrError =
        WhiteSpaceVisibilityKeeper::EnsureNoInvisibleWhiteSpaces(aHTMLEditor,
                                                                 pointToSplit);
    if (MOZ_UNLIKELY(pointToSplitOrError.isErr())) {
      NS_WARNING(
          "WhiteSpaceVisibilityKeeper::EnsureNoInvisibleWhiteSpaces() failed");
      return pointToSplitOrError.propagateErr();
    }
  }

  if (NS_WARN_IF(!pointToSplit.IsInContentNode())) {
    return Err(NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE);
  }

  if (pointToSplit.IsInTextNode()) {
    Result<EditorDOMPoint, nsresult> pointToSplitOrError =
        WhiteSpaceVisibilityKeeper::NormalizeWhiteSpacesToSplitTextNodeAt(
            aHTMLEditor, pointToSplit.AsInText(), aOptions);
    if (MOZ_UNLIKELY(pointToSplitOrError.isErr())) {
      NS_WARNING(
          "WhiteSpaceVisibilityKeeper::NormalizeWhiteSpacesToSplitTextNodeAt() "
          "failed");
      return pointToSplitOrError.propagateErr();
    }
    pointToSplit = pointToSplitOrError.unwrap().To<EditorDOMPoint>();
    if (NS_WARN_IF(!pointToSplit.IsInContentNode())) {
      return Err(NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE);
    }
    // If we normalize white-spaces in middle of the `Text`, we don't need to
    // touch surrounding `Text` nodes.
    if (pointToSplit.IsMiddleOfContainer()) {
      return pointToSplit;
    }
  }

  // Preceding and/or following white-space sequence may be across multiple
  // `Text` nodes.  Then, they may become unexpectedly visible without
  // normalizing the white-spaces.  Therefore, we need to list up all possible
  // `Text` nodes first. Then, normalize them unless the `Text` is not
  const RefPtr<Element> closestBlockElement =
      HTMLEditUtils::GetInclusiveAncestorElement(
          *pointToSplit.ContainerAs<nsIContent>(),
          HTMLEditUtils::ClosestBlockElement,
          BlockInlineCheck::UseComputedDisplayStyle);
  AutoTArray<OwningNonNull<Text>, 3> precedingTextNodes, followingTextNodes;
  if (!pointToSplit.IsInTextNode() || pointToSplit.IsStartOfContainer()) {
    for (nsCOMPtr<nsIContent> previousContent =
             HTMLEditUtils::GetPreviousLeafContentOrPreviousBlockElement(
                 pointToSplit, {LeafNodeType::LeafNodeOrChildBlock},
                 BlockInlineCheck::UseComputedDisplayStyle,
                 closestBlockElement);
         previousContent;
         previousContent =
             HTMLEditUtils::GetPreviousLeafContentOrPreviousBlockElement(
                 *previousContent, {LeafNodeType::LeafNodeOrChildBlock},
                 BlockInlineCheck::UseComputedDisplayStyle,
                 closestBlockElement)) {
      if (auto* const textNode = Text::FromNode(previousContent)) {
        if (!HTMLEditUtils::IsSimplyEditableNode(*textNode) &&
            textNode->TextDataLength()) {
          break;
        }
        if (aOptions.contains(
                NormalizeOption::StopIfPrecedingWhiteSpacesEndsWithNBP) &&
            textNode->TextFragment().SafeLastChar() == HTMLEditUtils::kNBSP) {
          break;
        }
        precedingTextNodes.AppendElement(*textNode);
        if (textNode->TextIsOnlyWhitespace()) {
          // white-space only `Text` will be removed, so, we need to check
          // preceding one too.
          continue;
        }
        break;
      }
      if (auto* const element = Element::FromNode(previousContent)) {
        if (HTMLEditUtils::IsBlockElement(
                *element, BlockInlineCheck::UseComputedDisplayStyle) ||
            HTMLEditUtils::IsNonEditableReplacedContent(*element)) {
          break;
        }
        // Ignore invisible inline elements
      }
    }
  }
  if (!pointToSplit.IsInTextNode() || pointToSplit.IsEndOfContainer()) {
    for (nsCOMPtr<nsIContent> nextContent =
             HTMLEditUtils::GetNextLeafContentOrNextBlockElement(
                 pointToSplit, {LeafNodeType::LeafNodeOrChildBlock},
                 BlockInlineCheck::UseComputedDisplayStyle,
                 closestBlockElement);
         nextContent;
         nextContent = HTMLEditUtils::GetNextLeafContentOrNextBlockElement(
             *nextContent, {LeafNodeType::LeafNodeOrChildBlock},
             BlockInlineCheck::UseComputedDisplayStyle, closestBlockElement)) {
      if (auto* const textNode = Text::FromNode(nextContent)) {
        if (!HTMLEditUtils::IsSimplyEditableNode(*textNode) &&
            textNode->TextDataLength()) {
          break;
        }
        if (aOptions.contains(
                NormalizeOption::StopIfFollowingWhiteSpacesStartsWithNBSP) &&
            textNode->TextFragment().SafeFirstChar() == HTMLEditUtils::kNBSP) {
          break;
        }
        followingTextNodes.AppendElement(*textNode);
        if (textNode->TextIsOnlyWhitespace() &&
            EditorUtils::IsWhiteSpacePreformatted(*textNode)) {
          // white-space only `Text` will be removed, so, we need to check next
          // one too.
          continue;
        }
        break;
      }
      if (auto* const element = Element::FromNode(nextContent)) {
        if (HTMLEditUtils::IsBlockElement(
                *element, BlockInlineCheck::UseComputedDisplayStyle) ||
            HTMLEditUtils::IsNonEditableReplacedContent(*element)) {
          break;
        }
        // Ignore invisible inline elements
      }
    }
  }
  AutoTrackDOMPoint trackPointToSplit(aHTMLEditor.RangeUpdaterRef(),
                                      &pointToSplit);
  for (const auto& textNode : precedingTextNodes) {
    Result<EditorDOMPoint, nsresult> normalizeWhiteSpacesResultOrError =
        WhiteSpaceVisibilityKeeper::NormalizeWhiteSpacesToSplitTextNodeAt(
            aHTMLEditor, EditorDOMPointInText::AtEndOf(textNode), aOptions);
    if (MOZ_UNLIKELY(normalizeWhiteSpacesResultOrError.isErr())) {
      NS_WARNING(
          "WhiteSpaceVisibilityKeeper::NormalizeWhiteSpacesToSplitTextNodeAt() "
          "failed");
      return normalizeWhiteSpacesResultOrError.propagateErr();
    }
    if (normalizeWhiteSpacesResultOrError.inspect().IsInTextNode() &&
        !normalizeWhiteSpacesResultOrError.inspect().IsStartOfContainer()) {
      // The white-space sequence started from middle of this node, so, we need
      // to do this for the preceding nodes.
      break;
    }
  }
  for (const auto& textNode : followingTextNodes) {
    Result<EditorDOMPoint, nsresult> normalizeWhiteSpacesResultOrError =
        WhiteSpaceVisibilityKeeper::NormalizeWhiteSpacesToSplitTextNodeAt(
            aHTMLEditor, EditorDOMPointInText(textNode, 0u), aOptions);
    if (MOZ_UNLIKELY(normalizeWhiteSpacesResultOrError.isErr())) {
      NS_WARNING(
          "WhiteSpaceVisibilityKeeper::NormalizeWhiteSpacesToSplitTextNodeAt() "
          "failed");
      return normalizeWhiteSpacesResultOrError.propagateErr();
    }
    if (normalizeWhiteSpacesResultOrError.inspect().IsInTextNode() &&
        !normalizeWhiteSpacesResultOrError.inspect().IsEndOfContainer()) {
      // The white-space sequence ended in middle of this node, so, we need
      // to do this for the following nodes.
      break;
    }
  }
  trackPointToSplit.FlushAndStopTracking();
  if (NS_WARN_IF(!pointToSplit.IsInContentNode())) {
    return Err(NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE);
  }
  return std::move(pointToSplit);
}

Result<EditorDOMRange, nsresult>
WhiteSpaceVisibilityKeeper::NormalizeSurroundingWhiteSpacesToJoin(
    HTMLEditor& aHTMLEditor, const EditorDOMRange& aRangeToDelete) {
  MOZ_ASSERT(StaticPrefs::editor_white_space_normalization_blink_compatible());
  MOZ_ASSERT(!aRangeToDelete.Collapsed());

  // Special case if the range for deleting text in same `Text`.  In the case,
  // we need to normalize the white-space sequence which may be joined after
  // deletion.
  if (aRangeToDelete.StartRef().IsInTextNode() &&
      aRangeToDelete.InSameContainer()) {
    const RefPtr<Text> textNode = aRangeToDelete.StartRef().ContainerAs<Text>();
    Result<EditorDOMRange, nsresult> rangeToDeleteOrError =
        WhiteSpaceVisibilityKeeper::
            NormalizeSurroundingWhiteSpacesToDeleteCharacters(
                aHTMLEditor, *textNode, aRangeToDelete.StartRef().Offset(),
                aRangeToDelete.EndRef().Offset() -
                    aRangeToDelete.StartRef().Offset());
    NS_WARNING_ASSERTION(
        rangeToDeleteOrError.isOk(),
        "WhiteSpaceVisibilityKeeper::"
        "NormalizeSurroundingWhiteSpacesToDeleteCharacters() failed");
    return rangeToDeleteOrError;
  }

  EditorDOMRange rangeToDelete(aRangeToDelete);
  // First, delete all invisible white-spaces around the end boundary.
  // The end boundary may be middle of invisible white-spaces.  If so,
  // NormalizeWhiteSpacesToSplitTextNodeAt() won't work well for this.
  {
    AutoTrackDOMRange trackRangeToDelete(aHTMLEditor.RangeUpdaterRef(),
                                         &rangeToDelete);
    const WSScanResult nextThing =
        WSRunScanner::ScanInclusiveNextVisibleNodeOrBlockBoundary(
            Scan::All, rangeToDelete.StartRef(),
            BlockInlineCheck::UseComputedDisplayOutsideStyle);
    if (nextThing.ReachedLineBoundary()) {
      nsresult rv =
          WhiteSpaceVisibilityKeeper::EnsureNoInvisibleWhiteSpacesBefore(
              aHTMLEditor, nextThing.PointAtReachedContent<EditorDOMPoint>());
      if (NS_FAILED(rv)) {
        NS_WARNING(
            "WhiteSpaceVisibilityKeeper::EnsureNoInvisibleWhiteSpacesBefore() "
            "failed");
        return Err(rv);
      }
    } else {
      Result<EditorDOMPoint, nsresult>
          deleteInvisibleLeadingWhiteSpaceResultOrError =
              WhiteSpaceVisibilityKeeper::EnsureNoInvisibleWhiteSpaces(
                  aHTMLEditor, rangeToDelete.EndRef());
      if (MOZ_UNLIKELY(deleteInvisibleLeadingWhiteSpaceResultOrError.isErr())) {
        NS_WARNING(
            "WhiteSpaceVisibilityKeeper::EnsureNoInvisibleWhiteSpaces() "
            "failed");
        return deleteInvisibleLeadingWhiteSpaceResultOrError.propagateErr();
      }
    }
    trackRangeToDelete.FlushAndStopTracking();
    if (NS_WARN_IF(!rangeToDelete.IsPositionedAndValidInComposedDoc())) {
      return Err(NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE);
    }
  }

  // Then, normalize white-spaces after the end boundary.
  if (rangeToDelete.EndRef().IsInTextNode() &&
      rangeToDelete.EndRef().IsMiddleOfContainer()) {
    Result<EditorDOMPoint, nsresult> pointToSplitOrError =
        WhiteSpaceVisibilityKeeper::NormalizeWhiteSpacesToSplitTextNodeAt(
            aHTMLEditor, rangeToDelete.EndRef().AsInText(),
            {NormalizeOption::HandleOnlyFollowingWhiteSpaces});
    if (MOZ_UNLIKELY(pointToSplitOrError.isErr())) {
      NS_WARNING(
          "WhiteSpaceVisibilityKeeper::NormalizeWhiteSpacesToSplitTextNodeAt("
          ") failed");
      return pointToSplitOrError.propagateErr();
    }
    EditorDOMPoint pointToSplit = pointToSplitOrError.unwrap();
    if (pointToSplit.IsSet() && pointToSplit != rangeToDelete.EndRef()) {
      MOZ_ASSERT(rangeToDelete.StartRef().EqualsOrIsBefore(pointToSplit));
      rangeToDelete.SetEnd(std::move(pointToSplit));
    }
  } else {
    AutoTrackDOMRange trackRangeToDelete(aHTMLEditor.RangeUpdaterRef(),
                                         &rangeToDelete);
    Result<EditorDOMPoint, nsresult> atFirstVisibleThingOrError =
        WhiteSpaceVisibilityKeeper::NormalizeWhiteSpacesAfter(
            aHTMLEditor, rangeToDelete.EndRef(), {});
    if (MOZ_UNLIKELY(atFirstVisibleThingOrError.isErr())) {
      NS_WARNING(
          "WhiteSpaceVisibilityKeeper::NormalizeWhiteSpacesAfter() failed");
      return atFirstVisibleThingOrError.propagateErr();
    }
    trackRangeToDelete.FlushAndStopTracking();
    if (NS_WARN_IF(!rangeToDelete.IsPositionedAndValidInComposedDoc())) {
      return Err(NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE);
    }
  }

  // If cleaning up the white-spaces around the end boundary made the range
  // collapsed, the range was in invisible white-spaces.  So, in the case, we
  // don't need to do nothing.
  if (MOZ_UNLIKELY(rangeToDelete.Collapsed())) {
    return rangeToDelete;
  }

  // Next, delete the invisible white-spaces around the start boundary.
  {
    AutoTrackDOMRange trackRangeToDelete(aHTMLEditor.RangeUpdaterRef(),
                                         &rangeToDelete);
    Result<EditorDOMPoint, nsresult>
        deleteInvisibleTrailingWhiteSpaceResultOrError =
            WhiteSpaceVisibilityKeeper::EnsureNoInvisibleWhiteSpaces(
                aHTMLEditor, rangeToDelete.StartRef());
    if (MOZ_UNLIKELY(deleteInvisibleTrailingWhiteSpaceResultOrError.isErr())) {
      NS_WARNING(
          "WhiteSpaceVisibilityKeeper::EnsureNoInvisibleWhiteSpaces() failed");
      return deleteInvisibleTrailingWhiteSpaceResultOrError.propagateErr();
    }
    trackRangeToDelete.FlushAndStopTracking();
    if (NS_WARN_IF(!rangeToDelete.IsPositionedAndValidInComposedDoc())) {
      return Err(NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE);
    }
  }

  // Finally, normalize white-spaces before the start boundary only when
  // the start boundary is middle of a `Text` node.  This is compatible with
  // the other browsers.
  if (rangeToDelete.StartRef().IsInTextNode() &&
      rangeToDelete.StartRef().IsMiddleOfContainer()) {
    AutoTrackDOMRange trackRangeToDelete(aHTMLEditor.RangeUpdaterRef(),
                                         &rangeToDelete);
    Result<EditorDOMPoint, nsresult> afterLastVisibleThingOrError =
        WhiteSpaceVisibilityKeeper::NormalizeWhiteSpacesToSplitTextNodeAt(
            aHTMLEditor, rangeToDelete.StartRef().AsInText(),
            {NormalizeOption::HandleOnlyPrecedingWhiteSpaces});
    if (MOZ_UNLIKELY(afterLastVisibleThingOrError.isErr())) {
      NS_WARNING(
          "WhiteSpaceVisibilityKeeper::NormalizeWhiteSpacesToSplitTextNodeAt() "
          "failed");
      return afterLastVisibleThingOrError.propagateErr();
    }
    trackRangeToDelete.FlushAndStopTracking();
    EditorDOMPoint pointToSplit = afterLastVisibleThingOrError.unwrap();
    if (pointToSplit.IsSet() && pointToSplit != rangeToDelete.StartRef()) {
      MOZ_ASSERT(pointToSplit.EqualsOrIsBefore(rangeToDelete.EndRef()));
      rangeToDelete.SetStart(std::move(pointToSplit));
    }
  }
  return rangeToDelete;
}

Result<EditorDOMRange, nsresult>
WhiteSpaceVisibilityKeeper::NormalizeSurroundingWhiteSpacesToDeleteCharacters(
    HTMLEditor& aHTMLEditor, Text& aTextNode, uint32_t aOffset,
    uint32_t aLength) {
  MOZ_ASSERT(StaticPrefs::editor_white_space_normalization_blink_compatible());
  MOZ_ASSERT(aOffset <= aTextNode.TextDataLength());
  MOZ_ASSERT(aOffset + aLength <= aTextNode.TextDataLength());

  const HTMLEditor::ReplaceWhiteSpacesData normalizedWhiteSpacesData =
      aHTMLEditor.GetSurroundingNormalizedStringToDelete(aTextNode, aOffset,
                                                         aLength);
  EditorDOMRange rangeToDelete(EditorDOMPoint(&aTextNode, aOffset),
                               EditorDOMPoint(&aTextNode, aOffset + aLength));
  if (!normalizedWhiteSpacesData.ReplaceLength()) {
    return rangeToDelete;
  }
  // mNewOffsetAfterReplace is set to aOffset after applying replacing the
  // range.
  MOZ_ASSERT(normalizedWhiteSpacesData.mNewOffsetAfterReplace != UINT32_MAX);
  MOZ_ASSERT(normalizedWhiteSpacesData.mNewOffsetAfterReplace >=
             normalizedWhiteSpacesData.mReplaceStartOffset);
  MOZ_ASSERT(normalizedWhiteSpacesData.mNewOffsetAfterReplace <=
             normalizedWhiteSpacesData.mReplaceEndOffset);
#ifdef DEBUG
  {
    const HTMLEditor::ReplaceWhiteSpacesData
        normalizedPrecedingWhiteSpacesData =
            normalizedWhiteSpacesData.PreviousDataOfNewOffset(aOffset);
    const HTMLEditor::ReplaceWhiteSpacesData
        normalizedFollowingWhiteSpacesData =
            normalizedWhiteSpacesData.NextDataOfNewOffset(aOffset + aLength);
    MOZ_ASSERT(normalizedPrecedingWhiteSpacesData.ReplaceLength() + aLength +
                   normalizedFollowingWhiteSpacesData.ReplaceLength() ==
               normalizedWhiteSpacesData.ReplaceLength());
    MOZ_ASSERT(
        normalizedPrecedingWhiteSpacesData.mNormalizedString.Length() +
            normalizedFollowingWhiteSpacesData.mNormalizedString.Length() ==
        normalizedWhiteSpacesData.mNormalizedString.Length());
  }
#endif
  const HTMLEditor::ReplaceWhiteSpacesData normalizedPrecedingWhiteSpacesData =
      normalizedWhiteSpacesData.PreviousDataOfNewOffset(aOffset)
          .GetMinimizedData(aTextNode);
  const HTMLEditor::ReplaceWhiteSpacesData normalizedFollowingWhiteSpacesData =
      normalizedWhiteSpacesData.NextDataOfNewOffset(aOffset + aLength)
          .GetMinimizedData(aTextNode);
  if (normalizedFollowingWhiteSpacesData.ReplaceLength()) {
    AutoTrackDOMRange trackRangeToDelete(aHTMLEditor.RangeUpdaterRef(),
                                         &rangeToDelete);
    Result<InsertTextResult, nsresult>
        replaceFollowingWhiteSpacesResultOrError =
            aHTMLEditor.ReplaceTextWithTransaction(
                aTextNode, normalizedFollowingWhiteSpacesData);
    if (MOZ_UNLIKELY(replaceFollowingWhiteSpacesResultOrError.isErr())) {
      NS_WARNING("HTMLEditor::ReplaceTextWithTransaction() failed");
      return replaceFollowingWhiteSpacesResultOrError.propagateErr();
    }
    trackRangeToDelete.FlushAndStopTracking();
    if (NS_WARN_IF(!rangeToDelete.IsPositioned())) {
      return Err(NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE);
    }
  }
  if (normalizedPrecedingWhiteSpacesData.ReplaceLength()) {
    AutoTrackDOMRange trackRangeToDelete(aHTMLEditor.RangeUpdaterRef(),
                                         &rangeToDelete);
    Result<InsertTextResult, nsresult>
        replacePrecedingWhiteSpacesResultOrError =
            aHTMLEditor.ReplaceTextWithTransaction(
                aTextNode, normalizedPrecedingWhiteSpacesData);
    if (MOZ_UNLIKELY(replacePrecedingWhiteSpacesResultOrError.isErr())) {
      NS_WARNING("HTMLEditor::ReplaceTextWithTransaction() failed");
      return replacePrecedingWhiteSpacesResultOrError.propagateErr();
    }
    trackRangeToDelete.FlushAndStopTracking();
    if (NS_WARN_IF(!rangeToDelete.IsPositioned())) {
      return Err(NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE);
    }
  }
  return std::move(rangeToDelete);
}

// static
Result<CreateLineBreakResult, nsresult>
WhiteSpaceVisibilityKeeper::InsertLineBreak(
    LineBreakType aLineBreakType, HTMLEditor& aHTMLEditor,
    const EditorDOMPoint& aPointToInsert) {
  if (MOZ_UNLIKELY(NS_WARN_IF(!aPointToInsert.IsSet()))) {
    return Err(NS_ERROR_INVALID_ARG);
  }

  EditorDOMPoint pointToInsert(aPointToInsert);
  // TODO: Delete this block once we ship the new normalizer.
  if (!StaticPrefs::editor_white_space_normalization_blink_compatible()) {
    // MOOSE: for now, we always assume non-PRE formatting.  Fix this later.
    // meanwhile, the pre case is handled in HandleInsertText() in
    // HTMLEditSubActionHandler.cpp

    const TextFragmentData textFragmentDataAtInsertionPoint(
        Scan::EditableNodes, aPointToInsert,
        BlockInlineCheck::UseComputedDisplayStyle);
    if (NS_WARN_IF(!textFragmentDataAtInsertionPoint.IsInitialized())) {
      return Err(NS_ERROR_FAILURE);
    }
    EditorDOMRange invisibleLeadingWhiteSpaceRangeOfNewLine =
        textFragmentDataAtInsertionPoint
            .GetNewInvisibleLeadingWhiteSpaceRangeIfSplittingAt(aPointToInsert);
    EditorDOMRange invisibleTrailingWhiteSpaceRangeOfCurrentLine =
        textFragmentDataAtInsertionPoint
            .GetNewInvisibleTrailingWhiteSpaceRangeIfSplittingAt(
                aPointToInsert);
    const Maybe<const VisibleWhiteSpacesData> visibleWhiteSpaces =
        !invisibleLeadingWhiteSpaceRangeOfNewLine.IsPositioned() ||
                !invisibleTrailingWhiteSpaceRangeOfCurrentLine.IsPositioned()
            ? Some(textFragmentDataAtInsertionPoint.VisibleWhiteSpacesDataRef())
            : Nothing();
    const PointPosition pointPositionWithVisibleWhiteSpaces =
        visibleWhiteSpaces.isSome() && visibleWhiteSpaces.ref().IsInitialized()
            ? visibleWhiteSpaces.ref().ComparePoint(aPointToInsert)
            : PointPosition::NotInSameDOMTree;

    EditorDOMPoint atNBSPReplaceableWithSP;
    if (!invisibleLeadingWhiteSpaceRangeOfNewLine.IsPositioned() &&
        (pointPositionWithVisibleWhiteSpaces ==
             PointPosition::MiddleOfFragment ||
         pointPositionWithVisibleWhiteSpaces == PointPosition::EndOfFragment)) {
      atNBSPReplaceableWithSP =
          textFragmentDataAtInsertionPoint
              .GetPreviousNBSPPointIfNeedToReplaceWithASCIIWhiteSpace(
                  pointToInsert)
              .To<EditorDOMPoint>();
    }

    {
      if (invisibleTrailingWhiteSpaceRangeOfCurrentLine.IsPositioned()) {
        if (!invisibleTrailingWhiteSpaceRangeOfCurrentLine.Collapsed()) {
          // XXX Why don't we remove all of the invisible white-spaces?
          MOZ_ASSERT(invisibleTrailingWhiteSpaceRangeOfCurrentLine.StartRef() ==
                     pointToInsert);
          AutoTrackDOMPoint trackPointToInsert(aHTMLEditor.RangeUpdaterRef(),
                                               &pointToInsert);
          AutoTrackDOMPoint trackEndOfLineNBSP(aHTMLEditor.RangeUpdaterRef(),
                                               &atNBSPReplaceableWithSP);
          AutoTrackDOMRange trackLeadingWhiteSpaceRange(
              aHTMLEditor.RangeUpdaterRef(),
              &invisibleLeadingWhiteSpaceRangeOfNewLine);
          Result<CaretPoint, nsresult> caretPointOrError =
              aHTMLEditor.DeleteTextAndTextNodesWithTransaction(
                  invisibleTrailingWhiteSpaceRangeOfCurrentLine.StartRef(),
                  invisibleTrailingWhiteSpaceRangeOfCurrentLine.EndRef(),
                  HTMLEditor::TreatEmptyTextNodes::
                      KeepIfContainerOfRangeBoundaries);
          if (MOZ_UNLIKELY(caretPointOrError.isErr())) {
            NS_WARNING(
                "HTMLEditor::DeleteTextAndTextNodesWithTransaction() failed");
            return caretPointOrError.propagateErr();
          }
          nsresult rv = caretPointOrError.unwrap().SuggestCaretPointTo(
              aHTMLEditor, {SuggestCaret::OnlyIfHasSuggestion,
                            SuggestCaret::OnlyIfTransactionsAllowedToDoIt,
                            SuggestCaret::AndIgnoreTrivialError});
          if (NS_FAILED(rv)) {
            NS_WARNING("CaretPoint::SuggestCaretPointTo() failed");
            return Err(rv);
          }
          NS_WARNING_ASSERTION(
              rv != NS_SUCCESS_EDITOR_BUT_IGNORED_TRIVIAL_ERROR,
              "CaretPoint::SuggestCaretPointTo() failed, but ignored");
          // Don't refer the following variables anymore unless tracking the
          // change.
          invisibleTrailingWhiteSpaceRangeOfCurrentLine.Clear();
        }
      }
      // If new line will start with visible white-spaces, it needs to be start
      // with an NBSP.
      else if (pointPositionWithVisibleWhiteSpaces ==
                   PointPosition::StartOfFragment ||
               pointPositionWithVisibleWhiteSpaces ==
                   PointPosition::MiddleOfFragment) {
        const auto atNextCharOfInsertionPoint =
            textFragmentDataAtInsertionPoint
                .GetInclusiveNextCharPoint<EditorDOMPointInText>(
                    pointToInsert, IgnoreNonEditableNodes::Yes);
        if (atNextCharOfInsertionPoint.IsSet() &&
            !atNextCharOfInsertionPoint.IsEndOfContainer() &&
            atNextCharOfInsertionPoint.IsCharCollapsibleASCIISpace()) {
          const auto atPreviousCharOfNextCharOfInsertionPoint =
              textFragmentDataAtInsertionPoint
                  .GetPreviousCharPoint<EditorDOMPointInText>(
                      atNextCharOfInsertionPoint, IgnoreNonEditableNodes::Yes);
          if (!atPreviousCharOfNextCharOfInsertionPoint.IsSet() ||
              atPreviousCharOfNextCharOfInsertionPoint.IsEndOfContainer() ||
              !atPreviousCharOfNextCharOfInsertionPoint.IsCharASCIISpace()) {
            AutoTrackDOMPoint trackPointToInsert(aHTMLEditor.RangeUpdaterRef(),
                                                 &pointToInsert);
            AutoTrackDOMPoint trackEndOfLineNBSP(aHTMLEditor.RangeUpdaterRef(),
                                                 &atNBSPReplaceableWithSP);
            AutoTrackDOMRange trackLeadingWhiteSpaceRange(
                aHTMLEditor.RangeUpdaterRef(),
                &invisibleLeadingWhiteSpaceRangeOfNewLine);
            // We are at start of non-NBSPs.  Convert to a single NBSP.
            const auto endOfCollapsibleASCIIWhiteSpaces =
                textFragmentDataAtInsertionPoint
                    .GetEndOfCollapsibleASCIIWhiteSpaces<EditorDOMPointInText>(
                        atNextCharOfInsertionPoint, nsIEditor::eNone,
                        // XXX Shouldn't be "No"?  Skipping non-editable nodes
                        // may have visible content.
                        IgnoreNonEditableNodes::Yes);
            nsresult rv =
                WhiteSpaceVisibilityKeeper::ReplaceTextAndRemoveEmptyTextNodes(
                    aHTMLEditor,
                    EditorDOMRangeInTexts(atNextCharOfInsertionPoint,
                                          endOfCollapsibleASCIIWhiteSpaces),
                    nsDependentSubstring(&HTMLEditUtils::kNBSP, 1));
            if (MOZ_UNLIKELY(NS_FAILED(rv))) {
              NS_WARNING(
                  "WhiteSpaceVisibilityKeeper::"
                  "ReplaceTextAndRemoveEmptyTextNodes() failed");
              return Err(rv);
            }
            // Don't refer the following variables anymore unless tracking the
            // change.
            invisibleTrailingWhiteSpaceRangeOfCurrentLine.Clear();
          }
        }
      }

      if (invisibleLeadingWhiteSpaceRangeOfNewLine.IsPositioned()) {
        if (!invisibleLeadingWhiteSpaceRangeOfNewLine.Collapsed()) {
          AutoTrackDOMPoint trackPointToInsert(aHTMLEditor.RangeUpdaterRef(),
                                               &pointToInsert);
          // XXX Why don't we remove all of the invisible white-spaces?
          MOZ_ASSERT(invisibleLeadingWhiteSpaceRangeOfNewLine.EndRef() ==
                     pointToInsert);
          Result<CaretPoint, nsresult> caretPointOrError =
              aHTMLEditor.DeleteTextAndTextNodesWithTransaction(
                  invisibleLeadingWhiteSpaceRangeOfNewLine.StartRef(),
                  invisibleLeadingWhiteSpaceRangeOfNewLine.EndRef(),
                  HTMLEditor::TreatEmptyTextNodes::
                      KeepIfContainerOfRangeBoundaries);
          if (MOZ_UNLIKELY(caretPointOrError.isErr())) {
            NS_WARNING(
                "HTMLEditor::DeleteTextAndTextNodesWithTransaction() failed");
            return caretPointOrError.propagateErr();
          }
          nsresult rv = caretPointOrError.unwrap().SuggestCaretPointTo(
              aHTMLEditor, {SuggestCaret::OnlyIfHasSuggestion,
                            SuggestCaret::OnlyIfTransactionsAllowedToDoIt,
                            SuggestCaret::AndIgnoreTrivialError});
          if (NS_FAILED(rv)) {
            NS_WARNING("CaretPoint::SuggestCaretPointTo() failed");
            return Err(rv);
          }
          NS_WARNING_ASSERTION(
              rv != NS_SUCCESS_EDITOR_BUT_IGNORED_TRIVIAL_ERROR,
              "CaretPoint::SuggestCaretPointTo() failed, but ignored");
          // Don't refer the following variables anymore unless tracking the
          // change.
          atNBSPReplaceableWithSP.Clear();
          invisibleLeadingWhiteSpaceRangeOfNewLine.Clear();
          invisibleTrailingWhiteSpaceRangeOfCurrentLine.Clear();
        }
      }
      // If the `<br>` element is put immediately after an NBSP, it should be
      // replaced with an ASCII white-space.
      else if (atNBSPReplaceableWithSP.IsInTextNode()) {
        const EditorDOMPointInText atNBSPReplacedWithASCIIWhiteSpace =
            atNBSPReplaceableWithSP.AsInText();
        if (!atNBSPReplacedWithASCIIWhiteSpace.IsEndOfContainer() &&
            atNBSPReplacedWithASCIIWhiteSpace.IsCharNBSP()) {
          AutoTrackDOMPoint trackPointToInsert(aHTMLEditor.RangeUpdaterRef(),
                                               &pointToInsert);
          Result<InsertTextResult, nsresult> replaceTextResult =
              aHTMLEditor.ReplaceTextWithTransaction(
                  MOZ_KnownLive(
                      *atNBSPReplacedWithASCIIWhiteSpace.ContainerAs<Text>()),
                  atNBSPReplacedWithASCIIWhiteSpace.Offset(), 1, u" "_ns);
          if (MOZ_UNLIKELY(replaceTextResult.isErr())) {
            NS_WARNING(
                "HTMLEditor::ReplaceTextWithTransaction() failed failed");
            return replaceTextResult.propagateErr();
          }
          // Ignore caret suggestion because there was
          // AutoTransactionsConserveSelection.
          replaceTextResult.unwrap().IgnoreCaretPointSuggestion();
          // Don't refer the following variables anymore unless tracking the
          // change.
          atNBSPReplaceableWithSP.Clear();
          invisibleLeadingWhiteSpaceRangeOfNewLine.Clear();
          invisibleTrailingWhiteSpaceRangeOfCurrentLine.Clear();
        }
      }
    }
  } else {
    // Chrome does not normalize preceding white-spaces at least when it ends
    // with an NBSP.
    Result<EditorDOMPoint, nsresult>
        normalizeSurroundingWhiteSpacesResultOrError =
            WhiteSpaceVisibilityKeeper::NormalizeWhiteSpacesToSplitAt(
                aHTMLEditor, aPointToInsert,
                {NormalizeOption::StopIfPrecedingWhiteSpacesEndsWithNBP});
    if (MOZ_UNLIKELY(normalizeSurroundingWhiteSpacesResultOrError.isErr())) {
      NS_WARNING(
          "WhiteSpaceVisibilityKeeper::NormalizeWhiteSpacesToSplitAt() failed");
      return normalizeSurroundingWhiteSpacesResultOrError.propagateErr();
    }
    pointToInsert = normalizeSurroundingWhiteSpacesResultOrError.unwrap();
    if (NS_WARN_IF(!pointToInsert.IsSet())) {
      return Err(NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE);
    }
  }

  Result<CreateLineBreakResult, nsresult> insertBRElementResultOrError =
      aHTMLEditor.InsertLineBreak(WithTransaction::Yes, aLineBreakType,
                                  pointToInsert);
  NS_WARNING_ASSERTION(insertBRElementResultOrError.isOk(),
                       "HTMLEditor::InsertLineBreak(WithTransaction::Yes, "
                       "aLineBreakType, eNone) failed");
  return insertBRElementResultOrError;
}

nsresult WhiteSpaceVisibilityKeeper::EnsureNoInvisibleWhiteSpacesAfter(
    HTMLEditor& aHTMLEditor, const EditorDOMPoint& aPoint) {
  MOZ_ASSERT(StaticPrefs::editor_white_space_normalization_blink_compatible());
  MOZ_ASSERT(aPoint.IsInContentNode());

  const RefPtr<Element> colsetBlockElement =
      HTMLEditUtils::GetInclusiveAncestorElement(
          *aPoint.ContainerAs<nsIContent>(),
          HTMLEditUtils::ClosestEditableBlockElement,
          BlockInlineCheck::UseComputedDisplayStyle);
  EditorDOMPoint atFirstInvisibleWhiteSpace;
  AutoTArray<OwningNonNull<nsIContent>, 32> unnecessaryContents;
  for (nsIContent* nextContent =
           HTMLEditUtils::GetNextLeafContentOrNextBlockElement(
               aPoint, {HTMLEditUtils::LeafNodeType::LeafNodeOrChildBlock},
               BlockInlineCheck::UseComputedDisplayStyle, colsetBlockElement);
       nextContent;
       nextContent = HTMLEditUtils::GetNextLeafContentOrNextBlockElement(
           EditorRawDOMPoint::After(*nextContent),
           {HTMLEditUtils::LeafNodeType::LeafNodeOrChildBlock},
           BlockInlineCheck::UseComputedDisplayStyle, colsetBlockElement)) {
    if (!HTMLEditUtils::IsSimplyEditableNode(*nextContent)) {
      // XXX Assume non-editable nodes are visible.
      break;
    }
    const RefPtr<Text> followingTextNode = Text::FromNode(nextContent);
    if (!followingTextNode &&
        HTMLEditUtils::IsVisibleElementEvenIfLeafNode(*nextContent)) {
      break;
    }
    if (!followingTextNode || !followingTextNode->TextDataLength()) {
      // If it's an empty inline element like `<b></b>` or an empty `Text`,
      // delete it.
      nsIContent* emptyInlineContent =
          HTMLEditUtils::GetMostDistantAncestorEditableEmptyInlineElement(
              *nextContent, BlockInlineCheck::UseComputedDisplayStyle);
      if (!emptyInlineContent) {
        emptyInlineContent = nextContent;
      }
      unnecessaryContents.AppendElement(*emptyInlineContent);
      continue;
    }
    const EditorRawDOMPointInText atFirstChar(followingTextNode, 0u);
    if (!atFirstChar.IsCharCollapsibleASCIISpace()) {
      break;
    }
    // If the preceding Text is collapsed and invisible, we should delete it
    // and keep deleting preceding invisible white-spaces.
    if (!HTMLEditUtils::IsVisibleTextNode(*followingTextNode)) {
      nsIContent* emptyInlineContent =
          HTMLEditUtils::GetMostDistantAncestorEditableEmptyInlineElement(
              *followingTextNode, BlockInlineCheck::UseComputedDisplayStyle);
      if (!emptyInlineContent) {
        emptyInlineContent = followingTextNode;
      }
      unnecessaryContents.AppendElement(*emptyInlineContent);
      continue;
    }
    Result<EditorDOMPoint, nsresult> startOfTextOrError =
        WhiteSpaceVisibilityKeeper::EnsureNoInvisibleWhiteSpaces(
            aHTMLEditor, EditorDOMPoint(followingTextNode, 0u));
    if (MOZ_UNLIKELY(startOfTextOrError.isErr())) {
      NS_WARNING("HTMLEditor::ReplaceTextWithTransaction() failed");
      return startOfTextOrError.unwrapErr();
    }
    break;
  }

  for (const auto& contentToDelete : unnecessaryContents) {
    if (MOZ_UNLIKELY(!contentToDelete->IsInComposedDoc())) {
      continue;
    }
    nsresult rv =
        aHTMLEditor.DeleteNodeWithTransaction(MOZ_KnownLive(contentToDelete));
    if (NS_FAILED(rv)) {
      NS_WARNING("EditorBase::DeleteNodeWithTransaction() failed");
      return rv;
    }
  }
  return NS_OK;
}

nsresult WhiteSpaceVisibilityKeeper::EnsureNoInvisibleWhiteSpacesBefore(
    HTMLEditor& aHTMLEditor, const EditorDOMPoint& aPoint) {
  MOZ_ASSERT(StaticPrefs::editor_white_space_normalization_blink_compatible());
  MOZ_ASSERT(aPoint.IsInContentNode());

  const RefPtr<Element> colsetBlockElement =
      HTMLEditUtils::GetInclusiveAncestorElement(
          *aPoint.ContainerAs<nsIContent>(),
          HTMLEditUtils::ClosestEditableBlockElement,
          BlockInlineCheck::UseComputedDisplayStyle);
  EditorDOMPoint atFirstInvisibleWhiteSpace;
  AutoTArray<OwningNonNull<nsIContent>, 32> unnecessaryContents;
  for (nsIContent* previousContent =
           HTMLEditUtils::GetPreviousLeafContentOrPreviousBlockElement(
               aPoint, {HTMLEditUtils::LeafNodeType::LeafNodeOrChildBlock},
               BlockInlineCheck::UseComputedDisplayStyle, colsetBlockElement);
       previousContent;
       previousContent =
           HTMLEditUtils::GetPreviousLeafContentOrPreviousBlockElement(
               EditorRawDOMPoint(previousContent),
               {HTMLEditUtils::LeafNodeType::LeafNodeOrChildBlock},
               BlockInlineCheck::UseComputedDisplayStyle, colsetBlockElement)) {
    if (!HTMLEditUtils::IsSimplyEditableNode(*previousContent)) {
      // XXX Assume non-editable nodes are visible.
      break;
    }
    const RefPtr<Text> precedingTextNode = Text::FromNode(previousContent);
    if (!precedingTextNode &&
        HTMLEditUtils::IsVisibleElementEvenIfLeafNode(*previousContent)) {
      break;
    }
    if (!precedingTextNode || !precedingTextNode->TextDataLength()) {
      // If it's an empty inline element like `<b></b>` or an empty `Text`,
      // delete it.
      nsIContent* emptyInlineContent =
          HTMLEditUtils::GetMostDistantAncestorEditableEmptyInlineElement(
              *previousContent, BlockInlineCheck::UseComputedDisplayStyle);
      if (!emptyInlineContent) {
        emptyInlineContent = previousContent;
      }
      unnecessaryContents.AppendElement(*emptyInlineContent);
      continue;
    }
    const auto atLastChar =
        EditorRawDOMPointInText::AtLastContentOf(*precedingTextNode);
    if (!atLastChar.IsCharCollapsibleASCIISpace()) {
      break;
    }
    // If the preceding Text is collapsed and invisible, we should delete it
    // and keep deleting preceding invisible white-spaces.
    if (!HTMLEditUtils::IsVisibleTextNode(*precedingTextNode)) {
      nsIContent* emptyInlineContent =
          HTMLEditUtils::GetMostDistantAncestorEditableEmptyInlineElement(
              *precedingTextNode, BlockInlineCheck::UseComputedDisplayStyle);
      if (!emptyInlineContent) {
        emptyInlineContent = precedingTextNode;
      }
      unnecessaryContents.AppendElement(*emptyInlineContent);
      continue;
    }
    Result<EditorDOMPoint, nsresult> endOfTextOrResult =
        WhiteSpaceVisibilityKeeper::EnsureNoInvisibleWhiteSpaces(
            aHTMLEditor, EditorDOMPoint::AtEndOf(*precedingTextNode));
    if (MOZ_UNLIKELY(endOfTextOrResult.isErr())) {
      NS_WARNING("HTMLEditor::ReplaceTextWithTransaction() failed");
      return endOfTextOrResult.unwrapErr();
    }
    break;
  }

  for (const auto& contentToDelete : Reversed(unnecessaryContents)) {
    if (MOZ_UNLIKELY(!contentToDelete->IsInComposedDoc())) {
      continue;
    }
    nsresult rv =
        aHTMLEditor.DeleteNodeWithTransaction(MOZ_KnownLive(contentToDelete));
    if (NS_FAILED(rv)) {
      NS_WARNING("EditorBase::DeleteNodeWithTransaction() failed");
      return rv;
    }
  }
  return NS_OK;
}

Result<EditorDOMPoint, nsresult>
WhiteSpaceVisibilityKeeper::EnsureNoInvisibleWhiteSpaces(
    HTMLEditor& aHTMLEditor, const EditorDOMPoint& aPoint) {
  if (EditorUtils::IsWhiteSpacePreformatted(
          *aPoint.ContainerAs<nsIContent>())) {
    return EditorDOMPoint();
  }
  if (aPoint.IsInTextNode() &&
      // If there is a previous char and it's not a collapsible ASCII
      // white-space, the point is not in the leading white-spaces.
      (!aPoint.IsStartOfContainer() && !aPoint.IsPreviousCharASCIISpace()) &&
      // If it does not points a collapsible ASCII white-space, the point is not
      // in the trailing white-spaces.
      (!aPoint.IsEndOfContainer() && !aPoint.IsCharCollapsibleASCIISpace())) {
    return EditorDOMPoint();
  }
  const Element* const closestBlockElement =
      HTMLEditUtils::GetInclusiveAncestorElement(
          *aPoint.ContainerAs<nsIContent>(), HTMLEditUtils::ClosestBlockElement,
          BlockInlineCheck::UseComputedDisplayStyle);
  if (MOZ_UNLIKELY(!closestBlockElement)) {
    return EditorDOMPoint();  // aPoint is not in a block.
  }
  const TextFragmentData textFragmentDataForLeadingWhiteSpaces(
      Scan::EditableNodes,
      aPoint.IsStartOfContainer() &&
              aPoint.GetContainer() == closestBlockElement
          ? aPoint
          : aPoint.PreviousPointOrParentPoint<EditorDOMPoint>(),
      BlockInlineCheck::UseComputedDisplayStyle, closestBlockElement);
  if (NS_WARN_IF(!textFragmentDataForLeadingWhiteSpaces.IsInitialized())) {
    return Err(NS_ERROR_FAILURE);
  }

  {
    const EditorDOMRange& leadingWhiteSpaceRange =
        textFragmentDataForLeadingWhiteSpaces
            .InvisibleLeadingWhiteSpaceRangeRef();
    if (leadingWhiteSpaceRange.IsPositioned() &&
        !leadingWhiteSpaceRange.Collapsed()) {
      EditorDOMPoint endOfLeadingWhiteSpaces(leadingWhiteSpaceRange.EndRef());
      AutoTrackDOMPoint trackEndOfLeadingWhiteSpaces(
          aHTMLEditor.RangeUpdaterRef(), &endOfLeadingWhiteSpaces);
      Result<CaretPoint, nsresult> caretPointOrError =
          aHTMLEditor.DeleteTextAndTextNodesWithTransaction(
              leadingWhiteSpaceRange.StartRef(),
              leadingWhiteSpaceRange.EndRef(),
              HTMLEditor::TreatEmptyTextNodes::
                  KeepIfContainerOfRangeBoundaries);
      if (MOZ_UNLIKELY(caretPointOrError.isErr())) {
        NS_WARNING(
            "HTMLEditor::DeleteTextAndTextNodesWithTransaction("
            "TreatEmptyTextNodes::KeepIfContainerOfRangeBoundaries) failed");
        return caretPointOrError.propagateErr();
      }
      caretPointOrError.unwrap().IgnoreCaretPointSuggestion();
      // If the leading white-spaces were split into multiple text node, we need
      // only the last `Text` node.
      if (!leadingWhiteSpaceRange.InSameContainer() &&
          leadingWhiteSpaceRange.StartRef().IsInTextNode() &&
          leadingWhiteSpaceRange.StartRef()
              .ContainerAs<Text>()
              ->IsInComposedDoc() &&
          leadingWhiteSpaceRange.EndRef().IsInTextNode() &&
          leadingWhiteSpaceRange.EndRef()
              .ContainerAs<Text>()
              ->IsInComposedDoc() &&
          !leadingWhiteSpaceRange.StartRef()
               .ContainerAs<Text>()
               ->TextDataLength()) {
        nsresult rv = aHTMLEditor.DeleteNodeWithTransaction(MOZ_KnownLive(
            *leadingWhiteSpaceRange.StartRef().ContainerAs<Text>()));
        if (NS_FAILED(rv)) {
          NS_WARNING("HTMLEditor::DeleteNodeWithTransaction() failed");
          return Err(rv);
        }
      }
      trackEndOfLeadingWhiteSpaces.FlushAndStopTracking();
      if (NS_WARN_IF(!endOfLeadingWhiteSpaces.IsSetAndValidInComposedDoc())) {
        return Err(NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE);
      }
      return endOfLeadingWhiteSpaces;
    }
  }

  const TextFragmentData textFragmentData =
      textFragmentDataForLeadingWhiteSpaces.ScanStartRef() == aPoint
          ? textFragmentDataForLeadingWhiteSpaces
          : TextFragmentData(Scan::EditableNodes, aPoint,
                             BlockInlineCheck::UseComputedDisplayStyle,
                             closestBlockElement);
  const EditorDOMRange& trailingWhiteSpaceRange =
      textFragmentData.InvisibleTrailingWhiteSpaceRangeRef();
  if (trailingWhiteSpaceRange.IsPositioned() &&
      !trailingWhiteSpaceRange.Collapsed()) {
    EditorDOMPoint startOfTrailingWhiteSpaces(
        trailingWhiteSpaceRange.StartRef());
    AutoTrackDOMPoint trackStartOfTrailingWhiteSpaces(
        aHTMLEditor.RangeUpdaterRef(), &startOfTrailingWhiteSpaces);
    Result<CaretPoint, nsresult> caretPointOrError =
        aHTMLEditor.DeleteTextAndTextNodesWithTransaction(
            trailingWhiteSpaceRange.StartRef(),
            trailingWhiteSpaceRange.EndRef(),
            HTMLEditor::TreatEmptyTextNodes::KeepIfContainerOfRangeBoundaries);
    if (MOZ_UNLIKELY(caretPointOrError.isErr())) {
      NS_WARNING(
          "HTMLEditor::DeleteTextAndTextNodesWithTransaction("
          "TreatEmptyTextNodes::KeepIfContainerOfRangeBoundaries) failed");
      return caretPointOrError.propagateErr();
    }
    caretPointOrError.unwrap().IgnoreCaretPointSuggestion();
    // If the leading white-spaces were split into multiple text node, we need
    // only the last `Text` node.
    if (!trailingWhiteSpaceRange.InSameContainer() &&
        trailingWhiteSpaceRange.StartRef().IsInTextNode() &&
        trailingWhiteSpaceRange.StartRef()
            .ContainerAs<Text>()
            ->IsInComposedDoc() &&
        trailingWhiteSpaceRange.EndRef().IsInTextNode() &&
        trailingWhiteSpaceRange.EndRef()
            .ContainerAs<Text>()
            ->IsInComposedDoc() &&
        !trailingWhiteSpaceRange.EndRef()
             .ContainerAs<Text>()
             ->TextDataLength()) {
      nsresult rv = aHTMLEditor.DeleteNodeWithTransaction(
          MOZ_KnownLive(*trailingWhiteSpaceRange.EndRef().ContainerAs<Text>()));
      if (NS_FAILED(rv)) {
        NS_WARNING("HTMLEditor::DeleteNodeWithTransaction() failed");
        return Err(rv);
      }
    }
    trackStartOfTrailingWhiteSpaces.FlushAndStopTracking();
    if (NS_WARN_IF(!startOfTrailingWhiteSpaces.IsSetAndValidInComposedDoc())) {
      return Err(NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE);
    }
    return startOfTrailingWhiteSpaces;
  }

  const auto atCollapsibleASCIISpace = [&]() -> EditorDOMPointInText {
    const auto point =
        textFragmentData.GetInclusiveNextCharPoint<EditorDOMPointInText>(
            textFragmentData.ScanStartRef(), IgnoreNonEditableNodes::Yes);
    if (point.IsSet() &&
        // XXX Perhaps, we should ignore empty `Text` nodes and keep scanning.
        !point.IsEndOfContainer() && point.IsCharCollapsibleASCIISpace()) {
      return point;
    }
    const auto prevPoint =
        textFragmentData.GetPreviousCharPoint<EditorDOMPointInText>(
            textFragmentData.ScanStartRef(), IgnoreNonEditableNodes::Yes);
    return prevPoint.IsSet() &&
                   // XXX Perhaps, we should ignore empty `Text` nodes and keep
                   // scanning.
                   !prevPoint.IsEndOfContainer() &&
                   prevPoint.IsCharCollapsibleASCIISpace()
               ? prevPoint
               : EditorDOMPointInText();
  }();
  if (!atCollapsibleASCIISpace.IsSet()) {
    return EditorDOMPoint();
  }
  const auto firstCollapsibleASCIISpacePoint =
      textFragmentData
          .GetFirstASCIIWhiteSpacePointCollapsedTo<EditorDOMPointInText>(
              atCollapsibleASCIISpace, nsIEditor::eNone,
              IgnoreNonEditableNodes::No);
  const auto endOfCollapsibleASCIISpacePoint =
      textFragmentData
          .GetEndOfCollapsibleASCIIWhiteSpaces<EditorDOMPointInText>(
              atCollapsibleASCIISpace, nsIEditor::eNone,
              IgnoreNonEditableNodes::No);
  if (firstCollapsibleASCIISpacePoint.NextPoint() ==
      endOfCollapsibleASCIISpacePoint) {
    // Only one white-space, so that nothing to do.
    return EditorDOMPoint();
  }
  // Okay, there are some collapsed white-spaces. We should delete them with
  // keeping first one.
  Result<CaretPoint, nsresult> deleteTextResultOrError =
      aHTMLEditor.DeleteTextAndTextNodesWithTransaction(
          firstCollapsibleASCIISpacePoint.NextPoint(),
          endOfCollapsibleASCIISpacePoint,
          HTMLEditor::TreatEmptyTextNodes::Remove);
  if (MOZ_UNLIKELY(deleteTextResultOrError.isErr())) {
    NS_WARNING("HTMLEditor::DeleteTextWithTransaction() failed");
    return deleteTextResultOrError.propagateErr();
  }
  return deleteTextResultOrError.unwrap().UnwrapCaretPoint();
}

// static
Result<InsertTextResult, nsresult>
WhiteSpaceVisibilityKeeper::InsertTextOrInsertOrUpdateCompositionString(
    HTMLEditor& aHTMLEditor, const nsAString& aStringToInsert,
    const EditorDOMRange& aRangeToBeReplaced, InsertTextTo aInsertTextTo,
    InsertTextFor aPurpose) {
  MOZ_ASSERT(aRangeToBeReplaced.StartRef().IsInContentNode());
  MOZ_ASSERT_IF(!EditorBase::InsertingTextForExtantComposition(aPurpose),
                aRangeToBeReplaced.Collapsed());
  if (aStringToInsert.IsEmpty()) {
    MOZ_ASSERT(aRangeToBeReplaced.Collapsed());
    return InsertTextResult();
  }

  // TODO: Delete this block once we ship the new normalizer.
  if (!StaticPrefs::editor_white_space_normalization_blink_compatible()) {
    // MOOSE: for now, we always assume non-PRE formatting.  Fix this later.
    // meanwhile, the pre case is handled in HandleInsertText() in
    // HTMLEditSubActionHandler.cpp

    // MOOSE: for now, just getting the ws logic straight.  This implementation
    // is very slow.  Will need to replace edit rules impl with a more efficient
    // text sink here that does the minimal amount of
    // searching/replacing/copying

    const TextFragmentData textFragmentDataAtStart(
        Scan::EditableNodes, aRangeToBeReplaced.StartRef(),
        BlockInlineCheck::UseComputedDisplayStyle);
    if (MOZ_UNLIKELY(NS_WARN_IF(!textFragmentDataAtStart.IsInitialized()))) {
      return Err(NS_ERROR_FAILURE);
    }
    const bool isInsertionPointEqualsOrIsBeforeStartOfText =
        aRangeToBeReplaced.StartRef().EqualsOrIsBefore(
            textFragmentDataAtStart.StartRef());
    TextFragmentData textFragmentDataAtEnd =
        aRangeToBeReplaced.Collapsed()
            ? textFragmentDataAtStart
            : TextFragmentData(Scan::EditableNodes, aRangeToBeReplaced.EndRef(),
                               BlockInlineCheck::UseComputedDisplayStyle);
    if (MOZ_UNLIKELY(NS_WARN_IF(!textFragmentDataAtEnd.IsInitialized()))) {
      return Err(NS_ERROR_FAILURE);
    }
    const bool isInsertionPointEqualsOrAfterEndOfText =
        textFragmentDataAtEnd.EndRef().EqualsOrIsBefore(
            aRangeToBeReplaced.EndRef());

    EditorDOMRange invisibleLeadingWhiteSpaceRangeAtStart =
        textFragmentDataAtStart
            .GetNewInvisibleLeadingWhiteSpaceRangeIfSplittingAt(
                aRangeToBeReplaced.StartRef());
    const bool isInvisibleLeadingWhiteSpaceRangeAtStartPositioned =
        invisibleLeadingWhiteSpaceRangeAtStart.IsPositioned();
    EditorDOMRange invisibleTrailingWhiteSpaceRangeAtEnd =
        textFragmentDataAtEnd
            .GetNewInvisibleTrailingWhiteSpaceRangeIfSplittingAt(
                aRangeToBeReplaced.EndRef());
    const bool isInvisibleTrailingWhiteSpaceRangeAtEndPositioned =
        invisibleTrailingWhiteSpaceRangeAtEnd.IsPositioned();
    const Maybe<const VisibleWhiteSpacesData> visibleWhiteSpacesAtStart =
        !isInvisibleLeadingWhiteSpaceRangeAtStartPositioned
            ? Some(textFragmentDataAtStart.VisibleWhiteSpacesDataRef())
            : Nothing();
    const PointPosition pointPositionWithVisibleWhiteSpacesAtStart =
        visibleWhiteSpacesAtStart.isSome() &&
                visibleWhiteSpacesAtStart.ref().IsInitialized()
            ? visibleWhiteSpacesAtStart.ref().ComparePoint(
                  aRangeToBeReplaced.StartRef())
            : PointPosition::NotInSameDOMTree;
    const Maybe<const VisibleWhiteSpacesData> visibleWhiteSpacesAtEnd =
        !isInvisibleTrailingWhiteSpaceRangeAtEndPositioned
            ? Some(textFragmentDataAtEnd.VisibleWhiteSpacesDataRef())
            : Nothing();
    const PointPosition pointPositionWithVisibleWhiteSpacesAtEnd =
        visibleWhiteSpacesAtEnd.isSome() &&
                visibleWhiteSpacesAtEnd.ref().IsInitialized()
            ? visibleWhiteSpacesAtEnd.ref().ComparePoint(
                  aRangeToBeReplaced.EndRef())
            : PointPosition::NotInSameDOMTree;

    EditorDOMPoint pointToPutCaret;
    EditorDOMPoint pointToInsert(aRangeToBeReplaced.StartRef());
    EditorDOMPoint atNBSPReplaceableWithSP;
    if (!invisibleTrailingWhiteSpaceRangeAtEnd.IsPositioned() &&
        (pointPositionWithVisibleWhiteSpacesAtStart ==
             PointPosition::MiddleOfFragment ||
         pointPositionWithVisibleWhiteSpacesAtStart ==
             PointPosition::EndOfFragment)) {
      atNBSPReplaceableWithSP =
          textFragmentDataAtStart
              .GetPreviousNBSPPointIfNeedToReplaceWithASCIIWhiteSpace(
                  pointToInsert)
              .To<EditorDOMPoint>();
    }
    nsAutoString theString(aStringToInsert);
    {
      if (invisibleTrailingWhiteSpaceRangeAtEnd.IsPositioned()) {
        if (!invisibleTrailingWhiteSpaceRangeAtEnd.Collapsed()) {
          AutoTrackDOMPoint trackPointToInsert(aHTMLEditor.RangeUpdaterRef(),
                                               &pointToInsert);
          AutoTrackDOMPoint trackPrecedingNBSP(aHTMLEditor.RangeUpdaterRef(),
                                               &atNBSPReplaceableWithSP);
          AutoTrackDOMRange trackInvisibleLeadingWhiteSpaceRange(
              aHTMLEditor.RangeUpdaterRef(),
              &invisibleLeadingWhiteSpaceRangeAtStart);
          AutoTrackDOMRange trackInvisibleTrailingWhiteSpaceRange(
              aHTMLEditor.RangeUpdaterRef(),
              &invisibleTrailingWhiteSpaceRangeAtEnd);
          // XXX Why don't we remove all of the invisible white-spaces?
          MOZ_ASSERT(invisibleTrailingWhiteSpaceRangeAtEnd.StartRef() ==
                     pointToInsert);
          Result<CaretPoint, nsresult> caretPointOrError =
              aHTMLEditor.DeleteTextAndTextNodesWithTransaction(
                  invisibleTrailingWhiteSpaceRangeAtEnd.StartRef(),
                  invisibleTrailingWhiteSpaceRangeAtEnd.EndRef(),
                  HTMLEditor::TreatEmptyTextNodes::
                      KeepIfContainerOfRangeBoundaries);
          if (MOZ_UNLIKELY(caretPointOrError.isErr())) {
            NS_WARNING(
                "HTMLEditor::DeleteTextAndTextNodesWithTransaction() failed");
            return caretPointOrError.propagateErr();
          }
          caretPointOrError.unwrap().MoveCaretPointTo(
              pointToPutCaret, {SuggestCaret::OnlyIfHasSuggestion});
        }
      }
      // Replace an NBSP at inclusive next character of replacing range to an
      // ASCII white-space if inserting into a visible white-space sequence.
      // XXX With modifying the inserting string later, this creates a line
      //     break opportunity after the inserting string, but this causes
      //     inconsistent result with inserting order.  E.g., type white-space
      //     n times with various order.
      else if (pointPositionWithVisibleWhiteSpacesAtEnd ==
                   PointPosition::StartOfFragment ||
               pointPositionWithVisibleWhiteSpacesAtEnd ==
                   PointPosition::MiddleOfFragment) {
        EditorDOMPointInText atNBSPReplacedWithASCIIWhiteSpace =
            textFragmentDataAtEnd
                .GetInclusiveNextNBSPPointIfNeedToReplaceWithASCIIWhiteSpace(
                    aRangeToBeReplaced.EndRef());
        if (atNBSPReplacedWithASCIIWhiteSpace.IsSet()) {
          AutoTrackDOMPoint trackPointToPutCaret(aHTMLEditor.RangeUpdaterRef(),
                                                 &pointToPutCaret);
          AutoTrackDOMPoint trackPointToInsert(aHTMLEditor.RangeUpdaterRef(),
                                               &pointToInsert);
          AutoTrackDOMPoint trackPrecedingNBSP(aHTMLEditor.RangeUpdaterRef(),
                                               &atNBSPReplaceableWithSP);
          AutoTrackDOMRange trackInvisibleLeadingWhiteSpaceRange(
              aHTMLEditor.RangeUpdaterRef(),
              &invisibleLeadingWhiteSpaceRangeAtStart);
          AutoTrackDOMRange trackInvisibleTrailingWhiteSpaceRange(
              aHTMLEditor.RangeUpdaterRef(),
              &invisibleTrailingWhiteSpaceRangeAtEnd);
          Result<InsertTextResult, nsresult> replaceTextResult =
              aHTMLEditor.ReplaceTextWithTransaction(
                  MOZ_KnownLive(
                      *atNBSPReplacedWithASCIIWhiteSpace.ContainerAs<Text>()),
                  atNBSPReplacedWithASCIIWhiteSpace.Offset(), 1, u" "_ns);
          if (MOZ_UNLIKELY(replaceTextResult.isErr())) {
            NS_WARNING("HTMLEditor::ReplaceTextWithTransaction() failed");
            return replaceTextResult.propagateErr();
          }
          // Ignore caret suggestion because there was
          // AutoTransactionsConserveSelection.
          replaceTextResult.unwrap().IgnoreCaretPointSuggestion();
        }
      }

      if (invisibleLeadingWhiteSpaceRangeAtStart.IsPositioned()) {
        if (!invisibleLeadingWhiteSpaceRangeAtStart.Collapsed()) {
          AutoTrackDOMPoint trackPointToPutCaret(aHTMLEditor.RangeUpdaterRef(),
                                                 &pointToPutCaret);
          AutoTrackDOMPoint trackPointToInsert(aHTMLEditor.RangeUpdaterRef(),
                                               &pointToInsert);
          AutoTrackDOMRange trackInvisibleTrailingWhiteSpaceRange(
              aHTMLEditor.RangeUpdaterRef(),
              &invisibleTrailingWhiteSpaceRangeAtEnd);
          // XXX Why don't we remove all of the invisible white-spaces?
          MOZ_ASSERT(invisibleLeadingWhiteSpaceRangeAtStart.EndRef() ==
                     pointToInsert);
          Result<CaretPoint, nsresult> caretPointOrError =
              aHTMLEditor.DeleteTextAndTextNodesWithTransaction(
                  invisibleLeadingWhiteSpaceRangeAtStart.StartRef(),
                  invisibleLeadingWhiteSpaceRangeAtStart.EndRef(),
                  HTMLEditor::TreatEmptyTextNodes::
                      KeepIfContainerOfRangeBoundaries);
          if (MOZ_UNLIKELY(caretPointOrError.isErr())) {
            NS_WARNING(
                "HTMLEditor::DeleteTextAndTextNodesWithTransaction() failed");
            return caretPointOrError.propagateErr();
          }
          trackPointToPutCaret.FlushAndStopTracking();
          caretPointOrError.unwrap().MoveCaretPointTo(
              pointToPutCaret, {SuggestCaret::OnlyIfHasSuggestion});
          // Don't refer the following variables anymore unless tracking the
          // change.
          atNBSPReplaceableWithSP.Clear();
          invisibleLeadingWhiteSpaceRangeAtStart.Clear();
        }
      }
      // Replace an NBSP at previous character of insertion point to an ASCII
      // white-space if inserting into a visible white-space sequence.
      // XXX With modifying the inserting string later, this creates a line
      // break
      //     opportunity before the inserting string, but this causes
      //     inconsistent result with inserting order.  E.g., type white-space
      //     n times with various order.
      else if (atNBSPReplaceableWithSP.IsInTextNode()) {
        EditorDOMPointInText atNBSPReplacedWithASCIIWhiteSpace =
            atNBSPReplaceableWithSP.AsInText();
        if (!atNBSPReplacedWithASCIIWhiteSpace.IsEndOfContainer() &&
            atNBSPReplacedWithASCIIWhiteSpace.IsCharNBSP()) {
          AutoTrackDOMPoint trackPointToPutCaret(aHTMLEditor.RangeUpdaterRef(),
                                                 &pointToPutCaret);
          AutoTrackDOMPoint trackPointToInsert(aHTMLEditor.RangeUpdaterRef(),
                                               &pointToInsert);
          AutoTrackDOMRange trackInvisibleTrailingWhiteSpaceRange(
              aHTMLEditor.RangeUpdaterRef(),
              &invisibleTrailingWhiteSpaceRangeAtEnd);
          Result<InsertTextResult, nsresult> replaceTextResult =
              aHTMLEditor.ReplaceTextWithTransaction(
                  MOZ_KnownLive(
                      *atNBSPReplacedWithASCIIWhiteSpace.ContainerAs<Text>()),
                  atNBSPReplacedWithASCIIWhiteSpace.Offset(), 1, u" "_ns);
          if (MOZ_UNLIKELY(replaceTextResult.isErr())) {
            NS_WARNING(
                "HTMLEditor::ReplaceTextWithTransaction() failed failed");
            return replaceTextResult.propagateErr();
          }
          // Ignore caret suggestion because there was
          // AutoTransactionsConserveSelection.
          replaceTextResult.unwrap().IgnoreCaretPointSuggestion();
          // Don't refer the following variables anymore unless tracking the
          // change.
          atNBSPReplaceableWithSP.Clear();
          invisibleLeadingWhiteSpaceRangeAtStart.Clear();
        }
      }
    }

    // If white-space and/or linefeed characters are collapsible, and inserting
    // string starts and/or ends with a collapsible characters, we need to
    // replace them with NBSP for making sure the collapsible characters
    // visible. FYI: There is no case only linefeeds are collapsible.  So, we
    // need to
    //      do the things only when white-spaces are collapsible.
    MOZ_DIAGNOSTIC_ASSERT(!theString.IsEmpty());
    if (NS_WARN_IF(!pointToInsert.IsInContentNode()) ||
        !EditorUtils::IsWhiteSpacePreformatted(
            *pointToInsert.ContainerAs<nsIContent>())) {
      const bool isNewLineCollapsible =
          !pointToInsert.IsInContentNode() ||
          !EditorUtils::IsNewLinePreformatted(
              *pointToInsert.ContainerAs<nsIContent>());
      auto IsCollapsibleChar = [&isNewLineCollapsible](char16_t aChar) -> bool {
        return nsCRT::IsAsciiSpace(aChar) &&
               (isNewLineCollapsible || aChar != HTMLEditUtils::kNewLine);
      };
      if (IsCollapsibleChar(theString[0])) {
        // If inserting string will follow some invisible leading white-spaces,
        // the string needs to start with an NBSP.
        if (isInvisibleLeadingWhiteSpaceRangeAtStartPositioned) {
          theString.SetCharAt(HTMLEditUtils::kNBSP, 0);
        }
        // If inserting around visible white-spaces, check whether the previous
        // character of insertion point is an NBSP or an ASCII white-space.
        else if (pointPositionWithVisibleWhiteSpacesAtStart ==
                     PointPosition::MiddleOfFragment ||
                 pointPositionWithVisibleWhiteSpacesAtStart ==
                     PointPosition::EndOfFragment) {
          const auto atPreviousChar =
              textFragmentDataAtStart
                  .GetPreviousCharPoint<EditorRawDOMPointInText>(
                      pointToInsert, IgnoreNonEditableNodes::Yes);
          if (atPreviousChar.IsSet() && !atPreviousChar.IsEndOfContainer() &&
              atPreviousChar.IsCharASCIISpace()) {
            theString.SetCharAt(HTMLEditUtils::kNBSP, 0);
          }
        }
        // If the insertion point is (was) before the start of text and it's
        // immediately after a hard line break, the first ASCII white-space
        // should be replaced with an NBSP for making it visible.
        else if ((textFragmentDataAtStart.StartsFromHardLineBreak() ||
                  textFragmentDataAtStart
                      .StartsFromInlineEditingHostBoundary()) &&
                 isInsertionPointEqualsOrIsBeforeStartOfText) {
          theString.SetCharAt(HTMLEditUtils::kNBSP, 0);
        }
      }

      // Then the tail.  Note that it may be the first character.
      const uint32_t lastCharIndex = theString.Length() - 1;
      if (IsCollapsibleChar(theString[lastCharIndex])) {
        // If inserting string will be followed by some invisible trailing
        // white-spaces, the string needs to end with an NBSP.
        if (isInvisibleTrailingWhiteSpaceRangeAtEndPositioned) {
          theString.SetCharAt(HTMLEditUtils::kNBSP, lastCharIndex);
        }
        // If inserting around visible white-spaces, check whether the inclusive
        // next character of end of replaced range is an NBSP or an ASCII
        // white-space.
        if (pointPositionWithVisibleWhiteSpacesAtEnd ==
                PointPosition::StartOfFragment ||
            pointPositionWithVisibleWhiteSpacesAtEnd ==
                PointPosition::MiddleOfFragment) {
          const auto atNextChar =
              textFragmentDataAtEnd
                  .GetInclusiveNextCharPoint<EditorRawDOMPointInText>(
                      pointToInsert, IgnoreNonEditableNodes::Yes);
          if (atNextChar.IsSet() && !atNextChar.IsEndOfContainer() &&
              atNextChar.IsCharASCIISpace()) {
            theString.SetCharAt(HTMLEditUtils::kNBSP, lastCharIndex);
          }
        }
        // If the end of replacing range is (was) after the end of text and it's
        // immediately before block boundary, the last ASCII white-space should
        // be replaced with an NBSP for making it visible.
        else if ((textFragmentDataAtEnd.EndsByBlockBoundary() ||
                  textFragmentDataAtEnd.EndsByInlineEditingHostBoundary()) &&
                 isInsertionPointEqualsOrAfterEndOfText) {
          theString.SetCharAt(HTMLEditUtils::kNBSP, lastCharIndex);
        }
      }

      // Next, scan string for adjacent ws and convert to nbsp/space combos
      // MOOSE: don't need to convert tabs here since that is done by
      // WillInsertText() before we are called.  Eventually, all that logic will
      // be pushed down into here and made more efficient.
      enum class PreviousChar {
        NonCollapsibleChar,
        CollapsibleChar,
        PreformattedNewLine,
      };
      PreviousChar previousChar = PreviousChar::NonCollapsibleChar;
      for (uint32_t i = 0; i <= lastCharIndex; i++) {
        if (IsCollapsibleChar(theString[i])) {
          // If current char is collapsible and 2nd or latter character of
          // collapsible characters, we need to make the previous character an
          // NBSP for avoiding current character to be collapsed to it.
          if (previousChar == PreviousChar::CollapsibleChar) {
            MOZ_ASSERT(i > 0);
            theString.SetCharAt(HTMLEditUtils::kNBSP, i - 1);
            // Keep previousChar as PreviousChar::CollapsibleChar.
            continue;
          }

          // If current character is a collapsbile white-space and the previous
          // character is a preformatted linefeed, we need to replace the
          // current character with an NBSP for avoiding collapsed with the
          // previous linefeed.
          if (previousChar == PreviousChar::PreformattedNewLine) {
            MOZ_ASSERT(i > 0);
            theString.SetCharAt(HTMLEditUtils::kNBSP, i);
            previousChar = PreviousChar::NonCollapsibleChar;
            continue;
          }

          previousChar = PreviousChar::CollapsibleChar;
          continue;
        }

        if (theString[i] != HTMLEditUtils::kNewLine) {
          previousChar = PreviousChar::NonCollapsibleChar;
          continue;
        }

        // If current character is a preformatted linefeed and the previous
        // character is collapbile white-space, the previous character will be
        // collapsed into current linefeed.  Therefore, we need to replace the
        // previous character with an NBSP.
        MOZ_ASSERT(!isNewLineCollapsible);
        if (previousChar == PreviousChar::CollapsibleChar) {
          MOZ_ASSERT(i > 0);
          theString.SetCharAt(HTMLEditUtils::kNBSP, i - 1);
        }
        previousChar = PreviousChar::PreformattedNewLine;
      }
    }

    // XXX If the point is not editable, InsertTextWithTransaction() returns
    //     error, but we keep handling it.  But I think that it wastes the
    //     runtime cost.  So, perhaps, we should return error code which
    //     couldn't modify it and make each caller of this method decide whether
    //     it should keep or stop handling the edit action.
    AutoTrackDOMPoint trackPointToPutCaret(aHTMLEditor.RangeUpdaterRef(),
                                           &pointToPutCaret);
    Result<InsertTextResult, nsresult> insertTextResult =
        aHTMLEditor.InsertTextWithTransaction(theString, pointToInsert,
                                              aInsertTextTo);
    if (MOZ_UNLIKELY(insertTextResult.isErr())) {
      NS_WARNING("HTMLEditor::InsertTextWithTransaction() failed");
      return insertTextResult.propagateErr();
    }
    trackPointToPutCaret.FlushAndStopTracking();
    if (insertTextResult.inspect().HasCaretPointSuggestion()) {
      return insertTextResult;
    }
    return InsertTextResult(insertTextResult.unwrap(),
                            std::move(pointToPutCaret));
  }

  if (NS_WARN_IF(!aRangeToBeReplaced.StartRef().IsInContentNode())) {
    return Err(NS_ERROR_FAILURE);  // Cannot insert text
  }

  EditorDOMPoint pointToInsert = aHTMLEditor.ComputePointToInsertText(
      aRangeToBeReplaced.StartRef(), aInsertTextTo);
  MOZ_ASSERT(pointToInsert.IsInContentNode());
  const bool isWhiteSpaceCollapsible = !EditorUtils::IsWhiteSpacePreformatted(
      *aRangeToBeReplaced.StartRef().ContainerAs<nsIContent>());

  // First, delete invisible leading white-spaces and trailing white-spaces if
  // they are there around the replacing range boundaries.  However, don't do
  // that if we're updating existing composition string to avoid the composition
  // transaction is broken by the text change around composition string.
  if (!EditorBase::InsertingTextForExtantComposition(aPurpose) &&
      isWhiteSpaceCollapsible && pointToInsert.IsInContentNode()) {
    AutoTrackDOMPoint trackPointToInsert(aHTMLEditor.RangeUpdaterRef(),
                                         &pointToInsert);
    Result<EditorDOMPoint, nsresult>
        deletePointOfInvisibleWhiteSpacesAtStartOrError =
            WhiteSpaceVisibilityKeeper::EnsureNoInvisibleWhiteSpaces(
                aHTMLEditor, pointToInsert);
    if (MOZ_UNLIKELY(deletePointOfInvisibleWhiteSpacesAtStartOrError.isErr())) {
      NS_WARNING(
          "WhiteSpaceVisibilityKeeper::EnsureNoInvisibleWhiteSpaces() failed");
      return deletePointOfInvisibleWhiteSpacesAtStartOrError.propagateErr();
    }
    trackPointToInsert.FlushAndStopTracking();
    const EditorDOMPoint deletePointOfInvisibleWhiteSpacesAtStart =
        deletePointOfInvisibleWhiteSpacesAtStartOrError.unwrap();
    if (NS_WARN_IF(deletePointOfInvisibleWhiteSpacesAtStart.IsSet() &&
                   !pointToInsert.IsSetAndValidInComposedDoc())) {
      return Err(NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE);
    }
    // If we're starting composition, we won't normalizing surrounding
    // white-spaces until end of the composition.  Additionally, at that time,
    // we need to assume all white-spaces of surrounding white-spaces are
    // visible because canceling composition may cause previous white-space
    // invisible temporarily. Therefore, we should normalize surrounding
    // white-spaces to delete invisible white-spaces contained in the sequence.
    // E.g., `NBSP SP SP NBSP`, in this case, one of the SP is invisible.
    if (EditorBase::InsertingTextForStartingComposition(aPurpose) &&
        pointToInsert.IsInTextNode()) {
      const auto whiteSpaceOffset = [&]() -> Maybe<uint32_t> {
        if (!pointToInsert.IsEndOfContainer() &&
            pointToInsert.IsCharCollapsibleASCIISpaceOrNBSP()) {
          return Some(pointToInsert.Offset());
        }
        if (!pointToInsert.IsStartOfContainer() &&
            pointToInsert.IsPreviousCharCollapsibleASCIISpaceOrNBSP()) {
          return Some(pointToInsert.Offset() - 1u);
        }
        return Nothing();
      }();
      if (whiteSpaceOffset.isSome()) {
        Maybe<AutoTrackDOMPoint> trackPointToInsert;
        if (pointToInsert.Offset() != *whiteSpaceOffset) {
          trackPointToInsert.emplace(aHTMLEditor.RangeUpdaterRef(),
                                     &pointToInsert);
        }
        Result<EditorDOMPoint, nsresult> pointToInsertOrError =
            WhiteSpaceVisibilityKeeper::NormalizeWhiteSpacesAt(
                aHTMLEditor,
                EditorDOMPointInText(pointToInsert.ContainerAs<Text>(),
                                     *whiteSpaceOffset));
        if (MOZ_UNLIKELY(pointToInsertOrError.isErr())) {
          NS_WARNING(
              "WhiteSpaceVisibilityKeeper::NormalizeWhiteSpacesAt() failed");
          return pointToInsertOrError.propagateErr();
        }
        if (trackPointToInsert.isSome()) {
          trackPointToInsert.reset();
        } else {
          pointToInsert = pointToInsertOrError.unwrap();
        }
        if (NS_WARN_IF(!pointToInsert.IsInContentNodeAndValidInComposedDoc())) {
          return Err(NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE);
        }
      }
    }
  }

  if (NS_WARN_IF(!pointToInsert.IsInContentNode())) {
    return Err(NS_ERROR_FAILURE);
  }

  const HTMLEditor::NormalizedStringToInsertText insertTextData =
      [&]() MOZ_NEVER_INLINE_DEBUG {
        if (!isWhiteSpaceCollapsible) {
          return HTMLEditor::NormalizedStringToInsertText(aStringToInsert,
                                                          pointToInsert);
        }
        if (pointToInsert.IsInTextNode() &&
            !EditorBase::InsertingTextForComposition(aPurpose)) {
          // If normalizing the surrounding white-spaces in the `Text`, we
          // should minimize the replacing range to avoid to unnecessary
          // replacement.
          return aHTMLEditor
              .NormalizeWhiteSpacesToInsertText(
                  pointToInsert, aStringToInsert,
                  HTMLEditor::NormalizeSurroundingWhiteSpaces::Yes)
              .GetMinimizedData(*pointToInsert.ContainerAs<Text>());
        }
        return aHTMLEditor.NormalizeWhiteSpacesToInsertText(
            pointToInsert, aStringToInsert,
            // If we're handling composition string, we should not replace
            // surrounding white-spaces to avoid to make
            // CompositionTransaction confused.
            EditorBase::InsertingTextForComposition(aPurpose)
                ? HTMLEditor::NormalizeSurroundingWhiteSpaces::No
                : HTMLEditor::NormalizeSurroundingWhiteSpaces::Yes);
      }();

  MOZ_ASSERT_IF(insertTextData.ReplaceLength(), pointToInsert.IsInTextNode());
  Result<InsertTextResult, nsresult> insertOrReplaceTextResultOrError =
      aHTMLEditor.InsertOrReplaceTextWithTransaction(pointToInsert,
                                                     insertTextData);
  if (MOZ_UNLIKELY(insertOrReplaceTextResultOrError.isErr())) {
    NS_WARNING("HTMLEditor::ReplaceTextWithTransaction() failed");
    return insertOrReplaceTextResultOrError;
  }
  // If the composition is committed, we should normalize surrounding
  // white-spaces of the commit string.
  if (aPurpose != InsertTextFor::CompositionEnd &&
      aPurpose != InsertTextFor::CompositionStartAndEnd) {
    return insertOrReplaceTextResultOrError;
  }
  InsertTextResult insertOrReplaceTextResult =
      insertOrReplaceTextResultOrError.unwrap();
  const EditorDOMPointInText endOfCommitString =
      insertOrReplaceTextResult.EndOfInsertedTextRef().GetAsInText();
  if (!endOfCommitString.IsSet() || endOfCommitString.IsContainerEmpty()) {
    return std::move(insertOrReplaceTextResult);
  }
  if (NS_WARN_IF(endOfCommitString.Offset() <
                 insertTextData.mNormalizedString.Length())) {
    insertOrReplaceTextResult.IgnoreCaretPointSuggestion();
    return Err(NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE);
  }
  const EditorDOMPointInText startOfCommitString(
      endOfCommitString.ContainerAs<Text>(),
      endOfCommitString.Offset() - insertTextData.mNormalizedString.Length());
  MOZ_ASSERT(insertOrReplaceTextResult.EndOfInsertedTextRef() ==
             insertOrReplaceTextResult.CaretPointRef());
  EditorDOMPoint pointToPutCaret = insertOrReplaceTextResult.UnwrapCaretPoint();
  // First, normalize the trailing white-spaces if there is.  Note that its
  // sequence may start from before the commit string.  In such case, the
  // another call of NormalizeWhiteSpacesAt() won't update the DOM.
  if (endOfCommitString.IsMiddleOfContainer()) {
    nsresult rv = WhiteSpaceVisibilityKeeper::
        NormalizeVisibleWhiteSpacesWithoutDeletingInvisibleWhiteSpaces(
            aHTMLEditor, endOfCommitString.PreviousPoint());
    if (NS_FAILED(rv)) {
      NS_WARNING(
          "WhiteSpaceVisibilityKeeper::"
          "NormalizeVisibleWhiteSpacesWithoutDeletingInvisibleWhiteSpaces() "
          "failed");
      return Err(rv);
    }
    if (NS_WARN_IF(!pointToPutCaret.IsSetAndValidInComposedDoc())) {
      return Err(NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE);
    }
  }
  // Finally, normalize the leading white-spaces if there is and not a part of
  // the trailing white-spaces.
  if (!startOfCommitString.IsStartOfContainer()) {
    nsresult rv = WhiteSpaceVisibilityKeeper::
        NormalizeVisibleWhiteSpacesWithoutDeletingInvisibleWhiteSpaces(
            aHTMLEditor, startOfCommitString.PreviousPoint());
    if (NS_FAILED(rv)) {
      NS_WARNING(
          "WhiteSpaceVisibilityKeeper::"
          "NormalizeVisibleWhiteSpacesWithoutDeletingInvisibleWhiteSpaces() "
          "failed");
      return Err(rv);
    }
    if (NS_WARN_IF(!pointToPutCaret.IsSetAndValidInComposedDoc())) {
      return Err(NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE);
    }
  }
  EditorDOMPoint endOfCommitStringAfterNormalized = pointToPutCaret;
  return InsertTextResult(std::move(endOfCommitStringAfterNormalized),
                          CaretPoint(std::move(pointToPutCaret)));
}

// static
nsresult WhiteSpaceVisibilityKeeper::
    NormalizeVisibleWhiteSpacesWithoutDeletingInvisibleWhiteSpaces(
        HTMLEditor& aHTMLEditor, const EditorDOMPointInText& aPoint) {
  MOZ_ASSERT(StaticPrefs::editor_white_space_normalization_blink_compatible());
  MOZ_ASSERT(aPoint.IsSet());
  MOZ_ASSERT(!aPoint.IsEndOfContainer());

  if (EditorUtils::IsWhiteSpacePreformatted(*aPoint.ContainerAs<Text>())) {
    return NS_OK;
  }
  Text& textNode = *aPoint.ContainerAs<Text>();
  const bool isNewLinePreformatted =
      EditorUtils::IsNewLinePreformatted(textNode);
  const auto IsCollapsibleChar = [&](char16_t aChar) {
    return aChar == HTMLEditUtils::kNewLine ? !isNewLinePreformatted
                                            : nsCRT::IsAsciiSpace(aChar);
  };
  const auto IsCollapsibleCharOrNBSP = [&](char16_t aChar) {
    return aChar == HTMLEditUtils::kNBSP || IsCollapsibleChar(aChar);
  };
  const auto whiteSpaceOffset = [&]() -> Maybe<uint32_t> {
    if (IsCollapsibleCharOrNBSP(aPoint.Char())) {
      return Some(aPoint.Offset());
    }
    if (!aPoint.IsAtLastContent() &&
        IsCollapsibleCharOrNBSP(aPoint.NextChar())) {
      return Some(aPoint.Offset() + 1u);
    }
    return Nothing();
  }();
  if (whiteSpaceOffset.isNothing()) {
    return NS_OK;
  }
  const uint32_t firstOffset = [&]() {
    for (const uint32_t offset : Reversed(IntegerRange(*whiteSpaceOffset))) {
      if (!IsCollapsibleCharOrNBSP(textNode.TextFragment().CharAt(offset))) {
        return offset + 1u;
      }
    }
    return 0u;
  }();
  const uint32_t endOffset = [&]() {
    for (const uint32_t offset :
         IntegerRange(*whiteSpaceOffset + 1, textNode.TextDataLength())) {
      if (!IsCollapsibleCharOrNBSP(textNode.TextFragment().CharAt(offset))) {
        return offset;
      }
    }
    return textNode.TextDataLength();
  }();
  nsAutoString normalizedString;
  const char16_t precedingChar =
      !firstOffset ? static_cast<char16_t>(0)
                   : textNode.TextFragment().CharAt(firstOffset - 1u);
  const char16_t followingChar =
      endOffset == textNode.TextDataLength()
          ? static_cast<char16_t>(0)
          : textNode.TextFragment().CharAt(endOffset);
  HTMLEditor::GenerateWhiteSpaceSequence(
      normalizedString, endOffset - firstOffset,
      !firstOffset ? HTMLEditor::CharPointData::InSameTextNode(
                         HTMLEditor::CharPointType::TextEnd)
                   : HTMLEditor::CharPointData::InSameTextNode(
                         precedingChar == HTMLEditUtils::kNewLine
                             ? HTMLEditor::CharPointType::PreformattedLineBreak
                             : HTMLEditor::CharPointType::VisibleChar),
      endOffset == textNode.TextDataLength()
          ? HTMLEditor::CharPointData::InSameTextNode(
                HTMLEditor::CharPointType::TextEnd)
          : HTMLEditor::CharPointData::InSameTextNode(
                followingChar == HTMLEditUtils::kNewLine
                    ? HTMLEditor::CharPointType::PreformattedLineBreak
                    : HTMLEditor::CharPointType::VisibleChar));
  MOZ_ASSERT(normalizedString.Length() == endOffset - firstOffset);
  const OwningNonNull<Text> text(textNode);
  Result<InsertTextResult, nsresult> normalizeWhiteSpaceSequenceResultOrError =
      aHTMLEditor.ReplaceTextWithTransaction(
          text, firstOffset, endOffset - firstOffset, normalizedString);
  if (MOZ_UNLIKELY(normalizeWhiteSpaceSequenceResultOrError.isErr())) {
    NS_WARNING("HTMLEditor::ReplaceTextWithTransaction() failed");
    return normalizeWhiteSpaceSequenceResultOrError.unwrapErr();
  }
  normalizeWhiteSpaceSequenceResultOrError.unwrap()
      .IgnoreCaretPointSuggestion();
  return NS_OK;
}

// static
Result<CaretPoint, nsresult>
WhiteSpaceVisibilityKeeper::DeletePreviousWhiteSpace(
    HTMLEditor& aHTMLEditor, const EditorDOMPoint& aPoint,
    const Element& aEditingHost) {
  MOZ_ASSERT(!StaticPrefs::editor_white_space_normalization_blink_compatible());

  const TextFragmentData textFragmentDataAtDeletion(
      Scan::EditableNodes, aPoint, BlockInlineCheck::UseComputedDisplayStyle);
  if (NS_WARN_IF(!textFragmentDataAtDeletion.IsInitialized())) {
    return Err(NS_ERROR_FAILURE);
  }
  const auto atPreviousCharOfStart =
      textFragmentDataAtDeletion.GetPreviousCharPoint<EditorDOMPointInText>(
          aPoint, IgnoreNonEditableNodes::Yes);
  if (!atPreviousCharOfStart.IsSet() ||
      atPreviousCharOfStart.IsEndOfContainer()) {
    return CaretPoint(EditorDOMPoint());
  }

  // If the char is a collapsible white-space or a non-collapsible new line
  // but it can collapse adjacent white-spaces, we need to extend the range
  // to delete all invisible white-spaces.
  if (atPreviousCharOfStart.IsCharCollapsibleASCIISpace() ||
      atPreviousCharOfStart
          .IsCharPreformattedNewLineCollapsedWithWhiteSpaces()) {
    auto startToDelete =
        textFragmentDataAtDeletion
            .GetFirstASCIIWhiteSpacePointCollapsedTo<EditorDOMPoint>(
                atPreviousCharOfStart, nsIEditor::ePrevious,
                // XXX Shouldn't be "No"?  Skipping non-editable nodes may have
                // visible content.
                IgnoreNonEditableNodes::Yes);
    auto endToDelete = textFragmentDataAtDeletion
                           .GetEndOfCollapsibleASCIIWhiteSpaces<EditorDOMPoint>(
                               atPreviousCharOfStart, nsIEditor::ePrevious,
                               // XXX Shouldn't be "No"?  Skipping non-editable
                               // nodes may have visible content.
                               IgnoreNonEditableNodes::Yes);
    EditorDOMPoint pointToPutCaret;
    {
      Result<CaretPoint, nsresult> caretPointOrError =
          WhiteSpaceVisibilityKeeper::PrepareToDeleteRangeAndTrackPoints(
              aHTMLEditor, &startToDelete, &endToDelete, aEditingHost);
      if (MOZ_UNLIKELY(caretPointOrError.isErr())) {
        NS_WARNING(
            "WhiteSpaceVisibilityKeeper::PrepareToDeleteRangeAndTrackPoints() "
            "failed");
        return caretPointOrError;
      }
      caretPointOrError.unwrap().MoveCaretPointTo(
          pointToPutCaret, {SuggestCaret::OnlyIfHasSuggestion});
    }

    {
      AutoTrackDOMPoint trackPointToPutCaret(aHTMLEditor.RangeUpdaterRef(),
                                             &pointToPutCaret);
      Result<CaretPoint, nsresult> caretPointOrError =
          aHTMLEditor.DeleteTextAndTextNodesWithTransaction(
              startToDelete, endToDelete,
              HTMLEditor::TreatEmptyTextNodes::
                  KeepIfContainerOfRangeBoundaries);
      if (MOZ_UNLIKELY(caretPointOrError.isErr())) {
        NS_WARNING(
            "HTMLEditor::DeleteTextAndTextNodesWithTransaction() failed");
        return caretPointOrError;
      }
      trackPointToPutCaret.FlushAndStopTracking();
      caretPointOrError.unwrap().MoveCaretPointTo(
          pointToPutCaret, {SuggestCaret::OnlyIfHasSuggestion});
    }
    return CaretPoint(std::move(pointToPutCaret));
  }

  if (atPreviousCharOfStart.IsCharCollapsibleNBSP()) {
    auto startToDelete = atPreviousCharOfStart.To<EditorDOMPoint>();
    auto endToDelete = startToDelete.NextPoint<EditorDOMPoint>();
    EditorDOMPoint pointToPutCaret;
    {
      Result<CaretPoint, nsresult> caretPointOrError =
          WhiteSpaceVisibilityKeeper::PrepareToDeleteRangeAndTrackPoints(
              aHTMLEditor, &startToDelete, &endToDelete, aEditingHost);
      if (MOZ_UNLIKELY(caretPointOrError.isErr())) {
        NS_WARNING(
            "WhiteSpaceVisibilityKeeper::PrepareToDeleteRangeAndTrackPoints() "
            "failed");
        return caretPointOrError;
      }
      caretPointOrError.unwrap().MoveCaretPointTo(
          pointToPutCaret, {SuggestCaret::OnlyIfHasSuggestion});
    }

    {
      AutoTrackDOMPoint trackPointToPutCaret(aHTMLEditor.RangeUpdaterRef(),
                                             &pointToPutCaret);
      Result<CaretPoint, nsresult> caretPointOrError =
          aHTMLEditor.DeleteTextAndTextNodesWithTransaction(
              startToDelete, endToDelete,
              HTMLEditor::TreatEmptyTextNodes::
                  KeepIfContainerOfRangeBoundaries);
      if (MOZ_UNLIKELY(caretPointOrError.isErr())) {
        NS_WARNING(
            "HTMLEditor::DeleteTextAndTextNodesWithTransaction() failed");
        return caretPointOrError;
      }
      trackPointToPutCaret.FlushAndStopTracking();
      caretPointOrError.unwrap().MoveCaretPointTo(
          pointToPutCaret, {SuggestCaret::OnlyIfHasSuggestion});
    }
    return CaretPoint(std::move(pointToPutCaret));
  }

  Result<CaretPoint, nsresult> caretPointOrError =
      aHTMLEditor.DeleteTextAndTextNodesWithTransaction(
          atPreviousCharOfStart, atPreviousCharOfStart.NextPoint(),
          HTMLEditor::TreatEmptyTextNodes::KeepIfContainerOfRangeBoundaries);
  NS_WARNING_ASSERTION(
      caretPointOrError.isOk(),
      "HTMLEditor::DeleteTextAndTextNodesWithTransaction() failed");
  return caretPointOrError;
}

// static
Result<CaretPoint, nsresult>
WhiteSpaceVisibilityKeeper::DeleteInclusiveNextWhiteSpace(
    HTMLEditor& aHTMLEditor, const EditorDOMPoint& aPoint,
    const Element& aEditingHost) {
  MOZ_ASSERT(!StaticPrefs::editor_white_space_normalization_blink_compatible());

  const TextFragmentData textFragmentDataAtDeletion(
      Scan::EditableNodes, aPoint, BlockInlineCheck::UseComputedDisplayStyle);
  if (NS_WARN_IF(!textFragmentDataAtDeletion.IsInitialized())) {
    return Err(NS_ERROR_FAILURE);
  }
  const auto atNextCharOfStart =
      textFragmentDataAtDeletion
          .GetInclusiveNextCharPoint<EditorDOMPointInText>(
              aPoint, IgnoreNonEditableNodes::Yes);
  if (!atNextCharOfStart.IsSet() || atNextCharOfStart.IsEndOfContainer()) {
    return CaretPoint(EditorDOMPoint());
  }

  // If the char is a collapsible white-space or a non-collapsible new line
  // but it can collapse adjacent white-spaces, we need to extend the range
  // to delete all invisible white-spaces.
  if (atNextCharOfStart.IsCharCollapsibleASCIISpace() ||
      atNextCharOfStart.IsCharPreformattedNewLineCollapsedWithWhiteSpaces()) {
    auto startToDelete =
        textFragmentDataAtDeletion
            .GetFirstASCIIWhiteSpacePointCollapsedTo<EditorDOMPoint>(
                atNextCharOfStart, nsIEditor::eNext,
                IgnoreNonEditableNodes::Yes);
    auto endToDelete = textFragmentDataAtDeletion
                           .GetEndOfCollapsibleASCIIWhiteSpaces<EditorDOMPoint>(
                               atNextCharOfStart, nsIEditor::eNext,
                               IgnoreNonEditableNodes::Yes);
    EditorDOMPoint pointToPutCaret;
    {
      Result<CaretPoint, nsresult> caretPointOrError =
          WhiteSpaceVisibilityKeeper::PrepareToDeleteRangeAndTrackPoints(
              aHTMLEditor, &startToDelete, &endToDelete, aEditingHost);
      if (MOZ_UNLIKELY(caretPointOrError.isErr())) {
        NS_WARNING(
            "WhiteSpaceVisibilityKeeper::PrepareToDeleteRangeAndTrackPoints() "
            "failed");
        return caretPointOrError;
      }
      caretPointOrError.unwrap().MoveCaretPointTo(
          pointToPutCaret, {SuggestCaret::OnlyIfHasSuggestion});
    }

    {
      AutoTrackDOMPoint trackPointToPutCaret(aHTMLEditor.RangeUpdaterRef(),
                                             &pointToPutCaret);
      Result<CaretPoint, nsresult> caretPointOrError =
          aHTMLEditor.DeleteTextAndTextNodesWithTransaction(
              startToDelete, endToDelete,
              HTMLEditor::TreatEmptyTextNodes::
                  KeepIfContainerOfRangeBoundaries);
      if (MOZ_UNLIKELY(caretPointOrError.isErr())) {
        NS_WARNING(
            "HTMLEditor::DeleteTextAndTextNodesWithTransaction() failed");
        return caretPointOrError;
      }
      trackPointToPutCaret.FlushAndStopTracking();
      caretPointOrError.unwrap().MoveCaretPointTo(
          pointToPutCaret, {SuggestCaret::OnlyIfHasSuggestion});
    }
    return CaretPoint(std::move(pointToPutCaret));
  }

  if (atNextCharOfStart.IsCharCollapsibleNBSP()) {
    auto startToDelete = atNextCharOfStart.To<EditorDOMPoint>();
    auto endToDelete = startToDelete.NextPoint<EditorDOMPoint>();
    EditorDOMPoint pointToPutCaret;
    {
      Result<CaretPoint, nsresult> caretPointOrError =
          WhiteSpaceVisibilityKeeper::PrepareToDeleteRangeAndTrackPoints(
              aHTMLEditor, &startToDelete, &endToDelete, aEditingHost);
      if (MOZ_UNLIKELY(caretPointOrError.isErr())) {
        NS_WARNING(
            "WhiteSpaceVisibilityKeeper::PrepareToDeleteRangeAndTrackPoints() "
            "failed");
        return caretPointOrError;
      }
      caretPointOrError.unwrap().MoveCaretPointTo(
          pointToPutCaret, {SuggestCaret::OnlyIfHasSuggestion});
    }

    {
      AutoTrackDOMPoint trackPointToPutCaret(aHTMLEditor.RangeUpdaterRef(),
                                             &pointToPutCaret);
      Result<CaretPoint, nsresult> caretPointOrError =
          aHTMLEditor.DeleteTextAndTextNodesWithTransaction(
              startToDelete, endToDelete,
              HTMLEditor::TreatEmptyTextNodes::
                  KeepIfContainerOfRangeBoundaries);
      if (MOZ_UNLIKELY(caretPointOrError.isErr())) {
        NS_WARNING(
            "HTMLEditor::DeleteTextAndTextNodesWithTransaction() failed");
        return caretPointOrError;
      }
      trackPointToPutCaret.FlushAndStopTracking();
      caretPointOrError.unwrap().MoveCaretPointTo(
          pointToPutCaret, {SuggestCaret::OnlyIfHasSuggestion});
    }
    return CaretPoint(std::move(pointToPutCaret));
  }

  Result<CaretPoint, nsresult> caretPointOrError =
      aHTMLEditor.DeleteTextAndTextNodesWithTransaction(
          atNextCharOfStart, atNextCharOfStart.NextPoint(),
          HTMLEditor::TreatEmptyTextNodes::KeepIfContainerOfRangeBoundaries);
  NS_WARNING_ASSERTION(
      caretPointOrError.isOk(),
      "HTMLEditor::DeleteTextAndTextNodesWithTransaction() failed");
  return caretPointOrError;
}

// static
Result<CaretPoint, nsresult>
WhiteSpaceVisibilityKeeper::DeleteContentNodeAndJoinTextNodesAroundIt(
    HTMLEditor& aHTMLEditor, nsIContent& aContentToDelete,
    const EditorDOMPoint& aCaretPoint, const Element& aEditingHost) {
  EditorDOMPoint atContent(&aContentToDelete);
  if (!atContent.IsSet()) {
    NS_WARNING("Deleting content node was an orphan node");
    return Err(NS_ERROR_FAILURE);
  }
  if (!HTMLEditUtils::IsRemovableNode(aContentToDelete)) {
    NS_WARNING("Deleting content node wasn't removable");
    return Err(NS_ERROR_FAILURE);
  }
  EditorDOMPoint pointToPutCaret(aCaretPoint);
  if (!StaticPrefs::editor_white_space_normalization_blink_compatible()) {
    AutoTrackDOMPoint trackPointToPutCaret(aHTMLEditor.RangeUpdaterRef(),
                                           &pointToPutCaret);
    Result<CaretPoint, nsresult> caretPointOrError =
        WhiteSpaceVisibilityKeeper::
            MakeSureToKeepVisibleStateOfWhiteSpacesAroundDeletingRange(
                aHTMLEditor, EditorDOMRange(atContent, atContent.NextPoint()),
                aEditingHost);
    if (MOZ_UNLIKELY(caretPointOrError.isErr())) {
      NS_WARNING(
          "WhiteSpaceVisibilityKeeper::"
          "MakeSureToKeepVisibleStateOfWhiteSpacesAroundDeletingRange() "
          "failed");
      return caretPointOrError;
    }
    trackPointToPutCaret.FlushAndStopTracking();
    caretPointOrError.unwrap().MoveCaretPointTo(
        pointToPutCaret, {SuggestCaret::OnlyIfHasSuggestion});
  } else {
    MOZ_ASSERT(
        StaticPrefs::editor_white_space_normalization_blink_compatible());

    // If we're removing a block, it may be surrounded by invisible
    // white-spaces.  We should remove them to avoid to make them accidentally
    // visible.
    if (HTMLEditUtils::IsBlockElement(
            aContentToDelete,
            BlockInlineCheck::UseComputedDisplayOutsideStyle)) {
      AutoTrackDOMPoint trackAtContent(aHTMLEditor.RangeUpdaterRef(),
                                       &atContent);
      {
        AutoTrackDOMPoint trackPointToPutCaret(aHTMLEditor.RangeUpdaterRef(),
                                               &pointToPutCaret);
        nsresult rv =
            WhiteSpaceVisibilityKeeper::EnsureNoInvisibleWhiteSpacesBefore(
                aHTMLEditor, EditorDOMPoint(aContentToDelete.AsElement()));
        if (NS_FAILED(rv)) {
          NS_WARNING(
              "WhiteSpaceVisibilityKeeper::EnsureNoInvisibleWhiteSpacesBefore()"
              " failed");
          return Err(rv);
        }
        if (NS_WARN_IF(!aContentToDelete.IsInComposedDoc())) {
          return Err(NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE);
        }
        rv = WhiteSpaceVisibilityKeeper::EnsureNoInvisibleWhiteSpacesAfter(
            aHTMLEditor, EditorDOMPoint::After(*aContentToDelete.AsElement()));
        if (NS_FAILED(rv)) {
          NS_WARNING(
              "WhiteSpaceVisibilityKeeper::EnsureNoInvisibleWhiteSpacesAfter() "
              "failed");
          return Err(rv);
        }
        if (NS_WARN_IF(!aContentToDelete.IsInComposedDoc())) {
          return Err(NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE);
        }
      }
      if (pointToPutCaret.IsInContentNode()) {
        // Additionally, we may put caret into the preceding block (this is the
        // case when caret was in an empty block and type `Backspace`, or when
        // caret is at end of the preceding block and type `Delete`).  In such
        // case, we need to normalize the white-space of the preceding `Text` of
        // the deleting empty block for the compatibility with the other
        // browsers.
        if (pointToPutCaret.IsBefore(EditorRawDOMPoint(&aContentToDelete))) {
          WSScanResult nextThingOfCaretPoint =
              WSRunScanner::ScanInclusiveNextVisibleNodeOrBlockBoundary(
                  Scan::All, pointToPutCaret,
                  BlockInlineCheck::UseComputedDisplayOutsideStyle);
          if (nextThingOfCaretPoint.ReachedBRElement() ||
              nextThingOfCaretPoint.ReachedPreformattedLineBreak()) {
            nextThingOfCaretPoint =
                WSRunScanner::ScanInclusiveNextVisibleNodeOrBlockBoundary(
                    Scan::All,
                    nextThingOfCaretPoint
                        .PointAfterReachedContent<EditorRawDOMPoint>(),
                    BlockInlineCheck::UseComputedDisplayOutsideStyle);
          }
          if (nextThingOfCaretPoint.ReachedBlockBoundary()) {
            const EditorDOMPoint atBlockBoundary =
                nextThingOfCaretPoint.ReachedCurrentBlockBoundary()
                    ? EditorDOMPoint::AtEndOf(
                          *nextThingOfCaretPoint.ElementPtr())
                    : EditorDOMPoint(nextThingOfCaretPoint.ElementPtr());
            Result<EditorDOMPoint, nsresult> afterLastVisibleThingOrError =
                WhiteSpaceVisibilityKeeper::NormalizeWhiteSpacesBefore(
                    aHTMLEditor, atBlockBoundary, {});
            if (MOZ_UNLIKELY(afterLastVisibleThingOrError.isErr())) {
              NS_WARNING(
                  "WhiteSpaceVisibilityKeeper::NormalizeWhiteSpacesBefore() "
                  "failed");
              return afterLastVisibleThingOrError.propagateErr();
            }
            if (NS_WARN_IF(!aContentToDelete.IsInComposedDoc())) {
              return Err(NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE);
            }
          }
        }
        // Similarly, we may put caret into the following block (this is the
        // case when caret was in an empty block and type `Delete`, or when
        // caret is at start of the following block and type `Backspace`).  In
        // such case, we need to normalize the white-space of the following
        // `Text` of the deleting empty block for the compatibility with the
        // other browsers.
        else if (EditorRawDOMPoint::After(aContentToDelete)
                     .EqualsOrIsBefore(pointToPutCaret)) {
          const WSScanResult previousThingOfCaretPoint =
              WSRunScanner::ScanPreviousVisibleNodeOrBlockBoundary(
                  Scan::All, pointToPutCaret,
                  BlockInlineCheck::UseComputedDisplayOutsideStyle);
          if (previousThingOfCaretPoint.ReachedBlockBoundary()) {
            const EditorDOMPoint atBlockBoundary =
                previousThingOfCaretPoint.ReachedCurrentBlockBoundary()
                    ? EditorDOMPoint(previousThingOfCaretPoint.ElementPtr(), 0u)
                    : EditorDOMPoint(previousThingOfCaretPoint.ElementPtr());
            Result<EditorDOMPoint, nsresult> atFirstVisibleThingOrError =
                WhiteSpaceVisibilityKeeper::NormalizeWhiteSpacesAfter(
                    aHTMLEditor, atBlockBoundary, {});
            if (MOZ_UNLIKELY(atFirstVisibleThingOrError.isErr())) {
              NS_WARNING(
                  "WhiteSpaceVisibilityKeeper::NormalizeWhiteSpacesAfter() "
                  "failed");
              return atFirstVisibleThingOrError.propagateErr();
            }
            if (NS_WARN_IF(!aContentToDelete.IsInComposedDoc())) {
              return Err(NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE);
            }
          }
        }
      }
      trackAtContent.FlushAndStopTracking();
      if (NS_WARN_IF(!atContent.IsInContentNodeAndValidInComposedDoc())) {
        return Err(NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE);
      }
    }
    // If we're deleting inline content which is not followed by visible
    // content, i.e., the preceding text will become the last Text node, we
    // should normalize the preceding white-spaces for compatibility with the
    // other browsers.
    else {
      const WSScanResult nextThing =
          WSRunScanner::ScanInclusiveNextVisibleNodeOrBlockBoundary(
              Scan::All, EditorRawDOMPoint::After(aContentToDelete),
              BlockInlineCheck::UseComputedDisplayOutsideStyle);
      if (nextThing.ReachedLineBoundary()) {
        AutoTrackDOMPoint trackAtContent(aHTMLEditor.RangeUpdaterRef(),
                                         &atContent);
        Result<EditorDOMPoint, nsresult> afterLastVisibleThingOrError =
            WhiteSpaceVisibilityKeeper::NormalizeWhiteSpacesBefore(
                aHTMLEditor, atContent, {});
        if (MOZ_UNLIKELY(afterLastVisibleThingOrError.isErr())) {
          NS_WARNING(
              "WhiteSpaceVisibilityKeeper::NormalizeWhiteSpacesBefore() "
              "failed");
          return afterLastVisibleThingOrError.propagateErr();
        }
        trackAtContent.FlushAndStopTracking();
        if (NS_WARN_IF(!atContent.IsInContentNodeAndValidInComposedDoc())) {
          return Err(NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE);
        }
      }
    }

    // Finally, we should normalize the following white-spaces for compatibility
    // with the other browsers.
    {
      AutoTrackDOMPoint trackAtContent(aHTMLEditor.RangeUpdaterRef(),
                                       &atContent);
      Result<EditorDOMPoint, nsresult> atFirstVisibleThingOrError =
          WhiteSpaceVisibilityKeeper::NormalizeWhiteSpacesAfter(
              aHTMLEditor, atContent.NextPoint(), {});
      if (MOZ_UNLIKELY(atFirstVisibleThingOrError.isErr())) {
        NS_WARNING(
            "WhiteSpaceVisibilityKeeper::NormalizeWhiteSpacesBefore() failed");
        return atFirstVisibleThingOrError.propagateErr();
      }
      trackAtContent.FlushAndStopTracking();
      if (NS_WARN_IF(!atContent.IsInContentNodeAndValidInComposedDoc())) {
        return Err(NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE);
      }
    }
  }

  nsCOMPtr<nsIContent> previousEditableSibling =
      HTMLEditUtils::GetPreviousSibling(
          aContentToDelete, {WalkTreeOption::IgnoreNonEditableNode});
  // Delete the node, and join like nodes if appropriate
  {
    AutoTrackDOMPoint trackPointToPutCaret(aHTMLEditor.RangeUpdaterRef(),
                                           &pointToPutCaret);
    nsresult rv = aHTMLEditor.DeleteNodeWithTransaction(aContentToDelete);
    if (NS_FAILED(rv)) {
      NS_WARNING("EditorBase::DeleteNodeWithTransaction() failed");
      return Err(rv);
    }
  }

  // Are they both text nodes?  If so, join them!
  // XXX This may cause odd behavior if there is non-editable nodes
  //     around the atomic content.
  if (!aCaretPoint.IsInTextNode() || !previousEditableSibling ||
      !previousEditableSibling->IsText()) {
    return CaretPoint(std::move(pointToPutCaret));
  }

  nsIContent* nextEditableSibling = HTMLEditUtils::GetNextSibling(
      *previousEditableSibling, {WalkTreeOption::IgnoreNonEditableNode});
  if (aCaretPoint.GetContainer() != nextEditableSibling) {
    return CaretPoint(std::move(pointToPutCaret));
  }

  if (!StaticPrefs::editor_white_space_normalization_blink_compatible()) {
    nsresult rv = aHTMLEditor.JoinNearestEditableNodesWithTransaction(
        *previousEditableSibling,
        MOZ_KnownLive(*aCaretPoint.ContainerAs<Text>()), &pointToPutCaret);
    if (NS_FAILED(rv)) {
      NS_WARNING(
          "HTMLEditor::JoinNearestEditableNodesWithTransaction() failed");
      return Err(rv);
    }
    if (!pointToPutCaret.IsSet()) {
      NS_WARNING(
          "HTMLEditor::JoinNearestEditableNodesWithTransaction() didn't return "
          "right node position");
      return Err(NS_ERROR_FAILURE);
    }
    return CaretPoint(std::move(pointToPutCaret));
  }
  Result<JoinNodesResult, nsresult> joinTextNodesResultOrError =
      aHTMLEditor.JoinTextNodesWithNormalizeWhiteSpaces(
          MOZ_KnownLive(*previousEditableSibling->AsText()),
          MOZ_KnownLive(*aCaretPoint.ContainerAs<Text>()));
  if (MOZ_UNLIKELY(joinTextNodesResultOrError.isErr())) {
    NS_WARNING("HTMLEditor::JoinTextNodesWithNormalizeWhiteSpaces() failed");
    return joinTextNodesResultOrError.propagateErr();
  }
  return CaretPoint(
      joinTextNodesResultOrError.unwrap().AtJoinedPoint<EditorDOMPoint>());
}

// static
Result<CaretPoint, nsresult> WhiteSpaceVisibilityKeeper::
    MakeSureToKeepVisibleStateOfWhiteSpacesAroundDeletingRange(
        HTMLEditor& aHTMLEditor, const EditorDOMRange& aRangeToDelete,
        const Element& aEditingHost) {
  MOZ_ASSERT(!StaticPrefs::editor_white_space_normalization_blink_compatible());

  if (NS_WARN_IF(!aRangeToDelete.IsPositionedAndValid()) ||
      NS_WARN_IF(!aRangeToDelete.IsInContentNodes())) {
    return Err(NS_ERROR_INVALID_ARG);
  }

  EditorDOMRange rangeToDelete(aRangeToDelete);
  bool mayBecomeUnexpectedDOMTree = aHTMLEditor.MayHaveMutationEventListeners(
      NS_EVENT_BITS_MUTATION_SUBTREEMODIFIED |
      NS_EVENT_BITS_MUTATION_NODEREMOVED |
      NS_EVENT_BITS_MUTATION_NODEREMOVEDFROMDOCUMENT |
      NS_EVENT_BITS_MUTATION_CHARACTERDATAMODIFIED);

  TextFragmentData textFragmentDataAtStart(
      Scan::EditableNodes, rangeToDelete.StartRef(),
      BlockInlineCheck::UseComputedDisplayStyle);
  if (NS_WARN_IF(!textFragmentDataAtStart.IsInitialized())) {
    return Err(NS_ERROR_FAILURE);
  }
  TextFragmentData textFragmentDataAtEnd(
      Scan::EditableNodes, rangeToDelete.EndRef(),
      BlockInlineCheck::UseComputedDisplayStyle);
  if (NS_WARN_IF(!textFragmentDataAtEnd.IsInitialized())) {
    return Err(NS_ERROR_FAILURE);
  }
  ReplaceRangeData replaceRangeDataAtEnd =
      textFragmentDataAtEnd.GetReplaceRangeDataAtEndOfDeletionRange(
          textFragmentDataAtStart);
  EditorDOMPoint pointToPutCaret;
  if (replaceRangeDataAtEnd.IsSet() && !replaceRangeDataAtEnd.Collapsed()) {
    MOZ_ASSERT(rangeToDelete.EndRef().EqualsOrIsBefore(
        replaceRangeDataAtEnd.EndRef()));
    // If there is some text after deleting range, replacing range start must
    // equal or be before end of the deleting range.
    MOZ_ASSERT_IF(rangeToDelete.EndRef().IsInTextNode() &&
                      !rangeToDelete.EndRef().IsEndOfContainer(),
                  replaceRangeDataAtEnd.StartRef().EqualsOrIsBefore(
                      rangeToDelete.EndRef()));
    // If the deleting range end is end of a text node, the replacing range
    // should:
    // - start with another node if the following text node starts with
    // white-spaces.
    // - start from prior point because end of the range may be in collapsible
    // white-spaces.
    MOZ_ASSERT_IF(rangeToDelete.EndRef().IsInTextNode() &&
                      rangeToDelete.EndRef().IsEndOfContainer(),
                  replaceRangeDataAtEnd.StartRef().EqualsOrIsBefore(
                      rangeToDelete.EndRef()) ||
                      replaceRangeDataAtEnd.StartRef().IsStartOfContainer());
    MOZ_ASSERT(rangeToDelete.StartRef().EqualsOrIsBefore(
        replaceRangeDataAtEnd.StartRef()));
    if (!replaceRangeDataAtEnd.HasReplaceString()) {
      EditorDOMPoint startToDelete(aRangeToDelete.StartRef());
      EditorDOMPoint endToDelete(replaceRangeDataAtEnd.StartRef());
      {
        AutoEditorDOMPointChildInvalidator lockOffsetOfStart(startToDelete);
        AutoEditorDOMPointChildInvalidator lockOffsetOfEnd(endToDelete);
        AutoTrackDOMPoint trackStartToDelete(aHTMLEditor.RangeUpdaterRef(),
                                             &startToDelete);
        AutoTrackDOMPoint trackEndToDelete(aHTMLEditor.RangeUpdaterRef(),
                                           &endToDelete);
        Result<CaretPoint, nsresult> caretPointOrError =
            aHTMLEditor.DeleteTextAndTextNodesWithTransaction(
                replaceRangeDataAtEnd.StartRef(),
                replaceRangeDataAtEnd.EndRef(),
                HTMLEditor::TreatEmptyTextNodes::
                    KeepIfContainerOfRangeBoundaries);
        if (MOZ_UNLIKELY(caretPointOrError.isErr())) {
          NS_WARNING(
              "HTMLEditor::DeleteTextAndTextNodesWithTransaction() failed");
          return caretPointOrError;
        }
        caretPointOrError.unwrap().MoveCaretPointTo(
            pointToPutCaret, {SuggestCaret::OnlyIfHasSuggestion});
      }
      if (mayBecomeUnexpectedDOMTree &&
          (NS_WARN_IF(!startToDelete.IsSetAndValid()) ||
           NS_WARN_IF(!endToDelete.IsSetAndValid()) ||
           NS_WARN_IF(!startToDelete.EqualsOrIsBefore(endToDelete)))) {
        return Err(NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE);
      }
      MOZ_ASSERT(startToDelete.EqualsOrIsBefore(endToDelete));
      rangeToDelete.SetStartAndEnd(startToDelete, endToDelete);
    } else {
      MOZ_ASSERT(replaceRangeDataAtEnd.RangeRef().IsInTextNodes());
      EditorDOMPoint startToDelete(aRangeToDelete.StartRef());
      EditorDOMPoint endToDelete(replaceRangeDataAtEnd.StartRef());
      {
        AutoTrackDOMPoint trackStartToDelete(aHTMLEditor.RangeUpdaterRef(),
                                             &startToDelete);
        AutoTrackDOMPoint trackEndToDelete(aHTMLEditor.RangeUpdaterRef(),
                                           &endToDelete);
        AutoTrackDOMPoint trackPointToPutCaret(aHTMLEditor.RangeUpdaterRef(),
                                               &pointToPutCaret);
        // FYI: ReplaceTextAndRemoveEmptyTextNodes() does not have any idea of
        // new caret position.
        nsresult rv =
            WhiteSpaceVisibilityKeeper::ReplaceTextAndRemoveEmptyTextNodes(
                aHTMLEditor, replaceRangeDataAtEnd.RangeRef().AsInTexts(),
                replaceRangeDataAtEnd.ReplaceStringRef());
        if (NS_FAILED(rv)) {
          NS_WARNING(
              "WhiteSpaceVisibilityKeeper::"
              "MakeSureToKeepVisibleStateOfWhiteSpacesAtEndOfDeletingRange() "
              "failed");
          return Err(rv);
        }
      }
      if (mayBecomeUnexpectedDOMTree &&
          (NS_WARN_IF(!startToDelete.IsSetAndValid()) ||
           NS_WARN_IF(!endToDelete.IsSetAndValid()) ||
           NS_WARN_IF(!startToDelete.EqualsOrIsBefore(endToDelete)))) {
        return Err(NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE);
      }
      MOZ_ASSERT(startToDelete.EqualsOrIsBefore(endToDelete));
      rangeToDelete.SetStartAndEnd(startToDelete, endToDelete);
    }

    if (mayBecomeUnexpectedDOMTree) {
      // If focus is changed by mutation event listeners, we should stop
      // handling this edit action.
      if (&aEditingHost != aHTMLEditor.ComputeEditingHost()) {
        NS_WARNING("Active editing host was changed");
        return Err(NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE);
      }
      if (!rangeToDelete.IsInContentNodes()) {
        NS_WARNING("The modified range was not in content");
        return Err(NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE);
      }
      // If the DOM tree might be changed by mutation event listeners, we
      // should retrieve the latest data for avoiding to delete/replace
      // unexpected range.
      textFragmentDataAtStart =
          TextFragmentData(Scan::EditableNodes, rangeToDelete.StartRef(),
                           BlockInlineCheck::UseComputedDisplayStyle);
      textFragmentDataAtEnd =
          TextFragmentData(Scan::EditableNodes, rangeToDelete.EndRef(),
                           BlockInlineCheck::UseComputedDisplayStyle);
    }
  }
  ReplaceRangeData replaceRangeDataAtStart =
      textFragmentDataAtStart.GetReplaceRangeDataAtStartOfDeletionRange(
          textFragmentDataAtEnd);
  if (!replaceRangeDataAtStart.IsSet() || replaceRangeDataAtStart.Collapsed()) {
    return CaretPoint(std::move(pointToPutCaret));
  }
  if (!replaceRangeDataAtStart.HasReplaceString()) {
    AutoTrackDOMPoint trackPointToPutCaret(aHTMLEditor.RangeUpdaterRef(),
                                           &pointToPutCaret);
    Result<CaretPoint, nsresult> caretPointOrError =
        aHTMLEditor.DeleteTextAndTextNodesWithTransaction(
            replaceRangeDataAtStart.StartRef(),
            replaceRangeDataAtStart.EndRef(),
            HTMLEditor::TreatEmptyTextNodes::KeepIfContainerOfRangeBoundaries);
    // XXX Should we validate the range for making this return
    //     NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE in this case?
    if (MOZ_UNLIKELY(caretPointOrError.isErr())) {
      NS_WARNING("HTMLEditor::DeleteTextAndTextNodesWithTransaction() failed");
      return caretPointOrError.propagateErr();
    }
    trackPointToPutCaret.FlushAndStopTracking();
    caretPointOrError.unwrap().MoveCaretPointTo(
        pointToPutCaret, {SuggestCaret::OnlyIfHasSuggestion});
    return CaretPoint(std::move(pointToPutCaret));
  }

  MOZ_ASSERT(replaceRangeDataAtStart.RangeRef().IsInTextNodes());
  {
    AutoTrackDOMPoint trackPointToPutCaret(aHTMLEditor.RangeUpdaterRef(),
                                           &pointToPutCaret);
    // FYI: ReplaceTextAndRemoveEmptyTextNodes() does not have any idea of
    // new caret position.
    nsresult rv =
        WhiteSpaceVisibilityKeeper::ReplaceTextAndRemoveEmptyTextNodes(
            aHTMLEditor, replaceRangeDataAtStart.RangeRef().AsInTexts(),
            replaceRangeDataAtStart.ReplaceStringRef());
    // XXX Should we validate the range for making this return
    //     NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE in this case?
    if (NS_FAILED(rv)) {
      NS_WARNING(
          "WhiteSpaceVisibilityKeeper::"
          "MakeSureToKeepVisibleStateOfWhiteSpacesAtStartOfDeletingRange() "
          "failed");
      return Err(rv);
    }
  }
  return CaretPoint(std::move(pointToPutCaret));
}

// static
nsresult
WhiteSpaceVisibilityKeeper::MakeSureToKeepVisibleWhiteSpacesVisibleAfterSplit(
    HTMLEditor& aHTMLEditor, const EditorDOMPoint& aPointToSplit) {
  MOZ_ASSERT(!StaticPrefs::editor_white_space_normalization_blink_compatible());

  const TextFragmentData textFragmentDataAtSplitPoint(
      Scan::EditableNodes, aPointToSplit,
      BlockInlineCheck::UseComputedDisplayStyle);
  if (NS_WARN_IF(!textFragmentDataAtSplitPoint.IsInitialized())) {
    return NS_ERROR_FAILURE;
  }

  // used to prepare white-space sequence to be split across two blocks.
  // The main issue here is make sure white-spaces around the split point
  // doesn't end up becoming non-significant leading or trailing ws after
  // the split.
  const VisibleWhiteSpacesData& visibleWhiteSpaces =
      textFragmentDataAtSplitPoint.VisibleWhiteSpacesDataRef();
  if (!visibleWhiteSpaces.IsInitialized()) {
    return NS_OK;  // No visible white-space sequence.
  }

  PointPosition pointPositionWithVisibleWhiteSpaces =
      visibleWhiteSpaces.ComparePoint(aPointToSplit);

  // XXX If we split white-space sequence, the following code modify the DOM
  //     tree twice.  This is not reasonable and the latter change may touch
  //     wrong position.  We should do this once.

  // If we insert block boundary to start or middle of the white-space sequence,
  // the character at the insertion point needs to be an NBSP.
  EditorDOMPoint pointToSplit(aPointToSplit);
  if (pointPositionWithVisibleWhiteSpaces == PointPosition::StartOfFragment ||
      pointPositionWithVisibleWhiteSpaces == PointPosition::MiddleOfFragment) {
    auto atNextCharOfStart =
        textFragmentDataAtSplitPoint
            .GetInclusiveNextCharPoint<EditorDOMPointInText>(
                pointToSplit, IgnoreNonEditableNodes::Yes);
    if (atNextCharOfStart.IsSet() && !atNextCharOfStart.IsEndOfContainer() &&
        atNextCharOfStart.IsCharCollapsibleASCIISpace()) {
      // pointToSplit will be referred bellow so that we need to keep
      // it a valid point.
      AutoEditorDOMPointChildInvalidator forgetChild(pointToSplit);
      AutoTrackDOMPoint trackSplitPoint(aHTMLEditor.RangeUpdaterRef(),
                                        &pointToSplit);
      if (atNextCharOfStart.IsStartOfContainer() ||
          atNextCharOfStart.IsPreviousCharASCIISpace()) {
        atNextCharOfStart =
            textFragmentDataAtSplitPoint
                .GetFirstASCIIWhiteSpacePointCollapsedTo<EditorDOMPointInText>(
                    atNextCharOfStart, nsIEditor::eNone,
                    // XXX Shouldn't be "No"?  Skipping non-editable nodes may
                    // have visible content.
                    IgnoreNonEditableNodes::Yes);
      }
      const auto endOfCollapsibleASCIIWhiteSpaces =
          textFragmentDataAtSplitPoint
              .GetEndOfCollapsibleASCIIWhiteSpaces<EditorDOMPointInText>(
                  atNextCharOfStart, nsIEditor::eNone,
                  // XXX Shouldn't be "No"?  Skipping non-editable nodes may
                  // have visible content.
                  IgnoreNonEditableNodes::Yes);
      nsresult rv =
          WhiteSpaceVisibilityKeeper::ReplaceTextAndRemoveEmptyTextNodes(
              aHTMLEditor,
              EditorDOMRangeInTexts(atNextCharOfStart,
                                    endOfCollapsibleASCIIWhiteSpaces),
              nsDependentSubstring(&HTMLEditUtils::kNBSP, 1));
      if (NS_FAILED(rv)) {
        NS_WARNING(
            "WhiteSpaceVisibilityKeeper::ReplaceTextAndRemoveEmptyTextNodes() "
            "failed");
        return rv;
      }
    }
  }

  // If we insert block boundary to middle of or end of the white-space
  // sequence, the previous character at the insertion point needs to be an
  // NBSP.
  if (pointPositionWithVisibleWhiteSpaces == PointPosition::MiddleOfFragment ||
      pointPositionWithVisibleWhiteSpaces == PointPosition::EndOfFragment) {
    auto atPreviousCharOfStart =
        textFragmentDataAtSplitPoint.GetPreviousCharPoint<EditorDOMPointInText>(
            pointToSplit, IgnoreNonEditableNodes::Yes);
    if (atPreviousCharOfStart.IsSet() &&
        !atPreviousCharOfStart.IsEndOfContainer() &&
        atPreviousCharOfStart.IsCharCollapsibleASCIISpace()) {
      if (atPreviousCharOfStart.IsStartOfContainer() ||
          atPreviousCharOfStart.IsPreviousCharASCIISpace()) {
        atPreviousCharOfStart =
            textFragmentDataAtSplitPoint
                .GetFirstASCIIWhiteSpacePointCollapsedTo<EditorDOMPointInText>(
                    atPreviousCharOfStart, nsIEditor::eNone,
                    // XXX Shouldn't be "No"?  Skipping non-editable nodes may
                    // have visible content.
                    IgnoreNonEditableNodes::Yes);
      }
      const auto endOfCollapsibleASCIIWhiteSpaces =
          textFragmentDataAtSplitPoint
              .GetEndOfCollapsibleASCIIWhiteSpaces<EditorDOMPointInText>(
                  atPreviousCharOfStart, nsIEditor::eNone,
                  // XXX Shouldn't be "No"?  Skipping non-editable nodes may
                  // have visible content.
                  IgnoreNonEditableNodes::Yes);
      nsresult rv =
          WhiteSpaceVisibilityKeeper::ReplaceTextAndRemoveEmptyTextNodes(
              aHTMLEditor,
              EditorDOMRangeInTexts(atPreviousCharOfStart,
                                    endOfCollapsibleASCIIWhiteSpaces),
              nsDependentSubstring(&HTMLEditUtils::kNBSP, 1));
      if (NS_FAILED(rv)) {
        NS_WARNING(
            "WhiteSpaceVisibilityKeeper::ReplaceTextAndRemoveEmptyTextNodes() "
            "failed");
        return rv;
      }
    }
  }
  return NS_OK;
}

// static
nsresult WhiteSpaceVisibilityKeeper::ReplaceTextAndRemoveEmptyTextNodes(
    HTMLEditor& aHTMLEditor, const EditorDOMRangeInTexts& aRangeToReplace,
    const nsAString& aReplaceString) {
  MOZ_ASSERT(aRangeToReplace.IsPositioned());
  MOZ_ASSERT(aRangeToReplace.StartRef().IsSetAndValid());
  MOZ_ASSERT(aRangeToReplace.EndRef().IsSetAndValid());
  MOZ_ASSERT(aRangeToReplace.StartRef().IsBefore(aRangeToReplace.EndRef()));

  {
    Result<InsertTextResult, nsresult> caretPointOrError =
        aHTMLEditor.ReplaceTextWithTransaction(
            MOZ_KnownLive(*aRangeToReplace.StartRef().ContainerAs<Text>()),
            aRangeToReplace.StartRef().Offset(),
            aRangeToReplace.InSameContainer()
                ? aRangeToReplace.EndRef().Offset() -
                      aRangeToReplace.StartRef().Offset()
                : aRangeToReplace.StartRef().ContainerAs<Text>()->TextLength() -
                      aRangeToReplace.StartRef().Offset(),
            aReplaceString);
    if (MOZ_UNLIKELY(caretPointOrError.isErr())) {
      NS_WARNING("HTMLEditor::ReplaceTextWithTransaction() failed");
      return caretPointOrError.unwrapErr();
    }
    // Ignore caret suggestion because there was
    // AutoTransactionsConserveSelection.
    caretPointOrError.unwrap().IgnoreCaretPointSuggestion();
  }

  if (aRangeToReplace.InSameContainer()) {
    return NS_OK;
  }

  Result<CaretPoint, nsresult> caretPointOrError =
      aHTMLEditor.DeleteTextAndTextNodesWithTransaction(
          EditorDOMPointInText::AtEndOf(
              *aRangeToReplace.StartRef().ContainerAs<Text>()),
          aRangeToReplace.EndRef(),
          HTMLEditor::TreatEmptyTextNodes::KeepIfContainerOfRangeBoundaries);
  if (MOZ_UNLIKELY(caretPointOrError.isErr())) {
    NS_WARNING("HTMLEditor::DeleteTextAndTextNodesWithTransaction() failed");
    return caretPointOrError.unwrapErr();
  }
  // Ignore caret suggestion because there was
  // AutoTransactionsConserveSelection.
  caretPointOrError.unwrap().IgnoreCaretPointSuggestion();
  return NS_OK;
}

// static
template <typename EditorDOMPointType>
nsresult WhiteSpaceVisibilityKeeper::NormalizeVisibleWhiteSpacesAt(
    HTMLEditor& aHTMLEditor, const EditorDOMPointType& aPoint,
    const Element& aEditingHost) {
  MOZ_ASSERT(!StaticPrefs::editor_white_space_normalization_blink_compatible());
  MOZ_ASSERT(aPoint.IsInContentNode());
  MOZ_ASSERT(EditorUtils::IsEditableContent(
      *aPoint.template ContainerAs<nsIContent>(), EditorType::HTML));
  const TextFragmentData textFragmentData(
      Scan::EditableNodes, aPoint, BlockInlineCheck::UseComputedDisplayStyle);
  if (NS_WARN_IF(!textFragmentData.IsInitialized())) {
    return NS_ERROR_FAILURE;
  }

  // this routine examines a run of ws and tries to get rid of some unneeded
  // nbsp's, replacing them with regular ascii space if possible.  Keeping
  // things simple for now and just trying to fix up the trailing ws in the run.
  if (!textFragmentData.FoundNoBreakingWhiteSpaces()) {
    // nothing to do!
    return NS_OK;
  }
  const VisibleWhiteSpacesData& visibleWhiteSpaces =
      textFragmentData.VisibleWhiteSpacesDataRef();
  if (!visibleWhiteSpaces.IsInitialized()) {
    return NS_OK;
  }

  // Remove this block if we ship Blink-compat white-space normalization.
  if (!StaticPrefs::editor_white_space_normalization_blink_compatible()) {
    // now check that what is to the left of it is compatible with replacing
    // nbsp with space
    const EditorDOMPoint& atEndOfVisibleWhiteSpaces =
        visibleWhiteSpaces.EndRef();
    auto atPreviousCharOfEndOfVisibleWhiteSpaces =
        textFragmentData.GetPreviousCharPoint<EditorDOMPointInText>(
            atEndOfVisibleWhiteSpaces, IgnoreNonEditableNodes::Yes);
    if (!atPreviousCharOfEndOfVisibleWhiteSpaces.IsSet() ||
        atPreviousCharOfEndOfVisibleWhiteSpaces.IsEndOfContainer() ||
        // If the NBSP is never replaced from an ASCII white-space, we cannot
        // replace it with an ASCII white-space.
        !atPreviousCharOfEndOfVisibleWhiteSpaces.IsCharCollapsibleNBSP()) {
      return NS_OK;
    }

    // now check that what is to the left of it is compatible with replacing
    // nbsp with space
    auto atPreviousCharOfPreviousCharOfEndOfVisibleWhiteSpaces =
        textFragmentData.GetPreviousCharPoint<EditorDOMPointInText>(
            atPreviousCharOfEndOfVisibleWhiteSpaces,
            IgnoreNonEditableNodes::Yes);
    bool isPreviousCharCollapsibleASCIIWhiteSpace =
        atPreviousCharOfPreviousCharOfEndOfVisibleWhiteSpaces.IsSet() &&
        !atPreviousCharOfPreviousCharOfEndOfVisibleWhiteSpaces
             .IsEndOfContainer() &&
        atPreviousCharOfPreviousCharOfEndOfVisibleWhiteSpaces
            .IsCharCollapsibleASCIISpace();
    const bool maybeNBSPFollowsVisibleContent =
        (atPreviousCharOfPreviousCharOfEndOfVisibleWhiteSpaces.IsSet() &&
         !isPreviousCharCollapsibleASCIIWhiteSpace) ||
        (!atPreviousCharOfPreviousCharOfEndOfVisibleWhiteSpaces.IsSet() &&
         (visibleWhiteSpaces.StartsFromNonCollapsibleCharacters() ||
          visibleWhiteSpaces.StartsFromSpecialContent()));
    bool followedByVisibleContent =
        visibleWhiteSpaces.EndsByNonCollapsibleCharacters() ||
        visibleWhiteSpaces.EndsBySpecialContent();
    bool followedByBRElement = visibleWhiteSpaces.EndsByBRElement();
    bool followedByPreformattedLineBreak =
        visibleWhiteSpaces.EndsByPreformattedLineBreak();

    // If the NBSP follows a visible content or a collapsible ASCII white-space,
    // i.e., unless NBSP is first character and start of a block, we may need to
    // insert <br> element and restore the NBSP to an ASCII white-space.
    if (maybeNBSPFollowsVisibleContent ||
        isPreviousCharCollapsibleASCIIWhiteSpace) {
      // First, try to insert <br> element if NBSP is at end of a block.
      // XXX We should stop this if there is a visible content.
      if ((visibleWhiteSpaces.EndsByBlockBoundary() ||
           visibleWhiteSpaces.EndsByInlineEditingHostBoundary()) &&
          aPoint.IsInContentNode()) {
        bool insertBRElement = HTMLEditUtils::IsBlockElement(
            *aPoint.template ContainerAs<nsIContent>(),
            BlockInlineCheck::UseComputedDisplayStyle);
        if (!insertBRElement) {
          NS_ASSERTION(
              EditorUtils::IsEditableContent(
                  *aPoint.template ContainerAs<nsIContent>(), EditorType::HTML),
              "Given content is not editable");
          NS_ASSERTION(aPoint.template ContainerAs<nsIContent>()
                           ->GetAsElementOrParentElement(),
                       "Given content is not an element and an orphan node");
          const Element* editableBlockElement =
              EditorUtils::IsEditableContent(
                  *aPoint.template ContainerAs<nsIContent>(), EditorType::HTML)
                  ? HTMLEditUtils::GetInclusiveAncestorElement(
                        *aPoint.template ContainerAs<nsIContent>(),
                        HTMLEditUtils::ClosestEditableBlockElement,
                        BlockInlineCheck::UseComputedDisplayStyle)
                  : nullptr;
          insertBRElement = !!editableBlockElement;
        }
        if (insertBRElement) {
          // We are at a block boundary.  Insert a <br>.  Why?  Well, first note
          // that the br will have no visible effect since it is up against a
          // block boundary.  |foo<br><p>bar| renders like |foo<p>bar| and
          // similarly |<p>foo<br></p>bar| renders like |<p>foo</p>bar|.  What
          // this <br> addition gets us is the ability to convert a trailing
          // nbsp to a space.  Consider: |<body>foo. '</body>|, where '
          // represents selection.  User types space attempting to put 2 spaces
          // after the end of their sentence.  We used to do this as:
          // |<body>foo. &nbsp</body>|  This caused problems with soft wrapping:
          // the nbsp would wrap to the next line, which looked attrocious.  If
          // you try to do: |<body>foo.&nbsp </body>| instead, the trailing
          // space is invisible because it is against a block boundary.  If you
          // do:
          // |<body>foo.&nbsp&nbsp</body>| then you get an even uglier soft
          // wrapping problem, where foo is on one line until you type the final
          // space, and then "foo  " jumps down to the next line.  Ugh.  The
          // best way I can find out of this is to throw in a harmless <br>
          // here, which allows us to do: |<body>foo.&nbsp <br></body>|, which
          // doesn't cause foo to jump lines, doesn't cause spaces to show up at
          // the beginning of soft wrapped lines, and lets the user see 2 spaces
          // when they type 2 spaces.

          if (NS_WARN_IF(!atEndOfVisibleWhiteSpaces.IsInContentNode())) {
            return Err(NS_ERROR_FAILURE);
          }
          const Maybe<LineBreakType> lineBreakType =
              aHTMLEditor.GetPreferredLineBreakType(
                  *atEndOfVisibleWhiteSpaces.ContainerAs<nsIContent>(),
                  aEditingHost);
          if (NS_WARN_IF(lineBreakType.isNothing())) {
            return Err(NS_ERROR_FAILURE);
          }
          Result<CreateLineBreakResult, nsresult> insertBRElementResultOrError =
              aHTMLEditor.InsertLineBreak(WithTransaction::Yes, *lineBreakType,
                                          atEndOfVisibleWhiteSpaces);
          if (MOZ_UNLIKELY(insertBRElementResultOrError.isErr())) {
            NS_WARNING(nsPrintfCString("HTMLEditor::InsertLineBreak("
                                       "WithTransaction::Yes, %s) failed",
                                       ToString(*lineBreakType).c_str())
                           .get());
            return insertBRElementResultOrError.propagateErr();
          }
          CreateLineBreakResult insertBRElementResult =
              insertBRElementResultOrError.unwrap();
          MOZ_ASSERT(insertBRElementResult.Handled());
          // Ignore caret suggestion because the caller must want to restore
          // `Selection` due to the purpose of this method.
          insertBRElementResult.IgnoreCaretPointSuggestion();

          atPreviousCharOfEndOfVisibleWhiteSpaces =
              textFragmentData.GetPreviousCharPoint<EditorDOMPointInText>(
                  atEndOfVisibleWhiteSpaces, IgnoreNonEditableNodes::Yes);
          if (NS_WARN_IF(!atPreviousCharOfEndOfVisibleWhiteSpaces.IsSet())) {
            return NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE;
          }
          atPreviousCharOfPreviousCharOfEndOfVisibleWhiteSpaces =
              textFragmentData.GetPreviousCharPoint<EditorDOMPointInText>(
                  atPreviousCharOfEndOfVisibleWhiteSpaces,
                  IgnoreNonEditableNodes::Yes);
          isPreviousCharCollapsibleASCIIWhiteSpace =
              atPreviousCharOfPreviousCharOfEndOfVisibleWhiteSpaces.IsSet() &&
              !atPreviousCharOfPreviousCharOfEndOfVisibleWhiteSpaces
                   .IsEndOfContainer() &&
              atPreviousCharOfPreviousCharOfEndOfVisibleWhiteSpaces
                  .IsCharCollapsibleASCIISpace();
          followedByBRElement = true;
          followedByVisibleContent = followedByPreformattedLineBreak = false;
        }
      }

      // Once insert a <br>, the remaining work is only for normalizing
      // white-space sequence in white-space collapsible text node.
      // So, if the the text node's white-spaces are preformatted, we need
      // to do nothing anymore.
      if (EditorUtils::IsWhiteSpacePreformatted(
              *atPreviousCharOfEndOfVisibleWhiteSpaces.ContainerAs<Text>())) {
        return NS_OK;
      }

      // Next, replace the NBSP with an ASCII white-space if it's surrounded
      // by visible contents (or immediately before a <br> element).
      // However, if it follows or is followed by a preformatted linefeed,
      // we shouldn't do this because an ASCII white-space will be collapsed
      // **into** the linefeed.
      if (maybeNBSPFollowsVisibleContent &&
          (followedByVisibleContent || followedByBRElement) &&
          !visibleWhiteSpaces.StartsFromPreformattedLineBreak()) {
        MOZ_ASSERT(!followedByPreformattedLineBreak);
        Result<InsertTextResult, nsresult> replaceTextResult =
            aHTMLEditor.ReplaceTextWithTransaction(
                MOZ_KnownLive(*atPreviousCharOfEndOfVisibleWhiteSpaces
                                   .ContainerAs<Text>()),
                atPreviousCharOfEndOfVisibleWhiteSpaces.Offset(), 1, u" "_ns);
        if (MOZ_UNLIKELY(replaceTextResult.isErr())) {
          NS_WARNING("HTMLEditor::ReplaceTextWithTransaction() failed");
          return replaceTextResult.propagateErr();
        }
        // Ignore caret suggestion because the caller must want to restore
        // `Selection` due to the purpose of this method.
        replaceTextResult.unwrap().IgnoreCaretPointSuggestion();
        return NS_OK;
      }
    }
    // If the text node is not preformatted, and the NBSP is followed by a <br>
    // element and following (maybe multiple) collapsible ASCII white-spaces,
    // remove the NBSP, but inserts a NBSP before the spaces.  This makes a line
    // break opportunity to wrap the line.
    // XXX This is different behavior from Blink.  Blink generates pairs of
    //     an NBSP and an ASCII white-space, but put NBSP at the end of the
    //     sequence.  We should follow the behavior for web-compat.
    if (maybeNBSPFollowsVisibleContent ||
        !isPreviousCharCollapsibleASCIIWhiteSpace ||
        !(followedByVisibleContent || followedByBRElement) ||
        EditorUtils::IsWhiteSpacePreformatted(
            *atPreviousCharOfPreviousCharOfEndOfVisibleWhiteSpaces
                 .ContainerAs<Text>())) {
      return NS_OK;
    }

    // Currently, we're at an NBSP following an ASCII space, and we need to
    // replace them with `"&nbsp; "` for avoiding collapsing white-spaces.
    MOZ_ASSERT(!atPreviousCharOfPreviousCharOfEndOfVisibleWhiteSpaces
                    .IsEndOfContainer());
    const auto atFirstASCIIWhiteSpace =
        textFragmentData
            .GetFirstASCIIWhiteSpacePointCollapsedTo<EditorDOMPointInText>(
                atPreviousCharOfPreviousCharOfEndOfVisibleWhiteSpaces,
                nsIEditor::eNone,
                // XXX Shouldn't be "No"?  Skipping non-editable nodes may have
                // visible content.
                IgnoreNonEditableNodes::Yes);
    uint32_t numberOfASCIIWhiteSpacesInStartNode =
        atFirstASCIIWhiteSpace.ContainerAs<Text>() ==
                atPreviousCharOfEndOfVisibleWhiteSpaces.ContainerAs<Text>()
            ? atPreviousCharOfEndOfVisibleWhiteSpaces.Offset() -
                  atFirstASCIIWhiteSpace.Offset()
            : atFirstASCIIWhiteSpace.ContainerAs<Text>()->Length() -
                  atFirstASCIIWhiteSpace.Offset();
    // Replace all preceding ASCII white-spaces **and** the NBSP.
    uint32_t replaceLengthInStartNode =
        numberOfASCIIWhiteSpacesInStartNode +
        (atFirstASCIIWhiteSpace.ContainerAs<Text>() ==
                 atPreviousCharOfEndOfVisibleWhiteSpaces.ContainerAs<Text>()
             ? 1
             : 0);
    Result<InsertTextResult, nsresult> replaceTextResult =
        aHTMLEditor.ReplaceTextWithTransaction(
            MOZ_KnownLive(*atFirstASCIIWhiteSpace.ContainerAs<Text>()),
            atFirstASCIIWhiteSpace.Offset(), replaceLengthInStartNode,
            textFragmentData.StartsFromPreformattedLineBreak() &&
                    textFragmentData.EndsByPreformattedLineBreak()
                ? u"\x00A0\x00A0"_ns
                : (textFragmentData.EndsByPreformattedLineBreak()
                       ? u" \x00A0"_ns
                       : u"\x00A0 "_ns));
    if (MOZ_UNLIKELY(replaceTextResult.isErr())) {
      NS_WARNING("HTMLEditor::ReplaceTextWithTransaction() failed");
      return replaceTextResult.propagateErr();
    }
    // Ignore caret suggestion because the caller must want to restore
    // `Selection` due to the purpose of this method.
    replaceTextResult.unwrap().IgnoreCaretPointSuggestion();

    if (atFirstASCIIWhiteSpace.GetContainer() ==
        atPreviousCharOfEndOfVisibleWhiteSpaces.GetContainer()) {
      return NS_OK;
    }

    // We need to remove the following unnecessary ASCII white-spaces and
    // NBSP at atPreviousCharOfEndOfVisibleWhiteSpaces because we collapsed them
    // into the start node.
    Result<CaretPoint, nsresult> caretPointOrError =
        aHTMLEditor.DeleteTextAndTextNodesWithTransaction(
            EditorDOMPointInText::AtEndOf(
                *atFirstASCIIWhiteSpace.ContainerAs<Text>()),
            atPreviousCharOfEndOfVisibleWhiteSpaces.NextPoint(),
            HTMLEditor::TreatEmptyTextNodes::KeepIfContainerOfRangeBoundaries);
    if (MOZ_UNLIKELY(caretPointOrError.isErr())) {
      NS_WARNING("HTMLEditor::DeleteTextAndTextNodesWithTransaction() failed");
      return caretPointOrError.propagateErr();
    }
    // Ignore caret suggestion because the caller must want to restore
    // `Selection` due to the purpose of this method.    }
    caretPointOrError.unwrap().IgnoreCaretPointSuggestion();
    return NS_OK;
  }

  // XXX This is called when top-level edit sub-action handling ends for
  //     3 points at most.  However, this is not compatible with Blink.
  //     Blink touches white-space sequence which includes new character
  //     or following white-space sequence of new <br> element or, if and
  //     only if deleting range is followed by white-space sequence (i.e.,
  //     not touched previous white-space sequence of deleting range).
  //     This should be done when we change to make each edit action
  //     handler directly normalize white-space sequence rather than
  //     OnEndHandlingTopLevelEditSucAction().

  // First, check if the last character is an NBSP.  Otherwise, we don't need
  // to do nothing here.
  const EditorDOMPoint& atEndOfVisibleWhiteSpaces = visibleWhiteSpaces.EndRef();
  const auto atPreviousCharOfEndOfVisibleWhiteSpaces =
      textFragmentData.GetPreviousCharPoint<EditorDOMPointInText>(
          atEndOfVisibleWhiteSpaces, IgnoreNonEditableNodes::Yes);
  if (!atPreviousCharOfEndOfVisibleWhiteSpaces.IsSet() ||
      atPreviousCharOfEndOfVisibleWhiteSpaces.IsEndOfContainer() ||
      !atPreviousCharOfEndOfVisibleWhiteSpaces.IsCharCollapsibleNBSP() ||
      // If the next character of the NBSP is a preformatted linefeed, we
      // shouldn't replace it with an ASCII white-space for avoiding collapsed
      // into the linefeed.
      visibleWhiteSpaces.EndsByPreformattedLineBreak()) {
    return NS_OK;
  }

  // Next, consider the range to collapse ASCII white-spaces before there.
  EditorDOMPointInText startToDelete, endToDelete;

  const auto atPreviousCharOfPreviousCharOfEndOfVisibleWhiteSpaces =
      textFragmentData.GetPreviousCharPoint<EditorDOMPointInText>(
          atPreviousCharOfEndOfVisibleWhiteSpaces, IgnoreNonEditableNodes::Yes);
  // If there are some preceding ASCII white-spaces, we need to treat them
  // as one white-space.  I.e., we need to collapse them.
  if (atPreviousCharOfEndOfVisibleWhiteSpaces.IsCharNBSP() &&
      atPreviousCharOfPreviousCharOfEndOfVisibleWhiteSpaces.IsSet() &&
      atPreviousCharOfPreviousCharOfEndOfVisibleWhiteSpaces
          .IsCharCollapsibleASCIISpace()) {
    startToDelete =
        textFragmentData
            .GetFirstASCIIWhiteSpacePointCollapsedTo<EditorDOMPointInText>(
                atPreviousCharOfPreviousCharOfEndOfVisibleWhiteSpaces,
                nsIEditor::eNone,
                // XXX Shouldn't be "No"?  Skipping non-editable nodes may have
                // visible content.
                IgnoreNonEditableNodes::Yes);
    endToDelete = atPreviousCharOfPreviousCharOfEndOfVisibleWhiteSpaces;
  }
  // Otherwise, we don't need to remove any white-spaces, but we may need
  // to normalize the white-space sequence containing the previous NBSP.
  else {
    startToDelete = endToDelete =
        atPreviousCharOfEndOfVisibleWhiteSpaces.NextPoint();
  }

  Result<CaretPoint, nsresult> caretPointOrError =
      aHTMLEditor.DeleteTextAndNormalizeSurroundingWhiteSpaces(
          startToDelete, endToDelete,
          HTMLEditor::TreatEmptyTextNodes::KeepIfContainerOfRangeBoundaries,
          HTMLEditor::DeleteDirection::Forward, aEditingHost);
  if (MOZ_UNLIKELY(caretPointOrError.isErr())) {
    NS_WARNING(
        "HTMLEditor::DeleteTextAndNormalizeSurroundingWhiteSpace() failed");
    return caretPointOrError.unwrapErr();
  }
  // Ignore caret suggestion because the caller must want to restore
  // `Selection` due to the purpose of this method.
  caretPointOrError.unwrap().IgnoreCaretPointSuggestion();
  return NS_OK;
}

// static
Result<CaretPoint, nsresult>
WhiteSpaceVisibilityKeeper::DeleteInvisibleASCIIWhiteSpaces(
    HTMLEditor& aHTMLEditor, const EditorDOMPoint& aPoint) {
  MOZ_ASSERT(aPoint.IsSet());
  const TextFragmentData textFragmentData(
      Scan::EditableNodes, aPoint, BlockInlineCheck::UseComputedDisplayStyle);
  if (NS_WARN_IF(!textFragmentData.IsInitialized())) {
    return Err(NS_ERROR_FAILURE);
  }
  const EditorDOMRange& leadingWhiteSpaceRange =
      textFragmentData.InvisibleLeadingWhiteSpaceRangeRef();
  // XXX Getting trailing white-space range now must be wrong because
  //     mutation event listener may invalidate it.
  const EditorDOMRange& trailingWhiteSpaceRange =
      textFragmentData.InvisibleTrailingWhiteSpaceRangeRef();
  EditorDOMPoint pointToPutCaret;
  DebugOnly<bool> leadingWhiteSpacesDeleted = false;
  if (leadingWhiteSpaceRange.IsPositioned() &&
      !leadingWhiteSpaceRange.Collapsed()) {
    Result<CaretPoint, nsresult> caretPointOrError =
        aHTMLEditor.DeleteTextAndTextNodesWithTransaction(
            leadingWhiteSpaceRange.StartRef(), leadingWhiteSpaceRange.EndRef(),
            HTMLEditor::TreatEmptyTextNodes::KeepIfContainerOfRangeBoundaries);
    if (MOZ_UNLIKELY(caretPointOrError.isErr())) {
      NS_WARNING("HTMLEditor::DeleteTextAndTextNodesWithTransaction() failed");
      return caretPointOrError;
    }
    caretPointOrError.unwrap().MoveCaretPointTo(
        pointToPutCaret, {SuggestCaret::OnlyIfHasSuggestion});
    leadingWhiteSpacesDeleted = true;
  }
  if (trailingWhiteSpaceRange.IsPositioned() &&
      !trailingWhiteSpaceRange.Collapsed() &&
      leadingWhiteSpaceRange != trailingWhiteSpaceRange) {
    NS_ASSERTION(!leadingWhiteSpacesDeleted,
                 "We're trying to remove trailing white-spaces with maybe "
                 "outdated range");
    AutoTrackDOMPoint trackPointToPutCaret(aHTMLEditor.RangeUpdaterRef(),
                                           &pointToPutCaret);
    Result<CaretPoint, nsresult> caretPointOrError =
        aHTMLEditor.DeleteTextAndTextNodesWithTransaction(
            trailingWhiteSpaceRange.StartRef(),
            trailingWhiteSpaceRange.EndRef(),
            HTMLEditor::TreatEmptyTextNodes::KeepIfContainerOfRangeBoundaries);
    if (MOZ_UNLIKELY(caretPointOrError.isErr())) {
      NS_WARNING("HTMLEditor::DeleteTextAndTextNodesWithTransaction() failed");
      return caretPointOrError.propagateErr();
    }
    trackPointToPutCaret.FlushAndStopTracking();
    caretPointOrError.unwrap().MoveCaretPointTo(
        pointToPutCaret, {SuggestCaret::OnlyIfHasSuggestion});
  }
  return CaretPoint(std::move(pointToPutCaret));
}

}  // namespace mozilla
