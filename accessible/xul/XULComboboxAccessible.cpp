/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "XULComboboxAccessible.h"

#include "LocalAccessible-inl.h"
#include "nsAccessibilityService.h"
#include "DocAccessible.h"
#include "nsCoreUtils.h"
#include "nsFocusManager.h"

#include "mozilla/a11y/DocAccessibleParent.h"
#include "mozilla/a11y/Role.h"
#include "States.h"

#include "mozilla/dom/Element.h"
#include "nsIDOMXULMenuListElement.h"

using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// XULComboboxAccessible
////////////////////////////////////////////////////////////////////////////////

XULComboboxAccessible::XULComboboxAccessible(nsIContent* aContent,
                                             DocAccessible* aDoc)
    : AccessibleWrap(aContent, aDoc) {
  mGenericTypes |= eCombobox;
}

role XULComboboxAccessible::NativeRole() const { return roles::COMBOBOX; }

uint64_t XULComboboxAccessible::NativeState() const {
  // As a nsComboboxAccessible we can have the following states:
  //     STATE_FOCUSED
  //     STATE_FOCUSABLE
  //     STATE_HASPOPUP
  //     STATE_EXPANDED
  //     STATE_COLLAPSED

  // Get focus status from base class
  uint64_t state = LocalAccessible::NativeState();

  nsCOMPtr<nsIDOMXULMenuListElement> menuList = Elm()->AsXULMenuList();
  if (menuList) {
    bool isOpen = false;
    menuList->GetOpen(&isOpen);
    if (isOpen) {
      state |= states::EXPANDED;
    } else {
      state |= states::COLLAPSED;
    }
  }

  return state | states::HASPOPUP;
}

bool XULComboboxAccessible::IsAcceptableChild(nsIContent* aContent) const {
  return AccessibleWrap::IsAcceptableChild(aContent) && !aContent->IsText();
}

void XULComboboxAccessible::Description(nsString& aDescription) const {
  aDescription.Truncate();
  // Use description of currently focused option
  nsCOMPtr<nsIDOMXULMenuListElement> menuListElm = Elm()->AsXULMenuList();
  if (!menuListElm) return;

  nsCOMPtr<dom::Element> focusedOptionItem;
  menuListElm->GetSelectedItem(getter_AddRefs(focusedOptionItem));
  if (focusedOptionItem && mDoc) {
    LocalAccessible* focusedOptionAcc = mDoc->GetAccessible(focusedOptionItem);
    if (focusedOptionAcc) focusedOptionAcc->Description(aDescription);
  }
}

void XULComboboxAccessible::Value(nsString& aValue) const {
  aValue.Truncate();

  // The value is the option or text shown entered in the combobox.
  nsCOMPtr<nsIDOMXULMenuListElement> menuList = Elm()->AsXULMenuList();
  if (menuList) menuList->GetLabel(aValue);
}

bool XULComboboxAccessible::HasPrimaryAction() const { return true; }

bool XULComboboxAccessible::DoAction(uint8_t aIndex) const {
  if (aIndex != XULComboboxAccessible::eAction_Click) return false;

  // Programmaticaly toggle the combo box.
  nsCOMPtr<nsIDOMXULMenuListElement> menuList = Elm()->AsXULMenuList();
  if (!menuList) return false;

  bool isDroppedDown = false;
  menuList->GetOpen(&isDroppedDown);
  menuList->SetOpen(!isDroppedDown);
  return true;
}

void XULComboboxAccessible::ActionNameAt(uint8_t aIndex, nsAString& aName) {
  aName.Truncate();
  if (aIndex != XULComboboxAccessible::eAction_Click) return;

  nsCOMPtr<nsIDOMXULMenuListElement> menuList = Elm()->AsXULMenuList();
  if (!menuList) return;

  bool isDroppedDown = false;
  menuList->GetOpen(&isDroppedDown);
  if (isDroppedDown) {
    aName.AssignLiteral("close");
  } else {
    aName.AssignLiteral("open");
  }
}

////////////////////////////////////////////////////////////////////////////////
// Widgets

bool XULComboboxAccessible::IsActiveWidget() const {
  if (mContent->AsElement()->AttrValueIs(kNameSpaceID_None, nsGkAtoms::editable,
                                         nsGkAtoms::_true, eIgnoreCase)) {
    int32_t childCount = mChildren.Length();
    for (int32_t idx = 0; idx < childCount; idx++) {
      LocalAccessible* child = mChildren[idx];
      if (child->Role() == roles::ENTRY) {
        return FocusMgr()->HasDOMFocus(child->GetContent());
      }
    }
    return false;
  }

  return FocusMgr()->HasDOMFocus(mContent);
}

bool XULComboboxAccessible::AreItemsOperable() const {
  nsCOMPtr<nsIDOMXULMenuListElement> menuListElm = Elm()->AsXULMenuList();
  if (menuListElm) {
    bool isOpen = false;
    menuListElm->GetOpen(&isOpen);
    return isOpen;
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
// XULContentSelectDropdownAccessible
////////////////////////////////////////////////////////////////////////////////

Accessible* XULContentSelectDropdownAccessible::Parent() const {
  // We render the expanded dropdown for <select>s in the parent process
  // as a child of the application accessible. This confuses some
  // ATs which expect the select to _always_ parent the dropdown (in
  // both expanded and collapsed states).
  // To rectify this, we spoof the <select> as the parent of the
  // expanded dropdown here. Note that we do not spoof the child relationship.

  // First, try to find the select that spawned this dropdown.
  // The select that was activated does not get states::EXPANDED, but
  // it should still have focus.
  Accessible* focusedAcc = nullptr;
  if (auto* focusedNode = FocusMgr()->FocusedDOMNode()) {
    // If we get a node here, we're in a non-remote browser.
    DocAccessible* doc =
        GetAccService()->GetDocAccessible(focusedNode->OwnerDoc());
    focusedAcc = doc->GetAccessible(focusedNode);
  } else {
    nsFocusManager* focusManagerDOM = nsFocusManager::GetFocusManager();
    dom::BrowsingContext* focusedContext =
        focusManagerDOM->GetFocusedBrowsingContextInChrome();

    DocAccessibleParent* focusedDoc =
        DocAccessibleParent::GetFrom(focusedContext);
    if (NS_WARN_IF(!focusedDoc)) {
      // We can fail to get a document here if a user is
      // performing a drag-and-drop selection with mouse. See
      // `browser/base/content/tests/browser_selectpopup_large.js`
      return LocalParent();
    }
    MOZ_ASSERT(focusedDoc->IsDoc(), "Got non-document?");
    focusedAcc = focusedDoc->AsDoc()->GetFocusedAcc();
  }

  if (!NS_WARN_IF(focusedAcc && focusedAcc->IsHTMLCombobox())) {
    // We can sometimes get a document here if the select that
    // this dropdown should anchor to loses focus. This can happen when
    // calling AXPressed on macOS. Call into the regular parent
    // function instead.
    return LocalParent();
  }

  return focusedAcc;
}
