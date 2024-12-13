// |reftest| shell-option(--enable-temporal) skip-if(!this.hasOwnProperty('Temporal')||!xulRuntime.shell) -- Temporal is not enabled unconditionally, requires shell-options
// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-get-temporal.plaindate.prototype.monthsinyear
description: Basic tests for monthsInYear().
features: [Temporal]
---*/

const plainDate = new Temporal.PlainDate(1976, 11, 18);
assert.sameValue(plainDate.monthsInYear, 12);

reportCompare(0, 0);
