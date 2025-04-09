/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_XULComboboxAccessible_h__
#define mozilla_a11y_XULComboboxAccessible_h__

#include "XULMenuAccessible.h"

namespace mozilla {
namespace a11y {

/**
 * Used for XUL comboboxes like xul:menulist and autocomplete textbox.
 */
class XULComboboxAccessible : public AccessibleWrap {
 public:
  enum { eAction_Click = 0 };

  XULComboboxAccessible(nsIContent* aContent, DocAccessible* aDoc);

  // LocalAccessible
  void Description(nsString& aDescription) const override;
  void Value(nsString& aValue) const override;
  a11y::role NativeRole() const override;
  uint64_t NativeState() const override;
  bool IsAcceptableChild(nsIContent*) const override;

  // ActionAccessible
  bool HasPrimaryAction() const override;
  void ActionNameAt(uint8_t aIndex, nsAString& aName) override;
  bool DoAction(uint8_t aIndex) const override;

  // Widgets
  bool IsActiveWidget() const override;
  MOZ_CAN_RUN_SCRIPT_BOUNDARY bool AreItemsOperable() const override;
};

/**
 * Used for the singular, global instance of a XULCombobox which is rendered
 * in the parent process and contains the options of the focused and expanded
 * HTML select in a content document. This combobox should have
 * id=ContentSelectDropdown
 */
class XULContentSelectDropdownAccessible : public XULComboboxAccessible {
 public:
  XULContentSelectDropdownAccessible(nsIContent* aContent, DocAccessible* aDoc)
      : XULComboboxAccessible(aContent, aDoc) {}
  // Accessible

  virtual Accessible* Parent() const override;
};

}  // namespace a11y
}  // namespace mozilla

#endif
