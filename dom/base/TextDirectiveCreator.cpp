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

}  // namespace mozilla::dom
