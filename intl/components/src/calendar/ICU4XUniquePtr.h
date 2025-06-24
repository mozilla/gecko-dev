/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef intl_components_calendar_ICU4XUniquePtr_h_
#define intl_components_calendar_ICU4XUniquePtr_h_

#include "mozilla/UniquePtr.h"

#include "icu4x/Calendar.hpp"
#include "icu4x/Date.hpp"
#include "icu4x/IsoDate.hpp"

namespace mozilla::intl::calendar {

class ICU4XCalendarDeleter {
 public:
  void operator()(icu4x::capi::Calendar* ptr) {
    icu4x::capi::icu4x_Calendar_destroy_mv1(ptr);
  }
};

using UniqueICU4XCalendar =
    mozilla::UniquePtr<icu4x::capi::Calendar, ICU4XCalendarDeleter>;

class ICU4XDateDeleter {
 public:
  void operator()(icu4x::capi::Date* ptr) {
    icu4x::capi::icu4x_Date_destroy_mv1(ptr);
  }
};

using UniqueICU4XDate = mozilla::UniquePtr<icu4x::capi::Date, ICU4XDateDeleter>;

class ICU4XIsoDateDeleter {
 public:
  void operator()(icu4x::capi::IsoDate* ptr) {
    icu4x::capi::icu4x_IsoDate_destroy_mv1(ptr);
  }
};

using UniqueICU4XIsoDate =
    mozilla::UniquePtr<icu4x::capi::IsoDate, ICU4XIsoDateDeleter>;

}  // namespace mozilla::intl::calendar

#endif
