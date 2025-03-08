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
  if (NS_WARN_IF(!aPointToSplit.IsInContentNode()) ||
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

  {
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

  OwningNonNull<Element> rightBlockElement = aRightBlockElement;
  EditorDOMPoint afterRightBlockChild = aAtRightBlockChild.NextPoint();
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

    // XXX AutoTrackDOMPoint instance, tracker, hasn't been destroyed here.
    //     Do we really need to do update rightBlockElement here??
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
  }

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

  if (!invisibleBRElementAtEndOfLeftBlockElement ||
      !invisibleBRElementAtEndOfLeftBlockElement->IsInComposedDoc()) {
    return moveContentResult;
  }

  MoveNodeResult unwrappedMoveContentResult = moveContentResult.unwrap();
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

  // NOTE: This method may extend deletion range:
  // - to delete invisible white-spaces at start of aRightBlockElement
  // - to delete invisible white-spaces before aRightBlockElement
  // - to delete invisible white-spaces at start of aAtLeftBlockChild.GetChild()
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
          "caused changing the right block element position in the left block "
          "element");
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

  OwningNonNull<Element> originalLeftBlockElement = aLeftBlockElement;
  OwningNonNull<Element> leftBlockElement = aLeftBlockElement;
  EditorDOMPoint atLeftBlockChild(aAtLeftBlockChild);
  {
    // We can't just track leftBlockElement because it's an Element, so track
    // something else.
    AutoTrackDOMPoint tracker(aHTMLEditor.RangeUpdaterRef(), &atLeftBlockChild);
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
        "WhiteSpaceVisibilityKeeper::DeleteInvisibleASCIIWhiteSpaces() caused "
        "unexpected DOM tree");
    return Err(NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE);
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

  if (!invisibleBRElementBeforeLeftBlockElement ||
      !invisibleBRElementBeforeLeftBlockElement->IsInComposedDoc()) {
    return moveContentResult;
  }

  MoveNodeResult unwrappedMoveContentResult = moveContentResult.unwrap();
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

  // NOTE: This method may extend deletion range:
  // - to delete invisible white-spaces at end of aLeftBlockElement
  // - to delete invisible white-spaces at start of aRightBlockElement
  // - to delete invisible `<br>` element at end of aLeftBlockElement

  // Adjust white-space at block boundaries
  {
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
  }
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
WhiteSpaceVisibilityKeeper::NormalizeWhiteSpacesToSplitTextNodeAt(
    HTMLEditor& aHTMLEditor, const EditorDOMPointInText& aPointToSplit) {
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
              // Chrome does not normalize the left `Text` at least when it ends
              // with an NBSP.
              aPointToSplit.IsPreviousCharNBSP()
          ? HTMLEditor::ReplaceWhiteSpacesData()
          : aHTMLEditor.GetPrecedingNormalizedStringToSplitAt(aPointToSplit);
  const HTMLEditor::ReplaceWhiteSpacesData replaceFollowingWhiteSpaceData =
      aHTMLEditor.GetFollowingNormalizedStringToSplitAt(aPointToSplit);
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
    HTMLEditor& aHTMLEditor, const EditorDOMPoint& aPointToSplit) {
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
            aHTMLEditor, pointToSplit.AsInText());
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
        // Chrome does not normalize preceding `Text` at least when it ends with
        // an NBSP.
        if (textNode->TextDataLength() &&
            textNode->TextFragment().CharAt(textNode->TextLength() - 1u) ==
                HTMLEditUtils::kNBSP) {
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
            aHTMLEditor, EditorDOMPointInText::AtEndOf(textNode));
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
            aHTMLEditor, EditorDOMPointInText(textNode, 0u));
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
    Result<EditorDOMPoint, nsresult>
        normalizeSurroundingWhiteSpacesResultOrError =
            WhiteSpaceVisibilityKeeper::NormalizeWhiteSpacesToSplitAt(
                aHTMLEditor, aPointToInsert);
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
  NS_WARNING_ASSERTION(insertOrReplaceTextResultOrError.isOk(),
                       "HTMLEditor::ReplaceTextWithTransaction() failed");
  // TODO: We need to normalize surrounding white-spaces if this insertion ends
  // a composition.  However, it requires more utility methods.  Therefore,
  // it'll be implemented in a following patch.
  return insertOrReplaceTextResultOrError;
}

// static
Result<CaretPoint, nsresult>
WhiteSpaceVisibilityKeeper::DeletePreviousWhiteSpace(
    HTMLEditor& aHTMLEditor, const EditorDOMPoint& aPoint,
    const Element& aEditingHost) {
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
  {
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

  nsresult rv = aHTMLEditor.JoinNearestEditableNodesWithTransaction(
      *previousEditableSibling, MOZ_KnownLive(*aCaretPoint.ContainerAs<Text>()),
      &pointToPutCaret);
  if (NS_FAILED(rv)) {
    NS_WARNING("HTMLEditor::JoinNearestEditableNodesWithTransaction() failed");
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

// static
Result<CaretPoint, nsresult> WhiteSpaceVisibilityKeeper::
    MakeSureToKeepVisibleStateOfWhiteSpacesAroundDeletingRange(
        HTMLEditor& aHTMLEditor, const EditorDOMRange& aRangeToDelete,
        const Element& aEditingHost) {
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
