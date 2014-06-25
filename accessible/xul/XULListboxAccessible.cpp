/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "XULListboxAccessible.h"

#include "Accessible-inl.h"
#include "nsAccessibilityService.h"
#include "nsAccUtils.h"
#include "DocAccessible.h"
#include "Role.h"
#include "States.h"

#include "nsComponentManagerUtils.h"
#include "nsIAutoCompleteInput.h"
#include "nsIAutoCompletePopup.h"
#include "nsIDOMXULMenuListElement.h"
#include "nsIDOMXULMultSelectCntrlEl.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMXULPopupElement.h"
#include "nsIDOMXULSelectCntrlItemEl.h"
#include "nsIMutableArray.h"
#include "nsIPersistentProperties2.h"

using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// XULColumAccessible
////////////////////////////////////////////////////////////////////////////////

XULColumAccessible::
  XULColumAccessible(nsIContent* aContent, DocAccessible* aDoc) :
  AccessibleWrap(aContent, aDoc)
{
}

role
XULColumAccessible::NativeRole()
{
  return roles::LIST;
}

uint64_t
XULColumAccessible::NativeState()
{
  return states::READONLY;
}


////////////////////////////////////////////////////////////////////////////////
// XULColumnItemAccessible
////////////////////////////////////////////////////////////////////////////////

XULColumnItemAccessible::
  XULColumnItemAccessible(nsIContent* aContent, DocAccessible* aDoc) :
  LeafAccessible(aContent, aDoc)
{
}

role
XULColumnItemAccessible::NativeRole()
{
  return roles::COLUMNHEADER;
}

uint64_t
XULColumnItemAccessible::NativeState()
{
  return states::READONLY;
}

uint8_t
XULColumnItemAccessible::ActionCount()
{
  return 1;
}

NS_IMETHODIMP
XULColumnItemAccessible::GetActionName(uint8_t aIndex, nsAString& aName)
{
  if (aIndex != eAction_Click)
    return NS_ERROR_INVALID_ARG;

  aName.AssignLiteral("click");
  return NS_OK;
}

NS_IMETHODIMP
XULColumnItemAccessible::DoAction(uint8_t aIndex)
{
  if (aIndex != eAction_Click)
    return NS_ERROR_INVALID_ARG;

  DoCommand();
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// XULListboxAccessible
////////////////////////////////////////////////////////////////////////////////

XULListboxAccessible::
  XULListboxAccessible(nsIContent* aContent, DocAccessible* aDoc) :
  XULSelectControlAccessible(aContent, aDoc), xpcAccessibleTable(this)
{
  nsIContent* parentContent = mContent->GetFlattenedTreeParent();
  if (parentContent) {
    nsCOMPtr<nsIAutoCompletePopup> autoCompletePopupElm =
      do_QueryInterface(parentContent);
    if (autoCompletePopupElm)
      mGenericTypes |= eAutoCompletePopup;
  }
}

NS_IMPL_ADDREF_INHERITED(XULListboxAccessible, XULSelectControlAccessible)
NS_IMPL_RELEASE_INHERITED(XULListboxAccessible, XULSelectControlAccessible)

nsresult
XULListboxAccessible::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  nsresult rv = XULSelectControlAccessible::QueryInterface(aIID, aInstancePtr);
  if (*aInstancePtr)
    return rv;

  if (aIID.Equals(NS_GET_IID(nsIAccessibleTable)) && IsMulticolumn()) {
    *aInstancePtr = static_cast<nsIAccessibleTable*>(this);
    NS_ADDREF_THIS();
    return NS_OK;
  }

  return NS_ERROR_NO_INTERFACE;
}

////////////////////////////////////////////////////////////////////////////////
// Accessible

void
XULListboxAccessible::Shutdown()
{
  mTable = nullptr;
  XULSelectControlAccessible::Shutdown();
}

bool
XULListboxAccessible::IsMulticolumn()
{
  int32_t numColumns = 0;
  nsresult rv = GetColumnCount(&numColumns);
  if (NS_FAILED(rv))
    return false;

  return numColumns > 1;
}

////////////////////////////////////////////////////////////////////////////////
// XULListboxAccessible. nsIAccessible

uint64_t
XULListboxAccessible::NativeState()
{
  // As a XULListboxAccessible we can have the following states:
  //   FOCUSED, READONLY, FOCUSABLE

  // Get focus status from base class
  uint64_t states = Accessible::NativeState();

  // see if we are multiple select if so set ourselves as such

  if (mContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::seltype,
                            nsGkAtoms::multiple, eCaseMatters)) {
      states |= states::MULTISELECTABLE | states::EXTSELECTABLE;
  }

  return states;
}

/**
  * Our value is the label of our ( first ) selected child.
  */
void
XULListboxAccessible::Value(nsString& aValue)
{
  aValue.Truncate();

  nsCOMPtr<nsIDOMXULSelectControlElement> select(do_QueryInterface(mContent));
  if (select) {
    nsCOMPtr<nsIDOMXULSelectControlItemElement> selectedItem;
    select->GetSelectedItem(getter_AddRefs(selectedItem));
    if (selectedItem)
      selectedItem->GetLabel(aValue);
  }
}

role
XULListboxAccessible::NativeRole()
{
  // A richlistbox is used with the new autocomplete URL bar, and has a parent
  // popup <panel>.
  nsCOMPtr<nsIDOMXULPopupElement> xulPopup =
    do_QueryInterface(mContent->GetParent());
  if (xulPopup)
    return roles::COMBOBOX_LIST;

  return IsMulticolumn() ? roles::TABLE : roles::LISTBOX;
}

////////////////////////////////////////////////////////////////////////////////
// XULListboxAccessible. nsIAccessibleTable

uint32_t
XULListboxAccessible::ColCount()
{
  nsIContent* headContent = nullptr;
  for (nsIContent* childContent = mContent->GetFirstChild(); childContent;
       childContent = childContent->GetNextSibling()) {
    if (childContent->NodeInfo()->Equals(nsGkAtoms::listcols,
                                         kNameSpaceID_XUL)) {
      headContent = childContent;
    }
  }
  if (!headContent)
    return 0;

  uint32_t columnCount = 0;
  for (nsIContent* childContent = headContent->GetFirstChild(); childContent;
       childContent = childContent->GetNextSibling()) {
    if (childContent->NodeInfo()->Equals(nsGkAtoms::listcol,
                                         kNameSpaceID_XUL)) {
      columnCount++;
    }
  }

  return columnCount;
}

uint32_t
XULListboxAccessible::RowCount()
{
  nsCOMPtr<nsIDOMXULSelectControlElement> element(do_QueryInterface(mContent));

  uint32_t itemCount = 0;
  if(element)
    element->GetItemCount(&itemCount);

  return itemCount;
}

Accessible*
XULListboxAccessible::CellAt(uint32_t aRowIndex, uint32_t aColumnIndex)
{
  nsCOMPtr<nsIDOMXULSelectControlElement> control =
    do_QueryInterface(mContent);
  NS_ENSURE_TRUE(control, nullptr);

  nsCOMPtr<nsIDOMXULSelectControlItemElement> item;
  control->GetItemAtIndex(aRowIndex, getter_AddRefs(item));
  if (!item)
    return nullptr;

  nsCOMPtr<nsIContent> itemContent(do_QueryInterface(item));
  if (!itemContent)
    return nullptr;

  Accessible* row = mDoc->GetAccessible(itemContent);
  NS_ENSURE_TRUE(row, nullptr);

  return row->GetChildAt(aColumnIndex);
}

bool
XULListboxAccessible::IsColSelected(uint32_t aColIdx)
{
  nsCOMPtr<nsIDOMXULMultiSelectControlElement> control =
    do_QueryInterface(mContent);
  NS_ASSERTION(control,
               "Doesn't implement nsIDOMXULMultiSelectControlElement.");

  int32_t selectedrowCount = 0;
  nsresult rv = control->GetSelectedCount(&selectedrowCount);
  NS_ENSURE_SUCCESS(rv, false);

  return selectedrowCount == static_cast<int32_t>(RowCount());
}

bool
XULListboxAccessible::IsRowSelected(uint32_t aRowIdx)
{
  nsCOMPtr<nsIDOMXULSelectControlElement> control =
    do_QueryInterface(mContent);
  NS_ASSERTION(control,
               "Doesn't implement nsIDOMXULSelectControlElement.");

  nsCOMPtr<nsIDOMXULSelectControlItemElement> item;
  nsresult rv = control->GetItemAtIndex(aRowIdx, getter_AddRefs(item));
  NS_ENSURE_SUCCESS(rv, false);

  bool isSelected = false;
  item->GetSelected(&isSelected);
  return isSelected;
}

bool
XULListboxAccessible::IsCellSelected(uint32_t aRowIdx, uint32_t aColIdx)
{
  return IsRowSelected(aRowIdx);
}

uint32_t
XULListboxAccessible::SelectedCellCount()
{
  nsCOMPtr<nsIDOMXULMultiSelectControlElement> control =
    do_QueryInterface(mContent);
  NS_ASSERTION(control,
               "Doesn't implement nsIDOMXULMultiSelectControlElement.");

  nsCOMPtr<nsIDOMNodeList> selectedItems;
  control->GetSelectedItems(getter_AddRefs(selectedItems));
  if (!selectedItems)
    return 0;

  uint32_t selectedItemsCount = 0;
  nsresult rv = selectedItems->GetLength(&selectedItemsCount);
  NS_ENSURE_SUCCESS(rv, 0);

  return selectedItemsCount * ColCount();
}

uint32_t
XULListboxAccessible::SelectedColCount()
{
  nsCOMPtr<nsIDOMXULMultiSelectControlElement> control =
    do_QueryInterface(mContent);
  NS_ASSERTION(control,
               "Doesn't implement nsIDOMXULMultiSelectControlElement.");

  int32_t selectedRowCount = 0;
  nsresult rv = control->GetSelectedCount(&selectedRowCount);
  NS_ENSURE_SUCCESS(rv, 0);

  return selectedRowCount > 0 &&
   selectedRowCount == static_cast<int32_t>(RowCount()) ? ColCount() : 0;
}

uint32_t
XULListboxAccessible::SelectedRowCount()
{
  nsCOMPtr<nsIDOMXULMultiSelectControlElement> control =
    do_QueryInterface(mContent);
  NS_ASSERTION(control,
               "Doesn't implement nsIDOMXULMultiSelectControlElement.");

  int32_t selectedRowCount = 0;
  nsresult rv = control->GetSelectedCount(&selectedRowCount);
  NS_ENSURE_SUCCESS(rv, 0);

  return selectedRowCount >= 0 ? selectedRowCount : 0;
}

void
XULListboxAccessible::SelectedCells(nsTArray<Accessible*>* aCells)
{
  nsCOMPtr<nsIDOMXULMultiSelectControlElement> control =
    do_QueryInterface(mContent);
  NS_ASSERTION(control,
               "Doesn't implement nsIDOMXULMultiSelectControlElement.");

  nsCOMPtr<nsIDOMNodeList> selectedItems;
  control->GetSelectedItems(getter_AddRefs(selectedItems));
  if (!selectedItems)
    return;

  uint32_t selectedItemsCount = 0;
  DebugOnly<nsresult> rv = selectedItems->GetLength(&selectedItemsCount);
  NS_ASSERTION(NS_SUCCEEDED(rv), "GetLength() Shouldn't fail!");

  for (uint32_t index = 0; index < selectedItemsCount; index++) {
    nsCOMPtr<nsIDOMNode> itemNode;
    selectedItems->Item(index, getter_AddRefs(itemNode));
    nsCOMPtr<nsIContent> itemContent(do_QueryInterface(itemNode));
    Accessible* item = mDoc->GetAccessible(itemContent);

    if (item) {
      uint32_t cellCount = item->ChildCount();
      for (uint32_t cellIdx = 0; cellIdx < cellCount; cellIdx++) {
        Accessible* cell = mChildren[cellIdx];
        if (cell->Role() == roles::CELL)
          aCells->AppendElement(cell);
      }
    }
  }
}

void
XULListboxAccessible::SelectedCellIndices(nsTArray<uint32_t>* aCells)
{
  nsCOMPtr<nsIDOMXULMultiSelectControlElement> control =
    do_QueryInterface(mContent);
  NS_ASSERTION(control,
               "Doesn't implement nsIDOMXULMultiSelectControlElement.");

  nsCOMPtr<nsIDOMNodeList> selectedItems;
  control->GetSelectedItems(getter_AddRefs(selectedItems));
  if (!selectedItems)
    return;

  uint32_t selectedItemsCount = 0;
  DebugOnly<nsresult> rv = selectedItems->GetLength(&selectedItemsCount);
  NS_ASSERTION(NS_SUCCEEDED(rv), "GetLength() Shouldn't fail!");

  uint32_t colCount = ColCount();
  aCells->SetCapacity(selectedItemsCount * colCount);
  aCells->AppendElements(selectedItemsCount * colCount);

  for (uint32_t selItemsIdx = 0, cellsIdx = 0;
       selItemsIdx < selectedItemsCount; selItemsIdx++) {

    nsCOMPtr<nsIDOMNode> itemNode;
    selectedItems->Item(selItemsIdx, getter_AddRefs(itemNode));
    nsCOMPtr<nsIDOMXULSelectControlItemElement> item =
      do_QueryInterface(itemNode);

    if (item) {
      int32_t itemIdx = -1;
      control->GetIndexOfItem(item, &itemIdx);
      if (itemIdx >= 0)
        for (uint32_t colIdx = 0; colIdx < colCount; colIdx++, cellsIdx++)
          aCells->ElementAt(cellsIdx) = itemIdx * colCount + colIdx;
    }
  }
}

void
XULListboxAccessible::SelectedColIndices(nsTArray<uint32_t>* aCols)
{
  uint32_t selColCount = SelectedColCount();
  aCols->SetCapacity(selColCount);

  for (uint32_t colIdx = 0; colIdx < selColCount; colIdx++)
    aCols->AppendElement(colIdx);
}

void
XULListboxAccessible::SelectedRowIndices(nsTArray<uint32_t>* aRows)
{
  nsCOMPtr<nsIDOMXULMultiSelectControlElement> control =
    do_QueryInterface(mContent);
  NS_ASSERTION(control,
               "Doesn't implement nsIDOMXULMultiSelectControlElement.");

  nsCOMPtr<nsIDOMNodeList> selectedItems;
  control->GetSelectedItems(getter_AddRefs(selectedItems));
  if (!selectedItems)
    return;

  uint32_t rowCount = 0;
  DebugOnly<nsresult> rv = selectedItems->GetLength(&rowCount);
  NS_ASSERTION(NS_SUCCEEDED(rv), "GetLength() Shouldn't fail!");

  if (!rowCount)
    return;

  aRows->SetCapacity(rowCount);
  aRows->AppendElements(rowCount);

  for (uint32_t rowIdx = 0; rowIdx < rowCount; rowIdx++) {
    nsCOMPtr<nsIDOMNode> itemNode;
    selectedItems->Item(rowIdx, getter_AddRefs(itemNode));
    nsCOMPtr<nsIDOMXULSelectControlItemElement> item =
      do_QueryInterface(itemNode);

    if (item) {
      int32_t itemIdx = -1;
      control->GetIndexOfItem(item, &itemIdx);
      if (itemIdx >= 0)
        aRows->ElementAt(rowIdx) = itemIdx;
    }
  }
}

void
XULListboxAccessible::SelectRow(uint32_t aRowIdx)
{
  nsCOMPtr<nsIDOMXULMultiSelectControlElement> control =
    do_QueryInterface(mContent);
  NS_ASSERTION(control,
               "Doesn't implement nsIDOMXULMultiSelectControlElement.");

  nsCOMPtr<nsIDOMXULSelectControlItemElement> item;
  control->GetItemAtIndex(aRowIdx, getter_AddRefs(item));
  control->SelectItem(item);
}

void
XULListboxAccessible::UnselectRow(uint32_t aRowIdx)
{
  nsCOMPtr<nsIDOMXULMultiSelectControlElement> control =
    do_QueryInterface(mContent);
  NS_ASSERTION(control,
               "Doesn't implement nsIDOMXULMultiSelectControlElement.");

  nsCOMPtr<nsIDOMXULSelectControlItemElement> item;
  control->GetItemAtIndex(aRowIdx, getter_AddRefs(item));
  control->RemoveItemFromSelection(item);
}

////////////////////////////////////////////////////////////////////////////////
// XULListboxAccessible: Widgets

bool
XULListboxAccessible::IsWidget() const
{
  return true;
}

bool
XULListboxAccessible::IsActiveWidget() const
{
  if (IsAutoCompletePopup()) {
    nsCOMPtr<nsIAutoCompletePopup> autoCompletePopupElm =
      do_QueryInterface(mContent->GetParent());

    if (autoCompletePopupElm) {
      bool isOpen = false;
      autoCompletePopupElm->GetPopupOpen(&isOpen);
      return isOpen;
    }
  }
  return FocusMgr()->HasDOMFocus(mContent);
}

bool
XULListboxAccessible::AreItemsOperable() const
{
  if (IsAutoCompletePopup()) {
    nsCOMPtr<nsIAutoCompletePopup> autoCompletePopupElm =
      do_QueryInterface(mContent->GetParent());

    if (autoCompletePopupElm) {
      bool isOpen = false;
      autoCompletePopupElm->GetPopupOpen(&isOpen);
      return isOpen;
    }
  }
  return true;
}

Accessible*
XULListboxAccessible::ContainerWidget() const
{
  if (IsAutoCompletePopup()) {
    // This works for XUL autocompletes. It doesn't work for HTML forms
    // autocomplete because of potential crossprocess calls (when autocomplete
    // lives in content process while popup lives in chrome process). If that's
    // a problem then rethink Widgets interface.
    nsCOMPtr<nsIDOMXULMenuListElement> menuListElm =
      do_QueryInterface(mContent->GetParent());
    if (menuListElm) {
      nsCOMPtr<nsIDOMNode> inputElm;
      menuListElm->GetInputField(getter_AddRefs(inputElm));
      if (inputElm) {
        nsCOMPtr<nsINode> inputNode = do_QueryInterface(inputElm);
        if (inputNode) {
          Accessible* input =
            mDoc->GetAccessible(inputNode);
          return input ? input->ContainerWidget() : nullptr;
        }
      }
    }
  }
  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
// XULListitemAccessible
////////////////////////////////////////////////////////////////////////////////

XULListitemAccessible::
  XULListitemAccessible(nsIContent* aContent, DocAccessible* aDoc) :
  XULMenuitemAccessible(aContent, aDoc)
{
  mIsCheckbox = mContent->AttrValueIs(kNameSpaceID_None,
                                      nsGkAtoms::type,
                                      nsGkAtoms::checkbox,
                                      eCaseMatters);
  mType = eXULListItemType;
}

NS_IMPL_ISUPPORTS_INHERITED0(XULListitemAccessible, Accessible)

Accessible*
XULListitemAccessible::GetListAccessible()
{
  if (IsDefunct())
    return nullptr;

  nsCOMPtr<nsIDOMXULSelectControlItemElement> listItem =
    do_QueryInterface(mContent);
  if (!listItem)
    return nullptr;

  nsCOMPtr<nsIDOMXULSelectControlElement> list;
  listItem->GetControl(getter_AddRefs(list));

  nsCOMPtr<nsIContent> listContent(do_QueryInterface(list));
  if (!listContent)
    return nullptr;

  return mDoc->GetAccessible(listContent);
}

////////////////////////////////////////////////////////////////////////////////
// XULListitemAccessible Accessible

void
XULListitemAccessible::Description(nsString& aDesc)
{
  AccessibleWrap::Description(aDesc);
}

////////////////////////////////////////////////////////////////////////////////
// XULListitemAccessible. nsIAccessible

/**
  * If there is a Listcell as a child ( not anonymous ) use it, otherwise
  *   default to getting the name from GetXULName
  */
ENameValueFlag
XULListitemAccessible::NativeName(nsString& aName)
{
  nsIContent* childContent = mContent->GetFirstChild();
  if (childContent) {
    if (childContent->NodeInfo()->Equals(nsGkAtoms::listcell,
                                         kNameSpaceID_XUL)) {
      childContent->GetAttr(kNameSpaceID_None, nsGkAtoms::label, aName);
      return eNameOK;
    }
  }

  return Accessible::NativeName(aName);
}

role
XULListitemAccessible::NativeRole()
{
  Accessible* list = GetListAccessible();
  if (!list) {
    NS_ERROR("No list accessible for listitem accessible!");
    return roles::NOTHING;
  }

  if (list->Role() == roles::TABLE)
    return roles::ROW;

  if (mIsCheckbox)
    return roles::CHECK_RICH_OPTION;

  if (mParent && mParent->Role() == roles::COMBOBOX_LIST)
    return roles::COMBOBOX_OPTION;

  return roles::RICH_OPTION;
}

uint64_t
XULListitemAccessible::NativeState()
{
  if (mIsCheckbox)
    return XULMenuitemAccessible::NativeState();

  uint64_t states = NativeInteractiveState();

  nsCOMPtr<nsIDOMXULSelectControlItemElement> listItem =
    do_QueryInterface(mContent);

  if (listItem) {
    bool isSelected;
    listItem->GetSelected(&isSelected);
    if (isSelected)
      states |= states::SELECTED;

    if (FocusMgr()->IsFocused(this))
      states |= states::FOCUSED;
  }

  return states;
}

uint64_t
XULListitemAccessible::NativeInteractiveState() const
{
  return NativelyUnavailable() || (mParent && mParent->NativelyUnavailable()) ?
    states::UNAVAILABLE : states::FOCUSABLE | states::SELECTABLE;
}

NS_IMETHODIMP
XULListitemAccessible::GetActionName(uint8_t aIndex, nsAString& aName)
{
  if (aIndex == eAction_Click && mIsCheckbox) {
    // check or uncheck
    uint64_t states = NativeState();

    if (states & states::CHECKED)
      aName.AssignLiteral("uncheck");
    else
      aName.AssignLiteral("check");

    return NS_OK;
  }
  return NS_ERROR_INVALID_ARG;
}

bool
XULListitemAccessible::CanHaveAnonChildren()
{
  // That indicates we should walk anonymous children for listitems
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// XULListitemAccessible: Widgets

Accessible*
XULListitemAccessible::ContainerWidget() const
{
  return Parent();
}


////////////////////////////////////////////////////////////////////////////////
// XULListCellAccessible
////////////////////////////////////////////////////////////////////////////////

XULListCellAccessible::
  XULListCellAccessible(nsIContent* aContent, DocAccessible* aDoc) :
  HyperTextAccessibleWrap(aContent, aDoc), xpcAccessibleTableCell(this)
{
  mGenericTypes |= eTableCell;
}

////////////////////////////////////////////////////////////////////////////////
// nsISupports

NS_IMPL_ISUPPORTS_INHERITED(XULListCellAccessible,
                            HyperTextAccessible,
                            nsIAccessibleTableCell)

////////////////////////////////////////////////////////////////////////////////
// XULListCellAccessible: nsIAccessibleTableCell implementation

TableAccessible*
XULListCellAccessible::Table() const
{
  Accessible* thisRow = Parent();
  if (!thisRow || thisRow->Role() != roles::ROW)
    return nullptr;

  Accessible* table = thisRow->Parent();
  if (!table || table->Role() != roles::TABLE)
    return nullptr;

  return table->AsTable();
}

uint32_t
XULListCellAccessible::ColIdx() const
{
  Accessible* row = Parent();
  if (!row)
    return 0;

  int32_t indexInRow = IndexInParent();
  uint32_t colIdx = 0;
  for (int32_t idx = 0; idx < indexInRow; idx++) {
    Accessible* cell = row->GetChildAt(idx);
    roles::Role role = cell->Role();
    if (role == roles::CELL || role == roles::GRID_CELL ||
        role == roles::ROWHEADER || role == roles::COLUMNHEADER)
      colIdx++;
  }

  return colIdx;
}

uint32_t
XULListCellAccessible::RowIdx() const
{
  Accessible* row = Parent();
  if (!row)
    return 0;

  Accessible* table = row->Parent();
  if (!table)
    return 0;

  int32_t indexInTable = row->IndexInParent();
  uint32_t rowIdx = 0;
  for (int32_t idx = 0; idx < indexInTable; idx++) {
    row = table->GetChildAt(idx);
    if (row->Role() == roles::ROW)
      rowIdx++;
  }

  return rowIdx;
}

void
XULListCellAccessible::ColHeaderCells(nsTArray<Accessible*>* aCells)
{
  TableAccessible* table = Table();
  NS_ASSERTION(table, "cell not in a table!");
  if (!table)
    return;

  // Get column header cell from XUL listhead.
  Accessible* list = nullptr;

  Accessible* tableAcc = table->AsAccessible();
  uint32_t tableChildCount = tableAcc->ChildCount();
  for (uint32_t childIdx = 0; childIdx < tableChildCount; childIdx++) {
    Accessible* child = tableAcc->GetChildAt(childIdx);
    if (child->Role() == roles::LIST) {
      list = child;
      break;
    }
  }

  if (list) {
    Accessible* headerCell = list->GetChildAt(ColIdx());
    if (headerCell) {
      aCells->AppendElement(headerCell);
      return;
    }
  }

  // No column header cell from XUL markup, try to get it from ARIA markup.
  TableCellAccessible::ColHeaderCells(aCells);
}

bool
XULListCellAccessible::Selected()
{
  TableAccessible* table = Table();
  NS_ENSURE_TRUE(table, false); // we expect to be in a listbox (table)

  return table->IsRowSelected(RowIdx());
}

////////////////////////////////////////////////////////////////////////////////
// XULListCellAccessible. Accessible implementation

void
XULListCellAccessible::Shutdown()
{
  mTableCell = nullptr;
  HyperTextAccessibleWrap::Shutdown();
}

role
XULListCellAccessible::NativeRole()
{
  return roles::CELL;
}

already_AddRefed<nsIPersistentProperties>
XULListCellAccessible::NativeAttributes()
{
  nsCOMPtr<nsIPersistentProperties> attributes =
    HyperTextAccessibleWrap::NativeAttributes();

  // "table-cell-index" attribute
  TableAccessible* table = Table();
  if (!table) // we expect to be in a listbox (table)
    return attributes.forget();

  nsAutoString stringIdx;
  stringIdx.AppendInt(table->CellIndexAt(RowIdx(), ColIdx()));
  nsAccUtils::SetAccAttr(attributes, nsGkAtoms::tableCellIndex, stringIdx);

  return attributes.forget();
}
