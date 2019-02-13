/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/HTMLHeadingElement.h"
#include "mozilla/dom/HTMLHeadingElementBinding.h"

#include "nsGkAtoms.h"
#include "nsStyleConsts.h"
#include "nsMappedAttributes.h"
#include "nsRuleData.h"
#include "mozAutoDocUpdate.h"

NS_IMPL_NS_NEW_HTML_ELEMENT(Heading)

namespace mozilla {
namespace dom {

HTMLHeadingElement::~HTMLHeadingElement()
{
}

NS_IMPL_ELEMENT_CLONE(HTMLHeadingElement)

JSObject*
HTMLHeadingElement::WrapNode(JSContext *aCx, JS::Handle<JSObject*> aGivenProto)
{
  return HTMLHeadingElementBinding::Wrap(aCx, this, aGivenProto);
}

bool
HTMLHeadingElement::ParseAttribute(int32_t aNamespaceID,
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
HTMLHeadingElement::MapAttributesIntoRule(const nsMappedAttributes* aAttributes,
                                          nsRuleData* aData)
{
  nsGenericHTMLElement::MapDivAlignAttributeInto(aAttributes, aData);
  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aData);
}

NS_IMETHODIMP_(bool)
HTMLHeadingElement::IsAttributeMapped(const nsIAtom* aAttribute) const
{
  static const MappedAttributeEntry* const map[] = {
    sDivAlignAttributeMap,
    sCommonAttributeMap
  };

  return FindAttributeDependence(aAttribute, map);
}


nsMapRuleToAttributesFunc
HTMLHeadingElement::GetAttributeMappingFunction() const
{
  return &MapAttributesIntoRule;
}

} // namespace dom
} // namespace mozilla
