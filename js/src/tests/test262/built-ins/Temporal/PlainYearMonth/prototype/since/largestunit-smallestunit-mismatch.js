// |reftest| shell-option(--enable-temporal) skip-if(!this.hasOwnProperty('Temporal')||!xulRuntime.shell) -- Temporal is not enabled unconditionally, requires shell-options
// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainyearmonth.prototype.since
description: RangeError thrown when smallestUnit is larger than largestUnit
features: [Temporal]
---*/

const earlier = new Temporal.PlainYearMonth(2000, 5);
const later = new Temporal.PlainYearMonth(2001, 6);
const units = ["years", "months"];
for (let largestIdx = 1; largestIdx < units.length; largestIdx++) {
  for (let smallestIdx = 0; smallestIdx < largestIdx; smallestIdx++) {
    const largestUnit = units[largestIdx];
    const smallestUnit = units[smallestIdx];
    assert.throws(RangeError, () => later.since(earlier, { largestUnit, smallestUnit }));
  }
}

reportCompare(0, 0);
