/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "TextDirectiveUtil.h"
#include "nsComputedDOMStyle.h"
#include "nsDOMAttributeMap.h"
#include "nsFind.h"
#include "nsFrameSelection.h"
#include "nsGkAtoms.h"
#include "nsIFrame.h"
#include "nsINode.h"
#include "nsIURI.h"
#include "nsRange.h"
#include "nsString.h"
#include "nsTArray.h"
#include "nsUnicharUtils.h"
#include "ContentIterator.h"
#include "Document.h"
#include "fragmentdirectives_ffi_generated.h"
#include "Text.h"
#include "mozilla/ContentIterator.h"
#include "mozilla/ResultVariant.h"
#include "mozilla/intl/WordBreaker.h"
#include "mozilla/SelectionMovementUtils.h"

namespace mozilla::dom {
LazyLogModule gFragmentDirectiveLog("FragmentDirective");

/* static */
Result<nsString, ErrorResult> TextDirectiveUtil::RangeContentAsString(
    nsRange* aRange) {
  nsString content;
  if (!aRange || aRange->Collapsed()) {
    return content;
  }
  UnsafePreContentIterator iter;
  nsresult rv = iter.Init(aRange);
  if (NS_FAILED(rv)) {
    return Err(ErrorResult(rv));
  }
  for (; !iter.IsDone(); iter.Next()) {
    nsINode* current = iter.GetCurrentNode();
    if (!TextDirectiveUtil::NodeIsVisibleTextNode(*current) ||
        TextDirectiveUtil::NodeIsPartOfNonSearchableSubTree(*current)) {
      continue;
    }
    const uint32_t startOffset =
        current == aRange->GetStartContainer() ? aRange->StartOffset() : 0;
    const uint32_t endOffset =
        std::min(current == aRange->GetEndContainer() ? aRange->EndOffset()
                                                      : current->Length(),
                 current->Length());
    const Text* text = Text::FromNode(current);
    text->TextFragment().AppendTo(content, startOffset,
                                  endOffset - startOffset);
  }
  content.CompressWhitespace();
  return content;
}

/* static */ bool TextDirectiveUtil::NodeIsVisibleTextNode(
    const nsINode& aNode) {
  const Text* text = Text::FromNode(aNode);
  if (!text) {
    return false;
  }
  const nsIFrame* frame = text->GetPrimaryFrame();
  return frame && frame->StyleVisibility()->IsVisible();
}

/* static */ RefPtr<nsRange> TextDirectiveUtil::FindStringInRange(
    const RangeBoundary& aSearchStart, const RangeBoundary& aSearchEnd,
    const nsAString& aQuery, bool aWordStartBounded, bool aWordEndBounded,
    nsContentUtils::NodeIndexCache* aCache) {
  TEXT_FRAGMENT_LOG("query='{}', wordStartBounded='{}', wordEndBounded='{}'.\n",
                    NS_ConvertUTF16toUTF8(aQuery), aWordStartBounded,
                    aWordEndBounded);
  RefPtr<nsFind> finder = new nsFind();
  finder->SetWordStartBounded(aWordStartBounded);
  finder->SetWordEndBounded(aWordEndBounded);
  finder->SetCaseSensitive(false);
  finder->SetNodeIndexCache(aCache);
  RefPtr<nsRange> result =
      finder->FindFromRangeBoundaries(aQuery, aSearchStart, aSearchEnd);
  if (!result || result->Collapsed()) {
    TEXT_FRAGMENT_LOG("Did not find query '{}'", NS_ConvertUTF16toUTF8(aQuery));
  } else {
    auto rangeToString = [](nsRange* range) -> nsCString {
      nsString rangeString;
      range->ToString(rangeString, IgnoreErrors());
      return NS_ConvertUTF16toUTF8(rangeString);
    };
    TEXT_FRAGMENT_LOG("find returned '{}'", rangeToString(result));
  }
  return result;
}

/* static */ bool TextDirectiveUtil::IsWhitespaceAtPosition(const Text* aText,
                                                            uint32_t aPos) {
  if (!aText || aText->Length() == 0 || aPos >= aText->Length()) {
    return false;
  }
  const nsTextFragment& frag = aText->TextFragment();
  const char NBSP_CHAR = char(0xA0);
  if (frag.Is2b()) {
    const char16_t* content = frag.Get2b();
    return IsSpaceCharacter(content[aPos]) ||
           content[aPos] == char16_t(NBSP_CHAR);
  }
  const char* content = frag.Get1b();
  return IsSpaceCharacter(content[aPos]) || content[aPos] == NBSP_CHAR;
}

/* static */ bool TextDirectiveUtil::NodeIsSearchInvisible(nsINode& aNode) {
  if (!aNode.IsElement()) {
    return false;
  }
  // 2. If the node serializes as void.
  nsAtom* nodeNameAtom = aNode.NodeInfo()->NameAtom();
  if (FragmentOrElement::IsHTMLVoid(nodeNameAtom)) {
    return true;
  }
  // 3. Is any of the following types: HTMLIFrameElement, HTMLImageElement,
  // HTMLMeterElement, HTMLObjectElement, HTMLProgressElement, HTMLStyleElement,
  // HTMLScriptElement, HTMLVideoElement, HTMLAudioElement
  if (aNode.IsAnyOfHTMLElements(
          nsGkAtoms::iframe, nsGkAtoms::image, nsGkAtoms::meter,
          nsGkAtoms::object, nsGkAtoms::progress, nsGkAtoms::style,
          nsGkAtoms::script, nsGkAtoms::video, nsGkAtoms::audio)) {
    return true;
  }
  // 4. Is a select element whose multiple content attribute is absent.
  if (aNode.IsHTMLElement(nsGkAtoms::select)) {
    return aNode.GetAttributes()->GetNamedItem(u"multiple"_ns) == nullptr;
  }
  // This is tested last because it's the most expensive check.
  // 1. The computed value of its 'display' property is 'none'.
  const Element* nodeAsElement = Element::FromNode(aNode);
  const RefPtr<const ComputedStyle> computedStyle =
      nsComputedDOMStyle::GetComputedStyleNoFlush(nodeAsElement);
  return !computedStyle ||
         computedStyle->StyleDisplay()->mDisplay == StyleDisplay::None;
}

/* static */ bool TextDirectiveUtil::NodeHasBlockLevelDisplay(nsINode& aNode) {
  if (!aNode.IsElement()) {
    return false;
  }
  const Element* nodeAsElement = Element::FromNode(aNode);
  const RefPtr<const ComputedStyle> computedStyle =
      nsComputedDOMStyle::GetComputedStyleNoFlush(nodeAsElement);
  if (!computedStyle) {
    return false;
  }
  const StyleDisplay& styleDisplay = computedStyle->StyleDisplay()->mDisplay;
  return styleDisplay == StyleDisplay::Block ||
         styleDisplay == StyleDisplay::Table ||
         styleDisplay == StyleDisplay::FlowRoot ||
         styleDisplay == StyleDisplay::Grid ||
         styleDisplay == StyleDisplay::Flex || styleDisplay.IsListItem();
}

/* static */ nsINode* TextDirectiveUtil::GetBlockAncestorForNode(
    nsINode* aNode) {
  // 1. Let curNode be node.
  RefPtr<nsINode> curNode = aNode;
  // 2. While curNode is non-null
  while (curNode) {
    // 2.1. If curNode is not a Text node and it has block-level display then
    // return curNode.
    if (!curNode->IsText() && NodeHasBlockLevelDisplay(*curNode)) {
      return curNode;
    }
    // 2.2. Otherwise, set curNode to curNode’s parent.
    curNode = curNode->GetParentNode();
  }
  // 3.Return node’s node document's document element.
  return aNode->GetOwnerDocument();
}

/* static */ bool TextDirectiveUtil::NodeIsPartOfNonSearchableSubTree(
    nsINode& aNode) {
  nsINode* node = &aNode;
  do {
    if (NodeIsSearchInvisible(*node)) {
      return true;
    }
  } while ((node = node->GetParentOrShadowHostNode()));
  return false;
}

/* static */ void TextDirectiveUtil::AdvanceStartToNextNonWhitespacePosition(
    nsRange& aRange) {
  // 1. While range is not collapsed:
  while (!aRange.Collapsed()) {
    // 1.1. Let node be range's start node.
    RefPtr<nsINode> node = aRange.GetStartContainer();
    MOZ_ASSERT(node);
    // 1.2. Let offset be range's start offset.
    const uint32_t offset = aRange.StartOffset();
    // 1.3. If node is part of a non-searchable subtree or if node is not a
    // visible text node or if offset is equal to node's length then:
    if (NodeIsPartOfNonSearchableSubTree(*node) ||
        !NodeIsVisibleTextNode(*node) || offset == node->Length()) {
      // 1.3.1. Set range's start node to the next node, in shadow-including
      // tree order.
      // 1.3.2. Set range's start offset to 0.
      if (NS_FAILED(aRange.SetStart(node->GetNextNode(), 0))) {
        return;
      }
      // 1.3.3. Continue.
      continue;
    }
    const Text* text = Text::FromNode(node);
    MOZ_ASSERT(text);
    // These steps are moved to `IsWhitespaceAtPosition()`.
    // 1.4. If the substring data of node at offset offset and count 6 is equal
    // to the string "&nbsp;" then:
    // 1.4.1. Add 6 to range’s start offset.
    // 1.5. Otherwise, if the substring data of node at offset offset and count
    // 5 is equal to the string "&nbsp" then:
    // 1.5.1. Add 5 to range’s start offset.
    // 1.6. Otherwise:
    // 1.6.1 Let cp be the code point at the offset index in node’s data.
    // 1.6.2 If cp does not have the White_Space property set, return.
    // 1.6.3 Add 1 to range’s start offset.
    if (!IsWhitespaceAtPosition(text, offset)) {
      return;
    }

    aRange.SetStart(node, offset + 1);
  }
}
// https://wicg.github.io/scroll-to-text-fragment/#find-a-range-from-a-text-directive
// Steps 2.2.3, 2.3.4
/* static */
RangeBoundary TextDirectiveUtil::MoveToNextBoundaryPoint(
    const RangeBoundary& aPoint) {
  MOZ_DIAGNOSTIC_ASSERT(aPoint.IsSetAndValid());
  Text* node = Text::FromNode(aPoint.GetContainer());
  MOZ_ASSERT(node);
  uint32_t pos =
      *aPoint.Offset(RangeBoundary::OffsetFilter::kValidOrInvalidOffsets);
  if (!node) {
    return {};
  }
  ++pos;
  if (pos < node->Length() &&
      node->GetText()->IsLowSurrogateFollowingHighSurrogateAt(pos)) {
    ++pos;
  }
  return {node, pos};
}

/* static */ Result<RangeBoundary, ErrorResult>
TextDirectiveUtil::FindNextBlockBoundary(const RangeBoundary& aRangeBoundary,
                                         TextScanDirection aDirection) {
  MOZ_ASSERT(aRangeBoundary.IsSetAndValid());
  auto findNextBlockBoundaryInternal =
      [aDirection](const RangeBoundary& rangeBoundary)
      -> Result<RangeBoundary, ErrorResult> {
    PeekOffsetOptions options = {
        PeekOffsetOption::JumpLines, PeekOffsetOption::StopAtScroller,
        PeekOffsetOption::IsKeyboardSelect, PeekOffsetOption::Extend};
    return SelectionMovementUtils::MoveRangeBoundaryToSomewhere(
               rangeBoundary,
               aDirection == TextScanDirection::Left ? nsDirection::eDirPrevious
                                                     : nsDirection::eDirNext,
               CaretAssociationHint::After,
               intl::BidiEmbeddingLevel::DefaultLTR(),
               nsSelectionAmount::eSelectParagraph, options)
        .mapErr([](nsresult rv) { return ErrorResult(rv); });
  };
  auto maybeNewBoundary = findNextBlockBoundaryInternal(aRangeBoundary);
  if (MOZ_UNLIKELY(maybeNewBoundary.isErr())) {
    return maybeNewBoundary.propagateErr();
  }
  auto newBoundary = maybeNewBoundary.unwrap();
  while (NormalizedRangeBoundariesAreEqual(aRangeBoundary, newBoundary)) {
    maybeNewBoundary = findNextBlockBoundaryInternal(newBoundary);
    if (MOZ_UNLIKELY(maybeNewBoundary.isErr())) {
      return maybeNewBoundary.propagateErr();
    }
    if (maybeNewBoundary.inspect() == newBoundary) {
      return newBoundary;  // we reached the end or so?
    }
    newBoundary = maybeNewBoundary.unwrap();
  }
  return newBoundary;
}

/* static */ bool TextDirectiveUtil::NormalizedRangeBoundariesAreEqual(
    const RangeBoundary& aRangeBoundary1, const RangeBoundary& aRangeBoundary2,
    nsContentUtils::NodeIndexCache* aCache /* = nullptr */) {
  MOZ_ASSERT(aRangeBoundary1.IsSetAndValid() &&
             aRangeBoundary2.IsSetAndValid());
  if (aRangeBoundary1 == aRangeBoundary2) {
    return true;
  }
  auto textSubStringIsOnlyWhitespace =
      [](const Text* textNode, uint32_t startIndex, uint32_t endIndex) {
        MOZ_ASSERT(textNode);
        if (startIndex > endIndex) {
          std::swap(startIndex, endIndex);
        }
        MOZ_ASSERT(startIndex < textNode->Length());
        if (startIndex == endIndex) {
          return true;
        }
        const nsTextFragment& textFragment = textNode->TextFragment();

        for (uint32_t i = startIndex; i < endIndex; ++i) {
          char16_t ch = textFragment.CharAt(i);
          if (!nsContentUtils::IsHTMLWhitespaceOrNBSP(ch)) {
            return false;
          }
        }
        return true;
      };
  const nsINode* node1 = aRangeBoundary1.GetContainer();
  const nsINode* node2 = aRangeBoundary2.GetContainer();
  size_t offset1 =
      *aRangeBoundary1.Offset(RangeBoundary::OffsetFilter::kValidOffsets);
  size_t offset2 =
      *aRangeBoundary2.Offset(RangeBoundary::OffsetFilter::kValidOffsets);

  if (node1 == node2) {
    if (const Text* text = Text::FromNodeOrNull(node1)) {
      return textSubStringIsOnlyWhitespace(text, offset1, offset2);
    }
    return offset1 == offset2;
  }

  mozilla::UnsafePreContentIterator iter;
  // ContentIterator classes require boundaries to be in correct order.
  auto comp =
      nsContentUtils::ComparePoints(aRangeBoundary1, aRangeBoundary2, aCache);
  if (!comp) {
    return false;
  }
  if (*comp == 0) {
    return true;
  }
  const auto& [firstBoundary, secondBoundary] =
      *comp == -1 ? std::tuple{&aRangeBoundary1, &aRangeBoundary2}
                  : std::tuple{&aRangeBoundary2, &aRangeBoundary1};

  if (NS_FAILED(iter.InitWithoutValidatingPoints(firstBoundary->AsRaw(),
                                                 secondBoundary->AsRaw()))) {
    return false;
  }

  for (; !iter.IsDone(); iter.Next()) {
    auto* node = iter.GetCurrentNode();
    if (!node || !TextDirectiveUtil::NodeIsVisibleTextNode(*node)) {
      continue;
    }
    if (node == firstBoundary->GetContainer()) {
      auto firstOffset =
          *firstBoundary->Offset(RangeBoundary::OffsetFilter::kValidOffsets);
      if (firstOffset == node->Length()) {
        // if this is the start node, it's a text node and the offset is at the
        // end, continue with the next node.
        continue;
      }
      if (const Text* text = Text::FromNodeOrNull(node)) {
        if (textSubStringIsOnlyWhitespace(text, firstOffset, text->Length())) {
          continue;
        }
      }
    }
    if (node == secondBoundary->GetContainer()) {
      auto secondOffset =
          *secondBoundary->Offset(RangeBoundary::OffsetFilter::kValidOffsets);
      if (secondOffset == 0) {
        // if this is the end node, it's a text node and the offset is 0, return
        // true.
        return true;
      }
      if (const Text* text = Text::FromNodeOrNull(node)) {
        if (textSubStringIsOnlyWhitespace(text, 0, secondOffset)) {
          return true;
        }
      }
    }

    if (node->Length() && !node->AsText()->TextIsOnlyWhitespace()) {
      // if the text node only contains whitespace, ignore it; otherwise, the
      // boundaries are not at the same spot.
      return false;
    }
  }
  return true;
}

}  // namespace mozilla::dom
