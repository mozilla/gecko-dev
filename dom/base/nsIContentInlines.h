/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsIContentInlines_h
#define nsIContentInlines_h

#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsBindingManager.h"
#include "nsContentUtils.h"
#include "nsAtom.h"
#include "nsIFrame.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/HTMLSlotElement.h"
#include "mozilla/dom/ShadowRoot.h"

inline bool nsIContent::IsInHTMLDocument() const {
  return OwnerDoc()->IsHTMLDocument();
}

inline bool nsIContent::IsInChromeDocument() const {
  return nsContentUtils::IsChromeDoc(OwnerDoc());
}

inline void nsIContent::SetPrimaryFrame(nsIFrame* aFrame) {
  MOZ_ASSERT(IsInUncomposedDoc() || IsInShadowTree(), "This will end badly!");

  // FIXME bug 749326
  NS_ASSERTION(!aFrame || !mPrimaryFrame || aFrame == mPrimaryFrame,
               "Losing track of existing primary frame");

  if (aFrame) {
    if (MOZ_LIKELY(!IsHTMLElement(nsGkAtoms::area)) ||
        aFrame->GetContent() == this) {
      aFrame->SetIsPrimaryFrame(true);
    }
  } else if (nsIFrame* currentPrimaryFrame = GetPrimaryFrame()) {
    if (MOZ_LIKELY(!IsHTMLElement(nsGkAtoms::area)) ||
        currentPrimaryFrame->GetContent() == this) {
      currentPrimaryFrame->SetIsPrimaryFrame(false);
    }
  }

  mPrimaryFrame = aFrame;
}

inline mozilla::dom::ShadowRoot* nsIContent::GetShadowRoot() const {
  if (!IsElement()) {
    return nullptr;
  }

  return AsElement()->GetShadowRoot();
}

template <nsINode::FlattenedParentType aType>
static inline nsINode* GetFlattenedTreeParentNode(const nsINode* aNode) {
  if (!aNode->IsContent()) {
    return nullptr;
  }

  nsINode* parent = aNode->GetParentNode();
  if (!parent || !parent->IsContent()) {
    return parent;
  }

  const nsIContent* content = aNode->AsContent();
  nsIContent* parentAsContent = parent->AsContent();

  if (aType == nsINode::eForStyle &&
      content->IsRootOfNativeAnonymousSubtree() &&
      parentAsContent == content->OwnerDoc()->GetRootElement()) {
    const bool docLevel =
        content->GetProperty(nsGkAtoms::docLevelNativeAnonymousContent);
    return docLevel ? content->OwnerDocAsNode() : parent;
  }

  if (content->IsRootOfAnonymousSubtree()) {
    return parent;
  }

  if (parentAsContent->GetShadowRoot()) {
    // If it's not assigned to any slot it's not part of the flat tree, and thus
    // we return null.
    return content->GetAssignedSlot();
  }

  if (parentAsContent->IsInShadowTree()) {
    if (auto* slot = mozilla::dom::HTMLSlotElement::FromNode(parentAsContent)) {
      // If the assigned nodes list is empty, we're fallback content which is
      // active, otherwise we are not part of the flat tree.
      return slot->AssignedNodes().IsEmpty() ? parent : nullptr;
    }

    if (auto* shadowRoot =
            mozilla::dom::ShadowRoot::FromNode(parentAsContent)) {
      return shadowRoot->GetHost();
    }
  }

  if (content->HasFlag(NODE_MAY_BE_IN_BINDING_MNGR) ||
      parent->HasFlag(NODE_MAY_BE_IN_BINDING_MNGR)) {
    if (nsIContent* xblInsertionPoint = content->GetXBLInsertionPoint()) {
      return xblInsertionPoint->GetParent();
    }

    if (parent->OwnerDoc()->BindingManager()->GetBindingWithContent(
            parentAsContent)) {
      // This is an unassigned node child of the bound element, so it isn't part
      // of the flat tree.
      return nullptr;
    }
  }

  MOZ_ASSERT(!parentAsContent->IsActiveChildrenElement(),
             "<xbl:children> isn't in the flattened tree");

  // Common case.
  return parent;
}

inline nsINode* nsINode::GetFlattenedTreeParentNode() const {
  return ::GetFlattenedTreeParentNode<nsINode::eNotForStyle>(this);
}

inline nsIContent* nsIContent::GetFlattenedTreeParent() const {
  nsINode* parent = GetFlattenedTreeParentNode();
  return (parent && parent->IsContent()) ? parent->AsContent() : nullptr;
}

inline bool nsIContent::IsEventAttributeName(nsAtom* aName) {
  const char16_t* name = aName->GetUTF16String();
  if (name[0] != 'o' || name[1] != 'n') {
    return false;
  }

  return IsEventAttributeNameInternal(aName);
}

inline nsINode* nsINode::GetFlattenedTreeParentNodeForStyle() const {
  return ::GetFlattenedTreeParentNode<nsINode::eForStyle>(this);
}

inline bool nsINode::NodeOrAncestorHasDirAuto() const {
  return AncestorHasDirAuto() || (IsElement() && AsElement()->HasDirAuto());
}

inline bool nsINode::IsEditable() const {
  if (HasFlag(NODE_IS_EDITABLE)) {
    // The node is in an editable contentEditable subtree.
    return true;
  }

  nsIDocument* doc = GetUncomposedDoc();

  // Check if the node is in a document and the document is in designMode.
  return doc && doc->HasFlag(NODE_IS_EDITABLE);
}

inline bool nsIContent::IsActiveChildrenElement() const {
  if (!mNodeInfo->Equals(nsGkAtoms::children, kNameSpaceID_XBL)) {
    return false;
  }

  nsIContent* bindingParent = GetBindingParent();
  if (!bindingParent) {
    return false;
  }

  // We reuse the binding parent machinery for Shadow DOM too, so prevent that
  // from getting us confused in this case.
  return !bindingParent->GetShadowRoot();
}

inline bool nsIContent::IsInAnonymousSubtree() const {
  NS_ASSERTION(
      !IsInNativeAnonymousSubtree() || GetBindingParent() ||
          (!IsInUncomposedDoc() && static_cast<nsIContent*>(SubtreeRoot())
                                       ->IsInNativeAnonymousSubtree()),
      "Must have binding parent when in native anonymous subtree which is in "
      "document.\n"
      "Native anonymous subtree which is not in document must have native "
      "anonymous root.");

  if (IsInNativeAnonymousSubtree()) {
    return true;
  }

  nsIContent* bindingParent = GetBindingParent();
  if (!bindingParent) {
    return false;
  }

  // We reuse the binding parent machinery for Shadow DOM too, so prevent that
  // from getting us confused in this case.
  return !bindingParent->GetShadowRoot();
}

#endif  // nsIContentInlines_h
