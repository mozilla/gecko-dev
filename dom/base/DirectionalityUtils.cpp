/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
  This file contains most of the code to implement html directionality.
  This includes default direction, inheritance, and auto directionality.

  A useful perspective is separating the static and dynamic case.
  In the static case, directionality is computed based on the current DOM,
  closely following the specification, e.g. in ComputeAutoDirectionality.
  Dynamic changes, e.g. OnSetDirAttr, are propagated to the impacted nodes,
  for which the static case is re-run.

  To minimize searching for dir=auto nodes impacted by a dynamic change, several
  flags are maintained (see their declaration for documentation):
  - NodeAncestorHasDirAuto and NodeAffectsDirAutoSlot apply to all nodes.
    They are set when a node is placed somewhere in the tree and set or cleared
    when a dir attribute changes.
  - NS_MAY_SET_DIR_AUTO applies to text. It is set whenever a text node might be
    responsible for the auto directionality of a dir=auto element. It is cleared
    when the element is unbound.
*/

#include "mozilla/dom/DirectionalityUtils.h"

#include "nsINode.h"
#include "nsIContent.h"
#include "nsIContentInlines.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/HTMLInputElement.h"
#include "mozilla/dom/HTMLSlotElement.h"
#include "mozilla/dom/HTMLTextAreaElement.h"
#include "mozilla/dom/ShadowRoot.h"
#include "mozilla/dom/Text.h"
#include "mozilla/dom/UnbindContext.h"
#include "mozilla/intl/UnicodeProperties.h"
#include "mozilla/Maybe.h"
#include "nsUnicodeProperties.h"
#include "nsTextFragment.h"
#include "nsAttrValue.h"

namespace mozilla {

using mozilla::dom::Element;
using mozilla::dom::HTMLInputElement;
using mozilla::dom::HTMLSlotElement;
using mozilla::dom::ShadowRoot;
using mozilla::dom::Text;

/**
 * Returns true if aElement is one of the elements whose text content should
 * affect its own direction, or the direction of ancestors with dir=auto.
 *
 * Note that this does not include <bdi>, whose content does affect its own
 * direction when it has dir=auto (which it has by default), so one needs to
 * test for it separately, e.g. with EstablishesOwnDirection.
 * It *does* include textarea, because even if a textarea has dir=auto, it has
 * unicode-bidi: plaintext and is handled automatically in bidi resolution.
 * It also includes `input`, because it takes the `dir` value from its value
 * attribute, instead of the child nodes.
 */
static bool ParticipatesInAutoDirection(const nsIContent* aContent) {
  if (aContent->IsInNativeAnonymousSubtree()) {
    return false;
  }
  if (aContent->IsShadowRoot()) {
    return true;
  }
  return !aContent->IsAnyOfHTMLElements(nsGkAtoms::script, nsGkAtoms::style,
                                        nsGkAtoms::input, nsGkAtoms::textarea);
}

static bool IsAutoDirectionalityFormAssociatedElement(Element* aElement) {
  if (HTMLInputElement* input = HTMLInputElement::FromNode(aElement)) {
    return input->IsAutoDirectionalityAssociated();
  }
  return aElement->IsHTMLElement(nsGkAtoms::textarea);
}

static Maybe<nsAutoString> GetValueIfFormAssociatedElement(Element* aElement) {
  Maybe<nsAutoString> result;
  if (HTMLInputElement* input = HTMLInputElement::FromNode(aElement)) {
    if (input->IsAutoDirectionalityAssociated()) {
      // It's unclear if per spec we should use the sanitized or unsanitized
      // value to set the directionality. But input may provide a known value
      // to us, which is unsanitized, so be consistent. Using what the user is
      // seeing to determine directionality instead of the sanitized
      // (empty if invalid) value probably makes more sense.
      result.emplace();
      input->GetValueInternal(*result, dom::CallerType::System);
    }
  } else if (dom::HTMLTextAreaElement* ta =
                 dom::HTMLTextAreaElement::FromNode(aElement)) {
    result.emplace();
    ta->GetValue(*result);
  }
  return result;
}

/**
 * Returns the directionality of a Unicode character
 */
static Directionality GetDirectionFromChar(uint32_t ch) {
  switch (intl::UnicodeProperties::GetBidiClass(ch)) {
    case intl::BidiClass::RightToLeft:
    case intl::BidiClass::RightToLeftArabic:
      return Directionality::Rtl;

    case intl::BidiClass::LeftToRight:
      return Directionality::Ltr;

    default:
      return Directionality::Unset;
  }
}

/**
 * Returns true if aElement establishes its own direction or does not have one.
 *
 * From https://html.spec.whatwg.org/#auto-directionality step 3.1., this is
 * bdi, script, style, textarea, and elements with auto, ltr or rtl dir.
 * Additionally, it includes input as the class handles directionality itself.
 */
inline static bool EstablishesOwnDirection(const Element* aElement) {
  return !ParticipatesInAutoDirection(aElement) ||
         aElement->IsHTMLElement(nsGkAtoms::bdi) || aElement->HasFixedDir() ||
         aElement->HasDirAuto();
}

/**
 * Returns true if aContent is dir=auto, affects a dir=auto ancestor, is
 * assigned to a dir=auto slot, or has an ancestor assigned to a dir=auto slot.
 *
 * It's false for input and textarea as they handle their directionality
 * themselves. We are concerned about steps 2 and 3 of
 * https://html.spec.whatwg.org/#auto-directionality
 */
inline static bool AffectsDirAutoElement(nsIContent* aContent) {
  return aContent && ParticipatesInAutoDirection(aContent) &&
         (aContent->NodeOrAncestorHasDirAuto() ||
          aContent->AffectsDirAutoSlot());
}

Directionality GetDirectionFromText(const char16_t* aText,
                                    const uint32_t aLength,
                                    uint32_t* aFirstStrong) {
  const char16_t* start = aText;
  const char16_t* end = aText + aLength;

  while (start < end) {
    uint32_t current = start - aText;
    uint32_t ch = *start++;

    if (start < end && NS_IS_SURROGATE_PAIR(ch, *start)) {
      ch = SURROGATE_TO_UCS4(ch, *start++);
      current++;
    }

    // Just ignore lone surrogates
    if (!IS_SURROGATE(ch)) {
      Directionality dir = GetDirectionFromChar(ch);
      if (dir != Directionality::Unset) {
        if (aFirstStrong) {
          *aFirstStrong = current;
        }
        return dir;
      }
    }
  }

  if (aFirstStrong) {
    *aFirstStrong = UINT32_MAX;
  }
  return Directionality::Unset;
}

static Directionality GetDirectionFromText(const char* aText,
                                           const uint32_t aLength,
                                           uint32_t* aFirstStrong = nullptr) {
  const char* start = aText;
  const char* end = aText + aLength;

  while (start < end) {
    uint32_t current = start - aText;
    unsigned char ch = (unsigned char)*start++;

    Directionality dir = GetDirectionFromChar(ch);
    if (dir != Directionality::Unset) {
      if (aFirstStrong) {
        *aFirstStrong = current;
      }
      return dir;
    }
  }

  if (aFirstStrong) {
    *aFirstStrong = UINT32_MAX;
  }
  return Directionality::Unset;
}

static Directionality GetDirectionFromText(const Text* aTextNode,
                                           uint32_t* aFirstStrong = nullptr) {
  const nsTextFragment* frag = &aTextNode->TextFragment();
  if (frag->Is2b()) {
    return GetDirectionFromText(frag->Get2b(), frag->GetLength(), aFirstStrong);
  }

  return GetDirectionFromText(frag->Get1b(), frag->GetLength(), aFirstStrong);
}

/**
 * Compute auto direction for aRoot. If aCanExcludeRoot is true and aRoot
 * establishes its own directionality, return early.
 * https://html.spec.whatwg.org/#contained-text-auto-directionality
 */
Directionality ContainedTextAutoDirectionality(nsINode* aRoot,
                                               bool aCanExcludeRoot) {
  MOZ_ASSERT_IF(aCanExcludeRoot, aRoot->IsElement());
  if (aCanExcludeRoot && EstablishesOwnDirection(aRoot->AsElement())) {
    return Directionality::Unset;
  }

  nsIContent* child = aRoot->GetFirstChild();
  while (child) {
    if (child->IsElement() && EstablishesOwnDirection(child->AsElement())) {
      child = child->GetNextNonChildNode(aRoot);
      continue;
    }

    // Step 1.2. If descendant is a slot element whose root is a shadow root,
    // then return the directionality of that shadow root's host.
    if (auto* slot = HTMLSlotElement::FromNode(child)) {
      if (const ShadowRoot* sr = slot->GetContainingShadow()) {
        Element* host = sr->GetHost();
        MOZ_ASSERT(host);
        return host->GetDirectionality();
      }
    }

    // Step 1.3-5. If descendant is a Text node, return its
    // text node directionality.
    if (auto* text = Text::FromNode(child)) {
      Directionality textNodeDir = GetDirectionFromText(text);
      if (textNodeDir != Directionality::Unset) {
        text->SetMaySetDirAuto();
        return textNodeDir;
      }
    }
    child = child->GetNextNode(aRoot);
  }

  return Directionality::Unset;
}

static Directionality ComputeAutoDirectionality(Element* aElement,
                                                bool aNotify);

/**
 * Compute auto direction aSlot should have based on assigned nodes
 * https://html.spec.whatwg.org/#auto-directionality step 2
 */
Directionality ComputeAutoDirectionFromAssignedNodes(
    HTMLSlotElement* aSlot, const nsTArray<RefPtr<nsINode>>& assignedNodes,
    bool aNotify) {
  // Step 2.1. For each node child of element's assigned nodes:
  for (const RefPtr<nsINode>& assignedNode : assignedNodes) {
    // Step 2.1.1. Let childDirection be null.
    Directionality childDirection = Directionality::Unset;

    // Step 2.1.2. If child is a Text node...
    if (auto* text = Text::FromNode(assignedNode)) {
      childDirection = GetDirectionFromText(text);
      if (childDirection != Directionality::Unset) {
        text->SetMaySetDirAuto();
      }
    } else {
      // Step 2.1.3.1. Assert: child is an Element node.
      Element* assignedElement = Element::FromNode(assignedNode);
      MOZ_ASSERT(assignedElement);

      // Step 2.1.3.2.
      childDirection = ContainedTextAutoDirectionality(assignedElement, true);
    }

    // Step 2.1.4. If childDirection is not null, then return childDirection.
    if (childDirection != Directionality::Unset) {
      return childDirection;
    }
  }
  // Step 2.2. Return null.
  return Directionality::Unset;
}

/**
 * Set the directionality of a node with dir=auto as defined in
 * https://html.spec.whatwg.org/#auto-directionality,
 * not including step 1: auto-directionality form-associated elements, this is
 * implemented by the elements themselves.
 *
 * Sets NodeMaySetDirAuto on the text node that determined the direction.
 */
static Directionality ComputeAutoDirectionality(Element* aElement,
                                                bool aNotify) {
  MOZ_ASSERT(aElement, "Must have an element");
  MOZ_ASSERT(ParticipatesInAutoDirection(aElement),
             "Cannot compute auto directionality of this element");

  // Step 2. If element is a slot element whose root is a shadow root and
  // element's assigned nodes are not empty:
  if (auto* slot = HTMLSlotElement::FromNode(aElement)) {
    const nsTArray<RefPtr<nsINode>>& assignedNodes = slot->AssignedNodes();
    if (!assignedNodes.IsEmpty()) {
      MOZ_ASSERT(slot->IsInShadowTree());
      return ComputeAutoDirectionFromAssignedNodes(slot, assignedNodes,
                                                   aNotify);
    }
  }

  // Step 3. find first text or slot that determines the direction
  Directionality nodeDir = ContainedTextAutoDirectionality(aElement, false);
  if (nodeDir != Directionality::Unset) {
    return nodeDir;
  }

  // Step 4. return null
  return Directionality::Unset;
}

Directionality GetParentDirectionality(const Element* aElement) {
  if (nsIContent* parent = aElement->GetParent()) {
    if (ShadowRoot* shadow = ShadowRoot::FromNode(parent)) {
      parent = shadow->GetHost();
    }
    if (parent && parent->IsElement()) {
      // If the node doesn't have an explicit dir attribute with a valid value,
      // the directionality is the same as the parent element (but don't
      // propagate the parent directionality if it isn't set yet).
      Directionality parentDir = parent->AsElement()->GetDirectionality();
      if (parentDir != Directionality::Unset) {
        return parentDir;
      }
    }
  }
  return Directionality::Ltr;
}

Directionality RecomputeDirectionality(Element* aElement, bool aNotify) {
  MOZ_ASSERT(!aElement->HasDirAuto(),
             "RecomputeDirectionality called with dir=auto");

  if (aElement->HasValidDir()) {
    return aElement->GetDirectionality();
  }

  // https://html.spec.whatwg.org/multipage/dom.html#the-directionality:
  //
  // If the element is an input element whose type attribute is in the
  // Telephone state, and the dir attribute is not in a defined state
  // (i.e. it is not present or has an invalid value)
  //
  //     The directionality of the element is 'ltr'.
  if (auto* input = HTMLInputElement::FromNode(*aElement)) {
    if (input->ControlType() == FormControlType::InputTel) {
      aElement->SetDirectionality(Directionality::Ltr, aNotify);
      return Directionality::Ltr;
    }
  }

  const Directionality dir = GetParentDirectionality(aElement);
  aElement->SetDirectionality(dir, aNotify);
  return dir;
}

// Whether the element establishes its own directionality and the one of its
// descendants.
static inline bool IsBoundary(const Element& aElement) {
  return aElement.HasValidDir() || aElement.HasDirAuto();
}

static void ResetAutoDirection(Element* aElement, bool aNotify);

/**
 * Called when shadow root host changes direction. Reset auto directionality
 * for dir=auto descendants whose direction may depend on the host
 * directionality through a slot element.
 *
 * Dynamic update for https://html.spec.whatwg.org/#auto-directionality step 3.2
 * If descendant is a slot element whose root is a shadow root, then return
 * the directionality of that shadow root's host.
 */
static void ResetAutoDirectionForAncestorsOfSlotDescendants(ShadowRoot* aShadow,
                                                            Directionality aDir,
                                                            bool aNotify) {
  // For now, reset auto directionality for all descendants, not only those
  // that have a slot descendant.
  for (nsIContent* cur = aShadow->GetFirstChild(); cur;
       cur = cur->GetNextNode(aShadow)) {
    if (Element* element = Element::FromNode(cur)) {
      if (element->HasDirAuto() && element->GetDirectionality() != aDir &&
          ParticipatesInAutoDirection(element)) {
        ResetAutoDirection(element, aNotify);
      }
    }
  }
}

static void SetDirectionalityOnDescendantsInternal(nsINode* aNode,
                                                   Directionality aDir,
                                                   bool aNotify) {
  if (auto* element = Element::FromNode(aNode)) {
    if (ShadowRoot* shadow = element->GetShadowRoot()) {
      // host direction changed, propagate it through slots to dir=auto elements
      ResetAutoDirectionForAncestorsOfSlotDescendants(shadow, aDir, aNotify);

      SetDirectionalityOnDescendantsInternal(shadow, aDir, aNotify);
    }
  }

  for (nsIContent* child = aNode->GetFirstChild(); child;) {
    auto* element = Element::FromNode(child);
    if (!element) {
      child = child->GetNextNode(aNode);
      continue;
    }

    if (IsBoundary(*element) || element->GetDirectionality() == aDir) {
      // If the element is a directionality boundary, or already
      // has the right directionality, then we can skip the whole subtree.
      child = child->GetNextNonChildNode(aNode);
      continue;
    }

    element->SetDirectionality(aDir, aNotify);

    if (ShadowRoot* shadow = element->GetShadowRoot()) {
      ResetAutoDirectionForAncestorsOfSlotDescendants(shadow, aDir, aNotify);

      SetDirectionalityOnDescendantsInternal(shadow, aDir, aNotify);
    }

    child = child->GetNextNode(aNode);
  }
}

// We want the public version of this only to acc
void SetDirectionalityOnDescendants(Element* aElement, Directionality aDir,
                                    bool aNotify) {
  return SetDirectionalityOnDescendantsInternal(aElement, aDir, aNotify);
}

static void ResetAutoDirection(Element* aElement, bool aNotify) {
  MOZ_ASSERT(aElement->HasDirAuto());
  Directionality dir = ComputeAutoDirectionality(aElement, aNotify);
  if (dir == Directionality::Unset) {
    dir = Directionality::Ltr;
  }
  if (dir != aElement->GetDirectionality()) {
    aElement->SetDirectionality(dir, aNotify);
    SetDirectionalityOnDescendants(aElement, aElement->GetDirectionality(),
                                   aNotify);
  }
}

/**
 * Reset auto direction of the dir=auto elements that aElement might impact.
 * Walk the parent chain till a dir=auto element is found, also reset dir=auto
 * slots an ancestor might be assigned to.
 */
static void WalkAncestorsResetAutoDirection(Element* aElement, bool aNotify) {
  for (nsIContent* ancestor = aElement; AffectsDirAutoElement(ancestor);
       ancestor = ancestor->GetParent()) {
    if (HTMLSlotElement* slot = ancestor->GetAssignedSlot()) {
      if (slot->HasDirAuto()) {
        ResetAutoDirection(slot, aNotify);
      }
    }

    auto* ancestorElement = Element::FromNode(*ancestor);
    if (ancestorElement && ancestorElement->HasDirAuto()) {
      ResetAutoDirection(ancestorElement, aNotify);
    }
  }
}

void SlotStateChanged(HTMLSlotElement* aSlot) {
  if (aSlot->HasDirAuto()) {
    ResetAutoDirection(aSlot, true);
  }
}

static void DownwardPropagateDirAutoFlags(nsINode* aRoot) {
  bool affectsAncestor = aRoot->NodeOrAncestorHasDirAuto(),
       affectsSlot = aRoot->AffectsDirAutoSlot();
  if (!affectsAncestor && !affectsSlot) {
    return;
  }

  nsIContent* child = aRoot->GetFirstChild();
  while (child) {
    if (child->IsElement() && EstablishesOwnDirection(child->AsElement())) {
      child = child->GetNextNonChildNode(aRoot);
      continue;
    }

    if (affectsAncestor) {
      child->SetAncestorHasDirAuto();
    }
    if (affectsSlot) {
      child->SetAffectsDirAutoSlot();
    }
    child = child->GetNextNode(aRoot);
  }
}

/**
 * aContent no longer affects the auto directionality of it's assigned slot,
 * e.g. as it is removed from the slot or the slot no longer has dir=auto.
 * Check if aContent impacts another slot and otherwise clear the flag.
 */
static void MaybeClearAffectsDirAutoSlot(nsIContent* aContent) {
  DebugOnly<HTMLSlotElement*> slot = aContent->GetAssignedSlot();
  MOZ_ASSERT(!slot || !slot->HasDirAuto(),
             "Function expects aContent not to impact its assigned slot");
  // check if aContent still inherits the flag from its parent
  if (Element* parent = aContent->GetParentElement()) {
    // do not check EstablishesOwnDirection(parent), as it is only true despite
    // AffectsDirAutoSlot if parent is directly assigned to a dir=auto slot
    if (parent->AffectsDirAutoSlot() &&
        !(aContent->IsElement() &&
          EstablishesOwnDirection(aContent->AsElement()))) {
      MOZ_ASSERT(aContent->AffectsDirAutoSlot());
      return;
    }
  }

  aContent->ClearAffectsDirAutoSlot();

  nsIContent* child = aContent->GetFirstChild();
  while (child) {
    if (child->IsElement() && EstablishesOwnDirection(child->AsElement())) {
      child = child->GetNextNonChildNode(aContent);
      continue;
    }
    if (HTMLSlotElement* slot = child->GetAssignedSlot()) {
      if (slot->HasDirAuto()) {
        child = child->GetNextNonChildNode(aContent);
        continue;
      }
    }

    child->ClearAffectsDirAutoSlot();
    child = child->GetNextNode(aContent);
  }
}

void SlotAssignedNodeAdded(HTMLSlotElement* aSlot, nsIContent& aAssignedNode) {
  if (aSlot->HasDirAuto()) {
    aAssignedNode.SetAffectsDirAutoSlot();
    DownwardPropagateDirAutoFlags(&aAssignedNode);
  }
  SlotStateChanged(aSlot);
}

void SlotAssignedNodeRemoved(HTMLSlotElement* aSlot,
                             nsIContent& aUnassignedNode) {
  if (aSlot->HasDirAuto()) {
    MaybeClearAffectsDirAutoSlot(&aUnassignedNode);
  }
  SlotStateChanged(aSlot);
}

/**
 * When dir=auto was set on aElement, reset it's auto direction and set the
 * flag on descendants
 */
void WalkDescendantsSetDirAuto(Element* aElement, bool aNotify) {
  MOZ_ASSERT(aElement->HasDirAuto());
  // Only test for ParticipatesInAutoDirection -- in other words, if aElement is
  // a <bdi> which is having its dir attribute set to auto (or
  // removed or set to an invalid value, which are equivalent to dir=auto for
  // <bdi>, we *do* want to set AncestorHasDirAuto on its descendants, unlike
  // in SetDirOnBind where we don't propagate AncestorHasDirAuto to a <bdi>
  // being bound to an existing node with dir=auto.
  if (ParticipatesInAutoDirection(aElement) &&
      !aElement->AncestorHasDirAuto()) {
    DownwardPropagateDirAutoFlags(aElement);
  }

  ResetAutoDirection(aElement, aNotify);
}

void WalkDescendantsClearAncestorDirAuto(nsIContent* aContent) {
  nsIContent* child = aContent->GetFirstChild();
  while (child) {
    if (child->IsElement() && EstablishesOwnDirection(child->AsElement())) {
      child = child->GetNextNonChildNode(aContent);
      continue;
    }

    child->ClearAncestorHasDirAuto();
    child = child->GetNextNode(aContent);
  }
}

/**
 * Returns whether answer is definitive, i.e. whether we found all dir=auto
 * elements impacted by aContent.
 * This is false when we hit the top of an ancestor chain without finding a
 * dir=auto element or an element with a fixed direction.
 * This is useful when processing node removals, since we might need to look at
 * the subtree we're removing from.
 */
static bool FindDirAutoElementsFrom(nsIContent* aContent,
                                    nsTArray<Element*>& aElements) {
  if (!AffectsDirAutoElement(aContent)) {
    return true;
  }

  for (nsIContent* ancestor = aContent; AffectsDirAutoElement(ancestor);
       ancestor = ancestor->GetParent()) {
    if (HTMLSlotElement* slot = ancestor->GetAssignedSlot()) {
      if (slot->HasDirAuto()) {
        aElements.AppendElement(slot);
        // need to check whether there are more dir=auto slots or ancestors
        nsIContent* parent = ancestor->GetParent();
        MOZ_ASSERT(parent, "Slotted content must have a parent");
        if (!parent->AffectsDirAutoSlot() &&
            !ancestor->NodeOrAncestorHasDirAuto()) {
          return true;
        }
      }
    }

    auto* ancestorElement = Element::FromNode(*ancestor);
    if (ancestorElement && ancestorElement->HasDirAuto()) {
      aElements.AppendElement(ancestorElement);
      return true;
    }
    if (ancestorElement && ancestorElement->IsInShadowTree() &&
        ancestorElement->IsHTMLElement(nsGkAtoms::slot)) {
      // further ancestors will inherit directionality from shadow host
      // https://html.spec.whatwg.org/#auto-directionality step 3.2
      // if descendant is a slot in a shadow DOM, return host directionality
      return true;
    }
  }

  return false;
}

/**
 * Reset auto directionality of ancestors of aTextNode
 */
static void SetAncestorDirectionIfAuto(Text* aTextNode, Directionality aDir,
                                       bool aNotify = true) {
  AutoTArray<Element*, 4> autoElements;
  FindDirAutoElementsFrom(aTextNode, autoElements);
  for (Element* autoElement : autoElements) {
    if (autoElement->GetDirectionality() == aDir) {
      // If we know that the directionality is already correct, we don't need to
      // reset it. But we might be responsible for the directionality of
      // parentElement.
      MOZ_ASSERT(aDir != Directionality::Unset);
      aTextNode->SetMaySetDirAuto();
    } else {
      // Otherwise recompute the directionality of parentElement.
      ResetAutoDirection(autoElement, aNotify);
    }
  }
}

bool TextNodeWillChangeDirection(Text* aTextNode, Directionality* aOldDir,
                                 uint32_t aOffset) {
  if (!AffectsDirAutoElement(aTextNode)) {
    return false;
  }

  // If the change has happened after the first character with strong
  // directionality in the text node, do nothing.
  uint32_t firstStrong;
  *aOldDir = GetDirectionFromText(aTextNode, &firstStrong);
  return (aOffset <= firstStrong);
}

void TextNodeChangedDirection(Text* aTextNode, Directionality aOldDir,
                              bool aNotify) {
  MOZ_ASSERT(AffectsDirAutoElement(aTextNode), "Caller should check");
  Directionality newDir = GetDirectionFromText(aTextNode);
  if (newDir == aOldDir) {
    return;
  }
  // If the old directionality is Unset, we might determine now dir=auto
  // ancestor direction now, even if we don't have the MaySetDirAuto flag.
  //
  // Otherwise we used to have a strong directionality and either no longer
  // does, or it changed. We might need to reset the direction.
  if (aOldDir == Directionality::Unset || aTextNode->MaySetDirAuto()) {
    SetAncestorDirectionIfAuto(aTextNode, newDir, aNotify);
  }
}

void SetDirectionFromNewTextNode(Text* aTextNode) {
  // Need to check parent as aTextNode does not yet have flags set
  if (!AffectsDirAutoElement(aTextNode->GetParent())) {
    return;
  }

  nsIContent* parent = aTextNode->GetParent();
  MOZ_ASSERT(parent);
  if (parent->NodeOrAncestorHasDirAuto()) {
    aTextNode->SetAncestorHasDirAuto();
  }
  if (parent->AffectsDirAutoSlot()) {
    aTextNode->SetAffectsDirAutoSlot();
  }

  Directionality dir = GetDirectionFromText(aTextNode);
  if (dir != Directionality::Unset) {
    SetAncestorDirectionIfAuto(aTextNode, dir);
  }
}

/**
 * Reset auto directionality for impacted elements when aTextNode is removed
 */
void ResetDirectionSetByTextNode(Text* aTextNode,
                                 dom::UnbindContext& aContext) {
  MOZ_ASSERT(!aTextNode->IsInComposedDoc(), "Should be disconnected already");
  if (!aTextNode->MaySetDirAuto()) {
    return;
  }
  AutoTArray<Element*, 4> autoElements;
  bool answerIsDefinitive = FindDirAutoElementsFrom(aTextNode, autoElements);

  if (answerIsDefinitive) {
    // All dir=auto elements are in our (now detached) subtree. We're done, as
    // nothing really changed for our purposes.
    return;
  }
  // The dir=auto element might have been on the element we're unbinding from.
  // In any case, this text node is clearly no longer what determines its
  // directionality.
  aTextNode->ClearMaySetDirAuto();
  auto* unboundFrom =
      nsIContent::FromNodeOrNull(aContext.GetOriginalSubtreeParent());
  if (!unboundFrom || !AffectsDirAutoElement(unboundFrom)) {
    return;
  }

  Directionality dir = GetDirectionFromText(aTextNode);
  if (dir == Directionality::Unset) {
    return;
  }

  autoElements.Clear();
  FindDirAutoElementsFrom(unboundFrom, autoElements);
  for (Element* autoElement : autoElements) {
    if (autoElement->GetDirectionality() != dir) {
      // it's dir was not determined by this text node
      continue;
    }
    ResetAutoDirection(autoElement, /* aNotify = */ true);
  }
}

void ResetDirFormAssociatedElement(Element* aElement, bool aNotify,
                                   bool aHasDirAuto,
                                   const nsAString* aKnownValue) {
  if (aHasDirAuto) {
    Directionality dir = Directionality::Unset;

    if (aKnownValue && IsAutoDirectionalityFormAssociatedElement(aElement)) {
      dir = GetDirectionFromText(aKnownValue->BeginReading(),
                                 aKnownValue->Length());
    } else if (!aKnownValue) {
      if (Maybe<nsAutoString> maybe =
              GetValueIfFormAssociatedElement(aElement)) {
        dir = GetDirectionFromText(maybe.value().BeginReading(),
                                   maybe.value().Length());
      }
    }

    // https://html.spec.whatwg.org/#the-directionality
    // If auto directionality returns null, then return ltr
    if (dir == Directionality::Unset) {
      dir = Directionality::Ltr;
    }

    if (aElement->GetDirectionality() != dir) {
      aElement->SetDirectionality(dir, aNotify);
    }
  }

  // If aElement is assigned to a dir=auto slot, it might determine its auto
  // directionality
  if (HTMLSlotElement* slot = aElement->GetAssignedSlot()) {
    if (slot->HasDirAuto() &&
        slot->GetDirectionality() != aElement->GetDirectionality()) {
      ResetAutoDirection(slot, aNotify);
    }
  }
}

void OnSetDirAttr(Element* aElement, const nsAttrValue* aNewValue,
                  bool hadValidDir, bool hadDirAuto, bool aNotify) {
  if (!ParticipatesInAutoDirection(aElement)) {
    return;
  }

  auto* elementAsSlot = HTMLSlotElement::FromNode(aElement);

  // If element was a boundary but is no more, inherit flags to subtree
  if ((hadDirAuto || hadValidDir) && !EstablishesOwnDirection(aElement)) {
    if (auto* slot = aElement->GetAssignedSlot()) {
      if (slot->HasDirAuto()) {
        aElement->SetAffectsDirAutoSlot();
      }
    }
    if (auto* parent = aElement->GetParent()) {
      DownwardPropagateDirAutoFlags(parent);
    }
  }

  if (AffectsDirAutoElement(aElement)) {
    // The element is a descendant of an element with dir = auto, is having its
    // dir attribute changed. Reset the direction of any of its ancestors whose
    // direction might be determined by a text node descendant
    WalkAncestorsResetAutoDirection(aElement, aNotify);
  } else if (hadDirAuto && !aElement->HasDirAuto()) {
    // The element isn't a descendant of an element with dir = auto, and is
    // having its dir attribute set to something other than auto.
    // Walk the descendant tree and clear the AncestorHasDirAuto flag.
    //
    // N.B: For elements other than <bdi> it would be enough to test that the
    //      current value of dir was "auto" in BeforeSetAttr to know that we
    //      were unsetting dir="auto". For <bdi> things are more complicated,
    //      since it behaves like dir="auto" whenever the dir attribute is
    //      empty or invalid, so we would have to check whether the old value
    //      was not either "ltr" or "rtl", and the new value was either "ltr"
    //      or "rtl". Element::HasDirAuto() encapsulates all that, so doing it
    //      here is simpler.
    WalkDescendantsClearAncestorDirAuto(aElement);
    if (elementAsSlot) {
      for (const auto& assignedNode : elementAsSlot->AssignedNodes()) {
        MaybeClearAffectsDirAutoSlot(assignedNode->AsContent());
      }
    }
  }

  if (aElement->HasDirAuto()) {
    if (elementAsSlot) {
      for (const auto& assignedNode : elementAsSlot->AssignedNodes()) {
        assignedNode->SetAffectsDirAutoSlot();
        DownwardPropagateDirAutoFlags(assignedNode);
      }
    }
    MaybeClearAffectsDirAutoSlot(aElement);
    WalkDescendantsSetDirAuto(aElement, aNotify);
  } else {
    Directionality oldDir = aElement->GetDirectionality();
    Directionality dir = RecomputeDirectionality(aElement, aNotify);
    if (oldDir != dir) {
      SetDirectionalityOnDescendants(aElement, dir, aNotify);
    }
  }
}

void SetDirOnBind(Element* aElement, nsIContent* aParent) {
  // Propagate flags from parent to new element
  if (!EstablishesOwnDirection(aElement) && AffectsDirAutoElement(aParent)) {
    if (aParent->NodeOrAncestorHasDirAuto()) {
      aElement->SetAncestorHasDirAuto();
    }
    if (aParent->AffectsDirAutoSlot()) {
      aElement->SetAffectsDirAutoSlot();
    }
    DownwardPropagateDirAutoFlags(aElement);

    if (aElement->GetFirstChild() ||
        (aElement->IsInShadowTree() && !aElement->HasValidDir() &&
         aElement->IsHTMLElement(nsGkAtoms::slot))) {
      // We may also need to reset the direction of an ancestor with dir=auto
      // as we are either an element with possible text descendants
      // or a slot that provides it's host directionality
      WalkAncestorsResetAutoDirection(aElement, true);
    }
  }

  if (!aElement->HasDirAuto()) {
    // if the element doesn't have dir=auto, set its own directionality from
    // the dir attribute or by inheriting from its ancestors.
    RecomputeDirectionality(aElement, false);
  }
}

void ResetDir(Element* aElement) {
  if (!aElement->HasDirAuto()) {
    RecomputeDirectionality(aElement, false);
  }
}

}  // end namespace mozilla
