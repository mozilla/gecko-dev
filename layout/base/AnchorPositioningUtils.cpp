/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AnchorPositioningUtils.h"

#include "mozilla/dom/Document.h"
#include "mozilla/dom/Element.h"
#include "mozilla/Maybe.h"
#include "mozilla/PresShell.h"
#include "nsIContent.h"
#include "nsIFrame.h"
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

[[maybe_unused]] size_t GetTopLayerIndex(const nsIFrame* aFrame) {
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

bool IsAnchorLaidOutStrictlyBeforeElement(
    const nsIFrame* /* aPossibleAnchorFrame */,
    const nsIFrame* /* aPositionedFrame */) {
  return true;  // TODO: Implement this check. For now, always true.
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
