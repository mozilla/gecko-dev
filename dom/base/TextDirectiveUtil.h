/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_TEXTDIRECTIVEUTIL_H_
#define DOM_TEXTDIRECTIVEUTIL_H_

#include "mozilla/dom/Text.h"
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

  /** Advances the start of `aRange` to the next non-whitespace position.
   * The function follows this section of the spec:
   * https://wicg.github.io/scroll-to-text-fragment/#next-non-whitespace-position
   */
  static void AdvanceStartToNextNonWhitespacePosition(nsRange& aRange);

  /**
   * @brief Returns a point moved by one character or unicode surrogate pair.
   */
  static RangeBoundary MoveToNextBoundaryPoint(const RangeBoundary& aPoint);

  template <TextScanDirection direction>
  static RangeBoundary FindNextBlockBoundary(
      const RangeBoundary& aRangeBoundary);
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

/**
 * @brief Iterator for visible text nodes with the same block ancestor.
 *
 * Allows to be used in range-based iteration. Returns the next visible text
 * node (as defined by `TextDirectiveUtil::NodeIsVisibleTextNode()` and
 * `TextDirectiveUtil::NodeIsPartOfNonSearchableSubTree()`) in the given
 * direction.
 *
 * @tparam direction Either left-to-right or right-to-left.
 */
template <TextScanDirection direction>
class SameBlockVisibleTextNodeIterator final {
 public:
  explicit SameBlockVisibleTextNodeIterator(nsINode& aStart)
      : mCurrent(&aStart),
        mBlockAncestor(TextDirectiveUtil::GetBlockAncestorForNode(mCurrent)) {
    while (mCurrent->HasChildNodes()) {
      nsINode* child = direction == TextScanDirection::Left
                           ? mCurrent->GetLastChild()
                           : mCurrent->GetFirstChild();
      if (TextDirectiveUtil::GetBlockAncestorForNode(child) != mBlockAncestor) {
        break;
      }
      mCurrent = child;
    }
  }

  SameBlockVisibleTextNodeIterator& begin() { return *this; }

  std::nullptr_t end() { return nullptr; }

  bool operator!=(std::nullptr_t) const { return !!mCurrent; }

  void operator++() {
    while (mCurrent) {
      mCurrent = direction == TextScanDirection::Left ? mCurrent->GetPrevNode()
                                                      : mCurrent->GetNextNode();
      if (!mCurrent) {
        return;
      }
      if (TextDirectiveUtil::GetBlockAncestorForNode(mCurrent) !=
          mBlockAncestor) {
        mCurrent = nullptr;
        return;
      }
      if (TextDirectiveUtil::NodeIsVisibleTextNode(*mCurrent) &&
          !TextDirectiveUtil::NodeIsPartOfNonSearchableSubTree(*mCurrent)) {
        break;
      }
    }
    MOZ_ASSERT_IF(mCurrent, mCurrent->IsText());
  }

  Text* operator*() { return Text::FromNodeOrNull(mCurrent); }

 private:
  nsINode* mCurrent = nullptr;
  nsINode* mBlockAncestor = nullptr;
};

template <TextScanDirection direction>
/*static*/ RangeBoundary TextDirectiveUtil::FindNextBlockBoundary(
    const RangeBoundary& aRangeBoundary) {
  MOZ_ASSERT(aRangeBoundary.IsSetAndValid());
  nsINode* current = aRangeBoundary.GetContainer();
  uint32_t offset =
      direction == TextScanDirection::Left ? 0u : current->Length();
  for (auto* node : SameBlockVisibleTextNodeIterator<direction>(*current)) {
    if (!node) {
      continue;
    }
    current = node;
    offset = direction == TextScanDirection::Left ? 0u : current->Length();
  }
  return {current, offset};
}

}  // namespace mozilla::dom

#endif
