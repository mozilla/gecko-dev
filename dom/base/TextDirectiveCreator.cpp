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

}  // namespace mozilla::dom
