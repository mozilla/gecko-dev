/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "InsertTextTransaction.h"

#include "ErrorList.h"
#include "mozilla/EditorBase.h"  // mEditorBase
#include "mozilla/Logging.h"
#include "mozilla/SelectionState.h"  // RangeUpdater
#include "mozilla/TextEditor.h"      // TextEditor
#include "mozilla/ToString.h"
#include "mozilla/dom/Selection.h"  // Selection local var
#include "mozilla/dom/Text.h"       // mTextNode

#include "nsAString.h"      // nsAString parameter
#include "nsDebug.h"        // for NS_ASSERTION, etc.
#include "nsError.h"        // for NS_OK, etc.
#include "nsQueryObject.h"  // for do_QueryObject

namespace mozilla {

using namespace dom;

// static
already_AddRefed<InsertTextTransaction> InsertTextTransaction::Create(
    EditorBase& aEditorBase, const nsAString& aStringToInsert,
    const EditorDOMPointInText& aPointToInsert) {
  MOZ_ASSERT(aPointToInsert.IsSetAndValid());
  RefPtr<InsertTextTransaction> transaction =
      aEditorBase.IsTextEditor()
          ? new InsertTextTransaction(aEditorBase, aStringToInsert,
                                      aPointToInsert)
          : new InsertTextIntoTextNodeTransaction(aEditorBase, aStringToInsert,
                                                  aPointToInsert);
  return transaction.forget();
}

InsertTextTransaction::InsertTextTransaction(
    EditorBase& aEditorBase, const nsAString& aStringToInsert,
    const EditorDOMPointInText& aPointToInsert)
    : mEditorBase(&aEditorBase),
      mStringToInsert(aStringToInsert),
      mOffset(aPointToInsert.Offset()) {}

std::ostream& operator<<(std::ostream& aStream,
                         const InsertTextTransaction& aTransaction) {
  const auto* transactionForHTMLEditor =
      aTransaction.GetAsInsertTextIntoTextNodeTransaction();
  if (transactionForHTMLEditor) {
    return aStream << *transactionForHTMLEditor;
  }
  aStream << "{ mOffset=" << aTransaction.mOffset << ", mStringToInsert=\""
          << NS_ConvertUTF16toUTF8(aTransaction.mStringToInsert).get() << "\""
          << ", mEditorBase=" << aTransaction.mEditorBase.get() << " }";
  return aStream;
}

NS_IMPL_CYCLE_COLLECTION_INHERITED(InsertTextTransaction, EditTransactionBase,
                                   mEditorBase)

NS_IMPL_ADDREF_INHERITED(InsertTextTransaction, EditTransactionBase)
NS_IMPL_RELEASE_INHERITED(InsertTextTransaction, EditTransactionBase)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(InsertTextTransaction)
NS_INTERFACE_MAP_END_INHERITING(EditTransactionBase)

Text* InsertTextTransaction::GetTextNode() const {
  if (MOZ_UNLIKELY(!mEditorBase)) {
    return nullptr;
  }
  if (TextEditor* const textEditor = mEditorBase->GetAsTextEditor()) {
    return textEditor->GetTextNode();
  }
  MOZ_ASSERT(GetAsInsertTextIntoTextNodeTransaction());
  return GetAsInsertTextIntoTextNodeTransaction()->mTextNode;
}

NS_IMETHODIMP InsertTextTransaction::DoTransaction() {
  MOZ_LOG(GetLogModule(), LogLevel::Info,
          ("%p InsertTextTransaction::%s this=%s", this, __FUNCTION__,
           ToString(*this).c_str()));

  if (NS_WARN_IF(!mEditorBase)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  const RefPtr<Text> textNode = GetTextNode();
  if (NS_WARN_IF(!textNode)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  const OwningNonNull<EditorBase> editorBase = *mEditorBase;
  ErrorResult error;
  editorBase->DoInsertText(*textNode, mOffset, mStringToInsert, error);
  if (error.Failed()) {
    NS_WARNING("EditorBase::DoInsertText() failed");
    return error.StealNSResult();
  }

  editorBase->RangeUpdaterRef().SelAdjInsertText(*textNode, mOffset,
                                                 mStringToInsert.Length());
  return NS_OK;
}

NS_IMETHODIMP InsertTextTransaction::UndoTransaction() {
  MOZ_LOG(GetLogModule(), LogLevel::Info,
          ("%p InsertTextTransaction::%s this=%s", this, __FUNCTION__,
           ToString(*this).c_str()));

  if (NS_WARN_IF(!mEditorBase)) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  const RefPtr<Text> textNode = GetTextNode();
  if (NS_WARN_IF(!textNode)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  const OwningNonNull<EditorBase> editorBase = *mEditorBase;
  ErrorResult error;
  editorBase->DoDeleteText(*textNode, mOffset, mStringToInsert.Length(), error);
  NS_WARNING_ASSERTION(!error.Failed(), "EditorBase::DoDeleteText() failed");
  return error.StealNSResult();
}

NS_IMETHODIMP InsertTextTransaction::RedoTransaction() {
  MOZ_LOG(GetLogModule(), LogLevel::Info,
          ("%p InsertTextTransaction::%s this=%s", this, __FUNCTION__,
           !mEditorBase || mEditorBase->IsTextEditor()
               ? ToString(*this).c_str()
               : ToString(*GetAsInsertTextIntoTextNodeTransaction()).c_str()));
  nsresult rv = DoTransaction();
  if (NS_FAILED(rv)) {
    NS_WARNING("InsertTextTransaction::DoTransaction() failed");
    return rv;
  }
  if (RefPtr<EditorBase> editorBase = mEditorBase) {
    nsresult rv = editorBase->CollapseSelectionTo(
        SuggestPointToPutCaret<EditorRawDOMPoint>());
    if (NS_WARN_IF(rv == NS_ERROR_EDITOR_DESTROYED)) {
      return NS_ERROR_EDITOR_DESTROYED;
    }
    NS_WARNING_ASSERTION(
        NS_SUCCEEDED(rv),
        "EditorBase::CollapseSelectionTo() failed, but ignored");
  }
  return NS_OK;
}

NS_IMETHODIMP InsertTextTransaction::Merge(nsITransaction* aOtherTransaction,
                                           bool* aDidMerge) {
  MOZ_LOG(GetLogModule(), LogLevel::Debug,
          ("%p InsertTextTransaction::%s(aOtherTransaction=%p) this=%s", this,
           __FUNCTION__, aOtherTransaction, ToString(*this).c_str()));

  if (NS_WARN_IF(!aOtherTransaction) || NS_WARN_IF(!aDidMerge)) {
    return NS_ERROR_INVALID_ARG;
  }
  // Set out param default value
  *aDidMerge = false;

  RefPtr<EditTransactionBase> otherTransactionBase =
      aOtherTransaction->GetAsEditTransactionBase();
  if (!otherTransactionBase) {
    MOZ_LOG(
        GetLogModule(), LogLevel::Debug,
        ("%p InsertTextTransaction::%s(aOtherTransaction=%p) returned false",
         this, __FUNCTION__, aOtherTransaction));
    return NS_OK;
  }

  // If aTransaction is a InsertTextTransaction, and if the selection hasn't
  // changed, then absorb it.
  InsertTextTransaction* otherInsertTextTransaction =
      otherTransactionBase->GetAsInsertTextTransaction();
  if (!otherInsertTextTransaction ||
      !IsSequentialInsert(*otherInsertTextTransaction)) {
    MOZ_LOG(
        GetLogModule(), LogLevel::Debug,
        ("%p InsertTextTransaction::%s(aOtherTransaction=%p) returned false",
         this, __FUNCTION__, aOtherTransaction));
    return NS_OK;
  }

  mStringToInsert += otherInsertTextTransaction->GetData();
  *aDidMerge = true;
  MOZ_LOG(GetLogModule(), LogLevel::Debug,
          ("%p InsertTextTransaction::%s(aOtherTransaction=%p) returned true",
           this, __FUNCTION__, aOtherTransaction));
  return NS_OK;
}

bool InsertTextTransaction::IsSequentialInsert(
    InsertTextTransaction& aOtherTransaction) const {
  return aOtherTransaction.GetTextNode() == GetTextNode() &&
         aOtherTransaction.mOffset == mOffset + mStringToInsert.Length();
}

/******************************************************************************
 * mozilla::InsertTextIntoTextNodeTransaction
 ******************************************************************************/

InsertTextIntoTextNodeTransaction::InsertTextIntoTextNodeTransaction(
    EditorBase& aEditorBase, const nsAString& aStringToInsert,
    const EditorDOMPointInText& aPointToInsert)
    : InsertTextTransaction(aEditorBase, aStringToInsert, aPointToInsert),
      mTextNode(aPointToInsert.ContainerAs<Text>()) {
  MOZ_ASSERT(aEditorBase.IsHTMLEditor());
}

std::ostream& operator<<(
    std::ostream& aStream,
    const InsertTextIntoTextNodeTransaction& aTransaction) {
  aStream << "{ mTextNode=" << aTransaction.mTextNode.get();
  if (aTransaction.mTextNode) {
    aStream << " (" << *aTransaction.mTextNode << ")";
  }
  aStream << ", mOffset=" << aTransaction.mOffset << ", mStringToInsert=\""
          << NS_ConvertUTF16toUTF8(aTransaction.mStringToInsert).get() << "\""
          << ", mEditorBase=" << aTransaction.mEditorBase.get() << " }";
  return aStream;
}

NS_IMPL_CYCLE_COLLECTION_INHERITED(InsertTextIntoTextNodeTransaction,
                                   InsertTextTransaction, mTextNode)

NS_IMPL_ADDREF_INHERITED(InsertTextIntoTextNodeTransaction,
                         InsertTextTransaction)
NS_IMPL_RELEASE_INHERITED(InsertTextIntoTextNodeTransaction,
                          InsertTextTransaction)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(InsertTextIntoTextNodeTransaction)
NS_INTERFACE_MAP_END_INHERITING(InsertTextTransaction)

}  // namespace mozilla
