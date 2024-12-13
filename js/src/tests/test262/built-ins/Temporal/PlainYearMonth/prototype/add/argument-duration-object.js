// |reftest| shell-option(--enable-temporal) skip-if(!this.hasOwnProperty('Temporal')||!xulRuntime.shell) -- Temporal is not enabled unconditionally, requires shell-options
// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainyearmonth.prototype.add
description: A Duration object is supported as the argument
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const nov94 = Temporal.PlainYearMonth.from("1994-11");
const diff = Temporal.Duration.from("P18Y7M");
TemporalHelpers.assertPlainYearMonth(nov94.add(diff), 2013, 6, "M06");

reportCompare(0, 0);
