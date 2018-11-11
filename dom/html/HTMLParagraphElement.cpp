/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/HTMLParagraphElement.h"
#include "mozilla/dom/HTMLParagraphElementBinding.h"

#include "nsStyleConsts.h"
#include "nsMappedAttributes.h"
#include "nsRuleData.h"


NS_IMPL_NS_NEW_HTML_ELEMENT(Paragraph)

namespace mozilla {
namespace dom {

HTMLParagraphElement::~HTMLParagraphElement()
{
}

NS_IMPL_ISUPPORTS_INHERITED(HTMLParagraphElement, nsGenericHTMLElement,
                            nsIDOMHTMLParagraphElement)

NS_IMPL_ELEMENT_CLONE(HTMLParagraphElement)

NS_IMPL_STRING_ATTR(HTMLParagraphElement, Align, align)

bool
HTMLParagraphElement::ParseAttribute(int32_t aNamespaceID,
                                     nsIAtom* aAttribute,
                                     const nsAString& aValue,
                                     nsAttrValue& aResult)
{
  if (aAttribute == nsGkAtoms::align && aNamespaceID == kNameSpaceID_None) {
    return ParseDivAlignValue(aValue, aResult);
  }

  return nsGenericHTMLElement::ParseAttribute(aNamespaceID, aAttribute, aValue,
                                              aResult);
}

void
HTMLParagraphElement::MapAttributesIntoRule(const nsMappedAttributes* aAttributes,
                                            nsRuleData* aData)
{
  nsGenericHTMLElement::MapDivAlignAttributeInto(aAttributes, aData);
  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aData);
}

NS_IMETHODIMP_(bool)
HTMLParagraphElement::IsAttributeMapped(const nsIAtom* aAttribute) const
{
  static const MappedAttributeEntry* const map[] = {
    sDivAlignAttributeMap,
    sCommonAttributeMap,
  };

  return FindAttributeDependence(aAttribute, map);
}


nsMapRuleToAttributesFunc
HTMLParagraphElement::GetAttributeMappingFunction() const
{
  return &MapAttributesIntoRule;
}

JSObject*
HTMLParagraphElement::WrapNode(JSContext *aCx, JS::Handle<JSObject*> aGivenProto)
{
  return HTMLParagraphElementBinding::Wrap(aCx, this, aGivenProto);
}

} // namespace dom
} // namespace mozilla
