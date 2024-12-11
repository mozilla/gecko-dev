// |reftest| shell-option(--enable-temporal) skip-if(!this.hasOwnProperty('Temporal')||!xulRuntime.shell) -- Temporal is not enabled unconditionally, requires shell-options
// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainyearmonth.from
description: RangeError thrown if a string with trailing junk is used as a PlainYearMonth
features: [Temporal]
---*/

assert.throws(RangeError, () => Temporal.PlainYearMonth.from("1976-11junk"));

reportCompare(0, 0);
