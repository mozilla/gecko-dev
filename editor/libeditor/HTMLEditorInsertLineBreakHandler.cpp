/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EditorBase.h"
#include "HTMLEditor.h"
#include "HTMLEditorInlines.h"
#include "HTMLEditorNestedClasses.h"

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
#include "mozilla/AutoRestore.h"
#include "mozilla/ContentIterator.h"
#include "mozilla/EditorForwards.h"
#include "mozilla/Maybe.h"
#include "mozilla/PresShell.h"
#include "mozilla/TextComposition.h"
#include "mozilla/dom/RangeBinding.h"
#include "mozilla/dom/Selection.h"
#include "nsContentUtils.h"
#include "nsDebug.h"
#include "nsError.h"
#include "nsFrameSelection.h"
#include "nsGkAtoms.h"
#include "nsIContent.h"
#include "nsINode.h"
#include "nsRange.h"
#include "nsTArray.h"
#include "nsTextNode.h"

class nsISupports;

namespace mozilla {

using namespace dom;
using EmptyCheckOption = HTMLEditUtils::EmptyCheckOption;
using EmptyCheckOptions = HTMLEditUtils::EmptyCheckOptions;

nsresult HTMLEditor::InsertLineBreakAsSubAction() {
  MOZ_ASSERT(IsEditActionDataAvailable());
  MOZ_ASSERT(!IsSelectionRangeContainerNotContent());

  if (NS_WARN_IF(!mInitSucceeded)) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  {
    Result<EditActionResult, nsresult> result = CanHandleHTMLEditSubAction();
    if (MOZ_UNLIKELY(result.isErr())) {
      NS_WARNING("HTMLEditor::CanHandleHTMLEditSubAction() failed");
      return result.unwrapErr();
    }
    if (result.inspect().Canceled()) {
      return NS_OK;
    }
  }

  // XXX This may be called by execCommand() with "insertLineBreak".
  //     In such case, naming the transaction "TypingTxnName" is odd.
  AutoPlaceholderBatch treatAsOneTransaction(*this, *nsGkAtoms::TypingTxnName,
                                             ScrollSelectionIntoView::Yes,
                                             __FUNCTION__);

  // calling it text insertion to trigger moz br treatment by rules
  // XXX Why do we use EditSubAction::eInsertText here?  Looks like
  //     EditSubAction::eInsertLineBreak or EditSubAction::eInsertNode
  //     is better.
  IgnoredErrorResult ignoredError;
  AutoEditSubActionNotifier startToHandleEditSubAction(
      *this, EditSubAction::eInsertText, nsIEditor::eNext, ignoredError);
  if (NS_WARN_IF(ignoredError.ErrorCodeIs(NS_ERROR_EDITOR_DESTROYED))) {
    return ignoredError.StealNSResult();
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
      return rv;
    }
  }

  const RefPtr<Element> editingHost =
      ComputeEditingHost(LimitInBodyElement::No);
  if (NS_WARN_IF(!editingHost)) {
    return NS_ERROR_FAILURE;
  }

  AutoInsertLineBreakHandler handler(*this, *editingHost);
  nsresult rv = handler.Run();
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "AutoInsertLineBreakHandler::Run() failed");
  return rv;
}

nsresult HTMLEditor::AutoInsertLineBreakHandler::Run() {
  MOZ_ASSERT(mHTMLEditor.IsEditActionDataAvailable());

  const auto atStartOfSelection =
      mHTMLEditor.GetFirstSelectionStartPoint<EditorDOMPoint>();
  if (NS_WARN_IF(!atStartOfSelection.IsInContentNode())) {
    return NS_ERROR_FAILURE;
  }
  MOZ_ASSERT(atStartOfSelection.IsSetAndValidInComposedDoc());

  const Maybe<LineBreakType> lineBreakType =
      mHTMLEditor.GetPreferredLineBreakType(
          *atStartOfSelection.ContainerAs<nsIContent>(), mEditingHost);
  if (MOZ_UNLIKELY(!lineBreakType)) {
    return NS_SUCCESS_DOM_NO_OPERATION;  // Cannot insert a line break there.
  }
  if (lineBreakType.value() == LineBreakType::BRElement) {
    nsresult rv = HandleInsertBRElement();
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "AutoInsertLineBreakHandler::HandleInsertBRElement()");
    return rv;
  }

  nsresult rv = HandleInsertLinefeed();
  NS_WARNING_ASSERTION(
      NS_SUCCEEDED(rv),
      "AutoInsertLineBreakHandler::HandleInsertLinefeed() failed");
  return rv;
}

nsresult HTMLEditor::AutoInsertLineBreakHandler::HandleInsertBRElement() {
  const auto atStartOfSelection =
      mHTMLEditor.GetFirstSelectionStartPoint<EditorDOMPoint>();
  MOZ_ASSERT(atStartOfSelection.IsInContentNode());
  Result<CreateLineBreakResult, nsresult> insertLineBreakResultOrError =
      mHTMLEditor.InsertLineBreak(WithTransaction::Yes,
                                  LineBreakType::BRElement, atStartOfSelection,
                                  nsIEditor::eNext);
  if (MOZ_UNLIKELY(insertLineBreakResultOrError.isErr())) {
    NS_WARNING(
        "HTMLEditor::InsertLineBreak(WithTransaction::Yes, "
        "LineBreakType::BRElement, eNext) failed");
    return insertLineBreakResultOrError.unwrapErr();
  }
  CreateLineBreakResult insertLineBreakResult =
      insertLineBreakResultOrError.unwrap();
  MOZ_ASSERT(insertLineBreakResult.Handled());
  insertLineBreakResult.IgnoreCaretPointSuggestion();

  auto pointToPutCaret = insertLineBreakResult.UnwrapCaretPoint();
  if (MOZ_UNLIKELY(!pointToPutCaret.IsSet())) {
    NS_WARNING("Inserted <br> was unexpectedly removed");
    return Err(NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE);
  }
  const WSScanResult backwardScanFromBeforeBRElementResult =
      WSRunScanner::ScanPreviousVisibleNodeOrBlockBoundary(
          WSRunScanner::Scan::EditableNodes,
          insertLineBreakResult.AtLineBreak<EditorDOMPoint>(),
          BlockInlineCheck::UseComputedDisplayStyle);
  if (MOZ_UNLIKELY(backwardScanFromBeforeBRElementResult.Failed())) {
    NS_WARNING("WSRunScanner::ScanPreviousVisibleNodeOrBlockBoundary() failed");
    return Err(NS_ERROR_FAILURE);
  }

  const WSScanResult forwardScanFromAfterBRElementResult =
      WSRunScanner::ScanInclusiveNextVisibleNodeOrBlockBoundary(
          WSRunScanner::Scan::EditableNodes, pointToPutCaret,
          BlockInlineCheck::UseComputedDisplayStyle);
  if (MOZ_UNLIKELY(forwardScanFromAfterBRElementResult.Failed())) {
    NS_WARNING("WSRunScanner::ScanNextVisibleNodeOrBlockBoundary() failed");
    return Err(NS_ERROR_FAILURE);
  }
  const bool brElementIsAfterBlock =
      backwardScanFromBeforeBRElementResult.ReachedBlockBoundary() ||
      // FIXME: This is wrong considering because the inline editing host may
      // be surrounded by visible inline content.  However, WSRunScanner is
      // not aware of block boundary around it and stopping this change causes
      // starting to fail some WPT.  Therefore, we need to keep doing this for
      // now.
      backwardScanFromBeforeBRElementResult.ReachedInlineEditingHostBoundary();
  const bool brElementIsBeforeBlock =
      forwardScanFromAfterBRElementResult.ReachedBlockBoundary() ||
      // FIXME: See above comment.
      forwardScanFromAfterBRElementResult.ReachedInlineEditingHostBoundary();
  const bool isEmptyEditingHost = HTMLEditUtils::IsEmptyNode(
      mEditingHost, {EmptyCheckOption::TreatNonEditableContentAsInvisible});
  if (brElementIsBeforeBlock &&
      (isEmptyEditingHost || !brElementIsAfterBlock)) {
    // Empty last line is invisible if it's immediately before either parent
    // or another block's boundary so that we need to put invisible <br>
    // element here for making it visible.
    Result<CreateLineBreakResult, nsresult>
        insertPaddingBRElementResultOrError =
            WhiteSpaceVisibilityKeeper::InsertLineBreak(
                LineBreakType::BRElement, mHTMLEditor, pointToPutCaret);
    if (MOZ_UNLIKELY(insertPaddingBRElementResultOrError.isErr())) {
      NS_WARNING(
          "WhiteSpaceVisibilityKeeper::InsertLineBreak(LineBreakType::"
          "BRElement) failed");
      return insertPaddingBRElementResultOrError.unwrapErr();
    }
    CreateLineBreakResult insertPaddingBRElementResult =
        insertPaddingBRElementResultOrError.unwrap();
    pointToPutCaret =
        insertPaddingBRElementResult.AtLineBreak<EditorDOMPoint>();
    insertPaddingBRElementResult.IgnoreCaretPointSuggestion();
  } else if (forwardScanFromAfterBRElementResult
                 .InVisibleOrCollapsibleCharacters()) {
    pointToPutCaret = forwardScanFromAfterBRElementResult
                          .PointAtReachedContent<EditorDOMPoint>();
  } else if (forwardScanFromAfterBRElementResult.ReachedSpecialContent()) {
    // Next inserting text should be inserted into styled inline elements if
    // they have first visible thing in the new line.
    pointToPutCaret = forwardScanFromAfterBRElementResult
                          .PointAtReachedContent<EditorDOMPoint>();
  }

  nsresult rv = mHTMLEditor.CollapseSelectionTo(pointToPutCaret);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "EditorBase::CollapseSelectionTo() failed");
  return rv;
}

nsresult HTMLEditor::AutoInsertLineBreakHandler::HandleInsertLinefeed() {
  nsresult rv = mHTMLEditor.EnsureNoPaddingBRElementForEmptyEditor();
  if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "EditorBase::EnsureNoPaddingBRElementForEmptyEditor() "
                       "failed, but ignored");

  if (NS_SUCCEEDED(rv) && mHTMLEditor.SelectionRef().IsCollapsed()) {
    nsresult rv =
        mHTMLEditor.EnsureCaretNotAfterInvisibleBRElement(mEditingHost);
    if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "HTMLEditor::EnsureCaretNotAfterInvisibleBRElement() "
                         "failed, but ignored");
    if (NS_SUCCEEDED(rv)) {
      nsresult rv = mHTMLEditor.PrepareInlineStylesForCaret();
      if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
        return NS_ERROR_EDITOR_DESTROYED;
      }
      NS_WARNING_ASSERTION(
          NS_SUCCEEDED(rv),
          "HTMLEditor::PrepareInlineStylesForCaret() failed, but ignored");
    }
  }

  const EditorDOMPoint atStartOfSelection =
      mHTMLEditor.GetFirstSelectionStartPoint<EditorDOMPoint>();
  if (NS_WARN_IF(!atStartOfSelection.IsInContentNode())) {
    return NS_ERROR_FAILURE;
  }
  MOZ_ASSERT(atStartOfSelection.IsSetAndValidInComposedDoc());

  // Do nothing if the node is read-only
  if (!HTMLEditUtils::IsSimplyEditableNode(
          *atStartOfSelection.GetContainer())) {
    return NS_SUCCESS_DOM_NO_OPERATION;
  }

  Result<EditorDOMPoint, nsresult> insertLineFeedResult =
      AutoInsertLineBreakHandler::InsertLinefeed(
          mHTMLEditor, atStartOfSelection, mEditingHost);
  if (MOZ_UNLIKELY(insertLineFeedResult.isErr())) {
    NS_WARNING("AutoInsertLineBreakHandler::InsertLinefeed() failed");
    return insertLineFeedResult.unwrapErr();
  }
  rv = mHTMLEditor.CollapseSelectionTo(insertLineFeedResult.inspect());
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "EditorBase::CollapseSelectionTo() failed");
  return rv;
}

// static
Result<EditorDOMPoint, nsresult>
HTMLEditor::AutoInsertLineBreakHandler::InsertLinefeed(
    HTMLEditor& aHTMLEditor, const EditorDOMPoint& aPointToBreak,
    const Element& aEditingHost) {
  if (NS_WARN_IF(!aPointToBreak.IsSet())) {
    return Err(NS_ERROR_INVALID_ARG);
  }

  const RefPtr<Document> document = aHTMLEditor.GetDocument();
  MOZ_DIAGNOSTIC_ASSERT(document);
  if (NS_WARN_IF(!document)) {
    return Err(NS_ERROR_FAILURE);
  }

  // TODO: The following code is duplicated from `HandleInsertText`.  They
  //       should be merged when we fix bug 92921.

  Result<EditorDOMPoint, nsresult> setStyleResult =
      aHTMLEditor.CreateStyleForInsertText(aPointToBreak, aEditingHost);
  if (MOZ_UNLIKELY(setStyleResult.isErr())) {
    NS_WARNING("HTMLEditor::CreateStyleForInsertText() failed");
    return setStyleResult.propagateErr();
  }

  EditorDOMPoint pointToInsert = setStyleResult.inspect().IsSet()
                                     ? setStyleResult.inspect()
                                     : aPointToBreak;
  if (NS_WARN_IF(!pointToInsert.IsSetAndValid()) ||
      NS_WARN_IF(!pointToInsert.IsInContentNode())) {
    return Err(NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE);
  }
  MOZ_ASSERT(pointToInsert.IsSetAndValid());

  // The node may not be able to have a text node so that we need to check it
  // here.
  if (!pointToInsert.IsInTextNode() &&
      !HTMLEditUtils::CanNodeContain(*pointToInsert.ContainerAs<nsIContent>(),
                                     *nsGkAtoms::textTagName)) {
    NS_WARNING(
        "AutoInsertLineBreakHandler::InsertLinefeed() couldn't insert a "
        "linefeed because the insertion position couldn't have text nodes");
    return Err(NS_ERROR_EDITOR_NO_EDITABLE_RANGE);
  }

  AutoRestore<bool> disableListener(
      aHTMLEditor.EditSubActionDataRef().mAdjustChangedRangeFromListener);
  aHTMLEditor.EditSubActionDataRef().mAdjustChangedRangeFromListener = false;

  // TODO: We don't need AutoTransactionsConserveSelection here in the normal
  //       cases, but removing this may cause the behavior with the legacy
  //       mutation event listeners.  We should try to delete this in a bug.
  AutoTransactionsConserveSelection dontChangeMySelection(aHTMLEditor);

  EditorDOMPoint pointToPutCaret;
  {
    AutoTrackDOMPoint trackingInsertingPosition(aHTMLEditor.RangeUpdaterRef(),
                                                &pointToInsert);
    Result<CreateLineBreakResult, nsresult> insertLinefeedResultOrError =
        aHTMLEditor.InsertLineBreak(WithTransaction::Yes,
                                    LineBreakType::Linefeed, pointToInsert,
                                    eNext);
    if (MOZ_UNLIKELY(insertLinefeedResultOrError.isErr())) {
      NS_WARNING(
          "HTMLEditor::InsertLineBreak(WithTransaction::Yes, "
          "LineBreakType::Linefeed, eNext) failed");
      return insertLinefeedResultOrError.propagateErr();
    }
    pointToPutCaret = insertLinefeedResultOrError.unwrap().UnwrapCaretPoint();
  }

  // Insert a padding <br> if the inserted linefeed is followed by a block
  // boundary.  Note that it should always be <br> for avoiding padding line
  // breaks appear in `.textContent` value.
  if (pointToPutCaret.IsInContentNode() && pointToPutCaret.IsEndOfContainer()) {
    const WSRunScanner wsScannerAtCaret(
        WSRunScanner::Scan::EditableNodes, pointToPutCaret,
        BlockInlineCheck::UseComputedDisplayStyle);
    if (wsScannerAtCaret.StartsFromPreformattedLineBreak() &&
        (wsScannerAtCaret.EndsByBlockBoundary() ||
         wsScannerAtCaret.EndsByInlineEditingHostBoundary()) &&
        HTMLEditUtils::CanNodeContain(*wsScannerAtCaret.GetEndReasonContent(),
                                      *nsGkAtoms::br)) {
      AutoTrackDOMPoint trackingInsertedPosition(aHTMLEditor.RangeUpdaterRef(),
                                                 &pointToInsert);
      AutoTrackDOMPoint trackingNewCaretPosition(aHTMLEditor.RangeUpdaterRef(),
                                                 &pointToPutCaret);
      Result<CreateLineBreakResult, nsresult> insertBRElementResultOrError =
          aHTMLEditor.InsertLineBreak(
              WithTransaction::Yes, LineBreakType::BRElement, pointToPutCaret);
      if (MOZ_UNLIKELY(insertBRElementResultOrError.isErr())) {
        NS_WARNING(
            "HTMLEditor::InsertLineBreak(WithTransaction::Yes, "
            "LineBreakType::BRElement) failed");
        return insertBRElementResultOrError.propagateErr();
      }
      CreateLineBreakResult insertBRElementResult =
          insertBRElementResultOrError.unwrap();
      MOZ_ASSERT(insertBRElementResult.Handled());
      insertBRElementResult.IgnoreCaretPointSuggestion();
    }
  }

  // manually update the doc changed range so that
  // OnEndHandlingTopLevelEditSubActionInternal will clean up the correct
  // portion of the document.
  MOZ_ASSERT(pointToPutCaret.IsSet());
  if (NS_WARN_IF(!pointToPutCaret.IsSet())) {
    // XXX Here is odd.  We did mChangedRange->SetStartAndEnd(pointToInsert,
    //     pointToPutCaret), but it always fails because of the latter is unset.
    //     Therefore, always returning NS_ERROR_FAILURE from here is the
    //     traditional behavior...
    // TODO: Stop updating the interline position of Selection with fixing here
    //       and returning expected point.
    DebugOnly<nsresult> rvIgnored =
        aHTMLEditor.SelectionRef().SetInterlinePosition(
            InterlinePosition::EndOfLine);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                         "Selection::SetInterlinePosition(InterlinePosition::"
                         "EndOfLine) failed, but ignored");
    if (NS_FAILED(aHTMLEditor.TopLevelEditSubActionDataRef()
                      .mChangedRange->CollapseTo(pointToInsert))) {
      NS_WARNING("nsRange::CollapseTo() failed");
      return Err(NS_ERROR_FAILURE);
    }
    NS_WARNING(
        "We always return NS_ERROR_FAILURE here because of a failure of "
        "updating mChangedRange");
    return Err(NS_ERROR_FAILURE);
  }

  if (NS_FAILED(aHTMLEditor.TopLevelEditSubActionDataRef()
                    .mChangedRange->SetStartAndEnd(
                        pointToInsert.ToRawRangeBoundary(),
                        pointToPutCaret.ToRawRangeBoundary()))) {
    NS_WARNING("nsRange::SetStartAndEnd() failed");
    return Err(NS_ERROR_FAILURE);
  }

  pointToPutCaret.SetInterlinePosition(InterlinePosition::EndOfLine);
  return pointToPutCaret;
}

}  // namespace mozilla
