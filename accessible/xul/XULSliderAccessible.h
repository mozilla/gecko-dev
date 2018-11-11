/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_XULSliderAccessible_h__
#define mozilla_a11y_XULSliderAccessible_h__

#include "AccessibleWrap.h"

#include "nsIDOMElement.h"

namespace mozilla {
namespace a11y {

/**
 * Used for XUL slider and scale elements.
 */
class XULSliderAccessible : public AccessibleWrap
{
public:
  XULSliderAccessible(nsIContent* aContent, DocAccessible* aDoc);

  // Accessible
  virtual void Value(nsString& aValue) override;
  virtual a11y::role NativeRole() override;
  virtual uint64_t NativeInteractiveState() const override;
  virtual bool NativelyUnavailable() const override;

  // Value
  virtual double MaxValue() const override;
  virtual double MinValue() const override;
  virtual double CurValue() const override;
  virtual double Step() const override;
  virtual bool SetCurValue(double aValue) override;

  // ActionAccessible
  virtual uint8_t ActionCount() override;
  virtual void ActionNameAt(uint8_t aIndex, nsAString& aName) override;
  virtual bool DoAction(uint8_t aIndex) override;

protected:
  /**
   * Return anonymous slider element.
   */
  nsIContent* GetSliderElement() const;

  nsresult GetSliderAttr(nsIAtom *aName, nsAString& aValue) const;
  nsresult SetSliderAttr(nsIAtom *aName, const nsAString& aValue);

  double GetSliderAttr(nsIAtom *aName) const;
  bool SetSliderAttr(nsIAtom *aName, double aValue);

private:
  mutable nsCOMPtr<nsIContent> mSliderNode;
};


/**
 * Used for slider's thumb element.
 */
class XULThumbAccessible : public AccessibleWrap
{
public:
  XULThumbAccessible(nsIContent* aContent, DocAccessible* aDoc);

  // Accessible
  virtual a11y::role NativeRole() override;
};

} // namespace a11y
} // namespace mozilla

#endif

