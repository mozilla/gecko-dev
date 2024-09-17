/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_TextLeafRange_h__
#define mozilla_a11y_TextLeafRange_h__

#include <stdint.h>

#include "AccAttributes.h"
#include "nsDirection.h"
#include "nsIAccessibleText.h"

namespace mozilla {
namespace dom {
class AbstractRange;
class Document;
}  // namespace dom

namespace a11y {
class Accessible;
class LocalAccessible;

/**
 * Represents a point within accessible text.
 * This is stored as a leaf Accessible and an offset into that Accessible.
 * For an empty Accessible, the offset will always be 0.
 * This will eventually replace TextPoint. Unlike TextPoint, this does not
 * use HyperTextAccessible offsets.
 */
class TextLeafPoint final {
 public:
  TextLeafPoint(Accessible* aAcc, int32_t aOffset);

  /**
   * Constructs an invalid TextPoint (mAcc is null).
   * A TextLeafPoint in this state will evaluate to false.
   * mAcc can be set later. Alternatively, this can be used to indicate an error
   * (e.g. if a requested point couldn't be found).
   */
  TextLeafPoint() : mAcc(nullptr), mOffset(0) {}

  /**
   * Construct a TextLeafPoint representing the caret.
   */
  static TextLeafPoint GetCaret(Accessible* aAcc);

  Accessible* mAcc;
  int32_t mOffset;

  /**
   * True if this point is the insertion point at the end of a line. This is the
   * point where the caret is positioned when pressing the end key, for example.
   * On the very last line, mOffset will be equal to the length of the text.
   * However, where text wraps across lines, this line end insertion point
   * doesn't have its own offset, so mOffset will be the offset for the first
   * character on the next line. This is where this flag becomes important.
   * Otherwise, for example, commanding a screen reader to read the current line
   * would read the next line instead of the current line in this case.
   */
  bool mIsEndOfLineInsertionPoint = false;

  bool operator==(const TextLeafPoint& aPoint) const {
    return mAcc == aPoint.mAcc && mOffset == aPoint.mOffset;
  }

  bool operator!=(const TextLeafPoint& aPoint) const {
    return !(*this == aPoint);
  }

  bool operator<(const TextLeafPoint& aPoint) const;

  bool operator<=(const TextLeafPoint& aPoint) const;

  /**
   * A valid TextLeafPoint evaluates to true. An invalid TextLeafPoint
   * evaluates to false.
   */
  explicit operator bool() const { return !!mAcc; }

  enum class BoundaryFlags : uint32_t {
    eDefaultBoundaryFlags = 0,
    // Return point unchanged if it is at the given boundary type.
    eIncludeOrigin = 1 << 0,
    // If current point is in editable, return point within samme editable.
    eStopInEditable = 1 << 1,
    // Skip over list items in searches and don't consider them line or
    // paragraph starts.
    eIgnoreListItemMarker = 1 << 2,
  };

  /**
   * Find a boundary (word start, line start, etc.) in a specific direction.
   * If no boundary is found, the start/end of the document is returned
   * (depending on the direction).
   */
  TextLeafPoint FindBoundary(
      AccessibleTextBoundary aBoundaryType, nsDirection aDirection,
      BoundaryFlags aFlags = BoundaryFlags::eDefaultBoundaryFlags) const;

  /**
   * These two functions find a line start boundary within the same
   * LocalAccessible as this. That is, they do not cross Accessibles. If no
   * boundary is found, an invalid TextLeafPoint is returned.
   * These are used by FindBoundary. Most callers will want FindBoundary
   * instead.
   */
  TextLeafPoint FindPrevLineStartSameLocalAcc(bool aIncludeOrigin) const;
  TextLeafPoint FindNextLineStartSameLocalAcc(bool aIncludeOrigin) const;

  /**
   * These two functions find a word start boundary within the same
   * Accessible as this. That is, they do not cross Accessibles. If no
   * boundary is found, an invalid TextLeafPoint is returned.
   * These are used by FindBoundary. Most callers will want FindBoundary
   * instead.
   */
  TextLeafPoint FindPrevWordStartSameAcc(bool aIncludeOrigin) const;
  TextLeafPoint FindNextWordStartSameAcc(bool aIncludeOrigin) const;

  /**
   * Get the text attributes at this point.
   * If aIncludeDefaults is true, default attributes on the HyperTextAccessible
   * will be included.
   */
  already_AddRefed<AccAttributes> GetTextAttributes(
      bool aIncludeDefaults = true) const;

  /**
   * Get Get the text attributes at this point in a LocalAccessible.
   * This is used by GetTextAttributes. Most callers will want GetTextAttributes
   * instead.
   */
  already_AddRefed<AccAttributes> GetTextAttributesLocalAcc(
      bool aIncludeDefaults = true) const;

  /**
   * Get all the attributes that apply to offset ranges in a given text leaf
   * LocalAccessible. This should only be used when pushing the cache. Most
   * callers will want FindTextAttrsStart instead.
   */
  static nsTArray<TextOffsetAttribute> GetTextOffsetAttributes(
      LocalAccessible* aAcc);

  /**
   * Queue a cache update for text offset attributes in a given DOM range.
   */
  static void UpdateCachedTextOffsetAttributes(
      dom::Document* aDocument, const dom::AbstractRange& aRange);

  /**
   * Find the start of a run of text attributes in a specific direction.
   * A text attributes run is a span of text where the attributes are the same.
   * If no boundary is found, the start/end of the container is returned
   * (depending on the direction).
   * If aIncludeorigin is true and this is at a boundary, this will be
   * returned unchanged.
   */
  TextLeafPoint FindTextAttrsStart(nsDirection aDirection,
                                   bool aIncludeOrigin = false) const;

  /**
   * Returns a rect (in dev pixels) describing position and size of
   * the character at mOffset in mAcc. This rect is screen-relative.
   * This function only works on remote accessibles, and assumes caching
   * is enabled.
   */
  LayoutDeviceIntRect CharBounds();

  /**
   * Returns true if the given point (in screen coords) is contained
   * in the char bounds of the current TextLeafPoint. Returns false otherwise.
   * If the current point is an empty container, we use the acc's bounds instead
   * of char bounds. Because this depends on CharBounds, this function only
   * works on remote accessibles, and assumes caching is enabled.
   */
  bool ContainsPoint(int32_t aX, int32_t aY);

  bool IsLineFeedChar() const { return GetChar() == '\n'; }

  bool IsSpace() const;

  bool IsParagraphStart(bool aIgnoreListItemMarker = false) const {
    return mOffset == 0 &&
           FindParagraphSameAcc(eDirPrevious, true, aIgnoreListItemMarker);
  }

  /**
   * Translate given TextLeafPoint into a DOM point.
   */
  MOZ_CAN_RUN_SCRIPT std::pair<nsIContent*, int32_t> ToDOMPoint(
      bool aIncludeGenerated = true) const;

 private:
  /**
   * If this is the insertion point at the end of a line, return an adjusted
   * point such that word and line boundaries can be calculated correctly.
   */
  TextLeafPoint AdjustEndOfLine() const;

  bool IsEmptyLastLine() const;

  bool IsDocEdge(nsDirection aDirection) const;

  bool IsLeafAfterListItemMarker() const;

  char16_t GetChar() const;

  TextLeafPoint FindLineStartSameRemoteAcc(nsDirection aDirection,
                                           bool aIncludeOrigin) const;

  /**
   * Helper which just calls the appropriate function based on whether mAcc
   *is local or remote.
   */
  TextLeafPoint FindLineStartSameAcc(nsDirection aDirection,
                                     bool aIncludeOrigin,
                                     bool aIgnoreListItemMarker = false) const;

  TextLeafPoint FindLineEnd(nsDirection aDirection, BoundaryFlags aFlags) const;
  TextLeafPoint FindWordEnd(nsDirection aDirection, BoundaryFlags aFlags) const;

  TextLeafPoint FindParagraphSameAcc(nsDirection aDirection,
                                     bool aIncludeOrigin,
                                     bool aIgnoreListItemMarker = false) const;

  TextLeafPoint FindClusterSameAcc(nsDirection aDirection,
                                   bool aIncludeOrigin) const;

  void AddTextOffsetAttributes(AccAttributes* aAttrs) const;

  /**
   * Find a text offset attribute boundary in the same Accessible. This function
   * searches for either start or end points, since either means a change in
   * text attributes. This only considers attributes such as spelling errors
   * which are mapped to DOM selections. Most callers will want
   * FindTextAttrsStart instead.
   */
  TextLeafPoint FindTextOffsetAttributeSameAcc(nsDirection aDirection,
                                               bool aIncludeOrigin) const;

  // Return the point immediately succeeding or preceding this leaf depending
  // on given direction.
  TextLeafPoint NeighborLeafPoint(nsDirection aDirection, bool aIsEditable,
                                  bool aIgnoreListItemMarker) const;

  /**
   * This function assumes mAcc is a LocalAccessible.
   * It iterates the continuations of mAcc's primary frame until it locates
   * the continuation containing mOffset (a rendered offset). It then uses
   * GetScreenRectInAppUnits to compute screen coords for the frame, resizing
   * such that the resulting rect contains only one character.
   */
  LayoutDeviceIntRect ComputeBoundsFromFrame() const;
};

MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS(TextLeafPoint::BoundaryFlags)

/**
 * Represents a range of accessible text.
 * This will eventually replace TextRange.
 */
class TextLeafRange final {
 public:
  TextLeafRange(const TextLeafPoint& aStart, const TextLeafPoint& aEnd)
      : mStart(aStart), mEnd(aEnd) {}
  explicit TextLeafRange(const TextLeafPoint& aStart)
      : mStart(aStart), mEnd(aStart) {}
  explicit TextLeafRange() {}

  // Create a TextLeafRange spanning the entire leaf.
  static TextLeafRange FromAccessible(Accessible* aAcc) {
    return {{aAcc, 0}, {aAcc, nsIAccessibleText::TEXT_OFFSET_END_OF_TEXT}};
  }

  /**
   * A valid TextLeafRange evaluates to true. An invalid TextLeafRange
   * evaluates to false.
   */
  explicit operator bool() const { return !!mStart && !!mEnd; }

  bool operator!=(const TextLeafRange& aOther) const {
    return mEnd != aOther.mEnd || mStart != aOther.mStart;
  }

  bool operator==(const TextLeafRange& aOther) const {
    return mEnd == aOther.mEnd && mStart == aOther.mStart;
  }

  TextLeafPoint Start() const { return mStart; }
  void SetStart(const TextLeafPoint& aStart) { mStart = aStart; }
  TextLeafPoint End() const { return mEnd; }
  void SetEnd(const TextLeafPoint& aEnd) { mEnd = aEnd; }

  bool Crop(Accessible* aContainer);

  /**
   * Returns a union rect (in dev pixels) of all character bounds in this range.
   * This rect is screen-relative and inclusive of mEnd. This function only
   * works on remote accessibles, and assumes caching is enabled.
   */
  LayoutDeviceIntRect Bounds() const;

  /*
   * Returns a TextLeafPoint corresponding to the point in the TextLeafRange
   * containing the given screen point. The function returns a TextLeafPoint
   * constructed from mStart if it does not find a containing character.
   */
  TextLeafPoint TextLeafPointAtScreenPoint(int32_t aX, int32_t aY) const;

  /**
   * Get the ranges of text that are selected within this Accessible. The caret
   * is not included as a collapsed range.
   */
  static void GetSelection(Accessible* aAcc, nsTArray<TextLeafRange>& aRanges);

  /**
   * Set range as DOM selection.
   * aSelectionNum is the selection index to use. If aSelectionNum is
   * out of bounds for current selection ranges, or is -1, a new selection
   * range is created.
   */
  MOZ_CAN_RUN_SCRIPT bool SetSelection(int32_t aSelectionNum) const;

  MOZ_CAN_RUN_SCRIPT void ScrollIntoView(uint32_t aScrollType) const;

 private:
  TextLeafPoint mStart;
  TextLeafPoint mEnd;

 public:
  /**
   * A TextLeafRange iterator will iterate through single leaf segments of the
   * given range.
   */

  class Iterator {
   public:
    Iterator(Iterator&& aOther)
        : mRange(aOther.mRange),
          mSegmentStart(aOther.mSegmentStart),
          mSegmentEnd(aOther.mSegmentEnd) {}

    static Iterator BeginIterator(const TextLeafRange& aRange);

    static Iterator EndIterator(const TextLeafRange& aRange);

    Iterator& operator++();

    bool operator!=(const Iterator& aOther) const {
      return mRange != aOther.mRange || mSegmentStart != aOther.mSegmentStart ||
             mSegmentEnd != aOther.mSegmentEnd;
    }

    TextLeafRange operator*() {
      return TextLeafRange(mSegmentStart, mSegmentEnd);
    }

   private:
    explicit Iterator(const TextLeafRange& aRange) : mRange(aRange) {}

    Iterator() = delete;
    Iterator(const Iterator&) = delete;
    Iterator& operator=(const Iterator&) = delete;
    Iterator& operator=(const Iterator&&) = delete;

    const TextLeafRange& mRange;
    TextLeafPoint mSegmentStart;
    TextLeafPoint mSegmentEnd;
  };

  Iterator begin() const { return Iterator::BeginIterator(*this); }
  Iterator end() const { return Iterator::EndIterator(*this); }
};

}  // namespace a11y
}  // namespace mozilla

#endif
