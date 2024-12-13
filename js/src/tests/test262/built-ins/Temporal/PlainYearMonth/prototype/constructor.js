// |reftest| shell-option(--enable-temporal) skip-if(!this.hasOwnProperty('Temporal')||!xulRuntime.shell) -- Temporal is not enabled unconditionally, requires shell-options
// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainyearmonth.prototype.constructor
description: Test for Temporal.PlainYearMonth.prototype.constructor.
info: The initial value of Temporal.PlainYearMonth.prototype.constructor is %Temporal.PlainYearMonth%.
includes: [propertyHelper.js]
features: [Temporal]
---*/

verifyProperty(Temporal.PlainYearMonth.prototype, "constructor", {
  value: Temporal.PlainYearMonth,
  writable: true,
  enumerable: false,
  configurable: true,
});

reportCompare(0, 0);
