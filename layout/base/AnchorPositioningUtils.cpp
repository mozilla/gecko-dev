/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AnchorPositioningUtils.h"

#include "mozilla/dom/Document.h"
#include "mozilla/dom/Element.h"
#include "mozilla/PresShell.h"
#include "nsCanvasFrame.h"
#include "nsContainerFrame.h"
#include "nsLayoutUtils.h"
#include "nsPlaceholderFrame.h"
#include "nsIContent.h"
#include "nsIFrame.h"
#include "nsIFrameInlines.h"
#include "nsINode.h"
#include "nsTArray.h"

namespace mozilla {

namespace {

bool IsAnchorInScopeForPositionedElement(
    const nsIFrame* /* aPossibleAnchorFrame */,
    const nsIFrame* /* aPositionedFrame */) {
  return true;  // TODO: Implement this check. For now, always in scope.
};

bool IsFullyStyleableTreeAbidingOrNotPseudoElement(const nsIFrame* aFrame) {
  if (!aFrame->Style()->IsPseudoElement()) {
    return true;
  }

  const PseudoStyleType pseudoElementType = aFrame->Style()->GetPseudoType();

  // See https://www.w3.org/TR/css-pseudo-4/#treelike
  return pseudoElementType == PseudoStyleType::before ||
         pseudoElementType == PseudoStyleType::after ||
         pseudoElementType == PseudoStyleType::marker;
}

size_t GetTopLayerIndex(const nsIFrame* aFrame) {
  MOZ_ASSERT(aFrame);

  const nsIContent* frameContent = aFrame->GetContent();

  if (!frameContent) {
    return 0;
  }

  // Within the array returned by Document::GetTopLayer,
  // a higher index means the layer sits higher in the stack,
  // matching Document::GetTopLayerTop()’s top-to-bottom logic.
  // See https://drafts.csswg.org/css-position-4/#in-a-higher-top-layer
  const nsTArray<dom::Element*>& topLayers =
      frameContent->OwnerDoc()->GetTopLayer();

  for (size_t index = 0; index < topLayers.Length(); ++index) {
    const auto& topLayer = topLayers.ElementAt(index);
    if (nsContentUtils::ContentIsFlattenedTreeDescendantOfForStyle(
            /* aPossibleDescendant */ frameContent,
            /* aPossibleAncestor */ topLayer)) {
      return 1 + index;
    }
  }

  return 0;
}

bool IsInitialContainingBlock(const nsIFrame* aContainingBlock) {
  // Initial containing block: The containing block of the root element.
  // https://drafts.csswg.org/css-display-4/#initial-containing-block
  return aContainingBlock == aContainingBlock->PresShell()
                                 ->FrameConstructor()
                                 ->GetDocElementContainingBlock();
}

bool IsContainingBlockGeneratedByElement(const nsIFrame* aContainingBlock) {
  // 2.1. Containing Blocks of Positioned Boxes
  // https://www.w3.org/TR/css-position-3/#def-cb
  return !(!aContainingBlock || aContainingBlock->IsViewportFrame() ||
           IsInitialContainingBlock(aContainingBlock));
}

bool IsAnchorLaidOutStrictlyBeforeElement(const nsIFrame* aPossibleAnchorFrame,
                                          const nsIFrame* aPositionedFrame) {
  // 1. positioned el is in a higher top layer than possible anchor,
  // see https://drafts.csswg.org/css-position-4/#in-a-higher-top-layer
  const size_t positionedTopLayerIndex = GetTopLayerIndex(aPositionedFrame);
  const size_t anchorTopLayerIndex = GetTopLayerIndex(aPossibleAnchorFrame);

  if (anchorTopLayerIndex != positionedTopLayerIndex) {
    return anchorTopLayerIndex < positionedTopLayerIndex;
  }

  // Note: The containing block of an absolutely positioned element
  // is just the parent frame.
  const nsIFrame* positionedContainingBlock = aPositionedFrame->GetParent();
  const nsIFrame* anchorContainingBlock =
      aPossibleAnchorFrame->GetContainingBlock();

  // 2. Both elements are in the same top layer but have different
  // containing blocks and positioned el's containing block is an
  // ancestor of possible anchor's containing block in the containing
  // block chain, aka one of the following:
  if (anchorContainingBlock != positionedContainingBlock) {
    // 2.1 positioned el's containing block is the viewport, and
    // possible anchor's containing block isn't.
    if (positionedContainingBlock->IsViewportFrame() &&
        !anchorContainingBlock->IsViewportFrame()) {
      return true;
    }

    auto isLastContainingBlockOrderable =
        [&aPositionedFrame, &anchorContainingBlock,
         &positionedContainingBlock]() -> bool {
      const nsIFrame* it = anchorContainingBlock;
      while (it) {
        const nsIFrame* parentContainingBlock = it->GetContainingBlock();
        if (!parentContainingBlock) {
          return false;
        }

        if (parentContainingBlock == positionedContainingBlock) {
          return !parentContainingBlock->IsAbsPosContainingBlock() ||
                 nsLayoutUtils::CompareTreePosition(
                     parentContainingBlock, aPositionedFrame, nullptr) < 0;
        }

        it = parentContainingBlock;
      }

      return false;
    };

    // 2.2 positioned el's containing block is the initial containing
    // block, and possible anchor's containing block is generated by an
    // element, and the last containing block in possible anchor's containing
    // block chain before reaching positioned el's containing block is either
    // not absolutely positioned or precedes positioned el in the tree order,
    const bool isAnchorContainingBlockGenerated =
        IsContainingBlockGeneratedByElement(anchorContainingBlock);
    if (isAnchorContainingBlockGenerated &&
        IsInitialContainingBlock(positionedContainingBlock)) {
      return isLastContainingBlockOrderable();
    }

    // 2.3 both elements' containing blocks are generated by elements,
    // and positioned el's containing block is an ancestor in the flat
    // tree to that of possible anchor's containing block, and the last
    // containing block in possible anchor’s containing block chain before
    // reaching positioned el’s containing block is either not absolutely
    // positioned or precedes positioned el in the tree order.
    if (isAnchorContainingBlockGenerated &&
        IsContainingBlockGeneratedByElement(positionedContainingBlock)) {
      return isLastContainingBlockOrderable();
    }

    return false;
  }

  // 3. Both elements are in the same top layer and have the same
  // containing block, and are both absolutely positioned, and possible
  // anchor is earlier in flat tree order than positioned el.
  const bool isAnchorAbsolutelyPositioned =
      aPossibleAnchorFrame->IsAbsolutelyPositioned();
  if (isAnchorAbsolutelyPositioned) {
    // We must have checked that the positioned element is absolutely
    // positioned by now.
    return nsLayoutUtils::CompareTreePosition(aPossibleAnchorFrame,
                                              aPositionedFrame, nullptr) < 0;
  }

  // 4. Both elements are in the same top layer and have the same
  // containing block, but possible anchor isn't absolutely positioned.
  return !isAnchorAbsolutelyPositioned;
}

bool IsPositionedElementAlsoSkippedWhenAnchorIsSkipped(
    const nsIFrame* /* aPossibleAnchorFrame */,
    const nsIFrame* /* aPositionedFrame */) {
  return true;  // TODO: Implement this check. For now, always true.
}

bool IsAcceptableAnchorElement(const nsIFrame* aPossibleAnchorFrame,
                               const nsIFrame* aPositionedFrame) {
  MOZ_ASSERT(aPossibleAnchorFrame);
  MOZ_ASSERT(aPositionedFrame);

  // An element possible anchor is an acceptable anchor element for an
  // absolutely positioned element positioned el if all of the following are
  // true:
  // - possible anchor is either an element or a fully styleable
  // tree-abiding pseudo-element.
  // - possible anchor is in scope for positioned el, per the effects of
  // anchor-scope on positioned el or its ancestors.
  // - possible anchor is laid out strictly before positioned el
  //
  // Note: Frames having an anchor name contain elements.
  // The phrase "element or a fully styleable tree-abiding pseudo-element"
  // used by the spec is taken to mean
  // "either not a pseudo-element or a pseudo-element of a specific kind".
  return (IsFullyStyleableTreeAbidingOrNotPseudoElement(aPossibleAnchorFrame) &&
          IsAnchorInScopeForPositionedElement(aPossibleAnchorFrame,
                                              aPositionedFrame) &&
          IsAnchorLaidOutStrictlyBeforeElement(aPossibleAnchorFrame,
                                               aPositionedFrame) &&
          IsPositionedElementAlsoSkippedWhenAnchorIsSkipped(
              aPossibleAnchorFrame, aPositionedFrame));
}

}  // namespace

nsIFrame* AnchorPositioningUtils::FindFirstAcceptableAnchor(
    const nsIFrame* aPositionedFrame,
    const nsTArray<nsIFrame*>& aPossibleAnchorFrames) {
  for (auto it = aPossibleAnchorFrames.rbegin();
       it != aPossibleAnchorFrames.rend(); ++it) {
    // Check if the possible anchor is an acceptable anchor element.
    if (IsAcceptableAnchorElement(*it, aPositionedFrame)) {
      return *it;
    }
  }

  // If we reach here, we didn't find any acceptable anchor.
  return nullptr;
}

}  // namespace mozilla
