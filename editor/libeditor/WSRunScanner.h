/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WSRunScanner_h
#define WSRunScanner_h

#include "EditorBase.h"
#include "EditorForwards.h"
#include "EditorDOMPoint.h"  // for EditorDOMPoint
#include "HTMLEditor.h"
#include "HTMLEditUtils.h"

#include "mozilla/Assertions.h"
#include "mozilla/Maybe.h"
#include "mozilla/Result.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/HTMLBRElement.h"
#include "mozilla/dom/Text.h"
#include "nsCOMPtr.h"
#include "nsIContent.h"

namespace mozilla {

/**
 * WSScanResult is result of ScanNextVisibleNodeOrBlockBoundaryFrom(),
 * ScanPreviousVisibleNodeOrBlockBoundaryFrom(), and their static wrapper
 * methods.  This will have information of found visible content (and its
 * position) or reached block element or topmost editable content at the
 * start of scanner.
 */
class MOZ_STACK_CLASS WSScanResult final {
 private:
  using Element = dom::Element;
  using HTMLBRElement = dom::HTMLBRElement;
  using Text = dom::Text;

  enum class WSType : uint8_t {
    NotInitialized,
    // Could be the DOM tree is broken as like crash tests.
    UnexpectedError,
    // The scanner cannot work in uncomposed tree, but tried to scan in it.
    InUncomposedDoc,
    // The run is maybe collapsible white-spaces at start of a hard line.
    LeadingWhiteSpaces,
    // The run is maybe collapsible white-spaces at end of a hard line.
    TrailingWhiteSpaces,
    // Collapsible, but visible white-spaces.
    CollapsibleWhiteSpaces,
    // Visible characters except collapsible white-spaces.
    NonCollapsibleCharacters,
    // Special content such as `<img>`, etc.
    SpecialContent,
    // <br> element.
    BRElement,
    // A linefeed which is preformatted.
    PreformattedLineBreak,
    // Other block's boundary (child block of current block, maybe).
    OtherBlockBoundary,
    // Current block's boundary.
    CurrentBlockBoundary,
    // Inline editing host boundary.
    InlineEditingHostBoundary,
  };

  friend std::ostream& operator<<(std::ostream& aStream, const WSType& aType) {
    switch (aType) {
      case WSType::NotInitialized:
        return aStream << "WSType::NotInitialized";
      case WSType::UnexpectedError:
        return aStream << "WSType::UnexpectedError";
      case WSType::InUncomposedDoc:
        return aStream << "WSType::InUncomposedDoc";
      case WSType::LeadingWhiteSpaces:
        return aStream << "WSType::LeadingWhiteSpaces";
      case WSType::TrailingWhiteSpaces:
        return aStream << "WSType::TrailingWhiteSpaces";
      case WSType::CollapsibleWhiteSpaces:
        return aStream << "WSType::CollapsibleWhiteSpaces";
      case WSType::NonCollapsibleCharacters:
        return aStream << "WSType::NonCollapsibleCharacters";
      case WSType::SpecialContent:
        return aStream << "WSType::SpecialContent";
      case WSType::BRElement:
        return aStream << "WSType::BRElement";
      case WSType::PreformattedLineBreak:
        return aStream << "WSType::PreformattedLineBreak";
      case WSType::OtherBlockBoundary:
        return aStream << "WSType::OtherBlockBoundary";
      case WSType::CurrentBlockBoundary:
        return aStream << "WSType::CurrentBlockBoundary";
      case WSType::InlineEditingHostBoundary:
        return aStream << "WSType::InlineEditingHostBoundary";
    }
    return aStream << "<Illegal value>";
  }

  friend class WSRunScanner;  // Because of WSType.

  explicit WSScanResult(WSType aReason) : mReason(aReason) {
    MOZ_ASSERT(mReason == WSType::UnexpectedError ||
               mReason == WSType::NotInitialized);
  }

 public:
  WSScanResult() = delete;
  enum class ScanDirection : bool { Backward, Forward };
  WSScanResult(const WSRunScanner& aScanner, ScanDirection aScanDirection,
               nsIContent& aContent, WSType aReason)
      : mContent(&aContent), mReason(aReason), mDirection(aScanDirection) {
    MOZ_ASSERT(aReason != WSType::CollapsibleWhiteSpaces &&
               aReason != WSType::NonCollapsibleCharacters &&
               aReason != WSType::PreformattedLineBreak);
    AssertIfInvalidData(aScanner);
  }
  WSScanResult(const WSRunScanner& aScanner, ScanDirection aScanDirection,
               const EditorDOMPoint& aPoint, WSType aReason)
      : mContent(aPoint.GetContainerAs<nsIContent>()),
        mOffset(Some(aPoint.Offset())),
        mReason(aReason),
        mDirection(aScanDirection) {
    AssertIfInvalidData(aScanner);
  }

  static WSScanResult Error() { return WSScanResult(WSType::UnexpectedError); }

  void AssertIfInvalidData(const WSRunScanner& aScanner) const;

  bool Failed() const {
    return mReason == WSType::NotInitialized ||
           mReason == WSType::UnexpectedError;
  }

  /**
   * GetContent() returns found visible and editable content/element.
   * See MOZ_ASSERT_IF()s in AssertIfInvalidData() for the detail.
   */
  nsIContent* GetContent() const { return mContent; }

  [[nodiscard]] bool ContentIsElement() const {
    return mContent && mContent->IsElement();
  }

  [[nodiscard]] bool ContentIsText() const {
    return mContent && mContent->IsText();
  }

  /**
   * The following accessors makes it easier to understand each callers.
   */
  MOZ_NEVER_INLINE_DEBUG Element* ElementPtr() const {
    MOZ_DIAGNOSTIC_ASSERT(mContent->IsElement());
    return mContent->AsElement();
  }
  MOZ_NEVER_INLINE_DEBUG HTMLBRElement* BRElementPtr() const {
    MOZ_DIAGNOSTIC_ASSERT(mContent->IsHTMLElement(nsGkAtoms::br));
    return static_cast<HTMLBRElement*>(mContent.get());
  }
  MOZ_NEVER_INLINE_DEBUG Text* TextPtr() const {
    MOZ_DIAGNOSTIC_ASSERT(mContent->IsText());
    return mContent->AsText();
  }

  /**
   * Returns true if found or reached content is editable.
   */
  bool IsContentEditable() const { return mContent && mContent->IsEditable(); }

  [[nodiscard]] bool IsContentEditableRoot() const {
    return mContent && mContent->IsElement() &&
           HTMLEditUtils::ElementIsEditableRoot(*mContent->AsElement());
  }

  /**
   * Offset_Deprecated() returns meaningful value only when
   * InVisibleOrCollapsibleCharacters() returns true or the scanner reached to
   * start or end of its scanning range and that is same as start or end
   * container which are specified when the scanner is initialized.  If it's
   * result of scanning backward, this offset means the point of the found
   * point. Otherwise, i.e., scanning forward, this offset means next point
   * of the found point.  E.g., if it reaches a collapsible white-space, this
   * offset is at the first non-collapsible character after it.
   */
  MOZ_NEVER_INLINE_DEBUG uint32_t Offset_Deprecated() const {
    NS_ASSERTION(mOffset.isSome(), "Retrieved non-meaningful offset");
    return mOffset.valueOr(0);
  }

  /**
   * Point_Deprecated() returns the position in found visible node or reached
   * block boundary.  So, this returns meaningful point only when
   * Offset_Deprecated() returns meaningful value.
   */
  template <typename EditorDOMPointType>
  EditorDOMPointType Point_Deprecated() const {
    NS_ASSERTION(mOffset.isSome(), "Retrieved non-meaningful point");
    return EditorDOMPointType(mContent, mOffset.valueOr(0));
  }

  /**
   * PointAtReachedContent() returns the position of found visible content or
   * reached block element.
   */
  template <typename EditorDOMPointType>
  EditorDOMPointType PointAtReachedContent() const {
    MOZ_ASSERT(mContent);
    switch (mReason) {
      case WSType::CollapsibleWhiteSpaces:
      case WSType::NonCollapsibleCharacters:
      case WSType::PreformattedLineBreak:
        MOZ_DIAGNOSTIC_ASSERT(mOffset.isSome());
        return mDirection == ScanDirection::Forward
                   ? EditorDOMPointType(mContent, mOffset.valueOr(0))
                   : EditorDOMPointType(mContent,
                                        std::max(mOffset.valueOr(1), 1u) - 1);
      default:
        return EditorDOMPointType(mContent);
    }
  }

  /**
   * PointAfterReachedContent() returns the next position of found visible
   * content or reached block element.
   */
  template <typename EditorDOMPointType>
  EditorDOMPointType PointAfterReachedContent() const {
    MOZ_ASSERT(mContent);
    return PointAtReachedContent<EditorDOMPointType>()
        .template NextPointOrAfterContainer<EditorDOMPointType>();
  }

  /**
   * The scanner reached <img> or something which is inline and is not a
   * container.
   */
  bool ReachedSpecialContent() const {
    return mReason == WSType::SpecialContent;
  }

  /**
   * The point is in visible characters or collapsible white-spaces.
   */
  bool InVisibleOrCollapsibleCharacters() const {
    return mReason == WSType::CollapsibleWhiteSpaces ||
           mReason == WSType::NonCollapsibleCharacters;
  }

  /**
   * The point is in collapsible white-spaces.
   */
  bool InCollapsibleWhiteSpaces() const {
    return mReason == WSType::CollapsibleWhiteSpaces;
  }

  /**
   * The point is in visible non-collapsible characters.
   */
  bool InNonCollapsibleCharacters() const {
    return mReason == WSType::NonCollapsibleCharacters;
  }

  /**
   * The scanner reached a <br> element.
   */
  bool ReachedBRElement() const { return mReason == WSType::BRElement; }
  bool ReachedVisibleBRElement() const {
    return ReachedBRElement() &&
           HTMLEditUtils::IsVisibleBRElement(*BRElementPtr());
  }
  bool ReachedInvisibleBRElement() const {
    return ReachedBRElement() &&
           HTMLEditUtils::IsInvisibleBRElement(*BRElementPtr());
  }

  bool ReachedPreformattedLineBreak() const {
    return mReason == WSType::PreformattedLineBreak;
  }

  /**
   * The scanner reached a <hr> element.
   */
  bool ReachedHRElement() const {
    return mContent && mContent->IsHTMLElement(nsGkAtoms::hr);
  }

  /**
   * The scanner reached current block boundary or other block element.
   */
  bool ReachedBlockBoundary() const {
    return mReason == WSType::CurrentBlockBoundary ||
           mReason == WSType::OtherBlockBoundary;
  }

  /**
   * The scanner reached current block element boundary.
   */
  bool ReachedCurrentBlockBoundary() const {
    return mReason == WSType::CurrentBlockBoundary;
  }

  /**
   * The scanner reached other block element.
   */
  bool ReachedOtherBlockElement() const {
    return mReason == WSType::OtherBlockBoundary;
  }

  /**
   * The scanner reached other block element that isn't editable
   */
  bool ReachedNonEditableOtherBlockElement() const {
    return ReachedOtherBlockElement() && !GetContent()->IsEditable();
  }

  /**
   * The scanner reached inline editing host boundary.
   */
  [[nodiscard]] bool ReachedInlineEditingHostBoundary() const {
    return mReason == WSType::InlineEditingHostBoundary;
  }

  /**
   * The scanner reached something non-text node.
   */
  bool ReachedSomethingNonTextContent() const {
    return !InVisibleOrCollapsibleCharacters();
  }

  [[nodiscard]] bool ReachedLineBoundary() const {
    switch (mReason) {
      case WSType::CurrentBlockBoundary:
      case WSType::OtherBlockBoundary:
      case WSType::BRElement:
      case WSType::PreformattedLineBreak:
        return true;
      default:
        return ReachedHRElement();
    }
  }

 private:
  nsCOMPtr<nsIContent> mContent;
  Maybe<uint32_t> mOffset;
  WSType mReason;
  ScanDirection mDirection = ScanDirection::Backward;
};

class MOZ_STACK_CLASS WSRunScanner final {
 private:
  using Element = dom::Element;
  using HTMLBRElement = dom::HTMLBRElement;
  using Text = dom::Text;

 public:
  using WSType = WSScanResult::WSType;

  enum class IgnoreNonEditableNodes : bool { No, Yes };
  enum class StopAtNonEditableNode : bool { No, Yes };
  enum class Scan : bool { All, EditableNodes };

  [[nodiscard]] constexpr static IgnoreNonEditableNodes
  ShouldIgnoreNonEditableSiblingsOrDescendants(Scan aScan) {
    return static_cast<IgnoreNonEditableNodes>(static_cast<bool>(aScan));
  }
  [[nodiscard]] constexpr static StopAtNonEditableNode
  ShouldStopAtNonEditableNode(Scan aScan) {
    return static_cast<StopAtNonEditableNode>(static_cast<bool>(aScan));
  }

  template <typename EditorDOMPointType>
  WSRunScanner(Scan aScanMode, const EditorDOMPointType& aScanStartPoint,
               BlockInlineCheck aBlockInlineCheck,
               const Element* aAncestorLimiter = nullptr)
      : mScanStartPoint(aScanStartPoint.template To<EditorDOMPoint>()),
        mTextFragmentDataAtStart(aScanMode, mScanStartPoint, aBlockInlineCheck,
                                 aAncestorLimiter),
        mScanMode(aScanMode) {}

  // ScanInclusiveNextVisibleNodeOrBlockBoundaryFrom() returns the first visible
  // node at or after aPoint.  If there is no visible nodes after aPoint,
  // returns topmost editable inline ancestor at end of current block.  See
  // comments around WSScanResult for the detail.  When you reach a character,
  // this returns WSScanResult both whose Point_Deprecated() and
  // PointAtReachedContent() return the found character position.
  template <typename PT, typename CT>
  WSScanResult ScanInclusiveNextVisibleNodeOrBlockBoundaryFrom(
      const EditorDOMPointBase<PT, CT>& aPoint) const;
  template <typename PT, typename CT>
  static WSScanResult ScanInclusiveNextVisibleNodeOrBlockBoundary(
      Scan aScanMode, const EditorDOMPointBase<PT, CT>& aPoint,
      BlockInlineCheck aBlockInlineCheck,
      const Element* aAncestorLimiter = nullptr) {
    return WSRunScanner(aScanMode, aPoint, aBlockInlineCheck, aAncestorLimiter)
        .ScanInclusiveNextVisibleNodeOrBlockBoundaryFrom(aPoint);
  }

  // ScanPreviousVisibleNodeOrBlockBoundaryFrom() returns the first visible node
  // before aPoint. If there is no visible nodes before aPoint, returns topmost
  // editable inline ancestor at start of current block.  See comments around
  // WSScanResult for the detail.  When you reach a character, this returns
  // WSScanResult whose Point_Deprecated() returns next point of the found
  // character and PointAtReachedContent() returns the point at found character.
  template <typename PT, typename CT>
  WSScanResult ScanPreviousVisibleNodeOrBlockBoundaryFrom(
      const EditorDOMPointBase<PT, CT>& aPoint) const;
  template <typename PT, typename CT>
  static WSScanResult ScanPreviousVisibleNodeOrBlockBoundary(
      Scan aScanMode, const EditorDOMPointBase<PT, CT>& aPoint,
      BlockInlineCheck aBlockInlineCheck,
      const Element* aAncestorLimiter = nullptr) {
    return WSRunScanner(aScanMode, aPoint, aBlockInlineCheck, aAncestorLimiter)
        .ScanPreviousVisibleNodeOrBlockBoundaryFrom(aPoint);
  }

  /**
   * Return a point in a `Text` node which is at current character or next
   * character if aPoint does not points a character or end of a `Text` node.
   */
  template <typename EditorDOMPointType, typename PT, typename CT>
  static EditorDOMPointType GetInclusiveNextCharPoint(
      Scan aScanMode, const EditorDOMPointBase<PT, CT>& aPoint,
      BlockInlineCheck aBlockInlineCheck,
      const Element* aAncestorLimiter = nullptr) {
    if (aPoint.IsInTextNode() && !aPoint.IsEndOfContainer() &&
        (aScanMode != Scan::EditableNodes ||
         HTMLEditUtils::IsSimplyEditableNode(
             *aPoint.template ContainerAs<Text>()))) {
      return EditorDOMPointType(aPoint.template ContainerAs<Text>(),
                                aPoint.Offset());
    }
    return WSRunScanner(aScanMode, aPoint, aBlockInlineCheck, aAncestorLimiter)
        .GetInclusiveNextCharPoint<EditorDOMPointType>(aPoint);
  }

  /**
   * Return a point in a `Text` node which is before aPoint.
   */
  template <typename EditorDOMPointType, typename PT, typename CT>
  static EditorDOMPointType GetPreviousCharPoint(
      Scan aScanMode, const EditorDOMPointBase<PT, CT>& aPoint,
      BlockInlineCheck aBlockInlineCheck,
      const Element* aAncestorLimiter = nullptr) {
    if (aPoint.IsInTextNode() && !aPoint.IsStartOfContainer() &&
        (aScanMode != Scan::EditableNodes ||
         HTMLEditUtils::IsSimplyEditableNode(
             *aPoint.template ContainerAs<Text>()))) {
      return EditorDOMPointType(aPoint.template ContainerAs<Text>(),
                                aPoint.Offset() - 1);
    }
    return WSRunScanner(aScanMode, aPoint, aBlockInlineCheck, aAncestorLimiter)
        .GetPreviousCharPoint<EditorDOMPointType>(aPoint);
  }

  /**
   * Scan aTextNode from end or start to find last or first visible things.
   * I.e., this returns a point immediately before or after invisible
   * white-spaces of aTextNode if aTextNode ends or begins with some invisible
   * white-spaces.
   * Note that the result may not be in different text node if aTextNode has
   * only invisible white-spaces and there is previous or next text node.
   */
  template <typename EditorDOMPointType>
  static EditorDOMPointType GetAfterLastVisiblePoint(
      Scan aScanMode, Text& aTextNode,
      const Element* aAncestorLimiter = nullptr);
  template <typename EditorDOMPointType>
  static EditorDOMPointType GetFirstVisiblePoint(
      Scan aScanMode, Text& aTextNode,
      const Element* aAncestorLimiter = nullptr);

  /**
   * GetRangeInTextNodesToForwardDeleteFrom() returns the range to remove
   * text when caret is at aPoint.
   */
  static Result<EditorDOMRangeInTexts, nsresult>
  GetRangeInTextNodesToForwardDeleteFrom(
      Scan aScanMode, const EditorDOMPoint& aPoint,
      const Element* aAncestorLimiter = nullptr);

  /**
   * GetRangeInTextNodesToBackspaceFrom() returns the range to remove text
   * when caret is at aPoint.
   */
  static Result<EditorDOMRangeInTexts, nsresult>
  GetRangeInTextNodesToBackspaceFrom(Scan aScanMode,
                                     const EditorDOMPoint& aPoint,
                                     const Element* aAncestorLimiter = nullptr);

  /**
   * GetRangesForDeletingAtomicContent() returns the range to delete
   * aAtomicContent.  If it's followed by invisible white-spaces, they will
   * be included into the range.
   */
  static EditorDOMRange GetRangesForDeletingAtomicContent(
      Scan aScanMode, const nsIContent& aAtomicContent,
      const Element* aAncestorLimiter = nullptr);

  /**
   * GetRangeForDeleteBlockElementBoundaries() returns a range starting from end
   * of aLeftBlockElement to start of aRightBlockElement and extend invisible
   * white-spaces around them.
   *
   * @param aLeftBlockElement   The block element which will be joined with
   *                            aRightBlockElement.
   * @param aRightBlockElement  The block element which will be joined with
   *                            aLeftBlockElement.  This must be an element
   *                            after aLeftBlockElement.
   * @param aPointContainingTheOtherBlock
   *                            When aRightBlockElement is an ancestor of
   *                            aLeftBlockElement, this must be set and the
   *                            container must be aRightBlockElement.
   *                            When aLeftBlockElement is an ancestor of
   *                            aRightBlockElement, this must be set and the
   *                            container must be aLeftBlockElement.
   *                            Otherwise, must not be set.
   */
  static EditorDOMRange GetRangeForDeletingBlockElementBoundaries(
      Scan aScanMode, const Element& aLeftBlockElement,
      const Element& aRightBlockElement,
      const EditorDOMPoint& aPointContainingTheOtherBlock,
      const Element* aAncestorLimiter = nullptr);

  /**
   * ShrinkRangeIfStartsFromOrEndsAfterAtomicContent() may shrink aRange if it
   * starts and/or ends with an atomic content, but the range boundary
   * is in adjacent text nodes.  Returns true if this modifies the range.
   */
  static Result<bool, nsresult> ShrinkRangeIfStartsFromOrEndsAfterAtomicContent(
      Scan aScanMode, nsRange& aRange,
      const Element* aAncestorLimiter = nullptr);

  /**
   * GetRangeContainingInvisibleWhiteSpacesAtRangeBoundaries() returns
   * extended range if range boundaries of aRange are in invisible white-spaces.
   */
  static EditorDOMRange GetRangeContainingInvisibleWhiteSpacesAtRangeBoundaries(
      Scan aScanMode, const EditorDOMRange& aRange,
      const Element* aAncestorLimiter = nullptr);

  /**
   * GetPrecedingBRElementUnlessVisibleContentFound() scans a `<br>` element
   * backward, but stops scanning it if the scanner finds visible character
   * or something.  In other words, this method ignores only invisible
   * white-spaces between `<br>` element and aPoint.
   */
  template <typename EditorDOMPointType>
  MOZ_NEVER_INLINE_DEBUG static HTMLBRElement*
  GetPrecedingBRElementUnlessVisibleContentFound(
      Scan aScanMode, const EditorDOMPointType& aPoint,
      BlockInlineCheck aBlockInlineCheck,
      const Element* aAncestorLimiter = nullptr) {
    MOZ_ASSERT(aPoint.IsSetAndValid());
    // XXX This method behaves differently even in similar point.
    //     If aPoint is in a text node following `<br>` element, reaches the
    //     `<br>` element when all characters between the `<br>` and
    //     aPoint are ASCII whitespaces.
    //     But if aPoint is not in a text node, e.g., at start of an inline
    //     element which is immediately after a `<br>` element, returns the
    //     `<br>` element even if there is no invisible white-spaces.
    if (aPoint.IsStartOfContainer()) {
      return nullptr;
    }
    // TODO: Scan for end boundary is redundant in this case, we should optimize
    //       it.
    TextFragmentData textFragmentData(aScanMode, aPoint, aBlockInlineCheck,
                                      aAncestorLimiter);
    return textFragmentData.StartsFromBRElement()
               ? textFragmentData.StartReasonBRElementPtr()
               : nullptr;
  }

  constexpr BlockInlineCheck BlockInlineCheckMode() const {
    return mTextFragmentDataAtStart.BlockInlineCheckMode();
  }

  const EditorDOMPoint& ScanStartRef() const { return mScanStartPoint; }

  /**
   * GetStartReasonContent() and GetEndReasonContent() return a node which
   * was found by scanning from mScanStartPoint backward or  forward.  If there
   * was white-spaces or text from the point, returns the text node.  Otherwise,
   * returns an element which is explained by the following methods.  Note that
   * when the reason is WSType::CurrentBlockBoundary, In most cases, it's
   * current block element which is editable, but also may be non-element and/or
   * non-editable.  See MOZ_ASSERT_IF()s in WSScanResult::AssertIfInvalidData()
   * for the detail.
   */
  nsIContent* GetStartReasonContent() const {
    return TextFragmentDataAtStartRef().GetStartReasonContent();
  }
  nsIContent* GetEndReasonContent() const {
    return TextFragmentDataAtStartRef().GetEndReasonContent();
  }

  bool StartsFromNonCollapsibleCharacters() const {
    return TextFragmentDataAtStartRef().StartsFromNonCollapsibleCharacters();
  }
  bool StartsFromSpecialContent() const {
    return TextFragmentDataAtStartRef().StartsFromSpecialContent();
  }
  bool StartsFromBRElement() const {
    return TextFragmentDataAtStartRef().StartsFromBRElement();
  }
  bool StartsFromVisibleBRElement() const {
    return TextFragmentDataAtStartRef().StartsFromVisibleBRElement();
  }
  bool StartsFromInvisibleBRElement() const {
    return TextFragmentDataAtStartRef().StartsFromInvisibleBRElement();
  }
  bool StartsFromPreformattedLineBreak() const {
    return TextFragmentDataAtStartRef().StartsFromPreformattedLineBreak();
  }
  bool StartsFromCurrentBlockBoundary() const {
    return TextFragmentDataAtStartRef().StartsFromCurrentBlockBoundary();
  }
  bool StartsFromOtherBlockElement() const {
    return TextFragmentDataAtStartRef().StartsFromOtherBlockElement();
  }
  bool StartsFromBlockBoundary() const {
    return TextFragmentDataAtStartRef().StartsFromBlockBoundary();
  }
  bool StartsFromInlineEditingHostBoundary() const {
    return TextFragmentDataAtStartRef().StartsFromInlineEditingHostBoundary();
  }
  bool StartsFromHardLineBreak() const {
    return TextFragmentDataAtStartRef().StartsFromHardLineBreak();
  }
  bool EndsByNonCollapsibleCharacters() const {
    return TextFragmentDataAtStartRef().EndsByNonCollapsibleCharacters();
  }
  bool EndsBySpecialContent() const {
    return TextFragmentDataAtStartRef().EndsBySpecialContent();
  }
  bool EndsByBRElement() const {
    return TextFragmentDataAtStartRef().EndsByBRElement();
  }
  bool EndsByVisibleBRElement() const {
    return TextFragmentDataAtStartRef().EndsByVisibleBRElement();
  }
  bool EndsByInvisibleBRElement() const {
    return TextFragmentDataAtStartRef().EndsByInvisibleBRElement();
  }
  bool EndsByPreformattedLineBreak() const {
    return TextFragmentDataAtStartRef().EndsByPreformattedLineBreak();
  }
  bool EndsByCurrentBlockBoundary() const {
    return TextFragmentDataAtStartRef().EndsByCurrentBlockBoundary();
  }
  bool EndsByOtherBlockElement() const {
    return TextFragmentDataAtStartRef().EndsByOtherBlockElement();
  }
  bool EndsByBlockBoundary() const {
    return TextFragmentDataAtStartRef().EndsByBlockBoundary();
  }
  bool EndsByInlineEditingHostBoundary() const {
    return TextFragmentDataAtStartRef().EndsByInlineEditingHostBoundary();
  }

  MOZ_NEVER_INLINE_DEBUG Element* StartReasonOtherBlockElementPtr() const {
    return TextFragmentDataAtStartRef().StartReasonOtherBlockElementPtr();
  }
  MOZ_NEVER_INLINE_DEBUG HTMLBRElement* StartReasonBRElementPtr() const {
    return TextFragmentDataAtStartRef().StartReasonBRElementPtr();
  }
  MOZ_NEVER_INLINE_DEBUG Element* EndReasonOtherBlockElementPtr() const {
    return TextFragmentDataAtStartRef().EndReasonOtherBlockElementPtr();
  }
  MOZ_NEVER_INLINE_DEBUG HTMLBRElement* EndReasonBRElementPtr() const {
    return TextFragmentDataAtStartRef().EndReasonBRElementPtr();
  }

 protected:
  using EditorType = EditorBase::EditorType;

  class TextFragmentData;

  // VisibleWhiteSpacesData represents 0 or more visible white-spaces.
  class MOZ_STACK_CLASS VisibleWhiteSpacesData final {
   public:
    bool IsInitialized() const {
      return mLeftWSType != WSType::NotInitialized ||
             mRightWSType != WSType::NotInitialized;
    }

    EditorDOMPoint StartRef() const { return mStartPoint; }
    EditorDOMPoint EndRef() const { return mEndPoint; }

    /**
     * Information why the white-spaces start from (i.e., this indicates the
     * previous content type of the fragment).
     */
    bool StartsFromNonCollapsibleCharacters() const {
      return mLeftWSType == WSType::NonCollapsibleCharacters;
    }
    bool StartsFromSpecialContent() const {
      return mLeftWSType == WSType::SpecialContent;
    }
    bool StartsFromPreformattedLineBreak() const {
      return mLeftWSType == WSType::PreformattedLineBreak;
    }

    /**
     * Information why the white-spaces end by (i.e., this indicates the
     * next content type of the fragment).
     */
    bool EndsByNonCollapsibleCharacters() const {
      return mRightWSType == WSType::NonCollapsibleCharacters;
    }
    bool EndsByTrailingWhiteSpaces() const {
      return mRightWSType == WSType::TrailingWhiteSpaces;
    }
    bool EndsBySpecialContent() const {
      return mRightWSType == WSType::SpecialContent;
    }
    bool EndsByBRElement() const { return mRightWSType == WSType::BRElement; }
    bool EndsByPreformattedLineBreak() const {
      return mRightWSType == WSType::PreformattedLineBreak;
    }
    bool EndsByBlockBoundary() const {
      return mRightWSType == WSType::CurrentBlockBoundary ||
             mRightWSType == WSType::OtherBlockBoundary;
    }
    bool EndsByInlineEditingHostBoundary() const {
      return mRightWSType == WSType::InlineEditingHostBoundary;
    }

    /**
     * ComparePoint() compares aPoint with the white-spaces.
     */
    enum class PointPosition {
      BeforeStartOfFragment,
      StartOfFragment,
      MiddleOfFragment,
      EndOfFragment,
      AfterEndOfFragment,
      NotInSameDOMTree,
    };
    template <typename EditorDOMPointType>
    PointPosition ComparePoint(const EditorDOMPointType& aPoint) const {
      MOZ_ASSERT(aPoint.IsSetAndValid());
      if (StartRef() == aPoint) {
        return PointPosition::StartOfFragment;
      }
      if (EndRef() == aPoint) {
        return PointPosition::EndOfFragment;
      }
      const bool startIsBeforePoint = StartRef().IsBefore(aPoint);
      const bool pointIsBeforeEnd = aPoint.IsBefore(EndRef());
      if (startIsBeforePoint && pointIsBeforeEnd) {
        return PointPosition::MiddleOfFragment;
      }
      if (startIsBeforePoint) {
        return PointPosition::AfterEndOfFragment;
      }
      if (pointIsBeforeEnd) {
        return PointPosition::BeforeStartOfFragment;
      }
      return PointPosition::NotInSameDOMTree;
    }

   private:
    // Initializers should be accessible only from `TextFragmentData`.
    friend class WSRunScanner::TextFragmentData;
    VisibleWhiteSpacesData()
        : mLeftWSType(WSType::NotInitialized),
          mRightWSType(WSType::NotInitialized) {}

    template <typename EditorDOMPointType>
    void SetStartPoint(const EditorDOMPointType& aStartPoint) {
      mStartPoint = aStartPoint;
    }
    template <typename EditorDOMPointType>
    void SetEndPoint(const EditorDOMPointType& aEndPoint) {
      mEndPoint = aEndPoint;
    }
    void SetStartFrom(WSType aLeftWSType) { mLeftWSType = aLeftWSType; }
    void SetStartFromLeadingWhiteSpaces() {
      mLeftWSType = WSType::LeadingWhiteSpaces;
    }
    void SetEndBy(WSType aRightWSType) { mRightWSType = aRightWSType; }
    void SetEndByTrailingWhiteSpaces() {
      mRightWSType = WSType::TrailingWhiteSpaces;
    }

    EditorDOMPoint mStartPoint;
    EditorDOMPoint mEndPoint;
    WSType mLeftWSType, mRightWSType;
  };

  using PointPosition = VisibleWhiteSpacesData::PointPosition;

  /**
   * Return aPoint if it points a character in a `Text` node, or start of next
   * `Text` node otherwise.
   * FYI: For the performance, this does not check whether given container is
   * not after mStart.mReasonContent or not.
   */
  template <typename EditorDOMPointType, typename PT, typename CT>
  EditorDOMPointType GetInclusiveNextCharPoint(
      const EditorDOMPointBase<PT, CT>& aPoint) const {
    return TextFragmentDataAtStartRef()
        .GetInclusiveNextCharPoint<EditorDOMPointType>(
            aPoint, ShouldIgnoreNonEditableSiblingsOrDescendants(mScanMode));
  }

  /**
   * Return the previous editable point in a `Text` node.  Note that this
   * returns the last character point when it meets non-empty text node,
   * otherwise, returns a point in an empty text node.
   * FYI: For the performance, this does not check whether given container is
   * not before mEnd.mReasonContent or not.
   */
  template <typename EditorDOMPointType, typename PT, typename CT>
  EditorDOMPointType GetPreviousCharPoint(
      const EditorDOMPointBase<PT, CT>& aPoint) const {
    return TextFragmentDataAtStartRef()
        .GetPreviousCharPoint<EditorDOMPointType>(
            aPoint, ShouldIgnoreNonEditableSiblingsOrDescendants(mScanMode));
  }

  /**
   * GetEndOfCollapsibleASCIIWhiteSpaces() returns the next visible char
   * (meaning a character except ASCII white-spaces) point or end of last text
   * node scanning from aPointAtASCIIWhiteSpace.
   * Note that this may return different text node from the container of
   * aPointAtASCIIWhiteSpace.
   */
  template <typename EditorDOMPointType>
  EditorDOMPointType GetEndOfCollapsibleASCIIWhiteSpaces(
      const EditorDOMPointInText& aPointAtASCIIWhiteSpace,
      nsIEditor::EDirection aDirectionToDelete) const {
    MOZ_ASSERT(aDirectionToDelete == nsIEditor::eNone ||
               aDirectionToDelete == nsIEditor::eNext ||
               aDirectionToDelete == nsIEditor::ePrevious);
    return TextFragmentDataAtStartRef()
        .GetEndOfCollapsibleASCIIWhiteSpaces<EditorDOMPointType>(
            aPointAtASCIIWhiteSpace, aDirectionToDelete);
  }

  /**
   * GetFirstASCIIWhiteSpacePointCollapsedTo() returns the first ASCII
   * white-space which aPointAtASCIIWhiteSpace belongs to.  In other words,
   * the white-space at aPointAtASCIIWhiteSpace should be collapsed into
   * the result.
   * Note that this may return different text node from the container of
   * aPointAtASCIIWhiteSpace.
   */
  template <typename EditorDOMPointType>
  EditorDOMPointType GetFirstASCIIWhiteSpacePointCollapsedTo(
      const EditorDOMPointInText& aPointAtASCIIWhiteSpace,
      nsIEditor::EDirection aDirectionToDelete) const {
    MOZ_ASSERT(aDirectionToDelete == nsIEditor::eNone ||
               aDirectionToDelete == nsIEditor::eNext ||
               aDirectionToDelete == nsIEditor::ePrevious);
    return TextFragmentDataAtStartRef()
        .GetFirstASCIIWhiteSpacePointCollapsedTo<EditorDOMPointType>(
            aPointAtASCIIWhiteSpace, aDirectionToDelete);
  }

  /**
   * TextFragmentData stores the information of white-space sequence which
   * contains `aPoint` of the constructor.
   */
  class MOZ_STACK_CLASS TextFragmentData final {
   private:
    class NoBreakingSpaceData;
    class MOZ_STACK_CLASS BoundaryData final {
     public:
      using NoBreakingSpaceData =
          WSRunScanner::TextFragmentData::NoBreakingSpaceData;

      /**
       * ScanCollapsibleWhiteSpaceStartFrom() returns start boundary data of
       * white-spaces containing aPoint.  When aPoint is in a text node and
       * points a non-white-space character or the text node is preformatted,
       * this returns the data at aPoint.
       *
       * @param aPoint            Scan start point.
       * @param aNBSPData         Optional.  If set, this recodes first and last
       *                          NBSP positions.
       */
      template <typename EditorDOMPointType>
      static BoundaryData ScanCollapsibleWhiteSpaceStartFrom(
          Scan aScanMode, const EditorDOMPointType& aPoint,
          NoBreakingSpaceData* aNBSPData, BlockInlineCheck aBlockInlineCheck,
          StopAtNonEditableNode aStopAtNonEditableNode,
          const Element& aAncestorLimiter);

      /**
       * ScanCollapsibleWhiteSpaceEndFrom() returns end boundary data of
       * white-spaces containing aPoint.  When aPoint is in a text node and
       * points a non-white-space character or the text node is preformatted,
       * this returns the data at aPoint.
       *
       * @param aPoint            Scan start point.
       * @param aNBSPData         Optional.  If set, this recodes first and last
       *                          NBSP positions.
       */
      template <typename EditorDOMPointType>
      static BoundaryData ScanCollapsibleWhiteSpaceEndFrom(
          Scan aScanMode, const EditorDOMPointType& aPoint,
          NoBreakingSpaceData* aNBSPData, BlockInlineCheck aBlockInlineCheck,
          StopAtNonEditableNode aStopAtNonEditableNode,
          const Element& aAncestorLimiter);

      BoundaryData() = default;
      template <typename EditorDOMPointType>
      BoundaryData(const EditorDOMPointType& aPoint, nsIContent& aReasonContent,
                   WSType aReason)
          : mReasonContent(&aReasonContent),
            mPoint(aPoint.template To<EditorDOMPoint>()),
            mReason(aReason) {}
      bool Initialized() const { return mReasonContent && mPoint.IsSet(); }

      nsIContent* GetReasonContent() const { return mReasonContent; }
      const EditorDOMPoint& PointRef() const { return mPoint; }
      WSType RawReason() const { return mReason; }

      bool IsNonCollapsibleCharacters() const {
        return mReason == WSType::NonCollapsibleCharacters;
      }
      bool IsSpecialContent() const {
        return mReason == WSType::SpecialContent;
      }
      bool IsBRElement() const { return mReason == WSType::BRElement; }
      bool IsPreformattedLineBreak() const {
        return mReason == WSType::PreformattedLineBreak;
      }
      bool IsCurrentBlockBoundary() const {
        return mReason == WSType::CurrentBlockBoundary;
      }
      bool IsOtherBlockBoundary() const {
        return mReason == WSType::OtherBlockBoundary;
      }
      bool IsBlockBoundary() const {
        return mReason == WSType::CurrentBlockBoundary ||
               mReason == WSType::OtherBlockBoundary;
      }
      bool IsInlineEditingHostBoundary() const {
        return mReason == WSType::InlineEditingHostBoundary;
      }
      bool IsHardLineBreak() const {
        return mReason == WSType::CurrentBlockBoundary ||
               mReason == WSType::OtherBlockBoundary ||
               mReason == WSType::BRElement ||
               mReason == WSType::PreformattedLineBreak;
      }
      MOZ_NEVER_INLINE_DEBUG Element* OtherBlockElementPtr() const {
        MOZ_DIAGNOSTIC_ASSERT(mReasonContent->IsElement());
        return mReasonContent->AsElement();
      }
      MOZ_NEVER_INLINE_DEBUG HTMLBRElement* BRElementPtr() const {
        MOZ_DIAGNOSTIC_ASSERT(mReasonContent->IsHTMLElement(nsGkAtoms::br));
        return static_cast<HTMLBRElement*>(mReasonContent.get());
      }

     private:
      /**
       * Helper methods of ScanCollapsibleWhiteSpaceStartFrom() and
       * ScanCollapsibleWhiteSpaceEndFrom() when they need to scan in a text
       * node.
       */
      template <typename EditorDOMPointType>
      static Maybe<BoundaryData> ScanCollapsibleWhiteSpaceStartInTextNode(
          const EditorDOMPointType& aPoint, NoBreakingSpaceData* aNBSPData,
          BlockInlineCheck aBlockInlineCheck);
      template <typename EditorDOMPointType>
      static Maybe<BoundaryData> ScanCollapsibleWhiteSpaceEndInTextNode(
          const EditorDOMPointType& aPoint, NoBreakingSpaceData* aNBSPData,
          BlockInlineCheck aBlockInlineCheck);

      nsCOMPtr<nsIContent> mReasonContent;
      EditorDOMPoint mPoint;
      // Must be one of WSType::NotInitialized,
      // WSType::NonCollapsibleCharacters, WSType::SpecialContent,
      // WSType::BRElement, WSType::CurrentBlockBoundary,
      // WSType::OtherBlockBoundary or WSType::InlineEditingHostBoundary.
      WSType mReason = WSType::NotInitialized;
    };

    class MOZ_STACK_CLASS NoBreakingSpaceData final {
     public:
      enum class Scanning { Forward, Backward };
      void NotifyNBSP(const EditorDOMPointInText& aPoint,
                      Scanning aScanningDirection) {
        MOZ_ASSERT(aPoint.IsSetAndValid());
        MOZ_ASSERT(aPoint.IsCharNBSP());
        if (!mFirst.IsSet() || aScanningDirection == Scanning::Backward) {
          mFirst = aPoint;
        }
        if (!mLast.IsSet() || aScanningDirection == Scanning::Forward) {
          mLast = aPoint;
        }
      }

      const EditorDOMPointInText& FirstPointRef() const { return mFirst; }
      const EditorDOMPointInText& LastPointRef() const { return mLast; }

      bool FoundNBSP() const {
        MOZ_ASSERT(mFirst.IsSet() == mLast.IsSet());
        return mFirst.IsSet();
      }

     private:
      EditorDOMPointInText mFirst;
      EditorDOMPointInText mLast;
    };

   public:
    TextFragmentData() = delete;

    /**
     * If aScanMode is Scan::EditableNodes and aPoint is in an editable node,
     * this scans only in the editing host.  Therefore, it's same as that
     * aAncestorLimiter is specified to the editing host.
     */
    template <typename EditorDOMPointType>
    TextFragmentData(Scan aScanMode, const EditorDOMPointType& aPoint,
                     BlockInlineCheck aBlockInlineCheck,
                     const Element* aAncestorLimiter = nullptr);

    bool IsInitialized() const {
      return mStart.Initialized() && mEnd.Initialized();
    }

    constexpr Scan ScanMode() const { return mScanMode; }

    constexpr BlockInlineCheck BlockInlineCheckMode() const {
      return mBlockInlineCheck;
    }

    nsIContent* GetStartReasonContent() const {
      return mStart.GetReasonContent();
    }
    nsIContent* GetEndReasonContent() const { return mEnd.GetReasonContent(); }

    bool StartsFromNonCollapsibleCharacters() const {
      return mStart.IsNonCollapsibleCharacters();
    }
    bool StartsFromSpecialContent() const { return mStart.IsSpecialContent(); }
    bool StartsFromBRElement() const { return mStart.IsBRElement(); }
    bool StartsFromVisibleBRElement() const {
      return StartsFromBRElement() &&
             HTMLEditUtils::IsVisibleBRElement(*GetStartReasonContent());
    }
    bool StartsFromInvisibleBRElement() const {
      return StartsFromBRElement() &&
             HTMLEditUtils::IsInvisibleBRElement(*GetStartReasonContent());
    }
    bool StartsFromPreformattedLineBreak() const {
      return mStart.IsPreformattedLineBreak();
    }
    bool StartsFromCurrentBlockBoundary() const {
      return mStart.IsCurrentBlockBoundary();
    }
    bool StartsFromOtherBlockElement() const {
      return mStart.IsOtherBlockBoundary();
    }
    bool StartsFromBlockBoundary() const { return mStart.IsBlockBoundary(); }
    bool StartsFromInlineEditingHostBoundary() const {
      return mStart.IsInlineEditingHostBoundary();
    }
    bool StartsFromHardLineBreak() const { return mStart.IsHardLineBreak(); }
    bool EndsByNonCollapsibleCharacters() const {
      return mEnd.IsNonCollapsibleCharacters();
    }
    bool EndsBySpecialContent() const { return mEnd.IsSpecialContent(); }
    bool EndsByBRElement() const { return mEnd.IsBRElement(); }
    bool EndsByVisibleBRElement() const {
      return EndsByBRElement() &&
             HTMLEditUtils::IsVisibleBRElement(*GetEndReasonContent());
    }
    bool EndsByInvisibleBRElement() const {
      return EndsByBRElement() &&
             HTMLEditUtils::IsInvisibleBRElement(*GetEndReasonContent());
    }
    bool EndsByPreformattedLineBreak() const {
      return mEnd.IsPreformattedLineBreak();
    }
    bool EndsByInvisiblePreformattedLineBreak() const {
      return mEnd.IsPreformattedLineBreak() &&
             HTMLEditUtils::IsInvisiblePreformattedNewLine(mEnd.PointRef());
    }
    bool EndsByCurrentBlockBoundary() const {
      return mEnd.IsCurrentBlockBoundary();
    }
    bool EndsByOtherBlockElement() const { return mEnd.IsOtherBlockBoundary(); }
    bool EndsByBlockBoundary() const { return mEnd.IsBlockBoundary(); }
    bool EndsByInlineEditingHostBoundary() const {
      return mEnd.IsInlineEditingHostBoundary();
    }

    WSType StartRawReason() const { return mStart.RawReason(); }
    WSType EndRawReason() const { return mEnd.RawReason(); }

    MOZ_NEVER_INLINE_DEBUG Element* StartReasonOtherBlockElementPtr() const {
      return mStart.OtherBlockElementPtr();
    }
    MOZ_NEVER_INLINE_DEBUG HTMLBRElement* StartReasonBRElementPtr() const {
      return mStart.BRElementPtr();
    }
    MOZ_NEVER_INLINE_DEBUG Element* EndReasonOtherBlockElementPtr() const {
      return mEnd.OtherBlockElementPtr();
    }
    MOZ_NEVER_INLINE_DEBUG HTMLBRElement* EndReasonBRElementPtr() const {
      return mEnd.BRElementPtr();
    }

    const EditorDOMPoint& StartRef() const { return mStart.PointRef(); }
    const EditorDOMPoint& EndRef() const { return mEnd.PointRef(); }

    const EditorDOMPoint& ScanStartRef() const { return mScanStartPoint; }

    bool FoundNoBreakingWhiteSpaces() const { return mNBSPData.FoundNBSP(); }
    const EditorDOMPointInText& FirstNBSPPointRef() const {
      return mNBSPData.FirstPointRef();
    }
    const EditorDOMPointInText& LastNBSPPointRef() const {
      return mNBSPData.LastPointRef();
    }

    /**
     * Return inclusive next point in inclusive next `Text` node from aPoint.
     * So, it may be in a collapsed white-space or invisible white-spaces.
     */
    template <typename EditorDOMPointType, typename PT, typename CT>
    [[nodiscard]] static EditorDOMPointType GetInclusiveNextCharPoint(
        const EditorDOMPointBase<PT, CT>& aPoint,
        BlockInlineCheck aBlockInlineCheck,
        IgnoreNonEditableNodes aIgnoreNonEditableNodes,
        const nsIContent* aFollowingLimiterContent = nullptr);

    template <typename EditorDOMPointType, typename PT, typename CT>
    [[nodiscard]] EditorDOMPointType GetInclusiveNextCharPoint(
        const EditorDOMPointBase<PT, CT>& aPoint,
        IgnoreNonEditableNodes aIgnoreNonEditableNodes) const {
      return GetInclusiveNextCharPoint<EditorDOMPointType>(
          aPoint, mBlockInlineCheck, aIgnoreNonEditableNodes,
          GetEndReasonContent());
    }

    /**
     * Return previous point in inclusive previous `Text` node from aPoint.
     * So, it may be in a collapsed white-space or invisible white-spaces.
     */
    template <typename EditorDOMPointType, typename PT, typename CT>
    [[nodiscard]] static EditorDOMPointType GetPreviousCharPoint(
        const EditorDOMPointBase<PT, CT>& aPoint,
        BlockInlineCheck aBlockInlineCheck,
        IgnoreNonEditableNodes aIgnoreNonEditableNodes,
        const nsIContent* aPrecedingLimiterContent = nullptr);

    template <typename EditorDOMPointType, typename PT, typename CT>
    [[nodiscard]] EditorDOMPointType GetPreviousCharPoint(
        const EditorDOMPointBase<PT, CT>& aPoint,
        IgnoreNonEditableNodes aIgnoreNonEditableNodes) const {
      return GetPreviousCharPoint<EditorDOMPointType>(aPoint, mBlockInlineCheck,
                                                      aIgnoreNonEditableNodes,
                                                      GetStartReasonContent());
    }

    /**
     * Return end of current collapsible ASCII white-spaces.
     *
     * @param aPointAtASCIIWhiteSpace   Must be in a sequence of collapsible
     *                                  ASCII white-spaces.
     * @param aDirectionToDelete        The direction to delete.
     */
    template <typename EditorDOMPointType>
    [[nodiscard]] static EditorDOMPointType GetEndOfCollapsibleASCIIWhiteSpaces(
        const EditorDOMPointInText& aPointAtASCIIWhiteSpace,
        nsIEditor::EDirection aDirectionToDelete,
        BlockInlineCheck aBlockInlineCheck,
        IgnoreNonEditableNodes aIgnoreNonEditableNodes,
        const nsIContent* aFollowingLimiterContent = nullptr);

    template <typename EditorDOMPointType>
    [[nodiscard]] EditorDOMPointType GetEndOfCollapsibleASCIIWhiteSpaces(
        const EditorDOMPointInText& aPointAtASCIIWhiteSpace,
        nsIEditor::EDirection aDirectionToDelete,
        IgnoreNonEditableNodes aIgnoreNonEditableNodes) const {
      return GetEndOfCollapsibleASCIIWhiteSpaces<EditorDOMPointType>(
          aPointAtASCIIWhiteSpace, aDirectionToDelete, mBlockInlineCheck,
          aIgnoreNonEditableNodes, GetEndReasonContent());
    }

    /**
     * Return start of current collapsible ASCII white-spaces.
     *
     * @param aPointAtASCIIWhiteSpace   Must be in a sequence of collapsible
     *                                  ASCII white-spaces.
     * @param aDirectionToDelete        The direction to delete.
     */
    template <typename EditorDOMPointType>
    [[nodiscard]] static EditorDOMPointType
    GetFirstASCIIWhiteSpacePointCollapsedTo(
        const EditorDOMPointInText& aPointAtASCIIWhiteSpace,
        nsIEditor::EDirection aDirectionToDelete,
        BlockInlineCheck aBlockInlineCheck,
        IgnoreNonEditableNodes aIgnoreNonEditableNodes,
        const nsIContent* aPrecedingLimiterContent = nullptr);

    template <typename EditorDOMPointType>
    [[nodiscard]] EditorDOMPointType GetFirstASCIIWhiteSpacePointCollapsedTo(
        const EditorDOMPointInText& aPointAtASCIIWhiteSpace,
        nsIEditor::EDirection aDirectionToDelete,
        IgnoreNonEditableNodes aIgnoreNonEditableNodes) const {
      return GetFirstASCIIWhiteSpacePointCollapsedTo<EditorDOMPointType>(
          aPointAtASCIIWhiteSpace, aDirectionToDelete, mBlockInlineCheck,
          aIgnoreNonEditableNodes, GetStartReasonContent());
    }

    /**
     * GetNonCollapsedRangeInTexts() returns non-empty range in texts which
     * is the largest range in aRange if there is some text nodes.
     */
    EditorDOMRangeInTexts GetNonCollapsedRangeInTexts(
        const EditorDOMRange& aRange) const;

    /**
     * InvisibleLeadingWhiteSpaceRangeRef() retruns reference to two DOM points,
     * start of the line and first visible point or end of the hard line.  When
     * this returns non-positioned range or positioned but collapsed range,
     * there is no invisible leading white-spaces.
     * Note that if there are only invisible white-spaces in a hard line,
     * this returns all of the white-spaces.
     */
    const EditorDOMRange& InvisibleLeadingWhiteSpaceRangeRef() const;

    /**
     * InvisibleTrailingWhiteSpaceRangeRef() returns reference to two DOM
     * points, first invisible white-space and end of the hard line.  When this
     * returns non-positioned range or positioned but collapsed range,
     * there is no invisible trailing white-spaces.
     * Note that if there are only invisible white-spaces in a hard line,
     * this returns all of the white-spaces.
     */
    const EditorDOMRange& InvisibleTrailingWhiteSpaceRangeRef() const;

    /**
     * GetNewInvisibleLeadingWhiteSpaceRangeIfSplittingAt() returns new
     * invisible leading white-space range which should be removed if
     * splitting invisible white-space sequence at aPointToSplit creates
     * new invisible leading white-spaces in the new line.
     * Note that the result may be collapsed range if the point is around
     * invisible white-spaces.
     */
    template <typename EditorDOMPointType>
    EditorDOMRange GetNewInvisibleLeadingWhiteSpaceRangeIfSplittingAt(
        const EditorDOMPointType& aPointToSplit) const {
      // If there are invisible trailing white-spaces and some or all of them
      // become invisible leading white-spaces in the new line, although we
      // don't need to delete them, but for aesthetically and backward
      // compatibility, we should remove them.
      const EditorDOMRange& trailingWhiteSpaceRange =
          InvisibleTrailingWhiteSpaceRangeRef();
      // XXX Why don't we check leading white-spaces too?
      if (!trailingWhiteSpaceRange.IsPositioned()) {
        return trailingWhiteSpaceRange;
      }
      // If the point is before the trailing white-spaces, the new line won't
      // start with leading white-spaces.
      if (aPointToSplit.IsBefore(trailingWhiteSpaceRange.StartRef())) {
        return EditorDOMRange();
      }
      // If the point is in the trailing white-spaces, the new line may
      // start with some leading white-spaces.  Returning collapsed range
      // is intentional because the caller may want to know whether the
      // point is in trailing white-spaces or not.
      if (aPointToSplit.EqualsOrIsBefore(trailingWhiteSpaceRange.EndRef())) {
        return EditorDOMRange(trailingWhiteSpaceRange.StartRef(),
                              aPointToSplit);
      }
      // Otherwise, if the point is after the trailing white-spaces, it may
      // be just outside of the text node.  E.g., end of parent element.
      // This is possible case but the validation cost is not worthwhile
      // due to the runtime cost in the worst case.  Therefore, we should just
      // return collapsed range at the end of trailing white-spaces.  Then,
      // callers can know the point is immediately after the trailing
      // white-spaces.
      return EditorDOMRange(trailingWhiteSpaceRange.EndRef());
    }

    /**
     * GetNewInvisibleTrailingWhiteSpaceRangeIfSplittingAt() returns new
     * invisible trailing white-space range which should be removed if
     * splitting invisible white-space sequence at aPointToSplit creates
     * new invisible trailing white-spaces in the new line.
     * Note that the result may be collapsed range if the point is around
     * invisible white-spaces.
     */
    template <typename EditorDOMPointType>
    EditorDOMRange GetNewInvisibleTrailingWhiteSpaceRangeIfSplittingAt(
        const EditorDOMPointType& aPointToSplit) const {
      // If there are invisible leading white-spaces and some or all of them
      // become end of current line, they will become visible.  Therefore, we
      // need to delete the invisible leading white-spaces before insertion
      // point.
      const EditorDOMRange& leadingWhiteSpaceRange =
          InvisibleLeadingWhiteSpaceRangeRef();
      if (!leadingWhiteSpaceRange.IsPositioned()) {
        return leadingWhiteSpaceRange;
      }
      // If the point equals or is after the leading white-spaces, the line
      // will end without trailing white-spaces.
      if (leadingWhiteSpaceRange.EndRef().IsBefore(aPointToSplit)) {
        return EditorDOMRange();
      }
      // If the point is in the leading white-spaces, the line may
      // end with some trailing white-spaces.  Returning collapsed range
      // is intentional because the caller may want to know whether the
      // point is in leading white-spaces or not.
      if (leadingWhiteSpaceRange.StartRef().EqualsOrIsBefore(aPointToSplit)) {
        return EditorDOMRange(aPointToSplit, leadingWhiteSpaceRange.EndRef());
      }
      // Otherwise, if the point is before the leading white-spaces, it may
      // be just outside of the text node.  E.g., start of parent element.
      // This is possible case but the validation cost is not worthwhile
      // due to the runtime cost in the worst case.  Therefore, we should
      // just return collapsed range at start of the leading white-spaces.
      // Then, callers can know the point is immediately before the leading
      // white-spaces.
      return EditorDOMRange(leadingWhiteSpaceRange.StartRef());
    }

    /**
     * FollowingContentMayBecomeFirstVisibleContent() returns true if some
     * content may be first visible content after removing content after aPoint.
     * Note that it's completely broken what this does.  Don't use this method
     * with new code.
     */
    template <typename EditorDOMPointType>
    bool FollowingContentMayBecomeFirstVisibleContent(
        const EditorDOMPointType& aPoint) const {
      MOZ_ASSERT(aPoint.IsSetAndValid());
      if (!mStart.IsHardLineBreak() && !mStart.IsInlineEditingHostBoundary()) {
        return false;
      }
      // If the point is before start of text fragment, that means that the
      // point may be at the block boundary or inline element boundary.
      if (aPoint.EqualsOrIsBefore(mStart.PointRef())) {
        return true;
      }
      // VisibleWhiteSpacesData is marked as start of line only when it
      // represents leading white-spaces.
      const EditorDOMRange& leadingWhiteSpaceRange =
          InvisibleLeadingWhiteSpaceRangeRef();
      if (!leadingWhiteSpaceRange.StartRef().IsSet()) {
        return false;
      }
      if (aPoint.EqualsOrIsBefore(leadingWhiteSpaceRange.StartRef())) {
        return true;
      }
      if (!leadingWhiteSpaceRange.EndRef().IsSet()) {
        return false;
      }
      return aPoint.EqualsOrIsBefore(leadingWhiteSpaceRange.EndRef());
    }

    /**
     * PrecedingContentMayBecomeInvisible() returns true if end of preceding
     * content is collapsed (when ends with an ASCII white-space).
     * Note that it's completely broken what this does.  Don't use this method
     * with new code.
     */
    template <typename EditorDOMPointType>
    bool PrecedingContentMayBecomeInvisible(
        const EditorDOMPointType& aPoint) const {
      MOZ_ASSERT(aPoint.IsSetAndValid());
      // If this fragment is ends by block boundary, always the caller needs
      // additional check.
      if (mEnd.IsBlockBoundary() || mEnd.IsInlineEditingHostBoundary()) {
        return true;
      }

      // If the point is in visible white-spaces and ends with an ASCII
      // white-space, it may be collapsed even if it won't be end of line.
      const VisibleWhiteSpacesData& visibleWhiteSpaces =
          VisibleWhiteSpacesDataRef();
      if (!visibleWhiteSpaces.IsInitialized()) {
        return false;
      }
      // XXX Odd case, but keep traditional behavior of `FindNearestRun()`.
      if (!visibleWhiteSpaces.StartRef().IsSet()) {
        return true;
      }
      if (!visibleWhiteSpaces.StartRef().EqualsOrIsBefore(aPoint)) {
        return false;
      }
      // XXX Odd case, but keep traditional behavior of `FindNearestRun()`.
      if (visibleWhiteSpaces.EndsByTrailingWhiteSpaces()) {
        return true;
      }
      // XXX Must be a bug.  This claims that the caller needs additional
      // check even when there is no white-spaces.
      if (visibleWhiteSpaces.StartRef() == visibleWhiteSpaces.EndRef()) {
        return true;
      }
      return aPoint.IsBefore(visibleWhiteSpaces.EndRef());
    }

    /**
     * GetPreviousNBSPPointIfNeedToReplaceWithASCIIWhiteSpace() may return an
     * NBSP point which should be replaced with an ASCII white-space when we're
     * inserting text into aPointToInsert. Note that this is a helper method for
     * the traditional white-space normalizer.  Don't use this with the new
     * white-space normalizer.
     * Must be called only when VisibleWhiteSpacesDataRef() returns initialized
     * instance and previous character of aPointToInsert is in the range.
     */
    EditorDOMPointInText GetPreviousNBSPPointIfNeedToReplaceWithASCIIWhiteSpace(
        const EditorDOMPoint& aPointToInsert) const;

    /**
     * GetInclusiveNextNBSPPointIfNeedToReplaceWithASCIIWhiteSpace() may return
     * an NBSP point which should be replaced with an ASCII white-space when
     * the caller inserts text into aPointToInsert.
     * Note that this is a helper method for the traditional white-space
     * normalizer.  Don't use this with the new white-space normalizer.
     * Must be called only when VisibleWhiteSpacesDataRef() returns initialized
     * instance, and inclusive next char of aPointToInsert is in the range.
     */
    EditorDOMPointInText
    GetInclusiveNextNBSPPointIfNeedToReplaceWithASCIIWhiteSpace(
        const EditorDOMPoint& aPointToInsert) const;

    /**
     * GetReplaceRangeDataAtEndOfDeletionRange() and
     * GetReplaceRangeDataAtStartOfDeletionRange() return delete range if
     * end or start of deleting range splits invisible trailing/leading
     * white-spaces and it may become visible, or return replace range if
     * end or start of deleting range splits visible white-spaces and it
     * causes some ASCII white-spaces become invisible unless replacing
     * with an NBSP.
     */
    ReplaceRangeData GetReplaceRangeDataAtEndOfDeletionRange(
        const TextFragmentData& aTextFragmentDataAtStartToDelete) const;
    ReplaceRangeData GetReplaceRangeDataAtStartOfDeletionRange(
        const TextFragmentData& aTextFragmentDataAtEndToDelete) const;

    /**
     * VisibleWhiteSpacesDataRef() returns reference to visible white-spaces
     * data. That is zero or more white-spaces which are visible.
     * Note that when there is no visible content, it's not initialized.
     * Otherwise, even if there is no white-spaces, it's initialized and
     * the range is collapsed in such case.
     */
    const VisibleWhiteSpacesData& VisibleWhiteSpacesDataRef() const;

   private:
    EditorDOMPoint mScanStartPoint;
    BoundaryData mStart;
    BoundaryData mEnd;
    NoBreakingSpaceData mNBSPData;
    mutable Maybe<EditorDOMRange> mLeadingWhiteSpaceRange;
    mutable Maybe<EditorDOMRange> mTrailingWhiteSpaceRange;
    mutable Maybe<VisibleWhiteSpacesData> mVisibleWhiteSpacesData;
    BlockInlineCheck mBlockInlineCheck;
    Scan mScanMode;
  };

  const TextFragmentData& TextFragmentDataAtStartRef() const {
    return mTextFragmentDataAtStart;
  }

  // The node passed to our constructor.
  EditorDOMPoint mScanStartPoint;
  // Together, the above represent the point at which we are building up ws
  // info.

 private:
  /**
   * ComputeRangeInTextNodesContainingInvisibleWhiteSpaces() returns range
   * containing invisible white-spaces if deleting between aStart and aEnd
   * causes them become visible.
   *
   * @param aStart      TextFragmentData at start of deleting range.
   *                    This must be initialized with DOM point in a text node.
   * @param aEnd        TextFragmentData at end of deleting range.
   *                    This must be initialized with DOM point in a text node.
   */
  static EditorDOMRangeInTexts
  ComputeRangeInTextNodesContainingInvisibleWhiteSpaces(
      const TextFragmentData& aStart, const TextFragmentData& aEnd);

  TextFragmentData mTextFragmentDataAtStart;

  const Scan mScanMode;

  friend class WhiteSpaceVisibilityKeeper;
  friend class WSScanResult;
};

}  // namespace mozilla

#endif  // #ifndef WSRunScanner_h
