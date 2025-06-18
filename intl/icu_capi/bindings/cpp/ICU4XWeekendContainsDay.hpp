#ifndef ICU4XWeekendContainsDay_HPP
#define ICU4XWeekendContainsDay_HPP
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <algorithm>
#include <memory>
#include <variant>
#include <optional>
#include "diplomat_runtime.hpp"

#include "ICU4XWeekendContainsDay.h"



/**
 * Documents which days of the week are considered to be a part of the weekend
 * 
 * See the [Rust documentation for `weekend`](https://docs.rs/icu/latest/icu/calendar/week/struct.WeekCalculator.html#method.weekend) for more information.
 */
struct ICU4XWeekendContainsDay {
 public:
  bool monday;
  bool tuesday;
  bool wednesday;
  bool thursday;
  bool friday;
  bool saturday;
  bool sunday;
};


#endif
