/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/HTMLHeadingElement.h"
#include "mozilla/dom/HTMLHeadingElementBinding.h"

#include "mozilla/MappedDeclarationsBuilder.h"
#include "nsGkAtoms.h"

NS_IMPL_NS_NEW_HTML_ELEMENT(Heading)

namespace mozilla::dom {

HTMLHeadingElement::~HTMLHeadingElement() = default;

NS_IMPL_ELEMENT_CLONE(HTMLHeadingElement)

JSObject* HTMLHeadingElement::WrapNode(JSContext* aCx,
                                       JS::Handle<JSObject*> aGivenProto) {
  return HTMLHeadingElement_Binding::Wrap(aCx, this, aGivenProto);
}

bool HTMLHeadingElement::ParseAttribute(int32_t aNamespaceID,
                                        nsAtom* aAttribute,
                                        const nsAString& aValue,
                                        nsIPrincipal* aMaybeScriptedPrincipal,
                                        nsAttrValue& aResult) {
  if (aAttribute == nsGkAtoms::align && aNamespaceID == kNameSpaceID_None) {
    return ParseDivAlignValue(aValue, aResult);
  }

  return nsGenericHTMLElement::ParseAttribute(aNamespaceID, aAttribute, aValue,
                                              aMaybeScriptedPrincipal, aResult);
}

void HTMLHeadingElement::UpdateLevel(bool aNotify) {
  AutoStateChangeNotifier notifier(*this, aNotify);
  RemoveStatesSilently(ElementState::HEADING_LEVEL_BITS);
  uint64_t level = ComputedLevel();

  // ElementState has 4 bits for the heading level, but they are not the LMB,
  // so we need to shift the given level up to those bits.
  MOZ_ASSERT(level > 0 && level < 16, "ComputedLevel() must fit into 4 bits!");
  uint64_t bits = (level << HEADING_LEVEL_OFFSET);
  MOZ_ASSERT((bits & ElementState::HEADING_LEVEL_BITS.bits) == bits);

  AddStatesSilently(ElementState(bits));
}

void HTMLHeadingElement::MapAttributesIntoRule(
    MappedDeclarationsBuilder& aBuilder) {
  nsGenericHTMLElement::MapDivAlignAttributeInto(aBuilder);
  nsGenericHTMLElement::MapCommonAttributesInto(aBuilder);
}

NS_IMETHODIMP_(bool)
HTMLHeadingElement::IsAttributeMapped(const nsAtom* aAttribute) const {
  static const MappedAttributeEntry* const map[] = {sDivAlignAttributeMap,
                                                    sCommonAttributeMap};

  return FindAttributeDependence(aAttribute, map);
}

nsMapRuleToAttributesFunc HTMLHeadingElement::GetAttributeMappingFunction()
    const {
  return &MapAttributesIntoRule;
}

}  // namespace mozilla::dom
