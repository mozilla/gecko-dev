/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/TextEditor.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/SelectionState.h"
#include "mozilla/dom/DataTransfer.h"
#include "mozilla/dom/DragEvent.h"
#include "mozilla/dom/Selection.h"
#include "nsAString.h"
#include "nsCOMPtr.h"
#include "nsComponentManagerUtils.h"
#include "nsContentUtils.h"
#include "nsDebug.h"
#include "nsError.h"
#include "nsIClipboard.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIDragService.h"
#include "nsIDragSession.h"
#include "nsIEditor.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeItem.h"
#include "nsIPrincipal.h"
#include "nsIFormControl.h"
#include "nsIPlaintextEditor.h"
#include "nsISupportsPrimitives.h"
#include "nsITransferable.h"
#include "nsIVariant.h"
#include "nsLiteralString.h"
#include "nsRange.h"
#include "nsServiceManagerUtils.h"
#include "nsString.h"
#include "nsXPCOM.h"
#include "nscore.h"

class nsILoadContext;
class nsISupports;

namespace mozilla {

using namespace dom;

nsresult TextEditor::PrepareTransferable(nsITransferable** transferable) {
  // Create generic Transferable for getting the data
  nsresult rv =
      CallCreateInstance("@mozilla.org/widget/transferable;1", transferable);
  NS_ENSURE_SUCCESS(rv, rv);

  // Get the nsITransferable interface for getting the data from the clipboard
  if (transferable) {
    nsCOMPtr<nsIDocument> destdoc = GetDocument();
    nsILoadContext* loadContext = destdoc ? destdoc->GetLoadContext() : nullptr;
    (*transferable)->Init(loadContext);

    (*transferable)->AddDataFlavor(kUnicodeMime);
    (*transferable)->AddDataFlavor(kMozTextInternal);
  };
  return NS_OK;
}

nsresult TextEditor::PrepareToInsertContent(
    const EditorDOMPoint& aPointToInsert, bool aDoDeleteSelection) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  MOZ_ASSERT(aPointToInsert.IsSet());

  EditorDOMPoint pointToInsert(aPointToInsert);
  if (aDoDeleteSelection) {
    AutoTrackDOMPoint tracker(RangeUpdaterRef(), &pointToInsert);
    nsresult rv = DeleteSelectionAsSubAction(eNone, eStrip);
    if (NS_WARN_IF(Destroyed())) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  ErrorResult error;
  SelectionRefPtr()->Collapse(pointToInsert, error);
  if (NS_WARN_IF(Destroyed())) {
    return NS_ERROR_EDITOR_DESTROYED;
  }
  if (NS_WARN_IF(error.Failed())) {
    return error.StealNSResult();
  }

  return NS_OK;
}

nsresult TextEditor::InsertTextAt(const nsAString& aStringToInsert,
                                  const EditorDOMPoint& aPointToInsert,
                                  bool aDoDeleteSelection) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  MOZ_ASSERT(aPointToInsert.IsSet());

  nsresult rv = PrepareToInsertContent(aPointToInsert, aDoDeleteSelection);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = InsertTextAsSubAction(aStringToInsert);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return NS_OK;
}

nsresult TextEditor::InsertTextFromTransferable(
    nsITransferable* aTransferable) {
  nsAutoCString bestFlavor;
  nsCOMPtr<nsISupports> genericDataObj;
  if (NS_SUCCEEDED(aTransferable->GetAnyTransferData(
          bestFlavor, getter_AddRefs(genericDataObj))) &&
      (bestFlavor.EqualsLiteral(kUnicodeMime) ||
       bestFlavor.EqualsLiteral(kMozTextInternal))) {
    AutoTransactionsConserveSelection dontChangeMySelection(*this);

    nsAutoString stuffToPaste;
    if (nsCOMPtr<nsISupportsString> text = do_QueryInterface(genericDataObj)) {
      text->GetData(stuffToPaste);
    }

    if (!stuffToPaste.IsEmpty()) {
      // Sanitize possible carriage returns in the string to be inserted
      nsContentUtils::PlatformToDOMLineBreaks(stuffToPaste);

      AutoPlaceholderBatch treatAsOneTransaction(*this);
      nsresult rv = InsertTextAsSubAction(stuffToPaste);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }
  }

  // Try to scroll the selection into view if the paste/drop succeeded
  ScrollSelectionIntoView(false);

  return NS_OK;
}

nsresult TextEditor::InsertFromDataTransfer(DataTransfer* aDataTransfer,
                                            int32_t aIndex,
                                            nsIDocument* aSourceDoc,
                                            const EditorDOMPoint& aDroppedAt,
                                            bool aDoDeleteSelection) {
  MOZ_ASSERT(GetEditAction() == EditAction::eDrop);
  MOZ_ASSERT(
      mPlaceholderBatch,
      "TextEditor::InsertFromDataTransfer() should be called only by OnDrop() "
      "and there should've already been placeholder transaction");
  MOZ_ASSERT(aDroppedAt.IsSet());

  nsCOMPtr<nsIVariant> data;
  aDataTransfer->GetDataAtNoSecurityCheck(NS_LITERAL_STRING("text/plain"),
                                          aIndex, getter_AddRefs(data));
  if (!data) {
    return NS_OK;
  }

  nsAutoString insertText;
  data->GetAsAString(insertText);
  nsContentUtils::PlatformToDOMLineBreaks(insertText);

  return InsertTextAt(insertText, aDroppedAt, aDoDeleteSelection);
}

nsresult TextEditor::OnDrop(DragEvent* aDropEvent) {
  if (NS_WARN_IF(!aDropEvent)) {
    return NS_ERROR_INVALID_ARG;
  }

  CommitComposition();

  AutoEditActionDataSetter editActionData(*this, EditAction::eDrop);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  RefPtr<DataTransfer> dataTransfer = aDropEvent->GetDataTransfer();
  if (NS_WARN_IF(!dataTransfer)) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIDragSession> dragSession = nsContentUtils::GetDragSession();
  if (NS_WARN_IF(!dragSession)) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsINode> sourceNode = dataTransfer->GetMozSourceNode();

  nsCOMPtr<nsIDocument> srcdoc;
  if (sourceNode) {
    srcdoc = sourceNode->OwnerDoc();
  }

  if (nsContentUtils::CheckForSubFrameDrop(
          dragSession, aDropEvent->WidgetEventPtr()->AsDragEvent())) {
    // Don't allow drags from subframe documents with different origins than
    // the drop destination.
    if (srcdoc && !IsSafeToInsertData(srcdoc)) {
      return NS_OK;
    }
  }

  // Current doc is destination
  nsIDocument* destdoc = GetDocument();
  if (NS_WARN_IF(!destdoc)) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  uint32_t numItems = dataTransfer->MozItemCount();
  if (NS_WARN_IF(!numItems)) {
    return NS_ERROR_FAILURE;  // Nothing to drop?
  }

  // We have to figure out whether to delete and relocate caret only once
  // Parent and offset are under the mouse cursor.
  EditorDOMPoint droppedAt(aDropEvent->GetRangeParent(),
                           aDropEvent->RangeOffset());
  if (NS_WARN_IF(!droppedAt.IsSet())) {
    return NS_ERROR_FAILURE;
  }

  // Check if dropping into a selected range.  If so and the source comes from
  // same document, jump through some hoops to determine if mouse is over
  // selection (bail) and whether user wants to copy selection or delete it.
  bool deleteSelection = false;
  if (!SelectionRefPtr()->IsCollapsed() && srcdoc == destdoc) {
    uint32_t rangeCount = SelectionRefPtr()->RangeCount();
    for (uint32_t j = 0; j < rangeCount; j++) {
      nsRange* range = SelectionRefPtr()->GetRangeAt(j);
      if (NS_WARN_IF(!range)) {
        // don't bail yet, iterate through them all
        continue;
      }
      IgnoredErrorResult errorIgnored;
      if (range->IsPointInRange(*droppedAt.GetContainer(), droppedAt.Offset(),
                                errorIgnored) &&
          !errorIgnored.Failed()) {
        // If source document and destination document is same and dropping
        // into one of selected ranges, we don't need to do nothing.
        // XXX If the source comes from outside of this editor, this check
        //     means that we don't allow to drop the item in the selected
        //     range.  However, the selection is hidden until the <input> or
        //     <textarea> gets focus, therefore, this looks odd.
        return NS_OK;
      }
    }

    // Delete if user doesn't want to copy when user moves selected content
    // to different place in same editor.
    // XXX This is odd when the source comes from outside of this editor since
    //     the selection is hidden until this gets focus and drag events set
    //     caret at the nearest insertion point under the cursor.  Therefore,
    //     once user drops the item, the item inserted at caret position *and*
    //     selected content is also removed.
    uint32_t dropEffect = dataTransfer->DropEffectInt();
    deleteSelection = !(dropEffect & nsIDragService::DRAGDROP_ACTION_COPY);
  }

  if (IsPlaintextEditor()) {
    for (nsIContent* content = droppedAt.GetContainerAsContent(); content;
         content = content->GetParent()) {
      nsCOMPtr<nsIFormControl> formControl(do_QueryInterface(content));
      if (formControl && !formControl->AllowDrop()) {
        // Don't allow dropping into a form control that doesn't allow being
        // dropped into.
        return NS_OK;
      }
    }
  }

  // Combine any deletion and drop insertion into one transaction.
  AutoPlaceholderBatch treatAsOneTransaction(*this);

  // Don't dispatch "selectionchange" event until inserting all contents.
  SelectionBatcher selectionBatcher(SelectionRefPtr());

  // Remove selected contents first here because we need to fire a pair of
  // "beforeinput" and "input" for deletion and web apps can cancel only
  // this deletion.  Note that callee may handle insertion asynchronously.
  // Therefore, it is the best to remove selected content here.
  if (deleteSelection && !SelectionRefPtr()->IsCollapsed()) {
    nsresult rv = PrepareToInsertContent(droppedAt, true);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    // Now, Selection should be collapsed at dropped point.  If somebody
    // changed Selection, we should think what should do it in such case
    // later.
    if (NS_WARN_IF(!SelectionRefPtr()->IsCollapsed()) ||
        NS_WARN_IF(!SelectionRefPtr()->RangeCount())) {
      return NS_ERROR_FAILURE;
    }
    droppedAt = SelectionRefPtr()->FocusRef();
    if (NS_WARN_IF(!droppedAt.IsSet())) {
      return NS_ERROR_FAILURE;
    }

    // Let's fire "input" event for the deletion now.
    if (mDispatchInputEvent) {
      FireInputEvent();
      if (NS_WARN_IF(Destroyed())) {
        return NS_ERROR_EDITOR_DESTROYED;
      }
    }

    // XXX Now, Selection may be changed by input event listeners.  If so,
    //     should we update |droppedAt|?
  }

  for (uint32_t i = 0; i < numItems; ++i) {
    InsertFromDataTransfer(dataTransfer, i, srcdoc, droppedAt, false);
    if (NS_WARN_IF(Destroyed())) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
  }

  ScrollSelectionIntoView(false);

  return NS_OK;
}

nsresult TextEditor::PasteAsAction(int32_t aClipboardType,
                                   bool aDispatchPasteEvent) {
  AutoEditActionDataSetter editActionData(*this, EditAction::ePaste);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  if (AsHTMLEditor()) {
    nsresult rv =
        AsHTMLEditor()->PasteInternal(aClipboardType, aDispatchPasteEvent);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    return NS_OK;
  }

  if (aDispatchPasteEvent && !FireClipboardEvent(ePaste, aClipboardType)) {
    return NS_OK;
  }

  // Get Clipboard Service
  nsresult rv;
  nsCOMPtr<nsIClipboard> clipboard =
      do_GetService("@mozilla.org/widget/clipboard;1", &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Get the nsITransferable interface for getting the data from the clipboard
  nsCOMPtr<nsITransferable> transferable;
  rv = PrepareTransferable(getter_AddRefs(transferable));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  if (NS_WARN_IF(!transferable)) {
    return NS_OK;  // XXX Why?
  }
  // Get the Data from the clipboard.
  rv = clipboard->GetData(transferable, aClipboardType);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return NS_OK;  // XXX Why?
  }
  // XXX Why don't we check this first?
  if (!IsModifiable()) {
    return NS_OK;
  }
  rv = InsertTextFromTransferable(transferable);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return NS_OK;
}

NS_IMETHODIMP
TextEditor::PasteTransferable(nsITransferable* aTransferable) {
  AutoEditActionDataSetter editActionData(*this, EditAction::ePaste);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  // Use an invalid value for the clipboard type as data comes from
  // aTransferable and we don't currently implement a way to put that in the
  // data transfer yet.
  if (!FireClipboardEvent(ePaste, -1)) {
    return NS_OK;
  }

  if (!IsModifiable()) {
    return NS_OK;
  }

  return InsertTextFromTransferable(aTransferable);
}

NS_IMETHODIMP
TextEditor::CanPaste(int32_t aSelectionType, bool* aCanPaste) {
  NS_ENSURE_ARG_POINTER(aCanPaste);
  *aCanPaste = false;

  // Always enable the paste command when inside of a HTML or XHTML document.
  nsCOMPtr<nsIDocument> doc = GetDocument();
  if (doc && doc->IsHTMLOrXHTML()) {
    *aCanPaste = true;
    return NS_OK;
  }

  // can't paste if readonly
  if (!IsModifiable()) {
    return NS_OK;
  }

  nsresult rv;
  nsCOMPtr<nsIClipboard> clipboard(
      do_GetService("@mozilla.org/widget/clipboard;1", &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  // the flavors that we can deal with
  const char* textEditorFlavors[] = {kUnicodeMime};

  bool haveFlavors;
  rv = clipboard->HasDataMatchingFlavors(textEditorFlavors,
                                         ArrayLength(textEditorFlavors),
                                         aSelectionType, &haveFlavors);
  NS_ENSURE_SUCCESS(rv, rv);

  *aCanPaste = haveFlavors;
  return NS_OK;
}

bool TextEditor::CanPasteTransferable(nsITransferable* aTransferable) {
  // can't paste if readonly
  if (!IsModifiable()) {
    return false;
  }

  // If |aTransferable| is null, assume that a paste will succeed.
  if (!aTransferable) {
    return true;
  }

  nsCOMPtr<nsISupports> data;
  nsresult rv =
      aTransferable->GetTransferData(kUnicodeMime, getter_AddRefs(data));
  if (NS_SUCCEEDED(rv) && data) {
    return true;
  }

  return false;
}

bool TextEditor::IsSafeToInsertData(nsIDocument* aSourceDoc) {
  // Try to determine whether we should use a sanitizing fragment sink
  bool isSafe = false;

  nsCOMPtr<nsIDocument> destdoc = GetDocument();
  NS_ASSERTION(destdoc, "Where is our destination doc?");
  nsCOMPtr<nsIDocShellTreeItem> dsti = destdoc->GetDocShell();
  nsCOMPtr<nsIDocShellTreeItem> root;
  if (dsti) {
    dsti->GetRootTreeItem(getter_AddRefs(root));
  }
  nsCOMPtr<nsIDocShell> docShell = do_QueryInterface(root);

  isSafe = docShell && docShell->GetAppType() == nsIDocShell::APP_TYPE_EDITOR;

  if (!isSafe && aSourceDoc) {
    nsIPrincipal* srcPrincipal = aSourceDoc->NodePrincipal();
    nsIPrincipal* destPrincipal = destdoc->NodePrincipal();
    NS_ASSERTION(srcPrincipal && destPrincipal,
                 "How come we don't have a principal?");
    srcPrincipal->Subsumes(destPrincipal, &isSafe);
  }

  return isSafe;
}

}  // namespace mozilla
