/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RangeUtils.h"

#include "mozilla/Assertions.h"
#include "mozilla/dom/AbstractRange.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/ShadowRoot.h"
#include "mozilla/dom/HTMLSlotElement.h"
#include "nsContentUtils.h"
#include "nsFrameSelection.h"

namespace mozilla {

using namespace dom;

template bool RangeUtils::IsValidPoints(const RangeBoundary& aStartBoundary,
                                        const RangeBoundary& aEndBoundary);
template bool RangeUtils::IsValidPoints(const RangeBoundary& aStartBoundary,
                                        const RawRangeBoundary& aEndBoundary);
template bool RangeUtils::IsValidPoints(const RawRangeBoundary& aStartBoundary,
                                        const RangeBoundary& aEndBoundary);
template bool RangeUtils::IsValidPoints(const RawRangeBoundary& aStartBoundary,
                                        const RawRangeBoundary& aEndBoundary);

template nsresult
RangeUtils::CompareNodeToRangeBoundaries<TreeKind::ShadowIncludingDOM>(
    nsINode* aNode, const RangeBoundary& aStartBoundary,
    const RangeBoundary& aEndBoundary, bool* aNodeIsBeforeRange,
    bool* aNodeIsAfterRange);
template nsresult RangeUtils::CompareNodeToRangeBoundaries<TreeKind::Flat>(
    nsINode* aNode, const RangeBoundary& aStartBoundary,
    const RangeBoundary& aEndBoundary, bool* aNodeIsBeforeRange,
    bool* aNodeIsAfterRange);

template nsresult
RangeUtils::CompareNodeToRangeBoundaries<TreeKind::ShadowIncludingDOM>(
    nsINode* aNode, const RangeBoundary& aStartBoundary,
    const RawRangeBoundary& aEndBoundary, bool* aNodeIsBeforeRange,
    bool* aNodeIsAfterRange);
template nsresult RangeUtils::CompareNodeToRangeBoundaries<TreeKind::Flat>(
    nsINode* aNode, const RangeBoundary& aStartBoundary,
    const RawRangeBoundary& aEndBoundary, bool* aNodeIsBeforeRange,
    bool* aNodeIsAfterRange);

template nsresult
RangeUtils::CompareNodeToRangeBoundaries<TreeKind::ShadowIncludingDOM>(
    nsINode* aNode, const RawRangeBoundary& aStartBoundary,
    const RangeBoundary& aEndBoundary, bool* aNodeIsBeforeRange,
    bool* aNodeIsAfterRange);
template nsresult RangeUtils::CompareNodeToRangeBoundaries<TreeKind::Flat>(
    nsINode* aNode, const RawRangeBoundary& aStartBoundary,
    const RangeBoundary& aEndBoundary, bool* aNodeIsBeforeRange,
    bool* aNodeIsAfterRange);

template nsresult
RangeUtils::CompareNodeToRangeBoundaries<TreeKind::ShadowIncludingDOM>(
    nsINode* aNode, const RawRangeBoundary& aStartBoundary,
    const RawRangeBoundary& aEndBoundary, bool* aNodeIsBeforeRange,
    bool* aNodeIsAfterRange);
template nsresult RangeUtils::CompareNodeToRangeBoundaries<TreeKind::Flat>(
    nsINode* aNode, const RawRangeBoundary& aStartBoundary,
    const RawRangeBoundary& aEndBoundary, bool* aNodeIsBeforeRange,
    bool* aNodeIsAfterRange);

template nsresult RangeUtils::CompareNodeToRange<TreeKind::ShadowIncludingDOM>(
    nsINode* aNode, AbstractRange* aAbstractRange, bool* aNodeIsBeforeRange,
    bool* aNodeIsAfterRange);
template nsresult RangeUtils::CompareNodeToRange<TreeKind::Flat>(
    nsINode* aNode, AbstractRange* aAbstractRange, bool* aNodeIsBeforeRange,
    bool* aNodeIsAfterRange);

template Maybe<bool>
RangeUtils::IsNodeContainedInRange<TreeKind::ShadowIncludingDOM>(
    nsINode& aNode, AbstractRange* aAbstractRange);
template Maybe<bool> RangeUtils::IsNodeContainedInRange<TreeKind::Flat>(
    nsINode& aNode, AbstractRange* aAbstractRange);

[[nodiscard]] static inline bool ParentNodeIsInSameSelection(
    const nsINode& aNode) {
  // Currently, independent selection root is always the anonymous <div> in a
  // text control which is an native anonymous subtree root.  Therefore, we
  // can skip most checks if the node is not a root of native anonymous subtree.
  if (!aNode.IsRootOfNativeAnonymousSubtree()) {
    return true;
  }
  // If the node returns nullptr for frame selection, it means that it's not the
  // anonymous <div> of the editable content root of a text control or just not
  // in composed doc.
  const nsFrameSelection* frameSelection = aNode.GetFrameSelection();
  if (!frameSelection || frameSelection->IsIndependentSelection()) {
    MOZ_ASSERT_IF(aNode.GetClosestNativeAnonymousSubtreeRootParentOrHost(),
                  aNode.GetClosestNativeAnonymousSubtreeRootParentOrHost()
                      ->IsTextControlElement());
    return false;
  }
  return true;
}

// static
nsINode* RangeUtils::ComputeRootNode(nsINode* aNode) {
  if (!aNode) {
    return nullptr;
  }

  if (aNode->IsContent()) {
    if (aNode->NodeInfo()->NameAtom() == nsGkAtoms::documentTypeNodeName) {
      return nullptr;
    }

    nsIContent* content = aNode->AsContent();

    // If the node is in a shadow tree then the ShadowRoot is the root.
    //
    // FIXME(emilio): Should this be after the NAC check below? We can have NAC
    // inside Shadow DOM which will peek this path rather than the one below.
    if (ShadowRoot* containingShadow = content->GetContainingShadow()) {
      return containingShadow;
    }

    // If the node is in NAC, then the NAC parent should be the root.
    if (nsINode* root =
            content->GetClosestNativeAnonymousSubtreeRootParentOrHost()) {
      return root;
    }
  }

  // Elements etc. must be in document or in document fragment,
  // text nodes in document, in document fragment or in attribute.
  if (nsINode* root = aNode->GetUncomposedDoc()) {
    return root;
  }

  NS_ASSERTION(!aNode->SubtreeRoot()->IsDocument(),
               "GetUncomposedDoc should have returned a doc");

  // We allow this because of backward compatibility.
  return aNode->SubtreeRoot();
}

// static
template <typename SPT, typename SRT, typename EPT, typename ERT>
bool RangeUtils::IsValidPoints(
    const RangeBoundaryBase<SPT, SRT>& aStartBoundary,
    const RangeBoundaryBase<EPT, ERT>& aEndBoundary) {
  // Use NS_WARN_IF() only for the cases where the arguments are unexpected.
  if (NS_WARN_IF(!aStartBoundary.IsSetAndValid()) ||
      NS_WARN_IF(!aEndBoundary.IsSetAndValid())) {
    return false;
  }

  MOZ_ASSERT(aStartBoundary.GetTreeKind() == aEndBoundary.GetTreeKind());

  // Otherwise, don't use NS_WARN_IF() for preventing to make console messy.
  // Instead, check one by one since it is easier to catch the error reason
  // with debugger.

  if (ComputeRootNode(aStartBoundary.GetContainer()) !=
      ComputeRootNode(aEndBoundary.GetContainer())) {
    return false;
  }

  const Maybe<int32_t> order =
      nsContentUtils::ComparePoints(aStartBoundary, aEndBoundary);
  if (!order) {
    MOZ_ASSERT_UNREACHABLE();
    return false;
  }

  return *order != 1;
}

// static
template <TreeKind aKind, typename Dummy>
Maybe<bool> RangeUtils::IsNodeContainedInRange(nsINode& aNode,
                                               AbstractRange* aAbstractRange) {
  bool nodeIsBeforeRange{false};
  bool nodeIsAfterRange{false};

  const nsresult rv = CompareNodeToRange<aKind>(
      &aNode, aAbstractRange, &nodeIsBeforeRange, &nodeIsAfterRange);
  if (NS_FAILED(rv)) {
    return Nothing();
  }

  return Some(!nodeIsBeforeRange && !nodeIsAfterRange);
}

// Utility routine to detect if a content node is completely contained in a
// range If outNodeBefore is returned true, then the node starts before the
// range does. If outNodeAfter is returned true, then the node ends after the
// range does. Note that both of the above might be true. If neither are true,
// the node is contained inside of the range.
// XXX - callers responsibility to ensure node in same doc as range!

// static
template <TreeKind aKind, typename Dummy>
nsresult RangeUtils::CompareNodeToRange(nsINode* aNode,
                                        AbstractRange* aAbstractRange,
                                        bool* aNodeIsBeforeRange,
                                        bool* aNodeIsAfterRange) {
  if (NS_WARN_IF(!aAbstractRange) ||
      NS_WARN_IF(!aAbstractRange->IsPositioned())) {
    return NS_ERROR_INVALID_ARG;
  }
  return CompareNodeToRangeBoundaries<aKind>(
      aNode, aAbstractRange->MayCrossShadowBoundaryStartRef(),
      aAbstractRange->MayCrossShadowBoundaryEndRef(), aNodeIsBeforeRange,
      aNodeIsAfterRange);
}
template <TreeKind aKind, typename SPT, typename SRT, typename EPT,
          typename ERT, typename Dummy>
nsresult RangeUtils::CompareNodeToRangeBoundaries(
    nsINode* aNode, const RangeBoundaryBase<SPT, SRT>& aStartBoundary,
    const RangeBoundaryBase<EPT, ERT>& aEndBoundary, bool* aNodeIsBeforeRange,
    bool* aNodeIsAfterRange) {
  MOZ_ASSERT(aNodeIsBeforeRange);
  MOZ_ASSERT(aNodeIsAfterRange);
  MOZ_ASSERT(aStartBoundary.GetTreeKind() == aEndBoundary.GetTreeKind());

  if (NS_WARN_IF(!aNode) ||
      NS_WARN_IF(!aStartBoundary.IsSet() || !aEndBoundary.IsSet())) {
    return NS_ERROR_INVALID_ARG;
  }

  // create a pair of dom points that expresses location of node:
  //     NODE(start), NODE(end)
  // Let incoming range be:
  //    {RANGE(start), RANGE(end)}
  // if (RANGE(start) <= NODE(start))  and (RANGE(end) => NODE(end))
  // then the Node is contained (completely) by the Range.

  // gather up the dom point info
  int32_t nodeStart;
  uint32_t nodeEnd;
  const nsINode* parent = nullptr;

  MOZ_ASSERT_IF(aKind == TreeKind::Flat,
                StaticPrefs::dom_shadowdom_selection_across_boundary_enabled());
  // ShadowRoot has no parent, nor can be represented by parent/offset pair.
  if (!aNode->IsShadowRoot()) {
    parent = ShadowDOMSelectionHelpers::GetParentNodeInSameSelection(
        *aNode, aKind == TreeKind::Flat ? AllowRangeCrossShadowBoundary::Yes
                                        : AllowRangeCrossShadowBoundary::No);
  }

  if (!parent) {
    // can't make a parent/offset pair to represent start or
    // end of the root node, because it has no parent.
    // so instead represent it by (node,0) and (node,numChildren)
    parent = aNode;
    nodeStart = 0;
    nodeEnd = aNode->GetChildCount();
  } else if (const HTMLSlotElement* slotAsParent =
                 HTMLSlotElement::FromNode(parent);
             slotAsParent && aKind == TreeKind::Flat) {
    // aNode is a slotted content, use the index in the assigned nodes
    // to represent this node.
    auto index = slotAsParent->AssignedNodes().IndexOf(aNode);
    nodeStart = index;
    nodeEnd = nodeStart + 1;
  } else {
    nodeStart = parent->ComputeIndexOf_Deprecated(aNode);
    NS_WARNING_ASSERTION(
        nodeStart >= 0,
        "aNode has the parent node but it does not have aNode!");
    nodeEnd = nodeStart + 1u;
    MOZ_ASSERT(nodeStart < 0 || static_cast<uint32_t>(nodeStart) < nodeEnd,
               "nodeStart should be less than nodeEnd");
  }

  // XXX nsContentUtils::ComparePoints() may be expensive.  If some callers
  //     just want one of aNodeIsBeforeRange or aNodeIsAfterRange, we can
  //     skip the other comparison.

  // In the ComparePoints calls below we use a container & offset instead of
  // a range boundary because the range boundary constructor warns if you pass
  // in a -1 offset and the ComputeIndexOf call above can return -1 if aNode
  // is native anonymous content. ComparePoints has comments about offsets
  // being -1 and it seems to deal with it, or at least we aren't aware of any
  // problems arising because of it. We don't have a better idea how to get
  // rid of the warning without much larger changes so we do this just to
  // silence the warning. (Bug 1438996)

  // is RANGE(start) <= NODE(start) ?
  Maybe<int32_t> order =
      nsContentUtils::ComparePoints_AllowNegativeOffsets<aKind>(
          aStartBoundary.GetContainer(),
          *aStartBoundary.Offset(
              RangeBoundaryBase<SPT,
                                SRT>::OffsetFilter::kValidOrInvalidOffsets),
          parent, nodeStart);
  if (NS_WARN_IF(!order)) {
    return NS_ERROR_DOM_WRONG_DOCUMENT_ERR;
  }
  *aNodeIsBeforeRange = *order > 0;
  // is RANGE(end) >= NODE(end) ?
  order = nsContentUtils::ComparePointsWithIndices<aKind>(
      aEndBoundary.GetContainer(),
      *aEndBoundary.Offset(
          RangeBoundaryBase<EPT, ERT>::OffsetFilter::kValidOrInvalidOffsets),
      parent, nodeEnd);

  if (NS_WARN_IF(!order)) {
    return NS_ERROR_DOM_WRONG_DOCUMENT_ERR;
  }
  *aNodeIsAfterRange = *order < 0;

  return NS_OK;
}

// static
nsINode* ShadowDOMSelectionHelpers::GetStartContainer(
    const AbstractRange* aRange,
    AllowRangeCrossShadowBoundary aAllowCrossShadowBoundary) {
  MOZ_ASSERT(aRange);
  return (StaticPrefs::dom_shadowdom_selection_across_boundary_enabled() &&
          aAllowCrossShadowBoundary == AllowRangeCrossShadowBoundary::Yes)
             ? aRange->GetMayCrossShadowBoundaryStartContainer()
             : aRange->GetStartContainer();
}

// static
uint32_t ShadowDOMSelectionHelpers::StartOffset(
    const AbstractRange* aRange,
    AllowRangeCrossShadowBoundary aAllowCrossShadowBoundary) {
  MOZ_ASSERT(aRange);
  return (StaticPrefs::dom_shadowdom_selection_across_boundary_enabled() &&
          aAllowCrossShadowBoundary == AllowRangeCrossShadowBoundary::Yes)
             ? aRange->MayCrossShadowBoundaryStartOffset()
             : aRange->StartOffset();
}

// static
nsINode* ShadowDOMSelectionHelpers::GetEndContainer(
    const AbstractRange* aRange,
    AllowRangeCrossShadowBoundary aAllowCrossShadowBoundary) {
  MOZ_ASSERT(aRange);
  return (StaticPrefs::dom_shadowdom_selection_across_boundary_enabled() &&
          aAllowCrossShadowBoundary == AllowRangeCrossShadowBoundary::Yes)
             ? aRange->GetMayCrossShadowBoundaryEndContainer()
             : aRange->GetEndContainer();
}

// static
uint32_t ShadowDOMSelectionHelpers::EndOffset(
    const AbstractRange* aRange,
    AllowRangeCrossShadowBoundary aAllowCrossShadowBoundary) {
  MOZ_ASSERT(aRange);
  return (StaticPrefs::dom_shadowdom_selection_across_boundary_enabled() &&
          aAllowCrossShadowBoundary == AllowRangeCrossShadowBoundary::Yes)
             ? aRange->MayCrossShadowBoundaryEndOffset()
             : aRange->EndOffset();
}

// static
nsINode* ShadowDOMSelectionHelpers::GetParentNodeInSameSelection(
    const nsINode& aNode,
    AllowRangeCrossShadowBoundary aAllowCrossShadowBoundary) {
  if (!ParentNodeIsInSameSelection(aNode)) {
    return nullptr;
  }

  if (StaticPrefs::dom_shadowdom_selection_across_boundary_enabled() &&
      aAllowCrossShadowBoundary == AllowRangeCrossShadowBoundary::Yes) {
    if (aNode.IsContent()) {
      if (HTMLSlotElement* slot = aNode.AsContent()->GetAssignedSlot();
          slot && GetShadowRoot(slot->GetContainingShadowHost(),
                                aAllowCrossShadowBoundary)) {
        return slot;
      }
    }
    return aNode.GetParentOrShadowHostNode();
  }
  return aNode.GetParentNode();
}

// static
ShadowRoot* ShadowDOMSelectionHelpers::GetShadowRoot(
    const nsINode* aNode,
    AllowRangeCrossShadowBoundary aAllowCrossShadowBoundary) {
  MOZ_ASSERT(aNode);
  return (StaticPrefs::dom_shadowdom_selection_across_boundary_enabled() &&
          aAllowCrossShadowBoundary == AllowRangeCrossShadowBoundary::Yes)
             ? aNode->GetShadowRootForSelection()
             : nullptr;
}  // namespace dom

}  // namespace mozilla
