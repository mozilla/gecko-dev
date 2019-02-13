/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DeleteRangeTxn.h"

#include "DeleteNodeTxn.h"
#include "DeleteTextTxn.h"
#include "mozilla/Assertions.h"
#include "mozilla/dom/Selection.h"
#include "mozilla/mozalloc.h"
#include "nsCOMPtr.h"
#include "nsDebug.h"
#include "nsEditor.h"
#include "nsError.h"
#include "nsIContent.h"
#include "nsIContentIterator.h"
#include "nsIDOMCharacterData.h"
#include "nsINode.h"
#include "nsAString.h"

using namespace mozilla;
using namespace mozilla::dom;

// note that aEditor is not refcounted
DeleteRangeTxn::DeleteRangeTxn()
  : EditAggregateTxn(),
    mRange(),
    mEditor(nullptr),
    mRangeUpdater(nullptr)
{
}

NS_IMPL_CYCLE_COLLECTION_INHERITED(DeleteRangeTxn, EditAggregateTxn,
                                   mRange)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(DeleteRangeTxn)
NS_INTERFACE_MAP_END_INHERITING(EditAggregateTxn)

nsresult
DeleteRangeTxn::Init(nsEditor* aEditor,
                     nsRange* aRange,
                     nsRangeUpdater* aRangeUpdater)
{
  MOZ_ASSERT(aEditor && aRange);

  mEditor = aEditor;
  mRange = aRange->CloneRange();
  mRangeUpdater = aRangeUpdater;

  NS_ENSURE_TRUE(mEditor->IsModifiableNode(mRange->GetStartParent()),
                 NS_ERROR_FAILURE);
  NS_ENSURE_TRUE(mEditor->IsModifiableNode(mRange->GetEndParent()),
                 NS_ERROR_FAILURE);
  NS_ENSURE_TRUE(mEditor->IsModifiableNode(mRange->GetCommonAncestor()),
                 NS_ERROR_FAILURE);

  return NS_OK;
}

NS_IMETHODIMP
DeleteRangeTxn::DoTransaction()
{
  MOZ_ASSERT(mRange && mEditor);
  nsresult res;

  // build the child transactions
  nsCOMPtr<nsINode> startParent = mRange->GetStartParent();
  int32_t startOffset = mRange->StartOffset();
  nsCOMPtr<nsINode> endParent = mRange->GetEndParent();
  int32_t endOffset = mRange->EndOffset();
  MOZ_ASSERT(startParent && endParent);

  if (startParent == endParent) {
    // the selection begins and ends in the same node
    res = CreateTxnsToDeleteBetween(startParent, startOffset, endOffset);
    NS_ENSURE_SUCCESS(res, res);
  } else {
    // the selection ends in a different node from where it started.  delete
    // the relevant content in the start node
    res = CreateTxnsToDeleteContent(startParent, startOffset, nsIEditor::eNext);
    NS_ENSURE_SUCCESS(res, res);
    // delete the intervening nodes
    res = CreateTxnsToDeleteNodesBetween();
    NS_ENSURE_SUCCESS(res, res);
    // delete the relevant content in the end node
    res = CreateTxnsToDeleteContent(endParent, endOffset, nsIEditor::ePrevious);
    NS_ENSURE_SUCCESS(res, res);
  }

  // if we've successfully built this aggregate transaction, then do it.
  res = EditAggregateTxn::DoTransaction();
  NS_ENSURE_SUCCESS(res, res);

  // only set selection to deletion point if editor gives permission
  bool bAdjustSelection;
  mEditor->ShouldTxnSetSelection(&bAdjustSelection);
  if (bAdjustSelection) {
    nsRefPtr<Selection> selection = mEditor->GetSelection();
    NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);
    res = selection->Collapse(startParent, startOffset);
    NS_ENSURE_SUCCESS(res, res);
  }
  // else do nothing - dom range gravity will adjust selection

  return NS_OK;
}

NS_IMETHODIMP
DeleteRangeTxn::UndoTransaction()
{
  MOZ_ASSERT(mRange && mEditor);

  return EditAggregateTxn::UndoTransaction();
}

NS_IMETHODIMP
DeleteRangeTxn::RedoTransaction()
{
  MOZ_ASSERT(mRange && mEditor);

  return EditAggregateTxn::RedoTransaction();
}

NS_IMETHODIMP
DeleteRangeTxn::GetTxnDescription(nsAString& aString)
{
  aString.AssignLiteral("DeleteRangeTxn");
  return NS_OK;
}

nsresult
DeleteRangeTxn::CreateTxnsToDeleteBetween(nsINode* aNode,
                                          int32_t aStartOffset,
                                          int32_t aEndOffset)
{
  // see what kind of node we have
  if (aNode->IsNodeOfType(nsINode::eDATA_NODE)) {
    // if the node is a chardata node, then delete chardata content
    int32_t numToDel;
    if (aStartOffset == aEndOffset) {
      numToDel = 1;
    } else {
      numToDel = aEndOffset - aStartOffset;
    }

    nsRefPtr<nsGenericDOMDataNode> charDataNode =
      static_cast<nsGenericDOMDataNode*>(aNode);

    nsRefPtr<DeleteTextTxn> txn =
      new DeleteTextTxn(*mEditor, *charDataNode, aStartOffset, numToDel,
                        mRangeUpdater);

    nsresult res = txn->Init();
    NS_ENSURE_SUCCESS(res, res);

    AppendChild(txn);
    return NS_OK;
  }

  nsCOMPtr<nsIContent> child = aNode->GetChildAt(aStartOffset);
  NS_ENSURE_STATE(child);

  nsresult res = NS_OK;
  for (int32_t i = aStartOffset; i < aEndOffset; ++i) {
    nsRefPtr<DeleteNodeTxn> txn = new DeleteNodeTxn();
    res = txn->Init(mEditor, child, mRangeUpdater);
    if (NS_SUCCEEDED(res)) {
      AppendChild(txn);
    }

    child = child->GetNextSibling();
  }

  NS_ENSURE_SUCCESS(res, res);
  return NS_OK;
}

nsresult
DeleteRangeTxn::CreateTxnsToDeleteContent(nsINode* aNode,
                                          int32_t aOffset,
                                          nsIEditor::EDirection aAction)
{
  // see what kind of node we have
  if (aNode->IsNodeOfType(nsINode::eDATA_NODE)) {
    // if the node is a chardata node, then delete chardata content
    uint32_t start, numToDelete;
    if (nsIEditor::eNext == aAction) {
      start = aOffset;
      numToDelete = aNode->Length() - aOffset;
    } else {
      start = 0;
      numToDelete = aOffset;
    }

    if (numToDelete) {
      nsRefPtr<nsGenericDOMDataNode> dataNode =
        static_cast<nsGenericDOMDataNode*>(aNode);
      nsRefPtr<DeleteTextTxn> txn = new DeleteTextTxn(*mEditor, *dataNode,
          start, numToDelete, mRangeUpdater);

      nsresult res = txn->Init();
      NS_ENSURE_SUCCESS(res, res);

      AppendChild(txn);
    }
  }

  return NS_OK;
}

nsresult
DeleteRangeTxn::CreateTxnsToDeleteNodesBetween()
{
  nsCOMPtr<nsIContentIterator> iter = NS_NewContentSubtreeIterator();

  nsresult res = iter->Init(mRange);
  NS_ENSURE_SUCCESS(res, res);

  while (!iter->IsDone()) {
    nsCOMPtr<nsINode> node = iter->GetCurrentNode();
    NS_ENSURE_TRUE(node, NS_ERROR_NULL_POINTER);

    nsRefPtr<DeleteNodeTxn> txn = new DeleteNodeTxn();

    res = txn->Init(mEditor, node, mRangeUpdater);
    NS_ENSURE_SUCCESS(res, res);
    AppendChild(txn);

    iter->Next();
  }
  return NS_OK;
}
