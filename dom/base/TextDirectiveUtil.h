/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_TEXTDIRECTIVEUTIL_H_
#define DOM_TEXTDIRECTIVEUTIL_H_

#include "mozilla/Logging.h"
#include "mozilla/RangeBoundary.h"
#include "mozilla/RefPtr.h"
#include "mozilla/TimeStamp.h"
#include "nsStringFwd.h"

class nsIURI;
class nsINode;
class nsRange;
struct TextDirective;

namespace mozilla::dom {

extern LazyLogModule gFragmentDirectiveLog;
#define TEXT_FRAGMENT_LOG_FN(msg, func, ...)                              \
  MOZ_LOG_FMT(gFragmentDirectiveLog, LogLevel::Debug, "{}(): " msg, func, \
              ##__VA_ARGS__)

// Shortcut macro for logging, which includes the current function name.
// To customize (eg. if in a lambda), use `TEXT_FRAGMENT_LOG_FN`.
#define TEXT_FRAGMENT_LOG(msg, ...) \
  TEXT_FRAGMENT_LOG_FN(msg, __FUNCTION__, ##__VA_ARGS__)

enum class TextScanDirection { Left = -1, Right = 1 };

class TextDirectiveUtil final {
 public:
  MOZ_ALWAYS_INLINE static bool ShouldLog() {
    return MOZ_LOG_TEST(gFragmentDirectiveLog, LogLevel::Debug);
  }

  static Result<nsString, ErrorResult> RangeContentAsString(nsRange* aRange);

  static Result<nsString, ErrorResult> RangeContentAsFoldCase(nsRange* aRange);

  /**
   * @brief Return true if `aNode` is a visible Text node.
   *
   * A node is a visible text node if it is a Text node, the computed value of
   * its parent element's visibility property is visible, and it is being
   * rendered.
   *
   * see https://wicg.github.io/scroll-to-text-fragment/#visible-text-node
   */
  static bool NodeIsVisibleTextNode(const nsINode& aNode);

  /**
   * @brief Finds the search query in the given search range.
   *
   * This is a thin wrapper around `nsFind`.
   */
  static RefPtr<nsRange> FindStringInRange(
      const RangeBoundary& aSearchStart, const RangeBoundary& aSearchEnd,
      const nsAString& aQuery, bool aWordStartBounded, bool aWordEndBounded,
      nsContentUtils::NodeIndexCache* aCache = nullptr);

  /**
   * @brief Moves `aRangeBoundary` one word in `aDirection`.
   *
   * Word boundaries are determined using `intl::WordBreaker::FindWord()`.
   *
   *
   * @param aRangeBoundary[in] The range boundary that should be moved.
   *                           Must be set and valid.
   * @param aDirection[in]     The direction into which to move.
   * @return A new `RangeBoundary` which is moved to the next word.
   */
  static RangeBoundary MoveRangeBoundaryOneWord(
      const RangeBoundary& aRangeBoundary, TextScanDirection aDirection);

  /**
   * @brief Tests if there is whitespace at the given position.
   *
   * This algorithm tests for whitespaces and `&nbsp;` at `aPos`.
   * It returns true if whitespace was found.
   *
   * This function assumes the reading direction is "right". If trying to check
   * for whitespace to the left, the caller must adjust the offset.
   *
   */
  static bool IsWhitespaceAtPosition(const Text* aText, uint32_t aPos);

  /**
   * @brief Determine if `aNode` should be considered when traversing the DOM.
   *
   * A node is "search invisible" if it is an element in the HTML namespace and
   *  1. The computed value of its `display` property is `none`
   *  2. It serializes as void
   *  3. It is one of the following types:
   *    - HTMLIFrameElement
   *    - HTMLImageElement
   *    - HTMLMeterElement
   *    - HTMLObjectElement
   *    - HTMLProgressElement
   *    - HTMLStyleElement
   *    - HTMLScriptElement
   *    - HTMLVideoElement
   *    - HTMLAudioElement
   *  4. It is a `select` element whose `multiple` content attribute is absent
   *
   * see https://wicg.github.io/scroll-to-text-fragment/#search-invisible
   */
  static bool NodeIsSearchInvisible(nsINode& aNode);

  /**
   * @brief Returns true if `aNode` has block-level display.
   * A node has block-level display if it is an element and the computed value
   * of its display property is any of
   *  - block
   *  - table
   *  - flow-root
   *  - grid
   *  - flex
   *  - list-item
   *
   * See https://wicg.github.io/scroll-to-text-fragment/#has-block-level-display
   */
  static bool NodeHasBlockLevelDisplay(nsINode& aNode);
  /**
   * @brief Get the Block Ancestor For `aNode`.
   *
   * see https://wicg.github.io/scroll-to-text-fragment/#nearest-block-ancestor
   */
  static nsINode* GetBlockAncestorForNode(nsINode* aNode);

  /**
   * @brief Returns true if `aNode` is part of a non-searchable subtree.
   *
   * A node is part of a non-searchable subtree if it is or has a
   * shadow-including ancestor that is search invisible.
   *
   * see https://wicg.github.io/scroll-to-text-fragment/#non-searchable-subtree
   */
  static bool NodeIsPartOfNonSearchableSubTree(nsINode& aNode);
  /**
   * @brief Convenience function that returns true if the given position in a
   * string is a word boundary.
   *
   * This is a thin wrapper around the `WordBreaker::FindWord()` function.
   *
   * @param aText The text input.
   * @param aPosition The position to check.
   * @return true if there is a word boundary at `aPosition`.
   * @return false otherwise.
   */
  static bool IsAtWordBoundary(const nsAString& aText, uint32_t aPosition);

  enum class IsEndIndex : bool { No, Yes };
  static RangeBoundary GetBoundaryPointAtIndex(
      uint32_t aIndex, const nsTArray<RefPtr<Text>>& aTextNodeList,
      IsEndIndex aIsEndIndex);

  /** Advances the start of `aRange` to the next non-whitespace position.
   * The function follows this section of the spec:
   * https://wicg.github.io/scroll-to-text-fragment/#next-non-whitespace-position
   */
  static void AdvanceStartToNextNonWhitespacePosition(nsRange& aRange);

  static RangeBoundary MoveBoundaryToNextNonWhitespacePosition(
      const RangeBoundary& aRangeBoundary);
  static RangeBoundary MoveBoundaryToPreviousNonWhitespacePosition(
      const RangeBoundary& aRangeBoundary);

  static Result<Maybe<RangeBoundary>, ErrorResult> FindBlockBoundaryInRange(
      const nsRange& aRange, TextScanDirection aDirection);

  static Result<RangeBoundary, ErrorResult> FindNextBlockBoundary(
      const RangeBoundary& aRangeBoundary, TextScanDirection aDirection);

  /**
   * @brief Compares two range boundaries whether they are "normalized equal".
   *
   * Range boundaries are "normalized equal" if there is no visible text between
   * them, for example here (range boundaries represented by `|`):
   *
   * ```html
   * <span>foo |<p>|bar</p></span>
   * ```
   *
   * In this case, comparing the boundaries for equality would return false.
   * But, when calling this function, they would be considered normalized equal.
   *
   * @return true if the boundaries are normalized equal.
   */
  static bool NormalizedRangeBoundariesAreEqual(
      const RangeBoundary& aRangeBoundary1,
      const RangeBoundary& aRangeBoundary2,
      nsContentUtils::NodeIndexCache* aCache = nullptr);

  /**
   * @brief Extends the range boundaries to word boundaries across nodes.
   *
   * @param[inout] aRange The range. Changes to the range are done in-place.
   *
   * @return Returns an error value if something failed along the way.
   */
  static Result<Ok, ErrorResult> ExtendRangeToWordBoundaries(nsRange& aRange);

  /**
   * @brief Create a `TextDirective` From `nsRange`s representing the context
   *        terms.
   *
   * Every parameter besides `aStart` is allowed to be nullptr or a collapsed
   * range. Ranges are converted to strings using their `ToString()` method.
   * Whitespace is compressed.
   *
   * @return The created `TextDirective`, or an error if converting the ranges
   *         to string fails.
   */
  static Result<TextDirective, ErrorResult> CreateTextDirectiveFromRanges(
      nsRange* aPrefix, nsRange* aStart, nsRange* aEnd, nsRange* aSuffix);

  /**
   * Find the length of the common prefix between two folded strings.
   *
   * @return The length of the common prefix.
   */
  static uint32_t FindCommonPrefix(const nsAString& aFoldedStr1,
                                   const nsAString& aFoldedStr2);

  /**
   * Find the length of the common suffix between two folded strings.
   *
   * @return The length of the common suffix.
   */
  static uint32_t FindCommonSuffix(const nsAString& aFoldedStr1,
                                   const nsAString& aFoldedStr2);

  /**
   * Map a logical offset to a container node and offset within the DOM.
   *
   * @param aRange         The nsRange to map the offset from.
   * @param aLogicalOffset The logical offset in the flattened text content of
   *                       the range. The offset is always starting at the start
   *                       of the range.
   * @return a `RangeBoundary` that represents the logical offset, or an error.
   */
  static RangeBoundary CreateRangeBoundaryByMovingOffsetFromRangeStart(
      nsRange* aRange, uint32_t aLogicalOffset);
};

class TimeoutWatchdog final {
 public:
  TimeoutWatchdog()
      : mStartTime(TimeStamp::Now()),
        mDuration(TimeDuration::FromSeconds(
            StaticPrefs::
                dom_text_fragments_create_text_fragment_timeout_seconds())) {}
  bool IsDone() const { return TimeStamp::Now() - mStartTime > mDuration; }

 private:
  TimeStamp mStartTime;
  TimeDuration mDuration;
};
}  // namespace mozilla::dom

#endif
