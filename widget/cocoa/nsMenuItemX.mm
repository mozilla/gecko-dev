/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsMenuItemX.h"
#include "nsMenuBarX.h"
#include "nsMenuX.h"
#include "nsMenuItemIconX.h"
#include "nsMenuUtilsX.h"
#include "nsCocoaUtils.h"

#include "nsObjCExceptions.h"

#include "nsCOMPtr.h"
#include "nsGkAtoms.h"

#include "mozilla/dom/Element.h"
#include "mozilla/dom/Event.h"
#include "mozilla/ErrorResult.h"
#include "nsIWidget.h"
#include "nsIDocument.h"

using namespace mozilla;

using mozilla::dom::Event;
using mozilla::dom::CallerType;

nsMenuItemX::nsMenuItemX()
{
  mType           = eRegularMenuItemType;
  mNativeMenuItem = nil;
  mMenuParent     = nullptr;
  mMenuGroupOwner = nullptr;
  mIsChecked      = false;

  MOZ_COUNT_CTOR(nsMenuItemX);
}

nsMenuItemX::~nsMenuItemX()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  // Prevent the icon object from outliving us.
  if (mIcon)
    mIcon->Destroy();

  // autorelease the native menu item so that anything else happening to this
  // object happens before the native menu item actually dies
  [mNativeMenuItem autorelease];

  if (mContent)
    mMenuGroupOwner->UnregisterForContentChanges(mContent);
  if (mCommandElement)
    mMenuGroupOwner->UnregisterForContentChanges(mCommandElement);

  MOZ_COUNT_DTOR(nsMenuItemX);

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

nsresult nsMenuItemX::Create(nsMenuX* aParent, const nsString& aLabel, EMenuItemType aItemType,
                             nsMenuGroupOwnerX* aMenuGroupOwner, nsIContent* aNode)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  mType = aItemType;
  mMenuParent = aParent;
  mContent = aNode;

  mMenuGroupOwner = aMenuGroupOwner;
  NS_ASSERTION(mMenuGroupOwner, "No menu owner given, must have one!");

  mMenuGroupOwner->RegisterForContentChanges(mContent, this);

  nsIDocument *doc = mContent->GetUncomposedDoc();

  // if we have a command associated with this menu item, register for changes
  // to the command DOM node
  if (doc) {
    nsAutoString ourCommand;
    if (mContent->IsElement()) {
      mContent->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::command, ourCommand);
    }

    if (!ourCommand.IsEmpty()) {
      Element* commandElement = doc->GetElementById(ourCommand);

      if (commandElement) {
        mCommandElement = commandElement;
        // register to observe the command DOM element
        mMenuGroupOwner->RegisterForContentChanges(mCommandElement, this);
      }
    }
  }

  // decide enabled state based on command content if it exists, otherwise do it based
  // on our own content
  bool isEnabled;
  if (mCommandElement)
    isEnabled = !mCommandElement->AttrValueIs(kNameSpaceID_None, nsGkAtoms::disabled, nsGkAtoms::_true, eCaseMatters);
  else
    isEnabled = !mContent->IsElement() ||
      !mContent->AsElement()->AttrValueIs(kNameSpaceID_None, nsGkAtoms::disabled, nsGkAtoms::_true, eCaseMatters);

  // set up the native menu item
  if (mType == eSeparatorMenuItemType) {
    mNativeMenuItem = [[NSMenuItem separatorItem] retain];
  }
  else {
    NSString *newCocoaLabelString = nsMenuUtilsX::GetTruncatedCocoaLabel(aLabel);
    mNativeMenuItem = [[NSMenuItem alloc] initWithTitle:newCocoaLabelString action:nil keyEquivalent:@""];

    [mNativeMenuItem setEnabled:(BOOL)isEnabled];

    SetChecked(mContent->IsElement() &&
               mContent->AsElement()->AttrValueIs(kNameSpaceID_None, nsGkAtoms::checked,
                                                  nsGkAtoms::_true, eCaseMatters));
    SetKeyEquiv();
  }

  mIcon = new nsMenuItemIconX(this, mContent, mNativeMenuItem);
  if (!mIcon)
    return NS_ERROR_OUT_OF_MEMORY;

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

nsresult nsMenuItemX::SetChecked(bool aIsChecked)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  mIsChecked = aIsChecked;

  // update the content model. This will also handle unchecking our siblings
  // if we are a radiomenu
  mContent->AsElement()->SetAttr(
    kNameSpaceID_None, nsGkAtoms::checked,
    mIsChecked ? NS_LITERAL_STRING("true") : NS_LITERAL_STRING("false"), true);

  // update native menu item
  if (mIsChecked)
    [mNativeMenuItem setState:NSOnState];
  else
    [mNativeMenuItem setState:NSOffState];

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

EMenuItemType nsMenuItemX::GetMenuItemType()
{
  return mType;
}

// Executes the "cached" javaScript command.
// Returns NS_OK if the command was executed properly, otherwise an error code.
void nsMenuItemX::DoCommand()
{
  // flip "checked" state if we're a checkbox menu, or an un-checked radio menu
  if (mType == eCheckboxMenuItemType ||
      (mType == eRadioMenuItemType && !mIsChecked)) {
    if (!mContent->IsElement() ||
        !mContent->AsElement()->AttrValueIs(
          kNameSpaceID_None, nsGkAtoms::autocheck, nsGkAtoms::_false, eCaseMatters))
    SetChecked(!mIsChecked);
    /* the AttributeChanged code will update all the internal state */
  }

  nsMenuUtilsX::DispatchCommandTo(mContent);
}

nsresult nsMenuItemX::DispatchDOMEvent(const nsString &eventName, bool *preventDefaultCalled)
{
  if (!mContent)
    return NS_ERROR_FAILURE;

  // get owner document for content
  nsCOMPtr<nsIDocument> parentDoc = mContent->OwnerDoc();

  // create DOM event
  ErrorResult rv;
  RefPtr<Event> event = parentDoc->CreateEvent(NS_LITERAL_STRING("Events"),
                                               CallerType::System,
                                               rv);
  if (rv.Failed()) {
    NS_WARNING("Failed to create Event");
    return rv.StealNSResult();
  }
  event->InitEvent(eventName, true, true);

  // mark DOM event as trusted
  event->SetTrusted(true);

  // send DOM event
  *preventDefaultCalled =
    mContent->DispatchEvent(*event, CallerType::System, rv);
  if (rv.Failed()) {
    NS_WARNING("Failed to send DOM event via EventTarget");
    return rv.StealNSResult();
  }

  return NS_OK;
}

// Walk the sibling list looking for nodes with the same name and
// uncheck them all.
void nsMenuItemX::UncheckRadioSiblings(nsIContent* inCheckedContent)
{
  nsAutoString myGroupName;
  if (inCheckedContent->IsElement()) {
    inCheckedContent->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::name, myGroupName);
  }
  if (!myGroupName.Length()) // no groupname, nothing to do
    return;

  nsCOMPtr<nsIContent> parent = inCheckedContent->GetParent();
  if (!parent)
    return;

  // loop over siblings
  for (nsIContent* sibling = parent->GetFirstChild();
       sibling; sibling = sibling->GetNextSibling()) {
    if (sibling != inCheckedContent &&
        sibling->IsElement()) { // skip this node
      // if the current sibling is in the same group, clear it
      if (sibling->AsElement()->AttrValueIs(kNameSpaceID_None, nsGkAtoms::name,
                                            myGroupName, eCaseMatters)) {
        sibling->AsElement()->SetAttr(kNameSpaceID_None, nsGkAtoms::checked, NS_LITERAL_STRING("false"), true);
      }
    }
  }
}

void nsMenuItemX::SetKeyEquiv()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  // Set key shortcut and modifiers
  nsAutoString keyValue;
  if (mContent->IsElement()) {
    mContent->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::key, keyValue);
  }

  if (!keyValue.IsEmpty() && mContent->GetUncomposedDoc()) {
    Element* keyContent = mContent->GetUncomposedDoc()->GetElementById(keyValue);
    if (keyContent) {
      nsAutoString keyChar;
      bool hasKey = keyContent->GetAttr(kNameSpaceID_None, nsGkAtoms::key, keyChar);

      if (!hasKey || keyChar.IsEmpty()) {
        nsAutoString keyCodeName;
        keyContent->GetAttr(kNameSpaceID_None, nsGkAtoms::keycode, keyCodeName);
        uint32_t charCode =
          nsCocoaUtils::ConvertGeckoNameToMacCharCode(keyCodeName);
        if (charCode) {
          keyChar.Assign(charCode);
        }
        else {
          keyChar.AssignLiteral(u" ");
        }
      }

      nsAutoString modifiersStr;
      keyContent->GetAttr(kNameSpaceID_None, nsGkAtoms::modifiers, modifiersStr);
      uint8_t modifiers = nsMenuUtilsX::GeckoModifiersForNodeAttribute(modifiersStr);

      unsigned int macModifiers = nsMenuUtilsX::MacModifiersForGeckoModifiers(modifiers);
      [mNativeMenuItem setKeyEquivalentModifierMask:macModifiers];

      NSString *keyEquivalent = [[NSString stringWithCharacters:(unichar*)keyChar.get()
                                                         length:keyChar.Length()] lowercaseString];
      if ([keyEquivalent isEqualToString:@" "])
        [mNativeMenuItem setKeyEquivalent:@""];
      else
        [mNativeMenuItem setKeyEquivalent:keyEquivalent];

      return;
    }
  }

  // if the key was removed, clear the key
  [mNativeMenuItem setKeyEquivalent:@""];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

//
// nsChangeObserver
//

void
nsMenuItemX::ObserveAttributeChanged(nsIDocument *aDocument, nsIContent *aContent, nsAtom *aAttribute)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (!aContent)
    return;

  if (aContent == mContent) { // our own content node changed
    if (aAttribute == nsGkAtoms::checked) {
      // if we're a radio menu, uncheck our sibling radio items. No need to
      // do any of this if we're just a normal check menu.
      if (mType == eRadioMenuItemType &&
          mContent->IsElement() &&
          mContent->AsElement()->AttrValueIs(
            kNameSpaceID_None, nsGkAtoms::checked, nsGkAtoms::_true, eCaseMatters)) {
        UncheckRadioSiblings(mContent);
      }
      mMenuParent->SetRebuild(true);
    }
    else if (aAttribute == nsGkAtoms::hidden ||
             aAttribute == nsGkAtoms::collapsed ||
             aAttribute == nsGkAtoms::label) {
      mMenuParent->SetRebuild(true);
    }
    else if (aAttribute == nsGkAtoms::key) {
      SetKeyEquiv();
    }
    else if (aAttribute == nsGkAtoms::image) {
      SetupIcon();
    }
    else if (aAttribute == nsGkAtoms::disabled) {
      if (aContent->AsElement()->AttrValueIs(kNameSpaceID_None, nsGkAtoms::disabled, nsGkAtoms::_true, eCaseMatters))
        [mNativeMenuItem setEnabled:NO];
      else
        [mNativeMenuItem setEnabled:YES];
    }
  }
  else if (aContent == mCommandElement) {
    // the only thing that really matters when the menu isn't showing is the
    // enabled state since it enables/disables keyboard commands
    if (aAttribute == nsGkAtoms::disabled) {
      // first we sync our menu item DOM node with the command DOM node
      nsAutoString commandDisabled;
      nsAutoString menuDisabled;
      aContent->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::disabled, commandDisabled);
      mContent->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::disabled, menuDisabled);
      if (!commandDisabled.Equals(menuDisabled)) {
        // The menu's disabled state needs to be updated to match the command.
        if (commandDisabled.IsEmpty())
          mContent->AsElement()->UnsetAttr(kNameSpaceID_None, nsGkAtoms::disabled, true);
        else
          mContent->AsElement()->SetAttr(kNameSpaceID_None, nsGkAtoms::disabled, commandDisabled, true);
      }
      // now we sync our native menu item with the command DOM node
      if (aContent->AsElement()->AttrValueIs(kNameSpaceID_None, nsGkAtoms::disabled, nsGkAtoms::_true, eCaseMatters))
        [mNativeMenuItem setEnabled:NO];
      else
        [mNativeMenuItem setEnabled:YES];
    }
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

void nsMenuItemX::ObserveContentRemoved(nsIDocument* aDocument,
                                        nsIContent* aContainer,
                                        nsIContent* aChild,
                                        nsIContent* aPreviousSibling)
{
  if (aChild == mCommandElement) {
    mMenuGroupOwner->UnregisterForContentChanges(mCommandElement);
    mCommandElement = nullptr;
  }

  mMenuParent->SetRebuild(true);
}

void nsMenuItemX::ObserveContentInserted(nsIDocument *aDocument, nsIContent* aContainer,
                                         nsIContent *aChild)
{
  mMenuParent->SetRebuild(true);
}

void nsMenuItemX::SetupIcon()
{
  if (mIcon)
    mIcon->SetupIcon();
}
