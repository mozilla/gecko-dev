/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/HTMLEditor.h"

#include "HTMLEditUtils.h"
#include "TextEditUtils.h"
#include "TypeInState.h"
#include "mozilla/Assertions.h"
#include "mozilla/EditAction.h"
#include "mozilla/EditorUtils.h"
#include "mozilla/SelectionState.h"
#include "mozilla/TextEditRules.h"
#include "mozilla/dom/Selection.h"
#include "mozilla/dom/Element.h"
#include "mozilla/mozalloc.h"
#include "nsAString.h"
#include "nsAttrName.h"
#include "nsCOMPtr.h"
#include "nsCaseTreatment.h"
#include "nsComponentManagerUtils.h"
#include "nsDebug.h"
#include "nsError.h"
#include "nsGkAtoms.h"
#include "nsAtom.h"
#include "nsIContent.h"
#include "nsIContentIterator.h"
#include "nsNameSpaceManager.h"
#include "nsINode.h"
#include "nsISupportsImpl.h"
#include "nsLiteralString.h"
#include "nsRange.h"
#include "nsReadableUtils.h"
#include "nsString.h"
#include "nsStringFwd.h"
#include "nsTArray.h"
#include "nsUnicharUtils.h"
#include "nscore.h"

class nsISupports;

namespace mozilla {

using namespace dom;

static already_AddRefed<nsAtom> AtomizeAttribute(const nsAString& aAttribute) {
  if (aAttribute.IsEmpty()) {
    return nullptr;  // Don't use nsGkAtoms::_empty for attribute.
  }
  return NS_Atomize(aAttribute);
}

bool HTMLEditor::IsEmptyTextNode(nsINode& aNode) {
  bool isEmptyTextNode = false;
  return EditorBase::IsTextNode(&aNode) &&
         NS_SUCCEEDED(IsEmptyNode(&aNode, &isEmptyTextNode)) && isEmptyTextNode;
}

nsresult HTMLEditor::SetInlinePropertyAsAction(nsAtom& aProperty,
                                               nsAtom* aAttribute,
                                               const nsAString& aValue) {
  AutoEditActionDataSetter editActionData(
      *this,
      HTMLEditUtils::GetEditActionForFormatText(aProperty, aAttribute, true));
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  AutoTransactionBatch treatAsOneTransaction(*this);

  if (&aProperty == nsGkAtoms::sup) {
    // Superscript and Subscript styles are mutually exclusive.
    nsresult rv = RemoveInlinePropertyInternal(nsGkAtoms::sub, nullptr);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  } else if (&aProperty == nsGkAtoms::sub) {
    // Superscript and Subscript styles are mutually exclusive.
    nsresult rv = RemoveInlinePropertyInternal(nsGkAtoms::sup, nullptr);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }
  nsresult rv = SetInlinePropertyInternal(aProperty, aAttribute, aValue);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return NS_OK;
}

NS_IMETHODIMP
HTMLEditor::SetInlineProperty(const nsAString& aProperty,
                              const nsAString& aAttribute,
                              const nsAString& aValue) {
  RefPtr<nsAtom> property = NS_Atomize(aProperty);
  if (NS_WARN_IF(!property)) {
    return NS_ERROR_INVALID_ARG;
  }
  RefPtr<nsAtom> attribute = AtomizeAttribute(aAttribute);
  AutoEditActionDataSetter editActionData(
      *this,
      HTMLEditUtils::GetEditActionForFormatText(*property, attribute, true));
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  return SetInlinePropertyInternal(*property, attribute, aValue);
}

nsresult HTMLEditor::SetInlinePropertyInternal(nsAtom& aProperty,
                                               nsAtom* aAttribute,
                                               const nsAString& aValue) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  if (NS_WARN_IF(!mRules)) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  RefPtr<TextEditRules> rules(mRules);
  CommitComposition();

  if (SelectionRefPtr()->IsCollapsed()) {
    // Manipulating text attributes on a collapsed selection only sets state
    // for the next text insertion
    mTypeInState->SetProp(&aProperty, aAttribute, aValue);
    return NS_OK;
  }

  AutoPlaceholderBatch treatAsOneTransaction(*this);
  AutoTopLevelEditSubActionNotifier maybeTopLevelEditSubAction(
      *this, EditSubAction::eInsertElement, nsIEditor::eNext);
  AutoSelectionRestorer restoreSelectionLater(*this);
  AutoTransactionsConserveSelection dontChangeMySelection(*this);

  bool cancel, handled;
  EditSubActionInfo subActionInfo(EditSubAction::eSetTextProperty);
  // Protect the edit rules object from dying
  nsresult rv = rules->WillDoAction(subActionInfo, &cancel, &handled);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  if (!cancel && !handled) {
    // Loop through the ranges in the selection
    AutoRangeArray arrayOfRanges(SelectionRefPtr());
    for (auto& range : arrayOfRanges.mRanges) {
      // Adjust range to include any ancestors whose children are entirely
      // selected
      rv = PromoteInlineRange(*range);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      // Check for easy case: both range endpoints in same text node
      nsCOMPtr<nsINode> startNode = range->GetStartContainer();
      nsCOMPtr<nsINode> endNode = range->GetEndContainer();
      if (startNode && startNode == endNode && startNode->GetAsText()) {
        rv = SetInlinePropertyOnTextNode(
            *startNode->GetAsText(), range->StartOffset(), range->EndOffset(),
            aProperty, aAttribute, aValue);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
        continue;
      }

      // Not the easy case.  Range not contained in single text node.  There
      // are up to three phases here.  There are all the nodes reported by the
      // subtree iterator to be processed.  And there are potentially a
      // starting textnode and an ending textnode which are only partially
      // contained by the range.

      // Let's handle the nodes reported by the iterator.  These nodes are
      // entirely contained in the selection range.  We build up a list of them
      // (since doing operations on the document during iteration would perturb
      // the iterator).

      OwningNonNull<nsIContentIterator> iter = NS_NewContentSubtreeIterator();

      nsTArray<OwningNonNull<nsIContent>> arrayOfNodes;

      // Iterate range and build up array
      rv = iter->Init(range);
      // Init returns an error if there are no nodes in range.  This can easily
      // happen with the subtree iterator if the selection doesn't contain any
      // *whole* nodes.
      if (NS_SUCCEEDED(rv)) {
        for (; !iter->IsDone(); iter->Next()) {
          OwningNonNull<nsINode> node = *iter->GetCurrentNode();

          if (node->IsContent() && IsEditable(node)) {
            arrayOfNodes.AppendElement(*node->AsContent());
          }
        }
      }
      // First check the start parent of the range to see if it needs to be
      // separately handled (it does if it's a text node, due to how the
      // subtree iterator works - it will not have reported it).
      if (startNode && startNode->GetAsText() && IsEditable(startNode)) {
        rv = SetInlinePropertyOnTextNode(
            *startNode->GetAsText(), range->StartOffset(), startNode->Length(),
            aProperty, aAttribute, aValue);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
      }

      // Then loop through the list, set the property on each node
      for (auto& node : arrayOfNodes) {
        rv = SetInlinePropertyOnNode(*node, aProperty, aAttribute, aValue);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
      }

      // Last check the end parent of the range to see if it needs to be
      // separately handled (it does if it's a text node, due to how the
      // subtree iterator works - it will not have reported it).
      if (endNode && endNode->GetAsText() && IsEditable(endNode)) {
        rv = SetInlinePropertyOnTextNode(*endNode->GetAsText(), 0,
                                         range->EndOffset(), aProperty,
                                         aAttribute, aValue);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
      }
    }
  }
  if (cancel) {
    return NS_OK;
  }

  rv = rules->DidDoAction(subActionInfo, rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return NS_OK;
}

// Helper function for SetInlinePropertyOn*: is aNode a simple old <b>, <font>,
// <span style="">, etc. that we can reuse instead of creating a new one?
bool HTMLEditor::IsSimpleModifiableNode(nsIContent* aContent, nsAtom* aProperty,
                                        nsAtom* aAttribute,
                                        const nsAString* aValue) {
  // aContent can be null, in which case we'll return false in a few lines
  MOZ_ASSERT(aProperty);
  MOZ_ASSERT_IF(aAttribute, aValue);

  nsCOMPtr<dom::Element> element = do_QueryInterface(aContent);
  if (!element) {
    return false;
  }

  // First check for <b>, <i>, etc.
  if (element->IsHTMLElement(aProperty) && !element->GetAttrCount() &&
      !aAttribute) {
    return true;
  }

  // Special cases for various equivalencies: <strong>, <em>, <s>
  if (!element->GetAttrCount() &&
      ((aProperty == nsGkAtoms::b &&
        element->IsHTMLElement(nsGkAtoms::strong)) ||
       (aProperty == nsGkAtoms::i && element->IsHTMLElement(nsGkAtoms::em)) ||
       (aProperty == nsGkAtoms::strike &&
        element->IsHTMLElement(nsGkAtoms::s)))) {
    return true;
  }

  // Now look for things like <font>
  if (aAttribute) {
    nsString attrValue;
    if (element->IsHTMLElement(aProperty) &&
        IsOnlyAttribute(element, aAttribute) &&
        element->GetAttr(kNameSpaceID_None, aAttribute, attrValue) &&
        attrValue.Equals(*aValue, nsCaseInsensitiveStringComparator())) {
      // This is not quite correct, because it excludes cases like
      // <font face=000> being the same as <font face=#000000>.
      // Property-specific handling is needed (bug 760211).
      return true;
    }
  }

  // No luck so far.  Now we check for a <span> with a single style=""
  // attribute that sets only the style we're looking for, if this type of
  // style supports it
  if (!CSSEditUtils::IsCSSEditableProperty(element, aProperty, aAttribute) ||
      !element->IsHTMLElement(nsGkAtoms::span) ||
      element->GetAttrCount() != 1 ||
      !element->HasAttr(kNameSpaceID_None, nsGkAtoms::style)) {
    return false;
  }

  // Some CSS styles are not so simple.  For instance, underline is
  // "text-decoration: underline", which decomposes into four different text-*
  // properties.  So for now, we just create a span, add the desired style, and
  // see if it matches.
  nsCOMPtr<Element> newSpan = CreateHTMLContent(nsGkAtoms::span);
  NS_ASSERTION(newSpan, "CreateHTMLContent failed");
  NS_ENSURE_TRUE(newSpan, false);
  mCSSEditUtils->SetCSSEquivalentToHTMLStyle(newSpan, aProperty, aAttribute,
                                             aValue,
                                             /*suppress transaction*/ true);

  return CSSEditUtils::ElementsSameStyle(newSpan, element);
}

nsresult HTMLEditor::SetInlinePropertyOnTextNode(
    Text& aText, int32_t aStartOffset, int32_t aEndOffset, nsAtom& aProperty,
    nsAtom* aAttribute, const nsAString& aValue) {
  if (!aText.GetParentNode() ||
      !CanContainTag(*aText.GetParentNode(), aProperty)) {
    return NS_OK;
  }

  // Don't need to do anything if no characters actually selected
  if (aStartOffset == aEndOffset) {
    return NS_OK;
  }

  // Don't need to do anything if property already set on node
  if (CSSEditUtils::IsCSSEditableProperty(&aText, &aProperty, aAttribute)) {
    // The HTML styles defined by aProperty/aAttribute have a CSS equivalence
    // for node; let's check if it carries those CSS styles
    if (CSSEditUtils::IsCSSEquivalentToHTMLInlineStyleSet(
            &aText, &aProperty, aAttribute, aValue, CSSEditUtils::eComputed)) {
      return NS_OK;
    }
  } else if (IsTextPropertySetByContent(&aText, &aProperty, aAttribute,
                                        &aValue)) {
    return NS_OK;
  }

  // Make the range an independent node.
  nsCOMPtr<nsIContent> textNodeForTheRange = &aText;

  // Split at the end of the range.
  EditorRawDOMPoint atEnd(textNodeForTheRange, aEndOffset);
  if (!atEnd.IsEndOfContainer()) {
    // We need to split off back of text node
    ErrorResult error;
    textNodeForTheRange = SplitNodeWithTransaction(atEnd, error);
    if (NS_WARN_IF(error.Failed())) {
      return error.StealNSResult();
    }
  }

  // Split at the start of the range.
  EditorRawDOMPoint atStart(textNodeForTheRange, aStartOffset);
  if (!atStart.IsStartOfContainer()) {
    // We need to split off front of text node
    ErrorResult error;
    nsCOMPtr<nsIContent> newLeftNode = SplitNodeWithTransaction(atStart, error);
    if (NS_WARN_IF(error.Failed())) {
      return error.StealNSResult();
    }
    Unused << newLeftNode;
  }

  if (aAttribute) {
    // Look for siblings that are correct type of node
    nsIContent* sibling = GetPriorHTMLSibling(textNodeForTheRange);
    if (IsSimpleModifiableNode(sibling, &aProperty, aAttribute, &aValue)) {
      // Previous sib is already right kind of inline node; slide this over
      return MoveNodeToEndWithTransaction(*textNodeForTheRange, *sibling);
    }
    sibling = GetNextHTMLSibling(textNodeForTheRange);
    if (IsSimpleModifiableNode(sibling, &aProperty, aAttribute, &aValue)) {
      // Following sib is already right kind of inline node; slide this over
      return MoveNodeWithTransaction(*textNodeForTheRange,
                                     EditorRawDOMPoint(sibling, 0));
    }
  }

  // Reparent the node inside inline node with appropriate {attribute,value}
  return SetInlinePropertyOnNode(*textNodeForTheRange, aProperty, aAttribute,
                                 aValue);
}

nsresult HTMLEditor::SetInlinePropertyOnNodeImpl(nsIContent& aNode,
                                                 nsAtom& aProperty,
                                                 nsAtom* aAttribute,
                                                 const nsAString& aValue) {
  // If this is an element that can't be contained in a span, we have to
  // recurse to its children.
  if (!TagCanContain(*nsGkAtoms::span, aNode)) {
    if (aNode.HasChildren()) {
      nsTArray<OwningNonNull<nsIContent>> arrayOfNodes;

      // Populate the list.
      for (nsCOMPtr<nsIContent> child = aNode.GetFirstChild(); child;
           child = child->GetNextSibling()) {
        if (IsEditable(child) && !IsEmptyTextNode(*child)) {
          arrayOfNodes.AppendElement(*child);
        }
      }

      // Then loop through the list, set the property on each node.
      for (auto& node : arrayOfNodes) {
        nsresult rv =
            SetInlinePropertyOnNode(node, aProperty, aAttribute, aValue);
        NS_ENSURE_SUCCESS(rv, rv);
      }
    }
    return NS_OK;
  }

  // First check if there's an adjacent sibling we can put our node into.
  nsCOMPtr<nsIContent> previousSibling = GetPriorHTMLSibling(&aNode);
  nsCOMPtr<nsIContent> nextSibling = GetNextHTMLSibling(&aNode);
  if (IsSimpleModifiableNode(previousSibling, &aProperty, aAttribute,
                             &aValue)) {
    nsresult rv = MoveNodeToEndWithTransaction(aNode, *previousSibling);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    if (IsSimpleModifiableNode(nextSibling, &aProperty, aAttribute, &aValue)) {
      rv = JoinNodesWithTransaction(*previousSibling, *nextSibling);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }
    return NS_OK;
  }
  if (IsSimpleModifiableNode(nextSibling, &aProperty, aAttribute, &aValue)) {
    nsresult rv =
        MoveNodeWithTransaction(aNode, EditorRawDOMPoint(nextSibling, 0));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    return NS_OK;
  }

  // Don't need to do anything if property already set on node
  if (CSSEditUtils::IsCSSEditableProperty(&aNode, &aProperty, aAttribute)) {
    if (CSSEditUtils::IsCSSEquivalentToHTMLInlineStyleSet(
            &aNode, &aProperty, aAttribute, aValue, CSSEditUtils::eComputed)) {
      return NS_OK;
    }
  } else if (IsTextPropertySetByContent(&aNode, &aProperty, aAttribute,
                                        &aValue)) {
    return NS_OK;
  }

  bool useCSS = (IsCSSEnabled() && CSSEditUtils::IsCSSEditableProperty(
                                       &aNode, &aProperty, aAttribute)) ||
                // bgcolor is always done using CSS
                aAttribute == nsGkAtoms::bgcolor;

  if (useCSS) {
    RefPtr<dom::Element> tmp;
    // We only add style="" to <span>s with no attributes (bug 746515).  If we
    // don't have one, we need to make one.
    if (aNode.IsHTMLElement(nsGkAtoms::span) &&
        !aNode.AsElement()->GetAttrCount()) {
      tmp = aNode.AsElement();
    } else {
      tmp = InsertContainerWithTransaction(aNode, *nsGkAtoms::span);
      if (NS_WARN_IF(!tmp)) {
        return NS_ERROR_FAILURE;
      }
    }

    // Add the CSS styles corresponding to the HTML style request
    mCSSEditUtils->SetCSSEquivalentToHTMLStyle(tmp, &aProperty, aAttribute,
                                               &aValue, false);
    return NS_OK;
  }

  // is it already the right kind of node, but with wrong attribute?
  if (aNode.IsHTMLElement(&aProperty)) {
    if (NS_WARN_IF(!aAttribute)) {
      return NS_ERROR_FAILURE;
    }
    // Just set the attribute on it.
    return SetAttributeWithTransaction(*aNode.AsElement(), *aAttribute, aValue);
  }

  // ok, chuck it in its very own container
  RefPtr<Element> tmp = InsertContainerWithTransaction(
      aNode, aProperty, aAttribute ? *aAttribute : *nsGkAtoms::_empty, aValue);
  if (NS_WARN_IF(!tmp)) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

nsresult HTMLEditor::SetInlinePropertyOnNode(nsIContent& aNode,
                                             nsAtom& aProperty,
                                             nsAtom* aAttribute,
                                             const nsAString& aValue) {
  nsCOMPtr<nsIContent> previousSibling = aNode.GetPreviousSibling(),
                       nextSibling = aNode.GetNextSibling();
  NS_ENSURE_STATE(aNode.GetParentNode());
  OwningNonNull<nsINode> parent = *aNode.GetParentNode();

  nsresult rv = RemoveStyleInside(aNode, &aProperty, aAttribute);
  NS_ENSURE_SUCCESS(rv, rv);

  if (aNode.GetParentNode()) {
    // The node is still where it was
    return SetInlinePropertyOnNodeImpl(aNode, aProperty, aAttribute, aValue);
  }

  // It's vanished.  Use the old siblings for reference to construct a
  // list.  But first, verify that the previous/next siblings are still
  // where we expect them; otherwise we have to give up.
  if ((previousSibling && previousSibling->GetParentNode() != parent) ||
      (nextSibling && nextSibling->GetParentNode() != parent)) {
    return NS_ERROR_UNEXPECTED;
  }
  nsTArray<OwningNonNull<nsIContent>> nodesToSet;
  nsCOMPtr<nsIContent> cur = previousSibling ? previousSibling->GetNextSibling()
                                             : parent->GetFirstChild();
  for (; cur && cur != nextSibling; cur = cur->GetNextSibling()) {
    if (IsEditable(cur)) {
      nodesToSet.AppendElement(*cur);
    }
  }

  for (auto& node : nodesToSet) {
    rv = SetInlinePropertyOnNodeImpl(node, aProperty, aAttribute, aValue);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

nsresult HTMLEditor::SplitStyleAboveRange(nsRange* aRange, nsAtom* aProperty,
                                          nsAtom* aAttribute) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  if (NS_WARN_IF(!aRange)) {
    return NS_ERROR_INVALID_ARG;
  }

  nsCOMPtr<nsINode> startNode = aRange->GetStartContainer();
  int32_t startOffset = aRange->StartOffset();
  nsCOMPtr<nsINode> endNode = aRange->GetEndContainer();
  int32_t endOffset = aRange->EndOffset();

  nsCOMPtr<nsINode> origStartNode = startNode;

  // split any matching style nodes above the start of range
  {
    AutoTrackDOMPoint tracker(RangeUpdaterRef(), address_of(endNode),
                              &endOffset);
    nsresult rv = SplitStyleAbovePoint(address_of(startNode), &startOffset,
                                       aProperty, aAttribute);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  // second verse, same as the first...
  nsresult rv = SplitStyleAbovePoint(address_of(endNode), &endOffset, aProperty,
                                     aAttribute);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // reset the range
  rv = aRange->SetStartAndEnd(startNode, startOffset, endNode, endOffset);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return NS_OK;
}

nsresult HTMLEditor::SplitStyleAbovePoint(
    nsCOMPtr<nsINode>* aNode, int32_t* aOffset,
    // null here means we split all properties
    nsAtom* aProperty, nsAtom* aAttribute, nsIContent** aOutLeftNode,
    nsIContent** aOutRightNode) {
  NS_ENSURE_TRUE(aNode && *aNode && aOffset, NS_ERROR_NULL_POINTER);
  NS_ENSURE_TRUE((*aNode)->IsContent(), NS_OK);

  if (aOutLeftNode) {
    *aOutLeftNode = nullptr;
  }
  if (aOutRightNode) {
    *aOutRightNode = nullptr;
  }

  // Split any matching style nodes above the node/offset
  nsCOMPtr<nsIContent> node = (*aNode)->AsContent();

  bool useCSS = IsCSSEnabled();

  bool isSet;
  while (!IsBlockNode(node) && node->GetParent() &&
         IsEditable(node->GetParent())) {
    isSet = false;
    if (useCSS &&
        CSSEditUtils::IsCSSEditableProperty(node, aProperty, aAttribute)) {
      // The HTML style defined by aProperty/aAttribute has a CSS equivalence
      // in this implementation for the node; let's check if it carries those
      // CSS styles
      nsAutoString firstValue;
      isSet = CSSEditUtils::IsCSSEquivalentToHTMLInlineStyleSet(
          node, aProperty, aAttribute, firstValue, CSSEditUtils::eSpecified);
    }
    if (  // node is the correct inline prop
        (aProperty && node->IsHTMLElement(aProperty)) ||
        // node is href - test if really <a href=...
        (aProperty == nsGkAtoms::href && HTMLEditUtils::IsLink(node)) ||
        // or node is any prop, and we asked to split them all
        (!aProperty && NodeIsProperty(*node)) ||
        // or the style is specified in the style attribute
        isSet) {
      // Found a style node we need to split
      SplitNodeResult splitNodeResult = SplitNodeDeepWithTransaction(
          *node, EditorRawDOMPoint(*aNode, *aOffset),
          SplitAtEdges::eAllowToCreateEmptyContainer);
      NS_WARNING_ASSERTION(splitNodeResult.Succeeded(),
                           "Failed to split the node");

      EditorRawDOMPoint atRightNode(splitNodeResult.SplitPoint());
      *aNode = atRightNode.GetContainer();
      *aOffset = atRightNode.Offset();
      if (aOutLeftNode) {
        NS_IF_ADDREF(*aOutLeftNode = splitNodeResult.GetPreviousNode());
      }
      if (aOutRightNode) {
        NS_IF_ADDREF(*aOutRightNode = splitNodeResult.GetNextNode());
      }
    }
    node = node->GetParent();
    if (NS_WARN_IF(!node)) {
      return NS_ERROR_FAILURE;
    }
  }

  return NS_OK;
}

nsresult HTMLEditor::ClearStyle(nsCOMPtr<nsINode>* aNode, int32_t* aOffset,
                                nsAtom* aProperty, nsAtom* aAttribute) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  nsCOMPtr<nsIContent> leftNode, rightNode;
  nsresult rv =
      SplitStyleAbovePoint(aNode, aOffset, aProperty, aAttribute,
                           getter_AddRefs(leftNode), getter_AddRefs(rightNode));
  NS_ENSURE_SUCCESS(rv, rv);

  if (leftNode) {
    bool bIsEmptyNode;
    IsEmptyNode(leftNode, &bIsEmptyNode, false, true);
    if (bIsEmptyNode) {
      // delete leftNode if it became empty
      rv = DeleteNodeWithTransaction(*leftNode);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }
  }
  if (rightNode) {
    nsCOMPtr<nsINode> secondSplitParent = GetLeftmostChild(rightNode);
    // don't try to split non-containers (br's, images, hr's, etc.)
    if (!secondSplitParent) {
      secondSplitParent = rightNode;
    }
    nsCOMPtr<Element> savedBR;
    if (!IsContainer(secondSplitParent)) {
      if (TextEditUtils::IsBreak(secondSplitParent)) {
        savedBR = do_QueryInterface(secondSplitParent);
        NS_ENSURE_STATE(savedBR);
      }

      secondSplitParent = secondSplitParent->GetParentNode();
    }
    *aOffset = 0;
    rv = SplitStyleAbovePoint(address_of(secondSplitParent), aOffset, aProperty,
                              aAttribute, getter_AddRefs(leftNode),
                              getter_AddRefs(rightNode));
    NS_ENSURE_SUCCESS(rv, rv);

    if (rightNode) {
      bool bIsEmptyNode;
      IsEmptyNode(rightNode, &bIsEmptyNode, false, true);
      if (bIsEmptyNode) {
        // delete rightNode if it became empty
        rv = DeleteNodeWithTransaction(*rightNode);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
      }
    }

    if (!leftNode) {
      return NS_OK;
    }

    // should be impossible to not get a new leftnode here
    nsCOMPtr<nsINode> newSelParent = GetLeftmostChild(leftNode);
    if (!newSelParent) {
      newSelParent = leftNode;
    }
    // If rightNode starts with a br, suck it out of right node and into
    // leftNode.  This is so we you don't revert back to the previous style
    // if you happen to click at the end of a line.
    if (savedBR) {
      rv =
          MoveNodeWithTransaction(*savedBR, EditorRawDOMPoint(newSelParent, 0));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }
    // remove the style on this new hierarchy
    int32_t newSelOffset = 0;
    {
      // Track the point at the new hierarchy.  This is so we can know where
      // to put the selection after we call RemoveStyleInside().
      // RemoveStyleInside() could remove any and all of those nodes, so I
      // have to use the range tracking system to find the right spot to put
      // selection.
      AutoTrackDOMPoint tracker(RangeUpdaterRef(), address_of(newSelParent),
                                &newSelOffset);
      rv = RemoveStyleInside(*leftNode, aProperty, aAttribute);
      NS_ENSURE_SUCCESS(rv, rv);
    }
    // reset our node offset values to the resulting new sel point
    *aNode = newSelParent;
    *aOffset = newSelOffset;
  }

  return NS_OK;
}

bool HTMLEditor::NodeIsProperty(nsINode& aNode) {
  return IsContainer(&aNode) && IsEditable(&aNode) && !IsBlockNode(&aNode) &&
         !aNode.IsHTMLElement(nsGkAtoms::a);
}

nsresult HTMLEditor::RemoveStyleInside(nsIContent& aNode, nsAtom* aProperty,
                                       nsAtom* aAttribute,
                                       const bool aChildrenOnly /* = false */) {
  if (!aNode.IsElement()) {
    return NS_OK;
  }

  // first process the children
  RefPtr<nsIContent> child = aNode.GetFirstChild();
  while (child) {
    // cache next sibling since we might remove child
    nsCOMPtr<nsIContent> next = child->GetNextSibling();
    nsresult rv = RemoveStyleInside(*child, aProperty, aAttribute);
    NS_ENSURE_SUCCESS(rv, rv);
    child = next.forget();
  }

  // then process the node itself
  if (!aChildrenOnly &&
      // node is prop we asked for
      ((aProperty && aNode.NodeInfo()->NameAtom() == aProperty) ||
       // but check for link (<a href=...)
       (aProperty == nsGkAtoms::href && HTMLEditUtils::IsLink(&aNode)) ||
       // and for named anchors
       (aProperty == nsGkAtoms::name && HTMLEditUtils::IsNamedAnchor(&aNode)) ||
       // or node is any prop and we asked for that
       (!aProperty && NodeIsProperty(aNode)))) {
    // if we weren't passed an attribute, then we want to
    // remove any matching inlinestyles entirely
    if (!aAttribute) {
      bool hasStyleAttr =
          aNode.AsElement()->HasAttr(kNameSpaceID_None, nsGkAtoms::style);
      bool hasClassAttr =
          aNode.AsElement()->HasAttr(kNameSpaceID_None, nsGkAtoms::_class);
      if (aProperty && (hasStyleAttr || hasClassAttr)) {
        // aNode carries inline styles or a class attribute so we can't
        // just remove the element... We need to create above the element
        // a span that will carry those styles or class, then we can delete
        // the node.
        RefPtr<Element> spanNode =
            InsertContainerWithTransaction(aNode, *nsGkAtoms::span);
        if (NS_WARN_IF(!spanNode)) {
          return NS_ERROR_FAILURE;
        }
        nsresult rv = CloneAttributeWithTransaction(
            *nsGkAtoms::style, *spanNode, *aNode.AsElement());
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
        rv = CloneAttributeWithTransaction(*nsGkAtoms::_class, *spanNode,
                                           *aNode.AsElement());
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
      }
      nsresult rv = RemoveContainerWithTransaction(*aNode.AsElement());
      NS_ENSURE_SUCCESS(rv, rv);
    } else if (aNode.IsElement()) {
      // otherwise we just want to eliminate the attribute
      if (aNode.AsElement()->HasAttr(kNameSpaceID_None, aAttribute)) {
        // if this matching attribute is the ONLY one on the node,
        // then remove the whole node.  Otherwise just nix the attribute.
        if (IsOnlyAttribute(aNode.AsElement(), aAttribute)) {
          nsresult rv = RemoveContainerWithTransaction(*aNode.AsElement());
          if (NS_WARN_IF(NS_FAILED(rv))) {
            return rv;
          }
        } else {
          nsresult rv =
              RemoveAttributeWithTransaction(*aNode.AsElement(), *aAttribute);
          if (NS_WARN_IF(NS_FAILED(rv))) {
            return rv;
          }
        }
      }
    }
  }

  if (!aChildrenOnly &&
      CSSEditUtils::IsCSSEditableProperty(&aNode, aProperty, aAttribute)) {
    // the HTML style defined by aProperty/aAttribute has a CSS equivalence in
    // this implementation for the node aNode; let's check if it carries those
    // css styles
    if (aNode.IsElement()) {
      bool hasAttribute = CSSEditUtils::HaveCSSEquivalentStyles(
          aNode, aProperty, aAttribute, CSSEditUtils::eSpecified);
      if (hasAttribute) {
        // yes, tmp has the corresponding css declarations in its style
        // attribute
        // let's remove them
        mCSSEditUtils->RemoveCSSEquivalentToHTMLStyle(
            aNode.AsElement(), aProperty, aAttribute, nullptr, false);
        // remove the node if it is a span or font, if its style attribute is
        // empty or absent, and if it does not have a class nor an id
        RemoveElementIfNoStyleOrIdOrClass(*aNode.AsElement());
      }
    }
  }

  // Or node is big or small and we are setting font size
  if (aChildrenOnly) {
    return NS_OK;
  }
  if (aProperty == nsGkAtoms::font &&
      (aNode.IsHTMLElement(nsGkAtoms::big) ||
       aNode.IsHTMLElement(nsGkAtoms::small)) &&
      aAttribute == nsGkAtoms::size) {
    // if we are setting font size, remove any nested bigs and smalls
    return RemoveContainerWithTransaction(*aNode.AsElement());
  }
  return NS_OK;
}

bool HTMLEditor::IsOnlyAttribute(const Element* aElement, nsAtom* aAttribute) {
  MOZ_ASSERT(aElement);

  uint32_t attrCount = aElement->GetAttrCount();
  for (uint32_t i = 0; i < attrCount; ++i) {
    const nsAttrName* name = aElement->GetAttrNameAt(i);
    if (!name->NamespaceEquals(kNameSpaceID_None)) {
      return false;
    }

    // if it's the attribute we know about, or a special _moz attribute,
    // keep looking
    if (name->LocalName() != aAttribute) {
      nsAutoString attrString;
      name->LocalName()->ToString(attrString);
      if (!StringBeginsWith(attrString, NS_LITERAL_STRING("_moz"))) {
        return false;
      }
    }
  }
  // if we made it through all of them without finding a real attribute
  // other than aAttribute, then return true
  return true;
}

nsresult HTMLEditor::PromoteRangeIfStartsOrEndsInNamedAnchor(nsRange& aRange) {
  // We assume that <a> is not nested.
  nsCOMPtr<nsINode> startNode = aRange.GetStartContainer();
  int32_t startOffset = aRange.StartOffset();
  nsCOMPtr<nsINode> endNode = aRange.GetEndContainer();
  int32_t endOffset = aRange.EndOffset();

  nsCOMPtr<nsINode> parent = startNode;

  while (parent && !parent->IsHTMLElement(nsGkAtoms::body) &&
         !HTMLEditUtils::IsNamedAnchor(parent)) {
    parent = parent->GetParentNode();
  }
  NS_ENSURE_TRUE(parent, NS_ERROR_NULL_POINTER);

  if (HTMLEditUtils::IsNamedAnchor(parent)) {
    startNode = parent->GetParentNode();
    startOffset = startNode ? startNode->ComputeIndexOf(parent) : -1;
  }

  parent = endNode;
  while (parent && !parent->IsHTMLElement(nsGkAtoms::body) &&
         !HTMLEditUtils::IsNamedAnchor(parent)) {
    parent = parent->GetParentNode();
  }
  NS_ENSURE_TRUE(parent, NS_ERROR_NULL_POINTER);

  if (HTMLEditUtils::IsNamedAnchor(parent)) {
    endNode = parent->GetParentNode();
    endOffset = endNode ? endNode->ComputeIndexOf(parent) + 1 : 0;
  }

  nsresult rv =
      aRange.SetStartAndEnd(startNode, startOffset, endNode, endOffset);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult HTMLEditor::PromoteInlineRange(nsRange& aRange) {
  nsCOMPtr<nsINode> startNode = aRange.GetStartContainer();
  int32_t startOffset = aRange.StartOffset();
  nsCOMPtr<nsINode> endNode = aRange.GetEndContainer();
  int32_t endOffset = aRange.EndOffset();

  while (startNode && !startNode->IsHTMLElement(nsGkAtoms::body) &&
         IsEditable(startNode) && IsAtFrontOfNode(*startNode, startOffset)) {
    nsCOMPtr<nsINode> parent = startNode->GetParentNode();
    NS_ENSURE_TRUE(parent, NS_ERROR_NULL_POINTER);
    startOffset = parent->ComputeIndexOf(startNode);
    startNode = parent;
  }

  while (endNode && !endNode->IsHTMLElement(nsGkAtoms::body) &&
         IsEditable(endNode) && IsAtEndOfNode(*endNode, endOffset)) {
    nsCOMPtr<nsINode> parent = endNode->GetParentNode();
    NS_ENSURE_TRUE(parent, NS_ERROR_NULL_POINTER);
    // We are AFTER this node
    endOffset = 1 + parent->ComputeIndexOf(endNode);
    endNode = parent;
  }

  nsresult rv =
      aRange.SetStartAndEnd(startNode, startOffset, endNode, endOffset);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

bool HTMLEditor::IsAtFrontOfNode(nsINode& aNode, int32_t aOffset) {
  if (!aOffset) {
    return true;
  }

  if (IsTextNode(&aNode)) {
    return false;
  }

  nsCOMPtr<nsIContent> firstNode = GetFirstEditableChild(aNode);
  NS_ENSURE_TRUE(firstNode, true);
  if (aNode.ComputeIndexOf(firstNode) < aOffset) {
    return false;
  }
  return true;
}

bool HTMLEditor::IsAtEndOfNode(nsINode& aNode, int32_t aOffset) {
  if (aOffset == (int32_t)aNode.Length()) {
    return true;
  }

  if (IsTextNode(&aNode)) {
    return false;
  }

  nsCOMPtr<nsIContent> lastNode = GetLastEditableChild(aNode);
  NS_ENSURE_TRUE(lastNode, true);
  if (aNode.ComputeIndexOf(lastNode) < aOffset) {
    return true;
  }
  return false;
}

nsresult HTMLEditor::GetInlinePropertyBase(nsAtom& aProperty,
                                           nsAtom* aAttribute,
                                           const nsAString* aValue,
                                           bool* aFirst, bool* aAny, bool* aAll,
                                           nsAString* outValue) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  *aAny = false;
  *aAll = true;
  *aFirst = false;
  bool first = true;

  bool isCollapsed = SelectionRefPtr()->IsCollapsed();
  RefPtr<nsRange> range = SelectionRefPtr()->GetRangeAt(0);
  // XXX: Should be a while loop, to get each separate range
  // XXX: ERROR_HANDLING can currentItem be null?
  if (range) {
    // For each range, set a flag
    bool firstNodeInRange = true;

    if (isCollapsed) {
      nsCOMPtr<nsINode> collapsedNode = range->GetStartContainer();
      if (NS_WARN_IF(!collapsedNode)) {
        return NS_ERROR_FAILURE;
      }
      bool isSet, theSetting;
      nsString tOutString;
      if (aAttribute) {
        mTypeInState->GetTypingState(isSet, theSetting, &aProperty, aAttribute,
                                     &tOutString);
        if (outValue) {
          outValue->Assign(tOutString);
        }
      } else {
        mTypeInState->GetTypingState(isSet, theSetting, &aProperty);
      }
      if (isSet) {
        *aFirst = *aAny = *aAll = theSetting;
        return NS_OK;
      }

      if (CSSEditUtils::IsCSSEditableProperty(collapsedNode, &aProperty,
                                              aAttribute)) {
        if (aValue) {
          tOutString.Assign(*aValue);
        }
        *aFirst = *aAny = *aAll =
            CSSEditUtils::IsCSSEquivalentToHTMLInlineStyleSet(
                collapsedNode, &aProperty, aAttribute, tOutString,
                CSSEditUtils::eComputed);
        if (outValue) {
          outValue->Assign(tOutString);
        }
        return NS_OK;
      }

      isSet = IsTextPropertySetByContent(collapsedNode, &aProperty, aAttribute,
                                         aValue, outValue);
      *aFirst = *aAny = *aAll = isSet;
      return NS_OK;
    }

    // Non-collapsed selection
    nsCOMPtr<nsIContentIterator> iter = NS_NewContentIterator();

    nsAutoString firstValue, theValue;

    nsCOMPtr<nsINode> endNode = range->GetEndContainer();
    int32_t endOffset = range->EndOffset();

    for (iter->Init(range); !iter->IsDone(); iter->Next()) {
      if (!iter->GetCurrentNode()->IsContent()) {
        continue;
      }
      nsCOMPtr<nsIContent> content = iter->GetCurrentNode()->AsContent();

      if (content->IsHTMLElement(nsGkAtoms::body)) {
        break;
      }

      // just ignore any non-editable nodes
      if (content->GetAsText() &&
          (!IsEditable(content) || IsEmptyTextNode(*content))) {
        continue;
      }
      if (content->GetAsText()) {
        if (!isCollapsed && first && firstNodeInRange) {
          firstNodeInRange = false;
          if (range->StartOffset() == content->Length()) {
            continue;
          }
        } else if (content == endNode && !endOffset) {
          continue;
        }
      } else if (content->IsElement()) {
        // handle non-text leaf nodes here
        continue;
      }

      bool isSet = false;
      if (first) {
        if (CSSEditUtils::IsCSSEditableProperty(content, &aProperty,
                                                aAttribute)) {
          // The HTML styles defined by aProperty/aAttribute have a CSS
          // equivalence in this implementation for node; let's check if it
          // carries those CSS styles
          if (aValue) {
            firstValue.Assign(*aValue);
          }
          isSet = CSSEditUtils::IsCSSEquivalentToHTMLInlineStyleSet(
              content, &aProperty, aAttribute, firstValue,
              CSSEditUtils::eComputed);
        } else {
          isSet = IsTextPropertySetByContent(content, &aProperty, aAttribute,
                                             aValue, &firstValue);
        }
        *aFirst = isSet;
        first = false;
        if (outValue) {
          *outValue = firstValue;
        }
      } else {
        if (CSSEditUtils::IsCSSEditableProperty(content, &aProperty,
                                                aAttribute)) {
          // The HTML styles defined by aProperty/aAttribute have a CSS
          // equivalence in this implementation for node; let's check if it
          // carries those CSS styles
          if (aValue) {
            theValue.Assign(*aValue);
          }
          isSet = CSSEditUtils::IsCSSEquivalentToHTMLInlineStyleSet(
              content, &aProperty, aAttribute, theValue,
              CSSEditUtils::eComputed);
        } else {
          isSet = IsTextPropertySetByContent(content, &aProperty, aAttribute,
                                             aValue, &theValue);
        }
        if (firstValue != theValue) {
          *aAll = false;
        }
      }

      if (isSet) {
        *aAny = true;
      } else {
        *aAll = false;
      }
    }
  }
  if (!*aAny) {
    // make sure that if none of the selection is set, we don't report all is
    // set
    *aAll = false;
  }
  return NS_OK;
}

NS_IMETHODIMP
HTMLEditor::GetInlineProperty(const nsAString& aProperty,
                              const nsAString& aAttribute,
                              const nsAString& aValue, bool* aFirst, bool* aAny,
                              bool* aAll) {
  RefPtr<nsAtom> property = NS_Atomize(aProperty);
  RefPtr<nsAtom> attribute = AtomizeAttribute(aAttribute);
  return GetInlineProperty(property, attribute, aValue, aFirst, aAny, aAll);
}

nsresult HTMLEditor::GetInlineProperty(nsAtom* aProperty, nsAtom* aAttribute,
                                       const nsAString& aValue, bool* aFirst,
                                       bool* aAny, bool* aAll) {
  if (NS_WARN_IF(!aProperty) || NS_WARN_IF(!aFirst) || NS_WARN_IF(!aAny) ||
      NS_WARN_IF(!aAll)) {
    return NS_ERROR_INVALID_ARG;
  }

  AutoEditActionDataSetter editActionData(*this, EditAction::eNotEditing);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  const nsAString* val = nullptr;
  if (!aValue.IsEmpty()) val = &aValue;
  return GetInlinePropertyBase(*aProperty, aAttribute, val, aFirst, aAny, aAll,
                               nullptr);
}

NS_IMETHODIMP
HTMLEditor::GetInlinePropertyWithAttrValue(const nsAString& aProperty,
                                           const nsAString& aAttribute,
                                           const nsAString& aValue,
                                           bool* aFirst, bool* aAny, bool* aAll,
                                           nsAString& outValue) {
  RefPtr<nsAtom> property = NS_Atomize(aProperty);
  RefPtr<nsAtom> attribute = AtomizeAttribute(aAttribute);
  return GetInlinePropertyWithAttrValue(property, attribute, aValue, aFirst,
                                        aAny, aAll, outValue);
}

nsresult HTMLEditor::GetInlinePropertyWithAttrValue(
    nsAtom* aProperty, nsAtom* aAttribute, const nsAString& aValue,
    bool* aFirst, bool* aAny, bool* aAll, nsAString& outValue) {
  if (NS_WARN_IF(!aProperty) || NS_WARN_IF(!aFirst) || NS_WARN_IF(!aAny) ||
      NS_WARN_IF(!aAll)) {
    return NS_ERROR_INVALID_ARG;
  }

  AutoEditActionDataSetter editActionData(*this, EditAction::eNotEditing);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  const nsAString* val = nullptr;
  if (!aValue.IsEmpty()) val = &aValue;
  return GetInlinePropertyBase(*aProperty, aAttribute, val, aFirst, aAny, aAll,
                               &outValue);
}

NS_IMETHODIMP
HTMLEditor::RemoveAllInlineProperties() {
  AutoEditActionDataSetter editActionData(
      *this, EditAction::eRemoveAllInlineStyleProperties);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  AutoPlaceholderBatch treatAsOneTransaction(*this);
  AutoTopLevelEditSubActionNotifier maybeTopLevelEditSubAction(
      *this, EditSubAction::eRemoveAllTextProperties, nsIEditor::eNext);

  nsresult rv = RemoveInlinePropertyInternal(nullptr, nullptr);
  NS_ENSURE_SUCCESS(rv, rv);
  return NS_OK;
}

nsresult HTMLEditor::RemoveInlinePropertyAsAction(nsAtom& aProperty,
                                                  nsAtom* aAttribute) {
  AutoEditActionDataSetter editActionData(
      *this,
      HTMLEditUtils::GetEditActionForFormatText(aProperty, aAttribute, false));
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  nsresult rv = RemoveInlinePropertyInternal(&aProperty, aAttribute);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return NS_OK;
}

NS_IMETHODIMP
HTMLEditor::RemoveInlineProperty(const nsAString& aProperty,
                                 const nsAString& aAttribute) {
  RefPtr<nsAtom> property = NS_Atomize(aProperty);
  RefPtr<nsAtom> attribute = AtomizeAttribute(aAttribute);

  AutoEditActionDataSetter editActionData(
      *this,
      HTMLEditUtils::GetEditActionForFormatText(*property, attribute, false));
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  return RemoveInlinePropertyInternal(property, attribute);
}

nsresult HTMLEditor::RemoveInlinePropertyInternal(nsAtom* aProperty,
                                                  nsAtom* aAttribute) {
  MOZ_ASSERT(IsEditActionDataAvailable());
  MOZ_ASSERT(aAttribute != nsGkAtoms::_empty);

  if (NS_WARN_IF(!mRules)) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  CommitComposition();

  if (SelectionRefPtr()->IsCollapsed()) {
    // Manipulating text attributes on a collapsed selection only sets state
    // for the next text insertion

    // For links, aProperty uses "href", use "a" instead
    if (aProperty == nsGkAtoms::href || aProperty == nsGkAtoms::name) {
      aProperty = nsGkAtoms::a;
    }

    if (aProperty) {
      mTypeInState->ClearProp(aProperty, aAttribute);
    } else {
      mTypeInState->ClearAllProps();
    }
    return NS_OK;
  }

  AutoPlaceholderBatch treatAsOneTransaction(*this);
  AutoTopLevelEditSubActionNotifier maybeTopLevelEditSubAction(
      *this, EditSubAction::eRemoveTextProperty, nsIEditor::eNext);
  AutoSelectionRestorer restoreSelectionLater(*this);
  AutoTransactionsConserveSelection dontChangeMySelection(*this);

  bool cancel, handled;
  EditSubActionInfo subActionInfo(EditSubAction::eRemoveTextProperty);
  // Protect the edit rules object from dying
  RefPtr<TextEditRules> rules(mRules);
  nsresult rv = rules->WillDoAction(subActionInfo, &cancel, &handled);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  if (!cancel && !handled) {
    // Loop through the ranges in the selection
    // Since ranges might be modified by SplitStyleAboveRange, we need hold
    // current ranges
    AutoRangeArray arrayOfRanges(SelectionRefPtr());
    for (auto& range : arrayOfRanges.mRanges) {
      if (aProperty == nsGkAtoms::name) {
        // Promote range if it starts or end in a named anchor and we want to
        // remove named anchors
        rv = PromoteRangeIfStartsOrEndsInNamedAnchor(*range);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
      } else {
        // Adjust range to include any ancestors whose children are entirely
        // selected
        rv = PromoteInlineRange(*range);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
      }

      // Remove this style from ancestors of our range endpoints, splitting
      // them as appropriate
      rv = SplitStyleAboveRange(range, aProperty, aAttribute);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      // Check for easy case: both range endpoints in same text node
      nsCOMPtr<nsINode> startNode = range->GetStartContainer();
      nsCOMPtr<nsINode> endNode = range->GetEndContainer();
      if (startNode && startNode == endNode && startNode->GetAsText()) {
        // We're done with this range!
        if (IsCSSEnabled() && CSSEditUtils::IsCSSEditableProperty(
                                  startNode, aProperty, aAttribute)) {
          // The HTML style defined by aProperty/aAttribute has a CSS
          // equivalence in this implementation for startNode
          if (CSSEditUtils::IsCSSEquivalentToHTMLInlineStyleSet(
                  startNode, aProperty, aAttribute, EmptyString(),
                  CSSEditUtils::eComputed)) {
            // startNode's computed style indicates the CSS equivalence to the
            // HTML style to remove is applied; but we found no element in the
            // ancestors of startNode carrying specified styles; assume it
            // comes from a rule and try to insert a span "inverting" the style
            if (CSSEditUtils::IsCSSInvertible(*aProperty, aAttribute)) {
              NS_NAMED_LITERAL_STRING(value, "-moz-editor-invert-value");
              SetInlinePropertyOnTextNode(
                  *startNode->GetAsText(), range->StartOffset(),
                  range->EndOffset(), *aProperty, aAttribute, value);
            }
          }
        }
      } else {
        // Not the easy case.  Range not contained in single text node.
        nsCOMPtr<nsIContentIterator> iter = NS_NewContentSubtreeIterator();

        nsTArray<OwningNonNull<nsIContent>> arrayOfNodes;

        // Iterate range and build up array
        for (iter->Init(range); !iter->IsDone(); iter->Next()) {
          nsCOMPtr<nsINode> node = iter->GetCurrentNode();
          if (NS_WARN_IF(!node)) {
            return NS_ERROR_FAILURE;
          }
          if (IsEditable(node) && node->IsContent()) {
            arrayOfNodes.AppendElement(*node->AsContent());
          }
        }

        // Loop through the list, remove the property on each node
        for (auto& node : arrayOfNodes) {
          rv = RemoveStyleInside(node, aProperty, aAttribute);
          if (NS_WARN_IF(NS_FAILED(rv))) {
            return rv;
          }
          if (IsCSSEnabled() &&
              CSSEditUtils::IsCSSEditableProperty(node, aProperty,
                                                  aAttribute) &&
              CSSEditUtils::IsCSSEquivalentToHTMLInlineStyleSet(
                  node, aProperty, aAttribute, EmptyString(),
                  CSSEditUtils::eComputed) &&
              // startNode's computed style indicates the CSS equivalence to
              // the HTML style to remove is applied; but we found no element
              // in the ancestors of startNode carrying specified styles;
              // assume it comes from a rule and let's try to insert a span
              // "inverting" the style
              CSSEditUtils::IsCSSInvertible(*aProperty, aAttribute)) {
            NS_NAMED_LITERAL_STRING(value, "-moz-editor-invert-value");
            SetInlinePropertyOnNode(node, *aProperty, aAttribute, value);
          }
        }
      }
    }
  }

  if (cancel) {
    return NS_OK;
  }

  rv = rules->DidDoAction(subActionInfo, rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return NS_OK;
}

NS_IMETHODIMP
HTMLEditor::IncreaseFontSize() {
  AutoEditActionDataSetter editActionData(*this,
                                          EditAction::eIncrementFontSize);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  return RelativeFontChange(FontSize::incr);
}

NS_IMETHODIMP
HTMLEditor::DecreaseFontSize() {
  AutoEditActionDataSetter editActionData(*this,
                                          EditAction::eDecrementFontSize);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  return RelativeFontChange(FontSize::decr);
}

nsresult HTMLEditor::RelativeFontChange(FontSize aDir) {
  MOZ_ASSERT(IsEditActionDataAvailable());

  CommitComposition();

  // If selection is collapsed, set typing state
  if (SelectionRefPtr()->IsCollapsed()) {
    nsAtom& atom = aDir == FontSize::incr ? *nsGkAtoms::big : *nsGkAtoms::small;

    // Let's see in what kind of element the selection is
    if (NS_WARN_IF(!SelectionRefPtr()->RangeCount())) {
      return NS_OK;
    }
    RefPtr<nsRange> firstRange = SelectionRefPtr()->GetRangeAt(0);
    if (NS_WARN_IF(!firstRange) ||
        NS_WARN_IF(!firstRange->GetStartContainer())) {
      return NS_OK;
    }
    OwningNonNull<nsINode> selectedNode = *firstRange->GetStartContainer();
    if (IsTextNode(selectedNode)) {
      if (NS_WARN_IF(!selectedNode->GetParentNode())) {
        return NS_OK;
      }
      selectedNode = *selectedNode->GetParentNode();
    }
    if (!CanContainTag(selectedNode, atom)) {
      return NS_OK;
    }

    // Manipulating text attributes on a collapsed selection only sets state
    // for the next text insertion
    mTypeInState->SetProp(&atom, nullptr, EmptyString());
    return NS_OK;
  }

  // Wrap with txn batching, rules sniffing, and selection preservation code
  AutoPlaceholderBatch treatAsOneTransaction(*this);
  AutoTopLevelEditSubActionNotifier maybeTopLevelEditSubAction(
      *this, EditSubAction::eSetTextProperty, nsIEditor::eNext);
  AutoSelectionRestorer restoreSelectionLater(*this);
  AutoTransactionsConserveSelection dontChangeMySelection(*this);

  // Loop through the ranges in the selection
  AutoRangeArray arrayOfRanges(SelectionRefPtr());
  for (auto& range : arrayOfRanges.mRanges) {
    // Adjust range to include any ancestors with entirely selected children
    nsresult rv = PromoteInlineRange(*range);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    // Check for easy case: both range endpoints in same text node
    nsCOMPtr<nsINode> startNode = range->GetStartContainer();
    nsCOMPtr<nsINode> endNode = range->GetEndContainer();
    if (startNode == endNode && IsTextNode(startNode)) {
      rv = RelativeFontChangeOnTextNode(aDir, *startNode->GetAsText(),
                                        range->StartOffset(),
                                        range->EndOffset());
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    } else {
      // Not the easy case.  Range not contained in single text node.  There
      // are up to three phases here.  There are all the nodes reported by the
      // subtree iterator to be processed.  And there are potentially a
      // starting textnode and an ending textnode which are only partially
      // contained by the range.

      // Let's handle the nodes reported by the iterator.  These nodes are
      // entirely contained in the selection range.  We build up a list of them
      // (since doing operations on the document during iteration would perturb
      // the iterator).

      OwningNonNull<nsIContentIterator> iter = NS_NewContentSubtreeIterator();

      // Iterate range and build up array
      rv = iter->Init(range);
      if (NS_SUCCEEDED(rv)) {
        nsTArray<OwningNonNull<nsIContent>> arrayOfNodes;
        for (; !iter->IsDone(); iter->Next()) {
          if (NS_WARN_IF(!iter->GetCurrentNode()->IsContent())) {
            return NS_ERROR_FAILURE;
          }
          OwningNonNull<nsIContent> node = *iter->GetCurrentNode()->AsContent();

          if (IsEditable(node)) {
            arrayOfNodes.AppendElement(node);
          }
        }

        // Now that we have the list, do the font size change on each node
        for (auto& node : arrayOfNodes) {
          rv = RelativeFontChangeOnNode(aDir == FontSize::incr ? +1 : -1, node);
          if (NS_WARN_IF(NS_FAILED(rv))) {
            return rv;
          }
        }
      }
      // Now check the start and end parents of the range to see if they need
      // to be separately handled (they do if they are text nodes, due to how
      // the subtree iterator works - it will not have reported them).
      if (IsTextNode(startNode) && IsEditable(startNode)) {
        rv = RelativeFontChangeOnTextNode(aDir, *startNode->GetAsText(),
                                          range->StartOffset(),
                                          startNode->Length());
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
      }
      if (IsTextNode(endNode) && IsEditable(endNode)) {
        rv = RelativeFontChangeOnTextNode(aDir, *endNode->GetAsText(), 0,
                                          range->EndOffset());
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
      }
    }
  }

  return NS_OK;
}

nsresult HTMLEditor::RelativeFontChangeOnTextNode(FontSize aDir,
                                                  Text& aTextNode,
                                                  int32_t aStartOffset,
                                                  int32_t aEndOffset) {
  // Don't need to do anything if no characters actually selected
  if (aStartOffset == aEndOffset) {
    return NS_OK;
  }

  if (!aTextNode.GetParentNode() ||
      !CanContainTag(*aTextNode.GetParentNode(), *nsGkAtoms::big)) {
    return NS_OK;
  }

  // -1 is a magic value meaning to the end of node
  if (aEndOffset == -1) {
    aEndOffset = aTextNode.Length();
  }

  // Make the range an independent node.
  nsCOMPtr<nsIContent> textNodeForTheRange = &aTextNode;

  // Split at the end of the range.
  EditorRawDOMPoint atEnd(textNodeForTheRange, aEndOffset);
  if (!atEnd.IsEndOfContainer()) {
    // We need to split off back of text node
    ErrorResult error;
    textNodeForTheRange = SplitNodeWithTransaction(atEnd, error);
    if (NS_WARN_IF(error.Failed())) {
      return error.StealNSResult();
    }
  }

  // Split at the start of the range.
  EditorRawDOMPoint atStart(textNodeForTheRange, aStartOffset);
  if (!atStart.IsStartOfContainer()) {
    // We need to split off front of text node
    ErrorResult error;
    nsCOMPtr<nsIContent> newLeftNode = SplitNodeWithTransaction(atStart, error);
    if (NS_WARN_IF(error.Failed())) {
      return error.StealNSResult();
    }
    Unused << newLeftNode;
  }

  // Look for siblings that are correct type of node
  nsAtom* nodeType = aDir == FontSize::incr ? nsGkAtoms::big : nsGkAtoms::small;
  nsCOMPtr<nsIContent> sibling = GetPriorHTMLSibling(textNodeForTheRange);
  if (sibling && sibling->IsHTMLElement(nodeType)) {
    // Previous sib is already right kind of inline node; slide this over
    nsresult rv = MoveNodeToEndWithTransaction(*textNodeForTheRange, *sibling);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    return NS_OK;
  }
  sibling = GetNextHTMLSibling(textNodeForTheRange);
  if (sibling && sibling->IsHTMLElement(nodeType)) {
    // Following sib is already right kind of inline node; slide this over
    nsresult rv = MoveNodeWithTransaction(*textNodeForTheRange,
                                          EditorRawDOMPoint(sibling, 0));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    return NS_OK;
  }

  // Else reparent the node inside font node with appropriate relative size
  RefPtr<Element> newElement =
      InsertContainerWithTransaction(*textNodeForTheRange, *nodeType);
  if (NS_WARN_IF(!newElement)) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

nsresult HTMLEditor::RelativeFontChangeHelper(int32_t aSizeChange,
                                              nsINode* aNode) {
  MOZ_ASSERT(aNode);

  /*  This routine looks for all the font nodes in the tree rooted by aNode,
      including aNode itself, looking for font nodes that have the size attr
      set.  Any such nodes need to have big or small put inside them, since
      they override any big/small that are above them.
  */

  // Can only change font size by + or - 1
  if (aSizeChange != 1 && aSizeChange != -1) {
    return NS_ERROR_ILLEGAL_VALUE;
  }

  // If this is a font node with size, put big/small inside it.
  if (aNode->IsHTMLElement(nsGkAtoms::font) &&
      aNode->AsElement()->HasAttr(kNameSpaceID_None, nsGkAtoms::size)) {
    // Cycle through children and adjust relative font size.
    AutoTArray<nsCOMPtr<nsIContent>, 10> childList;
    for (nsIContent* child = aNode->GetFirstChild(); child;
         child = child->GetNextSibling()) {
      childList.AppendElement(child);
    }

    for (const auto& child : childList) {
      nsresult rv = RelativeFontChangeOnNode(aSizeChange, child);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    // RelativeFontChangeOnNode already calls us recursively,
    // so we don't need to check our children again.
    return NS_OK;
  }

  // Otherwise cycle through the children.
  AutoTArray<nsCOMPtr<nsIContent>, 10> childList;
  for (nsIContent* child = aNode->GetFirstChild(); child;
       child = child->GetNextSibling()) {
    childList.AppendElement(child);
  }

  for (const auto& child : childList) {
    nsresult rv = RelativeFontChangeHelper(aSizeChange, child);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

nsresult HTMLEditor::RelativeFontChangeOnNode(int32_t aSizeChange,
                                              nsIContent* aNode) {
  MOZ_ASSERT(aNode);
  // Can only change font size by + or - 1
  if (aSizeChange != 1 && aSizeChange != -1) {
    return NS_ERROR_ILLEGAL_VALUE;
  }

  nsAtom* atom;
  if (aSizeChange == 1) {
    atom = nsGkAtoms::big;
  } else {
    atom = nsGkAtoms::small;
  }

  // Is it the opposite of what we want?
  if ((aSizeChange == 1 && aNode->IsHTMLElement(nsGkAtoms::small)) ||
      (aSizeChange == -1 && aNode->IsHTMLElement(nsGkAtoms::big))) {
    // first populate any nested font tags that have the size attr set
    nsresult rv = RelativeFontChangeHelper(aSizeChange, aNode);
    NS_ENSURE_SUCCESS(rv, rv);
    // in that case, just remove this node and pull up the children
    return RemoveContainerWithTransaction(*aNode->AsElement());
  }

  // can it be put inside a "big" or "small"?
  if (TagCanContain(*atom, *aNode)) {
    // first populate any nested font tags that have the size attr set
    nsresult rv = RelativeFontChangeHelper(aSizeChange, aNode);
    NS_ENSURE_SUCCESS(rv, rv);

    // ok, chuck it in.
    // first look at siblings of aNode for matching bigs or smalls.
    // if we find one, move aNode into it.
    nsIContent* sibling = GetPriorHTMLSibling(aNode);
    if (sibling && sibling->IsHTMLElement(atom)) {
      // previous sib is already right kind of inline node; slide this over into
      // it
      return MoveNodeToEndWithTransaction(*aNode, *sibling);
    }

    sibling = GetNextHTMLSibling(aNode);
    if (sibling && sibling->IsHTMLElement(atom)) {
      // following sib is already right kind of inline node; slide this over
      // into it
      return MoveNodeWithTransaction(*aNode, EditorRawDOMPoint(sibling, 0));
    }

    // else insert it above aNode
    RefPtr<Element> newElement = InsertContainerWithTransaction(*aNode, *atom);
    if (NS_WARN_IF(!newElement)) {
      return NS_ERROR_FAILURE;
    }
    return NS_OK;
  }

  // none of the above?  then cycle through the children.
  // MOOSE: we should group the children together if possible
  // into a single "big" or "small".  For the moment they are
  // each getting their own.
  AutoTArray<nsCOMPtr<nsIContent>, 10> childList;
  for (nsIContent* child = aNode->GetFirstChild(); child;
       child = child->GetNextSibling()) {
    childList.AppendElement(child);
  }

  for (const auto& child : childList) {
    nsresult rv = RelativeFontChangeOnNode(aSizeChange, child);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

NS_IMETHODIMP
HTMLEditor::GetFontFaceState(bool* aMixed, nsAString& outFace) {
  if (NS_WARN_IF(!aMixed)) {
    return NS_ERROR_INVALID_ARG;
  }

  *aMixed = true;
  outFace.Truncate();

  AutoEditActionDataSetter editActionData(*this, EditAction::eNotEditing);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  bool first, any, all;

  nsresult rv = GetInlinePropertyBase(*nsGkAtoms::font, nsGkAtoms::face,
                                      nullptr, &first, &any, &all, &outFace);
  NS_ENSURE_SUCCESS(rv, rv);
  if (any && !all) {
    return NS_OK;  // mixed
  }
  if (all) {
    *aMixed = false;
    return NS_OK;
  }

  // if there is no font face, check for tt
  rv = GetInlinePropertyBase(*nsGkAtoms::tt, nullptr, nullptr, &first, &any,
                             &all, nullptr);
  NS_ENSURE_SUCCESS(rv, rv);
  if (any && !all) {
    return rv;  // mixed
  }
  if (all) {
    *aMixed = false;
    outFace.AssignLiteral("tt");
  }

  if (!any) {
    // there was no font face attrs of any kind.  We are in normal font.
    outFace.Truncate();
    *aMixed = false;
  }
  return NS_OK;
}

nsresult HTMLEditor::GetFontColorState(bool* aMixed, nsAString& aOutColor) {
  if (NS_WARN_IF(!aMixed)) {
    return NS_ERROR_INVALID_ARG;
  }

  *aMixed = true;
  aOutColor.Truncate();

  AutoEditActionDataSetter editActionData(*this, EditAction::eNotEditing);
  if (NS_WARN_IF(!editActionData.CanHandle())) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  bool first, any, all;
  nsresult rv = GetInlinePropertyBase(*nsGkAtoms::font, nsGkAtoms::color,
                                      nullptr, &first, &any, &all, &aOutColor);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (any && !all) {
    return NS_OK;  // mixed
  }
  if (all) {
    *aMixed = false;
    return NS_OK;
  }

  if (!any) {
    // there was no font color attrs of any kind..
    aOutColor.Truncate();
    *aMixed = false;
  }
  return NS_OK;
}

// the return value is true only if the instance of the HTML editor we created
// can handle CSS styles (for instance, Composer can, Messenger can't) and if
// the CSS preference is checked
NS_IMETHODIMP
HTMLEditor::GetIsCSSEnabled(bool* aIsCSSEnabled) {
  *aIsCSSEnabled = IsCSSEnabled();
  return NS_OK;
}

static bool HasNonEmptyAttribute(Element* aElement, nsAtom* aName) {
  MOZ_ASSERT(aElement);

  nsAutoString value;
  return aElement->GetAttr(kNameSpaceID_None, aName, value) && !value.IsEmpty();
}

bool HTMLEditor::HasStyleOrIdOrClass(Element* aElement) {
  MOZ_ASSERT(aElement);

  // remove the node if its style attribute is empty or absent,
  // and if it does not have a class nor an id
  return HasNonEmptyAttribute(aElement, nsGkAtoms::style) ||
         HasNonEmptyAttribute(aElement, nsGkAtoms::_class) ||
         HasNonEmptyAttribute(aElement, nsGkAtoms::id);
}

nsresult HTMLEditor::RemoveElementIfNoStyleOrIdOrClass(Element& aElement) {
  // early way out if node is not the right kind of element
  if ((!aElement.IsHTMLElement(nsGkAtoms::span) &&
       !aElement.IsHTMLElement(nsGkAtoms::font)) ||
      HasStyleOrIdOrClass(&aElement)) {
    return NS_OK;
  }

  return RemoveContainerWithTransaction(aElement);
}

}  // namespace mozilla
