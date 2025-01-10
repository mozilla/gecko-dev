/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_SVG_SVGDISCARDELEMENT_H_
#define DOM_SVG_SVGDISCARDELEMENT_H_

#include "mozilla/Attributes.h"
#include "mozilla/dom/SVGAnimationElement.h"
#include "mozilla/SMILDiscardAnimationFunction.h"

nsresult NS_NewSVGDiscardElement(
    nsIContent** aResult, already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

namespace mozilla::dom {

class SVGDiscardElement final : public SVGAnimationElement {
 protected:
  explicit SVGDiscardElement(
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

  SMILDiscardAnimationFunction mAnimationFunction;

  void DoDiscard();

  friend nsresult(::NS_NewSVGDiscardElement(
      nsIContent** aResult,
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo));

  JSObject* WrapNode(JSContext* aCx,
                     JS::Handle<JSObject*> aGivenProto) override;

 public:
  // nsINode
  nsresult Clone(dom::NodeInfo*, nsINode** aResult) const override;

  // SVGAnimationElement
  SMILAnimationFunction& AnimationFunction() override;
  bool GetTargetAttributeName(int32_t* aNamespaceID,
                              nsAtom** aLocalName) const override;
  bool SupportsXLinkHref() const override { return false; }

  void AddDiscards(nsTObserverArray<RefPtr<Element>>& aDiscards) override;
};

}  // namespace mozilla::dom

#endif  // DOM_SVG_SVGDISCARDELEMENT_H_
