/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/AsyncEventDispatcher.h"
#include "mozilla/dom/HTMLMetaElement.h"
#include "mozilla/dom/HTMLMetaElementBinding.h"
#include "nsContentUtils.h"
#include "nsStyleConsts.h"

NS_IMPL_NS_NEW_HTML_ELEMENT(Meta)

namespace mozilla {
namespace dom {

HTMLMetaElement::HTMLMetaElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo)
  : nsGenericHTMLElement(aNodeInfo)
{
}

HTMLMetaElement::~HTMLMetaElement()
{
}


NS_IMPL_ISUPPORTS_INHERITED(HTMLMetaElement, nsGenericHTMLElement,
                            nsIDOMHTMLMetaElement)

NS_IMPL_ELEMENT_CLONE(HTMLMetaElement)


NS_IMPL_STRING_ATTR(HTMLMetaElement, Content, content)
NS_IMPL_STRING_ATTR(HTMLMetaElement, HttpEquiv, httpEquiv)
NS_IMPL_STRING_ATTR(HTMLMetaElement, Name, name)
NS_IMPL_STRING_ATTR(HTMLMetaElement, Scheme, scheme)

void
HTMLMetaElement::GetItemValueText(nsAString& aValue)
{
  GetContent(aValue);
}

void
HTMLMetaElement::SetItemValueText(const nsAString& aValue)
{
  SetContent(aValue);
}


nsresult
HTMLMetaElement::BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                            nsIContent* aBindingParent,
                            bool aCompileEventHandlers)
{
  nsresult rv = nsGenericHTMLElement::BindToTree(aDocument, aParent,
                                                 aBindingParent,
                                                 aCompileEventHandlers);
  NS_ENSURE_SUCCESS(rv, rv);
  if (aDocument &&
      AttrValueIs(kNameSpaceID_None, nsGkAtoms::name, nsGkAtoms::viewport, eIgnoreCase)) {
    nsAutoString content;
    rv = GetContent(content);
    NS_ENSURE_SUCCESS(rv, rv);
    nsContentUtils::ProcessViewportInfo(aDocument, content);  
  }
  CreateAndDispatchEvent(aDocument, NS_LITERAL_STRING("DOMMetaAdded"));
  return rv;
}

void
HTMLMetaElement::UnbindFromTree(bool aDeep, bool aNullParent)
{
  nsCOMPtr<nsIDocument> oldDoc = GetCurrentDoc();
  CreateAndDispatchEvent(oldDoc, NS_LITERAL_STRING("DOMMetaRemoved"));
  nsGenericHTMLElement::UnbindFromTree(aDeep, aNullParent);
}

void
HTMLMetaElement::CreateAndDispatchEvent(nsIDocument* aDoc,
                                        const nsAString& aEventName)
{
  if (!aDoc)
    return;

  nsRefPtr<AsyncEventDispatcher> asyncDispatcher =
    new AsyncEventDispatcher(this, aEventName, true, true);
  asyncDispatcher->PostDOMEvent();
}

JSObject*
HTMLMetaElement::WrapNode(JSContext* aCx)
{
  return HTMLMetaElementBinding::Wrap(aCx, this);
}

} // namespace dom
} // namespace mozilla
