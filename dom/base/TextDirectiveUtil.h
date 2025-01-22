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
#include "nsStringFwd.h"

class nsIURI;
class nsINode;
class nsRange;
struct TextDirective;

namespace mozilla::dom {

extern LazyLogModule sFragmentDirectiveLog;
#define TEXT_FRAGMENT_LOG_FN(msg, func, ...)      \
  MOZ_LOG(sFragmentDirectiveLog, LogLevel::Debug, \
          ("%s(): " msg, func, ##__VA_ARGS__))

// Shortcut macro for logging, which includes the current function name.
// To customize (eg. if in a lambda), use `TEXT_FRAGMENT_LOG_FN`.
#define TEXT_FRAGMENT_LOG(msg, ...) \
  TEXT_FRAGMENT_LOG_FN(msg, __FUNCTION__, ##__VA_ARGS__)

enum class TextScanDirection { Left = -1, Right = 1 };

class TextDirectiveUtil final {
 public:
  MOZ_ALWAYS_INLINE static bool ShouldLog() {
    return MOZ_LOG_TEST(sFragmentDirectiveLog, LogLevel::Debug);
  }
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
  static RefPtr<nsRange> FindStringInRange(nsRange* aSearchRange,
                                           const nsAString& aQuery,
                                           bool aWordStartBounded,
                                           bool aWordEndBounded);

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
};
}  // namespace mozilla::dom

#endif
