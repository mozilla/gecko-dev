/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ReplaceTextTransaction.h"

#include "HTMLEditUtils.h"

#include "mozilla/Logging.h"
#include "mozilla/OwningNonNull.h"
#include "mozilla/TextEditor.h"
#include "mozilla/ToString.h"

namespace mozilla {

using namespace dom;

// static
already_AddRefed<ReplaceTextTransaction> ReplaceTextTransaction::Create(
    EditorBase& aEditorBase, const nsAString& aStringToInsert,
    dom::Text& aTextNode, uint32_t aStartOffset, uint32_t aLength) {
  MOZ_ASSERT(aLength > 0, "Use InsertTextTransaction instead");
  MOZ_ASSERT(!aStringToInsert.IsEmpty(), "Use DeleteTextTransaction instead");
  MOZ_ASSERT(aTextNode.Length() >= aStartOffset);
  MOZ_ASSERT(aTextNode.Length() >= aStartOffset + aLength);

  RefPtr<ReplaceTextTransaction> transaction =
      aEditorBase.IsTextEditor()
          ? new ReplaceTextTransaction(aEditorBase, aStringToInsert, aTextNode,
                                       aStartOffset, aLength)
          : new ReplaceTextInTextNodeTransaction(
                aEditorBase, aStringToInsert, aTextNode, aStartOffset, aLength);
  return transaction.forget();
}

std::ostream& operator<<(std::ostream& aStream,
                         const ReplaceTextTransaction& aTransaction) {
  const auto* transactionForHTMLEditor =
      aTransaction.GetAsReplaceTextInTextNodeTransaction();
  if (transactionForHTMLEditor) {
    return aStream << *transactionForHTMLEditor;
  }
  aStream << "{ mStringToInsert=\""
          << NS_ConvertUTF16toUTF8(aTransaction.mStringToInsert).get() << "\""
          << ", mStringToBeReplaced=\""
          << NS_ConvertUTF16toUTF8(aTransaction.mStringToBeReplaced).get()
          << "\", mOffset=" << aTransaction.mOffset
          << ", mEditorBase=" << aTransaction.mEditorBase.get() << " }";
  return aStream;
}

NS_IMPL_CYCLE_COLLECTION_INHERITED(ReplaceTextTransaction, EditTransactionBase,
                                   mEditorBase)

NS_IMPL_ADDREF_INHERITED(ReplaceTextTransaction, EditTransactionBase)
NS_IMPL_RELEASE_INHERITED(ReplaceTextTransaction, EditTransactionBase)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ReplaceTextTransaction)
NS_INTERFACE_MAP_END_INHERITING(EditTransactionBase)

Text* ReplaceTextTransaction::GetTextNode() const {
  if (MOZ_UNLIKELY(!mEditorBase)) {
    return nullptr;
  }
  if (TextEditor* const textEditor = mEditorBase->GetAsTextEditor()) {
    return textEditor->GetTextNode();
  }
  MOZ_ASSERT(GetAsReplaceTextInTextNodeTransaction());
  return GetAsReplaceTextInTextNodeTransaction()->mTextNode;
}

NS_IMETHODIMP ReplaceTextTransaction::DoTransaction() {
  MOZ_LOG(GetLogModule(), LogLevel::Info,
          ("%p ReplaceTextTransaction::%s this=%s", this, __FUNCTION__,
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
  editorBase->DoReplaceText(*textNode, mOffset, mStringToBeReplaced.Length(),
                            mStringToInsert, error);
  if (MOZ_UNLIKELY(error.Failed())) {
    NS_WARNING("EditorBase::DoReplaceText() failed");
    return error.StealNSResult();
  }
  // XXX What should we do if mutation event listener changed the node?
  editorBase->RangeUpdaterRef().SelAdjReplaceText(*textNode, mOffset,
                                                  mStringToBeReplaced.Length(),
                                                  mStringToInsert.Length());
  return NS_OK;
}

NS_IMETHODIMP ReplaceTextTransaction::UndoTransaction() {
  MOZ_LOG(GetLogModule(), LogLevel::Info,
          ("%p ReplaceTextTransaction::%s this=%s", this, __FUNCTION__,
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

  IgnoredErrorResult error;
  nsAutoString insertedString;
  textNode->SubstringData(mOffset, mStringToInsert.Length(), insertedString,
                          error);
  if (MOZ_UNLIKELY(error.Failed())) {
    NS_WARNING("CharacterData::SubstringData() failed");
    return error.StealNSResult();
  }
  if (MOZ_UNLIKELY(insertedString != mStringToInsert)) {
    NS_WARNING(
        "ReplaceTextTransaction::UndoTransaction() did nothing due to "
        "unexpected text");
    return NS_OK;
  }

  const OwningNonNull<EditorBase> editorBase = *mEditorBase;

  editorBase->DoReplaceText(*textNode, mOffset, mStringToInsert.Length(),
                            mStringToBeReplaced, error);
  if (MOZ_UNLIKELY(error.Failed())) {
    NS_WARNING("EditorBase::DoReplaceText() failed");
    return error.StealNSResult();
  }
  // XXX What should we do if mutation event listener changed the node?
  editorBase->RangeUpdaterRef().SelAdjReplaceText(*textNode, mOffset,
                                                  mStringToInsert.Length(),
                                                  mStringToBeReplaced.Length());

  if (!editorBase->AllowsTransactionsToChangeSelection()) {
    return NS_OK;
  }

  // XXX Should we stop setting selection when mutation event listener
  //     modifies the text node?
  editorBase->CollapseSelectionTo(
      EditorRawDOMPoint(textNode, mOffset + mStringToBeReplaced.Length()),
      error);
  if (MOZ_UNLIKELY(error.ErrorCodeIs(NS_ERROR_EDITOR_DESTROYED))) {
    NS_WARNING(
        "EditorBase::CollapseSelectionTo() caused destroying the editor");
    return NS_ERROR_EDITOR_DESTROYED;
  }
  NS_ASSERTION(!error.Failed(),
               "EditorBase::CollapseSelectionTo() failed, but ignored");
  return NS_OK;
}

NS_IMETHODIMP ReplaceTextTransaction::RedoTransaction() {
  MOZ_LOG(GetLogModule(), LogLevel::Info,
          ("%p ReplaceTextTransaction::%s this=%s", this, __FUNCTION__,
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

  IgnoredErrorResult error;
  nsAutoString undoneString;
  textNode->SubstringData(mOffset, mStringToBeReplaced.Length(), undoneString,
                          error);
  if (MOZ_UNLIKELY(error.Failed())) {
    NS_WARNING("CharacterData::SubstringData() failed");
    return error.StealNSResult();
  }
  if (MOZ_UNLIKELY(undoneString != mStringToBeReplaced)) {
    NS_WARNING(
        "ReplaceTextTransaction::RedoTransaction() did nothing due to "
        "unexpected text");
    return NS_OK;
  }

  const OwningNonNull<EditorBase> editorBase = *mEditorBase;
  editorBase->DoReplaceText(*textNode, mOffset, mStringToBeReplaced.Length(),
                            mStringToInsert, error);
  if (MOZ_UNLIKELY(error.Failed())) {
    NS_WARNING("EditorBase::DoReplaceText() failed");
    return error.StealNSResult();
  }
  // XXX What should we do if mutation event listener changed the node?
  editorBase->RangeUpdaterRef().SelAdjReplaceText(*textNode, mOffset,
                                                  mStringToBeReplaced.Length(),
                                                  mStringToInsert.Length());

  if (!editorBase->AllowsTransactionsToChangeSelection()) {
    return NS_OK;
  }

  // XXX Should we stop setting selection when mutation event listener
  //     modifies the text node?
  editorBase->CollapseSelectionTo(SuggestPointToPutCaret<EditorRawDOMPoint>(),
                                  error);
  if (MOZ_UNLIKELY(error.ErrorCodeIs(NS_ERROR_EDITOR_DESTROYED))) {
    NS_WARNING(
        "EditorBase::CollapseSelectionTo() caused destroying the editor");
    return NS_ERROR_EDITOR_DESTROYED;
  }
  NS_ASSERTION(!error.Failed(),
               "EditorBase::CollapseSelectionTo() failed, but ignored");
  return NS_OK;
}

/******************************************************************************
 *
 ******************************************************************************/

std::ostream& operator<<(std::ostream& aStream,
                         const ReplaceTextInTextNodeTransaction& aTransaction) {
  aStream << "{ mTextNode=" << aTransaction.mTextNode.get();
  if (aTransaction.mTextNode) {
    aStream << " (" << *aTransaction.mTextNode << ")";
  }
  aStream << ", mStringToInsert=\""
          << NS_ConvertUTF16toUTF8(aTransaction.mStringToInsert).get() << "\""
          << ", mStringToBeReplaced=\""
          << NS_ConvertUTF16toUTF8(aTransaction.mStringToBeReplaced).get()
          << "\", mOffset=" << aTransaction.mOffset
          << ", mEditorBase=" << aTransaction.mEditorBase.get() << " }";
  return aStream;
}

NS_IMPL_CYCLE_COLLECTION_INHERITED(ReplaceTextInTextNodeTransaction,
                                   ReplaceTextTransaction, mTextNode)

NS_IMPL_ADDREF_INHERITED(ReplaceTextInTextNodeTransaction,
                         ReplaceTextTransaction)
NS_IMPL_RELEASE_INHERITED(ReplaceTextInTextNodeTransaction,
                          ReplaceTextTransaction)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ReplaceTextInTextNodeTransaction)
NS_INTERFACE_MAP_END_INHERITING(ReplaceTextTransaction)

}  // namespace mozilla
