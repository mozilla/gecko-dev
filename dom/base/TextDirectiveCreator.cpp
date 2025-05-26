/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TextDirectiveCreator.h"
#include "AbstractRange.h"
#include "StaticRange.h"
#include "TextDirectiveUtil.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/ResultVariant.h"
#include "nsINode.h"
#include "nsRange.h"
#include "Document.h"

namespace mozilla::dom {

TextDirectiveCreator::TextDirectiveCreator(Document& aDocument,
                                           AbstractRange* aRange)
    : mDocument(aDocument), mRange(aRange) {}

/* static */
mozilla::Result<nsCString, ErrorResult>
TextDirectiveCreator::CreateTextDirectiveFromRange(Document& aDocument,
                                                   AbstractRange* aInputRange) {
  MOZ_ASSERT(aInputRange);
  MOZ_ASSERT(!aInputRange->Collapsed());
  auto maybeRangeContent = TextDirectiveUtil::RangeContentAsString(aInputRange);
  if (MOZ_UNLIKELY(maybeRangeContent.isErr())) {
    return maybeRangeContent.propagateErr();
  }
  auto rangeContent = maybeRangeContent.unwrap();
  if (rangeContent.IsEmpty()) {
    TEXT_FRAGMENT_LOG("Input range does not contain text.");
    return VoidCString();
  }

  Result<RefPtr<AbstractRange>, ErrorResult> maybeRange =
      ExtendRangeToBlockBoundaries(aInputRange);
  if (MOZ_UNLIKELY(maybeRange.isErr())) {
    return maybeRange.propagateErr();
  }
  RefPtr range = maybeRange.unwrap();
  if (!range) {
    return VoidCString();
  }
  return CreateInstance(aDocument, range)
      .andThen([](auto self) -> Result<nsCString, ErrorResult> {
        MOZ_TRY(self->CollectContextTerms());
        self->CollectContextTermWordBoundaryDistances();
        MOZ_TRY(self->FindAllMatchingCandidates());
        return VoidCString();
      });
}

/* static */ Result<bool, ErrorResult>
TextDirectiveCreator::MustUseRangeBasedMatching(AbstractRange* aRange) {
  MOZ_ASSERT(aRange);
  if (TextDirectiveUtil::FindBlockBoundaryInRange<TextScanDirection::Right>(
          *aRange)
          .isSome()) {
    TEXT_FRAGMENT_LOG(
        "Use range-based matching because the target range contains a block "
        "boundary.");
    return true;
  }
  return TextDirectiveUtil::RangeContentAsString(aRange).andThen(
      [](const nsString& content) -> Result<bool, ErrorResult> {
        const uint32_t MAX_LENGTH = StaticPrefs::
            dom_text_fragments_create_text_fragment_exact_match_max_length();
        const bool rangeTooLong = content.Length() > MAX_LENGTH;
        if (rangeTooLong) {
          TEXT_FRAGMENT_LOG(
              "Use range-based matching becase the target range is too long "
              "({} chars > {} threshold)",
              content.Length(), MAX_LENGTH);
        } else {
          TEXT_FRAGMENT_LOG("Use exact matching.");
        }
        return rangeTooLong;
      });
}

Result<UniquePtr<TextDirectiveCreator>, ErrorResult>
TextDirectiveCreator::CreateInstance(Document& aDocument,
                                     AbstractRange* aRange) {
  return MustUseRangeBasedMatching(aRange).andThen(
      [&aDocument, aRange](bool useRangeBased)
          -> Result<UniquePtr<TextDirectiveCreator>, ErrorResult> {
        return useRangeBased
                   ? UniquePtr<TextDirectiveCreator>(
                         new RangeBasedTextDirectiveCreator(aDocument, aRange))
                   : UniquePtr<TextDirectiveCreator>(
                         new ExactMatchTextDirectiveCreator(aDocument, aRange));
      });
}

/*static*/
Result<RefPtr<AbstractRange>, ErrorResult>
TextDirectiveCreator::ExtendRangeToBlockBoundaries(AbstractRange* aRange) {
  MOZ_ASSERT(aRange && !aRange->Collapsed());
  TEXT_FRAGMENT_LOG(
      "Input range :\n{}",
      NS_ConvertUTF16toUTF8(
          TextDirectiveUtil::RangeContentAsString(aRange).unwrapOr(
              u"<Could not be converted to string>"_ns)));
  ErrorResult rv;
  RangeBoundary startPoint = TextDirectiveUtil::FindNextNonWhitespacePosition<
      TextScanDirection::Right>(aRange->StartRef());
  startPoint =
      TextDirectiveUtil::FindWordBoundary<TextScanDirection::Left>(startPoint);

  RangeBoundary endPoint =
      TextDirectiveUtil::FindNextNonWhitespacePosition<TextScanDirection::Left>(
          aRange->EndRef());
  endPoint =
      TextDirectiveUtil::FindWordBoundary<TextScanDirection::Right>(endPoint);
#if MOZ_DIAGNOSTIC_ASSERT_ENABLED
  auto cmp = nsContentUtils::ComparePoints(startPoint, endPoint);
  MOZ_DIAGNOSTIC_ASSERT(
      cmp && *cmp != 1,
      "The new end point must not be before the start point.");
#endif

  if (startPoint.IsSetAndValid() && endPoint.IsSetAndValid()) {
    ErrorResult rv;
    RefPtr<AbstractRange> range = StaticRange::Create(startPoint, endPoint, rv);
    if (MOZ_UNLIKELY(rv.Failed())) {
      return Err(std::move(rv));
    }
    MOZ_ASSERT(range);
    if (!range->Collapsed()) {
      TEXT_FRAGMENT_LOG(
          "Expanded target range to word boundaries:\n{}",
          NS_ConvertUTF16toUTF8(
              TextDirectiveUtil::RangeContentAsString(range).unwrapOr(
                  u"<Could not be converted to string>"_ns)));

      return range;
    }
  }
  TEXT_FRAGMENT_LOG("Extending to word boundaries collapsed the range.");
  return Result<RefPtr<AbstractRange>, ErrorResult>(nullptr);
}

Result<Ok, ErrorResult> ExactMatchTextDirectiveCreator::CollectContextTerms() {
  MOZ_ASSERT(mRange);
  if (MOZ_UNLIKELY(mRange->Collapsed())) {
    return Ok();
  }
  TEXT_FRAGMENT_LOG("Collecting context terms for the target range.");
  MOZ_TRY(CollectPrefixContextTerm());
  MOZ_TRY(CollectSuffixContextTerm());
  MOZ_TRY(TextDirectiveUtil::RangeContentAsString(mRange).andThen(
      [start =
           &mStartContent](const nsString& content) -> Result<Ok, ErrorResult> {
        *start = content;
        return Ok();
      }));
  mStartFoldCaseContent = mStartContent;
  ToFoldedCase(mStartFoldCaseContent);
  TEXT_FRAGMENT_LOG("Start term:\n{}", NS_ConvertUTF16toUTF8(mStartContent));
  TEXT_FRAGMENT_LOG("No end term present (exact match).");
  return Ok();
}

Result<Ok, ErrorResult> RangeBasedTextDirectiveCreator::CollectContextTerms() {
  MOZ_ASSERT(mRange);
  if (MOZ_UNLIKELY(mRange->Collapsed())) {
    return Ok();
  }
  TEXT_FRAGMENT_LOG("Collecting context terms for the target range.");
  MOZ_TRY(CollectPrefixContextTerm());
  MOZ_TRY(CollectSuffixContextTerm());
  if (const Maybe<RangeBoundary> firstBlockBoundaryInRange =
          TextDirectiveUtil::FindBlockBoundaryInRange<TextScanDirection::Right>(
              *mRange)) {
    ErrorResult rv;
    RefPtr startRange =
        StaticRange::Create(mRange->StartRef(), *firstBlockBoundaryInRange, rv);
    if (MOZ_UNLIKELY(rv.Failed())) {
      return Err(std::move(rv));
    }
    MOZ_DIAGNOSTIC_ASSERT(!startRange->Collapsed());
    MOZ_TRY(TextDirectiveUtil::RangeContentAsString(startRange)
                .andThen([start = &mStartContent](const nsString& content)
                             -> Result<Ok, ErrorResult> {
                  *start = content;
                  return Ok();
                }));
    const Maybe<RangeBoundary> lastBlockBoundaryInRange =
        TextDirectiveUtil::FindBlockBoundaryInRange<TextScanDirection::Left>(
            *mRange);
    MOZ_DIAGNOSTIC_ASSERT(
        lastBlockBoundaryInRange.isSome(),
        "If the target range contains a block boundary looking left-to-right, "
        "it must also contain one looking right-to-left");
    RefPtr endRange =
        StaticRange::Create(*lastBlockBoundaryInRange, mRange->EndRef(), rv);
    if (MOZ_UNLIKELY(rv.Failed())) {
      return Err(std::move(rv));
    }
    MOZ_DIAGNOSTIC_ASSERT(!endRange->Collapsed());
    MOZ_TRY(TextDirectiveUtil::RangeContentAsString(endRange).andThen(
        [end =
             &mEndContent](const nsString& content) -> Result<Ok, ErrorResult> {
          *end = content;
          return Ok();
        }));
  } else {
    const uint32_t kMaxLength = StaticPrefs::
        dom_text_fragments_create_text_fragment_exact_match_max_length();
    MOZ_TRY(TextDirectiveUtil::RangeContentAsString(mRange).andThen(
        [start = &mStartContent](
            const nsString& content) -> Result<Ok, ErrorResult> {
          *start = content;
          return Ok();
        }));
    MOZ_DIAGNOSTIC_ASSERT(mStartContent.Length() > kMaxLength);
    const auto [wordStart, wordEnd] =
        intl::WordBreaker::FindWord(mStartContent, mStartContent.Length() / 2);
    mEndContent = Substring(mStartContent, wordEnd);
    mStartContent = Substring(mStartContent, 0, wordEnd);
  }
  mStartFoldCaseContent = mStartContent;
  ToFoldedCase(mStartFoldCaseContent);
  TEXT_FRAGMENT_LOG("Maximum possible start term:\n{}",
                    NS_ConvertUTF16toUTF8(mStartContent));
  mEndFoldCaseContent = mEndContent;
  ToFoldedCase(mEndFoldCaseContent);
  TEXT_FRAGMENT_LOG("Maximum possible end term:\n{}",
                    NS_ConvertUTF16toUTF8(mEndContent));
  return Ok();
}

Result<Ok, ErrorResult> TextDirectiveCreator::CollectPrefixContextTerm() {
  ErrorResult rv;
  RangeBoundary prefixEnd =
      TextDirectiveUtil::FindNextNonWhitespacePosition<TextScanDirection::Left>(
          mRange->StartRef());
  RangeBoundary prefixStart =
      TextDirectiveUtil::FindNextBlockBoundary<TextScanDirection::Left>(
          prefixEnd);
  RefPtr prefixRange = StaticRange::Create(prefixStart, prefixEnd, rv);
  if (MOZ_UNLIKELY(rv.Failed())) {
    return Err(std::move(rv));
  }
  MOZ_ASSERT(prefixRange);
  MOZ_TRY(TextDirectiveUtil::RangeContentAsString(prefixRange)
              .andThen([prefix = &mPrefixContent](
                           const nsString& content) -> Result<Ok, ErrorResult> {
                *prefix = content;
                return Ok();
              }));
  mPrefixFoldCaseContent = mPrefixContent;
  ToFoldedCase(mPrefixFoldCaseContent);
  TEXT_FRAGMENT_LOG("Maximum possible prefix term:\n{}",
                    NS_ConvertUTF16toUTF8(mPrefixContent));
  return Ok();
}

Result<Ok, ErrorResult> TextDirectiveCreator::CollectSuffixContextTerm() {
  ErrorResult rv;
  RangeBoundary suffixBegin = TextDirectiveUtil::FindNextNonWhitespacePosition<
      TextScanDirection::Right>(mRange->EndRef());
  RangeBoundary suffixEnd =
      TextDirectiveUtil::FindNextBlockBoundary<TextScanDirection::Right>(
          suffixBegin);
  RefPtr suffixRange = StaticRange::Create(suffixBegin, suffixEnd, rv);
  if (MOZ_UNLIKELY(rv.Failed())) {
    return Err(std::move(rv));
  }
  MOZ_ASSERT(suffixRange);
  MOZ_TRY(TextDirectiveUtil::RangeContentAsString(suffixRange)
              .andThen([suffix = &mSuffixContent](
                           const nsString& content) -> Result<Ok, ErrorResult> {
                *suffix = content;
                return Ok();
              }));
  mSuffixFoldCaseContent = mSuffixContent;
  ToFoldedCase(mSuffixFoldCaseContent);
  TEXT_FRAGMENT_LOG("Maximum possible suffix term:\n{}",
                    NS_ConvertUTF16toUTF8(mSuffixContent));
  return Ok();
}

void ExactMatchTextDirectiveCreator::CollectContextTermWordBoundaryDistances() {
  mPrefixWordBeginDistances =
      TextDirectiveUtil::ComputeWordBoundaryDistances<TextScanDirection::Left>(
          mPrefixContent);
  TEXT_FRAGMENT_LOG("Word begin distances for prefix term: {}",
                    mPrefixWordBeginDistances);
  mSuffixWordEndDistances =
      TextDirectiveUtil::ComputeWordBoundaryDistances<TextScanDirection::Right>(
          mSuffixContent);
  TEXT_FRAGMENT_LOG("Word end distances for suffix term: {}",
                    mSuffixWordEndDistances);
}

void RangeBasedTextDirectiveCreator::CollectContextTermWordBoundaryDistances() {
  mPrefixWordBeginDistances =
      TextDirectiveUtil::ComputeWordBoundaryDistances<TextScanDirection::Left>(
          mPrefixContent);
  TEXT_FRAGMENT_LOG("Word begin distances for prefix term: {}",
                    mPrefixWordBeginDistances);
  mStartWordEndDistances =
      TextDirectiveUtil::ComputeWordBoundaryDistances<TextScanDirection::Right>(
          mStartContent);
  TEXT_FRAGMENT_LOG("Word end distances for start term: {}",
                    mStartWordEndDistances);
  mEndWordBeginDistances =
      TextDirectiveUtil::ComputeWordBoundaryDistances<TextScanDirection::Left>(
          mEndContent);
  TEXT_FRAGMENT_LOG("Word begin distances for end term: {}",
                    mEndWordBeginDistances);
  mSuffixWordEndDistances =
      TextDirectiveUtil::ComputeWordBoundaryDistances<TextScanDirection::Right>(
          mSuffixContent);
  TEXT_FRAGMENT_LOG("Word end distances for suffix term: {}",
                    mSuffixWordEndDistances);
}

Result<nsTArray<RefPtr<AbstractRange>>, ErrorResult>
TextDirectiveCreator::FindAllMatchingRanges(const nsString& aSearchQuery,
                                            const RangeBoundary& aSearchStart,
                                            const RangeBoundary& aSearchEnd) {
  MOZ_ASSERT(!aSearchQuery.IsEmpty());
  RangeBoundary searchStart = aSearchStart;
  nsTArray<RefPtr<AbstractRange>> matchingRanges;

  while (true) {
    if (mWatchdog.IsDone()) {
      return matchingRanges;
    }
    RefPtr<AbstractRange> searchResult = TextDirectiveUtil::FindStringInRange(
        searchStart, aSearchEnd, aSearchQuery, true, true, &mNodeIndexCache);
    if (!searchResult || searchResult->Collapsed()) {
      break;
    }
    searchStart = searchResult->StartRef();
    if (auto cmp = nsContentUtils::ComparePoints(searchStart, aSearchEnd,
                                                 &mNodeIndexCache);
        !cmp || *cmp != -1) {
      // this means hitting a bug in nsFind which apparently does not stop
      // exactly where it is told to. There are cases where it might
      // overshoot, e.g. if `aSearchEnd` is  a text node with offset=0.
      // However, due to reusing the cache used by nsFind this additional call
      // to ComparePoints should be very cheap.
      break;
    }
    matchingRanges.AppendElement(searchResult);

    MOZ_DIAGNOSTIC_ASSERT(searchResult->GetStartContainer()->IsText());
    auto newSearchStart =
        TextDirectiveUtil::MoveToNextBoundaryPoint(searchStart);
    MOZ_DIAGNOSTIC_ASSERT(newSearchStart != searchStart);
    searchStart = newSearchStart;
    if (auto cmp = nsContentUtils::ComparePoints(searchStart, aSearchEnd,
                                                 &mNodeIndexCache);
        !cmp || *cmp != -1) {
      break;
    }
  }

  TEXT_FRAGMENT_LOG(
      "Found {} matches for the input '{}' in the partial document.",
      matchingRanges.Length(), NS_ConvertUTF16toUTF8(aSearchQuery));
  return matchingRanges;
}

Result<Ok, ErrorResult>
ExactMatchTextDirectiveCreator::FindAllMatchingCandidates() {
  if (MOZ_UNLIKELY(mRange->Collapsed())) {
    return Ok();
  }

  TEXT_FRAGMENT_LOG(
      "Searching all occurrences of range content ({}) in the partial document "
      "from document begin to begin of target range.",
      NS_ConvertUTF16toUTF8(mStartContent));
  return FindAllMatchingRanges(mStartContent, {&mDocument, 0u},
                               mRange->StartRef())
      .andThen([this](const nsTArray<RefPtr<AbstractRange>>& matchRanges)
                   -> Result<Ok, ErrorResult> {
        FindCommonSubstringLengths(matchRanges);
        return Ok();
      });
}

void ExactMatchTextDirectiveCreator::FindCommonSubstringLengths(
    const nsTArray<RefPtr<AbstractRange>>& aMatchRanges) {
  if (mWatchdog.IsDone()) {
    return;
  }
  size_t loopCounter = 0;
  for (const auto& range : aMatchRanges) {
    TEXT_FRAGMENT_LOG("Computing common prefix substring length for match {}.",
                      ++loopCounter);
    const uint32_t commonPrefixLength =
        TextDirectiveUtil::ComputeCommonSubstringLength<
            TextScanDirection::Left>(
            mPrefixFoldCaseContent,
            TextDirectiveUtil::FindNextNonWhitespacePosition<
                TextScanDirection::Left>(range->StartRef()));

    TEXT_FRAGMENT_LOG("Computing common suffix substring length for match {}.",
                      loopCounter);
    const uint32_t commonSuffixLength =
        TextDirectiveUtil::ComputeCommonSubstringLength<
            TextScanDirection::Right>(
            mSuffixFoldCaseContent,
            TextDirectiveUtil::FindNextNonWhitespacePosition<
                TextScanDirection::Right>(range->EndRef()));

    mCommonSubstringLengths.EmplaceBack(commonPrefixLength, commonSuffixLength);
  }
}

Result<Ok, ErrorResult>
RangeBasedTextDirectiveCreator::FindAllMatchingCandidates() {
  nsString firstWordOfStartContent(
      Substring(mStartContent, 0, mStartWordEndDistances[0]));
  nsString lastWordOfEndContent(
      Substring(mEndContent, mEndContent.Length() - mEndWordBeginDistances[0]));

  TEXT_FRAGMENT_LOG(
      "Searching all occurrences of first word of start content ({}) in the "
      "partial document from document begin to begin of the target range.",
      NS_ConvertUTF16toUTF8(firstWordOfStartContent));

  MOZ_TRY(
      FindAllMatchingRanges(firstWordOfStartContent, {&mDocument, 0u},
                            mRange->StartRef())
          .andThen([this](const nsTArray<RefPtr<AbstractRange>>& ranges)
                       -> Result<Ok, ErrorResult> {
            FindStartMatchCommonSubstringLengths(ranges);
            return Ok();
          }));
  if (mWatchdog.IsDone()) {
    return Ok();
  }
  TEXT_FRAGMENT_LOG(
      "Searching all occurrences of last word of end content ({}) in the "
      "partial document from beginning of the target range to the end of the "
      "target range, excluding the last word.",
      NS_ConvertUTF16toUTF8(lastWordOfEndContent));

  auto searchEnd =
      TextDirectiveUtil::FindNextNonWhitespacePosition<TextScanDirection::Left>(
          mRange->EndRef());
  searchEnd =
      TextDirectiveUtil::FindWordBoundary<TextScanDirection::Left>(searchEnd);

  return FindAllMatchingRanges(lastWordOfEndContent, mRange->StartRef(),
                               searchEnd)
      .andThen([self = this](const nsTArray<RefPtr<AbstractRange>>& ranges)
                   -> Result<Ok, ErrorResult> {
        self->FindEndMatchCommonSubstringLengths(ranges);
        return Ok();
      });
}

void RangeBasedTextDirectiveCreator::FindStartMatchCommonSubstringLengths(
    const nsTArray<RefPtr<AbstractRange>>& aMatchRanges) {
  size_t loopCounter = 0;
  for (const auto& range : aMatchRanges) {
    ++loopCounter;
    TEXT_FRAGMENT_LOG(
        "Computing common prefix substring length for start match {}.",
        loopCounter);
    const uint32_t commonPrefixLength =
        TextDirectiveUtil::ComputeCommonSubstringLength<
            TextScanDirection::Left>(
            mPrefixFoldCaseContent,
            TextDirectiveUtil::FindNextNonWhitespacePosition<
                TextScanDirection::Left>(range->StartRef()));

    TEXT_FRAGMENT_LOG(
        "Computing common start substring length for start match {}.",
        loopCounter);
    const uint32_t commonStartLength =
        TextDirectiveUtil::ComputeCommonSubstringLength<
            TextScanDirection::Right>(mStartFoldCaseContent, range->StartRef());
    const uint32_t commonStartLengthWithoutFirstWord =
        std::max(0, int(commonStartLength - mStartWordEndDistances[0]));
    TEXT_FRAGMENT_LOG("Ignoring first word ({}). Remaining common length: {}",
                      NS_ConvertUTF16toUTF8(Substring(
                          mStartContent, 0, mStartWordEndDistances[0])),
                      commonStartLengthWithoutFirstWord);
    mStartMatchCommonSubstringLengths.EmplaceBack(
        commonPrefixLength, commonStartLengthWithoutFirstWord);
  }
}

void RangeBasedTextDirectiveCreator::FindEndMatchCommonSubstringLengths(
    const nsTArray<RefPtr<AbstractRange>>& aMatchRanges) {
  size_t loopCounter = 0;
  for (const auto& range : aMatchRanges) {
    ++loopCounter;
    TEXT_FRAGMENT_LOG("Computing common end substring length for end match {}.",
                      loopCounter);
    const uint32_t commonEndLength =
        TextDirectiveUtil::ComputeCommonSubstringLength<
            TextScanDirection::Left>(mEndFoldCaseContent, range->EndRef());
    const uint32_t commonEndLengthWithoutLastWord =
        std::max(0, int(commonEndLength - mEndWordBeginDistances[0]));
    TEXT_FRAGMENT_LOG(
        "Ignoring last word ({}). Remaining common length: {}",
        NS_ConvertUTF16toUTF8(Substring(
            mEndContent, mEndContent.Length() - mEndWordBeginDistances[0])),
        commonEndLengthWithoutLastWord);
    TEXT_FRAGMENT_LOG(
        "Computing common suffix substring length for end match {}.",
        loopCounter);
    const uint32_t commonSuffixLength =
        TextDirectiveUtil::ComputeCommonSubstringLength<
            TextScanDirection::Right>(
            mSuffixFoldCaseContent,
            TextDirectiveUtil::FindNextNonWhitespacePosition<
                TextScanDirection::Right>(range->EndRef()));

    mEndMatchCommonSubstringLengths.EmplaceBack(commonEndLengthWithoutLastWord,
                                                commonSuffixLength);
  }
}

}  // namespace mozilla::dom
