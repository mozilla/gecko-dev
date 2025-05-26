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

/* static */ Result<nsString, ErrorResult>
TextDirectiveUtil::RangeContentAsFoldCase(nsRange* aRange) {
  Result<nsString, ErrorResult> contentResult = RangeContentAsString(aRange);
  if (MOZ_UNLIKELY(contentResult.isErr())) {
    return contentResult.propagateErr();
  }
  nsString content = contentResult.unwrap();
  content.CompressWhitespace();
  ToFoldedCase(content);
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

/* static */ RangeBoundary TextDirectiveUtil::MoveRangeBoundaryOneWord(
    const RangeBoundary& aRangeBoundary, TextScanDirection aDirection) {
  MOZ_ASSERT(aRangeBoundary.IsSetAndValid());
  PeekOffsetOptions options = {PeekOffsetOption::JumpLines,
                               PeekOffsetOption::StopAtScroller,
                               PeekOffsetOption::IsKeyboardSelect};
  Result<RangeBoundary, nsresult> newBoundary =
      SelectionMovementUtils::MoveRangeBoundaryToSomewhere(
          aRangeBoundary,
          aDirection == TextScanDirection::Left ? nsDirection::eDirPrevious
                                                : nsDirection::eDirNext,
          aDirection == TextScanDirection::Left ? CaretAssociationHint::Before
                                                : CaretAssociationHint::After,
          intl::BidiEmbeddingLevel::DefaultLTR(),
          nsSelectionAmount::eSelectWord, options);
  return newBoundary.unwrapOr({});
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

/* static */ RangeBoundary
TextDirectiveUtil::MoveBoundaryToNextNonWhitespacePosition(
    const RangeBoundary& aRangeBoundary) {
  MOZ_ASSERT(aRangeBoundary.IsSetAndValid());
  nsINode* node = aRangeBoundary.GetContainer();
  uint32_t offset =
      *aRangeBoundary.Offset(RangeBoundary::OffsetFilter::kValidOffsets);
  while (node) {
    if (TextDirectiveUtil::NodeIsPartOfNonSearchableSubTree(*node) ||
        !TextDirectiveUtil::NodeIsVisibleTextNode(*node) ||
        offset == node->Length()) {
      nsINode* newNode = node->GetNextNode();
      if (!newNode) {
        // jjaschke: I don't see a situation where this could happen. However,
        // let's return the original range boundary as fallback.
        return aRangeBoundary;
      }
      node = newNode;
      offset = 0;
      continue;
    }
    const Text* text = Text::FromNode(node);
    MOZ_ASSERT(text);
    if (TextDirectiveUtil::IsWhitespaceAtPosition(text, offset)) {
      ++offset;
      continue;
    }
    return {node, offset};
  }
  MOZ_ASSERT_UNREACHABLE("All code paths must return in the loop.");
  return {};
}

/* static */ RangeBoundary
TextDirectiveUtil::MoveBoundaryToPreviousNonWhitespacePosition(
    const RangeBoundary& aRangeBoundary) {
  MOZ_ASSERT(aRangeBoundary.IsSetAndValid());
  nsINode* node = aRangeBoundary.GetContainer();
  uint32_t offset =
      *aRangeBoundary.Offset(RangeBoundary::OffsetFilter::kValidOffsets);
  // Decrement offset by one so that the actual previous character is used. This
  // means that we need to increment the offset by 1 when we have found the
  // non-whitespace character.
  while (node) {
    if (TextDirectiveUtil::NodeIsPartOfNonSearchableSubTree(*node) ||
        !TextDirectiveUtil::NodeIsVisibleTextNode(*node) || offset == 0) {
      nsIContent* newNode = node->GetPrevNode();
      if (!newNode) {
        // jjaschke: I don't see a situation where this could happen. However,
        // let's return the original range boundary as fallback.
        return aRangeBoundary;
      }
      node = newNode;
      offset = node->Length();
      continue;
    }
    const Text* text = Text::FromNode(node);
    MOZ_ASSERT(text);
    if (TextDirectiveUtil::IsWhitespaceAtPosition(text, offset - 1)) {
      --offset;
      continue;
    }
    return {node, offset};
  }
  MOZ_ASSERT_UNREACHABLE("All code paths must return in the loop.");
  return {};
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

/* static */ Result<Maybe<RangeBoundary>, ErrorResult>
TextDirectiveUtil::FindBlockBoundaryInRange(const nsRange& aRange,
                                            TextScanDirection aDirection) {
  if (aRange.Collapsed()) {
    return Result<Maybe<RangeBoundary>, ErrorResult>(Nothing{});
  }
  if (aDirection == TextScanDirection::Right) {
    Result<RangeBoundary, ErrorResult> maybeBoundary =
        FindNextBlockBoundary(aRange.StartRef(), TextScanDirection::Right);
    if (MOZ_UNLIKELY(maybeBoundary.isErr())) {
      return maybeBoundary.propagateErr();
    }
    RangeBoundary boundary = maybeBoundary.unwrap();

    Maybe<int32_t> compare =
        nsContentUtils::ComparePoints(boundary, aRange.EndRef());
    if (!compare || *compare != -1) {
      // *compare == -1 means that the found block boundary is after the range
      // end, and therefore outside of the range.
      return Result<Maybe<RangeBoundary>, ErrorResult>(Nothing{});
    }

    return Some(boundary);
  }
  Result<RangeBoundary, ErrorResult> maybeBoundary =
      FindNextBlockBoundary(aRange.EndRef(), TextScanDirection::Left);
  if (MOZ_UNLIKELY(maybeBoundary.isErr())) {
    return maybeBoundary.propagateErr();
  }
  RangeBoundary boundary = maybeBoundary.unwrap();
  auto compare = nsContentUtils::ComparePoints(aRange.StartRef(), boundary);
  if (!compare || *compare != -1) {
    // *compare == 1 means that the found block boundary is before the range
    // start boundary, and therefore outside of the range.
    return Result<Maybe<RangeBoundary>, ErrorResult>(Nothing{});
  }
  return Some(boundary);
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

/* static */ Result<Ok, ErrorResult>
TextDirectiveUtil::ExtendRangeToWordBoundaries(nsRange& aRange) {
  MOZ_ASSERT(!aRange.Collapsed());
  PeekOffsetOptions options = {
      PeekOffsetOption::JumpLines, PeekOffsetOption::StopAtScroller,
      PeekOffsetOption::IsKeyboardSelect, PeekOffsetOption::Extend};
  // To extend a range `inputRange` to its word boundaries, perform these steps:
  // 1. To extend the start boundary:
  // 1.1 Let `newStartBoundary` be a range boundary, initially null.
  // 1.2 Create a new range boundary `rangeStartWordEndBoundary` at the next
  //    word end boundary at `inputRange`s start point.
  // 1.3 Then, create a new range boundary `rangeStartWordStartBoundary`
  //     at the previous word start boundary of `rangeStartWordEndBoundary`
  // 1.4 If `rangeStartWordStartBoundary` is not at the same (normalized)
  //     position as `inputRange`s start point, let `newStartBoundary` be
  //     `rangeStartWordStartBoundary`.
  Result<Maybe<RangeBoundary>, ErrorResult> newStartBoundary =
      SelectionMovementUtils::MoveRangeBoundaryToSomewhere(
          aRange.StartRef(), nsDirection::eDirNext, CaretAssociationHint::After,
          intl::BidiEmbeddingLevel::DefaultLTR(),
          nsSelectionAmount::eSelectWord, options)
          .andThen([&options](const RangeBoundary& rangeStartWordEndBoundary) {
            return SelectionMovementUtils::MoveRangeBoundaryToSomewhere(
                rangeStartWordEndBoundary, nsDirection::eDirPrevious,
                CaretAssociationHint::Before,
                intl::BidiEmbeddingLevel::DefaultLTR(),
                nsSelectionAmount::eSelectWord, options);
          })
          .map([&rangeStart = aRange.StartRef()](
                   RangeBoundary&& rangeStartWordStartBoundary) {
            return NormalizedRangeBoundariesAreEqual(
                       rangeStartWordStartBoundary, rangeStart)
                       ? Nothing{}
                       : Some(std::move(rangeStartWordStartBoundary));
          })
          .mapErr([](nsresult rv) { return ErrorResult(rv); });
  if (MOZ_UNLIKELY(newStartBoundary.isErr())) {
    return newStartBoundary.propagateErr();
  }

  // 2. To extend the end boundary:
  // 2.1 Let `newEndBoundary` be a range boundary, initially null.
  // 2.2 Create a new range boundary `rangeEndWordStartBoundary` at the previous
  //     word start boundary at `inputRange`s end point.
  // 2.3 Then, create a new range boundary `rangeEndWordEndBoundary` at the next
  //     word end boundary from `rangeEndWordStartBoundary`.
  // 2.4 If `rangeEndWordEndBoundary` is not at the same (normalized) position
  //     as  `inputRange`s end point, let `newEndBoundary` be
  //     `rangEndWordEndBoundary`.
  Result<Maybe<RangeBoundary>, ErrorResult> newEndBoundary =
      SelectionMovementUtils::MoveRangeBoundaryToSomewhere(
          aRange.EndRef(), nsDirection::eDirPrevious,
          CaretAssociationHint::Before, intl::BidiEmbeddingLevel::DefaultLTR(),
          nsSelectionAmount::eSelectWord, options)
          .andThen([&options](const RangeBoundary& rangeEndWordStartBoundary) {
            return SelectionMovementUtils::MoveRangeBoundaryToSomewhere(
                rangeEndWordStartBoundary, nsDirection::eDirNext,
                CaretAssociationHint::After,
                intl::BidiEmbeddingLevel::DefaultLTR(),
                nsSelectionAmount::eSelectWord, options);
          })
          .map([&rangeEnd =
                    aRange.EndRef()](RangeBoundary&& rangeEndWordEndBoundary) {
            return NormalizedRangeBoundariesAreEqual(rangeEndWordEndBoundary,
                                                     rangeEnd)
                       ? Nothing{}
                       : Some(std::move(rangeEndWordEndBoundary));
          })
          .mapErr([](auto rv) { return ErrorResult(rv); });
  if (MOZ_UNLIKELY(newEndBoundary.isErr())) {
    return newEndBoundary.propagateErr();
  }
  // 3. If `newStartBoundary` is not null, set `inputRange`s start point to
  //    `newStartBoundary`.
  MOZ_TRY(newStartBoundary.andThen(
      [&aRange](Maybe<RangeBoundary>&& boundary) -> Result<Ok, ErrorResult> {
        if (boundary.isNothing() || !boundary->IsSetAndValid()) {
          return Ok();
        }
        ErrorResult rv;
        aRange.SetStart(boundary->AsRaw(), rv);
        if (MOZ_UNLIKELY(rv.Failed())) {
          return Err(std::move(rv));
        }
        return Ok();
      }));
  // 4. If `newEndBoundary` is not null, set `inputRange`s end point to
  //    `newEndBoundary`.
  MOZ_TRY(newEndBoundary.andThen(
      [&aRange](Maybe<RangeBoundary>&& boundary) -> Result<Ok, ErrorResult> {
        if (boundary.isNothing() || !boundary->IsSetAndValid()) {
          return Ok();
        }
        ErrorResult rv;
        aRange.SetEnd(boundary->AsRaw(), rv);
        if (MOZ_UNLIKELY(rv.Failed())) {
          return Err(std::move(rv));
        }
        return Ok();
      }));

  return Ok();
}

/* static */
Result<TextDirective, ErrorResult>
TextDirectiveUtil::CreateTextDirectiveFromRanges(nsRange* aPrefix,
                                                 nsRange* aStart, nsRange* aEnd,
                                                 nsRange* aSuffix) {
  MOZ_ASSERT(aStart && !aStart->Collapsed());

  ErrorResult rv;
  TextDirective textDirective;
  MOZ_TRY(RangeContentAsString(aStart).andThen(
      [&textDirective](nsString start) -> Result<Ok, ErrorResult> {
        textDirective.start = std::move(start);
        return Ok();
      }));
  MOZ_TRY(RangeContentAsString(aPrefix).andThen(
      [&textDirective](nsString prefix) -> Result<Ok, ErrorResult> {
        textDirective.prefix = std::move(prefix);
        return Ok();
      }));
  MOZ_TRY(RangeContentAsString(aEnd).andThen(
      [&textDirective](nsString end) -> Result<Ok, ErrorResult> {
        textDirective.end = std::move(end);
        return Ok();
      }));
  MOZ_TRY(RangeContentAsString(aSuffix).andThen(
      [&textDirective](nsString suffix) -> Result<Ok, ErrorResult> {
        textDirective.suffix = std::move(suffix);
        return Ok();
      }));
  return textDirective;
}

uint32_t TextDirectiveUtil::FindCommonPrefix(const nsAString& aFoldedStr1,
                                             const nsAString& aFoldedStr2) {
  const uint32_t maxCommonLength =
      std::min(aFoldedStr1.Length(), aFoldedStr2.Length());
  uint32_t commonLength = 0;
  const char16_t* iter1 = aFoldedStr1.BeginReading();
  const char16_t* iter2 = aFoldedStr2.BeginReading();

  while (commonLength < maxCommonLength) {
    if (*iter1 != *iter2) {
      break;
    }
    ++iter1;
    ++iter2;
    ++commonLength;
  }
  // this condition ensures that if the high surrogate is removed if the low
  // surrogate does not match.
  if (commonLength && NS_IS_HIGH_SURROGATE(*(iter1 - 1))) {
    --commonLength;
  }
  return commonLength;
}

uint32_t TextDirectiveUtil::FindCommonSuffix(const nsAString& aFoldedStr1,
                                             const nsAString& aFoldedStr2) {
  const uint32_t maxCommonLength =
      std::min(aFoldedStr1.Length(), aFoldedStr2.Length());
  uint32_t commonLength = 0;
  const char16_t* iter1 = aFoldedStr1.EndReading();
  const char16_t* iter2 = aFoldedStr2.EndReading();
  while (commonLength != maxCommonLength) {
    if (*(iter1 - 1) != *(iter2 - 1)) {
      break;
    }
    --iter1;
    --iter2;
    ++commonLength;
  }
  // this condition ensures that a matching low surrogate is removed if the high
  // surrogate does not match.
  if (commonLength && NS_IS_LOW_SURROGATE(*iter1)) {
    --commonLength;
  }
  return commonLength;
}

RangeBoundary
TextDirectiveUtil::CreateRangeBoundaryByMovingOffsetFromRangeStart(
    nsRange* aRange, uint32_t aLogicalOffset) {
  MOZ_ASSERT(!aRange->Collapsed());

  nsINode* node = aRange->GetStartContainer();
  uint32_t startOffset = aRange->StartOffset();

  uint32_t remaining = startOffset + aLogicalOffset;
  while (node && remaining) {
    if (NodeIsPartOfNonSearchableSubTree(*node) ||
        !NodeIsVisibleTextNode(*node)) {
      node = node->GetNextNode();
      continue;
    }
    MOZ_ASSERT_IF(node == aRange->GetEndContainer(),
                  remaining <= node->Length());
    if (node->Length() <= remaining) {
      remaining -= node->Length();
      node = node->GetNextNode();
      continue;
    }
    return {node, remaining};
  }
  return {node, remaining};
}

}  // namespace mozilla::dom
