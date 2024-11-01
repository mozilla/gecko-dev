/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "TextDirectiveUtil.h"
#include "nsComputedDOMStyle.h"
#include "nsDOMAttributeMap.h"
#include "nsFind.h"
#include "nsGkAtoms.h"
#include "nsIFrame.h"
#include "nsINode.h"
#include "nsIURI.h"
#include "nsRange.h"
#include "nsString.h"
#include "nsTArray.h"
#include "Document.h"
#include "fragmentdirectives_ffi_generated.h"
#include "Text.h"
#include "mozilla/intl/WordBreaker.h"

namespace mozilla::dom {
LazyLogModule sFragmentDirectiveLog("FragmentDirective");

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
    nsRange* aSearchRange, const nsAString& aQuery, bool aWordStartBounded,
    bool aWordEndBounded) {
  MOZ_ASSERT(aSearchRange);
  TEXT_FRAGMENT_LOG("query='%s', wordStartBounded='%d', wordEndBounded='%d'.\n",
                    NS_ConvertUTF16toUTF8(aQuery).Data(), aWordStartBounded,
                    aWordEndBounded);
  RefPtr<nsFind> finder = new nsFind();
  finder->SetWordStartBounded(aWordStartBounded);
  finder->SetWordEndBounded(aWordEndBounded);
  finder->SetCaseSensitive(false);
  RefPtr<nsRange> searchRangeStart = nsRange::Create(
      aSearchRange->StartRef(), aSearchRange->StartRef(), IgnoreErrors());
  RefPtr<nsRange> searchRangeEnd = nsRange::Create(
      aSearchRange->EndRef(), aSearchRange->EndRef(), IgnoreErrors());
  RefPtr<nsRange> result;
  Unused << finder->Find(aQuery, aSearchRange, searchRangeStart, searchRangeEnd,
                         getter_AddRefs(result));
  if (!result || result->Collapsed()) {
    TEXT_FRAGMENT_LOG("Did not find query '%s'",
                      NS_ConvertUTF16toUTF8(aQuery).Data());
  } else {
    auto rangeToString = [](nsRange* range) -> nsCString {
      nsString rangeString;
      range->ToString(rangeString, IgnoreErrors());
      return NS_ConvertUTF16toUTF8(rangeString);
    };
    TEXT_FRAGMENT_LOG("find returned '%s'", rangeToString(result).Data());
  }
  return result;
}

/* static */ RangeBoundary TextDirectiveUtil::MoveRangeBoundaryOneWord(
    const RangeBoundary& aRangeBoundary, TextScanDirection aDirection) {
  MOZ_ASSERT(aRangeBoundary.IsSetAndValid());
  RefPtr<nsINode> curNode = aRangeBoundary.Container();
  uint32_t offset = *aRangeBoundary.Offset(
      RangeBoundary::OffsetFilter::kValidOrInvalidOffsets);

  const int offsetIncrement = int(aDirection);
  // Get the text node of the start of the range and the offset.
  // This is the current position of the start of the range.
  nsAutoString textContent;
  if (NodeIsVisibleTextNode(*curNode)) {
    const Text* textNode = Text::FromNode(curNode);

    // Assuming that the current position might not be at a word boundary,
    // advance to the word boundary at word begin/end.
    if (!IsWhitespaceAtPosition(textNode, offset)) {
      textNode->GetData(textContent);
      const intl::WordRange wordRange =
          intl::WordBreaker::FindWord(textContent, offset);
      if (aDirection == TextScanDirection::Right &&
          offset != wordRange.mBegin) {
        offset = wordRange.mEnd;
      } else if (aDirection == TextScanDirection::Left &&
                 offset != wordRange.mEnd) {
        // The additional -1 is necessary to move to offset to *before* the
        // start of the word.
        offset = wordRange.mBegin - 1;
      }
    }
  }
  // Now, skip any whitespace, so that `offset` points to the word boundary of
  // the next word (which is the one this algorithm actually aims to move over).
  while (curNode) {
    if (!NodeIsVisibleTextNode(*curNode) || NodeIsSearchInvisible(*curNode) ||
        offset >= curNode->Length()) {
      curNode = aDirection == TextScanDirection::Left ? curNode->GetPrevNode()
                                                      : curNode->GetNextNode();
      if (!curNode) {
        break;
      }
      offset =
          aDirection == TextScanDirection::Left ? curNode->Length() - 1 : 0;
      continue;
    }
    const Text* textNode = Text::FromNode(curNode);
    if (IsWhitespaceAtPosition(textNode, offset)) {
      offset += offsetIncrement;
      continue;
    }

    // At this point, the caret has been moved to the next non-whitespace
    // position.
    // find word boundaries at the current position
    textNode->GetData(textContent);
    const intl::WordRange wordRange =
        intl::WordBreaker::FindWord(textContent, offset);
    offset = aDirection == TextScanDirection::Left ? wordRange.mBegin
                                                   : wordRange.mEnd;
    bool allPunctuation = true;
    for (uint32_t ch = wordRange.mBegin; ch != wordRange.mEnd && allPunctuation;
         ++ch) {
      allPunctuation = mozilla::IsPunctuationForWordSelect(textContent[ch]);
    }
    if (!allPunctuation) {
      return {curNode, offset};
    }
    offset += int(aDirection);
  }
  return {};
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

/* static */ bool TextDirectiveUtil::IsAtWordBoundary(const nsAString& aText,
                                                      uint32_t aPosition) {
  const intl::WordRange wordRange =
      intl::WordBreaker::FindWord(aText, aPosition);
  return wordRange.mBegin == aPosition || wordRange.mEnd == aPosition;
}

/* static */ RangeBoundary TextDirectiveUtil::GetBoundaryPointAtIndex(
    uint32_t aIndex, const nsTArray<RefPtr<Text>>& aTextNodeList,
    IsEndIndex aIsEndIndex) {
  // 1. Let counted be 0.
  uint32_t counted = 0;
  // 2. For each curNode of nodes:
  for (Text* curNode : aTextNodeList) {
    // 2.1. Let nodeEnd be counted + curNode’s length.
    uint32_t nodeEnd = counted + curNode->Length();
    // 2.2. If isEnd is true, add 1 to nodeEnd.
    if (aIsEndIndex == IsEndIndex::Yes) {
      ++nodeEnd;
    }
    // 2.3. If nodeEnd is greater than index then:
    if (nodeEnd > aIndex) {
      // 2.3.1. Return the boundary point (curNode, index − counted).
      return RangeBoundary(curNode->AsNode(), aIndex - counted);
    }
    // 2.4. Increment counted by curNode’s length.
    counted += curNode->Length();
  }
  return {};
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

}  // namespace mozilla::dom
