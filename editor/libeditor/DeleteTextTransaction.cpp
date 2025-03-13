/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DeleteTextTransaction.h"

#include "EditorBase.h"
#include "EditorDOMPoint.h"
#include "HTMLEditUtils.h"
#include "SelectionState.h"

#include "mozilla/Assertions.h"
#include "mozilla/TextEditor.h"
#include "mozilla/dom/Selection.h"

#include "nsDebug.h"
#include "nsError.h"
#include "nsISupportsImpl.h"
#include "nsAString.h"

namespace mozilla {

using namespace dom;

// static
already_AddRefed<DeleteTextTransaction> DeleteTextTransaction::MaybeCreate(
    EditorBase& aEditorBase, Text& aTextNode, uint32_t aOffset,
    uint32_t aLengthToDelete) {
  RefPtr<DeleteTextTransaction> transaction =
      aEditorBase.IsTextEditor()
          ? new DeleteTextTransaction(aEditorBase, aTextNode, aOffset,
                                      aLengthToDelete)
          : new DeleteTextFromTextNodeTransaction(aEditorBase, aTextNode,
                                                  aOffset, aLengthToDelete);
  return transaction.forget();
}

// static
already_AddRefed<DeleteTextTransaction>
DeleteTextTransaction::MaybeCreateForPreviousCharacter(EditorBase& aEditorBase,
                                                       Text& aTextNode,
                                                       uint32_t aOffset) {
  if (NS_WARN_IF(!aOffset)) {
    return nullptr;
  }

  nsAutoString data;
  aTextNode.GetData(data);
  if (NS_WARN_IF(data.IsEmpty())) {
    return nullptr;
  }

  uint32_t length = 1;
  uint32_t offset = aOffset - 1;
  if (offset && NS_IS_SURROGATE_PAIR(data[offset - 1], data[offset])) {
    ++length;
    --offset;
  }
  return DeleteTextTransaction::MaybeCreate(aEditorBase, aTextNode, offset,
                                            length);
}

// static
already_AddRefed<DeleteTextTransaction>
DeleteTextTransaction::MaybeCreateForNextCharacter(EditorBase& aEditorBase,
                                                   Text& aTextNode,
                                                   uint32_t aOffset) {
  nsAutoString data;
  aTextNode.GetData(data);
  if (NS_WARN_IF(aOffset >= data.Length()) || NS_WARN_IF(data.IsEmpty())) {
    return nullptr;
  }

  uint32_t length = 1;
  if (aOffset + 1 < data.Length() &&
      NS_IS_SURROGATE_PAIR(data[aOffset], data[aOffset + 1])) {
    ++length;
  }
  return DeleteTextTransaction::MaybeCreate(aEditorBase, aTextNode, aOffset,
                                            length);
}

DeleteTextTransaction::DeleteTextTransaction(EditorBase& aEditorBase,
                                             Text& aTextNode, uint32_t aOffset,
                                             uint32_t aLengthToDelete)
    : DeleteContentTransactionBase(aEditorBase),
      mOffset(aOffset),
      mLengthToDelete(aLengthToDelete) {
  MOZ_ASSERT(aTextNode.TextDataLength() >= aOffset + aLengthToDelete);
}

std::ostream& operator<<(std::ostream& aStream,
                         const DeleteTextTransaction& aTransaction) {
  const auto* transactionForHTMLEditor =
      aTransaction.GetAsDeleteTextFromTextNodeTransaction();
  if (transactionForHTMLEditor) {
    return aStream << *transactionForHTMLEditor;
  }
  aStream << "{ mOffset=" << aTransaction.mOffset
          << ", mLengthToDelete=" << aTransaction.mLengthToDelete
          << ", mDeletedText=\""
          << NS_ConvertUTF16toUTF8(aTransaction.mDeletedText).get() << "\""
          << ", mEditorBase=" << aTransaction.mEditorBase.get() << " }";
  return aStream;
}

NS_IMPL_CYCLE_COLLECTION_INHERITED(DeleteTextTransaction,
                                   DeleteContentTransactionBase)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(DeleteTextTransaction)
NS_INTERFACE_MAP_END_INHERITING(DeleteContentTransactionBase)

Text* DeleteTextTransaction::GetTextNode() const {
  if (MOZ_UNLIKELY(!mEditorBase)) {
    return nullptr;
  }
  if (TextEditor* const textEditor = mEditorBase->GetAsTextEditor()) {
    return textEditor->GetTextNode();
  }
  MOZ_ASSERT(GetAsDeleteTextFromTextNodeTransaction());
  return GetAsDeleteTextFromTextNodeTransaction()->mTextNode;
}

NS_IMETHODIMP DeleteTextTransaction::DoTransaction() {
  MOZ_LOG(GetLogModule(), LogLevel::Info,
          ("%p DeleteTextTransaction::%s this=%s", this, __FUNCTION__,
           ToString(*this).c_str()));

  if (NS_WARN_IF(!mEditorBase)) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  const RefPtr<Text> textNode = GetTextNode();
  if (NS_WARN_IF(!textNode) ||
      (mEditorBase->IsHTMLEditor() &&
       NS_WARN_IF(!HTMLEditUtils::IsSimplyEditableNode(*textNode)))) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // Get the text that we're about to delete
  IgnoredErrorResult error;
  textNode->SubstringData(mOffset, mLengthToDelete, mDeletedText, error);
  if (MOZ_UNLIKELY(error.Failed())) {
    NS_WARNING("Text::SubstringData() failed");
    return error.StealNSResult();
  }

  const OwningNonNull<EditorBase> editorBase = *mEditorBase;
  editorBase->DoDeleteText(*textNode, mOffset, mLengthToDelete, error);
  if (MOZ_UNLIKELY(error.Failed())) {
    NS_WARNING("EditorBase::DoDeleteText() failed");
    return error.StealNSResult();
  }

  editorBase->RangeUpdaterRef().SelAdjDeleteText(*textNode, mOffset,
                                                 mLengthToDelete);
  return NS_OK;
}

EditorDOMPoint DeleteTextTransaction::SuggestPointToPutCaret() const {
  if (NS_WARN_IF(!mEditorBase)) {
    return EditorDOMPoint();
  }
  Text* const textNode = GetTextNode();
  if (NS_WARN_IF(!textNode) ||
      (mEditorBase->IsHTMLEditor() &&
       NS_WARN_IF(!HTMLEditUtils::IsSimplyEditableNode(*textNode)))) {
    return EditorDOMPoint();
  }
  if (NS_WARN_IF(textNode->TextDataLength() < mOffset)) {
    return EditorDOMPoint();
  }
  EditorDOMPoint candidatePoint(textNode, mOffset);
  if (!candidatePoint.IsInNativeAnonymousSubtreeInTextControl() &&
      !HTMLEditUtils::IsSimplyEditableNode(*textNode)) {
    return EditorDOMPoint();
  }
  return candidatePoint;
}

// XXX: We may want to store the selection state and restore it properly.  Was
//     it an insertion point or an extended selection?
NS_IMETHODIMP DeleteTextTransaction::UndoTransaction() {
  MOZ_LOG(GetLogModule(), LogLevel::Info,
          ("%p DeleteTextTransaction::%s this=%s", this, __FUNCTION__,
           ToString(*this).c_str()));

  if (NS_WARN_IF(!mEditorBase)) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  const RefPtr<Text> textNode = GetTextNode();
  if (NS_WARN_IF(!textNode) ||
      (mEditorBase->IsHTMLEditor() &&
       NS_WARN_IF(!HTMLEditUtils::IsSimplyEditableNode(*textNode)))) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  const OwningNonNull<EditorBase> editorBase = *mEditorBase;
  IgnoredErrorResult error;
  editorBase->DoInsertText(*textNode, mOffset, mDeletedText, error);
  NS_WARNING_ASSERTION(!error.Failed(), "EditorBase::DoInsertText() failed");
  return error.StealNSResult();
}

NS_IMETHODIMP DeleteTextTransaction::RedoTransaction() {
  MOZ_LOG(GetLogModule(), LogLevel::Info,
          ("%p DeleteTextTransaction::%s this=%s", this, __FUNCTION__,
           !mEditorBase || mEditorBase->IsTextEditor()
               ? ToString(*this).c_str()
               : ToString(*GetAsDeleteTextFromTextNodeTransaction()).c_str()));
  nsresult rv = DoTransaction();
  if (NS_FAILED(rv)) {
    NS_WARNING("DeleteTextTransaction::DoTransaction() failed");
    return rv;
  }
  if (!mEditorBase || !mEditorBase->AllowsTransactionsToChangeSelection()) {
    return NS_OK;
  }
  const OwningNonNull<EditorBase> editorBase = *mEditorBase;
  rv = editorBase->CollapseSelectionTo(SuggestPointToPutCaret());
  if (NS_FAILED(rv)) {
    NS_WARNING("EditorBase::CollapseSelectionTo() failed");
    return rv;
  }
  return NS_OK;
}

/******************************************************************************
 * mozilla::DeleteTextFromTextNodeTransaction
 ******************************************************************************/

DeleteTextFromTextNodeTransaction::DeleteTextFromTextNodeTransaction(
    EditorBase& aEditorBase, Text& aTextNode, uint32_t aOffset,
    uint32_t aLengthToDelete)
    : DeleteTextTransaction(aEditorBase, aTextNode, aOffset, aLengthToDelete),
      mTextNode(&aTextNode) {
  MOZ_ASSERT(aEditorBase.IsHTMLEditor());
  MOZ_ASSERT(mTextNode->TextDataLength() >= aOffset + aLengthToDelete);
}

std::ostream& operator<<(
    std::ostream& aStream,
    const DeleteTextFromTextNodeTransaction& aTransaction) {
  aStream << "{ mTextNode=" << aTransaction.mTextNode.get();
  if (aTransaction.mTextNode) {
    aStream << " (" << *aTransaction.mTextNode << ")";
  }
  aStream << ", mOffset=" << aTransaction.mOffset
          << ", mLengthToDelete=" << aTransaction.mLengthToDelete
          << ", mDeletedText=\""
          << NS_ConvertUTF16toUTF8(aTransaction.mDeletedText).get() << "\""
          << ", mEditorBase=" << aTransaction.mEditorBase.get() << " }";
  return aStream;
}

NS_IMPL_CYCLE_COLLECTION_INHERITED(DeleteTextFromTextNodeTransaction,
                                   DeleteTextTransaction, mTextNode)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(DeleteTextFromTextNodeTransaction)
NS_INTERFACE_MAP_END_INHERITING(DeleteTextTransaction)

}  // namespace mozilla
