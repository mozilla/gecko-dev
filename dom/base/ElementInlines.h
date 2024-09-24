/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ElementInlines_h
#define mozilla_dom_ElementInlines_h

#include "mozilla/ServoBindingTypes.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/Element.h"
#include "nsGenericHTMLElement.h"
#include "nsIContentInlines.h"

namespace mozilla::dom {

inline void Element::RegisterActivityObserver() {
  OwnerDoc()->RegisterActivityObserver(this);
}

inline void Element::UnregisterActivityObserver() {
  OwnerDoc()->UnregisterActivityObserver(this);
}

inline bool Element::IsContentEditablePlainTextOnly() const {
  const auto* const htmlElement = nsGenericHTMLElement::FromNode(this);
  return htmlElement &&
         htmlElement->GetContentEditableState() ==
             nsGenericHTMLElement::ContentEditableState::PlainTextOnly;
}

}  // namespace mozilla::dom

inline mozilla::dom::Element* nsINode::GetFlattenedTreeParentElement() const {
  nsINode* parentNode = GetFlattenedTreeParentNode();
  if MOZ_LIKELY (parentNode && parentNode->IsElement()) {
    return parentNode->AsElement();
  }

  return nullptr;
}

inline mozilla::dom::Element* nsINode::GetFlattenedTreeParentElementForStyle()
    const {
  nsINode* parentNode = GetFlattenedTreeParentNodeForStyle();
  if (MOZ_LIKELY(parentNode && parentNode->IsElement())) {
    return parentNode->AsElement();
  }
  return nullptr;
}

inline mozilla::dom::Element*
nsINode::GetInclusiveFlattenedTreeAncestorElement() const {
  nsIContent* content = const_cast<nsIContent*>(nsIContent::FromNode(this));
  while (content && !content->IsElement()) {
    content = content->GetFlattenedTreeParent();
  }
  return mozilla::dom::Element::FromNodeOrNull(content);
}

#endif  // mozilla_dom_ElementInlines_h
