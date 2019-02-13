/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BaseAccessibles.h"

#include "Accessible-inl.h"
#include "HyperTextAccessibleWrap.h"
#include "nsAccessibilityService.h"
#include "nsAccUtils.h"
#include "nsCoreUtils.h"
#include "Role.h"
#include "States.h"
#include "nsIURI.h"

using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// LeafAccessible
////////////////////////////////////////////////////////////////////////////////

LeafAccessible::
  LeafAccessible(nsIContent* aContent, DocAccessible* aDoc) :
  AccessibleWrap(aContent, aDoc)
{
}

NS_IMPL_ISUPPORTS_INHERITED0(LeafAccessible, Accessible)

////////////////////////////////////////////////////////////////////////////////
// LeafAccessible: Accessible public

Accessible*
LeafAccessible::ChildAtPoint(int32_t aX, int32_t aY,
                             EWhichChildAtPoint aWhichChild)
{
  // Don't walk into leaf accessibles.
  return this;
}

bool
LeafAccessible::InsertChildAt(uint32_t aIndex, Accessible* aChild)
{
  NS_NOTREACHED("InsertChildAt called on leaf accessible!");
  return false;
}

bool
LeafAccessible::RemoveChild(Accessible* aChild)
{
  NS_NOTREACHED("RemoveChild called on leaf accessible!");
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// LeafAccessible: Accessible private

void
LeafAccessible::CacheChildren()
{
  // No children for leaf accessible.
}


////////////////////////////////////////////////////////////////////////////////
// LinkableAccessible
////////////////////////////////////////////////////////////////////////////////

LinkableAccessible::
  LinkableAccessible(nsIContent* aContent, DocAccessible* aDoc) :
  AccessibleWrap(aContent, aDoc),
  mActionAcc(nullptr),
  mIsLink(false),
  mIsOnclick(false)
{
}

NS_IMPL_ISUPPORTS_INHERITED0(LinkableAccessible, AccessibleWrap)

////////////////////////////////////////////////////////////////////////////////
// LinkableAccessible. nsIAccessible

void
LinkableAccessible::TakeFocus()
{
  if (mActionAcc)
    mActionAcc->TakeFocus();
  else
    AccessibleWrap::TakeFocus();
}

uint64_t
LinkableAccessible::NativeLinkState() const
{
  if (mIsLink)
    return states::LINKED | (mActionAcc->LinkState() & states::TRAVERSED);

  return 0;
}

void
LinkableAccessible::Value(nsString& aValue)
{
  aValue.Truncate();

  Accessible::Value(aValue);
  if (!aValue.IsEmpty())
    return;

  if (aValue.IsEmpty() && mIsLink)
    mActionAcc->Value(aValue);
}


uint8_t
LinkableAccessible::ActionCount()
{
  return (mIsOnclick || mIsLink) ? 1 : 0;
}

void
LinkableAccessible::ActionNameAt(uint8_t aIndex, nsAString& aName)
{
  aName.Truncate();

  // Action 0 (default action): Jump to link
  if (aIndex == eAction_Jump) {
    if (mIsLink)
      aName.AssignLiteral("jump");
    else if (mIsOnclick)
      aName.AssignLiteral("click");
  }
}

bool
LinkableAccessible::DoAction(uint8_t aIndex)
{
  if (aIndex != eAction_Jump)
    return false;

  return mActionAcc ? mActionAcc->DoAction(aIndex) :
    AccessibleWrap::DoAction(aIndex);
}

KeyBinding
LinkableAccessible::AccessKey() const
{
  return mActionAcc ?
    mActionAcc->AccessKey() : Accessible::AccessKey();
}

////////////////////////////////////////////////////////////////////////////////
// LinkableAccessible. Accessible

void
LinkableAccessible::Shutdown()
{
  mIsLink = false;
  mIsOnclick = false;
  mActionAcc = nullptr;
  AccessibleWrap::Shutdown();
}

////////////////////////////////////////////////////////////////////////////////
// LinkableAccessible: HyperLinkAccessible

already_AddRefed<nsIURI>
LinkableAccessible::AnchorURIAt(uint32_t aAnchorIndex)
{
  if (mIsLink) {
    NS_ASSERTION(mActionAcc->IsLink(), "HyperLink isn't implemented.");

    if (mActionAcc->IsLink())
      return mActionAcc->AnchorURIAt(aAnchorIndex);
  }

  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
// LinkableAccessible: Accessible protected

void
LinkableAccessible::BindToParent(Accessible* aParent,
                                 uint32_t aIndexInParent)
{
  AccessibleWrap::BindToParent(aParent, aIndexInParent);

  // Cache action content.
  mActionAcc = nullptr;
  mIsLink = false;
  mIsOnclick = false;

  if (nsCoreUtils::HasClickListener(mContent)) {
    mIsOnclick = true;
    return;
  }

  // XXX: The logic looks broken since the click listener may be registered
  // on non accessible node in parent chain but this node is skipped when tree
  // is traversed.
  Accessible* walkUpAcc = this;
  while ((walkUpAcc = walkUpAcc->Parent()) && !walkUpAcc->IsDoc()) {
    if (walkUpAcc->LinkState() & states::LINKED) {
      mIsLink = true;
      mActionAcc = walkUpAcc;
      return;
    }

    if (nsCoreUtils::HasClickListener(walkUpAcc->GetContent())) {
      mActionAcc = walkUpAcc;
      mIsOnclick = true;
      return;
    }
  }
}

void
LinkableAccessible::UnbindFromParent()
{
  mActionAcc = nullptr;
  mIsLink = false;
  mIsOnclick = false;

  AccessibleWrap::UnbindFromParent();
}

////////////////////////////////////////////////////////////////////////////////
// DummyAccessible
////////////////////////////////////////////////////////////////////////////////

uint64_t
DummyAccessible::NativeState()
{
  return 0;
}
uint64_t
DummyAccessible::NativeInteractiveState() const
{
  return 0;
}

uint64_t
DummyAccessible::NativeLinkState() const
{
  return 0;
}

bool
DummyAccessible::NativelyUnavailable() const
{
  return false;
}

void
DummyAccessible::ApplyARIAState(uint64_t* aState) const
{
}
