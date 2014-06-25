/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsContentCreatorFunctions.h"
#include "nsIContent.h"
#include "nsIDOMDocumentFragment.h"
#include "nsIDOMHTMLElement.h"
#include "nsIDOMHTMLMenuItemElement.h"
#include "nsXULContextMenuBuilder.h"
#include "nsIDocument.h"
#include "mozilla/dom/Element.h"

using namespace mozilla;
using namespace mozilla::dom;

nsXULContextMenuBuilder::nsXULContextMenuBuilder()
  : mCurrentGeneratedItemId(0)
{
}

nsXULContextMenuBuilder::~nsXULContextMenuBuilder()
{
}

NS_IMPL_CYCLE_COLLECTION(nsXULContextMenuBuilder, mFragment, mDocument,
                         mCurrentNode, mElements)

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsXULContextMenuBuilder)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsXULContextMenuBuilder)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsXULContextMenuBuilder)
  NS_INTERFACE_MAP_ENTRY(nsIMenuBuilder)
  NS_INTERFACE_MAP_ENTRY(nsIXULContextMenuBuilder)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIMenuBuilder)
NS_INTERFACE_MAP_END


NS_IMETHODIMP
nsXULContextMenuBuilder::OpenContainer(const nsAString& aLabel)
{
  if (!mFragment) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  if (!mCurrentNode) {
    mCurrentNode = mFragment;
  } else {
    nsCOMPtr<Element> menu;
    nsresult rv = CreateElement(nsGkAtoms::menu, nullptr, getter_AddRefs(menu));
    NS_ENSURE_SUCCESS(rv, rv);

    menu->SetAttr(kNameSpaceID_None, nsGkAtoms::label, aLabel, false);

    nsCOMPtr<Element> menuPopup;
    rv = CreateElement(nsGkAtoms::menupopup, nullptr,
                       getter_AddRefs(menuPopup));
    NS_ENSURE_SUCCESS(rv, rv);
        
    rv = menu->AppendChildTo(menuPopup, false);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = mCurrentNode->AppendChildTo(menu, false);
    NS_ENSURE_SUCCESS(rv, rv);

    mCurrentNode = menuPopup;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXULContextMenuBuilder::AddItemFor(nsIDOMHTMLMenuItemElement* aElement,
                                    bool aCanLoadIcon)
{
  if (!mFragment) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  nsCOMPtr<Element> menuitem;
  nsCOMPtr<nsIDOMHTMLElement> element = do_QueryInterface(aElement);
  nsresult rv = CreateElement(nsGkAtoms::menuitem, element,
                              getter_AddRefs(menuitem));
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoString type;
  aElement->GetType(type);
  if (type.EqualsLiteral("checkbox") || type.EqualsLiteral("radio")) {
    // The menu is only temporary, so we don't need to handle
    // the radio type precisely.
    menuitem->SetAttr(kNameSpaceID_None, nsGkAtoms::type,
                      NS_LITERAL_STRING("checkbox"), false);
    bool checked;
    aElement->GetChecked(&checked);
    if (checked) {
      menuitem->SetAttr(kNameSpaceID_None, nsGkAtoms::checked,
                        NS_LITERAL_STRING("true"), false);
    }
  }

  nsAutoString label;
  aElement->GetLabel(label);
  menuitem->SetAttr(kNameSpaceID_None, nsGkAtoms::label, label, false);

  nsAutoString icon;
  aElement->GetIcon(icon);
  if (!icon.IsEmpty()) {
    menuitem->SetAttr(kNameSpaceID_None, nsGkAtoms::_class,
                      NS_LITERAL_STRING("menuitem-iconic"), false);
    if (aCanLoadIcon) {
      menuitem->SetAttr(kNameSpaceID_None, nsGkAtoms::image, icon, false);
    }
  }

  bool disabled;
  aElement->GetDisabled(&disabled);
  if (disabled) {
    menuitem->SetAttr(kNameSpaceID_None, nsGkAtoms::disabled,
                      NS_LITERAL_STRING("true"), false);
  }

  return mCurrentNode->AppendChildTo(menuitem, false);
}

NS_IMETHODIMP
nsXULContextMenuBuilder::AddSeparator()
{
  if (!mFragment) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  nsCOMPtr<Element> menuseparator;
  nsresult rv = CreateElement(nsGkAtoms::menuseparator, nullptr,
                              getter_AddRefs(menuseparator));
  NS_ENSURE_SUCCESS(rv, rv);

  return mCurrentNode->AppendChildTo(menuseparator, false);
}

NS_IMETHODIMP
nsXULContextMenuBuilder::UndoAddSeparator()
{
  if (!mFragment) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  uint32_t count = mCurrentNode->GetChildCount();
  if (!count ||
      mCurrentNode->GetChildAt(count - 1)->Tag() != nsGkAtoms::menuseparator) {
    return NS_OK;
  }

  mCurrentNode->RemoveChildAt(count - 1, false);
  return NS_OK;
}

NS_IMETHODIMP
nsXULContextMenuBuilder::CloseContainer()
{
  if (!mFragment) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  if (mCurrentNode == mFragment) {
    mCurrentNode = nullptr;
  } else {
    nsIContent* parent = mCurrentNode->GetParent();
    mCurrentNode = parent->GetParent();
  }

  return NS_OK;
}


NS_IMETHODIMP
nsXULContextMenuBuilder::Init(nsIDOMDocumentFragment* aDocumentFragment,
                              const nsAString& aGeneratedItemIdAttrName)
{
  NS_ENSURE_ARG_POINTER(aDocumentFragment);

  mFragment = do_QueryInterface(aDocumentFragment);
  mDocument = mFragment->GetOwnerDocument();
  mGeneratedItemIdAttr = do_GetAtom(aGeneratedItemIdAttrName);

  return NS_OK;
}

NS_IMETHODIMP
nsXULContextMenuBuilder::Click(const nsAString& aGeneratedItemId)
{
  nsresult rv;
  int32_t idx = nsString(aGeneratedItemId).ToInteger(&rv);
  if (NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsIDOMHTMLElement> element = mElements.SafeObjectAt(idx);
    if (element) {
      element->DOMClick();
    }
  }

  return NS_OK;
}

nsresult
nsXULContextMenuBuilder::CreateElement(nsIAtom* aTag,
                                       nsIDOMHTMLElement* aHTMLElement,
                                       Element** aResult)
{
  *aResult = nullptr;

  nsRefPtr<mozilla::dom::NodeInfo> nodeInfo = mDocument->NodeInfoManager()->GetNodeInfo(
    aTag, nullptr, kNameSpaceID_XUL, nsIDOMNode::ELEMENT_NODE);

  nsresult rv = NS_NewElement(aResult, nodeInfo.forget(), NOT_FROM_PARSER);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsAutoString generateditemid;

  if (aHTMLElement) {
    mElements.AppendObject(aHTMLElement);
    generateditemid.AppendInt(mCurrentGeneratedItemId++);
  }

  (*aResult)->SetAttr(kNameSpaceID_None, mGeneratedItemIdAttr, generateditemid,
                      false);

  return NS_OK;
}
