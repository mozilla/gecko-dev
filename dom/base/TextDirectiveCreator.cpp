/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TextDirectiveCreator.h"
#include "TextDirectiveUtil.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/ResultVariant.h"
#include "nsRange.h"

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
  return TextDirectiveCandidate{startRange,   fullStartRange, endRange,
                                fullEndRange, prefixRange,    fullPrefixRange,
                                suffixRange,  fullSuffixRange};
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

  return nsCString{};
}
}  // namespace mozilla::dom
