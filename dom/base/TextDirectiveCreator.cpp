/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TextDirectiveCreator.h"
#include "TextDirectiveUtil.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/ResultVariant.h"
#include "mozilla/dom/fragmentdirectives_ffi_generated.h"
#include "nsDeque.h"
#include "nsINode.h"
#include "nsRange.h"
#include "Document.h"

namespace mozilla::dom {

Result<const nsString*, ErrorResult> RangeContentCache::GetOrCreate(
    nsRange* aRange) {
  if (!aRange) {
    return nullptr;
  }
  if (const auto rangeContent = mCache.Lookup(aRange)) {
    return rangeContent->get();
  }
  return TextDirectiveUtil::RangeContentAsFoldCase(aRange).andThen(
      [cache = &mCache, range = RefPtr(aRange)](
          nsString&& content) -> Result<const nsString*, ErrorResult> {
        return cache
            ->InsertOrUpdate(range, MakeUnique<nsString>(std::move(content)))
            .get();
      });
}

TextDirectiveCandidate::TextDirectiveCandidate(
    nsRange* aStartRange, nsRange* aFullStartRange, nsRange* aEndRange,
    nsRange* aFullEndRange, nsRange* aPrefixRange, nsRange* aFullPrefixRange,
    nsRange* aSuffixRange, nsRange* aFullSuffixRange)
    : mStartRange(aStartRange),
      mFullStartRange(aFullStartRange),
      mEndRange(aEndRange),
      mFullEndRange(aFullEndRange),
      mPrefixRange(aPrefixRange),
      mFullPrefixRange(aFullPrefixRange),
      mSuffixRange(aSuffixRange),
      mFullSuffixRange(aFullSuffixRange) {}

/* static */
Result<TextDirectiveCandidate, ErrorResult>
TextDirectiveCandidate::CreateFromInputRange(
    const nsRange* aInputRange, RangeContentCache& aRangeContentCache) {
  MOZ_ASSERT(aInputRange);
  ErrorResult rv;
  // To create a fully-populated text directive candidate from a given Range
  // `inputRange`, follow these steps:
  // 1. Let `fullStartRange`, `fullEndRange`, `startRange` and `endRange` be
  // Ranges, initially null.
  // 2. Let `firstBlockBoundary` be the result of searching for the first block
  // boundary inside of `inputRange`.
  // 3. If `firstBlockBoundary` is not null:
  // 3.1. Set the boundary points of `fullStartRange` to the start point of
  // `inputRange` and the found block boundary.
  Result<RefPtr<nsRange>, ErrorResult> maybeFullStartRange =
      MaybeCreateStartToBlockBoundaryRange(*aInputRange);
  if (MOZ_UNLIKELY(maybeFullStartRange.isErr())) {
    return maybeFullStartRange.propagateErr();
  }
  RefPtr<nsRange> fullStartRange = maybeFullStartRange.unwrap();
  RefPtr<nsRange> fullEndRange;
  // 4. Let `useExactMatching` be true if `fullStartRange` is null.
  // Note: This means that the context terms for this candidate will only be
  // prefix and suffix, instead of also start and end.
  const bool useExactMatching = !fullStartRange;

  RefPtr<nsRange> startRange;
  RefPtr<nsRange> endRange;
  // 5. If `useExactMatching` is true:
  if (useExactMatching) {
    // 5.1 Let `startRange` be a Range with the same boundary points as
    // `inputRange`.
    startRange = aInputRange->CloneRange();
  } else {
    // 6. Otherwise:
    // 6.1 Search for the first block boundary inside of `inputRange`, starting
    // from `inputRange`s end point.
    // 6.2 Assert: There is a block boundary in `inputRange`.
    // Note: Since searching from `inputRange`s start found a block boundary,
    // searching from the end also must find one.
    // 6.3 Let `fullEndRange` be a range starting at lastBlockBoundary and
    // ending at `inputRange`s end point.
    Result<RefPtr<nsRange>, ErrorResult> maybeFullEndRange =
        MaybeCreateEndToBlockBoundaryRange(*aInputRange);
    if (MOZ_UNLIKELY(maybeFullEndRange.isErr())) {
      return maybeFullEndRange.propagateErr();
    }
    fullEndRange = maybeFullEndRange.unwrap();
    MOZ_ASSERT(fullEndRange,
               "Searching from start found a range boundary in the range, so "
               "searching from the end must find one as well");

    // 6.4 Let `startRange` be a Range which starts at the start of `inputRange`
    // and ends at the first word boundary after `inputRange`s start point.
    startRange =
        nsRange::Create(aInputRange->StartRef(),
                        TextDirectiveUtil::MoveRangeBoundaryOneWord(
                            aInputRange->StartRef(), TextScanDirection::Right),
                        rv);
    if (MOZ_UNLIKELY(rv.Failed())) {
      return Err(std::move(rv));
    }
    // 6.5 Let `endRange` be a Range which starts at the beginning of the last
    // word of `inputRange` and ends at the end of `inputRange`.
    endRange =
        nsRange::Create(TextDirectiveUtil::MoveRangeBoundaryOneWord(
                            aInputRange->EndRef(), TextScanDirection::Left),
                        aInputRange->EndRef(), rv);
    if (rv.Failed()) {
      return Err(std::move(rv));
    }
  }

  // 7. Let `prefixRange` be a Range, collapsed at the start point of
  // `inputRange`.
  // 8. Let `prefixBlockBoundary` be the result of finding the next block
  // boundary, starting at `inputRange`s start point and going to the left.
  // Note: If `inputRange`s start point is a block boundary, this should extend
  // to the _next_ block boundary.
  // 9. Let `fullPrefixRange`  be a Range which starts at
  // `prefixBlockBoundary` and ends at `inputRange`s start point.
  auto prefixRanges = CreatePrefixRanges(startRange->StartRef());
  if (MOZ_UNLIKELY(prefixRanges.isErr())) {
    return prefixRanges.propagateErr();
  }
  auto [prefixRange, fullPrefixRange] = prefixRanges.unwrap();

  // 10. Let `suffixRange` be a Range, collapsed at the end point of
  // `inputRange`.
  // 11. Let `suffixBlockBoundary` be the result of finding the next block
  // boundary, starting at `inputRange`s end point and going to the right.
  // Note: If `inputRange`s end point is a block boundary, this should extend to
  // the _next_ block boundary.
  // 12. Let `fullSuffixRange` be a Range which starts at `inputRange`s end
  // point and ends at `suffixBlockBoundary`.
  auto suffixRanges =
      CreateSuffixRanges(endRange ? endRange->EndRef() : startRange->EndRef());
  if (MOZ_UNLIKELY(suffixRanges.isErr())) {
    return suffixRanges.propagateErr();
  }
  auto [suffixRange, fullSuffixRange] = suffixRanges.unwrap();

  // 13. Return a structure which contains `startRange`, `fullStartRange`,
  // `endRange`, `fullEndRange`, `prefixRange`, `fullPrefixRange`, `suffixRange`
  // and `fullSuffixRange`.
  auto instance = TextDirectiveCandidate{
      startRange,  fullStartRange,  endRange,    fullEndRange,
      prefixRange, fullPrefixRange, suffixRange, fullSuffixRange};
  MOZ_TRY(instance.CreateFoldCaseContents(aRangeContentCache));
  MOZ_TRY(instance.CreateTextDirectiveString());
  return instance;
}

/* static */
Result<TextDirectiveCandidate, ErrorResult>
TextDirectiveCandidate::CreateFromStartAndEndRange(
    const nsRange* aStartRange, const nsRange* aEndRange,
    RangeContentCache& aRangeContentCache) {
  MOZ_ASSERT(aStartRange);
  MOZ_ASSERT(aEndRange);
  ErrorResult rv;
  RefPtr<nsRange> startRange = aStartRange->CloneRange();
  RefPtr<nsRange> endRange = aEndRange->CloneRange();
  RefPtr<nsRange> fullRange =
      nsRange::Create(startRange->StartRef(), endRange->EndRef(), rv);
  if (MOZ_UNLIKELY(rv.Failed())) {
    return Err(std::move(rv));
  }
  Result<RefPtr<nsRange>, ErrorResult> maybeFullStartRange =
      MaybeCreateStartToBlockBoundaryRange(*fullRange);
  if (MOZ_UNLIKELY(maybeFullStartRange.isErr())) {
    return maybeFullStartRange.propagateErr();
  }
  RefPtr<nsRange> fullStartRange = maybeFullStartRange.unwrap();

  // This function is called in a context where there is not necessarily a block
  // boundary between `aStartRange` and `aEndRange`.
  // Therefore, it is not an error if `fullStartRange` is null (unlike in
  // `CreateFromInputRange()`).
  // If there is no block boundary, use the full range as `fullStartRange` and
  // `fullEndRange`.
  if (!fullStartRange) {
    fullStartRange = fullRange->CloneRange();
  }

  Result<RefPtr<nsRange>, ErrorResult> maybeFullEndRange =
      MaybeCreateEndToBlockBoundaryRange(*fullRange);
  if (MOZ_UNLIKELY(maybeFullEndRange.isErr())) {
    return maybeFullStartRange.propagateErr();
  }
  RefPtr<nsRange> fullEndRange = maybeFullStartRange.unwrap();
  if (!fullEndRange) {
    fullEndRange = fullRange->CloneRange();
  }

  auto prefixRanges = CreatePrefixRanges(startRange->StartRef());
  if (MOZ_UNLIKELY(prefixRanges.isErr())) {
    return prefixRanges.propagateErr();
  }
  auto [prefixRange, fullPrefixRange] = prefixRanges.unwrap();
  auto suffixRanges = CreateSuffixRanges(endRange->EndRef());
  if (MOZ_UNLIKELY(suffixRanges.isErr())) {
    return suffixRanges.propagateErr();
  }
  auto [suffixRange, fullSuffixRange] = suffixRanges.unwrap();
  auto instance = TextDirectiveCandidate{
      startRange,  fullStartRange,  endRange,    fullEndRange,
      prefixRange, fullPrefixRange, suffixRange, fullSuffixRange};
  MOZ_TRY(instance.CreateFoldCaseContents(aRangeContentCache));
  MOZ_TRY(instance.CreateTextDirectiveString());
  return instance;
}

Result<TextDirectiveCandidate, ErrorResult> TextDirectiveCandidate::CloneWith(
    RefPtr<nsRange>&& aNewPrefixRange, RefPtr<nsRange>&& aNewStartRange,
    RefPtr<nsRange>&& aNewEndRange, RefPtr<nsRange>&& aNewSuffixRange,
    RangeContentCache& aRangeContentCache) const {
  MOZ_ASSERT(
      mFullPrefixRange && mFullSuffixRange,
      "TextDirectiveCandidate::CloneWith(): Source object is in invalid state");

  // start with a shallow copy. This copies all ranges, and all fold case
  // content raw pointers.
  TextDirectiveCandidate clone(*this);

  // Replace the pointers for all ranges which are present in the parameter
  // list, and create new fold case representations.
  if (aNewPrefixRange) {
    clone.mPrefixRange = std::move(aNewPrefixRange);
    Result<const nsString*, ErrorResult> maybeContent =
        aRangeContentCache.GetOrCreate(clone.mPrefixRange);
    if (MOZ_UNLIKELY(maybeContent.isErr())) {
      return maybeContent.propagateErr();
    }
    clone.mFoldCaseContents.prefix_content = maybeContent.unwrap();
  }
  if (aNewStartRange) {
    clone.mStartRange = std::move(aNewStartRange);
    Result<const nsString*, ErrorResult> maybeContent =
        aRangeContentCache.GetOrCreate(clone.mStartRange);
    if (MOZ_UNLIKELY(maybeContent.isErr())) {
      return maybeContent.propagateErr();
    }
    clone.mFoldCaseContents.start_content = maybeContent.unwrap();
  }
  // mEndRange is null if exact matching is used.
  MOZ_ASSERT_IF(aNewEndRange, mEndRange);
  if (aNewEndRange) {
    clone.mEndRange = std::move(aNewEndRange);
    Result<const nsString*, ErrorResult> maybeContent =
        aRangeContentCache.GetOrCreate(clone.mEndRange);
    if (MOZ_UNLIKELY(maybeContent.isErr())) {
      return maybeContent.propagateErr();
    }
    clone.mFoldCaseContents.end_content = maybeContent.unwrap();
  }
  if (aNewSuffixRange) {
    clone.mSuffixRange = std::move(aNewSuffixRange);
    Result<const nsString*, ErrorResult> maybeContent =
        aRangeContentCache.GetOrCreate(clone.mSuffixRange);
    if (MOZ_UNLIKELY(maybeContent.isErr())) {
      return maybeContent.propagateErr();
    }
    clone.mFoldCaseContents.suffix_content = maybeContent.unwrap();
  }

  // from now on, the cloned candidate is immutable. Therefore it is allowed to
  // create a text directive string representation, which will be stored in the
  // object and used later.
  MOZ_TRY(clone.CreateTextDirectiveString());
  return clone;
}

Result<nsTArray<TextDirectiveCandidate>, ErrorResult>
TextDirectiveCandidate::CreateNewCandidatesForMatches(
    const nsTArray<const TextDirectiveCandidate*>& aMatches,
    RangeContentCache& aRangeContentCache) {
  TEXT_FRAGMENT_LOG("Creating new candidates for candidate {}",
                    mTextDirectiveString);

  // To Create a list of new candidates for a text directive candidate given a
  // set of text directive candidates `matches`, follow these steps:
  // 1. Let `newCandidates` be a list of text directive candidates, initially
  //    empty.
  nsTArray<TextDirectiveCandidate> newCandidates;
  // 2. For each `match` in `matches`:
  for (const auto* match : aMatches) {
    // 2.1 Let `newCandidatesForThisMatch` be the result of the Create New
    //     Candidates for a given match algorithm.
    Result<nsTArray<TextDirectiveCandidate>, ErrorResult>
        maybeNewCandidatesForThisMatch =
            CreateNewCandidatesForGivenMatch(*match, aRangeContentCache);
    if (MOZ_UNLIKELY(maybeNewCandidatesForThisMatch.isErr())) {
      return maybeNewCandidatesForThisMatch.propagateErr();
    }
    auto newCandidatesForThisMatch = maybeNewCandidatesForThisMatch.unwrap();
    // 2.2 If `newCandidatesForThisMatch` is empty, return an empty list.
    // Note: If it was not possible to create a new match (ie. it was not
    // possible to find a unique match by extending any of the context terms),
    // it is not possible to create a text directive for the given input range.
    // In this case, return an empty array to indicate this to the caller.
    if (newCandidatesForThisMatch.IsEmpty()) {
      return nsTArray<TextDirectiveCandidate>{};
    }
    // 2.3 Insert the elements of `newCandidatesForThisMatch` into
    //     `newCandidates`.
    newCandidates.AppendElements(std::move(newCandidatesForThisMatch));
  }
  // 3. Return `newCandidates`.
  return newCandidates;
}

Result<nsTArray<TextDirectiveCandidate>, ErrorResult>
TextDirectiveCandidate::CreateNewCandidatesForGivenMatch(
    const TextDirectiveCandidate& aOther,
    RangeContentCache& aRangeContentCache) const {
  // To Create New Candidates For A Given Match given a text directive candidate
  // `this` and a text directive candidate `other`, follow these steps:
  // 1. Let `newCandidates` be a list of text directive candidates, initially
  //    empty.
  // 2. For `contextTerm` in ['prefix´, 'suffix´, 'start', 'end']:
  // 2.1 Let `extendedRange` be a range, initially null.
  // 2.2 Let `thisFullRange` be the fully-expanded range for
  //     `contextTerm` of `this` candidate, and `otherFullRange` the
  //     fully-expanded range for `contextTerm` of the other candidate.
  // 2.3 if `contextTerm` is 'start' or 'end' and exact matching is used,
  //     continue.
  // 2.4 Let `thisFullRangeContent` and `otherFullRangeContent` be the fold-case
  //     representations of `thisFullRange` and `otherFullRange`.
  // 2.5 If `contextTerm` is 'prefix´ or 'end', let `commonContext` be the
  //     common suffix of `thisFullRangeContent` and `otherFullRangeContent`.
  //     Otherwise, let `commonContext` be the common prefix.
  // 2.6 If `commonContext` is equal to `thisFullRangeContent`, continue.
  // 2.7  If 'contextTerm' is 'prefix' or 'end':
  // 2.7.1 Create a range boundary `newRangeBoundary` by moving the length of
  //       the common suffix, starting from the end point of the fully
  //       expanded range of the property.
  // 2.7.2 Move `newRangeBoundary` one word to the left.
  // 2.7.3 Assert: `newRangeBoundary` is not before the start boundary of the
  //       fully expanded context term (this would conflict with step 2.6).
  // 2.7.4 Initialize `extendedRange` using `newRangeBoundary` as start point
  //       and the end point of the fully expanded range of `this` as end point.
  // 2.8 else:
  // 2.8.1 Create a range boundary `newRangeBoundary` by moving the length of
  //       the common prefix, starting from the start point of the fully
  //       expanded range of the property.
  // 2.8.2 Move `newRangeBoundary` one word to the right.
  // 2.8.3 Assert: `newRangeBoundary` is not behind the end boundary of the
  //       fully expanded context term (this would conflict with step 2.6).
  // 2.8.4 Initialize `extendedRange` using the start point of the fully
  //       expanded range of `this` as start point and `newRangeBoundary` as end
  //       point.
  // 2.9 If `contextTerm` is `start`:
  // 2.9.1 Let `expandLimit` be the start point of the non-expanded end range.
  // 2.9.2 If `newRangeBoundary` is behind `expandLimit`, set `newRangeBoundary`
  //       to `expandLimit`.
  // 2.10 If `contextTerm` is `end`:
  // 2.10.1 Let `expandLimit` be the end point of the non-expanded start range.
  // 2.10.2 If `newRangeBoundary` is before `expandLimit`, set
  //        `newRangeBoundary` to `expandLimit`.
  // 2.9 Clone the text directive candidate `this`, replacing `contextTerm`
  //     with `extendedRange` and insert it into `newCandidates`.
  // 3. Return `newCandidates`.

  AutoTArray<TextDirectiveCandidate, 4> newCandidates;
  auto createRangeExtendedUntilMismatch =
      [](nsRange* thisFullRange, const nsString& thisFullRangeFoldCase,
         nsRange* otherFullRange, const nsString& otherFullRangeFoldCase,
         TextScanDirection expandDirection,
         Maybe<RangeBoundary> expandLimit =
             Nothing{}) -> Result<RefPtr<nsRange>, ErrorResult> {
    ErrorResult rv;
    auto equalSubstringLength =
        expandDirection == TextScanDirection::Right
            ? TextDirectiveUtil::FindCommonPrefix(thisFullRangeFoldCase,
                                                  otherFullRangeFoldCase)
            : TextDirectiveUtil::FindCommonSuffix(thisFullRangeFoldCase,
                                                  otherFullRangeFoldCase);
    if (equalSubstringLength == thisFullRangeFoldCase.Length()) {
      // even when the range is fully extended, it would match `other`.
      // Therefore, it can be skipped.
      return RefPtr<nsRange>(nullptr);
    }
    size_t indexFromStartOfFullRange =
        expandDirection == TextScanDirection::Right
            ? equalSubstringLength
            : thisFullRangeFoldCase.Length() - equalSubstringLength;
    Result<RangeBoundary, ErrorResult> maybeNewRangeBoundary =
        TextDirectiveUtil::CreateRangeBoundaryByMovingOffsetFromRangeStart(
            thisFullRange, indexFromStartOfFullRange);
    if (MOZ_UNLIKELY(maybeNewRangeBoundary.isErr())) {
      return maybeNewRangeBoundary.propagateErr();
    }
    RangeBoundary newRangeBoundary =
        TextDirectiveUtil::MoveRangeBoundaryOneWord(
            maybeNewRangeBoundary.unwrap(), expandDirection);
    if (expandLimit.isSome()) {
      // in the range-based matching case it needs to be ensured that start and
      // end don't overlap.
      if (auto compareResult =
              nsContentUtils::ComparePoints(newRangeBoundary, *expandLimit)) {
        if ((expandDirection == TextScanDirection::Right &&
             *compareResult != 1) ||
            (expandDirection == TextScanDirection::Left &&
             *compareResult != -1)) {
          newRangeBoundary = *expandLimit;
        }
      }
    }
    RefPtr<nsRange> newRange = thisFullRange->CloneRange();
    if (expandDirection == TextScanDirection::Right) {
      newRange->SetEnd(newRangeBoundary.AsRaw(), rv);
      if (MOZ_UNLIKELY(rv.Failed())) {
        return Err(std::move(rv));
      }
    } else {
      newRange->SetStart(newRangeBoundary.AsRaw(), rv);
      if (MOZ_UNLIKELY(rv.Failed())) {
        return Err(std::move(rv));
      }
    }
    return newRange;
  };

  auto createAndAddCandidate =
      [candidate = this, &newCandidates, &aRangeContentCache,
       func = __FUNCTION__](
          const char* contextTermName, RefPtr<nsRange>&& extendedPrefix,
          RefPtr<nsRange>&& extendedStart, RefPtr<nsRange>&& extendedEnd,
          RefPtr<nsRange>&& extendedSuffix) -> Result<Ok, ErrorResult> {
    if (!extendedPrefix && !extendedStart && !extendedEnd && !extendedSuffix) {
      TEXT_FRAGMENT_LOG_FN("Could not extend the {} because it is ambiguous.",
                           func, contextTermName);
      return Ok();
    }
    Result<TextDirectiveCandidate, ErrorResult> extendedCandidate =
        candidate->CloneWith(std::move(extendedPrefix),
                             std::move(extendedStart), std::move(extendedEnd),
                             std::move(extendedSuffix), aRangeContentCache);
    if (MOZ_UNLIKELY(extendedCandidate.isErr())) {
      return extendedCandidate.propagateErr();
    }
    newCandidates.AppendElement(extendedCandidate.unwrap());
    TEXT_FRAGMENT_LOG_FN("Created candidate by extending the {}: {}", func,
                         contextTermName,
                         newCandidates.LastElement().TextDirectiveString());
    return Ok();
  };
  MOZ_ASSERT(mFoldCaseContents.full_prefix_content &&
             aOther.mFoldCaseContents.full_prefix_content);
  MOZ_TRY(
      createRangeExtendedUntilMismatch(
          mFullPrefixRange, *mFoldCaseContents.full_prefix_content,
          aOther.mFullPrefixRange,
          *aOther.mFoldCaseContents.full_prefix_content,
          TextScanDirection::Left)
          .andThen([&createAndAddCandidate](RefPtr<nsRange>&& extendedRange) {
            return createAndAddCandidate("prefix", std::move(extendedRange),
                                         nullptr, nullptr, nullptr);
          }));
  MOZ_ASSERT(mFoldCaseContents.full_suffix_content &&
             aOther.mFoldCaseContents.full_suffix_content);
  MOZ_TRY(
      createRangeExtendedUntilMismatch(
          mFullSuffixRange, *mFoldCaseContents.full_suffix_content,
          aOther.mFullSuffixRange,
          *aOther.mFoldCaseContents.full_suffix_content,
          TextScanDirection::Right)
          .andThen([&createAndAddCandidate](RefPtr<nsRange>&& extendedRange) {
            return createAndAddCandidate("suffix", nullptr, nullptr, nullptr,
                                         std::move(extendedRange));
          }));

  MOZ_ASSERT(UseExactMatch() == aOther.UseExactMatch());
  if (UseExactMatch()) {
    return std::move(newCandidates);
  }
  MOZ_ASSERT(mFoldCaseContents.full_start_content &&
             aOther.mFoldCaseContents.full_start_content);
  MOZ_TRY(
      createRangeExtendedUntilMismatch(
          mFullStartRange, *mFoldCaseContents.full_start_content,
          aOther.mFullStartRange, *aOther.mFoldCaseContents.full_start_content,
          TextScanDirection::Right, Some(mEndRange->StartRef()))
          .andThen([&createAndAddCandidate](RefPtr<nsRange>&& extendedRange) {
            return createAndAddCandidate(
                "start", nullptr, std::move(extendedRange), nullptr, nullptr);
          }));
  MOZ_ASSERT(mFoldCaseContents.full_end_content &&
             aOther.mFoldCaseContents.full_end_content);
  MOZ_TRY(
      createRangeExtendedUntilMismatch(
          mFullEndRange, *mFoldCaseContents.full_end_content,
          aOther.mFullEndRange, *aOther.mFoldCaseContents.full_end_content,
          TextScanDirection::Left)
          .andThen([&createAndAddCandidate](RefPtr<nsRange>&& extendedRange) {
            return createAndAddCandidate("end", nullptr, nullptr,
                                         std::move(extendedRange), nullptr);
          }));
  return std::move(newCandidates);
}

nsTArray<const TextDirectiveCandidate*>
TextDirectiveCandidate::FilterNonMatchingCandidates(
    const nsTArray<const TextDirectiveCandidate*>& aMatches) {
  AutoTArray<const TextDirectiveCandidate*, 8> stillMatching;
  for (const auto* match : aMatches) {
    if (ThisCandidateMatchesOther(*match)) {
      stillMatching.AppendElement(match);
    }
  }
  return std::move(stillMatching);
}

/* static */ Result<RefPtr<nsRange>, ErrorResult>
TextDirectiveCandidate::MaybeCreateStartToBlockBoundaryRange(
    const nsRange& aRange) {
  ErrorResult rv;
  // this call returns `Nothing` if the block boundary is outside of the input
  // range.
  Result<RefPtr<nsRange>, ErrorResult> fullRange =
      TextDirectiveUtil::FindBlockBoundaryInRange(aRange,
                                                  TextScanDirection::Right)
          .andThen([&aRange, &rv](
                       auto boundary) -> Result<RefPtr<nsRange>, ErrorResult> {
            RefPtr range =
                boundary ? nsRange::Create(aRange.StartRef(), *boundary, rv)
                         : nullptr;
            if (MOZ_UNLIKELY(rv.Failed())) {
              return Err(std::move(rv));
            }
            return range;
          });
  return fullRange;
}

/* static */ Result<RefPtr<nsRange>, ErrorResult>
TextDirectiveCandidate::MaybeCreateEndToBlockBoundaryRange(
    const nsRange& aRange) {
  ErrorResult rv;
  // this call returns `Nothing` if the block boundary is outside of the input
  // range.
  Result<RefPtr<nsRange>, ErrorResult> fullRange =
      TextDirectiveUtil::FindBlockBoundaryInRange(aRange,
                                                  TextScanDirection::Left)
          .andThen([&aRange, &rv](
                       auto boundary) -> Result<RefPtr<nsRange>, ErrorResult> {
            RefPtr range = boundary
                               ? nsRange::Create(*boundary, aRange.EndRef(), rv)
                               : nullptr;
            if (MOZ_UNLIKELY(rv.Failed())) {
              return Err(std::move(rv));
            }
            return range;
          });
  return fullRange;
}

bool TextDirectiveCandidate::ThisCandidateMatchesOther(
    const TextDirectiveCandidate& aOther) const {
  if (TextDirectiveUtil::FindCommonSuffix(
          *mFoldCaseContents.prefix_content,
          *aOther.mFoldCaseContents.full_prefix_content) !=
      mFoldCaseContents.prefix_content->Length()) {
    return false;
  }
  if (TextDirectiveUtil::FindCommonPrefix(
          *mFoldCaseContents.suffix_content,
          *aOther.mFoldCaseContents.full_suffix_content) !=
      mFoldCaseContents.suffix_content->Length()) {
    return false;
  }

  MOZ_ASSERT(UseExactMatch() == aOther.UseExactMatch());
  if (UseExactMatch()) {
    return true;
  }
  if (TextDirectiveUtil::FindCommonPrefix(
          *mFoldCaseContents.start_content,
          *aOther.mFoldCaseContents.full_start_content) !=
      mFoldCaseContents.start_content->Length()) {
    return false;
  }
  return TextDirectiveUtil::FindCommonSuffix(
             *mFoldCaseContents.end_content,
             *aOther.mFoldCaseContents.full_end_content) ==
         mFoldCaseContents.end_content->Length();
}

/* static */
Result<std::tuple<RefPtr<nsRange>, RefPtr<nsRange>>, ErrorResult>
TextDirectiveCandidate::CreatePrefixRanges(
    const RangeBoundary& aRangeBoundary) {
  MOZ_ASSERT(aRangeBoundary.IsSetAndValid());
  ErrorResult rv;
  RangeBoundary previousNonWhitespacePoint =
      TextDirectiveUtil::MoveBoundaryToPreviousNonWhitespacePosition(
          aRangeBoundary);
  RefPtr<nsRange> prefixRange = nsRange::Create(previousNonWhitespacePoint,
                                                previousNonWhitespacePoint, rv);
  if (rv.Failed()) {
    return Err(std::move(rv));
  }
  Result<RangeBoundary, ErrorResult> fullPrefixStartBoundary =
      TextDirectiveUtil::FindNextBlockBoundary(prefixRange->EndRef(),
                                               TextScanDirection::Left);
  if (MOZ_UNLIKELY(fullPrefixStartBoundary.isErr())) {
    return fullPrefixStartBoundary.propagateErr();
  }
  RefPtr<nsRange> fullPrefixRange = nsRange::Create(
      fullPrefixStartBoundary.unwrap(), prefixRange->EndRef(), rv);
  if (rv.Failed()) {
    return Err(std::move(rv));
  }
  return std::tuple{prefixRange, fullPrefixRange};
}

/* static */
Result<std::tuple<RefPtr<nsRange>, RefPtr<nsRange>>, ErrorResult>
TextDirectiveCandidate::CreateSuffixRanges(
    const RangeBoundary& aRangeBoundary) {
  MOZ_ASSERT(aRangeBoundary.IsSetAndValid());
  RangeBoundary nextNonWhitespacePoint =
      TextDirectiveUtil::MoveBoundaryToNextNonWhitespacePosition(
          aRangeBoundary);
  ErrorResult rv;
  RefPtr<nsRange> suffixRange =
      nsRange::Create(nextNonWhitespacePoint, nextNonWhitespacePoint, rv);
  if (rv.Failed()) {
    return Err(std::move(rv));
  }
  Result<RangeBoundary, ErrorResult> fullSuffixEndBoundary =
      TextDirectiveUtil::FindNextBlockBoundary(suffixRange->EndRef(),
                                               TextScanDirection::Right);
  if (MOZ_UNLIKELY(fullSuffixEndBoundary.isErr())) {
    return fullSuffixEndBoundary.propagateErr();
  }
  RefPtr<nsRange> fullSuffixRange = nsRange::Create(
      suffixRange->StartRef(), fullSuffixEndBoundary.unwrap(), rv);
  if (rv.Failed()) {
    return Err(std::move(rv));
  }
  return std::tuple{suffixRange, fullSuffixRange};
}

const nsCString& TextDirectiveCandidate::TextDirectiveString() const {
  return mTextDirectiveString;
}

Result<Ok, ErrorResult> TextDirectiveCandidate::CreateTextDirectiveString() {
  Result<TextDirective, ErrorResult> maybeTextDirective =
      TextDirectiveUtil::CreateTextDirectiveFromRanges(
          mPrefixRange, mStartRange, mEndRange, mSuffixRange);
  if (MOZ_UNLIKELY(maybeTextDirective.isErr())) {
    return maybeTextDirective.propagateErr();
  }
  TextDirective textDirective = maybeTextDirective.unwrap();
  create_text_directive(&textDirective, &mTextDirectiveString);
  return Ok();
}

Result<Ok, ErrorResult> TextDirectiveCandidate::CreateFoldCaseContents(
    RangeContentCache& aRangeContentCache) {
  auto getFoldCase = [&aRangeContentCache](
                         nsRange* range,
                         const nsString*& content) -> Result<Ok, ErrorResult> {
    return aRangeContentCache.GetOrCreate(range).andThen(
        [&content](const nsString*&& value) -> Result<Ok, ErrorResult> {
          content = value;
          return Ok();
        });
  };
  MOZ_ASSERT(mFullPrefixRange);
  MOZ_ASSERT(mFullSuffixRange);
  MOZ_ASSERT(mStartRange);
  MOZ_TRY(getFoldCase(mFullPrefixRange, mFoldCaseContents.full_prefix_content));
  MOZ_TRY(getFoldCase(mFullStartRange, mFoldCaseContents.full_start_content));
  MOZ_TRY(getFoldCase(mFullEndRange, mFoldCaseContents.full_end_content));
  MOZ_TRY(getFoldCase(mFullSuffixRange, mFoldCaseContents.full_suffix_content));

  MOZ_TRY(getFoldCase(mPrefixRange, mFoldCaseContents.prefix_content));
  MOZ_TRY(getFoldCase(mStartRange, mFoldCaseContents.start_content));
  MOZ_TRY(getFoldCase(mEndRange, mFoldCaseContents.end_content));
  MOZ_TRY(getFoldCase(mSuffixRange, mFoldCaseContents.suffix_content));
  mFoldCaseContents.use_exact_matching = UseExactMatch();
  return Ok();
}

void TextDirectiveCandidate::LogCurrentState(const char* aCallerFunc) const {
  if (!TextDirectiveUtil::ShouldLog()) {
    return;
  }
  auto getRangeContent = [](nsRange* range) {
    auto content = TextDirectiveUtil::RangeContentAsString(range).unwrapOr(
        u"<nsRange::ToString() failed>"_ns);
    return NS_ConvertUTF16toUTF8(content.IsEmpty() ? u"<empty range>"_ns
                                                   : content);
  };
  auto fullTextDirectiveString =
      TextDirectiveUtil::CreateTextDirectiveFromRanges(
          mFullPrefixRange, mFullStartRange ? mFullStartRange : mStartRange,
          mFullEndRange, mFullSuffixRange)
          .map([](const auto& textDirective) {
            nsCString value;
            create_text_directive(&textDirective, &value);
            return value;
          })
          .unwrapOr("<creating text directive failed>"_ns);
  TEXT_FRAGMENT_LOG_FN(
      "State of text directive candidate {:p}:\nPercent-encoded string: "
      "{}\n\nCurrent context terms:\nPrefix: {}\nStart: {}\nEnd: {}\nSuffix: "
      "{}\n\nMaximum expanded context terms:\nPercent-encoded string: "
      "{}\nPrefix:\n{}\nStart:\n{}\nEnd:\n{}\nSuffix:\n{}",
      aCallerFunc, static_cast<const void*>(this), mTextDirectiveString,
      getRangeContent(mPrefixRange), getRangeContent(mStartRange),
      getRangeContent(mEndRange), getRangeContent(mSuffixRange),
      fullTextDirectiveString, getRangeContent(mFullPrefixRange),
      getRangeContent(mFullStartRange), getRangeContent(mFullEndRange),
      getRangeContent(mFullSuffixRange));
}

TextDirectiveCreator::TextDirectiveCreator(
    Document& aDocument, nsRange* aInputRange,
    TextDirectiveCandidate&& aTextDirective,
    RangeContentCache&& aRangeContentCache)
    : mDocument(aDocument),
      mInputRange(aInputRange),
      mTextDirective(std::move(aTextDirective)),
      mRangeContentCache(std::move(aRangeContentCache)) {}

/* static */
mozilla::Result<nsCString, ErrorResult>
TextDirectiveCreator::CreateTextDirectiveFromRange(Document& aDocument,
                                                   nsRange* aInputRange) {
  MOZ_ASSERT(aInputRange);
  MOZ_ASSERT(!aInputRange->Collapsed());
  // To create a text directive string from a given range, follow these steps:
  // 1. extend inputRange to the next word boundaries in outward direction.
  RefPtr<nsRange> inputRangeExtendedToWordBoundaries =
      aInputRange->CloneRange();

  MOZ_TRY(TextDirectiveUtil::ExtendRangeToWordBoundaries(
      *inputRangeExtendedToWordBoundaries));
  // 2. If the content of the extended range is empty, return an empty string.
  if (inputRangeExtendedToWordBoundaries->Collapsed()) {
    return nsCString{};
  }
  Result<nsString, ErrorResult> rangeContent =
      TextDirectiveUtil::RangeContentAsString(
          inputRangeExtendedToWordBoundaries);
  if (MOZ_UNLIKELY(rangeContent.isErr())) {
    return rangeContent.propagateErr();
  }
  if (rangeContent.unwrap().IsEmpty()) {
    return nsCString{};
  }

  // 3. Create a fully-populated text directive candidate
  // textDirectiveCandidate.
  RangeContentCache rangeContentCache;
  Result<TextDirectiveCandidate, ErrorResult> maybeTextDirectiveCandidate =
      TextDirectiveCandidate::CreateFromInputRange(
          inputRangeExtendedToWordBoundaries, rangeContentCache);
  if (MOZ_UNLIKELY(maybeTextDirectiveCandidate.isErr())) {
    return maybeTextDirectiveCandidate.propagateErr();
  }
  auto textDirectiveCandidate = maybeTextDirectiveCandidate.unwrap();

  TextDirectiveCreator creator(aDocument, inputRangeExtendedToWordBoundaries,
                               std::move(textDirectiveCandidate),
                               std::move(rangeContentCache));

  // 4. Create a list of fully populated text directive candidates.
  // 5. Run the create text directive from matches algorithm and return its
  //    result.
  return creator.FindAllMatchingCandidates().andThen(
      [&creator](nsTArray<TextDirectiveCandidate>&& previousMatches)
          -> Result<nsCString, ErrorResult> {
        return creator.CreateTextDirectiveFromMatches(previousMatches);
      });
}

Result<nsTArray<TextDirectiveCandidate>, ErrorResult>
TextDirectiveCreator::FindAllMatchingCandidates() {
  ErrorResult rv;
  nsTArray<TextDirectiveCandidate> textDirectiveMatches;

  if (mTextDirective.UseExactMatch()) {
    Result<nsString, ErrorResult> rangeContent =
        TextDirectiveUtil::RangeContentAsString(mInputRange);
    if (MOZ_UNLIKELY(rangeContent.isErr())) {
      return rangeContent.propagateErr();
    }
    Result<nsTArray<RefPtr<nsRange>>, ErrorResult> maybeRangeMatches =
        FindAllMatchingRanges(rangeContent.unwrap(),
                              mTextDirective.StartRange()->EndRef());
    if (MOZ_UNLIKELY(maybeRangeMatches.isErr())) {
      return maybeRangeMatches.propagateErr();
    }
    if (mWatchdog.IsDone()) {
      return textDirectiveMatches;
    }
    auto rangeMatches = maybeRangeMatches.unwrap();
    textDirectiveMatches.SetCapacity(rangeMatches.Length());
    for (const auto& rangeMatch : rangeMatches) {
      auto candidate = TextDirectiveCandidate::CreateFromInputRange(
          rangeMatch, mRangeContentCache);
      if (MOZ_UNLIKELY(candidate.isErr())) {
        return candidate.propagateErr();
      }
      textDirectiveMatches.AppendElement(candidate.unwrap());
    }

    return textDirectiveMatches;
  }
  Result<nsString, ErrorResult> startRangeContent =
      TextDirectiveUtil::RangeContentAsString(mTextDirective.StartRange());
  if (MOZ_UNLIKELY(startRangeContent.isErr())) {
    return startRangeContent.propagateErr();
  }
  Result<nsTArray<RefPtr<nsRange>>, ErrorResult> maybeStartRangeMatches =
      FindAllMatchingRanges(startRangeContent.unwrap(),
                            mTextDirective.StartRange()->EndRef());
  if (MOZ_UNLIKELY(maybeStartRangeMatches.isErr())) {
    return maybeStartRangeMatches.propagateErr();
  }
  auto startRangeMatches = maybeStartRangeMatches.unwrap();

  if (mWatchdog.IsDone()) {
    return textDirectiveMatches;
  }

  Result<nsString, ErrorResult> endRangeContent =
      TextDirectiveUtil::RangeContentAsString(mTextDirective.EndRange());
  if (MOZ_UNLIKELY(endRangeContent.isErr())) {
    return endRangeContent.propagateErr();
  }
  Result<nsTArray<RefPtr<nsRange>>, ErrorResult> maybeEndRangeMatches =
      FindAllMatchingRanges(endRangeContent.unwrap(),
                            mTextDirective.EndRange()->EndRef());
  if (MOZ_UNLIKELY(maybeEndRangeMatches.isErr())) {
    return maybeEndRangeMatches.propagateErr();
  }
  auto endRangeMatchesArray = maybeEndRangeMatches.unwrap();

  if (mWatchdog.IsDone()) {
    return textDirectiveMatches;
  }

  nsDeque<nsRange> endRangeMatches;
  for (auto& element : endRangeMatchesArray) {
    endRangeMatches.Push(element.get());
  }
  endRangeMatches.Push(mTextDirective.EndRange());

  size_t counter = 0;
  for (const auto& matchStartRange : startRangeMatches) {
    for (auto* matchEndRange : endRangeMatches) {
      if (++counter % 100 == 0 && mWatchdog.IsDone()) {
        return textDirectiveMatches;
      }
      Maybe<int32_t> compare = nsContentUtils::ComparePoints(
          matchStartRange->EndRef(), matchEndRange->StartRef());
      if (!compare || *compare != -1) {
        endRangeMatches.PopFront();
        continue;
      }
      auto candidate = TextDirectiveCandidate::CreateFromStartAndEndRange(
          matchStartRange, matchEndRange, mRangeContentCache);
      if (MOZ_UNLIKELY(candidate.isErr())) {
        return candidate.propagateErr();
      }
      textDirectiveMatches.AppendElement(candidate.unwrap());
    }
  }
  return textDirectiveMatches;
}

Result<nsTArray<RefPtr<nsRange>>, ErrorResult>
TextDirectiveCreator::FindAllMatchingRanges(const nsString& aSearchQuery,
                                            const RangeBoundary& aSearchEnd) {
  MOZ_ASSERT(!aSearchQuery.IsEmpty());
  ErrorResult rv;
  nsContentUtils::NodeIndexCache nodeIndexCache;
  RangeBoundary documentStart{&mDocument, 0u};
  RefPtr<nsRange> searchRange = nsRange::Create(documentStart, aSearchEnd, rv);
  if (rv.Failed()) {
    return Err(std::move(rv));
  }
  nsTArray<RefPtr<nsRange>> matchingRanges;
  size_t counter = 0;
  RangeBoundary searchStart = searchRange->StartRef();
  const RangeBoundary searchEnd = searchRange->EndRef();
  while (!searchRange->Collapsed()) {
    if (++counter % 100 == 0 && mWatchdog.IsDone()) {
      return matchingRanges;
    }
    RefPtr<nsRange> searchResult = TextDirectiveUtil::FindStringInRange(
        searchStart, searchEnd, aSearchQuery, true, true, &nodeIndexCache);
    if (!searchResult) {
      // This would mean we reached a weird edge case in which the search query
      // is not in the search range.
      break;
    }
    if (TextDirectiveUtil::NormalizedRangeBoundariesAreEqual(
            searchResult->EndRef(), searchEnd, &nodeIndexCache)) {
      // It is safe to assume that this algorithm reached the end of all
      // potential ranges if the search result is equal to the search query.
      // Therefore, this range is not added to the results.
      break;
    }
    matchingRanges.AppendElement(searchResult);
    searchStart = TextDirectiveUtil::MoveRangeBoundaryOneWord(
        searchResult->StartRef(), TextScanDirection::Right);
  }

  TEXT_FRAGMENT_LOG(
      "Found {} matches for the input '{}' in the document until the end of "
      "the input range.",
      matchingRanges.Length(), NS_ConvertUTF16toUTF8(aSearchQuery));
  return matchingRanges;
}

nsTArray<
    std::pair<TextDirectiveCandidate, nsTArray<const TextDirectiveCandidate*>>>
TextDirectiveCreator::FindMatchesForCandidates(
    nsTArray<TextDirectiveCandidate>&& aCandidates,
    const nsTArray<const TextDirectiveCandidate*>& aMatches) {
  AutoTArray<const TextDirectiveCandidateContents*, 32> candidateContents;
  candidateContents.SetCapacity(aCandidates.Length());
  for (const auto& c : aCandidates) {
    candidateContents.AppendElement(&c.RangeContents());
  }
  AutoTArray<const TextDirectiveCandidateContents*, 32> matchContents;
  matchContents.SetCapacity(aMatches.Length());
  for (const auto* m : aMatches) {
    matchContents.AppendElement(&m->RangeContents());
  }
  nsTArray<nsTArray<size_t>> matchIndices;
  fragment_directive_filter_non_matching_candidates(
      &candidateContents, &matchContents, &matchIndices);

  nsTArray<std::pair<TextDirectiveCandidate,
                     nsTArray<const TextDirectiveCandidate*>>>
      candidatesAndMatches(aCandidates.Length());
  for (size_t index = 0; index < aCandidates.Length(); ++index) {
    AutoTArray<const TextDirectiveCandidate*, 8> stillMatching;
    for (auto i : matchIndices[index]) {
      stillMatching.AppendElement(aMatches[i]);
    }
    candidatesAndMatches.AppendElement(
        std::pair(std::move(aCandidates[index]), std::move(stillMatching)));
  }
  return candidatesAndMatches;
}

Result<nsCString, ErrorResult>
TextDirectiveCreator::CreateTextDirectiveFromMatches(
    const nsTArray<TextDirectiveCandidate>& aTextDirectiveMatches) {
  if (mWatchdog.IsDone()) {
    TEXT_FRAGMENT_LOG(
        "Hitting {}s timeout.",
        StaticPrefs::dom_text_fragments_create_text_fragment_timeout_seconds());
    return nsCString{};
  }
  TextDirectiveCandidate currentCandidate = std::move(mTextDirective);
  if (aTextDirectiveMatches.IsEmpty()) {
    TEXT_FRAGMENT_LOG(
        "There are no conflicting matches. Returning text directive '{}'.",
        currentCandidate.TextDirectiveString());
    return currentCandidate.TextDirectiveString();
  }

  TEXT_FRAGMENT_LOG("Found {} text directive matches to eliminate",
                    aTextDirectiveMatches.Length());
  // To create a text directive string which points to the input range using a
  // text directive candidate `currentCandidate` and a given set of
  // fully-expanded text directive candidates `matches` representing other
  // matches of input range, follow these steps:

  // Implementation note:
  // `aTextDirectiveMatches` keeps ownership of the `TextDirectiveCandidate`s
  // during this algorithm and is const. `matches` doesn't have ownership and
  // will change its content during the following loop.
  nsTArray<const TextDirectiveCandidate*> matches(
      aTextDirectiveMatches.Length());
  for (const auto& match : aTextDirectiveMatches) {
    matches.AppendElement(&match);
  }
  // 1. While `matches` is not empty:
  size_t loopCounter = 0;
  while (!matches.IsEmpty()) {
    ++loopCounter;
    if (mWatchdog.IsDone()) {
      TEXT_FRAGMENT_LOG(
          "Hitting {}s timeout.",
          StaticPrefs::
              dom_text_fragments_create_text_fragment_timeout_seconds());
      return nsCString{};
    }
    TEXT_FRAGMENT_LOG(
        "Entering loop {}. {} matches left. Current candidate state:\n",
        loopCounter, matches.Length());
    currentCandidate.LogCurrentState(__FUNCTION__);
    if (TextDirectiveUtil::ShouldLog()) {
      TEXT_FRAGMENT_LOG("State of remaining matches:");
      for (const auto* match : matches) {
        match->LogCurrentState(__FUNCTION__);
      }
    }
    // 1.1 Let `newCandidates` be a list of text directive candidates, created
    //     by running the Create New Candidates For Matches algorithm.
    Result<nsTArray<TextDirectiveCandidate>, ErrorResult> maybeNewCandidates =
        currentCandidate.CreateNewCandidatesForMatches(matches,
                                                       mRangeContentCache);
    if (MOZ_UNLIKELY(maybeNewCandidates.isErr())) {
      return maybeNewCandidates.propagateErr();
    }
    nsTArray<TextDirectiveCandidate> newCandidates =
        maybeNewCandidates.unwrap();
    // 1.2 If `newCandidates` is empty:
    if (newCandidates.IsEmpty()) {
      TEXT_FRAGMENT_LOG(
          "It is not possible to create a text directive that matches the "
          "input string.");
      // 1.2.1 Return an empty string.
      return nsCString{};
    }

    // 1.3 Let `candidatesAndMatchesForNextIteration` be a list of tuple,
    //     where the first element is a text directive candidate and the
    //     second is a list of text directive candidates.
    // Note: This will map all elements of `matches` which would still match
    //       against the new candidate. These matches would need to be
    //       examined further in the next iteration.
    // 1.4 For each element `newCandidate` in `newCandidates`:
    // 1.4.1 Insert a new element into
    //       `candidatesAndMatchesForNextIteration`, consisting of
    //       `newCandidate` and the result of running the Filter Non-matching
    //       Candidates algorithm using `newCandidate` and `matches`.
    nsTArray<std::pair<TextDirectiveCandidate,
                       nsTArray<const TextDirectiveCandidate*>>>
        candidatesAndMatchesForNextIteration =
            FindMatchesForCandidates(std::move(newCandidates), matches);

    // 1.5 Sort the elements of `candidatesAndMatchesForNextIteration`
    //     ascending using the following criteria:
    //      1. The number of remaining matches
    //      2. If the number of remaining matches is equal, sort by the length
    //         of the created text directive string.
    candidatesAndMatchesForNextIteration.Sort(
        [](const auto& a, const auto& b) -> int {
          // sorting by the number of still matching  candidates which still
          // exist has higher priority ...
          const int difference = a.second.Length() - b.second.Length();
          if (difference != 0) {
            return difference;
          }
          // ... than sorting by the resulting text directive string for
          // elements which have the same number of matching candidates.
          return a.first.TextDirectiveString().Length() -
                 b.first.TextDirectiveString().Length();
        });
    // 1.6 Let `currentCandidate` be the first element of the first element of
    //     `candidatesAndMatchesForNextIteration`.
    // 1.7. Let `matches` be the second element of the first element of
    //     `candidatesAndMatchesForNextIteration`.
    auto& [bestCandidate, matchesForCandidate] =
        candidatesAndMatchesForNextIteration.ElementAt(0);
    currentCandidate = std::move(bestCandidate);
    matches = std::move(matchesForCandidate);
  }
  // 2. Return the text directive string of `currentCandidate`.
  return currentCandidate.TextDirectiveString();
}

}  // namespace mozilla::dom
