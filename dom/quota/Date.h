/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_QUOTA_DATE_H_
#define DOM_QUOTA_DATE_H_

#include "mozilla/CheckedInt.h"
#include "prtime.h"

namespace mozilla::dom::quota {

namespace {

const int64_t kSecPerDay = 86400;

}

/**
 * A lightweight utility class representing a date as the number of days since
 * the Unix epoch (1970-01-01 UTC).
 *
 * This class is useful when full timestamp precision is not needed and only
 * a compact representation is required, such as when storing the value in an
 * int32_t field. An int32_t can safely represent dates out to the year ~5.8
 * million, making this format ideal for tracking coarse-grained time values
 * like origin maintenance dates, and similar use cases.
 *
 * Internally, the date is derived from PR_Now(), which returns microseconds
 * since the epoch. This ensures consistency with other quota-related timestamp
 * logic, such as origin last access time.
 */
class Date final {
 public:
  static Date FromDays(int32_t aValue) { return Date(aValue); }

  static Date FromTimestamp(int64_t aTimestamp) {
    CheckedInt32 value =
        (CheckedInt64(aTimestamp) / PR_USEC_PER_SEC / kSecPerDay)
            .toChecked<int32_t>();
    MOZ_ASSERT(value.isValid());

    return Date(value.value());
  }

  static Date Today() { return Date(FromTimestamp(PR_Now())); }

  int32_t ToDays() const { return mValue; }

  bool operator==(const Date& aOther) const { return mValue == aOther.mValue; }
  bool operator!=(const Date& aOther) const { return mValue != aOther.mValue; }
  bool operator<(const Date& aOther) const { return mValue < aOther.mValue; }
  bool operator<=(const Date& aOther) const { return mValue <= aOther.mValue; }
  bool operator>(const Date& aOther) const { return mValue > aOther.mValue; }
  bool operator>=(const Date& aOther) const { return mValue >= aOther.mValue; }

 private:
  explicit Date(int32_t aValue) : mValue(aValue) {}

  int32_t mValue;
};

}  // namespace mozilla::dom::quota

#endif  // DOM_QUOTA_DATE_H_
