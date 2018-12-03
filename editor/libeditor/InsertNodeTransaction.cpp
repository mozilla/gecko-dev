/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "InsertNodeTransaction.h"

#include "mozilla/EditorBase.h"      // for EditorBase
#include "mozilla/EditorDOMPoint.h"  // for EditorDOMPoint

#include "mozilla/dom/Selection.h"  // for Selection

#include "nsAString.h"
#include "nsDebug.h"          // for NS_ENSURE_TRUE, etc.
#include "nsError.h"          // for NS_ERROR_NULL_POINTER, etc.
#include "nsIContent.h"       // for nsIContent
#include "nsMemory.h"         // for nsMemory
#include "nsReadableUtils.h"  // for ToNewCString
#include "nsString.h"         // for nsString

namespace mozilla {

using namespace dom;

template already_AddRefed<InsertNodeTransaction> InsertNodeTransaction::Create(
    EditorBase& aEditorBase, nsIContent& aContentToInsert,
    const EditorDOMPoint& aPointToInsert);
template already_AddRefed<InsertNodeTransaction> InsertNodeTransaction::Create(
    EditorBase& aEditorBase, nsIContent& aContentToInsert,
    const EditorRawDOMPoint& aPointToInsert);

// static
template <typename PT, typename CT>
already_AddRefed<InsertNodeTransaction> InsertNodeTransaction::Create(
    EditorBase& aEditorBase, nsIContent& aContentToInsert,
    const EditorDOMPointBase<PT, CT>& aPointToInsert) {
  RefPtr<InsertNodeTransaction> transaction =
      new InsertNodeTransaction(aEditorBase, aContentToInsert, aPointToInsert);
  return transaction.forget();
}

template <typename PT, typename CT>
InsertNodeTransaction::InsertNodeTransaction(
    EditorBase& aEditorBase, nsIContent& aContentToInsert,
    const EditorDOMPointBase<PT, CT>& aPointToInsert)
    : mContentToInsert(&aContentToInsert),
      mPointToInsert(aPointToInsert),
      mEditorBase(&aEditorBase) {
  MOZ_ASSERT(mPointToInsert.IsSetAndValid());
  // Ensure mPointToInsert stores child at offset.
  Unused << mPointToInsert.GetChild();
}

InsertNodeTransaction::~InsertNodeTransaction() {}

NS_IMPL_CYCLE_COLLECTION_INHERITED(InsertNodeTransaction, EditTransactionBase,
                                   mEditorBase, mContentToInsert,
                                   mPointToInsert)

NS_IMPL_ADDREF_INHERITED(InsertNodeTransaction, EditTransactionBase)
NS_IMPL_RELEASE_INHERITED(InsertNodeTransaction, EditTransactionBase)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(InsertNodeTransaction)
NS_INTERFACE_MAP_END_INHERITING(EditTransactionBase)

NS_IMETHODIMP
InsertNodeTransaction::DoTransaction() {
  if (NS_WARN_IF(!mEditorBase) || NS_WARN_IF(!mContentToInsert) ||
      NS_WARN_IF(!mPointToInsert.IsSet())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  if (!mPointToInsert.IsSetAndValid()) {
    // It seems that DOM tree has been changed after first DoTransaction()
    // and current RedoTranaction() call.
    if (mPointToInsert.GetChild()) {
      EditorDOMPoint newPointToInsert(mPointToInsert.GetChild());
      if (!newPointToInsert.IsSet()) {
        // The insertion point has been removed from the DOM tree.
        // In this case, we should append the node to the container instead.
        newPointToInsert.SetToEndOf(mPointToInsert.GetContainer());
        if (NS_WARN_IF(!newPointToInsert.IsSet())) {
          return NS_ERROR_FAILURE;
        }
      }
      mPointToInsert = newPointToInsert;
    } else {
      mPointToInsert.SetToEndOf(mPointToInsert.GetContainer());
      if (NS_WARN_IF(!mPointToInsert.IsSet())) {
        return NS_ERROR_FAILURE;
      }
    }
  }

  mEditorBase->MarkNodeDirty(mContentToInsert);

  ErrorResult error;
  mPointToInsert.GetContainer()->InsertBefore(*mContentToInsert,
                                              mPointToInsert.GetChild(), error);
  error.WouldReportJSException();
  if (NS_WARN_IF(error.Failed())) {
    return error.StealNSResult();
  }

  if (!mEditorBase->AllowsTransactionsToChangeSelection()) {
    return NS_OK;
  }

  RefPtr<Selection> selection = mEditorBase->GetSelection();
  if (NS_WARN_IF(!selection)) {
    return NS_ERROR_FAILURE;
  }

  // Place the selection just after the inserted element.
  EditorRawDOMPoint afterInsertedNode(mContentToInsert);
  DebugOnly<bool> advanced = afterInsertedNode.AdvanceOffset();
  NS_WARNING_ASSERTION(advanced,
                       "Failed to advance offset after the inserted node");
  selection->Collapse(afterInsertedNode, error);
  if (NS_WARN_IF(error.Failed())) {
    error.SuppressException();
  }
  return NS_OK;
}

NS_IMETHODIMP
InsertNodeTransaction::UndoTransaction() {
  if (NS_WARN_IF(!mContentToInsert) || NS_WARN_IF(!mPointToInsert.IsSet())) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  // XXX If the inserted node has been moved to different container node or
  //     just removed from the DOM tree, this always fails.
  ErrorResult error;
  mPointToInsert.GetContainer()->RemoveChild(*mContentToInsert, error);
  if (NS_WARN_IF(error.Failed())) {
    return error.StealNSResult();
  }
  return NS_OK;
}

}  // namespace mozilla
