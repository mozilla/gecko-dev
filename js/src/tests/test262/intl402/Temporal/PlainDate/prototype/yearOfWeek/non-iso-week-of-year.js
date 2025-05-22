// |reftest| shell-option(--enable-temporal) skip-if(!this.hasOwnProperty('Temporal')||!xulRuntime.shell) -- Temporal is not enabled unconditionally, requires shell-options
// Copyright (C) 2024 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindate.prototype.yearofweek
description: >
  Temporal.PlainDate.prototype.yearOfWeek returns undefined for all
  non-ISO calendars without a well-defined week numbering system.
features: [Temporal]
---*/

assert.sameValue(
  new Temporal.PlainDate(2024, 1, 1, "gregory").yearOfWeek,
  undefined,
  "Gregorian calendar does not provide week numbers"
);

assert.sameValue(
  new Temporal.PlainDate(2024, 1, 1, "hebrew").yearOfWeek,
  undefined,
  "Hebrew calendar does not provide week numbers"
);

reportCompare(0, 0);
