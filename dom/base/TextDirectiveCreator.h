/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_TEXTDIRECTIVECREATOR_H_
#define DOM_TEXTDIRECTIVECREATOR_H_

#include <tuple>
#include "RangeBoundary.h"
#include "mozilla/dom/fragmentdirectives_ffi_generated.h"
#include "TextDirectiveUtil.h"
#include "nsStringFwd.h"
#include "mozilla/RefPtr.h"
#include "mozilla/Result.h"

class nsRange;

namespace mozilla {
class ErrorResult;
}

namespace mozilla::dom {
class Document;
/**
 * @brief Helper class to create a text directive string from a given `Range`.
 *
 * The class provides a public static creator function which encapsulates all
 * necessary logic.
 * This class serves as a base class that defines the main algorithm, and is
 * subclassed twice for exact and range-based matching.
 */
class TextDirectiveCreator {
 public:
  /**
   * @brief Static creator function. Takes a `Range` and creates a text
   *        directive string, if possible.
   *
   * @param aDocument   The document in which `aInputRange` lives.
   * @param aInputRange The input range. This range will not be modified.
   *
   * @return Returns a percent-encoded text directive string on success, an
   *         empty string if it's not possible to create a text fragment for the
   *         given range, or an error code.
   */
  static Result<nsCString, ErrorResult> CreateTextDirectiveFromRange(
      Document& aDocument, AbstractRange* aInputRange);

  virtual ~TextDirectiveCreator() = default;

 protected:
  TextDirectiveCreator(Document& aDocument, AbstractRange* aRange);

  /**
   * @brief Ensures the boundary points of the range point to word boundaries.
   *
   * This function always returns a new range.
   */
  static Result<RefPtr<AbstractRange>, ErrorResult> ExtendRangeToWordBoundaries(
      AbstractRange* aRange);

  /**
   * @brief Determines whether exact or range-based matching should be used.
   *
   * This function searches for a block boundary in `aRange`, which requires
   * range-based matching. If there is no block boundary, but the range content
   * is longer than a threshold, range-based matching is used as well.
   * This threshold is defined by the pref
   * `dom.text_fragments.create_text_fragment.exact_match_max_length`.
   *
   */
  static Result<bool, ErrorResult> MustUseRangeBasedMatching(
      AbstractRange* aRange);

  /**
   * @brief Creates an instance either for exact or range-based matching.
   */
  static Result<UniquePtr<TextDirectiveCreator>, ErrorResult> CreateInstance(
      Document& aDocument, AbstractRange* aRange);

  /**
   * @brief Collects text content surrounding the target range.
   *
   * The context terms are then stored both in normal and fold case form.
   */
  virtual Result<Ok, ErrorResult> CollectContextTerms() = 0;

  /**
   * @brief Common helper which collects the prefix term of the target range.
   */
  Result<Ok, ErrorResult> CollectPrefixContextTerm();

  /**
   * @brief Common helper which collects the suffix term of the target range.
   */
  Result<Ok, ErrorResult> CollectSuffixContextTerm();

  /**
   * @brief Collect the word begin / word end distances for the context terms.
   *
   * For start (for range-based matching) and suffix terms, the search direction
   * is left-to-right. Therefore, the distances are based off the beginning of
   * the context terms and use the word end boundary.
   *
   * For prefix and end (for range-based matching), the search direction is
   * right-to-left. Therefore, the distances are based off the end of the
   * context terms and use the word start boundary.
   *
   * The distances are always sorted, so that the first entry points to the
   * nearest word boundary in search direction.
   *
   * This method returns false if collecting context term word boundary
   * distances failed in a way that it's not considered a failure, but rather
   * it's not possible to create a text directive for the target range.
   * This can happen if the target range is too long for exact matching, but
   * does not contain a word boundary.
   */
  virtual bool CollectContextTermWordBoundaryDistances() = 0;

  /**
   * @brief Searches the document for other occurrences of the target range and
   *        converts the results into a comparable format.
   *
   * This method searches the partial document from the beginning up to the
   * target range for occurrences of the target range content.
   * This needs to be done differently based on whether matching is exact or
   * range-based. For exact matching, the whole text content of the target range
   * is searched for. For range-based matching, two search runs are required:
   * One for the minimal `start` term (ie., the first word), which ends at the
   * beginning of the target range. And one for the minimal `end` term (ie., the
   * last word), which starts at the beginning of the target range and ends
   * _before_ its end.
   * The resulting lists of matching ranges do not exclude the target range.
   */
  virtual Result<Ok, ErrorResult> FindAllMatchingCandidates() = 0;

  /**
   * @brief Find all occurrences of `aSearchQuery` in the partial document.
   *
   * This method uses `nsFind` to perform a case-insensitive search for
   * `aSearchQuery` in the partial document from `aSearchStart` to `aSearchEnd`.
   *
   * @return List of `Range`s which have the case-insensitive-same content as
   *         `aSearchQuery`.
   */
  Result<nsTArray<RefPtr<AbstractRange>>, ErrorResult> FindAllMatchingRanges(
      const nsString& aSearchQuery, const RangeBoundary& aSearchStart,
      const RangeBoundary& aSearchEnd);

  /**
   * @brief Creates the shortest possible text directive.
   *
   * @return A percent-encoded string containing a text directive. Returns empty
   *         string in cases where it's not possible to create a text directive.
   */
  Result<nsCString, ErrorResult> CreateTextDirective();

  /**
   * @brief Creates unique substring length arrays which are extended to the
   *        nearest word boundary.
   */
  static std::tuple<nsTArray<uint32_t>, nsTArray<uint32_t>>
  ExtendSubstringLengthsToWordBoundaries(
      const nsTArray<std::tuple<uint32_t, uint32_t>>& aExactSubstringLengths,
      const Span<const uint32_t>& aFirstWordPositions,
      const Span<const uint32_t>& aSecondWordPositions);

  /**
   * @brief Test all combinations to identify the shortest text directive.
   */
  virtual Maybe<TextDirective> FindShortestCombination() const = 0;

  /**
   * @brief Perform a brute-force optimization run to find the shortest
   *        combination of a combination of two context terms.
   *
   * Each combination of the extended values is compared against all exact
   * values. It is only considered valid if at least one value is longer than
   * the exact lengths.
   *
   * @param aExactWordLengths               Array of tuples containing the exact
   *                                        common sub string lengths of this
   *                                        combination.
   * @param aFirstExtendedToWordBoundaries  All valid substring lengths for the
   *                                        first context term, extended to its
   *                                        next word boundary in reading
   *                                        direction.
   * @param aSecondExtendedToWordBoundaries All valid substring lengths for the
   *                                        second context term, extended to its
   *                                        next word boundary in reading
   *                                        direction.
   * @return A tuple of sub string lengths extended to word boundaries, which is
   *         the shortest allowed combination to eliminate all matches.
   *         Returns `Nothing` if it's not possible to eliminate all matches.
   */
  static Maybe<std::tuple<uint32_t, uint32_t>> CheckAllCombinations(
      const nsTArray<std::tuple<uint32_t, uint32_t>>& aExactWordLengths,
      const nsTArray<uint32_t>& aFirstExtendedToWordBoundaries,
      const nsTArray<uint32_t>& aSecondExtendedToWordBoundaries);

  nsString mPrefixContent;
  nsString mPrefixFoldCaseContent;
  nsTArray<uint32_t> mPrefixWordBeginDistances;

  nsString mStartContent;
  nsString mStartFoldCaseContent;

  nsString mSuffixContent;
  nsString mSuffixFoldCaseContent;
  nsTArray<uint32_t> mSuffixWordEndDistances;

  Document& mDocument;
  RefPtr<AbstractRange> mRange;

  /**
   * The watchdog ensures that the algorithm exits after a defined time
   * duration, to ensure that the main thread is not blocked for too long.
   *
   * The duration is defined by the pref
   * `dom.text_fragments.create_text_fragment.timeout`.
   */
  TimeoutWatchdog mWatchdog;

  nsContentUtils::NodeIndexCache mNodeIndexCache;
};

/**
 * @brief Creator class which creates a range-based text directive.
 *
 */
class RangeBasedTextDirectiveCreator : public TextDirectiveCreator {
 private:
  using TextDirectiveCreator::TextDirectiveCreator;

  Result<Ok, ErrorResult> CollectContextTerms() override;

  bool CollectContextTermWordBoundaryDistances() override;

  Result<Ok, ErrorResult> FindAllMatchingCandidates() override;

  void FindStartMatchCommonSubstringLengths(
      const nsTArray<RefPtr<AbstractRange>>& aMatchRanges);

  void FindEndMatchCommonSubstringLengths(
      const nsTArray<RefPtr<AbstractRange>>& aMatchRanges);

  Maybe<TextDirective> FindShortestCombination() const override;

  nsString mEndContent;
  nsString mEndFoldCaseContent;

  nsTArray<uint32_t> mStartWordEndDistances;
  nsTArray<uint32_t> mEndWordBeginDistances;

  nsTArray<std::tuple<uint32_t, uint32_t>> mStartMatchCommonSubstringLengths;
  nsTArray<std::tuple<uint32_t, uint32_t>> mEndMatchCommonSubstringLengths;
};

/**
 * @brief Creator class which creates an exact match text directive.
 *
 */
class ExactMatchTextDirectiveCreator : public TextDirectiveCreator {
 private:
  using TextDirectiveCreator::TextDirectiveCreator;

  Result<Ok, ErrorResult> CollectContextTerms() override;

  bool CollectContextTermWordBoundaryDistances() override;

  Result<Ok, ErrorResult> FindAllMatchingCandidates() override;

  void FindCommonSubstringLengths(
      const nsTArray<RefPtr<AbstractRange>>& aMatchRanges);

  Maybe<TextDirective> FindShortestCombination() const override;

  nsTArray<std::tuple<uint32_t, uint32_t>> mCommonSubstringLengths;
};
}  // namespace mozilla::dom
#endif
