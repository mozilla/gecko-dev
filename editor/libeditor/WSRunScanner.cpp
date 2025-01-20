/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WSRunScanner.h"

#include "EditorDOMPoint.h"
#include "EditorUtils.h"
#include "ErrorList.h"
#include "HTMLEditor.h"
#include "HTMLEditUtils.h"

#include "mozilla/Assertions.h"
#include "mozilla/Casting.h"  // for AssertedCast

#include "nsCRT.h"
#include "nsDebug.h"
#include "nsError.h"
#include "nsIContent.h"
#include "nsIContentInlines.h"
#include "nsRange.h"
#include "nsTextFragment.h"

namespace mozilla {

using namespace dom;

using LeafNodeType = HTMLEditUtils::LeafNodeType;

template WSScanResult WSRunScanner::ScanPreviousVisibleNodeOrBlockBoundaryFrom(
    const EditorDOMPoint& aPoint) const;
template WSScanResult WSRunScanner::ScanPreviousVisibleNodeOrBlockBoundaryFrom(
    const EditorRawDOMPoint& aPoint) const;
template WSScanResult WSRunScanner::ScanPreviousVisibleNodeOrBlockBoundaryFrom(
    const EditorDOMPointInText& aPoint) const;
template WSScanResult WSRunScanner::ScanPreviousVisibleNodeOrBlockBoundaryFrom(
    const EditorRawDOMPointInText& aPoint) const;
template WSScanResult
WSRunScanner::ScanInclusiveNextVisibleNodeOrBlockBoundaryFrom(
    const EditorDOMPoint& aPoint) const;
template WSScanResult
WSRunScanner::ScanInclusiveNextVisibleNodeOrBlockBoundaryFrom(
    const EditorRawDOMPoint& aPoint) const;
template WSScanResult
WSRunScanner::ScanInclusiveNextVisibleNodeOrBlockBoundaryFrom(
    const EditorDOMPointInText& aPoint) const;
template WSScanResult
WSRunScanner::ScanInclusiveNextVisibleNodeOrBlockBoundaryFrom(
    const EditorRawDOMPointInText& aPoint) const;
template EditorDOMPoint WSRunScanner::GetAfterLastVisiblePoint(
    Text& aTextNode, const Element* aAncestorLimiter);
template EditorRawDOMPoint WSRunScanner::GetAfterLastVisiblePoint(
    Text& aTextNode, const Element* aAncestorLimiter);
template EditorDOMPoint WSRunScanner::GetFirstVisiblePoint(
    Text& aTextNode, const Element* aAncestorLimiter);
template EditorRawDOMPoint WSRunScanner::GetFirstVisiblePoint(
    Text& aTextNode, const Element* aAncestorLimiter);

template WSRunScanner::TextFragmentData::TextFragmentData(
    const EditorDOMPoint& aPoint, const Element* aEditingHost,
    BlockInlineCheck aBlockInlineCheck);
template WSRunScanner::TextFragmentData::TextFragmentData(
    const EditorRawDOMPoint& aPoint, const Element* aEditingHost,
    BlockInlineCheck aBlockInlineCheck);
template WSRunScanner::TextFragmentData::TextFragmentData(
    const EditorDOMPointInText& aPoint, const Element* aEditingHost,
    BlockInlineCheck aBlockInlineCheck);

NS_INSTANTIATE_CONST_METHOD_RETURNING_ANY_EDITOR_DOM_POINT(
    WSRunScanner::TextFragmentData::GetInclusiveNextEditableCharPoint,
    const EditorDOMPoint& aPoint);
NS_INSTANTIATE_CONST_METHOD_RETURNING_ANY_EDITOR_DOM_POINT(
    WSRunScanner::TextFragmentData::GetInclusiveNextEditableCharPoint,
    const EditorRawDOMPoint& aPoint);
NS_INSTANTIATE_CONST_METHOD_RETURNING_ANY_EDITOR_DOM_POINT(
    WSRunScanner::TextFragmentData::GetInclusiveNextEditableCharPoint,
    const EditorDOMPointInText& aPoint);
NS_INSTANTIATE_CONST_METHOD_RETURNING_ANY_EDITOR_DOM_POINT(
    WSRunScanner::TextFragmentData::GetInclusiveNextEditableCharPoint,
    const EditorRawDOMPointInText& aPoint);

NS_INSTANTIATE_CONST_METHOD_RETURNING_ANY_EDITOR_DOM_POINT(
    WSRunScanner::TextFragmentData::GetPreviousEditableCharPoint,
    const EditorDOMPoint& aPoint);
NS_INSTANTIATE_CONST_METHOD_RETURNING_ANY_EDITOR_DOM_POINT(
    WSRunScanner::TextFragmentData::GetPreviousEditableCharPoint,
    const EditorRawDOMPoint& aPoint);
NS_INSTANTIATE_CONST_METHOD_RETURNING_ANY_EDITOR_DOM_POINT(
    WSRunScanner::TextFragmentData::GetPreviousEditableCharPoint,
    const EditorDOMPointInText& aPoint);
NS_INSTANTIATE_CONST_METHOD_RETURNING_ANY_EDITOR_DOM_POINT(
    WSRunScanner::TextFragmentData::GetPreviousEditableCharPoint,
    const EditorRawDOMPointInText& aPoint);

template <typename PT, typename CT>
WSScanResult WSRunScanner::ScanPreviousVisibleNodeOrBlockBoundaryFrom(
    const EditorDOMPointBase<PT, CT>& aPoint) const {
  MOZ_ASSERT(aPoint.IsSet());
  MOZ_ASSERT(aPoint.IsInComposedDoc());

  if (MOZ_UNLIKELY(!aPoint.IsSet())) {
    return WSScanResult::Error();
  }

  // We may not be able to check editable state in uncomposed tree as expected.
  // For example, only some descendants in an editing host is temporarily
  // removed from the tree, they are not editable unless nested contenteditable
  // attribute is set to "true".
  if (MOZ_UNLIKELY(!aPoint.IsInComposedDoc())) {
    return WSScanResult(WSScanResult::ScanDirection::Backward,
                        *aPoint.template ContainerAs<nsIContent>(),
                        WSType::InUncomposedDoc, mBlockInlineCheck);
  }

  if (!TextFragmentDataAtStartRef().IsInitialized()) {
    return WSScanResult::Error();
  }

  // If the range has visible text and start of the visible text is before
  // aPoint, return previous character in the text.
  const VisibleWhiteSpacesData& visibleWhiteSpaces =
      TextFragmentDataAtStartRef().VisibleWhiteSpacesDataRef();
  if (visibleWhiteSpaces.IsInitialized() &&
      visibleWhiteSpaces.StartRef().IsBefore(aPoint)) {
    // If the visible things are not editable, we shouldn't scan "editable"
    // things now.  Whether keep scanning editable things or not should be
    // considered by the caller.
    if (aPoint.GetChild() && !aPoint.GetChild()->IsEditable()) {
      return WSScanResult(WSScanResult::ScanDirection::Backward,
                          *aPoint.GetChild(), WSType::SpecialContent,
                          mBlockInlineCheck);
    }
    const auto atPreviousChar =
        GetPreviousEditableCharPoint<EditorRawDOMPointInText>(aPoint);
    // When it's a non-empty text node, return it.
    if (atPreviousChar.IsSet() && !atPreviousChar.IsContainerEmpty()) {
      MOZ_ASSERT(!atPreviousChar.IsEndOfContainer());
      return WSScanResult(WSScanResult::ScanDirection::Backward,
                          atPreviousChar.template NextPoint<EditorDOMPoint>(),
                          atPreviousChar.IsCharCollapsibleASCIISpaceOrNBSP()
                              ? WSType::CollapsibleWhiteSpaces
                          : atPreviousChar.IsCharPreformattedNewLine()
                              ? WSType::PreformattedLineBreak
                              : WSType::NonCollapsibleCharacters,
                          mBlockInlineCheck);
    }
  }

  if (NS_WARN_IF(TextFragmentDataAtStartRef().StartRawReason() ==
                 WSType::UnexpectedError)) {
    return WSScanResult::Error();
  }

  switch (TextFragmentDataAtStartRef().StartRawReason()) {
    case WSType::CollapsibleWhiteSpaces:
    case WSType::NonCollapsibleCharacters:
    case WSType::PreformattedLineBreak:
      MOZ_ASSERT(TextFragmentDataAtStartRef().StartRef().IsSet());
      // XXX: If we find the character at last of a text node and we started
      // scanning from following text node of it, some callers may work with the
      // point in the following text node instead of end of the found text node.
      return WSScanResult(WSScanResult::ScanDirection::Backward,
                          TextFragmentDataAtStartRef().StartRef(),
                          TextFragmentDataAtStartRef().StartRawReason(),
                          mBlockInlineCheck);
    default:
      break;
  }

  // Otherwise, return the start of the range.
  if (TextFragmentDataAtStartRef().GetStartReasonContent() !=
      TextFragmentDataAtStartRef().StartRef().GetContainer()) {
    if (NS_WARN_IF(!TextFragmentDataAtStartRef().GetStartReasonContent())) {
      return WSScanResult::Error();
    }
    // In this case, TextFragmentDataAtStartRef().StartRef().Offset() is not
    // meaningful.
    return WSScanResult(WSScanResult::ScanDirection::Backward,
                        *TextFragmentDataAtStartRef().GetStartReasonContent(),
                        TextFragmentDataAtStartRef().StartRawReason(),
                        mBlockInlineCheck);
  }
  if (NS_WARN_IF(!TextFragmentDataAtStartRef().StartRef().IsSet())) {
    return WSScanResult::Error();
  }
  return WSScanResult(WSScanResult::ScanDirection::Backward,
                      TextFragmentDataAtStartRef().StartRef(),
                      TextFragmentDataAtStartRef().StartRawReason(),
                      mBlockInlineCheck);
}

template <typename PT, typename CT>
WSScanResult WSRunScanner::ScanInclusiveNextVisibleNodeOrBlockBoundaryFrom(
    const EditorDOMPointBase<PT, CT>& aPoint) const {
  MOZ_ASSERT(aPoint.IsSet());
  MOZ_ASSERT(aPoint.IsInComposedDoc());

  if (MOZ_UNLIKELY(!aPoint.IsSet())) {
    return WSScanResult::Error();
  }

  // We may not be able to check editable state in uncomposed tree as expected.
  // For example, only some descendants in an editing host is temporarily
  // removed from the tree, they are not editable unless nested contenteditable
  // attribute is set to "true".
  if (MOZ_UNLIKELY(!aPoint.IsInComposedDoc())) {
    return WSScanResult(WSScanResult::ScanDirection::Forward,
                        *aPoint.template ContainerAs<nsIContent>(),
                        WSType::InUncomposedDoc, mBlockInlineCheck);
  }

  if (!TextFragmentDataAtStartRef().IsInitialized()) {
    return WSScanResult::Error();
  }

  // If the range has visible text and aPoint equals or is before the end of the
  // visible text, return inclusive next character in the text.
  const VisibleWhiteSpacesData& visibleWhiteSpaces =
      TextFragmentDataAtStartRef().VisibleWhiteSpacesDataRef();
  if (visibleWhiteSpaces.IsInitialized() &&
      aPoint.EqualsOrIsBefore(visibleWhiteSpaces.EndRef())) {
    // If the visible things are not editable, we shouldn't scan "editable"
    // things now.  Whether keep scanning editable things or not should be
    // considered by the caller.
    if (aPoint.GetChild() && !aPoint.GetChild()->IsEditable()) {
      return WSScanResult(WSScanResult::ScanDirection::Forward,
                          *aPoint.GetChild(), WSType::SpecialContent,
                          mBlockInlineCheck);
    }
    const auto atNextChar =
        GetInclusiveNextEditableCharPoint<EditorDOMPoint>(aPoint);
    // When it's a non-empty text node, return it.
    if (atNextChar.IsSet() && !atNextChar.IsContainerEmpty()) {
      return WSScanResult(WSScanResult::ScanDirection::Forward, atNextChar,
                          !atNextChar.IsEndOfContainer() &&
                                  atNextChar.IsCharCollapsibleASCIISpaceOrNBSP()
                              ? WSType::CollapsibleWhiteSpaces
                          : !atNextChar.IsEndOfContainer() &&
                                  atNextChar.IsCharPreformattedNewLine()
                              ? WSType::PreformattedLineBreak
                              : WSType::NonCollapsibleCharacters,
                          mBlockInlineCheck);
    }
  }

  if (NS_WARN_IF(TextFragmentDataAtStartRef().EndRawReason() ==
                 WSType::UnexpectedError)) {
    return WSScanResult::Error();
  }

  switch (TextFragmentDataAtStartRef().EndRawReason()) {
    case WSType::CollapsibleWhiteSpaces:
    case WSType::NonCollapsibleCharacters:
    case WSType::PreformattedLineBreak:
      MOZ_ASSERT(TextFragmentDataAtStartRef().StartRef().IsSet());
      // XXX: If we find the character at start of a text node and we
      // started scanning from preceding text node of it, some callers may want
      // to work with the point at end of the preceding text node instead of
      // start of the found text node.
      return WSScanResult(WSScanResult::ScanDirection::Forward,
                          TextFragmentDataAtStartRef().EndRef(),
                          TextFragmentDataAtStartRef().EndRawReason(),
                          mBlockInlineCheck);
    default:
      break;
  }

  // Otherwise, return the end of the range.
  if (TextFragmentDataAtStartRef().GetEndReasonContent() !=
      TextFragmentDataAtStartRef().EndRef().GetContainer()) {
    if (NS_WARN_IF(!TextFragmentDataAtStartRef().GetEndReasonContent())) {
      return WSScanResult::Error();
    }
    // In this case, TextFragmentDataAtStartRef().EndRef().Offset() is not
    // meaningful.
    return WSScanResult(WSScanResult::ScanDirection::Forward,
                        *TextFragmentDataAtStartRef().GetEndReasonContent(),
                        TextFragmentDataAtStartRef().EndRawReason(),
                        mBlockInlineCheck);
  }
  if (NS_WARN_IF(!TextFragmentDataAtStartRef().EndRef().IsSet())) {
    return WSScanResult::Error();
  }
  return WSScanResult(WSScanResult::ScanDirection::Forward,
                      TextFragmentDataAtStartRef().EndRef(),
                      TextFragmentDataAtStartRef().EndRawReason(),
                      mBlockInlineCheck);
}

template <typename EditorDOMPointType>
WSRunScanner::TextFragmentData::TextFragmentData(
    const EditorDOMPointType& aPoint, const Element* aEditingHost,
    BlockInlineCheck aBlockInlineCheck)
    : mEditingHost(aEditingHost), mBlockInlineCheck(aBlockInlineCheck) {
  if (!aPoint.IsSetAndValid()) {
    NS_WARNING("aPoint was invalid");
    return;
  }
  if (!aPoint.IsInContentNode()) {
    NS_WARNING("aPoint was in Document or DocumentFragment");
    // I.e., we're try to modify outside of root element.  We don't need to
    // support such odd case because web apps cannot append text nodes as
    // direct child of Document node.
    return;
  }

  mScanStartPoint = aPoint.template To<EditorDOMPoint>();
  NS_ASSERTION(
      EditorUtils::IsEditableContent(*mScanStartPoint.ContainerAs<nsIContent>(),
                                     EditorType::HTML),
      "Given content is not editable");
  NS_ASSERTION(
      mScanStartPoint.ContainerAs<nsIContent>()->GetAsElementOrParentElement(),
      "Given content is not an element and an orphan node");
  if (NS_WARN_IF(!EditorUtils::IsEditableContent(
          *mScanStartPoint.ContainerAs<nsIContent>(), EditorType::HTML))) {
    return;
  }
  const Element* editableBlockElementOrInlineEditingHost =
      HTMLEditUtils::GetInclusiveAncestorElement(
          *mScanStartPoint.ContainerAs<nsIContent>(),
          HTMLEditUtils::ClosestEditableBlockElementOrInlineEditingHost,
          aBlockInlineCheck);
  if (!editableBlockElementOrInlineEditingHost) {
    NS_WARNING(
        "HTMLEditUtils::GetInclusiveAncestorElement(HTMLEditUtils::"
        "ClosestEditableBlockElementOrInlineEditingHost) couldn't find "
        "editing host");
    return;
  }

  mStart = BoundaryData::ScanCollapsibleWhiteSpaceStartFrom(
      mScanStartPoint, *editableBlockElementOrInlineEditingHost, mEditingHost,
      &mNBSPData, aBlockInlineCheck);
  MOZ_ASSERT_IF(mStart.IsNonCollapsibleCharacters(),
                !mStart.PointRef().IsPreviousCharPreformattedNewLine());
  MOZ_ASSERT_IF(mStart.IsPreformattedLineBreak(),
                mStart.PointRef().IsPreviousCharPreformattedNewLine());
  mEnd = BoundaryData::ScanCollapsibleWhiteSpaceEndFrom(
      mScanStartPoint, *editableBlockElementOrInlineEditingHost, mEditingHost,
      &mNBSPData, aBlockInlineCheck);
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
        const EditorDOMPointType& aPoint,
        const Element& aEditableBlockParentOrTopmostEditableInlineElement,
        const Element* aEditingHost, NoBreakingSpaceData* aNBSPData,
        BlockInlineCheck aBlockInlineCheck) {
  MOZ_ASSERT(aPoint.IsSetAndValid());
  MOZ_ASSERT(aEditableBlockParentOrTopmostEditableInlineElement.IsEditable());

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
        EditorDOMPoint(aPoint.template ContainerAs<Text>(), 0),
        aEditableBlockParentOrTopmostEditableInlineElement, aEditingHost,
        aNBSPData, aBlockInlineCheck);
  }

  // Then, we need to check previous leaf node.
  nsIContent* previousLeafContentOrBlock =
      HTMLEditUtils::GetPreviousLeafContentOrPreviousBlockElement(
          aPoint, aEditableBlockParentOrTopmostEditableInlineElement,
          {LeafNodeType::LeafNodeOrNonEditableNode}, aBlockInlineCheck,
          aEditingHost);
  if (!previousLeafContentOrBlock) {
    // No previous content means that we reached
    // aEditableBlockParentOrTopmostEditableInlineElement boundary.
    return BoundaryData(aPoint,
                        const_cast<Element&>(
                            aEditableBlockParentOrTopmostEditableInlineElement),
                        HTMLEditUtils::IsBlockElement(
                            aEditableBlockParentOrTopmostEditableInlineElement,
                            aBlockInlineCheck)
                            ? WSType::CurrentBlockBoundary
                            : WSType::InlineEditingHostBoundary);
  }

  if (HTMLEditUtils::IsBlockElement(*previousLeafContentOrBlock,
                                    aBlockInlineCheck)) {
    return BoundaryData(aPoint, *previousLeafContentOrBlock,
                        WSType::OtherBlockBoundary);
  }

  if (!previousLeafContentOrBlock->IsText() ||
      !previousLeafContentOrBlock->IsEditable()) {
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
        EditorDOMPointInText(previousLeafContentOrBlock->AsText(), 0),
        aEditableBlockParentOrTopmostEditableInlineElement, aEditingHost,
        aNBSPData, aBlockInlineCheck);
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
      EditorDOMPointInText(previousLeafContentOrBlock->AsText(), 0),
      aEditableBlockParentOrTopmostEditableInlineElement, aEditingHost,
      aNBSPData, aBlockInlineCheck);
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
    const EditorDOMPointType& aPoint,
    const Element& aEditableBlockParentOrTopmostEditableInlineElement,
    const Element* aEditingHost, NoBreakingSpaceData* aNBSPData,
    BlockInlineCheck aBlockInlineCheck) {
  MOZ_ASSERT(aPoint.IsSetAndValid());
  MOZ_ASSERT(aEditableBlockParentOrTopmostEditableInlineElement.IsEditable());

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
        EditorDOMPointInText::AtEndOf(*aPoint.template ContainerAs<Text>()),
        aEditableBlockParentOrTopmostEditableInlineElement, aEditingHost,
        aNBSPData, aBlockInlineCheck);
  }

  // Then, we need to check next leaf node.
  nsIContent* nextLeafContentOrBlock =
      HTMLEditUtils::GetNextLeafContentOrNextBlockElement(
          aPoint, aEditableBlockParentOrTopmostEditableInlineElement,
          {LeafNodeType::LeafNodeOrNonEditableNode}, aBlockInlineCheck,
          aEditingHost);
  if (!nextLeafContentOrBlock) {
    // No next content means that we reached
    // aEditableBlockParentOrTopmostEditableInlineElement boundary.
    return BoundaryData(aPoint.template To<EditorDOMPoint>(),
                        const_cast<Element&>(
                            aEditableBlockParentOrTopmostEditableInlineElement),
                        HTMLEditUtils::IsBlockElement(
                            aEditableBlockParentOrTopmostEditableInlineElement,
                            aBlockInlineCheck)
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
      !nextLeafContentOrBlock->IsEditable()) {
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
        EditorDOMPointInText(nextLeafContentOrBlock->AsText(), 0),
        aEditableBlockParentOrTopmostEditableInlineElement, aEditingHost,
        aNBSPData, aBlockInlineCheck);
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
      EditorDOMPointInText::AtEndOf(*nextLeafContentOrBlock->AsText()),
      aEditableBlockParentOrTopmostEditableInlineElement, aEditingHost,
      aNBSPData, aBlockInlineCheck);
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

  // If it's not immediately before a block boundary nor an invisible
  // preformatted linefeed, there is no invisible trailing white-spaces.  Note
  // that collapsible white-spaces before a `<br>` element is visible.
  if (!EndsByBlockBoundary() && !EndsByInlineEditingHostBoundary() &&
      !EndsByInvisiblePreformattedLineBreak()) {
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
          : GetInclusiveNextEditableCharPoint<EditorDOMPointInText>(
                aRange.StartRef());
  if (!firstPoint.IsSet()) {
    return EditorDOMRangeInTexts();
  }
  EditorDOMPointInText endPoint;
  if (aRange.EndRef().IsInTextNode()) {
    endPoint = aRange.EndRef().AsInText();
  } else {
    // FYI: GetPreviousEditableCharPoint() returns last character's point
    //      of preceding text node if it's not empty, but we need end of
    //      the text node here.
    endPoint = GetPreviousEditableCharPoint(aRange.EndRef());
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
  auto nextCharOfStartOfEnd =
      GetInclusiveNextEditableCharPoint<EditorDOMPointInText>(endToDelete);
  if (!nextCharOfStartOfEnd.IsSet() ||
      nextCharOfStartOfEnd.IsEndOfContainer() ||
      !nextCharOfStartOfEnd.IsCharCollapsibleASCIISpace()) {
    return ReplaceRangeData();
  }
  if (nextCharOfStartOfEnd.IsStartOfContainer() ||
      nextCharOfStartOfEnd.IsPreviousCharCollapsibleASCIISpace()) {
    nextCharOfStartOfEnd = aTextFragmentDataAtStartToDelete
                               .GetFirstASCIIWhiteSpacePointCollapsedTo(
                                   nextCharOfStartOfEnd, nsIEditor::eNone);
  }
  const EditorDOMPointInText endOfCollapsibleASCIIWhiteSpaces =
      aTextFragmentDataAtStartToDelete.GetEndOfCollapsibleASCIIWhiteSpaces(
          nextCharOfStartOfEnd, nsIEditor::eNone);
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
  EditorDOMPointInText atPreviousCharOfStart =
      GetPreviousEditableCharPoint(startToDelete);
  if (!atPreviousCharOfStart.IsSet() ||
      atPreviousCharOfStart.IsEndOfContainer() ||
      !atPreviousCharOfStart.IsCharCollapsibleASCIISpace()) {
    return ReplaceRangeData();
  }
  if (atPreviousCharOfStart.IsStartOfContainer() ||
      atPreviousCharOfStart.IsPreviousCharASCIISpace()) {
    atPreviousCharOfStart = GetFirstASCIIWhiteSpacePointCollapsedTo(
        atPreviousCharOfStart, nsIEditor::eNone);
  }
  const EditorDOMPointInText endOfCollapsibleASCIIWhiteSpaces =
      GetEndOfCollapsibleASCIIWhiteSpaces(atPreviousCharOfStart,
                                          nsIEditor::eNone);
  return ReplaceRangeData(atPreviousCharOfStart,
                          endOfCollapsibleASCIIWhiteSpaces,
                          nsDependentSubstring(&HTMLEditUtils::kNBSP, 1));
}

template <typename EditorDOMPointType, typename PT, typename CT>
EditorDOMPointType
WSRunScanner::TextFragmentData::GetInclusiveNextEditableCharPoint(
    const EditorDOMPointBase<PT, CT>& aPoint) const {
  MOZ_ASSERT(aPoint.IsSetAndValid());

  if (NS_WARN_IF(!aPoint.IsInContentNode()) ||
      NS_WARN_IF(!mScanStartPoint.IsInContentNode())) {
    return EditorDOMPointType();
  }

  EditorRawDOMPoint point;
  if (nsIContent* child =
          aPoint.CanContainerHaveChildren() ? aPoint.GetChild() : nullptr) {
    nsIContent* leafContent = child->HasChildren()
                                  ? HTMLEditUtils::GetFirstLeafContent(
                                        *child, {LeafNodeType::OnlyLeafNode})
                                  : child;
    if (NS_WARN_IF(!leafContent)) {
      return EditorDOMPointType();
    }
    point.Set(leafContent, 0);
  } else {
    point = aPoint.template To<EditorRawDOMPoint>();
  }

  // If it points a character in a text node, return it.
  // XXX For the performance, this does not check whether the container
  //     is outside of our range.
  if (point.IsInTextNode() && point.GetContainer()->IsEditable() &&
      !point.IsEndOfContainer()) {
    return EditorDOMPointType(point.ContainerAs<Text>(), point.Offset());
  }

  if (point.GetContainer() == GetEndReasonContent()) {
    return EditorDOMPointType();
  }

  NS_ASSERTION(
      EditorUtils::IsEditableContent(*mScanStartPoint.ContainerAs<nsIContent>(),
                                     EditorType::HTML),
      "Given content is not editable");
  NS_ASSERTION(
      mScanStartPoint.ContainerAs<nsIContent>()->GetAsElementOrParentElement(),
      "Given content is not an element and an orphan node");
  nsIContent* editableBlockElementOrInlineEditingHost =
      mScanStartPoint.ContainerAs<nsIContent>() &&
              EditorUtils::IsEditableContent(
                  *mScanStartPoint.ContainerAs<nsIContent>(), EditorType::HTML)
          ? HTMLEditUtils::GetInclusiveAncestorElement(
                *mScanStartPoint.ContainerAs<nsIContent>(),
                HTMLEditUtils::ClosestEditableBlockElementOrInlineEditingHost,
                mBlockInlineCheck)
          : nullptr;
  if (NS_WARN_IF(!editableBlockElementOrInlineEditingHost)) {
    // Meaning that the container of `mScanStartPoint` is not editable.
    editableBlockElementOrInlineEditingHost =
        mScanStartPoint.ContainerAs<nsIContent>();
  }

  for (nsIContent* nextContent =
           HTMLEditUtils::GetNextLeafContentOrNextBlockElement(
               *point.ContainerAs<nsIContent>(),
               *editableBlockElementOrInlineEditingHost,
               {LeafNodeType::LeafNodeOrNonEditableNode}, mBlockInlineCheck,
               mEditingHost);
       nextContent;
       nextContent = HTMLEditUtils::GetNextLeafContentOrNextBlockElement(
           *nextContent, *editableBlockElementOrInlineEditingHost,
           {LeafNodeType::LeafNodeOrNonEditableNode}, mBlockInlineCheck,
           mEditingHost)) {
    if (!nextContent->IsText() || !nextContent->IsEditable()) {
      if (nextContent == GetEndReasonContent()) {
        break;  // Reached end of current runs.
      }
      continue;
    }
    return EditorDOMPointType(nextContent->AsText(), 0);
  }
  return EditorDOMPointType();
}

template <typename EditorDOMPointType, typename PT, typename CT>
EditorDOMPointType WSRunScanner::TextFragmentData::GetPreviousEditableCharPoint(
    const EditorDOMPointBase<PT, CT>& aPoint) const {
  MOZ_ASSERT(aPoint.IsSetAndValid());

  if (NS_WARN_IF(!aPoint.IsInContentNode()) ||
      NS_WARN_IF(!mScanStartPoint.IsInContentNode())) {
    return EditorDOMPointType();
  }

  EditorRawDOMPoint point;
  if (nsIContent* previousChild = aPoint.CanContainerHaveChildren()
                                      ? aPoint.GetPreviousSiblingOfChild()
                                      : nullptr) {
    nsIContent* leafContent =
        previousChild->HasChildren()
            ? HTMLEditUtils::GetLastLeafContent(*previousChild,
                                                {LeafNodeType::OnlyLeafNode})
            : previousChild;
    if (NS_WARN_IF(!leafContent)) {
      return EditorDOMPointType();
    }
    point.SetToEndOf(leafContent);
  } else {
    point = aPoint.template To<EditorRawDOMPoint>();
  }

  // If it points a character in a text node and it's not first character
  // in it, return its previous point.
  // XXX For the performance, this does not check whether the container
  //     is outside of our range.
  if (point.IsInTextNode() && point.GetContainer()->IsEditable() &&
      !point.IsStartOfContainer()) {
    return EditorDOMPointType(point.ContainerAs<Text>(), point.Offset() - 1);
  }

  if (point.GetContainer() == GetStartReasonContent()) {
    return EditorDOMPointType();
  }

  NS_ASSERTION(
      EditorUtils::IsEditableContent(*mScanStartPoint.ContainerAs<nsIContent>(),
                                     EditorType::HTML),
      "Given content is not editable");
  NS_ASSERTION(
      mScanStartPoint.ContainerAs<nsIContent>()->GetAsElementOrParentElement(),
      "Given content is not an element and an orphan node");
  nsIContent* editableBlockElementOrInlineEditingHost =
      mScanStartPoint.ContainerAs<nsIContent>() &&
              EditorUtils::IsEditableContent(
                  *mScanStartPoint.ContainerAs<nsIContent>(), EditorType::HTML)
          ? HTMLEditUtils::GetInclusiveAncestorElement(
                *mScanStartPoint.ContainerAs<nsIContent>(),
                HTMLEditUtils::ClosestEditableBlockElementOrInlineEditingHost,
                mBlockInlineCheck)
          : nullptr;
  if (NS_WARN_IF(!editableBlockElementOrInlineEditingHost)) {
    // Meaning that the container of `mScanStartPoint` is not editable.
    editableBlockElementOrInlineEditingHost =
        mScanStartPoint.ContainerAs<nsIContent>();
  }

  for (nsIContent* previousContent =
           HTMLEditUtils::GetPreviousLeafContentOrPreviousBlockElement(
               *point.ContainerAs<nsIContent>(),
               *editableBlockElementOrInlineEditingHost,
               {LeafNodeType::LeafNodeOrNonEditableNode}, mBlockInlineCheck,
               mEditingHost);
       previousContent;
       previousContent =
           HTMLEditUtils::GetPreviousLeafContentOrPreviousBlockElement(
               *previousContent, *editableBlockElementOrInlineEditingHost,
               {LeafNodeType::LeafNodeOrNonEditableNode}, mBlockInlineCheck,
               mEditingHost)) {
    if (!previousContent->IsText() || !previousContent->IsEditable()) {
      if (previousContent == GetStartReasonContent()) {
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
EditorDOMPointType WSRunScanner::GetAfterLastVisiblePoint(
    Text& aTextNode, const Element* aAncestorLimiter) {
  EditorDOMPoint atLastCharOfTextNode(
      &aTextNode, AssertedCast<uint32_t>(std::max<int64_t>(
                      static_cast<int64_t>(aTextNode.Length()) - 1, 0)));
  if (!atLastCharOfTextNode.IsContainerEmpty() &&
      !atLastCharOfTextNode.IsCharCollapsibleASCIISpace()) {
    return EditorDOMPointType::AtEndOf(aTextNode);
  }
  TextFragmentData textFragmentData(atLastCharOfTextNode, aAncestorLimiter,
                                    BlockInlineCheck::UseComputedDisplayStyle);
  if (NS_WARN_IF(!textFragmentData.IsInitialized())) {
    return EditorDOMPointType();  // TODO: Make here return error with Err.
  }
  const EditorDOMRange& invisibleWhiteSpaceRange =
      textFragmentData.InvisibleTrailingWhiteSpaceRangeRef();
  if (!invisibleWhiteSpaceRange.IsPositioned() ||
      invisibleWhiteSpaceRange.Collapsed()) {
    return EditorDOMPointType::AtEndOf(aTextNode);
  }
  return invisibleWhiteSpaceRange.StartRef().To<EditorDOMPointType>();
}

// static
template <typename EditorDOMPointType>
EditorDOMPointType WSRunScanner::GetFirstVisiblePoint(
    Text& aTextNode, const Element* aAncestorLimiter) {
  EditorDOMPoint atStartOfTextNode(&aTextNode, 0);
  if (!atStartOfTextNode.IsContainerEmpty() &&
      atStartOfTextNode.IsCharCollapsibleASCIISpace()) {
    return atStartOfTextNode.To<EditorDOMPointType>();
  }
  TextFragmentData textFragmentData(atStartOfTextNode, aAncestorLimiter,
                                    BlockInlineCheck::UseComputedDisplayStyle);
  if (NS_WARN_IF(!textFragmentData.IsInitialized())) {
    return EditorDOMPointType();  // TODO: Make here return error with Err.
  }
  const EditorDOMRange& invisibleWhiteSpaceRange =
      textFragmentData.InvisibleLeadingWhiteSpaceRangeRef();
  if (!invisibleWhiteSpaceRange.IsPositioned() ||
      invisibleWhiteSpaceRange.Collapsed()) {
    return atStartOfTextNode.To<EditorDOMPointType>();
  }
  return invisibleWhiteSpaceRange.EndRef().To<EditorDOMPointType>();
}

template <typename EditorDOMPointType>
EditorDOMPointType
WSRunScanner::TextFragmentData::GetEndOfCollapsibleASCIIWhiteSpaces(
    const EditorDOMPointInText& aPointAtASCIIWhiteSpace,
    nsIEditor::EDirection aDirectionToDelete) const {
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
        GetInclusiveNextEditableCharPoint<EditorDOMPointInText>(
            atEndOfPreviousTextNode);
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

template <typename EditorDOMPointType>
EditorDOMPointType
WSRunScanner::TextFragmentData::GetFirstASCIIWhiteSpacePointCollapsedTo(
    const EditorDOMPointInText& aPointAtASCIIWhiteSpace,
    nsIEditor::EDirection aDirectionToDelete) const {
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
    const EditorDOMPointInText atLastCharOfPreviousTextNode =
        GetPreviousEditableCharPoint(atStartOfPreviousTextNode);
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

char16_t WSRunScanner::GetCharAt(Text* aTextNode, uint32_t aOffset) const {
  // return 0 if we can't get a char, for whatever reason
  if (NS_WARN_IF(!aTextNode) ||
      NS_WARN_IF(aOffset >= aTextNode->TextDataLength())) {
    return 0;
  }
  return aTextNode->TextFragment().CharAt(aOffset);
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
  const EditorDOMPointInText atPreviousChar =
      GetPreviousEditableCharPoint(aPointToInsert);
  if (!atPreviousChar.IsSet() || atPreviousChar.IsEndOfContainer() ||
      !atPreviousChar.IsCharNBSP() ||
      EditorUtils::IsWhiteSpacePreformatted(
          *atPreviousChar.ContainerAs<Text>())) {
    return EditorDOMPointInText();
  }

  const EditorDOMPointInText atPreviousCharOfPreviousChar =
      GetPreviousEditableCharPoint(atPreviousChar);
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
  auto atNextChar =
      GetInclusiveNextEditableCharPoint<EditorDOMPointInText>(aPointToInsert);
  if (!atNextChar.IsSet() || NS_WARN_IF(atNextChar.IsEndOfContainer()) ||
      !atNextChar.IsCharNBSP() ||
      EditorUtils::IsWhiteSpacePreformatted(*atNextChar.ContainerAs<Text>())) {
    return EditorDOMPointInText();
  }

  const auto atNextCharOfNextCharOfNBSP =
      GetInclusiveNextEditableCharPoint<EditorDOMPointInText>(
          atNextChar.NextPoint<EditorRawDOMPointInText>());
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

/*****************************************************************************
 * Implementation for new white-space normalizer
 *****************************************************************************/

// static
EditorDOMRangeInTexts
WSRunScanner::ComputeRangeInTextNodesContainingInvisibleWhiteSpaces(
    const TextFragmentData& aStart, const TextFragmentData& aEnd) {
  // Corresponding to handling invisible white-spaces part of
  // `TextFragmentData::GetReplaceRangeDataAtEndOfDeletionRange()` and
  // `TextFragmentData::GetReplaceRangeDataAtStartOfDeletionRange()`

  MOZ_ASSERT(aStart.ScanStartRef().IsSetAndValid());
  MOZ_ASSERT(aEnd.ScanStartRef().IsSetAndValid());
  MOZ_ASSERT(aStart.ScanStartRef().EqualsOrIsBefore(aEnd.ScanStartRef()));
  MOZ_ASSERT(aStart.ScanStartRef().IsInTextNode());
  MOZ_ASSERT(aEnd.ScanStartRef().IsInTextNode());

  // XXX `GetReplaceRangeDataAtEndOfDeletionRange()` and
  //     `GetReplaceRangeDataAtStartOfDeletionRange()` use
  //     `GetNewInvisibleLeadingWhiteSpaceRangeIfSplittingAt()` and
  //     `GetNewInvisibleTrailingWhiteSpaceRangeIfSplittingAt()`.
  //     However, they are really odd as mentioned with "XXX" comments
  //     in them.  For the new white-space normalizer, we need to treat
  //     invisible white-spaces stricter because the legacy path handles
  //     white-spaces multiple times (e.g., calling `HTMLEditor::
  //     DeleteNodeIfInvisibleAndEditableTextNode()` later) and that hides
  //     the bug, but in the new path, we should stop doing same things
  //     multiple times for both performance and footprint.  Therefore,
  //     even though the result might be different in some edge cases,
  //     we should use clean path for now.  Perhaps, we should fix the odd
  //     cases before shipping `beforeinput` event in release channel.

  const EditorDOMRange& invisibleLeadingWhiteSpaceRange =
      aStart.InvisibleLeadingWhiteSpaceRangeRef();
  const EditorDOMRange& invisibleTrailingWhiteSpaceRange =
      aEnd.InvisibleTrailingWhiteSpaceRangeRef();
  const bool hasInvisibleLeadingWhiteSpaces =
      invisibleLeadingWhiteSpaceRange.IsPositioned() &&
      !invisibleLeadingWhiteSpaceRange.Collapsed();
  const bool hasInvisibleTrailingWhiteSpaces =
      invisibleLeadingWhiteSpaceRange != invisibleTrailingWhiteSpaceRange &&
      invisibleTrailingWhiteSpaceRange.IsPositioned() &&
      !invisibleTrailingWhiteSpaceRange.Collapsed();

  EditorDOMRangeInTexts result(aStart.ScanStartRef().AsInText(),
                               aEnd.ScanStartRef().AsInText());
  MOZ_ASSERT(result.IsPositionedAndValid());
  if (!hasInvisibleLeadingWhiteSpaces && !hasInvisibleTrailingWhiteSpaces) {
    return result;
  }

  MOZ_ASSERT_IF(
      hasInvisibleLeadingWhiteSpaces && hasInvisibleTrailingWhiteSpaces,
      invisibleLeadingWhiteSpaceRange.StartRef().IsBefore(
          invisibleTrailingWhiteSpaceRange.StartRef()));
  const EditorDOMPoint& aroundFirstInvisibleWhiteSpace =
      hasInvisibleLeadingWhiteSpaces
          ? invisibleLeadingWhiteSpaceRange.StartRef()
          : invisibleTrailingWhiteSpaceRange.StartRef();
  if (aroundFirstInvisibleWhiteSpace.IsBefore(result.StartRef())) {
    if (aroundFirstInvisibleWhiteSpace.IsInTextNode()) {
      result.SetStart(aroundFirstInvisibleWhiteSpace.AsInText());
      MOZ_ASSERT(result.IsPositionedAndValid());
    } else {
      const auto atFirstInvisibleWhiteSpace =
          hasInvisibleLeadingWhiteSpaces
              ? aStart.GetInclusiveNextEditableCharPoint<EditorDOMPointInText>(
                    aroundFirstInvisibleWhiteSpace)
              : aEnd.GetInclusiveNextEditableCharPoint<EditorDOMPointInText>(
                    aroundFirstInvisibleWhiteSpace);
      MOZ_ASSERT(atFirstInvisibleWhiteSpace.IsSet());
      MOZ_ASSERT(
          atFirstInvisibleWhiteSpace.EqualsOrIsBefore(result.StartRef()));
      result.SetStart(atFirstInvisibleWhiteSpace);
      MOZ_ASSERT(result.IsPositionedAndValid());
    }
  }
  MOZ_ASSERT_IF(
      hasInvisibleLeadingWhiteSpaces && hasInvisibleTrailingWhiteSpaces,
      invisibleLeadingWhiteSpaceRange.EndRef().IsBefore(
          invisibleTrailingWhiteSpaceRange.EndRef()));
  const EditorDOMPoint& afterLastInvisibleWhiteSpace =
      hasInvisibleTrailingWhiteSpaces
          ? invisibleTrailingWhiteSpaceRange.EndRef()
          : invisibleLeadingWhiteSpaceRange.EndRef();
  if (afterLastInvisibleWhiteSpace.EqualsOrIsBefore(result.EndRef())) {
    MOZ_ASSERT(result.IsPositionedAndValid());
    return result;
  }
  if (afterLastInvisibleWhiteSpace.IsInTextNode()) {
    result.SetEnd(afterLastInvisibleWhiteSpace.AsInText());
    MOZ_ASSERT(result.IsPositionedAndValid());
    return result;
  }
  const auto atLastInvisibleWhiteSpace =
      hasInvisibleTrailingWhiteSpaces
          ? aEnd.GetPreviousEditableCharPoint(afterLastInvisibleWhiteSpace)
          : aStart.GetPreviousEditableCharPoint(afterLastInvisibleWhiteSpace);
  MOZ_ASSERT(atLastInvisibleWhiteSpace.IsSet());
  MOZ_ASSERT(atLastInvisibleWhiteSpace.IsContainerEmpty() ||
             atLastInvisibleWhiteSpace.IsAtLastContent());
  MOZ_ASSERT(result.EndRef().EqualsOrIsBefore(atLastInvisibleWhiteSpace));
  result.SetEnd(atLastInvisibleWhiteSpace.IsEndOfContainer()
                    ? atLastInvisibleWhiteSpace
                    : atLastInvisibleWhiteSpace.NextPoint());
  MOZ_ASSERT(result.IsPositionedAndValid());
  return result;
}

// static
Result<EditorDOMRangeInTexts, nsresult>
WSRunScanner::GetRangeInTextNodesToBackspaceFrom(const EditorDOMPoint& aPoint,
                                                 const Element& aEditingHost) {
  // Corresponding to computing delete range part of
  // `WhiteSpaceVisibilityKeeper::DeletePreviousWhiteSpace()`
  MOZ_ASSERT(aPoint.IsSetAndValid());

  TextFragmentData textFragmentDataAtCaret(
      aPoint, &aEditingHost, BlockInlineCheck::UseComputedDisplayStyle);
  if (NS_WARN_IF(!textFragmentDataAtCaret.IsInitialized())) {
    return Err(NS_ERROR_FAILURE);
  }
  EditorDOMPointInText atPreviousChar =
      textFragmentDataAtCaret.GetPreviousEditableCharPoint(aPoint);
  if (!atPreviousChar.IsSet()) {
    return EditorDOMRangeInTexts();  // There is no content in the block.
  }

  // XXX When previous char point is in an empty text node, we do nothing,
  //     but this must look odd from point of user view.  We should delete
  //     something before aPoint.
  if (atPreviousChar.IsEndOfContainer()) {
    return EditorDOMRangeInTexts();
  }

  // Extend delete range if previous char is a low surrogate following
  // a high surrogate.
  EditorDOMPointInText atNextChar = atPreviousChar.NextPoint();
  if (!atPreviousChar.IsStartOfContainer()) {
    if (atPreviousChar.IsCharLowSurrogateFollowingHighSurrogate()) {
      atPreviousChar = atPreviousChar.PreviousPoint();
    }
    // If caret is in middle of a surrogate pair, delete the surrogate pair
    // (blink-compat).
    else if (atPreviousChar.IsCharHighSurrogateFollowedByLowSurrogate()) {
      atNextChar = atNextChar.NextPoint();
    }
  }

  // If previous char is an collapsible white-spaces, delete all adjcent
  // white-spaces which are collapsed together.
  EditorDOMRangeInTexts rangeToDelete;
  if (atPreviousChar.IsCharCollapsibleASCIISpace() ||
      atPreviousChar.IsCharPreformattedNewLineCollapsedWithWhiteSpaces()) {
    const EditorDOMPointInText startToDelete =
        textFragmentDataAtCaret.GetFirstASCIIWhiteSpacePointCollapsedTo(
            atPreviousChar, nsIEditor::ePrevious);
    if (!startToDelete.IsSet()) {
      NS_WARNING(
          "WSRunScanner::GetFirstASCIIWhiteSpacePointCollapsedTo() failed");
      return Err(NS_ERROR_FAILURE);
    }
    const EditorDOMPointInText endToDelete =
        textFragmentDataAtCaret.GetEndOfCollapsibleASCIIWhiteSpaces(
            atPreviousChar, nsIEditor::ePrevious);
    if (!endToDelete.IsSet()) {
      NS_WARNING("WSRunScanner::GetEndOfCollapsibleASCIIWhiteSpaces() failed");
      return Err(NS_ERROR_FAILURE);
    }
    rangeToDelete = EditorDOMRangeInTexts(startToDelete, endToDelete);
  }
  // if previous char is not a collapsible white-space, remove it.
  else {
    rangeToDelete = EditorDOMRangeInTexts(atPreviousChar, atNextChar);
  }

  // If there is no removable and visible content, we should do nothing.
  if (rangeToDelete.Collapsed()) {
    return EditorDOMRangeInTexts();
  }

  // And also delete invisible white-spaces if they become visible.
  TextFragmentData textFragmentDataAtStart =
      rangeToDelete.StartRef() != aPoint
          ? TextFragmentData(rangeToDelete.StartRef(), &aEditingHost,
                             BlockInlineCheck::UseComputedDisplayStyle)
          : textFragmentDataAtCaret;
  TextFragmentData textFragmentDataAtEnd =
      rangeToDelete.EndRef() != aPoint
          ? TextFragmentData(rangeToDelete.EndRef(), &aEditingHost,
                             BlockInlineCheck::UseComputedDisplayStyle)
          : textFragmentDataAtCaret;
  if (NS_WARN_IF(!textFragmentDataAtStart.IsInitialized()) ||
      NS_WARN_IF(!textFragmentDataAtEnd.IsInitialized())) {
    return Err(NS_ERROR_FAILURE);
  }
  EditorDOMRangeInTexts extendedRangeToDelete =
      WSRunScanner::ComputeRangeInTextNodesContainingInvisibleWhiteSpaces(
          textFragmentDataAtStart, textFragmentDataAtEnd);
  MOZ_ASSERT(extendedRangeToDelete.IsPositionedAndValid());
  return extendedRangeToDelete.IsPositioned() ? extendedRangeToDelete
                                              : rangeToDelete;
}

// static
Result<EditorDOMRangeInTexts, nsresult>
WSRunScanner::GetRangeInTextNodesToForwardDeleteFrom(
    const EditorDOMPoint& aPoint, const Element& aEditingHost) {
  // Corresponding to computing delete range part of
  // `WhiteSpaceVisibilityKeeper::DeleteInclusiveNextWhiteSpace()`
  MOZ_ASSERT(aPoint.IsSetAndValid());

  TextFragmentData textFragmentDataAtCaret(
      aPoint, &aEditingHost, BlockInlineCheck::UseComputedDisplayStyle);
  if (NS_WARN_IF(!textFragmentDataAtCaret.IsInitialized())) {
    return Err(NS_ERROR_FAILURE);
  }
  auto atCaret =
      textFragmentDataAtCaret
          .GetInclusiveNextEditableCharPoint<EditorDOMPointInText>(aPoint);
  if (!atCaret.IsSet()) {
    return EditorDOMRangeInTexts();  // There is no content in the block.
  }
  // If caret is in middle of a surrogate pair, we should remove next
  // character (blink-compat).
  if (!atCaret.IsEndOfContainer() &&
      atCaret.IsCharLowSurrogateFollowingHighSurrogate()) {
    atCaret = atCaret.NextPoint();
  }

  // XXX When next char point is in an empty text node, we do nothing,
  //     but this must look odd from point of user view.  We should delete
  //     something after aPoint.
  if (atCaret.IsEndOfContainer()) {
    return EditorDOMRangeInTexts();
  }

  // Extend delete range if previous char is a low surrogate following
  // a high surrogate.
  EditorDOMPointInText atNextChar = atCaret.NextPoint();
  if (atCaret.IsCharHighSurrogateFollowedByLowSurrogate()) {
    atNextChar = atNextChar.NextPoint();
  }

  // If next char is a collapsible white-space, delete all adjcent white-spaces
  // which are collapsed together.
  EditorDOMRangeInTexts rangeToDelete;
  if (atCaret.IsCharCollapsibleASCIISpace() ||
      atCaret.IsCharPreformattedNewLineCollapsedWithWhiteSpaces()) {
    const EditorDOMPointInText startToDelete =
        textFragmentDataAtCaret.GetFirstASCIIWhiteSpacePointCollapsedTo(
            atCaret, nsIEditor::eNext);
    if (!startToDelete.IsSet()) {
      NS_WARNING(
          "WSRunScanner::GetFirstASCIIWhiteSpacePointCollapsedTo() failed");
      return Err(NS_ERROR_FAILURE);
    }
    const EditorDOMPointInText endToDelete =
        textFragmentDataAtCaret.GetEndOfCollapsibleASCIIWhiteSpaces(
            atCaret, nsIEditor::eNext);
    if (!endToDelete.IsSet()) {
      NS_WARNING("WSRunScanner::GetEndOfCollapsibleASCIIWhiteSpaces() failed");
      return Err(NS_ERROR_FAILURE);
    }
    rangeToDelete = EditorDOMRangeInTexts(startToDelete, endToDelete);
  }
  // if next char is not a collapsible white-space, remove it.
  else {
    rangeToDelete = EditorDOMRangeInTexts(atCaret, atNextChar);
  }

  // If there is no removable and visible content, we should do nothing.
  if (rangeToDelete.Collapsed()) {
    return EditorDOMRangeInTexts();
  }

  // And also delete invisible white-spaces if they become visible.
  TextFragmentData textFragmentDataAtStart =
      rangeToDelete.StartRef() != aPoint
          ? TextFragmentData(rangeToDelete.StartRef(), &aEditingHost,
                             BlockInlineCheck::UseComputedDisplayStyle)
          : textFragmentDataAtCaret;
  TextFragmentData textFragmentDataAtEnd =
      rangeToDelete.EndRef() != aPoint
          ? TextFragmentData(rangeToDelete.EndRef(), &aEditingHost,
                             BlockInlineCheck::UseComputedDisplayStyle)
          : textFragmentDataAtCaret;
  if (NS_WARN_IF(!textFragmentDataAtStart.IsInitialized()) ||
      NS_WARN_IF(!textFragmentDataAtEnd.IsInitialized())) {
    return Err(NS_ERROR_FAILURE);
  }
  EditorDOMRangeInTexts extendedRangeToDelete =
      WSRunScanner::ComputeRangeInTextNodesContainingInvisibleWhiteSpaces(
          textFragmentDataAtStart, textFragmentDataAtEnd);
  MOZ_ASSERT(extendedRangeToDelete.IsPositionedAndValid());
  return extendedRangeToDelete.IsPositioned() ? extendedRangeToDelete
                                              : rangeToDelete;
}

// static
EditorDOMRange WSRunScanner::GetRangesForDeletingAtomicContent(
    Element* aEditingHost, const nsIContent& aAtomicContent) {
  if (aAtomicContent.IsHTMLElement(nsGkAtoms::br)) {
    // Preceding white-spaces should be preserved, but the following
    // white-spaces should be invisible around `<br>` element.
    TextFragmentData textFragmentDataAfterBRElement(
        EditorDOMPoint::After(aAtomicContent), aEditingHost,
        BlockInlineCheck::UseComputedDisplayStyle);
    if (NS_WARN_IF(!textFragmentDataAfterBRElement.IsInitialized())) {
      return EditorDOMRange();  // TODO: Make here return error with Err.
    }
    const EditorDOMRangeInTexts followingInvisibleWhiteSpaces =
        textFragmentDataAfterBRElement.GetNonCollapsedRangeInTexts(
            textFragmentDataAfterBRElement
                .InvisibleLeadingWhiteSpaceRangeRef());
    return followingInvisibleWhiteSpaces.IsPositioned() &&
                   !followingInvisibleWhiteSpaces.Collapsed()
               ? EditorDOMRange(
                     EditorDOMPoint(const_cast<nsIContent*>(&aAtomicContent)),
                     followingInvisibleWhiteSpaces.EndRef())
               : EditorDOMRange(
                     EditorDOMPoint(const_cast<nsIContent*>(&aAtomicContent)),
                     EditorDOMPoint::After(aAtomicContent));
  }

  if (!HTMLEditUtils::IsBlockElement(
          aAtomicContent, BlockInlineCheck::UseComputedDisplayStyle)) {
    // Both preceding and following white-spaces around it should be preserved
    // around inline elements like `<img>`.
    return EditorDOMRange(
        EditorDOMPoint(const_cast<nsIContent*>(&aAtomicContent)),
        EditorDOMPoint::After(aAtomicContent));
  }

  // Both preceding and following white-spaces can be invisible around a
  // block element.
  TextFragmentData textFragmentDataBeforeAtomicContent(
      EditorDOMPoint(const_cast<nsIContent*>(&aAtomicContent)), aEditingHost,
      BlockInlineCheck::UseComputedDisplayStyle);
  if (NS_WARN_IF(!textFragmentDataBeforeAtomicContent.IsInitialized())) {
    return EditorDOMRange();  // TODO: Make here return error with Err.
  }
  const EditorDOMRangeInTexts precedingInvisibleWhiteSpaces =
      textFragmentDataBeforeAtomicContent.GetNonCollapsedRangeInTexts(
          textFragmentDataBeforeAtomicContent
              .InvisibleTrailingWhiteSpaceRangeRef());
  TextFragmentData textFragmentDataAfterAtomicContent(
      EditorDOMPoint::After(aAtomicContent), aEditingHost,
      BlockInlineCheck::UseComputedDisplayStyle);
  if (NS_WARN_IF(!textFragmentDataAfterAtomicContent.IsInitialized())) {
    return EditorDOMRange();  // TODO: Make here return error with Err.
  }
  const EditorDOMRangeInTexts followingInvisibleWhiteSpaces =
      textFragmentDataAfterAtomicContent.GetNonCollapsedRangeInTexts(
          textFragmentDataAfterAtomicContent
              .InvisibleLeadingWhiteSpaceRangeRef());
  if (precedingInvisibleWhiteSpaces.StartRef().IsSet() &&
      followingInvisibleWhiteSpaces.EndRef().IsSet()) {
    return EditorDOMRange(precedingInvisibleWhiteSpaces.StartRef(),
                          followingInvisibleWhiteSpaces.EndRef());
  }
  if (precedingInvisibleWhiteSpaces.StartRef().IsSet()) {
    return EditorDOMRange(precedingInvisibleWhiteSpaces.StartRef(),
                          EditorDOMPoint::After(aAtomicContent));
  }
  if (followingInvisibleWhiteSpaces.EndRef().IsSet()) {
    return EditorDOMRange(
        EditorDOMPoint(const_cast<nsIContent*>(&aAtomicContent)),
        followingInvisibleWhiteSpaces.EndRef());
  }
  return EditorDOMRange(
      EditorDOMPoint(const_cast<nsIContent*>(&aAtomicContent)),
      EditorDOMPoint::After(aAtomicContent));
}

// static
EditorDOMRange WSRunScanner::GetRangeForDeletingBlockElementBoundaries(
    const HTMLEditor& aHTMLEditor, const Element& aLeftBlockElement,
    const Element& aRightBlockElement,
    const EditorDOMPoint& aPointContainingTheOtherBlock) {
  MOZ_ASSERT(&aLeftBlockElement != &aRightBlockElement);
  MOZ_ASSERT_IF(
      aPointContainingTheOtherBlock.IsSet(),
      aPointContainingTheOtherBlock.GetContainer() == &aLeftBlockElement ||
          aPointContainingTheOtherBlock.GetContainer() == &aRightBlockElement);
  MOZ_ASSERT_IF(
      aPointContainingTheOtherBlock.GetContainer() == &aLeftBlockElement,
      aRightBlockElement.IsInclusiveDescendantOf(
          aPointContainingTheOtherBlock.GetChild()));
  MOZ_ASSERT_IF(
      aPointContainingTheOtherBlock.GetContainer() == &aRightBlockElement,
      aLeftBlockElement.IsInclusiveDescendantOf(
          aPointContainingTheOtherBlock.GetChild()));
  MOZ_ASSERT_IF(
      !aPointContainingTheOtherBlock.IsSet(),
      !aRightBlockElement.IsInclusiveDescendantOf(&aLeftBlockElement));
  MOZ_ASSERT_IF(
      !aPointContainingTheOtherBlock.IsSet(),
      !aLeftBlockElement.IsInclusiveDescendantOf(&aRightBlockElement));
  MOZ_ASSERT_IF(!aPointContainingTheOtherBlock.IsSet(),
                EditorRawDOMPoint(const_cast<Element*>(&aLeftBlockElement))
                    .IsBefore(EditorRawDOMPoint(
                        const_cast<Element*>(&aRightBlockElement))));

  const Element* editingHost = aHTMLEditor.ComputeEditingHost();

  EditorDOMRange range;
  // Include trailing invisible white-spaces in aLeftBlockElement.
  TextFragmentData textFragmentDataAtEndOfLeftBlockElement(
      aPointContainingTheOtherBlock.GetContainer() == &aLeftBlockElement
          ? aPointContainingTheOtherBlock
          : EditorDOMPoint::AtEndOf(const_cast<Element&>(aLeftBlockElement)),
      editingHost, BlockInlineCheck::UseComputedDisplayOutsideStyle);
  if (NS_WARN_IF(!textFragmentDataAtEndOfLeftBlockElement.IsInitialized())) {
    return EditorDOMRange();  // TODO: Make here return error with Err.
  }
  if (textFragmentDataAtEndOfLeftBlockElement.StartsFromInvisibleBRElement()) {
    // If the left block element ends with an invisible `<br>` element,
    // it'll be deleted (and it means there is no invisible trailing
    // white-spaces).  Therefore, the range should start from the invisible
    // `<br>` element.
    range.SetStart(EditorDOMPoint(
        textFragmentDataAtEndOfLeftBlockElement.StartReasonBRElementPtr()));
  } else {
    const EditorDOMRange& trailingWhiteSpaceRange =
        textFragmentDataAtEndOfLeftBlockElement
            .InvisibleTrailingWhiteSpaceRangeRef();
    if (trailingWhiteSpaceRange.StartRef().IsSet()) {
      range.SetStart(trailingWhiteSpaceRange.StartRef());
    } else {
      range.SetStart(textFragmentDataAtEndOfLeftBlockElement.ScanStartRef());
    }
  }
  // Include leading invisible white-spaces in aRightBlockElement.
  TextFragmentData textFragmentDataAtStartOfRightBlockElement(
      aPointContainingTheOtherBlock.GetContainer() == &aRightBlockElement &&
              !aPointContainingTheOtherBlock.IsEndOfContainer()
          ? aPointContainingTheOtherBlock.NextPoint()
          : EditorDOMPoint(const_cast<Element*>(&aRightBlockElement), 0u),
      editingHost, BlockInlineCheck::UseComputedDisplayOutsideStyle);
  if (NS_WARN_IF(!textFragmentDataAtStartOfRightBlockElement.IsInitialized())) {
    return EditorDOMRange();  // TODO: Make here return error with Err.
  }
  const EditorDOMRange& leadingWhiteSpaceRange =
      textFragmentDataAtStartOfRightBlockElement
          .InvisibleLeadingWhiteSpaceRangeRef();
  if (leadingWhiteSpaceRange.EndRef().IsSet()) {
    range.SetEnd(leadingWhiteSpaceRange.EndRef());
  } else {
    range.SetEnd(textFragmentDataAtStartOfRightBlockElement.ScanStartRef());
  }
  return range;
}

// static
EditorDOMRange
WSRunScanner::GetRangeContainingInvisibleWhiteSpacesAtRangeBoundaries(
    Element* aEditingHost, const EditorDOMRange& aRange) {
  MOZ_ASSERT(aRange.IsPositionedAndValid());
  MOZ_ASSERT(aRange.EndRef().IsSetAndValid());
  MOZ_ASSERT(aRange.StartRef().IsSetAndValid());

  EditorDOMRange result;
  TextFragmentData textFragmentDataAtStart(
      aRange.StartRef(), aEditingHost,
      BlockInlineCheck::UseComputedDisplayStyle);
  if (NS_WARN_IF(!textFragmentDataAtStart.IsInitialized())) {
    return EditorDOMRange();  // TODO: Make here return error with Err.
  }
  const EditorDOMRangeInTexts invisibleLeadingWhiteSpacesAtStart =
      textFragmentDataAtStart.GetNonCollapsedRangeInTexts(
          textFragmentDataAtStart.InvisibleLeadingWhiteSpaceRangeRef());
  if (invisibleLeadingWhiteSpacesAtStart.IsPositioned() &&
      !invisibleLeadingWhiteSpacesAtStart.Collapsed()) {
    result.SetStart(invisibleLeadingWhiteSpacesAtStart.StartRef());
  } else {
    const EditorDOMRangeInTexts invisibleTrailingWhiteSpacesAtStart =
        textFragmentDataAtStart.GetNonCollapsedRangeInTexts(
            textFragmentDataAtStart.InvisibleTrailingWhiteSpaceRangeRef());
    if (invisibleTrailingWhiteSpacesAtStart.IsPositioned() &&
        !invisibleTrailingWhiteSpacesAtStart.Collapsed()) {
      MOZ_ASSERT(
          invisibleTrailingWhiteSpacesAtStart.StartRef().EqualsOrIsBefore(
              aRange.StartRef()));
      result.SetStart(invisibleTrailingWhiteSpacesAtStart.StartRef());
    }
    // If there is no invisible white-space and the line starts with a
    // text node, shrink the range to start of the text node.
    else if (!aRange.StartRef().IsInTextNode() &&
             (textFragmentDataAtStart.StartsFromBlockBoundary() ||
              textFragmentDataAtStart.StartsFromInlineEditingHostBoundary()) &&
             textFragmentDataAtStart.EndRef().IsInTextNode()) {
      result.SetStart(textFragmentDataAtStart.EndRef());
    }
  }
  if (!result.StartRef().IsSet()) {
    result.SetStart(aRange.StartRef());
  }

  TextFragmentData textFragmentDataAtEnd(
      aRange.EndRef(), aEditingHost, BlockInlineCheck::UseComputedDisplayStyle);
  if (NS_WARN_IF(!textFragmentDataAtEnd.IsInitialized())) {
    return EditorDOMRange();  // TODO: Make here return error with Err.
  }
  const EditorDOMRangeInTexts invisibleLeadingWhiteSpacesAtEnd =
      textFragmentDataAtEnd.GetNonCollapsedRangeInTexts(
          textFragmentDataAtEnd.InvisibleTrailingWhiteSpaceRangeRef());
  if (invisibleLeadingWhiteSpacesAtEnd.IsPositioned() &&
      !invisibleLeadingWhiteSpacesAtEnd.Collapsed()) {
    result.SetEnd(invisibleLeadingWhiteSpacesAtEnd.EndRef());
  } else {
    const EditorDOMRangeInTexts invisibleLeadingWhiteSpacesAtEnd =
        textFragmentDataAtEnd.GetNonCollapsedRangeInTexts(
            textFragmentDataAtEnd.InvisibleLeadingWhiteSpaceRangeRef());
    if (invisibleLeadingWhiteSpacesAtEnd.IsPositioned() &&
        !invisibleLeadingWhiteSpacesAtEnd.Collapsed()) {
      MOZ_ASSERT(aRange.EndRef().EqualsOrIsBefore(
          invisibleLeadingWhiteSpacesAtEnd.EndRef()));
      result.SetEnd(invisibleLeadingWhiteSpacesAtEnd.EndRef());
    }
    // If there is no invisible white-space and the line ends with a text
    // node, shrink the range to end of the text node.
    else if (!aRange.EndRef().IsInTextNode() &&
             (textFragmentDataAtEnd.EndsByBlockBoundary() ||
              textFragmentDataAtEnd.EndsByInlineEditingHostBoundary()) &&
             textFragmentDataAtEnd.StartRef().IsInTextNode()) {
      result.SetEnd(EditorDOMPoint::AtEndOf(
          *textFragmentDataAtEnd.StartRef().ContainerAs<Text>()));
    }
  }
  if (!result.EndRef().IsSet()) {
    result.SetEnd(aRange.EndRef());
  }
  MOZ_ASSERT(result.IsPositionedAndValid());
  return result;
}

/******************************************************************************
 * Utilities for other things.
 ******************************************************************************/

// static
Result<bool, nsresult>
WSRunScanner::ShrinkRangeIfStartsFromOrEndsAfterAtomicContent(
    const HTMLEditor& aHTMLEditor, nsRange& aRange,
    const Element* aEditingHost) {
  MOZ_ASSERT(aRange.IsPositioned());
  MOZ_ASSERT(!aRange.IsInAnySelection(),
             "Changing range in selection may cause running script");

  if (NS_WARN_IF(!aRange.GetStartContainer()) ||
      NS_WARN_IF(!aRange.GetEndContainer())) {
    return Err(NS_ERROR_FAILURE);
  }

  if (!aRange.GetStartContainer()->IsContent() ||
      !aRange.GetEndContainer()->IsContent()) {
    return false;
  }

  // If the range crosses a block boundary, we should do nothing for now
  // because it hits a bug of inserting a padding `<br>` element after
  // joining the blocks.
  if (HTMLEditUtils::GetInclusiveAncestorElement(
          *aRange.GetStartContainer()->AsContent(),
          HTMLEditUtils::ClosestEditableBlockElementExceptHRElement,
          BlockInlineCheck::UseComputedDisplayStyle) !=
      HTMLEditUtils::GetInclusiveAncestorElement(
          *aRange.GetEndContainer()->AsContent(),
          HTMLEditUtils::ClosestEditableBlockElementExceptHRElement,
          BlockInlineCheck::UseComputedDisplayStyle)) {
    return false;
  }

  nsIContent* startContent = nullptr;
  if (aRange.GetStartContainer() && aRange.GetStartContainer()->IsText() &&
      aRange.GetStartContainer()->AsText()->Length() == aRange.StartOffset()) {
    // If next content is a visible `<br>` element, special inline content
    // (e.g., `<img>`, non-editable text node, etc) or a block level void
    // element like `<hr>`, the range should start with it.
    TextFragmentData textFragmentDataAtStart(
        EditorRawDOMPoint(aRange.StartRef()), aEditingHost,
        BlockInlineCheck::UseComputedDisplayStyle);
    if (NS_WARN_IF(!textFragmentDataAtStart.IsInitialized())) {
      return Err(NS_ERROR_FAILURE);
    }
    if (textFragmentDataAtStart.EndsByVisibleBRElement()) {
      startContent = textFragmentDataAtStart.EndReasonBRElementPtr();
    } else if (textFragmentDataAtStart.EndsBySpecialContent() ||
               (textFragmentDataAtStart.EndsByOtherBlockElement() &&
                !HTMLEditUtils::IsContainerNode(
                    *textFragmentDataAtStart
                         .EndReasonOtherBlockElementPtr()))) {
      startContent = textFragmentDataAtStart.GetEndReasonContent();
    }
  }

  nsIContent* endContent = nullptr;
  if (aRange.GetEndContainer() && aRange.GetEndContainer()->IsText() &&
      !aRange.EndOffset()) {
    // If previous content is a visible `<br>` element, special inline content
    // (e.g., `<img>`, non-editable text node, etc) or a block level void
    // element like `<hr>`, the range should end after it.
    TextFragmentData textFragmentDataAtEnd(
        EditorRawDOMPoint(aRange.EndRef()), aEditingHost,
        BlockInlineCheck::UseComputedDisplayStyle);
    if (NS_WARN_IF(!textFragmentDataAtEnd.IsInitialized())) {
      return Err(NS_ERROR_FAILURE);
    }
    if (textFragmentDataAtEnd.StartsFromVisibleBRElement()) {
      endContent = textFragmentDataAtEnd.StartReasonBRElementPtr();
    } else if (textFragmentDataAtEnd.StartsFromSpecialContent() ||
               (textFragmentDataAtEnd.StartsFromOtherBlockElement() &&
                !HTMLEditUtils::IsContainerNode(
                    *textFragmentDataAtEnd
                         .StartReasonOtherBlockElementPtr()))) {
      endContent = textFragmentDataAtEnd.GetStartReasonContent();
    }
  }

  if (!startContent && !endContent) {
    return false;
  }

  nsresult rv = aRange.SetStartAndEnd(
      startContent ? RangeBoundary(
                         startContent->GetParentNode(),
                         startContent->GetPreviousSibling())  // at startContent
                   : aRange.StartRef(),
      endContent ? RangeBoundary(endContent->GetParentNode(),
                                 endContent)  // after endContent
                 : aRange.EndRef());
  if (NS_FAILED(rv)) {
    NS_WARNING("nsRange::SetStartAndEnd() failed");
    return Err(rv);
  }
  return true;
}

}  // namespace mozilla
