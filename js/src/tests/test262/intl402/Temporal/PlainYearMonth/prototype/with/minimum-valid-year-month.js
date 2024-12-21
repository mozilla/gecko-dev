// |reftest| shell-option(--enable-temporal) skip-if(!this.hasOwnProperty('Temporal')||!xulRuntime.shell) -- Temporal is not enabled unconditionally, requires shell-options
// Copyright (C) 2024 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainyearmonth.prototype.with
description: Can set the minimum valid year-month for non-ISO8601 calendars.
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const apr2000 = new Temporal.PlainYearMonth(2000, 4, "gregory");

TemporalHelpers.assertPlainYearMonth(apr2000.with({year: -271821}),
                                     -271821, 4, "M04", "", "gregory-inverse", 271822);

reportCompare(0, 0);
