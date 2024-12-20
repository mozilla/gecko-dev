// |reftest| shell-option(--enable-temporal) skip-if(!this.hasOwnProperty('Temporal')||!xulRuntime.shell) -- Temporal is not enabled unconditionally, requires shell-options
// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
includes: [sm/non262-Temporal-PlainMonthDay-shell.js, sm/non262-shell.js, sm/non262.js]
flags:
- noStrict
features:
- Temporal
description: |
  pending
esid: pending
---*/

// Uncommon leap month with 30 days can be far into the past. Computing the
// results can take its time, therefore the test is marked as "slow".
//
// Month -> ISO year
// 
// M01L     1461
// M02L     <common>
// M03L     <common>
// M04L     <common>
// M05L     <common>
// M06L     <common>
// M07L     <common>
// M08L     <common>
// M09L     -6482
// M10L     -4633
// M11L     -2172
// M12L     -179
//
// See also "The Mathematics of the Chinese Calendar", Table 21 [1] for a
// distribution of leap months.
//
// [1] https://www.xirugu.com/CHI500/Dates_Time/Chinesecalender.pdf

const monthCodes = [
  "M01L",
  // M02L..M08L are common leap months.
  "M09L",
  "M10L",
  "M11L",
  "M12L",
];

function assertSameISOFields(actual, expected) {
  let actualFields = actual.getISOFields();
  let expectedFields = expected.getISOFields();

  assert.sameValue(typeof actualFields.isoYear, "number");
  assert.sameValue(typeof actualFields.isoMonth, "number");
  assert.sameValue(typeof actualFields.isoDay, "number");

  assert.sameValue(actualFields.isoMonth > 0, true);
  assert.sameValue(actualFields.isoDay > 0, true);

  assert.sameValue(actualFields.isoYear, expectedFields.isoYear);
  assert.sameValue(actualFields.isoMonth, expectedFields.isoMonth);
  assert.sameValue(actualFields.isoDay, expectedFields.isoDay);
}

const calendar = "chinese";

// Months can have up to 30 days.
const day = 30;

for (let monthCode of monthCodes) {
  let pmd = Temporal.PlainMonthDay.from({calendar, monthCode, day});
  assert.sameValue(pmd.monthCode, monthCode);
  assert.sameValue(pmd.day, day);

  let constrain = Temporal.PlainMonthDay.from({calendar, monthCode, day: day + 1}, {overflow: "constrain"});
  assert.sameValue(constrain.monthCode, monthCode);
  assert.sameValue(constrain.day, day);
  assertSameISOFields(constrain, pmd);

  assertThrowsInstanceOf(() => {
    Temporal.PlainMonthDay.from({calendar, monthCode, day: day + 1}, {overflow: "reject"});
  }, RangeError);
}


reportCompare(0, 0);
