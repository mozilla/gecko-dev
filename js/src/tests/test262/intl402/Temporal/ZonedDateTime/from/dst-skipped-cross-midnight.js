// |reftest| shell-option(--enable-temporal) skip-if(!this.hasOwnProperty('Temporal')||!xulRuntime.shell) -- Temporal is not enabled unconditionally, requires shell-options
// Copyright (C) 2024 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-temporal.zoneddatetime.from
description: Test TZDB edge case where start of day is not 00:00 nor 01:00
includes: [temporalHelpers.js]
features: [Temporal]
---*/

// DST spring-forward hour skipped from 1919-03-30T23:30 to 1919-03-31T00:30, so
// day starts at 00:30
const startOfDay = Temporal.ZonedDateTime.from("1919-03-31[America/Toronto]");
const midnightDisambiguated = Temporal.ZonedDateTime.from("1919-03-31T00[America/Toronto]");
TemporalHelpers.assertDuration(
  startOfDay.until(midnightDisambiguated),
  0, 0, 0, 0, 0, /* minutes = */ 30, 0, 0, 0, 0,
  "start of day is 30 minutes earlier than following the disambiguation strategy for midnight"
);

assert.sameValue(
  midnightDisambiguated.epochNanoseconds,
  Temporal.ZonedDateTime.from({ year: 1919, month: 3, day: 31, timeZone: "America/Toronto" }).epochNanoseconds,
  "start of day magic doesn't happen with property bag, missing properties are zero"
);

reportCompare(0, 0);
