/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGDiscardElement.h"
#include "mozilla/dom/SVGDiscardElementBinding.h"
#include "mozilla/StaticPrefs_svg.h"

NS_IMPL_NS_NEW_SVG_ELEMENT(Discard)

namespace mozilla::dom {

JSObject* SVGDiscardElement::WrapNode(JSContext* aCx,
                                      JS::Handle<JSObject*> aGivenProto) {
  return SVGDiscardElement_Binding::Wrap(aCx, this, aGivenProto);
}

//----------------------------------------------------------------------
// Implementation

SVGDiscardElement::SVGDiscardElement(
    already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo)
    : SVGAnimationElement(std::move(aNodeInfo)) {}

//----------------------------------------------------------------------
// nsINode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(SVGDiscardElement)

//----------------------------------------------------------------------

SMILAnimationFunction& SVGDiscardElement::AnimationFunction() {
  return mAnimationFunction;
}

bool SVGDiscardElement::GetTargetAttributeName(int32_t* aNamespaceID,
                                               nsAtom** aLocalName) const {
  // <discard> doesn't take an attributeName, since it doesn't target an
  // 'attribute' per se.  We'll use a dummy attribute-name so that our
  // SMILTargetIdentifier logic (which requires an attribute name) still works.
  *aNamespaceID = kNameSpaceID_None;
  *aLocalName = nsGkAtoms::_undefined;
  return true;
}

void SVGDiscardElement::AddDiscards(
    nsTObserverArray<RefPtr<Element>>& aDiscards) {
  if (!StaticPrefs::svg_discard_enabled()) {
    return;
  }
  if (RefPtr<Element> target = GetTargetElementContent()) {
    aDiscards.AppendElementUnlessExists(target);
  }
  aDiscards.AppendElementUnlessExists(this);
}

}  // namespace mozilla::dom
