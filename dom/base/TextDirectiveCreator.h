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
 * @brief Caches fold case representations of `nsRange`s.
 *
 * This class is a specialization for a hashmap using `nsRange*` as key and
 * `UniquePtr<nsString>` as value.
 * The class only exposes the strings as raw pointers, which are valid as long
 * as this cache is alive. The addresses will not change and the pointers can
 * safely be stored, as long as the lifetime of this cache is greater.
 *
 */
class RangeContentCache final {
 public:
  /**
   * @brief Get the fold case representation of `aRange`.
   *
   * This method uses a maximum of two hashtable lookups to either return a
   * cached fold case representation of `aRange`s content, or to create one.
   * The returned string pointer is alive as long as the cache is alive.
   * Returns `nullptr` if `aRange` is `nullptr`, and propagates the error of
   * converting the range content into fold case.
   */
  Result<const nsString*, ErrorResult> GetOrCreate(nsRange* aRange);

 private:
  nsTHashMap<nsRange*, UniquePtr<nsString>> mCache;
};

/**
 * @brief Helper which represents a potential text directive using `nsRange`s.
 *
 * In addition to the _current_ context terms of the text directive, it also
 * contains the _fully expanded_ context terms, i.e. the ranges until the next
 * block boundary.
 *
 * `TextDirectiveCandidate`s are immutable.
 * This allows sharing `nsRange`s across instances to save memory. Also, this
 * allows to create the text directive string representation
 * (`TextDirectiveString()`) once when the object is created.
 * However, the `nsRange` members cannot be marked as `const`:
 * - It's not possible to have the `nsRange` itself be const because
 *   `nsRange::ToString()` is not const.
 * - It's not possible to have the `RefPtr` be const because of move semantics.
 */
class TextDirectiveCandidate {
 public:
  TextDirectiveCandidate(TextDirectiveCandidate&&) = default;
  TextDirectiveCandidate& operator=(TextDirectiveCandidate&&) = default;

 private:
  TextDirectiveCandidate(const TextDirectiveCandidate&) = default;
  TextDirectiveCandidate& operator=(const TextDirectiveCandidate&) = default;

 public:
  /**
   * @brief Creates a candidate from a given input range.
   *
   * This function determines whether the candidate needs to use exact or
   * range-based matching based on whether the input range contains a block
   * boundary.
   * Then, it determines the fully-expanded ranges for all context terms and
   * creates an instance.
   *
   * @param aInputRange The input range.
   * @param aRangeContentCache A cache which stores fold case representations of
   *                           the ranges used to compare ranges for equality.
   * @return A text directive candidate, or an error.
   */
  static Result<TextDirectiveCandidate, ErrorResult> CreateFromInputRange(
      const nsRange* aInputRange, RangeContentCache& aRangeContentCache);

  static Result<TextDirectiveCandidate, ErrorResult> CreateFromStartAndEndRange(
      const nsRange* aStartRange, const nsRange* aEndRange,
      RangeContentCache& aRangeContentCache);

  /**
   * Creates new text directive candidates for each element of `aMatches`, which
   * eliminate the element.
   *
   * @see CreateNewCandidatesForGivenMatch()
   */
  Result<nsTArray<TextDirectiveCandidate>, ErrorResult>
  CreateNewCandidatesForMatches(
      const nsTArray<const TextDirectiveCandidate*>& aMatches,
      RangeContentCache& aRangeContentCache);

  /**
   * Creates new text directive candidates which eliminate `aOther` by extending
   * context terms in every direction.
   *
   * If exact matching is used, this function will create up to two new
   * candidates, one where the prefix is extended until it is not matching with
   * other, one where suffix is extended.
   * If range based matching is used, there will be additional candidates
   * created which extend the start and end term.
   *
   * If even a fully expanded context term is matching the context term of
   * `other`, no candidate is created.
   * Returning an empty array indicates that it is not possible to create a text
   * directive for the given text in the current document, because it is not
   * possible to create a text directive that would not match `other`.
   */
  Result<nsTArray<TextDirectiveCandidate>, ErrorResult>
  CreateNewCandidatesForGivenMatch(const TextDirectiveCandidate& aOther,
                                   RangeContentCache& aRangeContentCache) const;

  /**
   * Clones `this` and replaces ranges which are non-null.
   * The parameter ranges are moved into the function to emphasize that the
   * objects are not cloned. Therefore, the ranges must not be updated after
   * this call.
   */
  Result<TextDirectiveCandidate, ErrorResult> CloneWith(
      RefPtr<nsRange>&& aNewPrefixRange, RefPtr<nsRange>&& aNewStartRange,
      RefPtr<nsRange>&& aNewEndRange, RefPtr<nsRange>&& aNewSuffixRange,
      RangeContentCache& aRangeContentCache) const;

  /**
   * @brief Returns true if the text directive string in `this` would match the
   *        other candidate.
   *
   * The candidate matches another candidate if the context terms are fully
   * present in the fully-expanded context terms of the other candidate.
   */
  bool ThisCandidateMatchesOther(const TextDirectiveCandidate& aOther) const;

  /**
   * @brief Returns a filtered list of candidates, which still match against
   *        `this`.
   *
   * This method uses `ThisCandidateMatchesOther()` to check whether a candidate
   * is still matching against `this` or can be ruled out.
   */
  nsTArray<const TextDirectiveCandidate*> FilterNonMatchingCandidates(
      const nsTArray<const TextDirectiveCandidate*>& aMatches);

  /** Returns true if the candidate uses exact matching (and not range-based) */
  bool UseExactMatch() const { return !mEndRange; }

  nsRange* StartRange() { return mStartRange; }
  const nsRange* StartRange() const { return mStartRange; }

  nsRange* EndRange() { return mEndRange; }
  const nsRange* EndRange() const { return mEndRange; }

  /**
   * @brief Returns a percent-encoded text directive string representation of
   *        this candidate.
   */
  const nsCString& TextDirectiveString() const;

  /**
   * Logging utility. This function outputs the current state, ie. the text
   * directive string, the context term range contents, the fully expanded
   * context terms, and a fully expanded text directive string.
   */
  void LogCurrentState(const char* aCallerFunc) const;

  const TextDirectiveCandidateContents& RangeContents() const {
    return mFoldCaseContents;
  };

 private:
  TextDirectiveCandidate(nsRange* aStartRange, nsRange* aFullStartRange,
                         nsRange* aEndRange, nsRange* aFullEndRange,
                         nsRange* aPrefixRange, nsRange* aFullPrefixRange,
                         nsRange* aSuffixRange, nsRange* aFullSuffixRange);

  /**
   * @brief Creates a range which starts at the beginning of `aRange` and ends
   *        at the first block boundary inside of `aRange`.
   *
   * @return nullptr if `aRange` does not contain a block boundary.
   */
  static Result<RefPtr<nsRange>, ErrorResult>
  MaybeCreateStartToBlockBoundaryRange(const nsRange& aRange);

  /**
   * @brief Creates a range which starts at the last block boundary in `aRange`
   *        and ends at `aRange`s end.
   *
   * @return nullptr if `aRange` does not contain a block boundary.
   */
  static Result<RefPtr<nsRange>, ErrorResult>
  MaybeCreateEndToBlockBoundaryRange(const nsRange& aRange);

  /**
   * @brief Creates the collapsed and fully expanded prefix ranges.
   *
   * The created ranges _end_ at `aRangeBoundary`. The first returned element is
   * collapsed to `aRangeBoundary`, the second one is expanded to the nearest
   * block boundary to the left.
   *
   * @param aRangeBoundary The end point of the created ranges.
   * @return The first element is the collapsed range, the second one is the
   *         fully expanded range.
   */
  static Result<std::tuple<RefPtr<nsRange>, RefPtr<nsRange>>, ErrorResult>
  CreatePrefixRanges(const RangeBoundary& aRangeBoundary);

  /**
   * @brief Creates the collapsed and fully expanded suffix ranges.
   *
   * The created ranges _start_ at `aRangeBoundary`. The first returned element
   * is collapsed to `aRangeBoundary`, the second one is expanded to the nearest
   * block boundary to the right.
   *
   * @param aRangeBoundary The start point of the created ranges.
   * @return The first element is the collapsed range, the second one is the
   *         fully expanded range.
   */
  static Result<std::tuple<RefPtr<nsRange>, RefPtr<nsRange>>, ErrorResult>
  CreateSuffixRanges(const RangeBoundary& aRangeBoundary);

  /**
   * @brief Creates a percent-encoded string representation of the candidate.
   *
   */
  Result<Ok, ErrorResult> CreateTextDirectiveString();

  /**
   * Creates fold case representations of all ranges used in this candidate.
   */
  Result<Ok, ErrorResult> CreateFoldCaseContents(
      RangeContentCache& aRangeContentCache);

  RefPtr<nsRange> mStartRange;
  RefPtr<nsRange> mFullStartRange;
  RefPtr<nsRange> mEndRange;
  RefPtr<nsRange> mFullEndRange;

  RefPtr<nsRange> mPrefixRange;
  RefPtr<nsRange> mFullPrefixRange;
  RefPtr<nsRange> mSuffixRange;
  RefPtr<nsRange> mFullSuffixRange;

  nsCString mTextDirectiveString;

  TextDirectiveCandidateContents mFoldCaseContents{};
};

/**
 * @brief Helper class to create a text directive string from a given `nsRange`.
 *
 * The class provides a public static creator function which encapsulates all
 * necessary logic. The class itself stores necessary state throughout the
 * steps.
 */
class TextDirectiveCreator final {
 public:
  /**
   * @brief Static creator function. Takes an `nsRange` and creates a text
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
      Document& aDocument, nsRange* aInputRange);

 private:
  TextDirectiveCreator(Document& aDocument, nsRange* aInputRange,
                       TextDirectiveCandidate&& aTextDirective,
                       RangeContentCache&& aRangeContentCache);
  /**
   * Find all ranges up to the end of the target range that have the same
   * content. The input range will *not* be part of the result, therefore an
   * empty array indicates that there are no conflicts and the text directive
   * can be constructed trivially.
   */
  Result<nsTArray<TextDirectiveCandidate>, ErrorResult>
  FindAllMatchingCandidates();

  /**
   * @brief Find all occurrences of `aSearchQuery` in the partial document.
   *
   * This method uses `nsFind` to perform a case-insensitive search for
   * `aSearchQuery` in the document up to `aSearchEnd`.
   *
   * @return List of `nsRange`s which have the case-insensitive-same content as
   *         `aSearchQuery`.
   */
  Result<nsTArray<RefPtr<nsRange>>, ErrorResult> FindAllMatchingRanges(
      const nsString& aSearchQuery, const RangeBoundary& aSearchEnd);

  /**
   * @brief Creates a list of matches which match against every candidate.
   *
   * This method returns the subset of `aMatches` for each `aCandidate`, for
   * which the candidate is still a match.
   */
  nsTArray<std::pair<TextDirectiveCandidate,
                     nsTArray<const TextDirectiveCandidate*>>>
  FindMatchesForCandidates(
      nsTArray<TextDirectiveCandidate>&& aCandidates,
      const nsTArray<const TextDirectiveCandidate*>& aMatches);

  Result<nsCString, ErrorResult> CreateTextDirectiveFromMatches(
      const nsTArray<TextDirectiveCandidate>& aTextDirectiveMatches);

  Document& mDocument;
  RefPtr<nsRange> mInputRange;
  TextDirectiveCandidate mTextDirective;
  RangeContentCache mRangeContentCache;

  /**
   * The watchdog ensures that the algorithm exits after a defined time
   * duration, to ensure that the main thread is not blocked for too long.
   *
   * The duration is defined by the pref
   * `dom.text_fragments.create_text_fragment.timeout`.
   */
  TimeoutWatchdog mWatchdog;
};
}  // namespace mozilla::dom
#endif
