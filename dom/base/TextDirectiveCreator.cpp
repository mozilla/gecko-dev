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
TextDirectiveCandidate::CreateFromInputRange(const nsRange* aInputRange) {
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
  MOZ_TRY(instance.CreateTextDirectiveString());
  return instance;
}

/* static */
Result<TextDirectiveCandidate, ErrorResult>
TextDirectiveCandidate::CreateFromStartAndEndRange(const nsRange* aStartRange,
                                                   const nsRange* aEndRange) {
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
  MOZ_TRY(instance.CreateTextDirectiveString());
  return instance;
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

TextDirectiveCreator::TextDirectiveCreator(
    Document& aDocument, nsRange* aInputRange,
    TextDirectiveCandidate&& aTextDirective)
    : mDocument(aDocument),
      mInputRange(aInputRange),
      mTextDirective(std::move(aTextDirective)) {}

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
  Result<TextDirectiveCandidate, ErrorResult> maybeTextDirectiveCandidate =
      TextDirectiveCandidate::CreateFromInputRange(
          inputRangeExtendedToWordBoundaries);
  if (MOZ_UNLIKELY(maybeTextDirectiveCandidate.isErr())) {
    return maybeTextDirectiveCandidate.propagateErr();
  }
  auto textDirectiveCandidate = maybeTextDirectiveCandidate.unwrap();

  TextDirectiveCreator creator(aDocument, inputRangeExtendedToWordBoundaries,
                               std::move(textDirectiveCandidate));

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

  if (mTextDirective.UseExactMatch()) {
    Result<nsString, ErrorResult> rangeContent =
        TextDirectiveUtil::RangeContentAsString(mInputRange);
    if (MOZ_UNLIKELY(rangeContent.isErr())) {
      return rangeContent.propagateErr();
    }
    Result<nsTArray<RefPtr<nsRange>>, ErrorResult> maybeRangeMatches =
        FindAllMatchingRanges(rangeContent.unwrap());
    if (MOZ_UNLIKELY(maybeRangeMatches.isErr())) {
      return maybeRangeMatches.propagateErr();
    }
    auto rangeMatches = maybeRangeMatches.unwrap();
    nsTArray<TextDirectiveCandidate> textDirectiveMatches(
        rangeMatches.Length());
    for (const auto& rangeMatch : rangeMatches) {
      auto candidate = TextDirectiveCandidate::CreateFromInputRange(rangeMatch);
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
      FindAllMatchingRanges(startRangeContent.unwrap());
  if (MOZ_UNLIKELY(maybeStartRangeMatches.isErr())) {
    return maybeStartRangeMatches.propagateErr();
  }
  auto startRangeMatches = maybeStartRangeMatches.unwrap();

  Result<nsString, ErrorResult> endRangeContent =
      TextDirectiveUtil::RangeContentAsString(mTextDirective.EndRange());
  if (MOZ_UNLIKELY(endRangeContent.isErr())) {
    return endRangeContent.propagateErr();
  }
  Result<nsTArray<RefPtr<nsRange>>, ErrorResult> maybeEndRangeMatches =
      FindAllMatchingRanges(endRangeContent.unwrap());
  if (MOZ_UNLIKELY(maybeEndRangeMatches.isErr())) {
    return maybeEndRangeMatches.propagateErr();
  }
  nsDeque<nsRange> endRangeMatches;
  for (auto& element : maybeEndRangeMatches.unwrap()) {
    endRangeMatches.Push(element.get());
  }
  nsTArray<TextDirectiveCandidate> textDirectiveCandidates(
      startRangeMatches.Length());
  for (const auto& matchStartRange : startRangeMatches) {
    for (auto* matchEndRange : endRangeMatches) {
      Maybe<int32_t> compare = nsContentUtils::ComparePoints(
          matchStartRange->EndRef(), matchEndRange->StartRef());
      if (!compare || *compare == -1) {
        endRangeMatches.PopFront();
        continue;
      }
      auto candidate = TextDirectiveCandidate::CreateFromStartAndEndRange(
          matchStartRange, matchEndRange);
      if (MOZ_UNLIKELY(candidate.isErr())) {
        return candidate.propagateErr();
      }
      textDirectiveCandidates.AppendElement(candidate.unwrap());
    }
  }
  return textDirectiveCandidates;
}

Result<nsTArray<RefPtr<nsRange>>, ErrorResult>
TextDirectiveCreator::FindAllMatchingRanges(const nsString& aSearchQuery) {
  MOZ_ASSERT(!aSearchQuery.IsEmpty());
  ErrorResult rv;
  RangeBoundary documentStart{&mDocument, 0u};
  RefPtr<nsRange> searchRange =
      nsRange::Create(documentStart, mInputRange->EndRef(), rv);
  if (rv.Failed()) {
    return Err(std::move(rv));
  }
  nsTArray<RefPtr<nsRange>> matchingRanges;
  while (!searchRange->Collapsed()) {
    RefPtr<nsRange> searchResult = TextDirectiveUtil::FindStringInRange(
        searchRange, aSearchQuery, true, true);
    if (!searchResult) {
      // This would mean we reached a weird edge case in which the search query
      // is not in the search range.
      break;
    }
    if (TextDirectiveUtil::NormalizedRangeBoundariesAreEqual(
            searchResult->EndRef(), mInputRange->EndRef())) {
      // It is safe to assume that this algorithm reached the end of all
      // potential ranges if the search result is equal to the search query.
      // Therefore, this range is not added to the results.
      break;
    }
    matchingRanges.AppendElement(searchResult);
    RangeBoundary newStartBoundary =
        TextDirectiveUtil::MoveRangeBoundaryOneWord(searchResult->StartRef(),
                                                    TextScanDirection::Right);

    searchRange->SetStart(newStartBoundary.AsRaw(), rv);
  }

  TEXT_FRAGMENT_LOG(
      "Found %zu matches for the input '%s' in the document until the end of "
      "the input range.",
      matchingRanges.Length(), NS_ConvertUTF16toUTF8(aSearchQuery).Data());
  return matchingRanges;
}

Result<nsCString, ErrorResult>
TextDirectiveCreator::CreateTextDirectiveFromMatches(
    const nsTArray<TextDirectiveCandidate>& aTextDirectiveMatches) {
  TextDirectiveCandidate currentCandidate = std::move(mTextDirective);
  if (aTextDirectiveMatches.IsEmpty()) {
    TEXT_FRAGMENT_LOG(
        "There are no conflicting matches. Returning text directive '%s'.",
        currentCandidate.TextDirectiveString().Data());
    return currentCandidate.TextDirectiveString();
  }
  return nsCString{};
}

}  // namespace mozilla::dom
