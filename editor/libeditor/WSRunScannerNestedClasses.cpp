/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WSRunScanner.h"

#include "EditorDOMPoint.h"
#include "EditorUtils.h"
#include "HTMLEditUtils.h"

#include "mozilla/Assertions.h"
#include "mozilla/dom/AncestorIterator.h"

#include "nsCRT.h"
#include "nsDebug.h"
#include "nsIContent.h"
#include "nsIContentInlines.h"
#include "nsTextFragment.h"

namespace mozilla {

using namespace dom;

using AncestorType = HTMLEditUtils::AncestorType;
using AncestorTypes = HTMLEditUtils::AncestorTypes;
using LeafNodeType = HTMLEditUtils::LeafNodeType;
using LeafNodeTypes = HTMLEditUtils::LeafNodeTypes;

template WSRunScanner::TextFragmentData::TextFragmentData(
    Scan aScanMode, const EditorDOMPoint& aPoint,
    BlockInlineCheck aBlockInlineCheck, const Element* aAncestorLimiter);
template WSRunScanner::TextFragmentData::TextFragmentData(
    Scan aScanMode, const EditorRawDOMPoint& aPoint,
    BlockInlineCheck aBlockInlineCheck, const Element* aAncestorLimiter);
template WSRunScanner::TextFragmentData::TextFragmentData(
    Scan aScanMode, const EditorDOMPointInText& aPoint,
    BlockInlineCheck aBlockInlineCheck, const Element* aAncestorLimiter);
template WSRunScanner::TextFragmentData::TextFragmentData(
    Scan aScanMode, const EditorRawDOMPointInText& aPoint,
    BlockInlineCheck aBlockInlineCheck, const Element* aAncestorLimiter);

NS_INSTANTIATE_METHOD_RETURNING_ANY_EDITOR_DOM_POINT(
    WSRunScanner::TextFragmentData::GetInclusiveNextCharPoint,
    const EditorDOMPoint& aPoint, BlockInlineCheck aBlockInlineCheck,
    IgnoreNonEditableNodes aIgnoreNonEditableNodes,
    const nsIContent* aFollowingLimiterContent);
NS_INSTANTIATE_METHOD_RETURNING_ANY_EDITOR_DOM_POINT(
    WSRunScanner::TextFragmentData::GetInclusiveNextCharPoint,
    const EditorRawDOMPoint& aPoint, BlockInlineCheck aBlockInlineCheck,
    IgnoreNonEditableNodes aIgnoreNonEditableNodes,
    const nsIContent* aFollowingLimiterContent);
NS_INSTANTIATE_METHOD_RETURNING_ANY_EDITOR_DOM_POINT(
    WSRunScanner::TextFragmentData::GetInclusiveNextCharPoint,
    const EditorDOMPointInText& aPoint, BlockInlineCheck aBlockInlineCheck,
    IgnoreNonEditableNodes aIgnoreNonEditableNodes,
    const nsIContent* aFollowingLimiterContent);
NS_INSTANTIATE_METHOD_RETURNING_ANY_EDITOR_DOM_POINT(
    WSRunScanner::TextFragmentData::GetInclusiveNextCharPoint,
    const EditorRawDOMPointInText& aPoint, BlockInlineCheck aBlockInlineCheck,
    IgnoreNonEditableNodes aIgnoreNonEditableNodes,
    const nsIContent* aFollowingLimiterContent);

NS_INSTANTIATE_METHOD_RETURNING_ANY_EDITOR_DOM_POINT(
    WSRunScanner::TextFragmentData::GetPreviousCharPoint,
    const EditorDOMPoint& aPoint, BlockInlineCheck aBlockInlineCheck,
    IgnoreNonEditableNodes aIgnoreNonEditableNodes,
    const nsIContent* aPrecedingLimiterContent);
NS_INSTANTIATE_METHOD_RETURNING_ANY_EDITOR_DOM_POINT(
    WSRunScanner::TextFragmentData::GetPreviousCharPoint,
    const EditorRawDOMPoint& aPoint, BlockInlineCheck aBlockInlineCheck,
    IgnoreNonEditableNodes aIgnoreNonEditableNodes,
    const nsIContent* aPrecedingLimiterContent);
NS_INSTANTIATE_METHOD_RETURNING_ANY_EDITOR_DOM_POINT(
    WSRunScanner::TextFragmentData::GetPreviousCharPoint,
    const EditorDOMPointInText& aPoint, BlockInlineCheck aBlockInlineCheck,
    IgnoreNonEditableNodes aIgnoreNonEditableNodes,
    const nsIContent* aPrecedingLimiterContent);
NS_INSTANTIATE_METHOD_RETURNING_ANY_EDITOR_DOM_POINT(
    WSRunScanner::TextFragmentData::GetPreviousCharPoint,
    const EditorRawDOMPointInText& aPoint, BlockInlineCheck aBlockInlineCheck,
    IgnoreNonEditableNodes aIgnoreNonEditableNodes,
    const nsIContent* aPrecedingLimiterContent);

NS_INSTANTIATE_METHOD_RETURNING_ANY_EDITOR_DOM_POINT(
    WSRunScanner::TextFragmentData::GetEndOfCollapsibleASCIIWhiteSpaces,
    const EditorDOMPointInText& aPointAtASCIIWhiteSpace,
    nsIEditor::EDirection aDirectionToDelete,
    BlockInlineCheck aBlockInlineCheck,
    IgnoreNonEditableNodes aIgnoreNonEditableNodes,
    const nsIContent* aFollowingLimiterContent);

NS_INSTANTIATE_METHOD_RETURNING_ANY_EDITOR_DOM_POINT(
    WSRunScanner::TextFragmentData::GetFirstASCIIWhiteSpacePointCollapsedTo,
    const EditorDOMPointInText& aPointAtASCIIWhiteSpace,
    nsIEditor::EDirection aDirectionToDelete,
    BlockInlineCheck aBlockInlineCheck,
    IgnoreNonEditableNodes aIgnoreNonEditableNodes,
    const nsIContent* aPrecedingLimiterContent);

constexpr static const AncestorTypes kScanAnyRootAncestorTypes = {
    // If the point is in a block, we need to scan only in the block
    AncestorType::ClosestBlockElement,
    // So, we want a root element of the (shadow) tree root element of the
    // point
    // if there is no parent block
    AncestorType::AllowRootOrAncestorLimiterElement,
    // Basically, given point shouldn't be a void element, so, ignore
    // ancestor
    // void elements
    AncestorType::IgnoreHRElement};
constexpr static const AncestorTypes kScanEditableRootAncestorTypes = {
    // Only editable elements
    AncestorType::EditableElement,
    // And the others are same as kScanAnyRootAncestorTypes
    AncestorType::ClosestBlockElement,
    AncestorType::AllowRootOrAncestorLimiterElement,
    AncestorType::IgnoreHRElement};

template <typename EditorDOMPointType>
WSRunScanner::TextFragmentData::TextFragmentData(
    Scan aScanMode, const EditorDOMPointType& aPoint,
    BlockInlineCheck aBlockInlineCheck,
    const Element* aAncestorLimiter /* = nullptr */)
    : mBlockInlineCheck(aBlockInlineCheck), mScanMode(aScanMode) {
  if (NS_WARN_IF(!aPoint.IsInContentNodeAndValidInComposedDoc()) ||
      NS_WARN_IF(!aPoint.GetContainerOrContainerParentElement())) {
    // We don't need to support composing in uncomposed tree.
    return;
  }

  MOZ_ASSERT_IF(
      aAncestorLimiter,
      aPoint.template ContainerAs<nsIContent>()->IsInclusiveDescendantOf(
          aAncestorLimiter));

  mScanStartPoint = aPoint.template To<EditorDOMPoint>();
  const Element* const
      editableBlockElementOrInlineEditingHostOrNonEditableRootElement =
          HTMLEditUtils::GetInclusiveAncestorElement(
              *mScanStartPoint.ContainerAs<nsIContent>(),
              aScanMode == Scan::EditableNodes ? kScanEditableRootAncestorTypes
                                               : kScanAnyRootAncestorTypes,
              aBlockInlineCheck, aAncestorLimiter);
  if (NS_WARN_IF(
          !editableBlockElementOrInlineEditingHostOrNonEditableRootElement)) {
    return;
  }
  mStart = BoundaryData::ScanCollapsibleWhiteSpaceStartFrom(
      aScanMode, mScanStartPoint, &mNBSPData, aBlockInlineCheck,
      ShouldStopAtNonEditableNode(aScanMode),
      *editableBlockElementOrInlineEditingHostOrNonEditableRootElement);
  MOZ_ASSERT_IF(mStart.IsNonCollapsibleCharacters(),
                !mStart.PointRef().IsPreviousCharPreformattedNewLine());
  MOZ_ASSERT_IF(mStart.IsPreformattedLineBreak(),
                mStart.PointRef().IsPreviousCharPreformattedNewLine());
  mEnd = BoundaryData::ScanCollapsibleWhiteSpaceEndFrom(
      aScanMode, mScanStartPoint, &mNBSPData, aBlockInlineCheck,
      ShouldStopAtNonEditableNode(aScanMode),
      *editableBlockElementOrInlineEditingHostOrNonEditableRootElement);
  MOZ_ASSERT_IF(mEnd.IsNonCollapsibleCharacters(),
                !mEnd.PointRef().IsCharPreformattedNewLine());
  MOZ_ASSERT_IF(mEnd.IsPreformattedLineBreak(),
                mEnd.PointRef().IsCharPreformattedNewLine());
}

// static
template <typename EditorDOMPointType>
Maybe<WSRunScanner::TextFragmentData::BoundaryData> WSRunScanner::
    TextFragmentData::BoundaryData::ScanCollapsibleWhiteSpaceStartInTextNode(
        const EditorDOMPointType& aPoint, NoBreakingSpaceData* aNBSPData,
        BlockInlineCheck aBlockInlineCheck) {
  MOZ_ASSERT(aPoint.IsSetAndValid());
  MOZ_DIAGNOSTIC_ASSERT(aPoint.IsInTextNode());

  const bool isWhiteSpaceCollapsible = !EditorUtils::IsWhiteSpacePreformatted(
      *aPoint.template ContainerAs<Text>());
  const bool isNewLineCollapsible =
      !EditorUtils::IsNewLinePreformatted(*aPoint.template ContainerAs<Text>());
  const nsTextFragment& textFragment =
      aPoint.template ContainerAs<Text>()->TextFragment();
  for (uint32_t i = std::min(aPoint.Offset(), textFragment.GetLength()); i;
       i--) {
    WSType wsTypeOfNonCollapsibleChar;
    switch (textFragment.CharAt(i - 1)) {
      case HTMLEditUtils::kSpace:
      case HTMLEditUtils::kCarriageReturn:
      case HTMLEditUtils::kTab:
        if (isWhiteSpaceCollapsible) {
          continue;  // collapsible white-space or invisible white-space.
        }
        // preformatted white-space.
        wsTypeOfNonCollapsibleChar = WSType::NonCollapsibleCharacters;
        break;
      case HTMLEditUtils::kNewLine:
        if (isNewLineCollapsible) {
          continue;  // collapsible linefeed.
        }
        // preformatted linefeed.
        wsTypeOfNonCollapsibleChar = WSType::PreformattedLineBreak;
        break;
      case HTMLEditUtils::kNBSP:
        if (isWhiteSpaceCollapsible) {
          if (aNBSPData) {
            aNBSPData->NotifyNBSP(
                EditorDOMPointInText(aPoint.template ContainerAs<Text>(),
                                     i - 1),
                NoBreakingSpaceData::Scanning::Backward);
          }
          continue;
        }
        // NBSP is never converted from collapsible white-space/linefeed.
        wsTypeOfNonCollapsibleChar = WSType::NonCollapsibleCharacters;
        break;
      default:
        MOZ_ASSERT(!nsCRT::IsAsciiSpace(textFragment.CharAt(i - 1)));
        wsTypeOfNonCollapsibleChar = WSType::NonCollapsibleCharacters;
        break;
    }

    return Some(BoundaryData(
        EditorDOMPoint(aPoint.template ContainerAs<Text>(), i),
        *aPoint.template ContainerAs<Text>(), wsTypeOfNonCollapsibleChar));
  }

  return Nothing();
}

// static
template <typename EditorDOMPointType>
WSRunScanner::TextFragmentData::BoundaryData WSRunScanner::TextFragmentData::
    BoundaryData::ScanCollapsibleWhiteSpaceStartFrom(
        Scan aScanMode, const EditorDOMPointType& aPoint,
        NoBreakingSpaceData* aNBSPData, BlockInlineCheck aBlockInlineCheck,
        StopAtNonEditableNode aStopAtNonEditableNode,
        const Element& aAncestorLimiter) {
  MOZ_ASSERT(aPoint.IsSetAndValid());
  MOZ_ASSERT_IF(aScanMode == Scan::EditableNodes,
                // FIXME: Both values should be true here.
                HTMLEditUtils::IsSimplyEditableNode(*aPoint.GetContainer()) ==
                    HTMLEditUtils::IsSimplyEditableNode(aAncestorLimiter));

  if (aPoint.IsInTextNode() && !aPoint.IsStartOfContainer()) {
    Maybe<BoundaryData> startInTextNode =
        BoundaryData::ScanCollapsibleWhiteSpaceStartInTextNode(
            aPoint, aNBSPData, aBlockInlineCheck);
    if (startInTextNode.isSome()) {
      return startInTextNode.ref();
    }
    // The text node does not have visible character, let's keep scanning
    // preceding nodes.
    return BoundaryData::ScanCollapsibleWhiteSpaceStartFrom(
        aScanMode, EditorDOMPoint(aPoint.template ContainerAs<Text>(), 0),
        aNBSPData, aBlockInlineCheck, aStopAtNonEditableNode, aAncestorLimiter);
  }

  // Then, we need to check previous leaf node.
  const auto leafNodeTypes =
      aStopAtNonEditableNode == StopAtNonEditableNode::Yes
          ? LeafNodeTypes{LeafNodeType::LeafNodeOrNonEditableNode}
          : LeafNodeTypes{LeafNodeType::OnlyLeafNode};
  nsIContent* previousLeafContentOrBlock =
      HTMLEditUtils::GetPreviousLeafContentOrPreviousBlockElement(
          aPoint, leafNodeTypes, aBlockInlineCheck, &aAncestorLimiter);
  if (!previousLeafContentOrBlock) {
    // No previous content means that we reached the aAncestorLimiter boundary.
    return BoundaryData(
        aPoint, const_cast<Element&>(aAncestorLimiter),
        HTMLEditUtils::IsBlockElement(
            aAncestorLimiter, RespectParentBlockBoundary(aBlockInlineCheck))
            ? WSType::CurrentBlockBoundary
            : WSType::InlineEditingHostBoundary);
  }

  if (HTMLEditUtils::IsBlockElement(*previousLeafContentOrBlock,
                                    aBlockInlineCheck)) {
    return BoundaryData(aPoint, *previousLeafContentOrBlock,
                        WSType::OtherBlockBoundary);
  }

  if (!previousLeafContentOrBlock->IsText() ||
      (aStopAtNonEditableNode == StopAtNonEditableNode::Yes &&
       HTMLEditUtils::IsSimplyEditableNode(*previousLeafContentOrBlock) !=
           HTMLEditUtils::IsSimplyEditableNode(aAncestorLimiter))) {
    // it's a break or a special node, like <img>, that is not a block and
    // not a break but still serves as a terminator to ws runs.
    return BoundaryData(aPoint, *previousLeafContentOrBlock,
                        previousLeafContentOrBlock->IsHTMLElement(nsGkAtoms::br)
                            ? WSType::BRElement
                            : WSType::SpecialContent);
  }

  if (!previousLeafContentOrBlock->AsText()->TextLength()) {
    // If it's an empty text node, keep looking for its previous leaf content.
    // Note that even if the empty text node is preformatted, we should keep
    // looking for the previous one.
    return BoundaryData::ScanCollapsibleWhiteSpaceStartFrom(
        aScanMode,
        EditorDOMPointInText(previousLeafContentOrBlock->AsText(), 0),
        aNBSPData, aBlockInlineCheck, aStopAtNonEditableNode, aAncestorLimiter);
  }

  Maybe<BoundaryData> startInTextNode =
      BoundaryData::ScanCollapsibleWhiteSpaceStartInTextNode(
          EditorDOMPointInText::AtEndOf(*previousLeafContentOrBlock->AsText()),
          aNBSPData, aBlockInlineCheck);
  if (startInTextNode.isSome()) {
    return startInTextNode.ref();
  }

  // The text node does not have visible character, let's keep scanning
  // preceding nodes.
  return BoundaryData::ScanCollapsibleWhiteSpaceStartFrom(
      aScanMode, EditorDOMPointInText(previousLeafContentOrBlock->AsText(), 0),
      aNBSPData, aBlockInlineCheck, aStopAtNonEditableNode, aAncestorLimiter);
}

// static
template <typename EditorDOMPointType>
Maybe<WSRunScanner::TextFragmentData::BoundaryData> WSRunScanner::
    TextFragmentData::BoundaryData::ScanCollapsibleWhiteSpaceEndInTextNode(
        const EditorDOMPointType& aPoint, NoBreakingSpaceData* aNBSPData,
        BlockInlineCheck aBlockInlineCheck) {
  MOZ_ASSERT(aPoint.IsSetAndValid());
  MOZ_DIAGNOSTIC_ASSERT(aPoint.IsInTextNode());

  const bool isWhiteSpaceCollapsible = !EditorUtils::IsWhiteSpacePreformatted(
      *aPoint.template ContainerAs<Text>());
  const bool isNewLineCollapsible =
      !EditorUtils::IsNewLinePreformatted(*aPoint.template ContainerAs<Text>());
  const nsTextFragment& textFragment =
      aPoint.template ContainerAs<Text>()->TextFragment();
  for (uint32_t i = aPoint.Offset(); i < textFragment.GetLength(); i++) {
    WSType wsTypeOfNonCollapsibleChar;
    switch (textFragment.CharAt(i)) {
      case HTMLEditUtils::kSpace:
      case HTMLEditUtils::kCarriageReturn:
      case HTMLEditUtils::kTab:
        if (isWhiteSpaceCollapsible) {
          continue;  // collapsible white-space or invisible white-space.
        }
        // preformatted white-space.
        wsTypeOfNonCollapsibleChar = WSType::NonCollapsibleCharacters;
        break;
      case HTMLEditUtils::kNewLine:
        if (isNewLineCollapsible) {
          continue;  // collapsible linefeed.
        }
        // preformatted linefeed.
        wsTypeOfNonCollapsibleChar = WSType::PreformattedLineBreak;
        break;
      case HTMLEditUtils::kNBSP:
        if (isWhiteSpaceCollapsible) {
          if (aNBSPData) {
            aNBSPData->NotifyNBSP(
                EditorDOMPointInText(aPoint.template ContainerAs<Text>(), i),
                NoBreakingSpaceData::Scanning::Forward);
          }
          continue;
        }
        // NBSP is never converted from collapsible white-space/linefeed.
        wsTypeOfNonCollapsibleChar = WSType::NonCollapsibleCharacters;
        break;
      default:
        MOZ_ASSERT(!nsCRT::IsAsciiSpace(textFragment.CharAt(i)));
        wsTypeOfNonCollapsibleChar = WSType::NonCollapsibleCharacters;
        break;
    }

    return Some(BoundaryData(
        EditorDOMPoint(aPoint.template ContainerAs<Text>(), i),
        *aPoint.template ContainerAs<Text>(), wsTypeOfNonCollapsibleChar));
  }

  return Nothing();
}

// static
template <typename EditorDOMPointType>
WSRunScanner::TextFragmentData::BoundaryData
WSRunScanner::TextFragmentData::BoundaryData::ScanCollapsibleWhiteSpaceEndFrom(
    Scan aScanMode, const EditorDOMPointType& aPoint,
    NoBreakingSpaceData* aNBSPData, BlockInlineCheck aBlockInlineCheck,
    StopAtNonEditableNode aStopAtNonEditableNode,
    const Element& aAncestorLimiter) {
  MOZ_ASSERT(aPoint.IsSetAndValid());
  MOZ_ASSERT_IF(aScanMode == Scan::EditableNodes,
                // FIXME: Both values should be true here.
                HTMLEditUtils::IsSimplyEditableNode(*aPoint.GetContainer()) ==
                    HTMLEditUtils::IsSimplyEditableNode(aAncestorLimiter));

  if (aPoint.IsInTextNode() && !aPoint.IsEndOfContainer()) {
    Maybe<BoundaryData> endInTextNode =
        BoundaryData::ScanCollapsibleWhiteSpaceEndInTextNode(aPoint, aNBSPData,
                                                             aBlockInlineCheck);
    if (endInTextNode.isSome()) {
      return endInTextNode.ref();
    }
    // The text node does not have visible character, let's keep scanning
    // following nodes.
    return BoundaryData::ScanCollapsibleWhiteSpaceEndFrom(
        aScanMode,
        EditorDOMPointInText::AtEndOf(*aPoint.template ContainerAs<Text>()),
        aNBSPData, aBlockInlineCheck, aStopAtNonEditableNode, aAncestorLimiter);
  }

  // Then, we need to check next leaf node.
  const auto leafNodeTypes =
      aStopAtNonEditableNode == StopAtNonEditableNode::Yes
          ? LeafNodeTypes{LeafNodeType::LeafNodeOrNonEditableNode}
          : LeafNodeTypes{LeafNodeType::OnlyLeafNode};
  nsIContent* nextLeafContentOrBlock =
      HTMLEditUtils::GetNextLeafContentOrNextBlockElement(
          aPoint, leafNodeTypes, aBlockInlineCheck, &aAncestorLimiter);
  if (!nextLeafContentOrBlock) {
    // No next content means that we reached aAncestorLimiter boundary.
    return BoundaryData(
        aPoint.template To<EditorDOMPoint>(),
        const_cast<Element&>(aAncestorLimiter),
        HTMLEditUtils::IsBlockElement(
            aAncestorLimiter, RespectParentBlockBoundary(aBlockInlineCheck))
            ? WSType::CurrentBlockBoundary
            : WSType::InlineEditingHostBoundary);
  }

  if (HTMLEditUtils::IsBlockElement(*nextLeafContentOrBlock,
                                    aBlockInlineCheck)) {
    // we encountered a new block.  therefore no more ws.
    return BoundaryData(aPoint, *nextLeafContentOrBlock,
                        WSType::OtherBlockBoundary);
  }

  if (!nextLeafContentOrBlock->IsText() ||
      (aStopAtNonEditableNode == StopAtNonEditableNode::Yes &&
       HTMLEditUtils::IsSimplyEditableNode(*nextLeafContentOrBlock) !=
           HTMLEditUtils::IsSimplyEditableNode(aAncestorLimiter))) {
    // we encountered a break or a special node, like <img>,
    // that is not a block and not a break but still
    // serves as a terminator to ws runs.
    return BoundaryData(aPoint, *nextLeafContentOrBlock,
                        nextLeafContentOrBlock->IsHTMLElement(nsGkAtoms::br)
                            ? WSType::BRElement
                            : WSType::SpecialContent);
  }

  if (!nextLeafContentOrBlock->AsText()->TextFragment().GetLength()) {
    // If it's an empty text node, keep looking for its next leaf content.
    // Note that even if the empty text node is preformatted, we should keep
    // looking for the next one.
    return BoundaryData::ScanCollapsibleWhiteSpaceEndFrom(
        aScanMode, EditorDOMPointInText(nextLeafContentOrBlock->AsText(), 0),
        aNBSPData, aBlockInlineCheck, aStopAtNonEditableNode, aAncestorLimiter);
  }

  Maybe<BoundaryData> endInTextNode =
      BoundaryData::ScanCollapsibleWhiteSpaceEndInTextNode(
          EditorDOMPointInText(nextLeafContentOrBlock->AsText(), 0), aNBSPData,
          aBlockInlineCheck);
  if (endInTextNode.isSome()) {
    return endInTextNode.ref();
  }

  // The text node does not have visible character, let's keep scanning
  // following nodes.
  return BoundaryData::ScanCollapsibleWhiteSpaceEndFrom(
      aScanMode,
      EditorDOMPointInText::AtEndOf(*nextLeafContentOrBlock->AsText()),
      aNBSPData, aBlockInlineCheck, aStopAtNonEditableNode, aAncestorLimiter);
}

const EditorDOMRange&
WSRunScanner::TextFragmentData::InvisibleLeadingWhiteSpaceRangeRef() const {
  if (mLeadingWhiteSpaceRange.isSome()) {
    return mLeadingWhiteSpaceRange.ref();
  }

  // If it's start of line, there is no invisible leading white-spaces.
  if (!StartsFromHardLineBreak() && !StartsFromInlineEditingHostBoundary()) {
    mLeadingWhiteSpaceRange.emplace();
    return mLeadingWhiteSpaceRange.ref();
  }

  // If there is no NBSP, all of the given range is leading white-spaces.
  // Note that this result may be collapsed if there is no leading white-spaces.
  if (!mNBSPData.FoundNBSP()) {
    MOZ_ASSERT(mStart.PointRef().IsSet() || mEnd.PointRef().IsSet());
    mLeadingWhiteSpaceRange.emplace(mStart.PointRef(), mEnd.PointRef());
    return mLeadingWhiteSpaceRange.ref();
  }

  MOZ_ASSERT(mNBSPData.LastPointRef().IsSetAndValid());

  // Even if the first NBSP is the start, i.e., there is no invisible leading
  // white-space, return collapsed range.
  mLeadingWhiteSpaceRange.emplace(mStart.PointRef(), mNBSPData.FirstPointRef());
  return mLeadingWhiteSpaceRange.ref();
}

const EditorDOMRange&
WSRunScanner::TextFragmentData::InvisibleTrailingWhiteSpaceRangeRef() const {
  if (mTrailingWhiteSpaceRange.isSome()) {
    return mTrailingWhiteSpaceRange.ref();
  }

  // If it's not immediately before a block boundary, there is no invisible
  // trailing white-spaces.  Note that a collapsible white-space before a <br>
  // element or a preformatted linefeed is visible.
  if (!EndsByBlockBoundary() && !EndsByInlineEditingHostBoundary()) {
    mTrailingWhiteSpaceRange.emplace();
    return mTrailingWhiteSpaceRange.ref();
  }

  // If there is no NBSP, all of the given range is trailing white-spaces.
  // Note that this result may be collapsed if there is no trailing white-
  // spaces.
  if (!mNBSPData.FoundNBSP()) {
    MOZ_ASSERT(mStart.PointRef().IsSet() || mEnd.PointRef().IsSet());
    mTrailingWhiteSpaceRange.emplace(mStart.PointRef(), mEnd.PointRef());
    return mTrailingWhiteSpaceRange.ref();
  }

  MOZ_ASSERT(mNBSPData.LastPointRef().IsSetAndValid());

  // If last NBSP is immediately before the end, there is no trailing white-
  // spaces.
  if (mEnd.PointRef().IsSet() &&
      mNBSPData.LastPointRef().GetContainer() ==
          mEnd.PointRef().GetContainer() &&
      mNBSPData.LastPointRef().Offset() == mEnd.PointRef().Offset() - 1) {
    mTrailingWhiteSpaceRange.emplace();
    return mTrailingWhiteSpaceRange.ref();
  }

  // Otherwise, the may be some trailing white-spaces.
  MOZ_ASSERT(!mNBSPData.LastPointRef().IsEndOfContainer());
  mTrailingWhiteSpaceRange.emplace(mNBSPData.LastPointRef().NextPoint(),
                                   mEnd.PointRef());
  return mTrailingWhiteSpaceRange.ref();
}

EditorDOMRangeInTexts
WSRunScanner::TextFragmentData::GetNonCollapsedRangeInTexts(
    const EditorDOMRange& aRange) const {
  if (!aRange.IsPositioned()) {
    return EditorDOMRangeInTexts();
  }
  if (aRange.Collapsed()) {
    // If collapsed, we can do nothing.
    return EditorDOMRangeInTexts();
  }
  if (aRange.IsInTextNodes()) {
    // Note that this may return a range which don't include any invisible
    // white-spaces due to empty text nodes.
    return aRange.GetAsInTexts();
  }

  const auto firstPoint =
      aRange.StartRef().IsInTextNode()
          ? aRange.StartRef().AsInText()
          : GetInclusiveNextCharPoint<EditorDOMPointInText>(
                aRange.StartRef(),
                ShouldIgnoreNonEditableSiblingsOrDescendants(mScanMode));
  if (!firstPoint.IsSet()) {
    return EditorDOMRangeInTexts();
  }
  EditorDOMPointInText endPoint;
  if (aRange.EndRef().IsInTextNode()) {
    endPoint = aRange.EndRef().AsInText();
  } else {
    // FYI: GetPreviousCharPoint() returns last character's point of preceding
    //      text node if it's not empty, but we need end of the text node here.
    endPoint = GetPreviousCharPoint<EditorDOMPointInText>(
        aRange.EndRef(),
        ShouldIgnoreNonEditableSiblingsOrDescendants(mScanMode));
    if (endPoint.IsSet() && endPoint.IsAtLastContent()) {
      MOZ_ALWAYS_TRUE(endPoint.AdvanceOffset());
    }
  }
  if (!endPoint.IsSet() || firstPoint == endPoint) {
    return EditorDOMRangeInTexts();
  }
  return EditorDOMRangeInTexts(firstPoint, endPoint);
}

const WSRunScanner::VisibleWhiteSpacesData&
WSRunScanner::TextFragmentData::VisibleWhiteSpacesDataRef() const {
  if (mVisibleWhiteSpacesData.isSome()) {
    return mVisibleWhiteSpacesData.ref();
  }

  {
    // If all things are obviously visible, we can return range for all of the
    // things quickly.
    const bool mayHaveInvisibleLeadingSpace =
        !StartsFromNonCollapsibleCharacters() && !StartsFromSpecialContent();
    const bool mayHaveInvisibleTrailingWhiteSpace =
        !EndsByNonCollapsibleCharacters() && !EndsBySpecialContent() &&
        !EndsByBRElement() && !EndsByInvisiblePreformattedLineBreak();

    if (!mayHaveInvisibleLeadingSpace && !mayHaveInvisibleTrailingWhiteSpace) {
      VisibleWhiteSpacesData visibleWhiteSpaces;
      if (mStart.PointRef().IsSet()) {
        visibleWhiteSpaces.SetStartPoint(mStart.PointRef());
      }
      visibleWhiteSpaces.SetStartFrom(mStart.RawReason());
      if (mEnd.PointRef().IsSet()) {
        visibleWhiteSpaces.SetEndPoint(mEnd.PointRef());
      }
      visibleWhiteSpaces.SetEndBy(mEnd.RawReason());
      mVisibleWhiteSpacesData.emplace(visibleWhiteSpaces);
      return mVisibleWhiteSpacesData.ref();
    }
  }

  // If all of the range is invisible leading or trailing white-spaces,
  // there is no visible content.
  const EditorDOMRange& leadingWhiteSpaceRange =
      InvisibleLeadingWhiteSpaceRangeRef();
  const bool maybeHaveLeadingWhiteSpaces =
      leadingWhiteSpaceRange.StartRef().IsSet() ||
      leadingWhiteSpaceRange.EndRef().IsSet();
  if (maybeHaveLeadingWhiteSpaces &&
      leadingWhiteSpaceRange.StartRef() == mStart.PointRef() &&
      leadingWhiteSpaceRange.EndRef() == mEnd.PointRef()) {
    mVisibleWhiteSpacesData.emplace(VisibleWhiteSpacesData());
    return mVisibleWhiteSpacesData.ref();
  }
  const EditorDOMRange& trailingWhiteSpaceRange =
      InvisibleTrailingWhiteSpaceRangeRef();
  const bool maybeHaveTrailingWhiteSpaces =
      trailingWhiteSpaceRange.StartRef().IsSet() ||
      trailingWhiteSpaceRange.EndRef().IsSet();
  if (maybeHaveTrailingWhiteSpaces &&
      trailingWhiteSpaceRange.StartRef() == mStart.PointRef() &&
      trailingWhiteSpaceRange.EndRef() == mEnd.PointRef()) {
    mVisibleWhiteSpacesData.emplace(VisibleWhiteSpacesData());
    return mVisibleWhiteSpacesData.ref();
  }

  if (!StartsFromHardLineBreak() && !StartsFromInlineEditingHostBoundary()) {
    VisibleWhiteSpacesData visibleWhiteSpaces;
    if (mStart.PointRef().IsSet()) {
      visibleWhiteSpaces.SetStartPoint(mStart.PointRef());
    }
    visibleWhiteSpaces.SetStartFrom(mStart.RawReason());
    if (!maybeHaveTrailingWhiteSpaces) {
      visibleWhiteSpaces.SetEndPoint(mEnd.PointRef());
      visibleWhiteSpaces.SetEndBy(mEnd.RawReason());
      mVisibleWhiteSpacesData = Some(visibleWhiteSpaces);
      return mVisibleWhiteSpacesData.ref();
    }
    if (trailingWhiteSpaceRange.StartRef().IsSet()) {
      visibleWhiteSpaces.SetEndPoint(trailingWhiteSpaceRange.StartRef());
    }
    visibleWhiteSpaces.SetEndByTrailingWhiteSpaces();
    mVisibleWhiteSpacesData.emplace(visibleWhiteSpaces);
    return mVisibleWhiteSpacesData.ref();
  }

  MOZ_ASSERT(StartsFromHardLineBreak() ||
             StartsFromInlineEditingHostBoundary());
  MOZ_ASSERT(maybeHaveLeadingWhiteSpaces);

  VisibleWhiteSpacesData visibleWhiteSpaces;
  if (leadingWhiteSpaceRange.EndRef().IsSet()) {
    visibleWhiteSpaces.SetStartPoint(leadingWhiteSpaceRange.EndRef());
  }
  visibleWhiteSpaces.SetStartFromLeadingWhiteSpaces();
  if (!EndsByBlockBoundary() && !EndsByInlineEditingHostBoundary()) {
    // then no trailing ws.  this normal run ends the overall ws run.
    if (mEnd.PointRef().IsSet()) {
      visibleWhiteSpaces.SetEndPoint(mEnd.PointRef());
    }
    visibleWhiteSpaces.SetEndBy(mEnd.RawReason());
    mVisibleWhiteSpacesData.emplace(visibleWhiteSpaces);
    return mVisibleWhiteSpacesData.ref();
  }

  MOZ_ASSERT(EndsByBlockBoundary() || EndsByInlineEditingHostBoundary());

  if (!maybeHaveTrailingWhiteSpaces) {
    // normal ws runs right up to adjacent block (nbsp next to block)
    visibleWhiteSpaces.SetEndPoint(mEnd.PointRef());
    visibleWhiteSpaces.SetEndBy(mEnd.RawReason());
    mVisibleWhiteSpacesData.emplace(visibleWhiteSpaces);
    return mVisibleWhiteSpacesData.ref();
  }

  if (trailingWhiteSpaceRange.StartRef().IsSet()) {
    visibleWhiteSpaces.SetEndPoint(trailingWhiteSpaceRange.StartRef());
  }
  visibleWhiteSpaces.SetEndByTrailingWhiteSpaces();
  mVisibleWhiteSpacesData.emplace(visibleWhiteSpaces);
  return mVisibleWhiteSpacesData.ref();
}

ReplaceRangeData
WSRunScanner::TextFragmentData::GetReplaceRangeDataAtEndOfDeletionRange(
    const TextFragmentData& aTextFragmentDataAtStartToDelete) const {
  const EditorDOMPoint& startToDelete =
      aTextFragmentDataAtStartToDelete.ScanStartRef();
  const EditorDOMPoint& endToDelete = mScanStartPoint;

  MOZ_ASSERT(startToDelete.IsSetAndValid());
  MOZ_ASSERT(endToDelete.IsSetAndValid());
  MOZ_ASSERT(startToDelete.EqualsOrIsBefore(endToDelete));

  if (EndRef().EqualsOrIsBefore(endToDelete)) {
    return ReplaceRangeData();
  }

  // If deleting range is followed by invisible trailing white-spaces, we need
  // to remove it for making them not visible.
  const EditorDOMRange invisibleTrailingWhiteSpaceRangeAtEnd =
      GetNewInvisibleTrailingWhiteSpaceRangeIfSplittingAt(endToDelete);
  if (invisibleTrailingWhiteSpaceRangeAtEnd.IsPositioned()) {
    if (invisibleTrailingWhiteSpaceRangeAtEnd.Collapsed()) {
      return ReplaceRangeData();
    }
    // XXX Why don't we remove all invisible white-spaces?
    MOZ_ASSERT(invisibleTrailingWhiteSpaceRangeAtEnd.StartRef() == endToDelete);
    return ReplaceRangeData(invisibleTrailingWhiteSpaceRangeAtEnd, u""_ns);
  }

  // If end of the deleting range is followed by visible white-spaces which
  // is not preformatted, we might need to replace the following ASCII
  // white-spaces with an NBSP.
  const VisibleWhiteSpacesData& nonPreformattedVisibleWhiteSpacesAtEnd =
      VisibleWhiteSpacesDataRef();
  if (!nonPreformattedVisibleWhiteSpacesAtEnd.IsInitialized()) {
    return ReplaceRangeData();
  }
  const PointPosition pointPositionWithNonPreformattedVisibleWhiteSpacesAtEnd =
      nonPreformattedVisibleWhiteSpacesAtEnd.ComparePoint(endToDelete);
  if (pointPositionWithNonPreformattedVisibleWhiteSpacesAtEnd !=
          PointPosition::StartOfFragment &&
      pointPositionWithNonPreformattedVisibleWhiteSpacesAtEnd !=
          PointPosition::MiddleOfFragment) {
    return ReplaceRangeData();
  }
  // If start of deleting range follows white-spaces or end of delete
  // will be start of a line, the following text cannot start with an
  // ASCII white-space for keeping it visible.
  if (!aTextFragmentDataAtStartToDelete
           .FollowingContentMayBecomeFirstVisibleContent(startToDelete)) {
    return ReplaceRangeData();
  }
  auto nextCharOfStartOfEnd = GetInclusiveNextCharPoint<EditorDOMPointInText>(
      endToDelete, ShouldIgnoreNonEditableSiblingsOrDescendants(mScanMode));
  if (!nextCharOfStartOfEnd.IsSet() ||
      nextCharOfStartOfEnd.IsEndOfContainer() ||
      !nextCharOfStartOfEnd.IsCharCollapsibleASCIISpace()) {
    return ReplaceRangeData();
  }
  if (nextCharOfStartOfEnd.IsStartOfContainer() ||
      nextCharOfStartOfEnd.IsPreviousCharCollapsibleASCIISpace()) {
    nextCharOfStartOfEnd =
        aTextFragmentDataAtStartToDelete
            .GetFirstASCIIWhiteSpacePointCollapsedTo<EditorDOMPointInText>(
                nextCharOfStartOfEnd, nsIEditor::eNone,
                ShouldIgnoreNonEditableSiblingsOrDescendants(mScanMode));
  }
  const auto endOfCollapsibleASCIIWhiteSpaces =
      aTextFragmentDataAtStartToDelete
          .GetEndOfCollapsibleASCIIWhiteSpaces<EditorDOMPointInText>(
              nextCharOfStartOfEnd, nsIEditor::eNone,
              ShouldIgnoreNonEditableSiblingsOrDescendants(mScanMode));
  return ReplaceRangeData(nextCharOfStartOfEnd,
                          endOfCollapsibleASCIIWhiteSpaces,
                          nsDependentSubstring(&HTMLEditUtils::kNBSP, 1));
}

ReplaceRangeData
WSRunScanner::TextFragmentData::GetReplaceRangeDataAtStartOfDeletionRange(
    const TextFragmentData& aTextFragmentDataAtEndToDelete) const {
  const EditorDOMPoint& startToDelete = mScanStartPoint;
  const EditorDOMPoint& endToDelete =
      aTextFragmentDataAtEndToDelete.ScanStartRef();

  MOZ_ASSERT(startToDelete.IsSetAndValid());
  MOZ_ASSERT(endToDelete.IsSetAndValid());
  MOZ_ASSERT(startToDelete.EqualsOrIsBefore(endToDelete));

  if (startToDelete.EqualsOrIsBefore(StartRef())) {
    return ReplaceRangeData();
  }

  const EditorDOMRange invisibleLeadingWhiteSpaceRangeAtStart =
      GetNewInvisibleLeadingWhiteSpaceRangeIfSplittingAt(startToDelete);

  // If deleting range follows invisible leading white-spaces, we need to
  // remove them for making them not visible.
  if (invisibleLeadingWhiteSpaceRangeAtStart.IsPositioned()) {
    if (invisibleLeadingWhiteSpaceRangeAtStart.Collapsed()) {
      return ReplaceRangeData();
    }

    // XXX Why don't we remove all leading white-spaces?
    return ReplaceRangeData(invisibleLeadingWhiteSpaceRangeAtStart, u""_ns);
  }

  // If start of the deleting range follows visible white-spaces which is not
  // preformatted, we might need to replace previous ASCII white-spaces with
  // an NBSP.
  const VisibleWhiteSpacesData& nonPreformattedVisibleWhiteSpacesAtStart =
      VisibleWhiteSpacesDataRef();
  if (!nonPreformattedVisibleWhiteSpacesAtStart.IsInitialized()) {
    return ReplaceRangeData();
  }
  const PointPosition
      pointPositionWithNonPreformattedVisibleWhiteSpacesAtStart =
          nonPreformattedVisibleWhiteSpacesAtStart.ComparePoint(startToDelete);
  if (pointPositionWithNonPreformattedVisibleWhiteSpacesAtStart !=
          PointPosition::MiddleOfFragment &&
      pointPositionWithNonPreformattedVisibleWhiteSpacesAtStart !=
          PointPosition::EndOfFragment) {
    return ReplaceRangeData();
  }
  // If end of the deleting range is (was) followed by white-spaces or
  // previous character of start of deleting range will be immediately
  // before a block boundary, the text cannot ends with an ASCII white-space
  // for keeping it visible.
  if (!aTextFragmentDataAtEndToDelete.PrecedingContentMayBecomeInvisible(
          endToDelete)) {
    return ReplaceRangeData();
  }
  auto atPreviousCharOfStart = GetPreviousCharPoint<EditorDOMPointInText>(
      startToDelete, ShouldIgnoreNonEditableSiblingsOrDescendants(mScanMode));
  if (!atPreviousCharOfStart.IsSet() ||
      atPreviousCharOfStart.IsEndOfContainer() ||
      !atPreviousCharOfStart.IsCharCollapsibleASCIISpace()) {
    return ReplaceRangeData();
  }
  if (atPreviousCharOfStart.IsStartOfContainer() ||
      atPreviousCharOfStart.IsPreviousCharASCIISpace()) {
    atPreviousCharOfStart =
        GetFirstASCIIWhiteSpacePointCollapsedTo<EditorDOMPointInText>(
            atPreviousCharOfStart, nsIEditor::eNone,
            ShouldIgnoreNonEditableSiblingsOrDescendants(mScanMode));
  }
  const auto endOfCollapsibleASCIIWhiteSpaces =
      GetEndOfCollapsibleASCIIWhiteSpaces<EditorDOMPointInText>(
          atPreviousCharOfStart, nsIEditor::eNone,
          ShouldIgnoreNonEditableSiblingsOrDescendants(mScanMode));
  return ReplaceRangeData(atPreviousCharOfStart,
                          endOfCollapsibleASCIIWhiteSpaces,
                          nsDependentSubstring(&HTMLEditUtils::kNBSP, 1));
}

// static
template <typename EditorDOMPointType, typename PT, typename CT>
EditorDOMPointType WSRunScanner::TextFragmentData::GetInclusiveNextCharPoint(
    const EditorDOMPointBase<PT, CT>& aPoint,
    BlockInlineCheck aBlockInlineCheck,
    IgnoreNonEditableNodes aIgnoreNonEditableNodes,
    const nsIContent* aFollowingLimiterContent /* = nullptr */) {
  MOZ_ASSERT(aPoint.IsSetAndValid());

  if (NS_WARN_IF(!aPoint.IsInContentNode())) {
    return EditorDOMPointType();
  }

  const EditorRawDOMPoint point = [&]() {
    nsIContent* const child =
        aPoint.CanContainerHaveChildren() ? aPoint.GetChild() : nullptr;
    if (!child) {
      return aPoint.template To<EditorRawDOMPoint>();
    }
    if (!child->HasChildNodes()) {
      return EditorRawDOMPoint(child, 0);
    }
    // FIXME: This may skip aFollowingLimiterContent, so, this utility should
    // take a stopper param.
    // FIXME: I think we should stop looking for a leaf node if there is a child
    // block because end reason content should not be the other side of the
    // following block boundary.
    nsIContent* const leafContent = HTMLEditUtils::GetFirstLeafContent(
        *child, {LeafNodeType::OnlyLeafNode});
    if (NS_WARN_IF(!leafContent)) {
      return EditorRawDOMPoint();
    }
    return EditorRawDOMPoint(leafContent, 0);
  }();
  if (!point.IsSet()) {
    return EditorDOMPointType();
  }

  // If it points a character in a text node, return it.
  // XXX For the performance, this does not check whether the container
  //     is outside of our range.
  if (point.IsInTextNode() &&
      (aIgnoreNonEditableNodes == IgnoreNonEditableNodes::No ||
       HTMLEditUtils::IsSimplyEditableNode(*point.GetContainer())) &&
      !point.IsEndOfContainer()) {
    return EditorDOMPointType(point.ContainerAs<Text>(), point.Offset());
  }

  if (point.GetContainer() == aFollowingLimiterContent) {
    return EditorDOMPointType();
  }

  const Element* const
      editableBlockElementOrInlineEditingHostOrNonEditableRootElement =
          HTMLEditUtils::GetInclusiveAncestorElement(
              *aPoint.template ContainerAs<nsIContent>(),
              HTMLEditUtils::IsSimplyEditableNode(
                  *aPoint.template ContainerAs<nsIContent>())
                  ? kScanEditableRootAncestorTypes
                  : kScanAnyRootAncestorTypes,
              aBlockInlineCheck);
  if (NS_WARN_IF(
          !editableBlockElementOrInlineEditingHostOrNonEditableRootElement)) {
    return EditorDOMPointType();
  }

  const auto leafNodeTypes =
      aIgnoreNonEditableNodes == IgnoreNonEditableNodes::Yes
          ? LeafNodeTypes(LeafNodeType::LeafNodeOrNonEditableNode)
          : LeafNodeTypes(LeafNodeType::OnlyLeafNode);
  for (nsIContent* nextContent =
           HTMLEditUtils::GetNextLeafContentOrNextBlockElement(
               *point.ContainerAs<nsIContent>(), leafNodeTypes,
               aBlockInlineCheck,
               editableBlockElementOrInlineEditingHostOrNonEditableRootElement);
       nextContent;
       nextContent = HTMLEditUtils::GetNextLeafContentOrNextBlockElement(
           *nextContent, leafNodeTypes, aBlockInlineCheck,
           editableBlockElementOrInlineEditingHostOrNonEditableRootElement)) {
    if (!nextContent->IsText() ||
        (aIgnoreNonEditableNodes == IgnoreNonEditableNodes::Yes &&
         !HTMLEditUtils::IsSimplyEditableNode(*nextContent))) {
      if (nextContent == aFollowingLimiterContent) {
        break;  // Reached end of current runs.
      }
      continue;
    }
    return EditorDOMPointType(nextContent->AsText(), 0);
  }
  return EditorDOMPointType();
}

// static
template <typename EditorDOMPointType, typename PT, typename CT>
EditorDOMPointType WSRunScanner::TextFragmentData::GetPreviousCharPoint(
    const EditorDOMPointBase<PT, CT>& aPoint,
    BlockInlineCheck aBlockInlineCheck,
    IgnoreNonEditableNodes aIgnoreNonEditableNodes,
    const nsIContent* aPrecedingLimiterContent /* = nullptr */) {
  MOZ_ASSERT(aPoint.IsSetAndValid());

  if (NS_WARN_IF(!aPoint.IsInContentNode())) {
    return EditorDOMPointType();
  }

  const EditorRawDOMPoint point = [&]() {
    nsIContent* const previousChild = aPoint.CanContainerHaveChildren()
                                          ? aPoint.GetPreviousSiblingOfChild()
                                          : nullptr;
    if (!previousChild) {
      return aPoint.template To<EditorRawDOMPoint>();
    }
    if (!previousChild->HasChildren()) {
      return EditorRawDOMPoint::AtEndOf(*previousChild);
    }
    // FIXME: This may skip aPrecedingLimiterContent, so, this utility should
    // take a stopper param.
    // FIXME: I think we should stop looking for a leaf node if there is a child
    // block because end reason content should not be the other side of the
    // following block boundary.
    nsIContent* const leafContent = HTMLEditUtils::GetLastLeafContent(
        *previousChild, {LeafNodeType::OnlyLeafNode});
    if (NS_WARN_IF(!leafContent)) {
      return EditorRawDOMPoint();
    }
    return EditorRawDOMPoint::AtEndOf(*leafContent);
  }();
  if (!point.IsSet()) {
    return EditorDOMPointType();
  }

  // If it points a character in a text node and it's not first character
  // in it, return its previous point.
  // XXX For the performance, this does not check whether the container
  //     is outside of our range.
  if (point.IsInTextNode() &&
      (aIgnoreNonEditableNodes == IgnoreNonEditableNodes::No ||
       HTMLEditUtils::IsSimplyEditableNode(*point.GetContainer())) &&
      !point.IsStartOfContainer()) {
    return EditorDOMPointType(point.ContainerAs<Text>(), point.Offset() - 1);
  }

  if (point.GetContainer() == aPrecedingLimiterContent) {
    return EditorDOMPointType();
  }

  const Element* const
      editableBlockElementOrInlineEditingHostOrNonEditableRootElement =
          HTMLEditUtils::GetInclusiveAncestorElement(
              *aPoint.template ContainerAs<nsIContent>(),
              HTMLEditUtils::IsSimplyEditableNode(
                  *aPoint.template ContainerAs<nsIContent>())
                  ? kScanEditableRootAncestorTypes
                  : kScanAnyRootAncestorTypes,
              aBlockInlineCheck);
  if (NS_WARN_IF(
          !editableBlockElementOrInlineEditingHostOrNonEditableRootElement)) {
    return EditorDOMPointType();
  }

  const auto leafNodeTypes =
      aIgnoreNonEditableNodes == IgnoreNonEditableNodes::Yes
          ? LeafNodeTypes(LeafNodeType::LeafNodeOrNonEditableNode)
          : LeafNodeTypes(LeafNodeType::OnlyLeafNode);
  for (
      nsIContent* previousContent =
          HTMLEditUtils::GetPreviousLeafContentOrPreviousBlockElement(
              *point.ContainerAs<nsIContent>(), leafNodeTypes,
              aBlockInlineCheck,
              editableBlockElementOrInlineEditingHostOrNonEditableRootElement);
      previousContent;
      previousContent =
          HTMLEditUtils::GetPreviousLeafContentOrPreviousBlockElement(
              *previousContent, leafNodeTypes, aBlockInlineCheck,
              editableBlockElementOrInlineEditingHostOrNonEditableRootElement)) {
    if (!previousContent->IsText() ||
        (aIgnoreNonEditableNodes == IgnoreNonEditableNodes::Yes &&
         !HTMLEditUtils::IsSimplyEditableNode(*previousContent))) {
      if (previousContent == aPrecedingLimiterContent) {
        break;  // Reached start of current runs.
      }
      continue;
    }
    return EditorDOMPointType(previousContent->AsText(),
                              previousContent->AsText()->TextLength()
                                  ? previousContent->AsText()->TextLength() - 1
                                  : 0);
  }
  return EditorDOMPointType();
}

// static
template <typename EditorDOMPointType>
EditorDOMPointType
WSRunScanner::TextFragmentData::GetEndOfCollapsibleASCIIWhiteSpaces(
    const EditorDOMPointInText& aPointAtASCIIWhiteSpace,
    nsIEditor::EDirection aDirectionToDelete,
    BlockInlineCheck aBlockInlineCheck,
    IgnoreNonEditableNodes aIgnoreNonEditableNodes,
    const nsIContent* aFollowingLimiterContent /* = nullptr */) {
  MOZ_ASSERT(aDirectionToDelete == nsIEditor::eNone ||
             aDirectionToDelete == nsIEditor::eNext ||
             aDirectionToDelete == nsIEditor::ePrevious);
  MOZ_ASSERT(aPointAtASCIIWhiteSpace.IsSet());
  MOZ_ASSERT(!aPointAtASCIIWhiteSpace.IsEndOfContainer());
  MOZ_ASSERT_IF(!EditorUtils::IsNewLinePreformatted(
                    *aPointAtASCIIWhiteSpace.ContainerAs<Text>()),
                aPointAtASCIIWhiteSpace.IsCharCollapsibleASCIISpace());
  MOZ_ASSERT_IF(EditorUtils::IsNewLinePreformatted(
                    *aPointAtASCIIWhiteSpace.ContainerAs<Text>()),
                aPointAtASCIIWhiteSpace.IsCharASCIISpace());

  // If we're deleting text forward and the next visible character is first
  // preformatted new line but white-spaces can be collapsed, we need to
  // delete its following collapsible white-spaces too.
  bool hasSeenPreformattedNewLine =
      aPointAtASCIIWhiteSpace.IsCharPreformattedNewLine();
  auto NeedToScanFollowingWhiteSpaces =
      [&hasSeenPreformattedNewLine, &aDirectionToDelete](
          const EditorDOMPointInText& aAtNextVisibleCharacter) -> bool {
    MOZ_ASSERT(!aAtNextVisibleCharacter.IsEndOfContainer());
    return !hasSeenPreformattedNewLine &&
           aDirectionToDelete == nsIEditor::eNext &&
           aAtNextVisibleCharacter
               .IsCharPreformattedNewLineCollapsedWithWhiteSpaces();
  };
  auto ScanNextNonCollapsibleChar =
      [&hasSeenPreformattedNewLine, &NeedToScanFollowingWhiteSpaces](
          const EditorDOMPointInText& aPoint) -> EditorDOMPointInText {
    Maybe<uint32_t> nextVisibleCharOffset =
        HTMLEditUtils::GetNextNonCollapsibleCharOffset(aPoint);
    if (!nextVisibleCharOffset.isSome()) {
      return EditorDOMPointInText();  // Keep scanning following text nodes
    }
    EditorDOMPointInText atNextVisibleChar(aPoint.ContainerAs<Text>(),
                                           nextVisibleCharOffset.value());
    if (!NeedToScanFollowingWhiteSpaces(atNextVisibleChar)) {
      return atNextVisibleChar;
    }
    hasSeenPreformattedNewLine |= atNextVisibleChar.IsCharPreformattedNewLine();
    nextVisibleCharOffset =
        HTMLEditUtils::GetNextNonCollapsibleCharOffset(atNextVisibleChar);
    if (nextVisibleCharOffset.isSome()) {
      MOZ_ASSERT(aPoint.ContainerAs<Text>() ==
                 atNextVisibleChar.ContainerAs<Text>());
      return EditorDOMPointInText(atNextVisibleChar.ContainerAs<Text>(),
                                  nextVisibleCharOffset.value());
    }
    return EditorDOMPointInText();  // Keep scanning following text nodes
  };

  // If it's not the last character in the text node, let's scan following
  // characters in it.
  if (!aPointAtASCIIWhiteSpace.IsAtLastContent()) {
    const EditorDOMPointInText atNextVisibleChar(
        ScanNextNonCollapsibleChar(aPointAtASCIIWhiteSpace));
    if (atNextVisibleChar.IsSet()) {
      return atNextVisibleChar.To<EditorDOMPointType>();
    }
  }

  // Otherwise, i.e., the text node ends with ASCII white-space, keep scanning
  // the following text nodes.
  // XXX Perhaps, we should stop scanning if there is non-editable and visible
  //     content.
  EditorDOMPointInText afterLastWhiteSpace = EditorDOMPointInText::AtEndOf(
      *aPointAtASCIIWhiteSpace.ContainerAs<Text>());
  for (EditorDOMPointInText atEndOfPreviousTextNode = afterLastWhiteSpace;;) {
    const auto atStartOfNextTextNode =
        TextFragmentData::GetInclusiveNextCharPoint<EditorDOMPointInText>(
            atEndOfPreviousTextNode, aBlockInlineCheck, aIgnoreNonEditableNodes,
            aFollowingLimiterContent);
    if (!atStartOfNextTextNode.IsSet()) {
      // There is no more text nodes.  Return end of the previous text node.
      return afterLastWhiteSpace.To<EditorDOMPointType>();
    }

    // We can ignore empty text nodes (even if it's preformatted).
    if (atStartOfNextTextNode.IsContainerEmpty()) {
      atEndOfPreviousTextNode = atStartOfNextTextNode;
      continue;
    }

    // If next node starts with non-white-space character or next node is
    // preformatted, return end of previous text node.  However, if it
    // starts with a preformatted linefeed but white-spaces are collapsible,
    // we need to scan following collapsible white-spaces when we're deleting
    // text forward.
    if (!atStartOfNextTextNode.IsCharCollapsibleASCIISpace() &&
        !NeedToScanFollowingWhiteSpaces(atStartOfNextTextNode)) {
      return afterLastWhiteSpace.To<EditorDOMPointType>();
    }

    // Otherwise, scan the text node.
    const EditorDOMPointInText atNextVisibleChar(
        ScanNextNonCollapsibleChar(atStartOfNextTextNode));
    if (atNextVisibleChar.IsSet()) {
      return atNextVisibleChar.To<EditorDOMPointType>();
    }

    // The next text nodes ends with white-space too.  Try next one.
    afterLastWhiteSpace = atEndOfPreviousTextNode =
        EditorDOMPointInText::AtEndOf(
            *atStartOfNextTextNode.ContainerAs<Text>());
  }
}

// static
template <typename EditorDOMPointType>
EditorDOMPointType
WSRunScanner::TextFragmentData::GetFirstASCIIWhiteSpacePointCollapsedTo(
    const EditorDOMPointInText& aPointAtASCIIWhiteSpace,
    nsIEditor::EDirection aDirectionToDelete,
    BlockInlineCheck aBlockInlineCheck,
    IgnoreNonEditableNodes aIgnoreNonEditableNodes,
    const nsIContent* aPrecedingLimiterContent) {
  MOZ_ASSERT(aDirectionToDelete == nsIEditor::eNone ||
             aDirectionToDelete == nsIEditor::eNext ||
             aDirectionToDelete == nsIEditor::ePrevious);
  MOZ_ASSERT(aPointAtASCIIWhiteSpace.IsSet());
  MOZ_ASSERT(!aPointAtASCIIWhiteSpace.IsEndOfContainer());
  MOZ_ASSERT_IF(!EditorUtils::IsNewLinePreformatted(
                    *aPointAtASCIIWhiteSpace.ContainerAs<Text>()),
                aPointAtASCIIWhiteSpace.IsCharCollapsibleASCIISpace());
  MOZ_ASSERT_IF(EditorUtils::IsNewLinePreformatted(
                    *aPointAtASCIIWhiteSpace.ContainerAs<Text>()),
                aPointAtASCIIWhiteSpace.IsCharASCIISpace());

  // If we're deleting text backward and the previous visible character is first
  // preformatted new line but white-spaces can be collapsed, we need to delete
  // its preceding collapsible white-spaces too.
  bool hasSeenPreformattedNewLine =
      aPointAtASCIIWhiteSpace.IsCharPreformattedNewLine();
  auto NeedToScanPrecedingWhiteSpaces =
      [&hasSeenPreformattedNewLine, &aDirectionToDelete](
          const EditorDOMPointInText& aAtPreviousVisibleCharacter) -> bool {
    MOZ_ASSERT(!aAtPreviousVisibleCharacter.IsEndOfContainer());
    return !hasSeenPreformattedNewLine &&
           aDirectionToDelete == nsIEditor::ePrevious &&
           aAtPreviousVisibleCharacter
               .IsCharPreformattedNewLineCollapsedWithWhiteSpaces();
  };
  auto ScanPreviousNonCollapsibleChar =
      [&hasSeenPreformattedNewLine, &NeedToScanPrecedingWhiteSpaces](
          const EditorDOMPointInText& aPoint) -> EditorDOMPointInText {
    Maybe<uint32_t> previousVisibleCharOffset =
        HTMLEditUtils::GetPreviousNonCollapsibleCharOffset(aPoint);
    if (previousVisibleCharOffset.isNothing()) {
      return EditorDOMPointInText();  // Keep scanning preceding text nodes
    }
    EditorDOMPointInText atPreviousVisibleCharacter(
        aPoint.ContainerAs<Text>(), previousVisibleCharOffset.value());
    if (!NeedToScanPrecedingWhiteSpaces(atPreviousVisibleCharacter)) {
      return atPreviousVisibleCharacter.NextPoint();
    }
    hasSeenPreformattedNewLine |=
        atPreviousVisibleCharacter.IsCharPreformattedNewLine();
    previousVisibleCharOffset =
        HTMLEditUtils::GetPreviousNonCollapsibleCharOffset(
            atPreviousVisibleCharacter);
    if (previousVisibleCharOffset.isSome()) {
      MOZ_ASSERT(aPoint.ContainerAs<Text>() ==
                 atPreviousVisibleCharacter.ContainerAs<Text>());
      return EditorDOMPointInText(
          atPreviousVisibleCharacter.ContainerAs<Text>(),
          previousVisibleCharOffset.value() + 1);
    }
    return EditorDOMPointInText();  // Keep scanning preceding text nodes
  };

  // If there is some characters before it, scan it in the text node first.
  if (!aPointAtASCIIWhiteSpace.IsStartOfContainer()) {
    EditorDOMPointInText atFirstASCIIWhiteSpace(
        ScanPreviousNonCollapsibleChar(aPointAtASCIIWhiteSpace));
    if (atFirstASCIIWhiteSpace.IsSet()) {
      return atFirstASCIIWhiteSpace.To<EditorDOMPointType>();
    }
  }

  // Otherwise, i.e., the text node starts with ASCII white-space, keep scanning
  // the preceding text nodes.
  // XXX Perhaps, we should stop scanning if there is non-editable and visible
  //     content.
  EditorDOMPointInText atLastWhiteSpace =
      EditorDOMPointInText(aPointAtASCIIWhiteSpace.ContainerAs<Text>(), 0u);
  for (EditorDOMPointInText atStartOfPreviousTextNode = atLastWhiteSpace;;) {
    const auto atLastCharOfPreviousTextNode =
        TextFragmentData::GetPreviousCharPoint<EditorDOMPointInText>(
            atStartOfPreviousTextNode, aBlockInlineCheck,
            aIgnoreNonEditableNodes, aPrecedingLimiterContent);
    if (!atLastCharOfPreviousTextNode.IsSet()) {
      // There is no more text nodes.  Return end of last text node.
      return atLastWhiteSpace.To<EditorDOMPointType>();
    }

    // We can ignore empty text nodes (even if it's preformatted).
    if (atLastCharOfPreviousTextNode.IsContainerEmpty()) {
      atStartOfPreviousTextNode = atLastCharOfPreviousTextNode;
      continue;
    }

    // If next node ends with non-white-space character or next node is
    // preformatted, return start of previous text node.
    if (!atLastCharOfPreviousTextNode.IsCharCollapsibleASCIISpace() &&
        !NeedToScanPrecedingWhiteSpaces(atLastCharOfPreviousTextNode)) {
      return atLastWhiteSpace.To<EditorDOMPointType>();
    }

    // Otherwise, scan the text node.
    const EditorDOMPointInText atFirstASCIIWhiteSpace(
        ScanPreviousNonCollapsibleChar(atLastCharOfPreviousTextNode));
    if (atFirstASCIIWhiteSpace.IsSet()) {
      return atFirstASCIIWhiteSpace.To<EditorDOMPointType>();
    }

    // The next text nodes starts with white-space too.  Try next one.
    atLastWhiteSpace = atStartOfPreviousTextNode = EditorDOMPointInText(
        atLastCharOfPreviousTextNode.ContainerAs<Text>(), 0u);
  }
}

EditorDOMPointInText WSRunScanner::TextFragmentData::
    GetPreviousNBSPPointIfNeedToReplaceWithASCIIWhiteSpace(
        const EditorDOMPoint& aPointToInsert) const {
  MOZ_ASSERT(aPointToInsert.IsSetAndValid());
  MOZ_ASSERT(VisibleWhiteSpacesDataRef().IsInitialized());
  NS_ASSERTION(VisibleWhiteSpacesDataRef().ComparePoint(aPointToInsert) ==
                       PointPosition::MiddleOfFragment ||
                   VisibleWhiteSpacesDataRef().ComparePoint(aPointToInsert) ==
                       PointPosition::EndOfFragment,
               "Previous char of aPoint should be in the visible white-spaces");

  // Try to change an NBSP to a space, if possible, just to prevent NBSP
  // proliferation.  This routine is called when we are about to make this
  // point in the ws abut an inserted break or text, so we don't have to worry
  // about what is after it.  What is after it now will end up after the
  // inserted object.
  const auto atPreviousChar = GetPreviousCharPoint<EditorDOMPointInText>(
      aPointToInsert, ShouldIgnoreNonEditableSiblingsOrDescendants(mScanMode));
  if (!atPreviousChar.IsSet() || atPreviousChar.IsEndOfContainer() ||
      !atPreviousChar.IsCharNBSP() ||
      EditorUtils::IsWhiteSpacePreformatted(
          *atPreviousChar.ContainerAs<Text>())) {
    return EditorDOMPointInText();
  }

  const auto atPreviousCharOfPreviousChar =
      GetPreviousCharPoint<EditorDOMPointInText>(
          atPreviousChar,
          ShouldIgnoreNonEditableSiblingsOrDescendants(mScanMode));
  if (atPreviousCharOfPreviousChar.IsSet()) {
    // If the previous char is in different text node and it's preformatted,
    // we shouldn't touch it.
    if (atPreviousChar.ContainerAs<Text>() !=
            atPreviousCharOfPreviousChar.ContainerAs<Text>() &&
        EditorUtils::IsWhiteSpacePreformatted(
            *atPreviousCharOfPreviousChar.ContainerAs<Text>())) {
      return EditorDOMPointInText();
    }
    // If the previous char of the NBSP at previous position of aPointToInsert
    // is an ASCII white-space, we don't need to replace it with same character.
    if (!atPreviousCharOfPreviousChar.IsEndOfContainer() &&
        atPreviousCharOfPreviousChar.IsCharASCIISpace()) {
      return EditorDOMPointInText();
    }
    return atPreviousChar;
  }

  // If previous content of the NBSP is block boundary, we cannot replace the
  // NBSP with an ASCII white-space to keep it rendered.
  const VisibleWhiteSpacesData& visibleWhiteSpaces =
      VisibleWhiteSpacesDataRef();
  if (!visibleWhiteSpaces.StartsFromNonCollapsibleCharacters() &&
      !visibleWhiteSpaces.StartsFromSpecialContent()) {
    return EditorDOMPointInText();
  }
  return atPreviousChar;
}

EditorDOMPointInText WSRunScanner::TextFragmentData::
    GetInclusiveNextNBSPPointIfNeedToReplaceWithASCIIWhiteSpace(
        const EditorDOMPoint& aPointToInsert) const {
  MOZ_ASSERT(aPointToInsert.IsSetAndValid());
  MOZ_ASSERT(VisibleWhiteSpacesDataRef().IsInitialized());
  NS_ASSERTION(VisibleWhiteSpacesDataRef().ComparePoint(aPointToInsert) ==
                       PointPosition::StartOfFragment ||
                   VisibleWhiteSpacesDataRef().ComparePoint(aPointToInsert) ==
                       PointPosition::MiddleOfFragment,
               "Inclusive next char of aPointToInsert should be in the visible "
               "white-spaces");

  // Try to change an nbsp to a space, if possible, just to prevent nbsp
  // proliferation This routine is called when we are about to make this point
  // in the ws abut an inserted text, so we don't have to worry about what is
  // before it.  What is before it now will end up before the inserted text.
  const auto atNextChar = GetInclusiveNextCharPoint<EditorDOMPointInText>(
      aPointToInsert, ShouldIgnoreNonEditableSiblingsOrDescendants(mScanMode));
  if (!atNextChar.IsSet() || NS_WARN_IF(atNextChar.IsEndOfContainer()) ||
      !atNextChar.IsCharNBSP() ||
      EditorUtils::IsWhiteSpacePreformatted(*atNextChar.ContainerAs<Text>())) {
    return EditorDOMPointInText();
  }

  const auto atNextCharOfNextCharOfNBSP =
      GetInclusiveNextCharPoint<EditorDOMPointInText>(
          atNextChar.NextPoint<EditorRawDOMPointInText>(),
          ShouldIgnoreNonEditableSiblingsOrDescendants(mScanMode));
  if (atNextCharOfNextCharOfNBSP.IsSet()) {
    // If the next char is in different text node and it's preformatted,
    // we shouldn't touch it.
    if (atNextChar.ContainerAs<Text>() !=
            atNextCharOfNextCharOfNBSP.ContainerAs<Text>() &&
        EditorUtils::IsWhiteSpacePreformatted(
            *atNextCharOfNextCharOfNBSP.ContainerAs<Text>())) {
      return EditorDOMPointInText();
    }
    // If following character of an NBSP is an ASCII white-space, we don't
    // need to replace it with same character.
    if (!atNextCharOfNextCharOfNBSP.IsEndOfContainer() &&
        atNextCharOfNextCharOfNBSP.IsCharASCIISpace()) {
      return EditorDOMPointInText();
    }
    return atNextChar;
  }

  // If the NBSP is last character in the hard line, we don't need to
  // replace it because it's required to render multiple white-spaces.
  const VisibleWhiteSpacesData& visibleWhiteSpaces =
      VisibleWhiteSpacesDataRef();
  if (!visibleWhiteSpaces.EndsByNonCollapsibleCharacters() &&
      !visibleWhiteSpaces.EndsBySpecialContent() &&
      !visibleWhiteSpaces.EndsByBRElement()) {
    return EditorDOMPointInText();
  }

  return atNextChar;
}

}  // namespace mozilla
