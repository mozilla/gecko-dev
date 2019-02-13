/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_HTMLElementAccessibles_h__
#define mozilla_a11y_HTMLElementAccessibles_h__

#include "BaseAccessibles.h"
#include "nsAutoPtr.h"

namespace mozilla {
namespace a11y {

/**
 * Used for HTML hr element.
 */
class HTMLHRAccessible : public LeafAccessible
{
public:

  HTMLHRAccessible(nsIContent* aContent, DocAccessible* aDoc) :
    LeafAccessible(aContent, aDoc) {}

  // Accessible
  virtual a11y::role NativeRole() override;
};

/**
 * Used for HTML br element.
 */
class HTMLBRAccessible : public LeafAccessible
{
public:
  HTMLBRAccessible(nsIContent* aContent, DocAccessible* aDoc) :
    LeafAccessible(aContent, aDoc)
  {
    mType = eHTMLBRType;
  }

  // Accessible
  virtual a11y::role NativeRole() override;
  virtual uint64_t NativeState() override;

protected:
  // Accessible
  virtual ENameValueFlag NativeName(nsString& aName) override;
};

/**
 * Used for HTML label element.
 */
class HTMLLabelAccessible : public HyperTextAccessibleWrap
{
public:

  HTMLLabelAccessible(nsIContent* aContent, DocAccessible* aDoc) :
    HyperTextAccessibleWrap(aContent, aDoc) {}

  NS_DECL_ISUPPORTS_INHERITED

  // Accessible
  virtual Relation RelationByType(RelationType aType) override;

protected:
  virtual ~HTMLLabelAccessible() {}
  virtual ENameValueFlag NativeName(nsString& aName) override;
};

/**
 * Used for HTML output element.
 */
class HTMLOutputAccessible : public HyperTextAccessibleWrap
{
public:

  HTMLOutputAccessible(nsIContent* aContent, DocAccessible* aDoc) :
    HyperTextAccessibleWrap(aContent, aDoc) {}

  NS_DECL_ISUPPORTS_INHERITED

  // Accessible
  virtual Relation RelationByType(RelationType aType) override;

protected:
  virtual ~HTMLOutputAccessible() {}
};

} // namespace a11y
} // namespace mozilla

#endif
