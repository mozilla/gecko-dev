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
  return nsCString{};
}
}  // namespace mozilla::dom
