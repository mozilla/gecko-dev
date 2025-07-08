/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EditorBase.h"
#include "HTMLEditor.h"
#include "HTMLEditorInlines.h"
#include "HTMLEditorNestedClasses.h"

#include <utility>

#include "AutoClonedRangeArray.h"
#include "CSSEditUtils.h"
#include "EditAction.h"
#include "EditorDOMPoint.h"
#include "EditorLineBreak.h"
#include "EditorUtils.h"
#include "HTMLEditHelpers.h"
#include "HTMLEditUtils.h"
#include "PendingStyles.h"  // for SpecifiedStyle
#include "WhiteSpaceVisibilityKeeper.h"
#include "WSRunScanner.h"

#include "ErrorList.h"
#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/ContentIterator.h"
#include "mozilla/EditorForwards.h"
#include "mozilla/Maybe.h"
#include "mozilla/PresShell.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/HTMLBRElement.h"
#include "mozilla/dom/Selection.h"
#include "nsAtom.h"
#include "nsContentUtils.h"
#include "nsDebug.h"
#include "nsError.h"
#include "nsGkAtoms.h"
#include "nsIContent.h"
#include "nsIFrame.h"
#include "nsINode.h"
#include "nsTArray.h"
#include "nsTextNode.h"

namespace mozilla {

using namespace dom;
using EmptyCheckOption = HTMLEditUtils::EmptyCheckOption;
using EmptyCheckOptions = HTMLEditUtils::EmptyCheckOptions;
using LeafNodeType = HTMLEditUtils::LeafNodeType;
using LeafNodeTypes = HTMLEditUtils::LeafNodeTypes;
using WalkTreeOption = HTMLEditUtils::WalkTreeOption;

Result<EditActionResult, nsresult>
HTMLEditor::InsertParagraphSeparatorAsSubAction(const Element& aEditingHost) {
  if (NS_WARN_IF(!mInitSucceeded)) {
    return Err(NS_ERROR_NOT_INITIALIZED);
  }

  {
    Result<EditActionResult, nsresult> result = CanHandleHTMLEditSubAction(
        CheckSelectionInReplacedElement::OnlyWhenNotInSameNode);
    if (MOZ_UNLIKELY(result.isErr())) {
      NS_WARNING("HTMLEditor::CanHandleHTMLEditSubAction() failed");
      return result;
    }
    if (result.inspect().Canceled()) {
      return result;
    }
  }

  // XXX This may be called by execCommand() with "insertParagraph".
  //     In such case, naming the transaction "TypingTxnName" is odd.
  AutoPlaceholderBatch treatAsOneTransaction(*this, *nsGkAtoms::TypingTxnName,
                                             ScrollSelectionIntoView::Yes,
                                             __FUNCTION__);

  IgnoredErrorResult ignoredError;
  AutoEditSubActionNotifier startToHandleEditSubAction(
      *this, EditSubAction::eInsertParagraphSeparator, nsIEditor::eNext,
      ignoredError);
  if (NS_WARN_IF(ignoredError.ErrorCodeIs(NS_ERROR_EDITOR_DESTROYED))) {
    return Err(ignoredError.StealNSResult());
  }
  NS_WARNING_ASSERTION(
      !ignoredError.Failed(),
      "HTMLEditor::OnStartToHandleTopLevelEditSubAction() failed, but ignored");

  UndefineCaretBidiLevel();

  // If the selection isn't collapsed, delete it.
  if (!SelectionRef().IsCollapsed()) {
    nsresult rv =
        DeleteSelectionAsSubAction(nsIEditor::eNone, nsIEditor::eStrip);
    if (NS_FAILED(rv)) {
      NS_WARNING(
          "EditorBase::DeleteSelectionAsSubAction(eNone, eStrip) failed");
      return Err(rv);
    }
  }

  AutoInsertParagraphHandler insertParagraphHandler(*this, aEditingHost);
  Result<EditActionResult, nsresult> insertParagraphResult =
      insertParagraphHandler.Run();
  NS_WARNING_ASSERTION(insertParagraphResult.isOk(),
                       "AutoInsertParagraphHandler::Run() failed");
  return insertParagraphResult;
}

Result<EditActionResult, nsresult>
HTMLEditor::AutoInsertParagraphHandler::Run() {
  MOZ_ASSERT(mHTMLEditor.IsEditActionDataAvailable());
  MOZ_ASSERT(mHTMLEditor.IsTopLevelEditSubActionDataAvailable());

  nsresult rv = mHTMLEditor.EnsureNoPaddingBRElementForEmptyEditor();
  if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
    return Err(NS_ERROR_EDITOR_DESTROYED);
  }
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "EditorBase::EnsureNoPaddingBRElementForEmptyEditor() "
                       "failed, but ignored");

  if (NS_SUCCEEDED(rv) && mHTMLEditor.SelectionRef().IsCollapsed()) {
    nsresult rv =
        mHTMLEditor.EnsureCaretNotAfterInvisibleBRElement(mEditingHost);
    if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
      return Err(NS_ERROR_EDITOR_DESTROYED);
    }
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "HTMLEditor::EnsureCaretNotAfterInvisibleBRElement() "
                         "failed, but ignored");
    if (NS_SUCCEEDED(rv)) {
      nsresult rv = mHTMLEditor.PrepareInlineStylesForCaret();
      if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
        return Err(NS_ERROR_EDITOR_DESTROYED);
      }
      NS_WARNING_ASSERTION(
          NS_SUCCEEDED(rv),
          "HTMLEditor::PrepareInlineStylesForCaret() failed, but ignored");
    }
  }

  AutoClonedSelectionRangeArray selectionRanges(mHTMLEditor.SelectionRef());
  selectionRanges.EnsureOnlyEditableRanges(mEditingHost);

  auto pointToInsert =
      selectionRanges.GetFirstRangeStartPoint<EditorDOMPoint>();
  if (NS_WARN_IF(!pointToInsert.IsInContentNode())) {
    return Err(NS_ERROR_FAILURE);
  }
  while (true) {
    Element* element = pointToInsert.GetContainerOrContainerParentElement();
    if (MOZ_UNLIKELY(!element)) {
      return Err(NS_ERROR_FAILURE);
    }
    // If the element can have a <br> element (it means that the element or its
    // container must be able to have <div> or <p> too), we can handle
    // insertParagraph at the point.
    if (HTMLEditUtils::CanNodeContain(*element, *nsGkAtoms::br)) {
      break;
    }
    // Otherwise, try to insert paragraph at the parent.
    pointToInsert = pointToInsert.ParentPoint();
  }

  if (mHTMLEditor.IsMailEditor()) {
    if (const RefPtr<Element> mailCiteElement =
            mHTMLEditor.GetMostDistantAncestorMailCiteElement(
                *pointToInsert.ContainerAs<nsIContent>())) {
      // Split any mailcites in the way.  Should we abort this if we encounter
      // table cell boundaries?
      Result<CaretPoint, nsresult> caretPointOrError =
          HandleInMailCiteElement(*mailCiteElement, pointToInsert);
      if (MOZ_UNLIKELY(caretPointOrError.isErr())) {
        NS_WARNING(
            "AutoInsertParagraphHandler::HandleInMailCiteElement() failed");
        return caretPointOrError.propagateErr();
      }
      CaretPoint caretPoint = caretPointOrError.unwrap();
      MOZ_ASSERT(caretPoint.HasCaretPointSuggestion());
      MOZ_ASSERT(caretPoint.CaretPointRef().GetInterlinePosition() ==
                 InterlinePosition::StartOfNextLine);
      MOZ_ASSERT(caretPoint.CaretPointRef().GetChild());
      MOZ_ASSERT(
          caretPoint.CaretPointRef().GetChild()->IsHTMLElement(nsGkAtoms::br));
      nsresult rv = caretPoint.SuggestCaretPointTo(mHTMLEditor, {});
      if (NS_FAILED(rv)) {
        NS_WARNING("CaretPoint::SuggestCaretPointTo() failed");
        return Err(rv);
      }
      return EditActionResult::HandledResult();
    }
  }

  // If the active editing host is an inline element, or if the active editing
  // host is the block parent itself and we're configured to use <br> as a
  // paragraph separator, just append a <br>.
  // If the editing host parent element is editable, it means that the editing
  // host must be a <body> element and the selection may be outside the body
  // element.  If the selection is outside the editing host, we should not
  // insert new paragraph nor <br> element.
  // XXX Currently, we don't support editing outside <body> element, but Blink
  //     does it.
  if (mEditingHost.GetParentElement() &&
      HTMLEditUtils::IsSimplyEditableNode(*mEditingHost.GetParentElement()) &&
      !nsContentUtils::ContentIsFlattenedTreeDescendantOf(
          pointToInsert.ContainerAs<nsIContent>(), &mEditingHost)) {
    return Err(NS_ERROR_EDITOR_NO_EDITABLE_RANGE);
  }

  // Look for the nearest parent block.  However, don't return error even if
  // there is no block parent here because in such case, i.e., editing host
  // is an inline element, we should insert <br> simply.
  RefPtr<Element> editableBlockElement =
      HTMLEditUtils::GetInclusiveAncestorElement(
          *pointToInsert.ContainerAs<nsIContent>(),
          HTMLEditUtils::ClosestEditableBlockElementOrButtonElement,
          BlockInlineCheck::UseComputedDisplayOutsideStyle);

  // If we cannot insert a <p>/<div> element at the selection, we should insert
  // a <br> element or a linefeed instead.
  if (ShouldInsertLineBreakInstead(editableBlockElement, pointToInsert)) {
    const Maybe<LineBreakType> lineBreakType =
        mHTMLEditor.GetPreferredLineBreakType(
            *pointToInsert.ContainerAs<nsIContent>(), mEditingHost);
    if (MOZ_UNLIKELY(!lineBreakType)) {
      // Cannot insert a line break there.
      return EditActionResult::IgnoredResult();
    }
    if (lineBreakType.value() == LineBreakType::Linefeed) {
      Result<EditActionResult, nsresult> insertLinefeedResultOrError =
          HandleInsertLinefeed(pointToInsert);
      NS_WARNING_ASSERTION(
          insertLinefeedResultOrError.isOk(),
          "AutoInsertParagraphHandler::HandleInsertLinefeed() failed");
      return insertLinefeedResultOrError;
    }
    Result<EditActionResult, nsresult> insertBRElementResultOrError =
        HandleInsertBRElement(pointToInsert);
    NS_WARNING_ASSERTION(
        insertBRElementResultOrError.isOk(),
        "AutoInsertParagraphHandler::HandleInsertBRElement() failed");
    return insertBRElementResultOrError;
  }

  RefPtr<Element> blockElementToPutCaret;
  // If the default paragraph separator is not <br> and selection is not in
  // a splittable block element, we should wrap selected contents in a new
  // paragraph, then, split it.
  if (!HTMLEditUtils::IsSplittableNode(*editableBlockElement) &&
      mDefaultParagraphSeparator != ParagraphSeparator::br) {
    MOZ_ASSERT(mDefaultParagraphSeparator == ParagraphSeparator::div ||
               mDefaultParagraphSeparator == ParagraphSeparator::p);
    // FIXME: If there is no splittable block element, the other browsers wrap
    // the right nodes into new paragraph, but keep the left node as-is.
    // We should follow them to make here simpler and better compatibility.
    Result<RefPtr<Element>, nsresult> suggestBlockElementToPutCaretOrError =
        mHTMLEditor.FormatBlockContainerWithTransaction(
            selectionRanges,
            MOZ_KnownLive(HTMLEditor::ToParagraphSeparatorTagName(
                mDefaultParagraphSeparator)),
            // For keeping the traditional behavior at insertParagraph command,
            // let's use the XUL paragraph state command targets even if we're
            // handling HTML insertParagraph command.
            FormatBlockMode::XULParagraphStateCommand, mEditingHost);
    if (MOZ_UNLIKELY(suggestBlockElementToPutCaretOrError.isErr())) {
      NS_WARNING("HTMLEditor::FormatBlockContainerWithTransaction() failed");
      return suggestBlockElementToPutCaretOrError.propagateErr();
    }
    if (selectionRanges.HasSavedRanges()) {
      selectionRanges.RestoreFromSavedRanges();
    }
    pointToInsert = selectionRanges.GetFirstRangeStartPoint<EditorDOMPoint>();
    if (NS_WARN_IF(!pointToInsert.IsInContentNode())) {
      return Err(NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE);
    }
    MOZ_ASSERT(pointToInsert.IsSetAndValid());
    blockElementToPutCaret = suggestBlockElementToPutCaretOrError.unwrap();

    editableBlockElement = HTMLEditUtils::GetInclusiveAncestorElement(
        *pointToInsert.ContainerAs<nsIContent>(),
        HTMLEditUtils::ClosestEditableBlockElementOrButtonElement,
        BlockInlineCheck::UseComputedDisplayOutsideStyle);
    if (NS_WARN_IF(!editableBlockElement)) {
      return Err(NS_ERROR_UNEXPECTED);
    }
    if (NS_WARN_IF(!HTMLEditUtils::IsSplittableNode(*editableBlockElement))) {
      // Didn't create a new block for some reason, fall back to <br>
      Result<EditActionResult, nsresult> insertBRElementResultOrError =
          HandleInsertBRElement(pointToInsert, blockElementToPutCaret);
      NS_WARNING_ASSERTION(
          insertBRElementResultOrError.isOk(),
          "AutoInsertParagraphHandler::HandleInsertBRElement() failed");
      return insertBRElementResultOrError;
    }
    // We want to collapse selection in the editable block element.
    blockElementToPutCaret = editableBlockElement;
  }

  // If block is empty, populate with br.  (For example, imagine a div that
  // contains the word "text".  The user selects "text" and types return.
  // "Text" is deleted leaving an empty block.  We want to put in one br to
  // make block have a line.  Then code further below will put in a second br.)
  RefPtr<Element> insertedPaddingBRElement;
  {
    Result<CreateLineBreakResult, nsresult> insertBRElementResultOrError =
        InsertBRElementIfEmptyBlockElement(
            *editableBlockElement, InsertBRElementIntoEmptyBlock::End,
            BlockInlineCheck::UseComputedDisplayOutsideStyle);
    if (MOZ_UNLIKELY(insertBRElementResultOrError.isErr())) {
      NS_WARNING(
          "AutoInsertParagraphHandler::InsertBRElementIfEmptyBlockElement("
          "InsertBRElementIntoEmptyBlock::End, "
          "BlockInlineCheck::UseComputedDisplayOutsideStyle) failed");
      return insertBRElementResultOrError.propagateErr();
    }

    CreateLineBreakResult insertBRElementResult =
        insertBRElementResultOrError.unwrap();
    insertBRElementResult.IgnoreCaretPointSuggestion();
    if (insertBRElementResult.Handled()) {
      insertedPaddingBRElement = &insertBRElementResult->BRElementRef();
    }

    pointToInsert = selectionRanges.GetFirstRangeStartPoint<EditorDOMPoint>();
    if (NS_WARN_IF(!pointToInsert.IsInContentNode())) {
      return Err(NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE);
    }
  }

  RefPtr<Element> maybeNonEditableListItem =
      HTMLEditUtils::GetClosestAncestorListItemElement(*editableBlockElement,
                                                       &mEditingHost);
  if (maybeNonEditableListItem &&
      HTMLEditUtils::IsSplittableNode(*maybeNonEditableListItem)) {
    Result<InsertParagraphResult, nsresult> insertParagraphInListItemResult =
        HandleInListItemElement(*maybeNonEditableListItem, pointToInsert);
    if (MOZ_UNLIKELY(insertParagraphInListItemResult.isErr())) {
      if (NS_WARN_IF(insertParagraphInListItemResult.unwrapErr() ==
                     NS_ERROR_EDITOR_DESTROYED)) {
        return Err(NS_ERROR_EDITOR_DESTROYED);
      }
      NS_WARNING(
          "AutoInsertParagraphHandler::HandleInListItemElement() failed, but "
          "ignored");
      return EditActionResult::HandledResult();
    }
    InsertParagraphResult unwrappedInsertParagraphInListItemResult =
        insertParagraphInListItemResult.unwrap();
    MOZ_ASSERT(unwrappedInsertParagraphInListItemResult.Handled());
    MOZ_ASSERT(unwrappedInsertParagraphInListItemResult.GetNewNode());
    const RefPtr<Element> listItemOrParagraphElement =
        unwrappedInsertParagraphInListItemResult.UnwrapNewNode();
    const EditorDOMPoint pointToPutCaret =
        unwrappedInsertParagraphInListItemResult.UnwrapCaretPoint();
    nsresult rv = CollapseSelectionToPointOrIntoBlockWhichShouldHaveCaret(
        pointToPutCaret, listItemOrParagraphElement,
        {SuggestCaret::AndIgnoreTrivialError});
    if (NS_FAILED(rv)) {
      NS_WARNING(
          "AutoInsertParagraphHandler::"
          "CollapseSelectionToPointOrIntoBlockWhichShouldHaveCaret() failed");
      return Err(rv);
    }
    NS_WARNING_ASSERTION(rv != NS_SUCCESS_EDITOR_BUT_IGNORED_TRIVIAL_ERROR,
                         "CollapseSelection() failed, but ignored");
    return EditActionResult::HandledResult();
  }

  if (HTMLEditUtils::IsHeader(*editableBlockElement)) {
    Result<InsertParagraphResult, nsresult>
        insertParagraphInHeadingElementResult =
            HandleInHeadingElement(*editableBlockElement, pointToInsert);
    if (MOZ_UNLIKELY(insertParagraphInHeadingElementResult.isErr())) {
      NS_WARNING(
          "AutoInsertParagraphHandler::HandleInHeadingElement() failed, but "
          "ignored");
      return EditActionResult::HandledResult();
    }
    InsertParagraphResult unwrappedInsertParagraphInHeadingElementResult =
        insertParagraphInHeadingElementResult.unwrap();
    if (unwrappedInsertParagraphInHeadingElementResult.Handled()) {
      MOZ_ASSERT(unwrappedInsertParagraphInHeadingElementResult.GetNewNode());
      blockElementToPutCaret =
          unwrappedInsertParagraphInHeadingElementResult.UnwrapNewNode();
    }
    const EditorDOMPoint pointToPutCaret =
        unwrappedInsertParagraphInHeadingElementResult.UnwrapCaretPoint();
    nsresult rv = CollapseSelectionToPointOrIntoBlockWhichShouldHaveCaret(
        pointToPutCaret, blockElementToPutCaret,
        {SuggestCaret::OnlyIfHasSuggestion,
         SuggestCaret::AndIgnoreTrivialError});
    if (NS_FAILED(rv)) {
      NS_WARNING(
          "AutoInsertParagraphHandler::"
          "CollapseSelectionToPointOrIntoBlockWhichShouldHaveCaret() failed");
      return Err(rv);
    }
    NS_WARNING_ASSERTION(rv != NS_SUCCESS_EDITOR_BUT_IGNORED_TRIVIAL_ERROR,
                         "CollapseSelection() failed, but ignored");
    return EditActionResult::HandledResult();
  }

  // XXX Ideally, we should take same behavior with both <p> container and
  //     <div> container.  However, we are still using <br> as default
  //     paragraph separator (non-standard) and we've split only <p> container
  //     long time.  Therefore, some web apps may depend on this behavior like
  //     Gmail.  So, let's use traditional odd behavior only when the default
  //     paragraph separator is <br>.  Otherwise, take consistent behavior
  //     between <p> container and <div> container.
  if ((mDefaultParagraphSeparator == ParagraphSeparator::br &&
       editableBlockElement->IsHTMLElement(nsGkAtoms::p)) ||
      (mDefaultParagraphSeparator != ParagraphSeparator::br &&
       editableBlockElement->IsAnyOfHTMLElements(nsGkAtoms::p,
                                                 nsGkAtoms::div))) {
    // Paragraphs: special rules to look for <br>s
    Result<SplitNodeResult, nsresult> splitNodeResult = HandleInParagraph(
        *editableBlockElement, insertedPaddingBRElement
                                   ? EditorDOMPoint(insertedPaddingBRElement)
                                   : pointToInsert);
    if (MOZ_UNLIKELY(splitNodeResult.isErr())) {
      NS_WARNING("HTMLEditor::HandleInsertParagraphInParagraph() failed");
      return splitNodeResult.propagateErr();
    }
    if (splitNodeResult.inspect().Handled()) {
      SplitNodeResult unwrappedSplitNodeResult = splitNodeResult.unwrap();
      const RefPtr<Element> rightParagraphElement =
          unwrappedSplitNodeResult.DidSplit()
              ? unwrappedSplitNodeResult.GetNextContentAs<Element>()
              : blockElementToPutCaret.get();
      const EditorDOMPoint pointToPutCaret =
          unwrappedSplitNodeResult.UnwrapCaretPoint();
      nsresult rv = CollapseSelectionToPointOrIntoBlockWhichShouldHaveCaret(
          pointToPutCaret, rightParagraphElement,
          {SuggestCaret::AndIgnoreTrivialError});
      if (NS_FAILED(rv)) {
        NS_WARNING(
            "AutoInsertParagraphHandler::"
            "CollapseSelectionToPointOrIntoBlockWhichShouldHaveCaret() failed");
        return Err(rv);
      }
      NS_WARNING_ASSERTION(
          rv != NS_SUCCESS_EDITOR_BUT_IGNORED_TRIVIAL_ERROR,
          "AutoInsertParagraphHandler::"
          "CollapseSelectionToPointOrIntoBlockWhichShouldHaveCaret() "
          "failed, but ignored");
      return EditActionResult::HandledResult();
    }
    MOZ_ASSERT(!splitNodeResult.inspect().HasCaretPointSuggestion());

    // Fall through, if HandleInsertParagraphInParagraph() didn't handle it.
    MOZ_ASSERT(pointToInsert.IsSetAndValid(),
               "HTMLEditor::HandleInsertParagraphInParagraph() shouldn't touch "
               "the DOM tree if it returns not-handled state");
  }

  // If nobody handles this edit action, let's insert new <br> at the selection.
  Result<EditActionResult, nsresult> insertBRElementResultOrError =
      HandleInsertBRElement(pointToInsert, blockElementToPutCaret);
  NS_WARNING_ASSERTION(
      insertBRElementResultOrError.isOk(),
      "AutoInsertParagraphHandler::HandleInsertBRElement() failed");
  return insertBRElementResultOrError;
}

Result<EditActionResult, nsresult>
HTMLEditor::AutoInsertParagraphHandler::HandleInsertBRElement(
    const EditorDOMPoint& aPointToInsert,
    const Element* aBlockElementWhichShouldHaveCaret /* = nullptr */) {
  Result<CreateElementResult, nsresult> insertBRElementResult =
      InsertBRElement(aPointToInsert);
  if (MOZ_UNLIKELY(insertBRElementResult.isErr())) {
    NS_WARNING("AutoInsertParagraphHandler::InsertBRElement() failed");
    return insertBRElementResult.propagateErr();
  }
  const EditorDOMPoint pointToPutCaret =
      insertBRElementResult.unwrap().UnwrapCaretPoint();
  if (MOZ_UNLIKELY(!pointToPutCaret.IsSet())) {
    NS_WARNING(
        "AutoInsertParagraphHandler::InsertBRElement() didn't suggest a "
        "point to put caret");
    return Err(NS_ERROR_FAILURE);
  }
  nsresult rv = CollapseSelectionToPointOrIntoBlockWhichShouldHaveCaret(
      pointToPutCaret, aBlockElementWhichShouldHaveCaret, {});
  if (NS_FAILED(rv)) {
    NS_WARNING(
        "AutoInsertParagraphHandler::"
        "CollapseSelectionToPointOrIntoBlockWhichShouldHaveCaret() failed");
    return Err(rv);
  }
  return EditActionResult::HandledResult();
}

Result<EditActionResult, nsresult>
HTMLEditor::AutoInsertParagraphHandler::HandleInsertLinefeed(
    const EditorDOMPoint& aPointToInsert) {
  Result<EditorDOMPoint, nsresult> insertLineFeedResult =
      AutoInsertLineBreakHandler::InsertLinefeed(mHTMLEditor, aPointToInsert,
                                                 mEditingHost);
  if (MOZ_UNLIKELY(insertLineFeedResult.isErr())) {
    NS_WARNING("AutoInsertLineBreakHandler::InsertLinefeed() failed");
    return insertLineFeedResult.propagateErr();
  }
  nsresult rv = mHTMLEditor.CollapseSelectionTo(insertLineFeedResult.inspect());
  if (NS_FAILED(rv)) {
    NS_WARNING("EditorBase::CollapseSelectionTo() failed");
    return Err(rv);
  }
  return EditActionResult::HandledResult();
}

bool HTMLEditor::AutoInsertParagraphHandler::ShouldInsertLineBreakInstead(
    const Element* aEditableBlockElement,
    const EditorDOMPoint& aCandidatePointToSplit) {
  // If there is no block parent in the editing host, i.e., the editing
  // host itself is also a non-block element, we should insert a line
  // break.
  if (!aEditableBlockElement) {
    // XXX Chromium checks if the CSS box of the editing host is a block.
    return true;
  }

  // If the editable block element is not splittable, e.g., it's an
  // editing host, and the default paragraph separator is <br> or the
  // element cannot contain a <p> element, we should insert a <br>
  // element.
  if (!HTMLEditUtils::IsSplittableNode(*aEditableBlockElement)) {
    return mDefaultParagraphSeparator == ParagraphSeparator::br ||
           !HTMLEditUtils::CanElementContainParagraph(*aEditableBlockElement) ||
           (aCandidatePointToSplit.IsInContentNode() &&
            mHTMLEditor
                    .GetPreferredLineBreakType(
                        *aCandidatePointToSplit.ContainerAs<nsIContent>(),
                        mEditingHost)
                    .valueOr(LineBreakType::BRElement) ==
                LineBreakType::Linefeed &&
            HTMLEditUtils::IsDisplayOutsideInline(mEditingHost));
  }

  // If the nearest block parent is a single-line container declared in
  // the execCommand spec and not the editing host, we should separate the
  // block even if the default paragraph separator is <br> element.
  if (HTMLEditUtils::IsSingleLineContainer(*aEditableBlockElement)) {
    return false;
  }

  // Otherwise, unless there is no block ancestor which can contain <p>
  // element, we shouldn't insert a line break here.
  for (const Element* editableBlockAncestor = aEditableBlockElement;
       editableBlockAncestor;
       editableBlockAncestor = HTMLEditUtils::GetAncestorElement(
           *editableBlockAncestor,
           HTMLEditUtils::ClosestEditableBlockElementOrButtonElement,
           BlockInlineCheck::UseComputedDisplayOutsideStyle)) {
    if (HTMLEditUtils::CanElementContainParagraph(*editableBlockAncestor)) {
      return false;
    }
  }
  return true;
}

// static
nsresult HTMLEditor::AutoInsertParagraphHandler::
    CollapseSelectionToPointOrIntoBlockWhichShouldHaveCaret(
        const EditorDOMPoint& aCandidatePointToPutCaret,
        const Element* aBlockElementShouldHaveCaret,
        const SuggestCaretOptions& aOptions) {
  if (!aCandidatePointToPutCaret.IsSet()) {
    if (aOptions.contains(SuggestCaret::OnlyIfHasSuggestion)) {
      return NS_OK;
    }
    return aOptions.contains(SuggestCaret::AndIgnoreTrivialError)
               ? NS_SUCCESS_EDITOR_BUT_IGNORED_TRIVIAL_ERROR
               : NS_ERROR_FAILURE;
  }
  EditorDOMPoint pointToPutCaret(aCandidatePointToPutCaret);
  if (aBlockElementShouldHaveCaret) {
    Result<EditorDOMPoint, nsresult> pointToPutCaretOrError =
        HTMLEditUtils::ComputePointToPutCaretInElementIfOutside<EditorDOMPoint>(
            *aBlockElementShouldHaveCaret, aCandidatePointToPutCaret);
    if (MOZ_UNLIKELY(pointToPutCaretOrError.isErr())) {
      NS_WARNING(
          "HTMLEditUtils::ComputePointToPutCaretInElementIfOutside() "
          "failed, but ignored");
    } else if (pointToPutCaretOrError.inspect().IsSet()) {
      pointToPutCaret = pointToPutCaretOrError.unwrap();
    }
  }
  nsresult rv = mHTMLEditor.CollapseSelectionTo(pointToPutCaret);
  if (NS_FAILED(rv) && MOZ_LIKELY(rv != NS_ERROR_EDITOR_DESTROYED) &&
      aOptions.contains(SuggestCaret::AndIgnoreTrivialError)) {
    rv = NS_SUCCESS_EDITOR_BUT_IGNORED_TRIVIAL_ERROR;
  }
  return rv;
}

Result<CreateElementResult, nsresult>
HTMLEditor::AutoInsertParagraphHandler::InsertBRElement(
    const EditorDOMPoint& aPointToBreak) {
  MOZ_ASSERT(aPointToBreak.IsInContentNode());

  const bool editingHostIsEmpty = HTMLEditUtils::IsEmptyNode(
      mEditingHost, {EmptyCheckOption::TreatNonEditableContentAsInvisible});
  const WSRunScanner wsRunScanner(WSRunScanner::Scan::EditableNodes,
                                  aPointToBreak,
                                  BlockInlineCheck::UseComputedDisplayStyle);
  const WSScanResult backwardScanResult =
      wsRunScanner.ScanPreviousVisibleNodeOrBlockBoundaryFrom(aPointToBreak);
  if (MOZ_UNLIKELY(backwardScanResult.Failed())) {
    NS_WARNING(
        "WSRunScanner::ScanPreviousVisibleNodeOrBlockBoundaryFrom() failed");
    return Err(NS_ERROR_FAILURE);
  }
  const bool brElementIsAfterBlock =
      backwardScanResult.ReachedBlockBoundary() ||
      // FIXME: This is wrong considering because the inline editing host may
      // be surrounded by visible inline content.  However, WSRunScanner is
      // not aware of block boundary around it and stopping this change causes
      // starting to fail some WPT.  Therefore, we need to keep doing this for
      // now.
      backwardScanResult.ReachedInlineEditingHostBoundary();
  const WSScanResult forwardScanResult =
      wsRunScanner.ScanInclusiveNextVisibleNodeOrBlockBoundaryFrom(
          aPointToBreak);
  if (MOZ_UNLIKELY(forwardScanResult.Failed())) {
    NS_WARNING("WSRunScanner::ScanNextVisibleNodeOrBlockBoundaryFrom() failed");
    return Err(NS_ERROR_FAILURE);
  }
  const bool brElementIsBeforeBlock =
      forwardScanResult.ReachedBlockBoundary() ||
      // FIXME: See above comment
      forwardScanResult.ReachedInlineEditingHostBoundary();

  // First, insert a <br> element.
  RefPtr<Element> brElement;
  if (mHTMLEditor.IsPlaintextMailComposer()) {
    Result<CreateLineBreakResult, nsresult> insertBRElementResultOrError =
        mHTMLEditor.InsertLineBreak(WithTransaction::Yes,
                                    LineBreakType::BRElement, aPointToBreak);
    if (MOZ_UNLIKELY(insertBRElementResultOrError.isErr())) {
      NS_WARNING(
          "HTMLEditor::InsertLineBreak(WithTransaction::Yes, "
          "LineBreakType::BRElement) failed");
      return insertBRElementResultOrError.propagateErr();
    }
    CreateLineBreakResult insertBRElementResult =
        insertBRElementResultOrError.unwrap();
    // We'll return with suggesting new caret position and nobody refers
    // selection after here.  So we don't need to update selection here.
    insertBRElementResult.IgnoreCaretPointSuggestion();
    brElement = &insertBRElementResult->BRElementRef();
  } else {
    EditorDOMPoint pointToBreak(aPointToBreak);
    // If the container of the break is a link, we need to split it and
    // insert new <br> between the split links.
    RefPtr<Element> linkNode =
        HTMLEditor::GetLinkElement(pointToBreak.GetContainer());
    if (linkNode) {
      // FIXME: Normalize surrounding white-spaces before splitting the
      // insertion point here.
      Result<SplitNodeResult, nsresult> splitLinkNodeResult =
          mHTMLEditor.SplitNodeDeepWithTransaction(
              *linkNode, pointToBreak,
              SplitAtEdges::eDoNotCreateEmptyContainer);
      if (MOZ_UNLIKELY(splitLinkNodeResult.isErr())) {
        NS_WARNING(
            "HTMLEditor::SplitNodeDeepWithTransaction(SplitAtEdges::"
            "eDoNotCreateEmptyContainer) failed");
        return splitLinkNodeResult.propagateErr();
      }
      // TODO: Some methods called by
      //       WhiteSpaceVisibilityKeeper::InsertLineBreak() use
      //       ComputeEditingHost() which depends on selection.  Therefore,
      //       we cannot skip updating selection here.
      nsresult rv = splitLinkNodeResult.inspect().SuggestCaretPointTo(
          mHTMLEditor, {SuggestCaret::OnlyIfHasSuggestion,
                        SuggestCaret::OnlyIfTransactionsAllowedToDoIt});
      if (NS_FAILED(rv)) {
        NS_WARNING("SplitNodeResult::SuggestCaretPointTo() failed");
        return Err(rv);
      }
      pointToBreak =
          splitLinkNodeResult.inspect().AtSplitPoint<EditorDOMPoint>();
    }
    Result<CreateLineBreakResult, nsresult> insertBRElementResultOrError =
        WhiteSpaceVisibilityKeeper::InsertLineBreak(LineBreakType::BRElement,
                                                    mHTMLEditor, pointToBreak);
    if (MOZ_UNLIKELY(insertBRElementResultOrError.isErr())) {
      NS_WARNING(
          "WhiteSpaceVisibilityKeeper::InsertLineBreak(LineBreakType::"
          "BRElement) failed");
      return insertBRElementResultOrError.propagateErr();
    }
    CreateLineBreakResult insertBRElementResult =
        insertBRElementResultOrError.unwrap();
    // We'll return with suggesting new caret position and nobody refers
    // selection after here.  So we don't need to update selection here.
    insertBRElementResult.IgnoreCaretPointSuggestion();
    brElement = &insertBRElementResult->BRElementRef();
  }

  if (MOZ_UNLIKELY(!brElement->GetParentNode())) {
    NS_WARNING("Inserted <br> element was removed by the web app");
    return Err(NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE);
  }
  auto afterBRElement = EditorDOMPoint::After(brElement);

  const auto InsertAdditionalInvisibleLineBreak =
      [this, &afterBRElement]()
          MOZ_CAN_RUN_SCRIPT -> Result<CreateLineBreakResult, nsresult> {
    // Empty last line is invisible if it's immediately before either parent or
    // another block's boundary so that we need to put invisible <br> element
    // here for making it visible.
    Result<CreateLineBreakResult, nsresult>
        insertPaddingBRElementResultOrError =
            WhiteSpaceVisibilityKeeper::InsertLineBreak(
                LineBreakType::BRElement, mHTMLEditor, afterBRElement);
    NS_WARNING_ASSERTION(insertPaddingBRElementResultOrError.isOk(),
                         "WhiteSpaceVisibilityKeeper::InsertLineBreak("
                         "LineBreakType::BRElement) failed");
    // afterBRElement points after the first <br> with referring an old child.
    // Therefore, we need to update it with new child which is the new invisible
    // <br>.
    afterBRElement = insertPaddingBRElementResultOrError.inspect()
                         .AtLineBreak<EditorDOMPoint>();
    return insertPaddingBRElementResultOrError;
  };

  if (brElementIsAfterBlock && brElementIsBeforeBlock) {
    // We just placed a <br> between block boundaries.  This is the one case
    // where we want the selection to be before the br we just placed, as the
    // br will be on a new line, rather than at end of prior line.
    // XXX brElementIsAfterBlock and brElementIsBeforeBlock were set before
    //     modifying the DOM tree.  So, now, the <br> element may not be
    //     between blocks.
    EditorDOMPoint pointToPutCaret;
    if (editingHostIsEmpty) {
      Result<CreateLineBreakResult, nsresult>
          insertPaddingBRElementResultOrError =
              InsertAdditionalInvisibleLineBreak();
      if (MOZ_UNLIKELY(insertPaddingBRElementResultOrError.isErr())) {
        return insertPaddingBRElementResultOrError.propagateErr();
      }
      insertPaddingBRElementResultOrError.unwrap().IgnoreCaretPointSuggestion();
      pointToPutCaret = std::move(afterBRElement);
    } else {
      pointToPutCaret =
          EditorDOMPoint(brElement, InterlinePosition::StartOfNextLine);
    }
    return CreateElementResult(std::move(brElement),
                               std::move(pointToPutCaret));
  }

  const WSScanResult forwardScanFromAfterBRElementResult =
      WSRunScanner::ScanInclusiveNextVisibleNodeOrBlockBoundary(
          WSRunScanner::Scan::EditableNodes, afterBRElement,
          BlockInlineCheck::UseComputedDisplayStyle);
  if (MOZ_UNLIKELY(forwardScanFromAfterBRElementResult.Failed())) {
    NS_WARNING("WSRunScanner::ScanNextVisibleNodeOrBlockBoundary() failed");
    return Err(NS_ERROR_FAILURE);
  }
  if (forwardScanFromAfterBRElementResult.ReachedBRElement()) {
    // The next thing after the break we inserted is another break.  Move the
    // second break to be the first break's sibling.  This will prevent them
    // from being in different inline nodes, which would break
    // SetInterlinePosition().  It will also assure that if the user clicks
    // away and then clicks back on their new blank line, they will still get
    // the style from the line above.
    if (brElement->GetNextSibling() !=
        forwardScanFromAfterBRElementResult.BRElementPtr()) {
      MOZ_ASSERT(forwardScanFromAfterBRElementResult.BRElementPtr());
      Result<MoveNodeResult, nsresult> moveBRElementResult =
          mHTMLEditor.MoveNodeWithTransaction(
              MOZ_KnownLive(
                  *forwardScanFromAfterBRElementResult.BRElementPtr()),
              afterBRElement);
      if (MOZ_UNLIKELY(moveBRElementResult.isErr())) {
        NS_WARNING("HTMLEditor::MoveNodeWithTransaction() failed");
        return moveBRElementResult.propagateErr();
      }
      nsresult rv = moveBRElementResult.inspect().SuggestCaretPointTo(
          mHTMLEditor, {SuggestCaret::OnlyIfHasSuggestion,
                        SuggestCaret::OnlyIfTransactionsAllowedToDoIt,
                        SuggestCaret::AndIgnoreTrivialError});
      if (NS_FAILED(rv)) {
        NS_WARNING("MoveNodeResult::SuggestCaretPointTo() failed");
        return Err(rv);
      }
      NS_WARNING_ASSERTION(
          rv != NS_SUCCESS_EDITOR_BUT_IGNORED_TRIVIAL_ERROR,
          "MoveNodeResult::SuggestCaretPointTo() failed, but ignored");
      // afterBRElement points after the first <br> with referring an old child.
      // Therefore, we need to update it with new child which is the new
      // invisible <br>.
      afterBRElement.Set(forwardScanFromAfterBRElementResult.BRElementPtr());
    }
  } else if ((forwardScanFromAfterBRElementResult.ReachedBlockBoundary() ||
              // FIXME: This is wrong considering because the inline editing
              // host may be surrounded by visible inline content.  However,
              // WSRunScanner is not aware of block boundary around it and
              // stopping this change causes starting to fail some WPT.
              // Therefore, we need to keep doing this for now.
              forwardScanFromAfterBRElementResult
                  .ReachedInlineEditingHostBoundary()) &&
             !brElementIsAfterBlock) {
    Result<CreateLineBreakResult, nsresult>
        insertPaddingBRElementResultOrError =
            InsertAdditionalInvisibleLineBreak();
    if (MOZ_UNLIKELY(insertPaddingBRElementResultOrError.isErr())) {
      return insertPaddingBRElementResultOrError.propagateErr();
    }
    insertPaddingBRElementResultOrError.unwrap().IgnoreCaretPointSuggestion();
  }

  // We want the caret to stick to whatever is past the break.  This is because
  // the break is on the same line we were on, but the next content will be on
  // the following line.

  // An exception to this is if the break has a next sibling that is a block
  // node.  Then we stick to the left to avoid an uber caret.
  nsIContent* nextSiblingOfBRElement = brElement->GetNextSibling();
  afterBRElement.SetInterlinePosition(
      nextSiblingOfBRElement && HTMLEditUtils::IsBlockElement(
                                    *nextSiblingOfBRElement,
                                    BlockInlineCheck::UseComputedDisplayStyle)
          ? InterlinePosition::EndOfLine
          : InterlinePosition::StartOfNextLine);
  return CreateElementResult(std::move(brElement), afterBRElement);
}

Result<CaretPoint, nsresult>
HTMLEditor::AutoInsertParagraphHandler::HandleInMailCiteElement(
    Element& aMailCiteElement, const EditorDOMPoint& aPointToSplit) {
  MOZ_ASSERT(aPointToSplit.IsSet());
  NS_ASSERTION(!HTMLEditUtils::IsEmptyNode(
                   aMailCiteElement,
                   {EmptyCheckOption::TreatNonEditableContentAsInvisible}),
               "The mail-cite element will be deleted, does it expected result "
               "for you?");

  auto splitCiteElementResult =
      SplitMailCiteElement(aPointToSplit, aMailCiteElement);
  if (MOZ_UNLIKELY(splitCiteElementResult.isErr())) {
    NS_WARNING("Failed to split a mail-cite element");
    return splitCiteElementResult.propagateErr();
  }
  SplitNodeResult unwrappedSplitCiteElementResult =
      splitCiteElementResult.unwrap();
  // When adding caret suggestion to SplitNodeResult, here didn't change
  // selection so that just ignore it.
  unwrappedSplitCiteElementResult.IgnoreCaretPointSuggestion();

  // Add an invisible <br> to the end of left cite node if it was a <span> of
  // style="display: block".  This is important, since when serializing the cite
  // to plain text, the span which caused the visual break is discarded.  So the
  // added <br> will guarantee that the serializer will insert a break where the
  // user saw one.
  // FYI: unwrappedSplitCiteElementResult grabs the previous node and the next
  //      node with nsCOMPtr or EditorDOMPoint.  So, it's safe to access
  //      leftCiteElement and rightCiteElement even after changing the DOM tree
  //      and/or selection even though it's raw pointer.
  auto* const leftCiteElement =
      unwrappedSplitCiteElementResult.GetPreviousContentAs<Element>();
  auto* const rightCiteElement =
      unwrappedSplitCiteElementResult.GetNextContentAs<Element>();
  if (leftCiteElement && leftCiteElement->IsHTMLElement(nsGkAtoms::span) &&
      // XXX Oh, this depends on layout information of new element, and it's
      //     created by the hacky flush in DoSplitNode().  So we need to
      //     redesign around this for bug 1710784.
      leftCiteElement->GetPrimaryFrame() &&
      leftCiteElement->GetPrimaryFrame()->IsBlockFrameOrSubclass()) {
    nsIContent* lastChild = leftCiteElement->GetLastChild();
    if (lastChild && !lastChild->IsHTMLElement(nsGkAtoms::br)) {
      Result<CreateLineBreakResult, nsresult>
          insertPaddingBRElementResultOrError = mHTMLEditor.InsertLineBreak(
              WithTransaction::Yes, LineBreakType::BRElement,
              EditorDOMPoint::AtEndOf(*leftCiteElement));
      if (MOZ_UNLIKELY(insertPaddingBRElementResultOrError.isErr())) {
        NS_WARNING(
            "HTMLEditor::InsertLineBreak(WithTransaction::Yes, "
            "LineBreakType::BRElement) failed");
        return insertPaddingBRElementResultOrError.propagateErr();
      }
      CreateLineBreakResult insertPaddingBRElementResult =
          insertPaddingBRElementResultOrError.unwrap();
      MOZ_ASSERT(insertPaddingBRElementResult.Handled());
      // We don't need to update selection here because we'll do another
      // InsertLineBreak call soon.
      insertPaddingBRElementResult.IgnoreCaretPointSuggestion();
    }
  }

  // In most cases, <br> should be inserted after current cite.  However, if
  // left cite hasn't been created because the split point was start of the
  // cite node, <br> should be inserted before the current cite.
  Result<CreateLineBreakResult, nsresult> insertBRElementResultOrError =
      mHTMLEditor.InsertLineBreak(
          WithTransaction::Yes, LineBreakType::BRElement,
          unwrappedSplitCiteElementResult.AtSplitPoint<EditorDOMPoint>());
  if (MOZ_UNLIKELY(insertBRElementResultOrError.isErr())) {
    NS_WARNING(
        "HTMLEditor::InsertLineBreak(WithTransaction::Yes, "
        "LineBreakType::BRElement) failed");
    return Err(insertBRElementResultOrError.unwrapErr());
  }
  CreateLineBreakResult insertBRElementResult =
      insertBRElementResultOrError.unwrap();
  MOZ_ASSERT(insertBRElementResult.Handled());
  // We'll return with suggesting caret position.  Therefore, we don't need
  // to update selection here.
  insertBRElementResult.IgnoreCaretPointSuggestion();
  // if aMailCiteElement wasn't a block, we might also want another break before
  // it. We need to examine the content both before the br we just added and
  // also just after it.  If we don't have another br or block boundary
  // adjacent, then we will need a 2nd br added to achieve blank line that user
  // expects.
  {
    nsresult rvOfInsertPaddingBRElement =
        MaybeInsertPaddingBRElementToInlineMailCiteElement(
            insertBRElementResult.AtLineBreak<EditorDOMPoint>(),
            aMailCiteElement);
    if (NS_FAILED(rvOfInsertPaddingBRElement)) {
      NS_WARNING(
          "Failed to insert additional <br> element before the inline right "
          "mail-cite element");
      return Err(rvOfInsertPaddingBRElement);
    }
  }

  if (leftCiteElement &&
      HTMLEditUtils::IsEmptyNode(
          *leftCiteElement,
          {EmptyCheckOption::TreatNonEditableContentAsInvisible})) {
    // MOZ_KnownLive(leftCiteElement) because it's grabbed by
    // unwrappedSplitCiteElementResult.
    nsresult rv =
        mHTMLEditor.DeleteNodeWithTransaction(MOZ_KnownLive(*leftCiteElement));
    if (NS_FAILED(rv)) {
      NS_WARNING("EditorBase::DeleteNodeWithTransaction() failed");
      return Err(rv);
    }
  }

  if (rightCiteElement &&
      HTMLEditUtils::IsEmptyNode(
          *rightCiteElement,
          {EmptyCheckOption::TreatNonEditableContentAsInvisible})) {
    // MOZ_KnownLive(rightCiteElement) because it's grabbed by
    // unwrappedSplitCiteElementResult.
    nsresult rv =
        mHTMLEditor.DeleteNodeWithTransaction(MOZ_KnownLive(*rightCiteElement));
    if (NS_FAILED(rv)) {
      NS_WARNING("EditorBase::DeleteNodeWithTransaction() failed");
      return Err(rv);
    }
  }

  if (NS_WARN_IF(!insertBRElementResult.LineBreakIsInComposedDoc())) {
    return Err(NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE);
  }
  auto pointToPutCaret = insertBRElementResult.AtLineBreak<EditorDOMPoint>();
  pointToPutCaret.SetInterlinePosition(InterlinePosition::StartOfNextLine);
  return CaretPoint(std::move(pointToPutCaret));
}

Result<SplitNodeResult, nsresult>
HTMLEditor::AutoInsertParagraphHandler::SplitMailCiteElement(
    const EditorDOMPoint& aPointToSplit, Element& aMailCiteElement) {
  EditorDOMPoint pointToSplit(aPointToSplit);

  // If our selection is just before a break, nudge it to be just after
  // it. This does two things for us.  It saves us the trouble of having
  // to add a break here ourselves to preserve the "blockness" of the
  // inline span mailquote (in the inline case), and : it means the break
  // won't end up making an empty line that happens to be inside a
  // mailquote (in either inline or block case). The latter can confuse a
  // user if they click there and start typing, because being in the
  // mailquote may affect wrapping behavior, or font color, etc.
  const WSScanResult forwardScanFromPointToSplitResult =
      WSRunScanner::ScanInclusiveNextVisibleNodeOrBlockBoundary(
          WSRunScanner::Scan::EditableNodes, pointToSplit,
          BlockInlineCheck::UseHTMLDefaultStyle);
  if (forwardScanFromPointToSplitResult.Failed()) {
    return Err(NS_ERROR_FAILURE);
  }
  // If selection start point is before a break and it's inside the
  // mailquote, let's split it after the visible node.
  if (forwardScanFromPointToSplitResult.ReachedBRElement() &&
      forwardScanFromPointToSplitResult.BRElementPtr() != &aMailCiteElement &&
      aMailCiteElement.Contains(
          forwardScanFromPointToSplitResult.BRElementPtr())) {
    pointToSplit = forwardScanFromPointToSplitResult
                       .PointAfterReachedContent<EditorDOMPoint>();
  }

  if (NS_WARN_IF(!pointToSplit.IsInContentNode())) {
    return Err(NS_ERROR_FAILURE);
  }

  Result<EditorDOMPoint, nsresult> pointToSplitOrError =
      WhiteSpaceVisibilityKeeper::NormalizeWhiteSpacesToSplitAt(
          mHTMLEditor, pointToSplit,
          {WhiteSpaceVisibilityKeeper::NormalizeOption::
               StopIfPrecedingWhiteSpacesEndsWithNBP,
           WhiteSpaceVisibilityKeeper::NormalizeOption::
               StopIfFollowingWhiteSpacesStartsWithNBSP});
  if (MOZ_UNLIKELY(pointToSplitOrError.isErr())) {
    NS_WARNING(
        "WhiteSpaceVisibilityKeeper::NormalizeWhiteSpacesToSplitAt() "
        "failed");
    return pointToSplitOrError.propagateErr();
  }
  pointToSplit = pointToSplitOrError.unwrap();
  if (NS_WARN_IF(!pointToSplit.IsInContentNode())) {
    return Err(NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE);
  }

  Result<SplitNodeResult, nsresult> splitResult =
      mHTMLEditor.SplitNodeDeepWithTransaction(
          aMailCiteElement, pointToSplit,
          SplitAtEdges::eDoNotCreateEmptyContainer);
  if (MOZ_UNLIKELY(splitResult.isErr())) {
    NS_WARNING(
        "HTMLEditor::SplitNodeDeepWithTransaction(aMailCiteElement, "
        "SplitAtEdges::eDoNotCreateEmptyContainer) failed");
    return splitResult;
  }
  // FIXME: We should make the caller handle `Selection`.
  nsresult rv = splitResult.inspect().SuggestCaretPointTo(
      mHTMLEditor, {SuggestCaret::OnlyIfHasSuggestion,
                    SuggestCaret::OnlyIfTransactionsAllowedToDoIt});
  if (NS_FAILED(rv)) {
    NS_WARNING("SplitNodeResult::SuggestCaretPointTo() failed");
    return Err(rv);
  }
  return splitResult;
}

nsresult HTMLEditor::AutoInsertParagraphHandler::
    MaybeInsertPaddingBRElementToInlineMailCiteElement(
        const EditorDOMPoint& aPointToInsertBRElement,
        Element& aMailCiteElement) {
  if (!HTMLEditUtils::IsInlineContent(aMailCiteElement,
                                      BlockInlineCheck::UseHTMLDefaultStyle)) {
    return NS_SUCCESS_DOM_NO_OPERATION;
  }
  // XXX Cannot we replace this complicated check with just a call of
  //     HTMLEditUtils::IsVisibleBRElement with
  //     resultOfInsertingBRElement.inspect()?
  const WSScanResult backwardScanFromPointToCreateNewBRElementResult =
      WSRunScanner::ScanPreviousVisibleNodeOrBlockBoundary(
          WSRunScanner::Scan::EditableNodes, aPointToInsertBRElement,
          BlockInlineCheck::UseHTMLDefaultStyle);
  if (MOZ_UNLIKELY(backwardScanFromPointToCreateNewBRElementResult.Failed())) {
    NS_WARNING(
        "WSRunScanner::ScanPreviousVisibleNodeOrBlockBoundary() "
        "failed");
    return NS_ERROR_FAILURE;
  }
  if (!backwardScanFromPointToCreateNewBRElementResult
           .InVisibleOrCollapsibleCharacters() &&
      !backwardScanFromPointToCreateNewBRElementResult
           .ReachedSpecialContent()) {
    return NS_SUCCESS_DOM_NO_OPERATION;
  }
  const WSScanResult forwardScanFromPointAfterNewBRElementResult =
      WSRunScanner::ScanInclusiveNextVisibleNodeOrBlockBoundary(
          WSRunScanner::Scan::EditableNodes,
          EditorRawDOMPoint::After(aPointToInsertBRElement),
          BlockInlineCheck::UseHTMLDefaultStyle);
  if (MOZ_UNLIKELY(forwardScanFromPointAfterNewBRElementResult.Failed())) {
    NS_WARNING("WSRunScanner::ScanNextVisibleNodeOrBlockBoundary() failed");
    return NS_ERROR_FAILURE;
  }
  if (!forwardScanFromPointAfterNewBRElementResult
           .InVisibleOrCollapsibleCharacters() &&
      !forwardScanFromPointAfterNewBRElementResult.ReachedSpecialContent() &&
      // In case we're at the very end.
      !forwardScanFromPointAfterNewBRElementResult
           .ReachedCurrentBlockBoundary()) {
    return NS_SUCCESS_DOM_NO_OPERATION;
  }
  Result<CreateLineBreakResult, nsresult> insertAnotherBRElementResultOrError =
      mHTMLEditor.InsertLineBreak(WithTransaction::Yes,
                                  LineBreakType::BRElement,
                                  aPointToInsertBRElement);
  if (MOZ_UNLIKELY(insertAnotherBRElementResultOrError.isErr())) {
    NS_WARNING(
        "HTMLEditor::InsertLineBreak(WithTransaction::Yes, "
        "LineBreakType::BRElement) failed");
    return insertAnotherBRElementResultOrError.unwrapErr();
  }
  CreateLineBreakResult insertAnotherBRElementResult =
      insertAnotherBRElementResultOrError.unwrap();
  MOZ_ASSERT(insertAnotherBRElementResult.Handled());
  insertAnotherBRElementResult.IgnoreCaretPointSuggestion();
  return NS_OK;
}

Result<InsertParagraphResult, nsresult>
HTMLEditor::AutoInsertParagraphHandler::HandleInHeadingElement(
    Element& aHeadingElement, const EditorDOMPoint& aPointToSplit) {
  // FIXME: Stop splitting aHeadingElement if it's not required.
  auto splitHeadingResult =
      [this, &aPointToSplit, &aHeadingElement]()
          MOZ_CAN_RUN_SCRIPT -> Result<SplitNodeResult, nsresult> {
    // Normalize collapsible white-spaces around the split point to keep
    // them visible after the split.  Note that this does not touch
    // selection because of using AutoTransactionsConserveSelection in
    // WhiteSpaceVisibilityKeeper::ReplaceTextAndRemoveEmptyTextNodes().
    Result<EditorDOMPoint, nsresult> preparationResult =
        WhiteSpaceVisibilityKeeper::PrepareToSplitBlockElement(
            mHTMLEditor, aPointToSplit, aHeadingElement);
    if (MOZ_UNLIKELY(preparationResult.isErr())) {
      NS_WARNING(
          "WhiteSpaceVisibilityKeeper::PrepareToSplitBlockElement() "
          "failed");
      return preparationResult.propagateErr();
    }
    EditorDOMPoint pointToSplit = preparationResult.unwrap();
    MOZ_ASSERT(pointToSplit.IsInContentNode());

    // Split the header
    Result<SplitNodeResult, nsresult> splitResult =
        mHTMLEditor.SplitNodeDeepWithTransaction(
            aHeadingElement, pointToSplit,
            SplitAtEdges::eAllowToCreateEmptyContainer);
    NS_WARNING_ASSERTION(
        splitResult.isOk(),
        "HTMLEditor::SplitNodeDeepWithTransaction(aHeadingElement, "
        "SplitAtEdges::eAllowToCreateEmptyContainer) failed");
    return splitResult;
  }();
  if (MOZ_UNLIKELY(splitHeadingResult.isErr())) {
    NS_WARNING("Failed to splitting aHeadingElement");
    return splitHeadingResult.propagateErr();
  }
  SplitNodeResult unwrappedSplitHeadingResult = splitHeadingResult.unwrap();
  unwrappedSplitHeadingResult.IgnoreCaretPointSuggestion();
  if (MOZ_UNLIKELY(!unwrappedSplitHeadingResult.DidSplit())) {
    NS_WARNING(
        "HTMLEditor::SplitNodeDeepWithTransaction(SplitAtEdges::"
        "eAllowToCreateEmptyContainer) didn't split aHeadingElement");
    return Err(NS_ERROR_FAILURE);
  }

  // If the left heading element is empty, put a padding <br> element for empty
  // last line into it.
  // FYI: leftHeadingElement is grabbed by unwrappedSplitHeadingResult so that
  //      it's safe to access anytime.
  auto* const leftHeadingElement =
      unwrappedSplitHeadingResult.GetPreviousContentAs<Element>();
  MOZ_ASSERT(leftHeadingElement,
             "SplitNodeResult::GetPreviousContent() should return something if "
             "DidSplit() returns true");
  MOZ_DIAGNOSTIC_ASSERT(HTMLEditUtils::IsHeader(*leftHeadingElement));
  if (HTMLEditUtils::IsEmptyNode(
          *leftHeadingElement,
          {EmptyCheckOption::TreatSingleBRElementAsVisible,
           EmptyCheckOption::TreatNonEditableContentAsInvisible})) {
    Result<CreateElementResult, nsresult> insertPaddingBRElementResult =
        mHTMLEditor.InsertPaddingBRElementForEmptyLastLineWithTransaction(
            EditorDOMPoint(leftHeadingElement, 0u));
    if (MOZ_UNLIKELY(insertPaddingBRElementResult.isErr())) {
      NS_WARNING(
          "HTMLEditor::InsertPaddingBRElementForEmptyLastLineWithTransaction("
          ") failed");
      return insertPaddingBRElementResult.propagateErr();
    }
    insertPaddingBRElementResult.inspect().IgnoreCaretPointSuggestion();
  }

  // Put caret at start of the right head element if it's not empty.
  auto* const rightHeadingElement =
      unwrappedSplitHeadingResult.GetNextContentAs<Element>();
  MOZ_ASSERT(rightHeadingElement,
             "SplitNodeResult::GetNextContent() should return something if "
             "DidSplit() returns true");
  if (!HTMLEditUtils::IsEmptyBlockElement(
          *rightHeadingElement,
          {EmptyCheckOption::TreatNonEditableContentAsInvisible},
          BlockInlineCheck::UseComputedDisplayOutsideStyle)) {
    return InsertParagraphResult(rightHeadingElement,
                                 EditorDOMPoint(rightHeadingElement, 0u));
  }

  // If the right heading element is empty, delete it.
  // TODO: If we know the new heading element becomes empty, we stop spliting
  //       the heading element.
  // MOZ_KnownLive(rightHeadingElement) because it's grabbed by
  // unwrappedSplitHeadingResult.
  nsresult rv = mHTMLEditor.DeleteNodeWithTransaction(
      MOZ_KnownLive(*rightHeadingElement));
  if (NS_FAILED(rv)) {
    NS_WARNING("EditorBase::DeleteNodeWithTransaction() failed");
    return Err(rv);
  }

  // Layout tells the caret to blink in a weird place if we don't place a
  // break after the header.
  // XXX This block is dead code unless the removed right heading element is
  //     reconnected by a mutation event listener.  This is a regression of
  //     bug 1405751:
  //     https://searchfox.org/mozilla-central/diff/879f3317d1331818718e18776caa47be7f426a22/editor/libeditor/HTMLEditRules.cpp#6389
  //     However, the traditional behavior is different from the other browsers.
  //     Chrome creates new paragraph in this case.  Therefore, we should just
  //     drop this block in a follow up bug.
  if (rightHeadingElement->GetNextSibling()) {
    // XXX Ignoring non-editable <br> element here is odd because non-editable
    //     <br> elements also work as <br> from point of view of layout.
    nsIContent* nextEditableSibling =
        HTMLEditUtils::GetNextSibling(*rightHeadingElement->GetNextSibling(),
                                      {WalkTreeOption::IgnoreNonEditableNode});
    if (nextEditableSibling &&
        nextEditableSibling->IsHTMLElement(nsGkAtoms::br)) {
      auto afterEditableBRElement = EditorDOMPoint::After(*nextEditableSibling);
      if (NS_WARN_IF(!afterEditableBRElement.IsSet())) {
        return Err(NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE);
      }
      // Put caret at the <br> element.
      return InsertParagraphResult::NotHandled(
          std::move(afterEditableBRElement));
    }
  }

  if (MOZ_UNLIKELY(!leftHeadingElement->IsInComposedDoc())) {
    NS_WARNING("The left heading element was unexpectedly removed");
    return Err(NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE);
  }

  // XXX This makes HTMLEditor instance stateful.  So, we should move this out
  // from AutoInsertParagraphHandler with adding a method which HTMLEditor can
  // consider to do this.
  mHTMLEditor.TopLevelEditSubActionDataRef().mCachedPendingStyles->Clear();
  mHTMLEditor.mPendingStylesToApplyToNewContent->ClearAllStyles();

  // Create a paragraph if the right heading element is not followed by an
  // editable <br> element.
  nsStaticAtom& newParagraphTagName =
      &mDefaultParagraphSeparatorTagName == nsGkAtoms::br
          ? *nsGkAtoms::p
          : mDefaultParagraphSeparatorTagName;
  // We want a wrapper element even if we separate with a <br>.
  // MOZ_KnownLive(newParagraphTagName) because it's available until shutdown.
  Result<CreateElementResult, nsresult> createNewParagraphElementResult =
      mHTMLEditor.CreateAndInsertElement(
          WithTransaction::Yes, MOZ_KnownLive(newParagraphTagName),
          EditorDOMPoint::After(*leftHeadingElement),
          HTMLEditor::InsertNewBRElement);
  if (MOZ_UNLIKELY(createNewParagraphElementResult.isErr())) {
    NS_WARNING(
        "HTMLEditor::CreateAndInsertElement(WithTransaction::Yes) failed");
    return createNewParagraphElementResult.propagateErr();
  }
  CreateElementResult unwrappedCreateNewParagraphElementResult =
      createNewParagraphElementResult.unwrap();
  // Put caret at the <br> element in the following paragraph.
  unwrappedCreateNewParagraphElementResult.IgnoreCaretPointSuggestion();
  MOZ_ASSERT(unwrappedCreateNewParagraphElementResult.GetNewNode());
  EditorDOMPoint pointToPutCaret(
      unwrappedCreateNewParagraphElementResult.GetNewNode(), 0u);
  return InsertParagraphResult(
      unwrappedCreateNewParagraphElementResult.UnwrapNewNode(),
      std::move(pointToPutCaret));
}

Result<SplitNodeResult, nsresult>
HTMLEditor::AutoInsertParagraphHandler::HandleInParagraph(
    Element& aParentDivOrP, const EditorDOMPoint& aCandidatePointToSplit) {
  MOZ_ASSERT(aCandidatePointToSplit.IsSetAndValid());

  // First, get a better split point to avoid to create a new empty link in the
  // right paragraph.
  EditorDOMPoint pointToSplit = GetBetterSplitPointToAvoidToContinueLink(
      aCandidatePointToSplit, aParentDivOrP);
  MOZ_ASSERT(pointToSplit.IsSetAndValid());

  const bool createNewParagraph =
      mHTMLEditor.GetReturnInParagraphCreatesNewParagraph();
  RefPtr<HTMLBRElement> brElement;
  if (createNewParagraph && pointToSplit.GetContainer() == &aParentDivOrP) {
    // We are try to split only the current paragraph.  Therefore, we don't need
    // to create new <br> elements around it (if left and/or right paragraph
    // becomes empty, it'll be treated by SplitParagraphWithTransaction().
    brElement = nullptr;
  } else if (pointToSplit.IsInTextNode()) {
    if (pointToSplit.IsStartOfContainer()) {
      // If we're splitting the paragraph at start of a text node and there is
      // no preceding visible <br> element, we need to create a <br> element to
      // keep the inline elements containing this text node.
      // TODO: If the parent of the text node is the splitting paragraph,
      //       obviously we don't need to do this because empty paragraphs will
      //       be treated by SplitParagraphWithTransaction().  In this case, we
      //       just need to update pointToSplit for using the same path as the
      //       previous `if` block.
      brElement =
          HTMLBRElement::FromNodeOrNull(HTMLEditUtils::GetPreviousSibling(
              *pointToSplit.ContainerAs<Text>(),
              {WalkTreeOption::IgnoreNonEditableNode}));
      if (!brElement || HTMLEditUtils::IsInvisibleBRElement(*brElement) ||
          EditorUtils::IsPaddingBRElementForEmptyLastLine(*brElement)) {
        // If insertParagraph does not create a new paragraph, default to
        // insertLineBreak.
        if (!createNewParagraph) {
          return SplitNodeResult::NotHandled(pointToSplit);
        }
        const EditorDOMPoint pointToInsertBR = pointToSplit.ParentPoint();
        MOZ_ASSERT(pointToInsertBR.IsSet());
        if (pointToInsertBR.IsInContentNode() &&
            HTMLEditUtils::CanNodeContain(
                *pointToInsertBR.ContainerAs<nsIContent>(), *nsGkAtoms::br)) {
          Result<CreateLineBreakResult, nsresult> insertBRElementResultOrError =
              mHTMLEditor.InsertLineBreak(WithTransaction::Yes,
                                          LineBreakType::BRElement,
                                          pointToInsertBR);
          if (MOZ_UNLIKELY(insertBRElementResultOrError.isErr())) {
            NS_WARNING(
                "HTMLEditor::InsertLineBreak(WithTransaction::Yes, "
                "LineBreakType::BRElement) failed");
            return insertBRElementResultOrError.propagateErr();
          }
          CreateLineBreakResult insertBRElementResult =
              insertBRElementResultOrError.unwrap();
          // We'll collapse `Selection` to the place suggested by
          // SplitParagraphWithTransaction.
          insertBRElementResult.IgnoreCaretPointSuggestion();
          brElement = &insertBRElementResult->BRElementRef();
        }
      }
    } else if (pointToSplit.IsEndOfContainer()) {
      // If we're splitting the paragraph at end of a text node and there is not
      // following visible <br> element, we need to create a <br> element after
      // the text node to make current style specified by parent inline elements
      // keep in the right paragraph.
      // TODO: Same as above, we don't need to do this if the text node is a
      //       direct child of the paragraph.  For using the simplest path, we
      //       just need to update `pointToSplit` in the case.
      brElement = HTMLBRElement::FromNodeOrNull(HTMLEditUtils::GetNextSibling(
          *pointToSplit.ContainerAs<Text>(),
          {WalkTreeOption::IgnoreNonEditableNode}));
      if (!brElement || HTMLEditUtils::IsInvisibleBRElement(*brElement) ||
          EditorUtils::IsPaddingBRElementForEmptyLastLine(*brElement)) {
        // If insertParagraph does not create a new paragraph, default to
        // insertLineBreak.
        if (!createNewParagraph) {
          return SplitNodeResult::NotHandled(pointToSplit);
        }
        const auto pointToInsertBR =
            EditorDOMPoint::After(*pointToSplit.ContainerAs<Text>());
        MOZ_ASSERT(pointToInsertBR.IsSet());
        if (pointToInsertBR.IsInContentNode() &&
            HTMLEditUtils::CanNodeContain(
                *pointToInsertBR.ContainerAs<nsIContent>(), *nsGkAtoms::br)) {
          Result<CreateLineBreakResult, nsresult> insertBRElementResultOrError =
              mHTMLEditor.InsertLineBreak(WithTransaction::Yes,
                                          LineBreakType::BRElement,
                                          pointToInsertBR);
          if (MOZ_UNLIKELY(insertBRElementResultOrError.isErr())) {
            NS_WARNING(
                "HTMLEditor::InsertLineBreak(WithTransaction::Yes, "
                "LineBreakType::BRElement) failed");
            return insertBRElementResultOrError.propagateErr();
          }
          CreateLineBreakResult insertBRElementResult =
              insertBRElementResultOrError.unwrap();
          // We'll collapse `Selection` to the place suggested by
          // SplitParagraphWithTransaction.
          insertBRElementResult.IgnoreCaretPointSuggestion();
          brElement = &insertBRElementResult->BRElementRef();
        }
      }
    } else {
      // If insertParagraph does not create a new paragraph, default to
      // insertLineBreak.
      if (!createNewParagraph) {
        return SplitNodeResult::NotHandled(pointToSplit);
      }

      // If we're splitting the paragraph at middle of a text node, we should
      // split the text node here and put a <br> element next to the left text
      // node.
      // XXX Why? I think that this should be handled in
      //     SplitParagraphWithTransaction() directly because I don't find
      //     the necessary case of the <br> element.

      // XXX We split a text node here if caret is middle of it to insert
      //     <br> element **before** splitting aParentDivOrP.  Then, if
      //     the <br> element becomes unnecessary, it'll be removed again.
      //     So this does much more complicated things than what we want to
      //     do here.  We should handle this case separately to make the code
      //     much simpler.

      // Normalize collapsible white-spaces around the split point to keep
      // them visible after the split.  Note that this does not touch
      // selection because of using AutoTransactionsConserveSelection in
      // WhiteSpaceVisibilityKeeper::ReplaceTextAndRemoveEmptyTextNodes().
      Result<EditorDOMPoint, nsresult> pointToSplitOrError =
          WhiteSpaceVisibilityKeeper::PrepareToSplitBlockElement(
              mHTMLEditor, pointToSplit, aParentDivOrP);
      if (NS_WARN_IF(mHTMLEditor.Destroyed())) {
        return Err(NS_ERROR_EDITOR_DESTROYED);
      }
      if (MOZ_UNLIKELY(pointToSplitOrError.isErr())) {
        NS_WARNING(
            "WhiteSpaceVisibilityKeeper::PrepareToSplitBlockElement() "
            "failed");
        return pointToSplitOrError.propagateErr();
      }
      MOZ_ASSERT(pointToSplitOrError.inspect().IsSetAndValid());
      if (pointToSplitOrError.inspect().IsSet()) {
        pointToSplit = pointToSplitOrError.unwrap();
      }
      Result<SplitNodeResult, nsresult> splitParentDivOrPResult =
          mHTMLEditor.SplitNodeWithTransaction(pointToSplit);
      if (MOZ_UNLIKELY(splitParentDivOrPResult.isErr())) {
        NS_WARNING("HTMLEditor::SplitNodeWithTransaction() failed");
        return splitParentDivOrPResult;
      }
      // We'll collapse `Selection` to the place suggested by
      // SplitParagraphWithTransaction.
      splitParentDivOrPResult.inspect().IgnoreCaretPointSuggestion();

      pointToSplit.SetToEndOf(
          splitParentDivOrPResult.inspect().GetPreviousContent());
      if (NS_WARN_IF(!pointToSplit.IsInContentNode())) {
        return Err(NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE);
      }

      // We need to put new <br> after the left node if given node was split
      // above.
      const auto pointToInsertBR =
          EditorDOMPoint::After(*pointToSplit.ContainerAs<nsIContent>());
      MOZ_ASSERT(pointToInsertBR.IsSet());
      if (pointToInsertBR.IsInContentNode() &&
          HTMLEditUtils::CanNodeContain(
              *pointToInsertBR.ContainerAs<nsIContent>(), *nsGkAtoms::br)) {
        AutoTrackDOMPoint trackPointToSplit(mHTMLEditor.RangeUpdaterRef(),
                                            &pointToSplit);
        Result<CreateLineBreakResult, nsresult> insertBRElementResultOrError =
            mHTMLEditor.InsertLineBreak(WithTransaction::Yes,
                                        LineBreakType::BRElement,
                                        pointToInsertBR);
        if (MOZ_UNLIKELY(insertBRElementResultOrError.isErr())) {
          NS_WARNING(
              "HTMLEditor::InsertLineBreak(WithTransaction::Yes, "
              "LineBreakType::BRElement) failed");
          return insertBRElementResultOrError.propagateErr();
        }
        CreateLineBreakResult insertBRElementResult =
            insertBRElementResultOrError.unwrap();
        // We'll collapse `Selection` to the place suggested by
        // SplitParagraphWithTransaction.
        insertBRElementResult.IgnoreCaretPointSuggestion();
        brElement = &insertBRElementResult->BRElementRef();
        trackPointToSplit.FlushAndStopTracking();
        if (NS_WARN_IF(!pointToSplit.IsInContentNodeAndValidInComposedDoc())) {
          return Err(NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE);
        }
      }
    }
  } else {
    // If we're splitting in a child element of the paragraph, and there is no
    // <br> element around it, we should insert a <br> element at the split
    // point and keep splitting the paragraph after the new <br> element.
    // XXX Why? We probably need to do this if we're splitting in an inline
    //     element which and whose parents provide some styles, we should put
    //     the <br> element for making a placeholder in the left paragraph for
    //     moving to the caret, but I think that this could be handled in fewer
    //     cases than this.
    brElement = HTMLBRElement::FromNodeOrNull(HTMLEditUtils::GetPreviousContent(
        pointToSplit, {WalkTreeOption::IgnoreNonEditableNode},
        BlockInlineCheck::Unused, &mEditingHost));
    if (!brElement || HTMLEditUtils::IsInvisibleBRElement(*brElement) ||
        EditorUtils::IsPaddingBRElementForEmptyLastLine(*brElement)) {
      // is there a BR after it?
      brElement = HTMLBRElement::FromNodeOrNull(HTMLEditUtils::GetNextContent(
          pointToSplit, {WalkTreeOption::IgnoreNonEditableNode},
          BlockInlineCheck::Unused, &mEditingHost));
      if (!brElement || HTMLEditUtils::IsInvisibleBRElement(*brElement) ||
          EditorUtils::IsPaddingBRElementForEmptyLastLine(*brElement)) {
        // If insertParagraph does not create a new paragraph, default to
        // insertLineBreak.
        if (!createNewParagraph) {
          return SplitNodeResult::NotHandled(pointToSplit);
        }
        if (pointToSplit.IsInContentNode() &&
            HTMLEditUtils::CanNodeContain(
                *pointToSplit.ContainerAs<nsIContent>(), *nsGkAtoms::br)) {
          Result<CreateLineBreakResult, nsresult> insertBRElementResultOrError =
              mHTMLEditor.InsertLineBreak(
                  WithTransaction::Yes, LineBreakType::BRElement, pointToSplit);
          if (MOZ_UNLIKELY(insertBRElementResultOrError.isErr())) {
            NS_WARNING(
                "HTMLEditor::InsertLineBreak(WithTransaction::Yes, "
                "LineBreakType::BRElement) failed");
            return insertBRElementResultOrError.propagateErr();
          }
          CreateLineBreakResult insertBRElementResult =
              insertBRElementResultOrError.unwrap();
          // We'll collapse `Selection` to the place suggested by
          // SplitParagraphWithTransaction.
          insertBRElementResult.IgnoreCaretPointSuggestion();
          brElement = &insertBRElementResult->BRElementRef();
          // We split the parent after the <br>.
          pointToSplit.SetAfter(brElement);
          if (NS_WARN_IF(!pointToSplit.IsSet())) {
            return Err(NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE);
          }
        }
      }
    }
  }

  Result<SplitNodeResult, nsresult> splitParagraphResult =
      SplitParagraphWithTransaction(aParentDivOrP, pointToSplit, brElement);
  if (MOZ_UNLIKELY(splitParagraphResult.isErr())) {
    NS_WARNING(
        "AutoInsertParagraphHandler::SplitParagraphWithTransaction() failed");
    return splitParagraphResult;
  }
  if (MOZ_UNLIKELY(!splitParagraphResult.inspect().DidSplit())) {
    NS_WARNING(
        "AutoInsertParagraphHandler::SplitParagraphWithTransaction() didn't "
        "split the paragraph");
    splitParagraphResult.inspect().IgnoreCaretPointSuggestion();
    return Err(NS_ERROR_FAILURE);
  }
  MOZ_ASSERT(splitParagraphResult.inspect().Handled());
  return splitParagraphResult;
}

// static
EditorDOMPoint HTMLEditor::AutoInsertParagraphHandler::
    GetBetterSplitPointToAvoidToContinueLink(
        const EditorDOMPoint& aCandidatePointToSplit,
        const Element& aElementToSplit) {
  // We shouldn't create new anchor element which has non-empty href unless
  // splitting middle of it because we assume that users don't want to create
  // *same* anchor element across two or more paragraphs in most cases.
  // So, adjust selection start if it's edge of anchor element(s).
  // XXX We don't support white-space collapsing in these cases since it needs
  //     some additional work with WhiteSpaceVisibilityKeeper but it's not
  //     usual case. E.g., |<a href="foo"><b>foo []</b> </a>|
  if (aCandidatePointToSplit.IsStartOfContainer()) {
    EditorDOMPoint candidatePoint(aCandidatePointToSplit);
    for (nsIContent* container =
             aCandidatePointToSplit.GetContainerAs<nsIContent>();
         container && container != &aElementToSplit;
         container = container->GetParent()) {
      if (HTMLEditUtils::IsLink(container)) {
        // Found link should be only in right node.  So, we shouldn't split
        // it.
        candidatePoint.Set(container);
        // Even if we found an anchor element, don't break because DOM API
        // allows to nest anchor elements.
      }
      // If the container is middle of its parent, stop adjusting split point.
      if (container->GetPreviousSibling()) {
        // XXX Should we check if previous sibling is visible content?
        //     E.g., should we ignore comment node, invisible <br> element?
        break;
      }
    }
    return candidatePoint;
  }

  // We also need to check if selection is at invisible <br> element at end
  // of an <a href="foo"> element because editor inserts a <br> element when
  // user types Enter key after a white-space which is at middle of
  // <a href="foo"> element and when setting selection at end of the element,
  // selection becomes referring the <br> element.  We may need to change this
  // behavior later if it'd be standardized.
  if (!aCandidatePointToSplit.IsEndOfContainer() &&
      !aCandidatePointToSplit.IsBRElementAtEndOfContainer()) {
    return aCandidatePointToSplit;
  }
  // If there are 2 <br> elements, the first <br> element is visible.  E.g.,
  // |<a href="foo"><b>boo[]<br></b><br></a>|, we should split the <a>
  // element.  Otherwise, E.g., |<a href="foo"><b>boo[]<br></b></a>|,
  // we should not split the <a> element and ignore inline elements in it.
  bool foundBRElement = aCandidatePointToSplit.IsBRElementAtEndOfContainer();
  EditorDOMPoint candidatePoint(aCandidatePointToSplit);
  for (nsIContent* container =
           aCandidatePointToSplit.GetContainerAs<nsIContent>();
       container && container != &aElementToSplit;
       container = container->GetParent()) {
    if (HTMLEditUtils::IsLink(container)) {
      // Found link should be only in left node.  So, we shouldn't split it.
      candidatePoint.SetAfter(container);
      // Even if we found an anchor element, don't break because DOM API
      // allows to nest anchor elements.
    }
    // If the container is middle of its parent, stop adjusting split point.
    if (nsIContent* nextSibling = container->GetNextSibling()) {
      if (foundBRElement) {
        // If we've already found a <br> element, we assume found node is
        // visible <br> or something other node.
        // XXX Should we check if non-text data node like comment?
        break;
      }

      // XXX Should we check if non-text data node like comment?
      if (!nextSibling->IsHTMLElement(nsGkAtoms::br)) {
        break;
      }
      foundBRElement = true;
    }
  }
  return candidatePoint;
}

Result<SplitNodeResult, nsresult>
HTMLEditor::AutoInsertParagraphHandler::SplitParagraphWithTransaction(
    Element& aParentDivOrP, const EditorDOMPoint& aStartOfRightNode,
    HTMLBRElement* aMayBecomeVisibleBRElement) {
  Result<EditorDOMPoint, nsresult> preparationResult =
      WhiteSpaceVisibilityKeeper::PrepareToSplitBlockElement(
          mHTMLEditor, aStartOfRightNode, aParentDivOrP);
  if (MOZ_UNLIKELY(preparationResult.isErr())) {
    NS_WARNING(
        "WhiteSpaceVisibilityKeeper::PrepareToSplitBlockElement() failed");
    return preparationResult.propagateErr();
  }
  EditorDOMPoint pointToSplit = preparationResult.unwrap();
  MOZ_ASSERT(pointToSplit.IsInContentNode());

  // Split the paragraph.
  Result<SplitNodeResult, nsresult> splitDivOrPResult =
      mHTMLEditor.SplitNodeDeepWithTransaction(
          aParentDivOrP, pointToSplit,
          SplitAtEdges::eAllowToCreateEmptyContainer);
  if (MOZ_UNLIKELY(splitDivOrPResult.isErr())) {
    NS_WARNING("HTMLEditor::SplitNodeDeepWithTransaction() failed");
    return splitDivOrPResult;
  }
  SplitNodeResult unwrappedSplitDivOrPResult = splitDivOrPResult.unwrap();
  if (MOZ_UNLIKELY(!unwrappedSplitDivOrPResult.DidSplit())) {
    NS_WARNING(
        "HTMLEditor::SplitNodeDeepWithTransaction() didn't split any nodes");
    return unwrappedSplitDivOrPResult;
  }

  // We'll compute caret suggestion later.  So the simple result is not needed.
  unwrappedSplitDivOrPResult.IgnoreCaretPointSuggestion();

  auto* const leftDivOrParagraphElement =
      unwrappedSplitDivOrPResult.GetPreviousContentAs<Element>();
  MOZ_ASSERT(leftDivOrParagraphElement,
             "SplitNodeResult::GetPreviousContent() should return something if "
             "DidSplit() returns true");
  auto* const rightDivOrParagraphElement =
      unwrappedSplitDivOrPResult.GetNextContentAs<Element>();
  MOZ_ASSERT(rightDivOrParagraphElement,
             "SplitNodeResult::GetNextContent() should return something if "
             "DidSplit() returns true");

  // Get rid of the break, if it is visible (otherwise it may be needed to
  // prevent an empty p).
  if (aMayBecomeVisibleBRElement &&
      HTMLEditUtils::IsVisibleBRElement(*aMayBecomeVisibleBRElement)) {
    nsresult rv =
        mHTMLEditor.DeleteNodeWithTransaction(*aMayBecomeVisibleBRElement);
    if (NS_FAILED(rv)) {
      NS_WARNING("EditorBase::DeleteNodeWithTransaction() failed");
      return Err(rv);
    }
  }

  // Remove ID attribute on the paragraph from the right node.
  // MOZ_KnownLive(rightDivOrParagraphElement) because it's grabbed by
  // unwrappedSplitDivOrPResult.
  nsresult rv = mHTMLEditor.RemoveAttributeWithTransaction(
      MOZ_KnownLive(*rightDivOrParagraphElement), *nsGkAtoms::id);
  if (NS_FAILED(rv)) {
    NS_WARNING(
        "EditorBase::RemoveAttributeWithTransaction(nsGkAtoms::id) failed");
    return Err(rv);
  }

  // We need to ensure to both paragraphs visible even if they are empty.
  // However, padding <br> element for empty last line isn't useful in this
  // case because it'll be ignored by PlaintextSerializer.  Additionally,
  // it'll be exposed as <br> with Element.innerHTML.  Therefore, we can use
  // normal <br> elements for placeholder in this case.  Note that Chromium
  // also behaves so.

  // MOZ_KnownLive(leftDivOrParagraphElement) because it's grabbed by
  // splitDivOrResult.
  {
    Result<CreateLineBreakResult, nsresult> insertBRElementResultOrError =
        InsertBRElementIfEmptyBlockElement(
            MOZ_KnownLive(*leftDivOrParagraphElement),
            InsertBRElementIntoEmptyBlock::Start,
            BlockInlineCheck::UseComputedDisplayStyle);
    if (MOZ_UNLIKELY(insertBRElementResultOrError.isErr())) {
      NS_WARNING(
          "InsertBRElementIfEmptyBlockElement(leftDivOrParagraphElement, "
          "InsertBRElementIntoEmptyBlock::Start, "
          "BlockInlineCheck::UseComputedDisplayStyle) failed");
      return Err(rv);
    }
    insertBRElementResultOrError.unwrap().IgnoreCaretPointSuggestion();
  }

  if (HTMLEditUtils::IsEmptyNode(*rightDivOrParagraphElement)) {
    // If the right paragraph is empty, it might have an empty inline element
    // (which may contain other empty inline containers) and optionally a <br>
    // element which may not be in the deepest inline element.
    if (const RefPtr<Element> deepestInlineContainerElement =
            GetDeepestFirstChildInlineContainerElement(
                *rightDivOrParagraphElement)) {
      const Maybe<EditorLineBreak> lineBreak =
          HTMLEditUtils::GetFirstLineBreak<EditorLineBreak>(
              *rightDivOrParagraphElement);
      if (lineBreak.isSome()) {
        // If there is a <br> element and it is in the deepest inline container,
        // we need to do nothing anymore. Let's suggest caret position as at the
        // <br>.
        if (lineBreak->IsHTMLBRElement() &&
            lineBreak->BRElementRef().GetParentNode() ==
                deepestInlineContainerElement) {
          auto pointAtBRElement = lineBreak->To<EditorDOMPoint>();
          {
            AutoEditorDOMPointChildInvalidator lockOffset(pointAtBRElement);
            nsresult rv = mHTMLEditor.UpdateBRElementType(
                MOZ_KnownLive(lineBreak->BRElementRef()),
                BRElementType::PaddingForEmptyLastLine);
            if (NS_FAILED(rv)) {
              NS_WARNING("EditorBase::UpdateBRElementType() failed");
              return Err(rv);
            }
          }
          return SplitNodeResult(std::move(unwrappedSplitDivOrPResult),
                                 pointAtBRElement);
        }
        // Otherwise, we should put a padding line break into the deepest
        // inline container and then, existing line break (if there is)
        // becomes unnecessary.
        Result<EditorDOMPoint, nsresult> lineBreakPointOrError =
            mHTMLEditor.DeleteLineBreakWithTransaction(
                lineBreak.ref(), nsIEditor::eStrip, mEditingHost);
        if (MOZ_UNLIKELY(lineBreakPointOrError.isErr())) {
          NS_WARNING("HTMLEditor::DeleteLineBreakWithTransaction() failed");
          return lineBreakPointOrError.propagateErr();
        }
        Result<CreateElementResult, nsresult> insertPaddingBRElementResult =
            mHTMLEditor.InsertPaddingBRElementForEmptyLastLineWithTransaction(
                EditorDOMPoint::AtEndOf(deepestInlineContainerElement));
        if (MOZ_UNLIKELY(insertPaddingBRElementResult.isErr())) {
          NS_WARNING(
              "HTMLEditor::"
              "InsertPaddingBRElementForEmptyLastLineWithTransaction() failed");
          return insertPaddingBRElementResult.propagateErr();
        }
        insertPaddingBRElementResult.inspect().IgnoreCaretPointSuggestion();
        return SplitNodeResult(
            std::move(unwrappedSplitDivOrPResult),
            EditorDOMPoint(
                insertPaddingBRElementResult.inspect().GetNewNode()));
      }
    }

    // If there is no inline container elements, we just need to make the
    // right paragraph visible.
    Result<CreateLineBreakResult, nsresult> insertBRElementResultOrError =
        InsertBRElementIfEmptyBlockElement(
            MOZ_KnownLive(*rightDivOrParagraphElement),
            InsertBRElementIntoEmptyBlock::Start,
            BlockInlineCheck::UseComputedDisplayStyle);
    if (MOZ_UNLIKELY(insertBRElementResultOrError.isErr())) {
      NS_WARNING(
          "InsertBRElementIfEmptyBlockElement(rightDivOrParagraphElement, "
          "InsertBRElementIntoEmptyBlock::Start, "
          "BlockInlineCheck::UseComputedDisplayStyle) failed");
      return insertBRElementResultOrError.propagateErr();
    }
    insertBRElementResultOrError.unwrap().IgnoreCaretPointSuggestion();
  }

  // Let's put caret at start of the first leaf container.
  nsIContent* child = HTMLEditUtils::GetFirstLeafContent(
      *rightDivOrParagraphElement, {LeafNodeType::LeafNodeOrChildBlock},
      BlockInlineCheck::UseComputedDisplayStyle);
  if (MOZ_UNLIKELY(!child)) {
    return SplitNodeResult(std::move(unwrappedSplitDivOrPResult),
                           EditorDOMPoint(rightDivOrParagraphElement, 0u));
  }
  return child->IsText() || HTMLEditUtils::IsContainerNode(*child)
             ? SplitNodeResult(std::move(unwrappedSplitDivOrPResult),
                               EditorDOMPoint(child, 0u))
             : SplitNodeResult(std::move(unwrappedSplitDivOrPResult),
                               EditorDOMPoint(child));
}

Result<CreateLineBreakResult, nsresult>
HTMLEditor::AutoInsertParagraphHandler::InsertBRElementIfEmptyBlockElement(
    Element& aMaybeBlockElement,
    InsertBRElementIntoEmptyBlock aInsertBRElementIntoEmptyBlock,
    BlockInlineCheck aBlockInlineCheck) {
  if (!HTMLEditUtils::IsBlockElement(aMaybeBlockElement, aBlockInlineCheck)) {
    return CreateLineBreakResult::NotHandled();
  }

  if (!HTMLEditUtils::IsEmptyNode(
          aMaybeBlockElement,
          {EmptyCheckOption::TreatSingleBRElementAsVisible})) {
    return CreateLineBreakResult::NotHandled();
  }

  // XXX: Probably, we should use
  //      InsertPaddingBRElementForEmptyLastLineWithTransaction here, and
  //      if there are some empty inline container, we should put the <br>
  //      into the last one.
  Result<CreateLineBreakResult, nsresult> insertBRElementResultOrError =
      mHTMLEditor.InsertLineBreak(
          WithTransaction::Yes, LineBreakType::BRElement,
          aInsertBRElementIntoEmptyBlock == InsertBRElementIntoEmptyBlock::Start
              ? EditorDOMPoint(&aMaybeBlockElement, 0u)
              : EditorDOMPoint::AtEndOf(aMaybeBlockElement));
  NS_WARNING_ASSERTION(insertBRElementResultOrError.isOk(),
                       "HTMLEditor::InsertLineBreak(WithTransaction::Yes, "
                       "LineBreakType::BRElement) failed");
  return insertBRElementResultOrError;
}

// static
Element* HTMLEditor::AutoInsertParagraphHandler::
    GetDeepestFirstChildInlineContainerElement(Element& aBlockElement) {
  // XXX Should we ignore invisible children like empty Text, Comment, etc?
  Element* result = nullptr;
  for (Element* maybeDeepestInlineContainer =
           Element::FromNodeOrNull(aBlockElement.GetFirstChild());
       maybeDeepestInlineContainer &&
       HTMLEditUtils::IsInlineContent(
           *maybeDeepestInlineContainer,
           BlockInlineCheck::UseComputedDisplayStyle) &&
       HTMLEditUtils::IsContainerNode(*maybeDeepestInlineContainer);
       // FIXME: There may be visible node before first element child, so, here
       // is obviously wrong.
       maybeDeepestInlineContainer =
           maybeDeepestInlineContainer->GetFirstElementChild()) {
    result = maybeDeepestInlineContainer;
  }
  return result;
}

Result<InsertParagraphResult, nsresult>
HTMLEditor::AutoInsertParagraphHandler::HandleInListItemElement(
    Element& aListItemElement, const EditorDOMPoint& aPointToSplit) {
  MOZ_ASSERT(HTMLEditUtils::IsListItem(&aListItemElement));

  // If aListItemElement is empty, then we want to outdent its content.
  if (&mEditingHost != aListItemElement.GetParentElement() &&
      HTMLEditUtils::IsEmptyBlockElement(
          aListItemElement,
          {EmptyCheckOption::TreatNonEditableContentAsInvisible},
          BlockInlineCheck::UseComputedDisplayOutsideStyle)) {
    RefPtr<Element> leftListElement = aListItemElement.GetParentElement();
    // If the given list item element is not the last list item element of
    // its parent nor not followed by sub list elements, split the parent
    // before it.
    if (!HTMLEditUtils::IsLastChild(aListItemElement,
                                    {WalkTreeOption::IgnoreNonEditableNode})) {
      Result<SplitNodeResult, nsresult> splitListItemParentResult =
          mHTMLEditor.SplitNodeWithTransaction(
              EditorDOMPoint(&aListItemElement));
      if (MOZ_UNLIKELY(splitListItemParentResult.isErr())) {
        NS_WARNING("HTMLEditor::SplitNodeWithTransaction() failed");
        return splitListItemParentResult.propagateErr();
      }
      SplitNodeResult unwrappedSplitListItemParentResult =
          splitListItemParentResult.unwrap();
      if (MOZ_UNLIKELY(!unwrappedSplitListItemParentResult.DidSplit())) {
        NS_WARNING(
            "HTMLEditor::SplitNodeWithTransaction() didn't split the parent of "
            "aListItemElement");
        MOZ_ASSERT(
            !unwrappedSplitListItemParentResult.HasCaretPointSuggestion());
        return Err(NS_ERROR_FAILURE);
      }
      unwrappedSplitListItemParentResult.IgnoreCaretPointSuggestion();
      leftListElement =
          unwrappedSplitListItemParentResult.GetPreviousContentAs<Element>();
      MOZ_DIAGNOSTIC_ASSERT(leftListElement);
    }

    auto afterLeftListElement = EditorDOMPoint::After(leftListElement);
    if (MOZ_UNLIKELY(!afterLeftListElement.IsSet())) {
      return Err(NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE);
    }

    // If aListItemElement is in an invalid sub-list element, move it into
    // the grand parent list element in order to outdent.
    if (HTMLEditUtils::IsAnyListElement(afterLeftListElement.GetContainer())) {
      Result<MoveNodeResult, nsresult> moveListItemElementResult =
          mHTMLEditor.MoveNodeWithTransaction(aListItemElement,
                                              afterLeftListElement);
      if (MOZ_UNLIKELY(moveListItemElementResult.isErr())) {
        NS_WARNING("HTMLEditor::MoveNodeWithTransaction() failed");
        return moveListItemElementResult.propagateErr();
      }
      moveListItemElementResult.inspect().IgnoreCaretPointSuggestion();
      return InsertParagraphResult(&aListItemElement,
                                   EditorDOMPoint(&aListItemElement, 0u));
    }

    // Otherwise, replace the empty aListItemElement with a new paragraph.
    nsresult rv = mHTMLEditor.DeleteNodeWithTransaction(aListItemElement);
    if (NS_FAILED(rv)) {
      NS_WARNING("EditorBase::DeleteNodeWithTransaction() failed");
      return Err(rv);
    }
    nsStaticAtom& newParagraphTagName =
        &mDefaultParagraphSeparatorTagName == nsGkAtoms::br
            ? *nsGkAtoms::p
            : mDefaultParagraphSeparatorTagName;
    // MOZ_KnownLive(newParagraphTagName) because it's available until shutdown.
    Result<CreateElementResult, nsresult> createNewParagraphElementResult =
        mHTMLEditor.CreateAndInsertElement(
            WithTransaction::Yes, MOZ_KnownLive(newParagraphTagName),
            afterLeftListElement, HTMLEditor::InsertNewBRElement);
    if (MOZ_UNLIKELY(createNewParagraphElementResult.isErr())) {
      NS_WARNING(
          "HTMLEditor::CreateAndInsertElement(WithTransaction::Yes) failed");
      return createNewParagraphElementResult.propagateErr();
    }
    createNewParagraphElementResult.inspect().IgnoreCaretPointSuggestion();
    MOZ_ASSERT(createNewParagraphElementResult.inspect().GetNewNode());
    EditorDOMPoint pointToPutCaret(
        createNewParagraphElementResult.inspect().GetNewNode(), 0u);
    return InsertParagraphResult(
        createNewParagraphElementResult.inspect().GetNewNode(),
        std::move(pointToPutCaret));
  }

  // If aListItemElement has some content or aListItemElement is empty but it's
  // a child of editing host, we want a new list item at the same list level.
  // First, sort out white-spaces.
  Result<EditorDOMPoint, nsresult> preparationResult =
      WhiteSpaceVisibilityKeeper::PrepareToSplitBlockElement(
          mHTMLEditor, aPointToSplit, aListItemElement);
  if (preparationResult.isErr()) {
    NS_WARNING(
        "WhiteSpaceVisibilityKeeper::PrepareToSplitBlockElement() failed");
    return Err(preparationResult.unwrapErr());
  }
  EditorDOMPoint pointToSplit = preparationResult.unwrap();
  MOZ_ASSERT(pointToSplit.IsInContentNode());

  // Now split the list item.
  Result<SplitNodeResult, nsresult> splitListItemResult =
      mHTMLEditor.SplitNodeDeepWithTransaction(
          aListItemElement, pointToSplit,
          SplitAtEdges::eAllowToCreateEmptyContainer);
  if (MOZ_UNLIKELY(splitListItemResult.isErr())) {
    NS_WARNING("HTMLEditor::SplitNodeDeepWithTransaction() failed");
    return splitListItemResult.propagateErr();
  }
  SplitNodeResult unwrappedSplitListItemElement = splitListItemResult.unwrap();
  unwrappedSplitListItemElement.IgnoreCaretPointSuggestion();
  if (MOZ_UNLIKELY(!aListItemElement.GetParent())) {
    NS_WARNING("Somebody disconnected the target listitem from the parent");
    return Err(NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE);
  }

  // If aListItemElement is not replaced, we should not do anything anymore.
  if (MOZ_UNLIKELY(!unwrappedSplitListItemElement.DidSplit()) ||
      NS_WARN_IF(!unwrappedSplitListItemElement.GetNewContentAs<Element>()) ||
      NS_WARN_IF(
          !unwrappedSplitListItemElement.GetOriginalContentAs<Element>())) {
    NS_WARNING("HTMLEditor::SplitNodeDeepWithTransaction() didn't split");
    return Err(NS_ERROR_FAILURE);
  }

  // FYI: They are grabbed by unwrappedSplitListItemElement so that they are
  // known live
  //      things.
  auto& leftListItemElement =
      *unwrappedSplitListItemElement.GetPreviousContentAs<Element>();
  auto& rightListItemElement =
      *unwrappedSplitListItemElement.GetNextContentAs<Element>();

  // Hack: until I can change the damaged doc range code back to being
  // extra-inclusive, I have to manually detect certain list items that may be
  // left empty.
  if (HTMLEditUtils::IsEmptyNode(
          leftListItemElement,
          {EmptyCheckOption::TreatSingleBRElementAsVisible,
           EmptyCheckOption::TreatNonEditableContentAsInvisible})) {
    Result<CreateElementResult, nsresult> insertPaddingBRElementResult =
        mHTMLEditor.InsertPaddingBRElementForEmptyLastLineWithTransaction(
            EditorDOMPoint(&leftListItemElement, 0u));
    if (MOZ_UNLIKELY(insertPaddingBRElementResult.isErr())) {
      NS_WARNING(
          "HTMLEditor::InsertPaddingBRElementForEmptyLastLineWithTransaction("
          ") failed");
      return insertPaddingBRElementResult.propagateErr();
    }
    // We're returning a candidate point to put caret so that we don't need to
    // update now.
    insertPaddingBRElementResult.inspect().IgnoreCaretPointSuggestion();
    return InsertParagraphResult(&rightListItemElement,
                                 EditorDOMPoint(&rightListItemElement, 0u));
  }

  if (HTMLEditUtils::IsEmptyNode(
          rightListItemElement,
          {EmptyCheckOption::TreatNonEditableContentAsInvisible})) {
    // If aListItemElement is a <dd> or a <dt> and the right list item is empty
    // or a direct child of the editing host, replace it a new list item element
    // whose type is the other one.
    if (aListItemElement.IsAnyOfHTMLElements(nsGkAtoms::dd, nsGkAtoms::dt)) {
      nsStaticAtom& nextDefinitionListItemTagName =
          aListItemElement.IsHTMLElement(nsGkAtoms::dt) ? *nsGkAtoms::dd
                                                        : *nsGkAtoms::dt;
      // MOZ_KnownLive(nextDefinitionListItemTagName) because it's available
      // until shutdown.
      Result<CreateElementResult, nsresult> createNewListItemElementResult =
          mHTMLEditor.CreateAndInsertElement(
              WithTransaction::Yes,
              MOZ_KnownLive(nextDefinitionListItemTagName),
              EditorDOMPoint::After(rightListItemElement));
      if (MOZ_UNLIKELY(createNewListItemElementResult.isErr())) {
        NS_WARNING(
            "HTMLEditor::CreateAndInsertElement(WithTransaction::Yes) failed");
        return createNewListItemElementResult.propagateErr();
      }
      CreateElementResult unwrappedCreateNewListItemElementResult =
          createNewListItemElementResult.unwrap();
      unwrappedCreateNewListItemElementResult.IgnoreCaretPointSuggestion();
      RefPtr<Element> newListItemElement =
          unwrappedCreateNewListItemElementResult.UnwrapNewNode();
      MOZ_ASSERT(newListItemElement);
      // MOZ_KnownLive(rightListItemElement) because it's grabbed by
      // unwrappedSplitListItemElement.
      nsresult rv = mHTMLEditor.DeleteNodeWithTransaction(
          MOZ_KnownLive(rightListItemElement));
      if (NS_FAILED(rv)) {
        NS_WARNING("EditorBase::DeleteNodeWithTransaction() failed");
        return Err(rv);
      }
      EditorDOMPoint pointToPutCaret(newListItemElement, 0u);
      return InsertParagraphResult(std::move(newListItemElement),
                                   std::move(pointToPutCaret));
    }

    // If aListItemElement is a <li> and the right list item becomes empty or a
    // direct child of the editing host, copy all inline elements affecting to
    // the style at end of the left list item element to the right list item
    // element.
    // MOZ_KnownLive(leftListItemElement) and
    // MOZ_KnownLive(rightListItemElement) because they are grabbed by
    // unwrappedSplitListItemElement.
    Result<EditorDOMPoint, nsresult> pointToPutCaretOrError =
        mHTMLEditor.CopyLastEditableChildStylesWithTransaction(
            MOZ_KnownLive(leftListItemElement),
            MOZ_KnownLive(rightListItemElement), mEditingHost);
    if (MOZ_UNLIKELY(pointToPutCaretOrError.isErr())) {
      NS_WARNING(
          "HTMLEditor::CopyLastEditableChildStylesWithTransaction() failed");
      return pointToPutCaretOrError.propagateErr();
    }
    return InsertParagraphResult(&rightListItemElement,
                                 pointToPutCaretOrError.unwrap());
  }

  // If the right list item element is not empty, we need to consider where to
  // put caret in it. If it has non-container inline elements, <br> or <hr>, at
  // the element is proper position.
  const WSScanResult forwardScanFromStartOfListItemResult =
      WSRunScanner::ScanInclusiveNextVisibleNodeOrBlockBoundary(
          WSRunScanner::Scan::EditableNodes,
          EditorRawDOMPoint(&rightListItemElement, 0u),
          BlockInlineCheck::UseComputedDisplayStyle);
  if (MOZ_UNLIKELY(forwardScanFromStartOfListItemResult.Failed())) {
    NS_WARNING("WSRunScanner::ScanNextVisibleNodeOrBlockBoundary() failed");
    return Err(NS_ERROR_FAILURE);
  }
  if (forwardScanFromStartOfListItemResult.ReachedSpecialContent() ||
      forwardScanFromStartOfListItemResult.ReachedBRElement() ||
      forwardScanFromStartOfListItemResult.ReachedHRElement()) {
    auto atFoundElement = forwardScanFromStartOfListItemResult
                              .PointAtReachedContent<EditorDOMPoint>();
    if (NS_WARN_IF(!atFoundElement.IsSetAndValid())) {
      return Err(NS_ERROR_FAILURE);
    }
    return InsertParagraphResult(&rightListItemElement,
                                 std::move(atFoundElement));
  }

  // If we reached a block boundary (end of the list item or a child block),
  // let's put deepest start of the list item or the child block.
  if (forwardScanFromStartOfListItemResult.ReachedBlockBoundary() ||
      // FIXME: This is wrong considering because the inline editing host may
      // be surrounded by visible inline content.  However, WSRunScanner is
      // not aware of block boundary around it and stopping this change causes
      // starting to fail some WPT.  Therefore, we need to keep doing this for
      // now.
      forwardScanFromStartOfListItemResult.ReachedInlineEditingHostBoundary()) {
    return InsertParagraphResult(
        &rightListItemElement,
        HTMLEditUtils::GetDeepestEditableStartPointOf<EditorDOMPoint>(
            forwardScanFromStartOfListItemResult.GetContent()
                ? *forwardScanFromStartOfListItemResult.GetContent()
                : rightListItemElement));
  }

  // Otherwise, return the point at first visible thing.
  // XXX This may be not meaningful position if it reached block element
  //     in aListItemElement.
  return InsertParagraphResult(&rightListItemElement,
                               forwardScanFromStartOfListItemResult
                                   .PointAtReachedContent<EditorDOMPoint>());
}

}  // namespace mozilla
