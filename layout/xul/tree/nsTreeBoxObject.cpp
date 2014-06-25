/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsTreeBoxObject.h"
#include "nsCOMPtr.h"
#include "nsIDOMXULElement.h"
#include "nsIXULTemplateBuilder.h"
#include "nsTreeContentView.h"
#include "nsITreeSelection.h"
#include "ChildIterator.h"
#include "nsContentUtils.h"
#include "nsError.h"
#include "nsTreeBodyFrame.h"

NS_IMPL_CYCLE_COLLECTION_INHERITED(nsTreeBoxObject, nsBoxObject,
                                   mView)

NS_IMPL_ADDREF_INHERITED(nsTreeBoxObject, nsBoxObject)
NS_IMPL_RELEASE_INHERITED(nsTreeBoxObject, nsBoxObject)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(nsTreeBoxObject)
  NS_INTERFACE_MAP_ENTRY(nsITreeBoxObject)
NS_INTERFACE_MAP_END_INHERITING(nsBoxObject)

void
nsTreeBoxObject::Clear()
{
  ClearCachedValues();

  // Drop the view's ref to us.
  if (mView) {
    nsCOMPtr<nsITreeSelection> sel;
    mView->GetSelection(getter_AddRefs(sel));
    if (sel)
      sel->SetTree(nullptr);
    mView->SetTree(nullptr); // Break the circular ref between the view and us.
  }
  mView = nullptr;

  nsBoxObject::Clear();
}


nsTreeBoxObject::nsTreeBoxObject()
  : mTreeBody(nullptr)
{
}

nsTreeBoxObject::~nsTreeBoxObject()
{
  /* destructor code */
}

static nsIContent* FindBodyElement(nsIContent* aParent)
{
  mozilla::dom::FlattenedChildIterator iter(aParent);
  for (nsIContent* content = iter.GetNextChild(); content; content = iter.GetNextChild()) {
    mozilla::dom::NodeInfo *ni = content->NodeInfo();
    if (ni->Equals(nsGkAtoms::treechildren, kNameSpaceID_XUL)) {
      return content;
    } else if (ni->Equals(nsGkAtoms::tree, kNameSpaceID_XUL)) {
      // There are nesting tree elements. Only the innermost should
      // find the treechilren.
      return nullptr;
    } else if (content->IsElement() &&
               !ni->Equals(nsGkAtoms::_template, kNameSpaceID_XUL)) {
      nsIContent* result = FindBodyElement(content);
      if (result)
        return result;
    }
  }

  return nullptr;
}

nsTreeBodyFrame*
nsTreeBoxObject::GetTreeBody(bool aFlushLayout)
{
  // Make sure our frames are up to date, and layout as needed.  We
  // have to do this before checking for our cached mTreeBody, since
  // it might go away on style flush, and in any case if aFlushLayout
  // is true we need to make sure to flush no matter what.
  // XXXbz except that flushing style when we were not asked to flush
  // layout here breaks things.  See bug 585123.
  nsIFrame* frame;
  if (aFlushLayout) {
    frame = GetFrame(aFlushLayout);
    if (!frame)
      return nullptr;
  }

  if (mTreeBody) {
    // Have one cached already.
    return mTreeBody;
  }

  if (!aFlushLayout) {
    frame = GetFrame(aFlushLayout);
    if (!frame)
      return nullptr;
  }

  // Iterate over our content model children looking for the body.
  nsCOMPtr<nsIContent> content = FindBodyElement(frame->GetContent());
  if (!content)
    return nullptr;

  frame = content->GetPrimaryFrame();
  if (!frame)
     return nullptr;

  // Make sure that the treebodyframe has a pointer to |this|.
  nsTreeBodyFrame *treeBody = do_QueryFrame(frame);
  NS_ENSURE_TRUE(treeBody && treeBody->GetTreeBoxObject() == this, nullptr);

  mTreeBody = treeBody;
  return mTreeBody;
}

NS_IMETHODIMP nsTreeBoxObject::GetView(nsITreeView * *aView)
{
  if (!mTreeBody) {
    if (!GetTreeBody()) {
      // Don't return an uninitialised view
      *aView = nullptr;
      return NS_OK;
    }

    if (mView)
      // Our new frame needs to initialise itself
      return mTreeBody->GetView(aView);
  }
  if (!mView) {
    nsCOMPtr<nsIDOMXULElement> xulele = do_QueryInterface(mContent);
    if (xulele) {
      // See if there is a XUL tree builder associated with the element
      nsCOMPtr<nsIXULTemplateBuilder> builder;
      xulele->GetBuilder(getter_AddRefs(builder));
      mView = do_QueryInterface(builder);

      if (!mView) {
        // No tree builder, create a tree content view.
        nsresult rv = NS_NewTreeContentView(getter_AddRefs(mView));
        NS_ENSURE_SUCCESS(rv, rv);
      }

      // Initialise the frame and view
      mTreeBody->SetView(mView);
    }
  }
  NS_IF_ADDREF(*aView = mView);
  return NS_OK;
}

static bool
CanTrustView(nsISupports* aValue)
{
  // Untrusted content is only allowed to specify known-good views
  if (nsContentUtils::IsCallerChrome())
    return true;
  nsCOMPtr<nsINativeTreeView> nativeTreeView = do_QueryInterface(aValue);
  if (!nativeTreeView || NS_FAILED(nativeTreeView->EnsureNative())) {
    // XXX ERRMSG need a good error here for developers
    return false;
  }
  return true;
}

NS_IMETHODIMP nsTreeBoxObject::SetView(nsITreeView * aView)
{
  if (!CanTrustView(aView))
    return NS_ERROR_DOM_SECURITY_ERR;
  
  mView = aView;
  nsTreeBodyFrame* body = GetTreeBody();
  if (body)
    body->SetView(aView);

  return NS_OK;
}

NS_IMETHODIMP nsTreeBoxObject::GetFocused(bool* aFocused)
{
  *aFocused = false;
  nsTreeBodyFrame* body = GetTreeBody();
  if (body)
    return body->GetFocused(aFocused);
  return NS_OK;
}

NS_IMETHODIMP nsTreeBoxObject::SetFocused(bool aFocused)
{
  nsTreeBodyFrame* body = GetTreeBody();
  if (body)
    return body->SetFocused(aFocused);
  return NS_OK;
}

NS_IMETHODIMP nsTreeBoxObject::GetTreeBody(nsIDOMElement** aElement)
{
  *aElement = nullptr;
  nsTreeBodyFrame* body = GetTreeBody();
  if (body) 
    return body->GetTreeBody(aElement);
  return NS_OK;
}

NS_IMETHODIMP nsTreeBoxObject::GetColumns(nsITreeColumns** aColumns)
{
  *aColumns = nullptr;
  nsTreeBodyFrame* body = GetTreeBody();
  if (body) 
    *aColumns = body->Columns().take();
  return NS_OK;
}

NS_IMETHODIMP nsTreeBoxObject::GetRowHeight(int32_t* aRowHeight)
{
  *aRowHeight = 0;
  nsTreeBodyFrame* body = GetTreeBody();
  if (body) 
    return body->GetRowHeight(aRowHeight);
  return NS_OK;
}

NS_IMETHODIMP nsTreeBoxObject::GetRowWidth(int32_t *aRowWidth)
{
  *aRowWidth = 0;
  nsTreeBodyFrame* body = GetTreeBody();
  if (body) 
    return body->GetRowWidth(aRowWidth);
  return NS_OK;
}

NS_IMETHODIMP nsTreeBoxObject::GetFirstVisibleRow(int32_t *aFirstVisibleRow)
{
  *aFirstVisibleRow = 0;
  nsTreeBodyFrame* body = GetTreeBody();
  if (body)
    *aFirstVisibleRow = body->FirstVisibleRow();
  return NS_OK;
}

NS_IMETHODIMP nsTreeBoxObject::GetLastVisibleRow(int32_t *aLastVisibleRow)
{
  *aLastVisibleRow = 0;
  nsTreeBodyFrame* body = GetTreeBody();
  if (body)
    *aLastVisibleRow = body->LastVisibleRow();
  return NS_OK;
}

NS_IMETHODIMP nsTreeBoxObject::GetHorizontalPosition(int32_t *aHorizontalPosition)
{
  *aHorizontalPosition = 0;
  nsTreeBodyFrame* body = GetTreeBody();
  if (body)
    return body->GetHorizontalPosition(aHorizontalPosition);
  return NS_OK;
}

NS_IMETHODIMP nsTreeBoxObject::GetPageLength(int32_t *aPageLength)
{
  *aPageLength = 0;
  nsTreeBodyFrame* body = GetTreeBody();
  if (body)
    *aPageLength = body->PageLength();
  return NS_OK;
}

NS_IMETHODIMP nsTreeBoxObject::GetSelectionRegion(nsIScriptableRegion **aRegion)
{
 *aRegion = nullptr;
  nsTreeBodyFrame* body = GetTreeBody();
  if (body)
    return body->GetSelectionRegion(aRegion);
  return NS_OK;
}

NS_IMETHODIMP
nsTreeBoxObject::EnsureRowIsVisible(int32_t aRow)
{
  nsTreeBodyFrame* body = GetTreeBody();
  if (body)
    return body->EnsureRowIsVisible(aRow);
  return NS_OK;
}

NS_IMETHODIMP 
nsTreeBoxObject::EnsureCellIsVisible(int32_t aRow, nsITreeColumn* aCol)
{
  nsTreeBodyFrame* body = GetTreeBody();
  if (body)
    return body->EnsureCellIsVisible(aRow, aCol);
  return NS_OK;
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsTreeBoxObject::ScrollToRow(int32_t aRow)
{
  nsTreeBodyFrame* body = GetTreeBody(true);
  if (body)
    return body->ScrollToRow(aRow);
  return NS_OK;
}

NS_IMETHODIMP
nsTreeBoxObject::ScrollByLines(int32_t aNumLines)
{
  nsTreeBodyFrame* body = GetTreeBody();
  if (body)
    return body->ScrollByLines(aNumLines);
  return NS_OK;
}

NS_IMETHODIMP
nsTreeBoxObject::ScrollByPages(int32_t aNumPages)
{
  nsTreeBodyFrame* body = GetTreeBody();
  if (body)
    return body->ScrollByPages(aNumPages);
  return NS_OK;
}

NS_IMETHODIMP 
nsTreeBoxObject::ScrollToCell(int32_t aRow, nsITreeColumn* aCol)
{
  nsTreeBodyFrame* body = GetTreeBody();
  if (body)
    return body->ScrollToCell(aRow, aCol);
  return NS_OK;
}

NS_IMETHODIMP 
nsTreeBoxObject::ScrollToColumn(nsITreeColumn* aCol)
{
  nsTreeBodyFrame* body = GetTreeBody();
  if (body)
    return body->ScrollToColumn(aCol);
  return NS_OK;
}

NS_IMETHODIMP 
nsTreeBoxObject::ScrollToHorizontalPosition(int32_t aHorizontalPosition)
{
  nsTreeBodyFrame* body = GetTreeBody();
  if (body)
    return body->ScrollToHorizontalPosition(aHorizontalPosition);
  return NS_OK;
}

NS_IMETHODIMP nsTreeBoxObject::Invalidate()
{
  nsTreeBodyFrame* body = GetTreeBody();
  if (body)
    return body->Invalidate();
  return NS_OK;
}

NS_IMETHODIMP nsTreeBoxObject::InvalidateColumn(nsITreeColumn* aCol)
{
  nsTreeBodyFrame* body = GetTreeBody();
  if (body)
    return body->InvalidateColumn(aCol);
  return NS_OK;
}

NS_IMETHODIMP nsTreeBoxObject::InvalidateRow(int32_t aIndex)
{
  nsTreeBodyFrame* body = GetTreeBody();
  if (body)
    return body->InvalidateRow(aIndex);
  return NS_OK;
}

NS_IMETHODIMP nsTreeBoxObject::InvalidateCell(int32_t aRow, nsITreeColumn* aCol)
{
  nsTreeBodyFrame* body = GetTreeBody();
  if (body)
    return body->InvalidateCell(aRow, aCol);
  return NS_OK;
}

NS_IMETHODIMP nsTreeBoxObject::InvalidateRange(int32_t aStart, int32_t aEnd)
{
  nsTreeBodyFrame* body = GetTreeBody();
  if (body)
    return body->InvalidateRange(aStart, aEnd);
  return NS_OK;
}

NS_IMETHODIMP nsTreeBoxObject::InvalidateColumnRange(int32_t aStart, int32_t aEnd, nsITreeColumn* aCol)
{
  nsTreeBodyFrame* body = GetTreeBody();
  if (body)
    return body->InvalidateColumnRange(aStart, aEnd, aCol);
  return NS_OK;
}

NS_IMETHODIMP nsTreeBoxObject::GetRowAt(int32_t x, int32_t y, int32_t *aRow)
{
  *aRow = 0;
  nsTreeBodyFrame* body = GetTreeBody();
  if (body)
    return body->GetRowAt(x, y, aRow);
  return NS_OK;
}

NS_IMETHODIMP nsTreeBoxObject::GetCellAt(int32_t aX, int32_t aY, int32_t *aRow, nsITreeColumn** aCol,
                                         nsACString& aChildElt)
{
  *aRow = 0;
  *aCol = nullptr;
  nsTreeBodyFrame* body = GetTreeBody();
  if (body)
    return body->GetCellAt(aX, aY, aRow, aCol, aChildElt);
  return NS_OK;
}

NS_IMETHODIMP
nsTreeBoxObject::GetCoordsForCellItem(int32_t aRow, nsITreeColumn* aCol, const nsACString& aElement, 
                                      int32_t *aX, int32_t *aY, int32_t *aWidth, int32_t *aHeight)
{
  *aX = *aY = *aWidth = *aHeight = 0;
  nsTreeBodyFrame* body = GetTreeBody();
  if (body)
    return body->GetCoordsForCellItem(aRow, aCol, aElement, aX, aY, aWidth, aHeight);
  return NS_OK;
}

NS_IMETHODIMP
nsTreeBoxObject::IsCellCropped(int32_t aRow, nsITreeColumn* aCol, bool *aIsCropped)
{  
  *aIsCropped = false;
  nsTreeBodyFrame* body = GetTreeBody();
  if (body)
    return body->IsCellCropped(aRow, aCol, aIsCropped);
  return NS_OK;
}

NS_IMETHODIMP nsTreeBoxObject::RowCountChanged(int32_t aIndex, int32_t aDelta)
{
  nsTreeBodyFrame* body = GetTreeBody();
  if (body)
    return body->RowCountChanged(aIndex, aDelta);
  return NS_OK;
}

NS_IMETHODIMP nsTreeBoxObject::BeginUpdateBatch()
{
  nsTreeBodyFrame* body = GetTreeBody();
  if (body)
    return body->BeginUpdateBatch();
  return NS_OK;
}

NS_IMETHODIMP nsTreeBoxObject::EndUpdateBatch()
{
  nsTreeBodyFrame* body = GetTreeBody();
  if (body)
    return body->EndUpdateBatch();
  return NS_OK;
}

NS_IMETHODIMP nsTreeBoxObject::ClearStyleAndImageCaches()
{
  nsTreeBodyFrame* body = GetTreeBody();
  if (body)
    return body->ClearStyleAndImageCaches();
  return NS_OK;
}

void
nsTreeBoxObject::ClearCachedValues()
{
  mTreeBody = nullptr;
}    

// Creation Routine ///////////////////////////////////////////////////////////////////////

nsresult
NS_NewTreeBoxObject(nsIBoxObject** aResult)
{
  *aResult = new nsTreeBoxObject;
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(*aResult);
  return NS_OK;
}

